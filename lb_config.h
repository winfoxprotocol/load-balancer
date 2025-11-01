#ifndef LB_CONFIG_H
#define LB_CONFIG_H

#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>

using namespace std;

struct BackendServer {
    string ip;
    int port;
    int id;
    
    bool healthy;
    double avg_rtt_ms;
    int consecutive_failures;
    chrono::steady_clock::time_point last_check;
    
    BackendServer() : port(0), id(0), healthy(true), avg_rtt_ms(0.0), 
                      consecutive_failures(0) {}
    
    BackendServer(int server_id, string server_ip, int server_port) 
        : ip(server_ip), port(server_port), id(server_id), 
          healthy(true), avg_rtt_ms(0.0), consecutive_failures(0) {}
};

struct LBConfig {
    string lb_ip;
    int lb_port;
    vector<BackendServer> backends;
    
    LBConfig() : lb_ip("127.0.0.1"), lb_port(8000) {}
};

LBConfig parse_lb_config(const string& filename);

#endif