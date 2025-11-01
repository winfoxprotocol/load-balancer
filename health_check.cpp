#include "health_check.h"
#include "protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>
#include <sys/select.h>
#include <fcntl.h>

using namespace std;

HealthChecker::HealthChecker(vector<BackendServer> &backend_list,
                             atomic<bool> &shutdown_flag)
    : backends(backend_list), shutdown(shutdown_flag)
{
    log_file.open("health_check.log");
    if (log_file.is_open())
    {
        log_file << "timestamp_ms,backend_id,ip,port,rtt_ms,status\n";
        log_file.flush();
    }
}

HealthChecker::~HealthChecker()
{
    if (log_file.is_open())
    {
        log_file.close();
    }
}

bool HealthChecker::send_health_check(BackendServer &backend, double &rtt_ms)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return false;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(backend.port);
    inet_pton(AF_INET, backend.ip.c_str(), &server_addr.sin_addr);

    auto start_time = chrono::steady_clock::now();

    int result = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (result < 0 && errno != EINPROGRESS)
    {
        close(sock);
        return false;
    }

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = HEALTH_TIMEOUT_MS / 1000;
    timeout.tv_usec = (HEALTH_TIMEOUT_MS % 1000) * 1000;

    result = select(sock + 1, NULL, &write_fds, NULL, &timeout);

    if (result <= 0)
    {
        close(sock);
        return false;
    }

    fcntl(sock, F_SETFL, flags);

    if (!send_line(sock, PROTOCOL_HEALTH))
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

    auto end_time = chrono::steady_clock::now();
    rtt_ms = chrono::duration<double, milli>(end_time - start_time).count();

    close(sock);

    return (response == PROTOCOL_HEALTH_OK);
}

void HealthChecker::update_backend_health(BackendServer &backend, bool success, double rtt_ms)
{
    backend.last_check = chrono::steady_clock::now();

    if (success)
    {
        backend.consecutive_failures = 0;
        backend.healthy = true;

        if (backend.avg_rtt_ms == 0.0)
        {
            backend.avg_rtt_ms = rtt_ms;
        }
        else
        {
            backend.avg_rtt_ms = RTT_ALPHA * rtt_ms + (1.0 - RTT_ALPHA) * backend.avg_rtt_ms;
        }
    }
    else
    {
        backend.consecutive_failures++;
        if (backend.consecutive_failures >= MAX_CONSECUTIVE_FAILURES)
        {
            backend.healthy = false;
        }
    }
}

void HealthChecker::log_health_check(const BackendServer &backend, double rtt_ms, bool success)
{
    lock_guard<mutex> lock(log_mutex);

    if (!log_file.is_open())
    {
        return;
    }

    auto now = chrono::system_clock::now();
    auto timestamp_ms = chrono::duration_cast<chrono::milliseconds>(
                            now.time_since_epoch())
                            .count();

    log_file << timestamp_ms << ","
             << backend.id << ","
             << backend.ip << ","
             << backend.port << ","
             << (success ? rtt_ms : -1.0) << ","
             << (success ? "OK" : "FAIL") << "\n";
    log_file.flush();
}

void HealthChecker::run()
{
    cout << "[HealthChecker] Started" << endl;

    while (!shutdown)
    {
        for (auto &backend : backends)
        {
            double rtt_ms = 0.0;
            bool success = send_health_check(backend, rtt_ms);

            update_backend_health(backend, success, rtt_ms);
            log_health_check(backend, rtt_ms, success);

            if (success)
            {
                cout << "[HealthCheck] Backend " << backend.id
                     << " (" << backend.ip << ":" << backend.port << ") "
                     << "RTT: " << rtt_ms << " ms" << endl;
            }
            else
            {
                cerr << "[HealthCheck] Backend " << backend.id
                     << " (" << backend.ip << ":" << backend.port << ") "
                     << "FAILED" << endl;
            }
        }

        for (int i = 0; i < 10 && !shutdown; ++i)
        {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }

    cout << "[HealthChecker] Stopped" << endl;
}

void HealthChecker::start()
{
    run();
}