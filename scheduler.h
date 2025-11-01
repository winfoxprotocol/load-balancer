#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "protocol.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>

using namespace std;

enum class SchedulingPolicy {
    FCFS,
  SJF,
    RR
};

class Scheduler {
protected:
    queue<shared_ptr<Request>> request_queue;
  mutex queue_mutex;
condition_variable queue_cv;
    bool shutdown;
    
public:
    Scheduler() : shutdown(false) {}
    virtual ~Scheduler() {}
    
  virtual void add_request(shared_ptr<Request> req);
    
    virtual shared_ptr<Request> get_next_request() = 0;
    
    void signal_shutdown();
    
  bool empty();
};

class FCFSScheduler : public Scheduler {
public:
    shared_ptr<Request> get_next_request() override;
};

class SJFScheduler : public Scheduler {
private:
    struct SJFComparator {
        bool operator()(const shared_ptr<Request>& a, 
                       const shared_ptr<Request>& b) const {
            return a->file_size > b->file_size;
        }
    };
    
  priority_queue<shared_ptr<Request>, 
                       vector<shared_ptr<Request>>, 
                       SJFComparator> sjf_queue;
    
public:
    void add_request(shared_ptr<Request> req) override;
  shared_ptr<Request> get_next_request() override;
};

class RRScheduler : public Scheduler {
private:
    int quantum;
  queue<shared_ptr<Request>> rr_queue;
    
public:
    explicit RRScheduler(int q) : quantum(q) {}
    
    void add_request(shared_ptr<Request> req) override;
    shared_ptr<Request> get_next_request() override;
    
    void requeue_request(shared_ptr<Request> req);
    
  int get_quantum() const { return quantum; }
};

unique_ptr<Scheduler> create_scheduler(SchedulingPolicy policy, int quantum = 0);

SchedulingPolicy parse_policy(const string& policy_str);

#endif