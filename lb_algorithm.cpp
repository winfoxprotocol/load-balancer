#include "lb_algorithm.h"
#include <algorithm>
#include <limits>
#include <iostream>

using namespace std;

RoundRobinLB::RoundRobinLB(vector<BackendServer> &backend_list)
    : LBAlgorithm(backend_list), current_index(0) {}

BackendServer *RoundRobinLB::select_backend()
{
    lock_guard<mutex> lock(selection_mutex);

    if (backends.empty())
    {
        return nullptr;
    }

    size_t start_index = current_index;
    size_t attempts = 0;

    while (attempts < backends.size())
    {
        BackendServer &backend = backends[current_index];
        current_index = (current_index + 1) % backends.size();

        if (backend.healthy)
        {
            return &backend;
        }

        attempts++;
    }

    current_index = (start_index + 1) % backends.size();
    return &backends[start_index];
}

LeastResponseTimeLB::LeastResponseTimeLB(vector<BackendServer> &backend_list)
    : LBAlgorithm(backend_list) {}

BackendServer *LeastResponseTimeLB::select_backend()
{
    lock_guard<mutex> lock(selection_mutex);

    if (backends.empty())
    {
        return nullptr;
    }

    BackendServer *best = nullptr;
    double min_rtt = numeric_limits<double>::max();

    for (auto &backend : backends)
    {
        if (backend.healthy && backend.avg_rtt_ms < min_rtt)
        {
            min_rtt = backend.avg_rtt_ms;
            best = &backend;
        }
    }

    if (!best)
    {
        best = &backends[0];
    }

    return best;
}

unique_ptr<LBAlgorithm> create_lb_algorithm(LBAlgorithmType type,
                                            vector<BackendServer> &backends)
{
    switch (type)
    {
    case LBAlgorithmType::ROUND_ROBIN:
        return make_unique<RoundRobinLB>(backends);
    case LBAlgorithmType::LEAST_RESPONSE_TIME:
        return make_unique<LeastResponseTimeLB>(backends);
    default:
        throw runtime_error("Unknown LB algorithm type");
    }
}

LBAlgorithmType parse_lb_algorithm(const string &algo_str)
{
    string lower = algo_str;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "rr" || lower == "roundrobin" || lower == "round_robin")
    {
        return LBAlgorithmType::ROUND_ROBIN;
    }
    if (lower == "lrt" || lower == "least_response_time" || lower == "leastresponsetime")
    {
        return LBAlgorithmType::LEAST_RESPONSE_TIME;
    }

    throw runtime_error("Invalid LB algorithm: " + algo_str +
                        " (must be rr or lrt)");
}