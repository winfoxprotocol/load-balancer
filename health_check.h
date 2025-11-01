#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

#include "lb_config.h"
#include <string>
#include <fstream>
#include <mutex>
#include <atomic>

using namespace std;

class HealthChecker {
private:
    vector<BackendServer>& backends;
    atomic<bool>& shutdown;
    ofstream log_file;
    mutex log_mutex;
    
    const int HEALTH_TIMEOUT_MS = 1000;
    const int MAX_CONSECUTIVE_FAILURES = 3;
    const double RTT_ALPHA = 0.3;
    
    bool send_health_check(BackendServer& backend, double& rtt_ms);
    void update_backend_health(BackendServer& backend, bool success, double rtt_ms);
    void log_health_check(const BackendServer& backend, double rtt_ms, bool success);
    
public:
    HealthChecker(vector<BackendServer>& backend_list, atomic<bool>& shutdown_flag);
    ~HealthChecker();
    
    void start();
    void run();
};

#endif