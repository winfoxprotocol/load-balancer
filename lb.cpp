#include "lb_config.h"
#include "lb_algorithm.h"
#include "health_check.h"
#include "protocol.h"
#include "utils.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <mutex>
#include <getopt.h>

using namespace std;

atomic<bool> shutdown_requested(false);
int global_lb_sock = -1;

ofstream lb_metrics_file;
mutex metrics_mutex;

void signal_handler(int signum)
{
    cout << "\n[LB] Received signal " << signum << ", shutting down..." << endl;
    shutdown_requested = true;

    if (global_lb_sock >= 0)
    {
        shutdown(global_lb_sock, SHUT_RDWR);
        close(global_lb_sock);
    }
}

void log_request(const string &request_type, int backend_id, double response_time_ms)
{
    lock_guard<mutex> lock(metrics_mutex);

    if (!lb_metrics_file.is_open())
    {
        return;
    }

    auto now = chrono::system_clock::now();
    auto timestamp_ms = chrono::duration_cast<chrono::milliseconds>(
                            now.time_since_epoch())
                            .count();

    lb_metrics_file << timestamp_ms << ","
                    << request_type << ","
                    << backend_id << ","
                    << response_time_ms << "\n";
    lb_metrics_file.flush();
}

int connect_to_backend(const BackendServer &backend)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(backend.port);
    inet_pton(AF_INET, backend.ip.c_str(), &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}

bool forward_put_request(int client_sock, int backend_sock, const Request &request)
{
    if (!send_line(backend_sock, PROTOCOL_PUT + " " + request.filename))
    {
        return false;
    }

    if (!send_line(backend_sock, PROTOCOL_SIZE + " " + to_string(request.file_size)))
    {
        return false;
    }

    if (!send_file(backend_sock, request.file_lines, 10))
    {
        return false;
    }

    string response;
    if (!recv_line(backend_sock, response))
    {
        return false;
    }

    return send_line(client_sock, response);
}

bool forward_get_request(int client_sock, int backend_sock, const Request &request)
{
    if (!send_line(backend_sock, PROTOCOL_GET + " " + request.filename))
    {
        return false;
    }

    string response;
    if (!recv_line(backend_sock, response))
    {
        return false;
    }

    if (!send_line(client_sock, response))
    {
        return false;
    }

    if (response != PROTOCOL_OK)
    {
        return false;
    }

    string size_line;
    if (!recv_line(backend_sock, size_line))
    {
        return false;
    }

    if (!send_line(client_sock, size_line))
    {
        return false;
    }

    size_t file_size;
    sscanf(size_line.c_str(), "SIZE %zu", &file_size);

    vector<string> lines;
    if (!recv_file(backend_sock, file_size, lines))
    {
        return false;
    }

    return send_file(client_sock, lines, 10);
}

void handle_client(int client_sock, LBAlgorithm *lb_algo, LBConfig)
{
    auto request_start = chrono::steady_clock::now();

    Request request;
    if (!parse_request(client_sock, request))
    {
        cerr << "[LB] Failed to parse client request" << endl;
        send_line(client_sock, PROTOCOL_ERROR + " Malformed request");
        close(client_sock);
        return;
    }

    cout << "[LB] Received "
         << (request.type == RequestType::PUT ? "PUT" : "GET")
         << " request for " << request.filename << endl;

    BackendServer *backend = lb_algo->select_backend();
    if (!backend)
    {
        cerr << "[LB] No backend available" << endl;
        send_line(client_sock, PROTOCOL_ERROR + " No backend available");
        close(client_sock);
        return;
    }

    cout << "[LB] Selected backend " << backend->id
         << " (" << backend->ip << ":" << backend->port << ")" << endl;

    int backend_sock = connect_to_backend(*backend);
    if (backend_sock < 0)
    {
        cerr << "[LB] Failed to connect to backend " << backend->id << endl;
        send_line(client_sock, PROTOCOL_ERROR + " Backend unavailable");
        close(client_sock);
        return;
    }

    bool success = false;
    if (request.type == RequestType::PUT)
    {
        success = forward_put_request(client_sock, backend_sock, request);
    }
    else if (request.type == RequestType::GET)
    {
        success = forward_get_request(client_sock, backend_sock, request);
    }

    auto request_end = chrono::steady_clock::now();
    double response_time_ms = chrono::duration<double, milli>(request_end - request_start).count();

    string req_type = (request.type == RequestType::PUT ? "PUT" : "GET");
    log_request(req_type, backend->id, response_time_ms);

    if (success)
    {
        cout << "[LB] Successfully forwarded " << req_type
             << " request (took " << response_time_ms << " ms)" << endl;
    }
    else
    {
        cerr << "[LB] Failed to forward " << req_type << " request" << endl;
    }

    close(backend_sock);
    close(client_sock);
}

