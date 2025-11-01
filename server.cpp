#include "config.h"
#include "protocol.h"
#include "scheduler.h"
#include "utils.h"
#include <iostream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <getopt.h>

using namespace std;

map<string, vector<string>> file_storage;
mutex storage_mutex;

vector<Request> completed_requests;
mutex metrics_mutex;

int packet_size = 10;
unique_ptr<Scheduler> scheduler;

atomic<bool> shutdown_requested(false);
int global_server_sock = -1;

void signal_handler(int signum)
{
    cout << "\n[Server] Received signal " << signum << ", shutting down..." << endl;
    shutdown_requested = true;

    if (global_server_sock >= 0)
    {
        shutdown(global_server_sock, SHUT_RDWR);
        close(global_server_sock);
    }

    if (scheduler)
    {
        scheduler->signal_shutdown();
    }
}

void store_file(const string &filename, const vector<string> &lines)
{
    lock_guard<mutex> lock(storage_mutex);
    file_storage[filename] = lines;
    cout << "[Server] Stored file: " << filename
         << " (" << lines.size() << " lines)" << endl;
}

bool retrieve_file(const string &filename, vector<string> &lines)
{
    lock_guard<mutex> lock(storage_mutex);
    auto it = file_storage.find(filename);
    if (it == file_storage.end())
    {
        return false;
    }

    lines = it->second;
    return true;
}

bool handle_put(int client_sock, Request &request)
{
    store_file(request.filename, request.file_lines);

    return send_line(client_sock, PROTOCOL_OK);
}

bool handle_get(int client_sock, Request &request)
{
    vector<string> lines;
    if (!retrieve_file(request.filename, lines))
    {
        send_line(client_sock, PROTOCOL_ERROR + " File not found");
        return false;
    }

    if (!send_line(client_sock, PROTOCOL_OK))
    {
        return false;
    }

    size_t file_size = get_file_size(lines);

    if (!send_line(client_sock, PROTOCOL_SIZE + " " + to_string(file_size)))
    {
        return false;
    }

    return send_file(client_sock, lines, packet_size);
}

void process_request(shared_ptr<Request> request, int client_sock)
{
    request->start_time = get_current_time_ns();

    bool success = false;
    if (request->type == RequestType::PUT)
    {
        success = handle_put(client_sock, *request);
    }
    else if (request->type == RequestType::GET)
    {
        success = handle_get(client_sock, *request);
    }

    request->finish_time = get_current_time_ns();

    {
        lock_guard<mutex> lock(metrics_mutex);
        completed_requests.push_back(*request);
    }

    if (success)
    {
        cout << "[Worker] Completed "
             << (request->type == RequestType::PUT ? "PUT" : "GET")
             << " " << request->filename
             << " (Response time: " << ns_to_ms(request->finish_time - request->arrival_time)
             << " ms)" << endl;
    }

    close(client_sock);
}

bool process_request_chunk_timed(shared_ptr<Request> request)
{
    if (request->type == RequestType::PUT)
    {
        store_file(request->filename, request->file_lines);
        send_line(request->client_id, PROTOCOL_OK);
        return true;
    }
    else if (request->type == RequestType::GET)
    {

        if (request->lines_processed == 0)
        {
            if (!send_line(request->client_id, PROTOCOL_OK))
            {
                return true;
            }
            if (!send_line(request->client_id, PROTOCOL_SIZE + " " + to_string(request->file_size)))
            {
                return true;
            }
        }

        RRScheduler *rr_sched = dynamic_cast<RRScheduler *>(scheduler.get());
        long long quantum_ms = rr_sched ? rr_sched->get_quantum() : 10;
        long long quantum_ns = quantum_ms * 1'000'000LL;
        auto chunk_start_time = chrono::steady_clock::now();

        while (true)
        {
            if (request->lines_processed >= request->file_lines.size())
            {
                send_line(request->client_id, PROTOCOL_END);
                return true;
            }

            const string &line_to_send = request->file_lines[request->lines_processed];
            if (!send_line(request->client_id, line_to_send))
            {
                return true;
            }
            request->lines_processed++;

            auto elapsed_ns = chrono::duration_cast<chrono::nanoseconds>(
                                  chrono::steady_clock::now() - chunk_start_time)
                                  .count();

            if (elapsed_ns >= quantum_ns)
            {
                break;
            }
        }
        return false;
    }

    return true;
}

