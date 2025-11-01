GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

LB_BIN="./lb"
CLIENT_BIN="./client"
TEST_DIR="testdata"
RESULTS_DIR="results_lb"

print_msg() {
    echo -e "${GREEN}[EXPERIMENT]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

cleanup() {
    print_msg "Cleaning up..."
    ./stop_backends.sh 2>/dev/null || true
    pkill -9 lb 2>/dev/null || true
    sleep 2
}

run_experiment() {
    local name=$1
    local algo=$2
    local num_clients=$3
    local requests=$4
    
    print_msg "Running: $name (algo=$algo, clients=$num_clients, requests=$requests)"
    
    cat > config.json <<EOF
    {
        "server_ip": "127.0.0.1",
        "server_port": 8000,
        "server_threads": 4,
        "client_threads": $num_clients
    }
EOF
    
    ./start_backends.sh > /dev/null 2>&1
    sleep 3
    
    $LB_BIN --algo $algo --config config_lb.json > ${name}_lb.log 2>&1 &
    local lb_pid=$!
    sleep 3
    
    $CLIENT_BIN --test $TEST_DIR --requests $requests > ${name}_client.log 2>&1
    
    sleep 5
    
    kill -INT $lb_pid 2>/dev/null || kill -9 $lb_pid 2>/dev/null
    
    ./stop_backends.sh > /dev/null 2>&1
    
    [ -f health_check.log ] && mv health_check.log "$RESULTS_DIR/${name}_health.log"
    [ -f lb_metrics.log ] && mv lb_metrics.log "$RESULTS_DIR/${name}_metrics.log"
    mv ${name}_lb.log "$RESULTS_DIR/" 2>/dev/null
    mv ${name}_client.log "$RESULTS_DIR/" 2>/dev/null
    
    print_msg "✓ Completed: $name"
    sleep 2
}

cleanup

mkdir -p $RESULTS_DIR

print_msg "=== Part B: Load Balancer Experiments ==="
echo ""

print_msg "=== Experiment 1: Algorithm Comparison ==="
for clients in 4 8 16; do
    run_experiment "exp1_rr_c${clients}" "rr" $clients 20
    run_experiment "exp1_lrt_c${clients}" "lrt" $clients 20
done

print_msg "=== Experiment 2: High Load Test ==="
run_experiment "exp2_rr_stress" "rr" 32 50
run_experiment "exp2_lrt_stress" "lrt" 32 50

print_msg "=== Experiment 3: Fault Tolerance ==="

run_experiment "exp3_rr_baseline" "rr" 8 30
run_experiment "exp3_lrt_baseline" "lrt" 8 30

print_msg "=== All experiments complete! ==="
print_msg "Results saved in: $RESULTS_DIR/"
print_msg "Generated files:"
ls -lh $RESULTS_DIR/

cleanup