void acceptor_thread(int lb_sock, LBAlgorithm *lb_algo, LBConfig &config)
{
    cout << "[LB] Acceptor thread started" << endl;

    while (!shutdown_requested)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_sock = accept(lb_sock, (struct sockaddr *)&client_addr, &client_len);

        if (shutdown_requested)
        {
            break;
        }

        if (client_sock < 0)
        {
            continue;
        }

        cout << "[LB] Accepted connection from "
             << inet_ntoa(client_addr.sin_addr) << endl;

        thread client_thread(handle_client, client_sock, lb_algo, ref(config));
        client_thread.detach();
    }

    cout << "[LB] Acceptor thread exiting" << endl;
}

void print_usage(const char *prog_name)
{
    cout << "Usage: " << prog_name << " [options]\n"
         << "Options:\n"
         << "  --algo <algorithm>    Load balancing algorithm (rr or lrt) [required]\n"
         << "  --config <path>       Config file path (default: config_lb.json)\n"
         << "  --help                Show this help message\n";
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    string config_file = "config_lb.json";
    string algo_str;

    static struct option long_options[] = {
        {"algo", required_argument, 0, 'a'},
        {"config", required_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "a:c:h", long_options, nullptr)) != -1)
    {
        switch (opt)
        {
        case 'a':
            algo_str = optarg;
            break;
        case 'c':
            config_file = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (algo_str.empty())
    {
        cerr << "Error: --algo is required\n";
        print_usage(argv[0]);
        return 1;
    }

    LBConfig config;
    try
    {
        config = parse_lb_config(config_file);
    }
    catch (const exception &e)
    {
        cerr << "Error loading config: " << e.what() << endl;
        return 1;
    }

    LBAlgorithmType algo_type;
    try
    {
        algo_type = parse_lb_algorithm(algo_str);
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    auto lb_algo = create_lb_algorithm(algo_type, config.backends);

    cout << "=== Load Balancer Configuration ===\n"
         << "IP: " << config.lb_ip << "\n"
         << "Port: " << config.lb_port << "\n"
         << "Algorithm: " << lb_algo->get_name() << "\n"
         << "Backends:\n";

    for (const auto &backend : config.backends)
    {
        cout << "  Backend " << backend.id << ": "
             << backend.ip << ":" << backend.port << "\n";
    }
    cout << "===================================\n"
         << endl;

    lb_metrics_file.open("lb_metrics.log");
    if (lb_metrics_file.is_open())
    {
        lb_metrics_file << "timestamp_ms,request_type,backend_selected,response_time_ms\n";
        lb_metrics_file.flush();
    }

    int lb_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (lb_sock < 0)
    {
        cerr << "Error: Cannot create socket" << endl;
        return 1;
    }
    global_lb_sock = lb_sock;

    int opt_val = 1;
    setsockopt(lb_sock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    struct sockaddr_in lb_addr;
    memset(&lb_addr, 0, sizeof(lb_addr));
    lb_addr.sin_family = AF_INET;
    lb_addr.sin_port = htons(config.lb_port);
    inet_pton(AF_INET, config.lb_ip.c_str(), &lb_addr.sin_addr);

    if (::bind(lb_sock, (struct sockaddr *)&lb_addr, sizeof(lb_addr)) < 0)
    {
        cerr << "Error: Cannot bind socket" << endl;
        close(lb_sock);
        return 1;
    }

    if (listen(lb_sock, 100) < 0)
    {
        cerr << "Error: Cannot listen on socket" << endl;
        close(lb_sock);
        return 1;
    }

    cout << "[LB] Listening on " << config.lb_ip << ":" << config.lb_port << endl;

    thread health_thread([&config]()
                         {
HealthChecker checker(config.backends, shutdown_requested);
        checker.start(); });

    thread acceptor(acceptor_thread, lb_sock, lb_algo.get(), ref(config));

    cout << "[LB] Press Ctrl+C to stop...\n"
         << endl;

    acceptor.join();
    health_thread.join();

    if (global_lb_sock >= 0)
    {
        close(global_lb_sock);
    }

    if (lb_metrics_file.is_open())
    {
        lb_metrics_file.close();
    }

    cout << "[LB] Shutdown complete" << endl;
    return 0;
}