void worker_thread()
{
    while (true)
    {
        auto request = scheduler->get_next_request();
        if (!request)
        {
            break;
        }

        RRScheduler *rr_sched = dynamic_cast<RRScheduler *>(scheduler.get());

        if (rr_sched)
        {
            if (request->start_time == 0)
            {
                request->start_time = get_current_time_ns();
            }

            bool is_complete = process_request_chunk_timed(request);

            if (is_complete)
            {
                request->finish_time = get_current_time_ns();
                {
                    lock_guard<mutex> lock(metrics_mutex);
                    completed_requests.push_back(*request);
                }
                cout << "[Worker] Completed (RR) " << request->filename << endl;
                close(request->client_id);
            }
            else
            {
                rr_sched->requeue_request(request);
            }
        }
        else
        {
            int client_sock = request->client_id;
            process_request(request, client_sock);
        }
    }
}

void acceptor_thread(int server_sock)
{
    while (!shutdown_requested)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        if (shutdown_requested)
        {
            break;
        }

        if (client_sock < 0)
        {
            continue;
        }

        cout << "[Server] Accepted connection from "
             << inet_ntoa(client_addr.sin_addr) << endl;
        string first_line;
        char buffer[1024];
        ssize_t bytes_peeked = recv(client_sock, buffer, sizeof(buffer) - 1, MSG_PEEK);

        if (bytes_peeked > 0)
        {
            buffer[bytes_peeked] = '\0';
            string peeked_data(buffer);
            size_t newline_pos = peeked_data.find('\n');
            if (newline_pos != string::npos)
            {
                first_line = peeked_data.substr(0, newline_pos);
            }
        }

        if (first_line == PROTOCOL_HEALTH)
        {
            string health_line;
            recv_line(client_sock, health_line);

            send_line(client_sock, PROTOCOL_HEALTH_OK);
            close(client_sock);

            cout << "[Server] Responded to health check" << endl;
            continue;
        }

        auto request = make_shared<Request>();
        request->arrival_time = get_current_time_ns();

        if (!parse_request(client_sock, *request))
        {
            cerr << "[Server] Failed to parse request" << endl;
            send_line(client_sock, PROTOCOL_ERROR + " Malformed request");
            close(client_sock);
            continue;
        }

        request->client_id = client_sock;

        if (request->type == RequestType::GET)
        {
            vector<string> lines;
            if (retrieve_file(request->filename, lines))
            {
                request->file_size = get_file_size(lines);
                request->file_lines = lines;
            }
            else
            {
                request->file_size = 0;
            }
        }
        scheduler->add_request(request);
    }

    cout << "[Server] Acceptor thread exiting" << endl;
}

void save_metrics(const string &filename)
{
    ofstream file(filename);
    if (!file.is_open())
    {
        cerr << "Error: Cannot create metrics file" << endl;
        return;
    }

    file << "request_type,filename,file_size,arrival_time_ns,start_time_ns,finish_time_ns,"
         << "response_time_ms,waiting_time_ms\n";

    lock_guard<mutex> lock(metrics_mutex);
    for (const auto &req : completed_requests)
    {
        double response_time = ns_to_ms(req.finish_time - req.arrival_time);
        double waiting_time = ns_to_ms(req.start_time - req.arrival_time);
        file << (req.type == RequestType::PUT ? "PUT" : "GET") << ","
             << req.filename << ","
             << req.file_size << ","
             << req.arrival_time << ","
             << req.start_time << ","
             << req.finish_time << ","
             << response_time << ","
             << waiting_time << "\n";
    }

    file.close();
    cout << "[Server] Saved metrics to " << filename << endl;
}

