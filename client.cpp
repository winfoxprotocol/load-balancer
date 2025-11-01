#include "config.h"
#include "protocol.h"
#include "utils.h"
#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <random>
#include <chrono>
#include <sys/stat.h>

using namespace std;

int connect_to_server(const string &ip, int port)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    close(sock);
    return -1;
  }

  return sock;
}

bool send_put_request(const string &server_ip, int server_port,
                      const string &filename)
{
  vector<string> lines;
  if (!read_file_lines(filename, lines))
  {
    cerr << "[Client] Cannot read file: " << filename << endl;
    return false;
  }

  int sock = connect_to_server(server_ip, server_port);
  if (sock < 0)
  {
    cerr << "[Client] Cannot connect to server" << endl;
    return false;
  }

  string base_filename = get_filename(filename);
  if (!send_line(sock, PROTOCOL_PUT + " " + base_filename))
  {
    close(sock);
    return false;
  }

  size_t file_size = get_file_size(lines);
  if (!send_line(sock, PROTOCOL_SIZE + " " + to_string(file_size)))
  {
    close(sock);
    return false;
  }

  if (!send_file(sock, lines, 1))
  {
    close(sock);
    return false;
  }

  string response;
  if (!recv_line(sock, response))
  {
    close(sock);
    return false;
  }

  close(sock);

  if (response == PROTOCOL_OK)
  {
    cout << "[Client] PUT " << base_filename << " - SUCCESS" << endl;
    return true;
  }
  else
  {
    cerr << "[Client] PUT " << base_filename << " - FAILED: " << response << endl;
    return false;
  }
}

bool send_get_request(const string &server_ip, int server_port,
                      const string &filename, const string &output_path)
{
  int sock = connect_to_server(server_ip, server_port);
  if (sock < 0)
  {
    cerr << "[Client] Cannot connect to server" << endl;
    return false;
  }

  if (!send_line(sock, PROTOCOL_GET + " " + filename))
  {
    close(sock);
    return false;
  }

  string response;
  if (!recv_line(sock, response))
  {
    close(sock);
    return false;
  }

  if (response != PROTOCOL_OK)
  {
    cerr << "[Client] GET " << filename << " - FAILED: " << response << endl;
    close(sock);
    return false;
  }

  string size_line;
  if (!recv_line(sock, size_line))
  {
    close(sock);
    return false;
  }

  size_t file_size;
  sscanf(size_line.c_str(), "SIZE %zu", &file_size);

  vector<string> lines;
  if (!recv_file(sock, file_size, lines))
  {
    close(sock);
    return false;
  }

  close(sock);

  if (!write_file_lines(output_path, lines))
  {
    return false;
  }

  cout << "[Client] GET " << filename << " - SUCCESS ("
       << lines.size() << " lines)" << endl;
  return true;
}

void client_thread_func(int thread_id, const Config &config,
                        const vector<string> &test_files,
                        int num_requests_per_thread)
{
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> file_dist(0, test_files.size() - 1);
  uniform_int_distribution<> op_dist(0, 1);

  for (int i = 0; i < num_requests_per_thread; ++i)
  {
    int file_idx = file_dist(gen);
    string filename = test_files[file_idx];

    bool is_put = (op_dist(gen) == 0);

    if (is_put)
    {
      send_put_request(config.server_ip, config.server_port, filename);
    }
    else
    {
      string output = "client_outputs/output_" + to_string(thread_id) + "_" +
                      to_string(i) + "_" + get_filename(filename);
      send_get_request(config.server_ip, config.server_port,
                       get_filename(filename), output);
    }

    this_thread::sleep_for(chrono::milliseconds(10));
  }
}

void interactive_mode(const Config &config)
{
  mkdir("client_outputs", 0755);

  cout << "\n=== Interactive Client Mode ===\n"
       << "Commands:\n"
       << "  put <local_file>       Upload file to server\n"
       << "  get <remote_file>      Download file from server\n"
       << "  quit                   Exit\n"
       << "===============================\n"
       << endl;

  string command;
  while (true)
  {
    cout << "> ";
    getline(cin, command);

    if (command.empty())
      continue;

    istringstream iss(command);
    string op, filename;
    iss >> op >> filename;

    if (op == "quit" || op == "exit")
    {
      break;
    }
    else if (op == "put")
    {
      if (filename.empty())
      {
        cout << "Usage: put <local_file>" << endl;
        continue;
      }
      send_put_request(config.server_ip, config.server_port, filename);
    }
    else if (op == "get")
    {
      if (filename.empty())
      {
        cout << "Usage: get <remote_file>" << endl;
        continue;
      }
      string output = "client_outputs/downloaded_" + filename;
      send_get_request(config.server_ip, config.server_port, filename, output);
    }
    else
    {
      cout << "Unknown command: " << op << endl;
    }
  }
}

void test_mode(const Config &config, const vector<string> &test_files,
               int num_requests_per_thread)
{
  mkdir("client_outputs", 0755);

  cout << "\n=== Running Test Mode ===\n"
       << "Client threads: " << config.client_threads << "\n"
       << "Requests per thread: " << num_requests_per_thread << "\n"
       << "Test files: " << test_files.size() << "\n"
       << "========================\n"
       << endl;

  auto start_time = chrono::steady_clock::now();

  vector<thread> threads;
  for (int i = 0; i < config.client_threads; ++i)
  {
    threads.emplace_back(client_thread_func, i, cref(config),
                         cref(test_files), num_requests_per_thread);
  }

  for (auto &t : threads)
  {
    t.join();
  }

  auto end_time = chrono::steady_clock::now();
  auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);

  cout << "\n=== Test Complete ===\n"
       << "Total time: " << duration.count() << " ms\n"
       << "Total requests: " << (config.client_threads * num_requests_per_thread) << "\n"
       << "====================\n"
       << endl;
}

void print_usage(const char *prog_name)
{
  cout << "Usage: " << prog_name << " [options]\n"
       << "Options:\n"
       << "  --interactive         Run in interactive mode\n"
       << "  --test <dir>          Run test mode with files from directory\n"
       << "  --requests <N>        Number of requests per thread in test mode (default: 10)\n"
       << "  --help                Show this help message\n";
}

int main(int argc, char *argv[])
{
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

  cout << "=== Client Configuration ===\n"
       << "Server: " << config.server_ip << ":" << config.server_port << "\n"
       << "Client threads: " << config.client_threads << "\n"
       << "============================\n"
       << endl;

  bool interactive = false;
  string test_dir;
  int num_requests = 10;

  for (int i = 1; i < argc; ++i)
  {
    string arg = argv[i];
    if (arg == "--interactive")
    {
      interactive = true;
    }
    else if (arg == "--test" && i + 1 < argc)
    {
      test_dir = argv[++i];
    }
    else if (arg == "--requests" && i + 1 < argc)
    {
      num_requests = atoi(argv[++i]);
    }
    else if (arg == "--help")
    {
      print_usage(argv[0]);
      return 0;
    }
  }

  if (interactive)
  {
    interactive_mode(config);
  }
  else if (!test_dir.empty())
  {
    vector<string> test_files;
    if (!list_files(test_dir, test_files) || test_files.empty())
    {
      cerr << "Error: Cannot list files in " << test_dir << endl;
      return 1;
    }
    test_mode(config, test_files, num_requests);
  }
  else
  {
    cout << "No mode specified. Use --interactive or --test <dir>\n";
    print_usage(argv[0]);
    return 1;
  }

  return 0;
}