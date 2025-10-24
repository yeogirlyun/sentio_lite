#!/bin/bash
#
# Launch SIGOR Live Trading with Alpaca Paper Account
#
# This script coordinates:
# 1. Alpaca WebSocket bridge (receives live bars)
# 2. C++ SIGOR trading engine (generates signals)
# 3. Alpaca order client (submits orders)
#
# Usage:
#   ./scripts/launch_sigor_live.sh
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  SIGOR Live Trading - Alpaca Paper Account${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

# Check if running from project root
if [ ! -f "build/sentio_lite" ]; then
    echo -e "${RED}❌ Error: Must run from project root${NC}"
    echo "   cd /path/to/sentio_lite && ./scripts/launch_sigor_live.sh"
    exit 1
fi

# Load credentials from config.env if it exists
if [ -f "config.env" ]; then
    echo "Loading credentials from config.env..."
    set -a  # Export all variables
    source config.env
    set +a
fi

# Check for credentials (Polygon preferred, Alpaca fallback)
USE_POLYGON=false
if [ ! -z "$POLYGON_API_KEY" ]; then
    echo -e "${GREEN}✓${NC} Polygon API key found (preferred - full market coverage)"
    echo -e "   API Key: ${POLYGON_API_KEY:0:8}..."
    USE_POLYGON=true
elif [ ! -z "$ALPACA_PAPER_API_KEY" ] && [ ! -z "$ALPACA_PAPER_SECRET_KEY" ]; then
    echo -e "${GREEN}✓${NC} Alpaca credentials found (fallback - IEX data only)"
    echo -e "   API Key: ${ALPACA_PAPER_API_KEY:0:8}..."
else
    echo -e "${RED}❌ Error: No API credentials found${NC}"
    echo ""
    echo "Please set credentials in config.env or export them:"
    echo ""
    echo "Option 1: Polygon (recommended - full market coverage)"
    echo "  export POLYGON_API_KEY='your_key'"
    echo ""
    echo "Option 2: Alpaca (fallback - IEX data only)"
    echo "  export ALPACA_PAPER_API_KEY='your_key'"
    echo "  export ALPACA_PAPER_SECRET_KEY='your_secret'"
    echo ""
    exit 1
fi
echo ""

# Check dependencies
echo "Checking dependencies..."

if ! command -v python3 &> /dev/null; then
    echo -e "${RED}❌ Error: python3 not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓${NC} python3 found"

# Check Python packages
if ! python3 -c "import alpaca" &> /dev/null; then
    echo -e "${YELLOW}⚠${NC}  alpaca-py not installed"
    echo "   Installing: pip3 install alpaca-py"
    pip3 install alpaca-py
fi
echo -e "${GREEN}✓${NC} alpaca-py installed"

if ! python3 -c "import certifi" &> /dev/null; then
    echo -e "${YELLOW}⚠${NC}  certifi not installed"
    echo "   Installing: pip3 install certifi"
    pip3 install certifi
fi
echo -e "${GREEN}✓${NC} certifi installed"

echo ""

# Create logs directory
mkdir -p logs/live

# Fetch today's historical bars for SIGOR warmup (optional but recommended)
echo -e "${BLUE}Fetching today's historical bars for SIGOR warmup...${NC}"
echo "   (This gives SIGOR lookback data for indicators)"
echo ""

python3 scripts/fetch_today_bars.py 2>/dev/null || {
    echo -e "${YELLOW}   ⚠️  Failed to fetch warmup bars${NC}"
    echo "   → SIGOR will start trading after ~30 minutes of live bars"
    echo ""
}

echo ""

# PIDs for cleanup
BRIDGE_PID=""
ORDER_CLIENT_PID=""
TRADER_PID=""

# Cleanup function
cleanup() {
    echo ""
    echo -e "${YELLOW}Shutting down...${NC}"

    if [ ! -z "$TRADER_PID" ]; then
        echo "  Stopping C++ trader (PID: $TRADER_PID)"
        kill $TRADER_PID 2>/dev/null || true
    fi

    if [ ! -z "$ORDER_CLIENT_PID" ]; then
        echo "  Stopping order client (PID: $ORDER_CLIENT_PID)"
        kill $ORDER_CLIENT_PID 2>/dev/null || true
    fi

    if [ ! -z "$BRIDGE_PID" ]; then
        echo "  Stopping websocket bridge (PID: $BRIDGE_PID)"
        kill $BRIDGE_PID 2>/dev/null || true
    fi

    # Cleanup FIFOs
    rm -f /tmp/alpaca_bars.fifo
    rm -f /tmp/alpaca_orders.fifo
    rm -f /tmp/alpaca_responses.fifo

    echo -e "${GREEN}✓${NC} Cleanup complete"
    exit 0
}

# Set trap for cleanup
trap cleanup INT TERM EXIT

echo -e "${BLUE}Starting components...${NC}"
echo ""

# 1. Start WebSocket Bridge (Polygon or Alpaca)
if [ "$USE_POLYGON" = true ]; then
    echo -e "${YELLOW}[1/3]${NC} Starting Polygon WebSocket bridge (full market coverage)..."
    python3 scripts/polygon_websocket_bridge_rotation.py > logs/live/websocket_bridge.log 2>&1 &
    BRIDGE_PID=$!
    echo -e "${GREEN}✓${NC} Polygon bridge started (PID: $BRIDGE_PID)"
else
    echo -e "${YELLOW}[1/3]${NC} Starting Alpaca WebSocket bridge (IEX data only)..."
    python3 scripts/alpaca_websocket_bridge_rotation.py > logs/live/websocket_bridge.log 2>&1 &
    BRIDGE_PID=$!
    echo -e "${GREEN}✓${NC} Alpaca bridge started (PID: $BRIDGE_PID)"
fi
echo "      Log: logs/live/websocket_bridge.log"
sleep 3  # Give it time to create FIFO and connect

# 2. Start Alpaca Order Client (optional - for actual order submission)
# Uncomment to enable order submission:
# echo -e "${YELLOW}[2/3]${NC} Starting Alpaca order client..."
# python3 scripts/alpaca_order_client.py > logs/live/order_client.log 2>&1 &
# ORDER_CLIENT_PID=$!
# echo -e "${GREEN}✓${NC} Order client started (PID: $ORDER_CLIENT_PID)"
# echo "      Log: logs/live/order_client.log"
# sleep 1

echo -e "${YELLOW}[2/3]${NC} Order client: ${YELLOW}DISABLED${NC} (observation mode only)"
echo "      Enable in script to submit real orders"

# 3. Start C++ SIGOR Trader
echo -e "${YELLOW}[3/3]${NC} Starting SIGOR trading engine..."
echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

# Run trader in foreground (so we can see output and handle Ctrl+C)
./build/sentio_lite live --strategy sigor

# This will block until trader exits or Ctrl+C

# Cleanup will be called automatically via trap
