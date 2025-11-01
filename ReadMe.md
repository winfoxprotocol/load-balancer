# Link Scheduling - Part B: Load Balancer

## Overview

This is the Part B implementation featuring a **Load Balancer** that distributes client requests across 4 backend servers. The load balancer implements two distinct load balancing algorithms and includes comprehensive health monitoring.

## Architecture


┌─────────┐      ┌──────────────┐      ┌──────────┐
│ Clients │─────▶│     Load     │─────▶│ Backend  │
│         │      │   Balancer   │      │ Server 1 │
└─────────┘      │              │      └──────────┘
                 │  Port 8000   │      ┌──────────┐
                 │              │─────▶│ Backend  │
                 │ - Forwards   │      │ Server 2 │
                 │ - Routes     │      └──────────┘
                 │ - Monitors   │      ┌──────────┐
                 │              │─────▶│ Backend  │
                 │ Health       │      │ Server 3 │
                 │ Checks       │      └──────────┘
                 │ (1/sec)      │      ┌──────────┐
                 │              │─────▶│ Backend  │
                 └──────────────┘      │ Server 4 │
                                       └──────────┘


## Features

### Load Balancer
Concurrent request handling: Multiple client threads supported

Health monitoring: Automatic health checks every 1 second

Two LB algorithms: Round Robin and Least Response Time

Comprehensive logging: Request metrics and health check data

Fault tolerance: Automatic rerouting from unhealthy backends

### Backend Servers

4 independent server instances (ports 9001-9004)

Same API as Part A (PUT/GET operations)

Health check response capability

FCFS scheduling with 4 worker threads each

## Project Structure


Part B Files:
├── lb.cpp                  # Load balancer main implementation
├── lb_config.h/cpp         # LB configuration parser
├── lb_algorithm.h/cpp      # LB algorithms (RR & LRT)
├── health_check.h/cpp      # Health monitoring system
├── config_lb.json          # LB configuration file
├── start_backends.sh       # Script to start 4 backends
├── stop_backends.sh        # Script to stop backends
├── run_lb_experiments.sh   # Automated experiments
├── Makefile                # Build system (updated)
└── README_PARTB.md         # This file

Modified from Part A:
├── protocol.h              # Added health check protocol
├── server.cpp              # Added health check handler

Reused from Part A:
├── client.cpp
├── config.cpp/h
├── protocol.cpp
├── utils.cpp/h
└── testdata/

Python Scripts

analyze_health.py			# Analyze health logs
compare_algorithms.py		# Compare lb algorithms

## Building

### Prerequisites

g++ with C++17 support

POSIX threads (pthread)

Linux/Unix environment

### Build and Execution Instructions

bash
make clean
make all


This builds three executables:
- `server` - Backend server
- `client` - Client application
- `lb` - Load balancer

The easiest way to run all experiments:

bash
chmod +x *.sh
./run_lb_experiments.sh


This script:
1. Runs multiple experiments with both algorithms
2. Varies client load (4, 8, 16, 32 clients)
3. Collects health check logs and metrics
4. Saves all results to `results_lb/` directory

## Configuration

### config_lb.json

json
{
    "lb_ip": "127.0.0.1",
    "lb_port": 8000,
    "server1_ip": "127.0.0.1",
    "server1_port": 9001,
    "server2_ip": "127.0.0.1",
    "server2_port": 9002,
    "server3_ip": "127.0.0.1",
    "server3_port": 9003,
    "server4_ip": "127.0.0.1",
    "server4_port": 9004
}


### config.json (for clients)

json
{
    "server_ip": "127.0.0.1",
    "server_port": 8000,
    "server_threads": 4,
    "client_threads": 8
}


**Note**: Clients connect to the load balancer (port 8000), not directly to backend servers.

## Running

### Automated Experiments


### Manual Execution

#### Step 1: Start Backend Servers

bash
./start_backends.sh


This starts 4 backend servers on ports 9001-9004.

#### Step 2: Start Load Balancer

Choose one of the two algorithms:

Round Robin:
bash
./lb --algo rr


