#include "scheduler.h"
#include <algorithm>
#include <stdexcept>

using namespace std;

void Scheduler::add_request(shared_ptr<Request> req)
{
  lock_guard<mutex> lock(queue_mutex);
  request_queue.push(req);
  queue_cv.notify_one();
}

void Scheduler::signal_shutdown()
{
  lock_guard<mutex> lock(queue_mutex);
  shutdown = true;
  queue_cv.notify_all();
}

bool Scheduler::empty()
{
  lock_guard<mutex> lock(queue_mutex);
  return request_queue.empty();
}

shared_ptr<Request> FCFSScheduler::get_next_request()
{
  unique_lock<mutex> lock(queue_mutex);

  queue_cv.wait(lock, [this]
                { return !request_queue.empty() || shutdown; });

  if (shutdown && request_queue.empty())
  {
    return nullptr;
  }

  auto req = request_queue.front();
  request_queue.pop();
  return req;
}

void SJFScheduler::add_request(shared_ptr<Request> req)
{
  lock_guard<mutex> lock(queue_mutex);
  sjf_queue.push(req);
  queue_cv.notify_one();
}

shared_ptr<Request> SJFScheduler::get_next_request()
{
  unique_lock<mutex> lock(queue_mutex);

  queue_cv.wait(lock, [this]
                { return !sjf_queue.empty() || shutdown; });

  if (shutdown && sjf_queue.empty())
  {
    return nullptr;
  }

  auto req = sjf_queue.top();
  sjf_queue.pop();
  return req;
}

void RRScheduler::add_request(shared_ptr<Request> req)
{
  lock_guard<mutex> lock(queue_mutex);
  rr_queue.push(req);
  queue_cv.notify_one();
}

shared_ptr<Request> RRScheduler::get_next_request()
{
  unique_lock<mutex> lock(queue_mutex);

  queue_cv.wait(lock, [this]
                { return !rr_queue.empty() || shutdown; });

  if (shutdown && rr_queue.empty())
  {
    return nullptr;
  }

  auto req = rr_queue.front();
  rr_queue.pop();
  return req;
}

void RRScheduler::requeue_request(shared_ptr<Request> req)
{
  lock_guard<mutex> lock(queue_mutex);
  rr_queue.push(req);
  queue_cv.notify_one();
}

unique_ptr<Scheduler> create_scheduler(SchedulingPolicy policy, int quantum)
{
  switch (policy)
  {
  case SchedulingPolicy::FCFS:
    return make_unique<FCFSScheduler>();
  case SchedulingPolicy::SJF:
    return make_unique<SJFScheduler>();
  case SchedulingPolicy::RR:
    if (quantum <= 0)
    {
      throw runtime_error("Round Robin requires positive quantum value");
    }
    return make_unique<RRScheduler>(quantum);
  default:
    throw runtime_error("Unknown scheduling policy");
  }
}

SchedulingPolicy parse_policy(const string &policy_str)
{
  string lower = policy_str;
  transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "fcfs")
    return SchedulingPolicy::FCFS;
  if (lower == "sjf")
    return SchedulingPolicy::SJF;
  if (lower == "rr")
    return SchedulingPolicy::RR;

  throw runtime_error("Invalid scheduling policy: " + policy_str +
                      " (must be fcfs, sjf, or rr)");
}