void print_usage(const char *prog_name)
{
    cout << "Usage: " << prog_name << " [options]\n"
         << "Options:\n"
         << "  --sched <policy>    Scheduling policy (fcfs, sjf, rr) [required]\n"
         << "  --quantum <Q>       Time quantum for RR (required if --sched rr)\n"
         << "  --file <path>       Input file or directory [required]\n"
         << "  --p <N>             Packetization parameter (lines per packet) [required]\n"
         << "  --help              Show this help message\n";
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    string sched_policy_str;
    int quantum = 0;
    string file_path;

    static struct option long_options[] = {
        {"sched", required_argument, 0, 's'},
        {"quantum", required_argument, 0, 'q'},
        {"file", required_argument, 0, 'f'},
        {"p", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "s:q:f:p:h", long_options, nullptr)) != -1)
    {
        switch (opt)
        {
        case 's':
            sched_policy_str = optarg;
            break;
        case 'q':
            quantum = atoi(optarg);
            break;
        case 'f':
            file_path = optarg;
            break;
        case 'p':
            packet_size = atoi(optarg);
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (sched_policy_str.empty() || file_path.empty() || packet_size <= 0)
    {
        cerr << "Error: Missing required arguments\n";
        print_usage(argv[0]);
        return 1;
    }

    SchedulingPolicy policy;
    try
    {
        policy = parse_policy(sched_policy_str);
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    if (policy == SchedulingPolicy::RR && quantum <= 0)
    {
        cerr << "Error: --quantum required for Round Robin scheduling\n";
        return 1;
    }

    Config config;
    try
    {
        config = parse_config("config.json");
    }
    catch (const exception &e)
    {
        cerr << "Error loading config: " << e.what() << endl;
        return 1;
    }

    cout << "=== Server Configuration ===\n"
         << "IP: " << config.server_ip << "\n"
         << "Port: " << config.server_port << "\n"
         << "Worker threads: " << config.server_threads << "\n"
         << "Scheduling policy: " << sched_policy_str << "\n";

    if (policy == SchedulingPolicy::RR)
    {
        cout << "Quantum: " << quantum << "\n";
    }

    cout << "Packetization: " << packet_size << " lines/packet\n"
         << "===========================\n"
         << endl;
    if (is_directory(file_path))
    {
        vector<string> files;
        if (list_files(file_path, files))
        {
            for (const auto &file : files)
            {
                vector<string> lines;
                if (read_file_lines(file, lines))
                {
                    store_file(get_filename(file), lines);
                }
            }
        }
    }
    else
    {
        vector<string> lines;
        if (read_file_lines(file_path, lines))
        {
            store_file(get_filename(file_path), lines);
        }
    }

    scheduler = create_scheduler(policy, quantum);
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        cerr << "Error: Cannot create socket" << endl;
        return 1;
    }
    global_server_sock = server_sock;

    int opt_val = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.server_port);
    inet_pton(AF_INET, config.server_ip.c_str(), &server_addr.sin_addr);

    if (::bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Error: Cannot bind socket" << endl;
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 100) < 0)
    {
        cerr << "Error: Cannot listen on socket" << endl;
        close(server_sock);
        return 1;
    }

    cout << "[Server] Listening on " << config.server_ip
         << ":" << config.server_port << endl;
    vector<thread> workers;
    for (int i = 0; i < config.server_threads; ++i)
    {
        workers.emplace_back(worker_thread);
    }
    thread acceptor(acceptor_thread, server_sock);

    cout << "[Server] Press Ctrl+C to stop...\n"
         << endl;
    acceptor.join();

    cout << "[Server] Waiting for workers to finish..." << endl;
    for (auto &worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    if (global_server_sock >= 0)
    {
        close(global_server_sock);
    }

    cout << "[Server] Saving metrics..." << endl;
    save_metrics("metrics.csv");
    cout << "[Server] Shutdown complete" << endl;
    return 0;
}