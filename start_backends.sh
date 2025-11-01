GREEN='\033[0;32m'
NC='\033[0m'

SERVER_BIN="./server"
TEST_DIR="testdata"

if [ ! -f "$SERVER_BIN" ]; then
    echo "Error: server binary not found. Run 'make' first."
    exit 1
fi

if [ ! -d "$TEST_DIR" ]; then
    echo "Error: testdata directory not found."
    exit 1
fi

echo -e "${GREEN}Starting 4 backend servers...${NC}"

pkill -9 server 2>/dev/null || true
sleep 1

for i in 1 2 3 4; do
    port=$((9000 + i))
    cat > config_server${i}.json <<EOF
{
    "server_ip": "127.0.0.1",
    "server_port": ${port},
    "server_threads": 4,
    "client_threads": 8
}
EOF
done

for i in 1 2 3 4; do
    port=$((9000 + i))
    echo -e "${GREEN}Starting Backend Server ${i} on port ${port}${NC}"
    
    cp config_server${i}.json config.json
    $SERVER_BIN --sched fcfs --p 10 --file $TEST_DIR > backend${i}.log 2>&1 &
    echo $! > backend${i}.pid
    
    sleep 1
done

echo ""
echo -e "${GREEN}All backend servers started!${NC}"
echo "Backend 1: Port 9001 (PID: $(cat backend1.pid))"
echo "Backend 2: Port 9002 (PID: $(cat backend2.pid))"
echo "Backend 3: Port 9003 (PID: $(cat backend3.pid))"
echo "Backend 4: Port 9004 (PID: $(cat backend4.pid))"
echo ""
echo "To stop all backends, run: ./stop_backends.sh"