Least Response Time:
bash
./lb --algo lrt


#### Step 3: Run Clients

Update `config.json` to point to LB (port 8000), then:

bash

# Interactive mode
./client --interactive

# Test mode
./client --test testdata/ --requests 20


#### Step 4: Stop Everything

bash
# Stop backends
./stop_backends.sh

# Stop LB (Ctrl+C or)
pkill lb


## Load Balancing Algorithms

### 1. Round Robin (RR)

#### Strategy: Distributes requests evenly across all healthy backends in circular order.

##### Characteristics:

Simple and fair distribution

No bias toward any backend

Ignores backend performance differences

Best for homogeneous backends

#####Selection Logic:

1. Maintain counter (current_index)
2. Select backends[current_index]
3. If healthy: return backend
4. Else: try next backend
5. Increment counter (mod num_backends)


### 2. Least Response Time (LRT)

#### Strategy: Routes requests to the backend with the lowest average response time based on health check data.

##### Characteristics:

Performance-aware routing
Adapts to backend load and capacity
Uses exponential moving average (α=0.3) for RTT
Best for heterogeneous backends or varying loads

##### Selection Logic:

1. For each healthy backend:
   - Track avg_rtt_ms from health checks
2. Select backend with minimum avg_rtt_ms
3. Fallback to Round Robin if RTTs equal


## Health Check System

### Protocol


LB → Backend: "HEALTH\n"
Backend → LB: "HEALTH_OK\n"


### Behavior

Frequency: Every 1 second
Timeout: 1000ms per check
Failure threshold: 3 consecutive failures → mark unhealthy
RTT tracking: Exponential moving average (α=0.3)

### Health Check Log Format

File: health_check.log

csv
timestamp_ms,backend_id,ip,port,rtt_ms,status
1234567890123,1,127.0.0.1,9001,2.45,OK
1234567891123,2,127.0.0.1,9002,-1,FAIL


## Metrics and Logging

### LB Metrics Log

File: `lb_metrics.log`

csv
timestamp_ms,request_type,backend_selected,response_time_ms
1234567890123,PUT,2,45.67
1234567891234,GET,1,23.45


### Backend Logs

Each backend generates:
- `backend1.log` through `backend4.log` (console output)
- Individual `metrics.csv` (per-request timing)

## Experiments

### Experiment 1: Algorithm Comparison
- Compare RR vs LRT
- Varying client counts: 4, 8, 16
- Metrics: request distribution, response times

### Experiment 2: High Load Test
- Stress test with 32 clients
- 50 requests per thread
- Observe algorithm behavior under load

### Experiment 3: Fault Tolerance
- Baseline with all backends healthy
- Manual: kill backend during test
- Observe failover behavior

## Analysis Scripts

We created Python scripts for analysis:

bash
# Analyze health check data
python3 analyze_health.py results_lb/exp1_rr_c8_health.log

# Compare algorithms
python3 compare_algorithms.py results_lb/


## Troubleshooting

### Backends won't start
bash
# Check if ports are in use
netstat -tuln | grep -E "900[1-4]"

# Kill existing processes
./stop_backends.sh
pkill -9 server


### LB can't connect to backends
bash
# Verify backends are running
ps aux | grep server

# Check backend logs
tail -f backend*.log

# Verify health checks
tail -f health_check.log


### Client connection refused
bash
# Ensure LB is running
ps aux | grep lb

# Check LB is listening
netstat -tuln | grep 8000

# Verify config.json points to port 8000
cat config.json


## Performance Tips

1. Increase worker threads: Edit start_backends.sh to use more server_threads
2. Adjust health check frequency: Modify HEALTH_TIMEOUT_MS in health_check.cpp
3. Tune RTT smoothing: Change RTT_ALPHA in health_check.cpp

## Cleanup

bash
# Clean all generated files
make clean

# Stop all processes
./stop_backends.sh
pkill lb client server

# Remove logs and results
rm -f *.log *.pid backend*.log
rm -rf results_lb/


## Authors

 Girish Singh Thakur
 Nitish Kumar

# License 

MIT License
