RED='\033[0;31m'
NC='\033[0m'

echo -e "${RED}Stopping all backend servers...${NC}"

for i in 1 2 3 4; do
    if [ -f backend${i}.pid ]; then
        pid=$(cat backend${i}.pid)
        echo "Stopping Backend Server ${i} (PID: ${pid})"
        kill -INT $pid 2>/dev/null || kill -9 $pid 2>/dev/null
        rm -f backend${i}.pid
    fi
done

pkill -9 server 2>/dev/null || true

echo -e "${RED}All backend servers stopped${NC}"

rm -f config_server*.json backend*.log backend*.pid
