#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <stdexcept>

using namespace std;

struct Config {
    string server_ip;
int server_port;
  int server_threads;
    int client_threads;
    
  Config() : server_ip("127.0.0.1"), server_port(9000), 
         server_threads(4), client_threads(8) {}
};

Config parse_config(const string& filename);

#endif