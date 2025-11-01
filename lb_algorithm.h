#ifndef LB_ALGORITHM_H
#define LB_ALGORITHM_H

#include "lb_config.h"
#include <memory>
#include <mutex>
#include <string>

using namespace std;

enum class LBAlgorithmType {
    ROUND_ROBIN,
    LEAST_RESPONSE_TIME
};

class LBAlgorithm {
protected:
    vector<BackendServer>& backends;
    mutex selection_mutex;
    
public:
    LBAlgorithm(vector<BackendServer>& backend_list) : backends(backend_list) {}
    virtual ~LBAlgorithm() {}
    
    virtual BackendServer* select_backend() = 0;
    virtual string get_name() const = 0;
};

class RoundRobinLB : public LBAlgorithm {
private:
    size_t current_index;
    
public:
    RoundRobinLB(vector<BackendServer>& backend_list);
    BackendServer* select_backend() override;
    string get_name() const override { return "Round Robin"; }
};

class LeastResponseTimeLB : public LBAlgorithm {
public:
    LeastResponseTimeLB(vector<BackendServer>& backend_list);
    BackendServer* select_backend() override;
    string get_name() const override { return "Least Response Time"; }
};

unique_ptr<LBAlgorithm> create_lb_algorithm(LBAlgorithmType type, 
                                             vector<BackendServer>& backends);

LBAlgorithmType parse_lb_algorithm(const string& algo_str);

#endif