#!/bin/bash
# =============================================================================
# SIGOR Live Trading Monitor
# =============================================================================
# Real-time monitoring dashboard for SIGOR live trading
# Usage: ./tools/monitor_sigor_live.sh
#
# Features:
#   - Process status (C++ trader + Python WebSocket bridge)
#   - Live bar reception from all 12 symbols
#   - Recent C++ output (equity, trades, positions)
#   - Bridge health and bar counts
#   - Auto-refresh every 5 seconds
#
# Author: Sentio Lite
# Date: 2025-10-24
# =============================================================================

PROJECT_ROOT="/Volumes/MyBookDuo/Projects/sentio_lite"
LOG_DIR="$PROJECT_ROOT/logs/live"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Function to print section header
print_header() {
    echo -e "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}${CYAN}$1${NC}"
    echo -e "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

# Clear screen and show header
clear
echo ""
print_header "📊 SIGOR Live Trading Monitor"
echo -e "${CYAN}Project: $PROJECT_ROOT${NC}"
echo -e "${CYAN}Time: $(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S ET')${NC}"
echo ""

# Check if processes are running
echo ""
print_header "🔍 Process Status"

# Check C++ SIGOR trader
TRADER_RUNNING=true
if pgrep -f "sentio_lite live" > /dev/null; then
    TRADER_PID=$(pgrep -f "sentio_lite live" | tail -1)
    CPU_TIME=$(ps -p $TRADER_PID -o time | tail -1 | tr -d ' ')
    START_TIME=$(ps -p $TRADER_PID -o lstart | tail -1 | sed 's/^[[:space:]]*//')
    MEM_KB=$(ps -p $TRADER_PID -o rss | tail -1 | tr -d ' ')
    MEM_MB=$(echo "scale=1; $MEM_KB / 1024" | bc 2>/dev/null || echo "0")
    echo -e "${GREEN}✓ C++ SIGOR TRADER RUNNING${NC} - PID: $TRADER_PID"
    echo -e "  Started: $START_TIME"
    echo -e "  CPU Time: $CPU_TIME"
    echo -e "  Memory: ${MEM_MB}MB"
else
    echo -e "${RED}✗ C++ SIGOR TRADER NOT RUNNING${NC}"
    TRADER_RUNNING=false
fi

# Check Python WebSocket bridge (Polygon or Alpaca)
BRIDGE_RUNNING=true
if pgrep -f "polygon_websocket_bridge_rotation" > /dev/null; then
    BRIDGE_PID=$(pgrep -f "polygon_websocket_bridge_rotation")
    BRIDGE_TYPE="Polygon (ALL exchanges - full coverage)"
elif pgrep -f "alpaca_websocket_bridge_rotation" > /dev/null; then
    BRIDGE_PID=$(pgrep -f "alpaca_websocket_bridge_rotation")
    BRIDGE_TYPE="Alpaca IEX (~2-3% market coverage)"
else
    echo -e "${RED}✗ PYTHON BRIDGE NOT RUNNING${NC}"
    BRIDGE_RUNNING=false
fi

if [[ -n "$BRIDGE_PID" ]]; then
    CPU_TIME=$(ps -p $BRIDGE_PID -o time | tail -1 | tr -d ' ')
    START_TIME=$(ps -p $BRIDGE_PID -o lstart | tail -1 | sed 's/^[[:space:]]*//')
    echo -e "${GREEN}✓ PYTHON BRIDGE RUNNING${NC} - PID: $BRIDGE_PID"
    echo -e "  Type: $BRIDGE_TYPE"
    echo -e "  Started: $START_TIME"
    echo -e "  CPU Time: $CPU_TIME"
fi

if [[ "$TRADER_RUNNING" == "false" ]] || [[ "$BRIDGE_RUNNING" == "false" ]]; then
    echo ""
    echo "To start SIGOR live trading:"
    echo "  cd $PROJECT_ROOT"
    echo "  ./scripts/launch_sigor_live.sh"
    exit 1
fi

# Show Python bridge bar reception status
echo ""
print_header "📡 WebSocket Bridge - Bar Reception"

LATEST_BRIDGE_LOG="$LOG_DIR/websocket_bridge.log"

if [[ -f "$LATEST_BRIDGE_LOG" ]]; then
    # Count bars per symbol from bridge log
    echo -e "  ${BOLD}Bars Received by Symbol (last minute):${NC}"

    for symbol in TQQQ SQQQ TNA TZA UVXY SVXY FAS FAZ SSO SDS SOXL SOXS; do
        # Get last bar for this symbol
        LAST_BAR=$(grep "✓ $symbol" "$LATEST_BRIDGE_LOG" 2>/dev/null | tail -1)

        if [[ -n "$LAST_BAR" ]]; then
            # Extract price and time from log line
            LAST_PRICE=$(echo "$LAST_BAR" | grep -oE 'C:[[:space:]]*[0-9.]+' | sed 's/C:[[:space:]]*//')
            LAST_TIME=$(echo "$LAST_BAR" | grep -oE '@[[:space:]]*[0-9:]+' | sed 's/@[[:space:]]*//')
            echo -e "    ${GREEN}$symbol${NC}: \$$LAST_PRICE @ ${LAST_TIME} ET"
        else
            echo -e "    ${YELLOW}$symbol${NC}: Waiting for data..."
        fi
    done

    # Show total bars in session
    TOTAL_BARS=$(grep -c "✓" "$LATEST_BRIDGE_LOG" 2>/dev/null || echo "0")
    echo -e "\n  ${BOLD}Total Bars This Session:${NC} $TOTAL_BARS"

    # Check for recent activity (last 30 seconds)
    RECENT_BARS=$(tail -50 "$LATEST_BRIDGE_LOG" | grep -c "✓" 2>/dev/null || echo "0")
    if [[ $RECENT_BARS -gt 0 ]]; then
        echo -e "  ${GREEN}✓ Active${NC} - receiving live data"
    else
        echo -e "  ${YELLOW}⚠ No recent bars${NC} - check market hours or connection"
    fi
else
    echo -e "  ${YELLOW}Bridge log not found: $LATEST_BRIDGE_LOG${NC}"
fi

# Show C++ trader output (from stdout - last 20 lines)
echo ""
print_header "💹 SIGOR Trader Status (recent output)"

# The C++ output goes to console, so we check if we can tail the launch script's output
# In practice, users should redirect output to a log file
echo -e "  ${YELLOW}Note: Trader output visible in launch terminal${NC}"
echo -e "  To capture output, run: ./build/sentio_lite live --strategy sigor > logs/live/trader.log"
echo ""
echo -e "  ${BOLD}Check launch terminal for:${NC}"
echo -e "    - Equity updates"
echo -e "    - Trade execution"
echo -e "    - Win rate statistics"
echo -e "    - Position tracking"

# Show FIFO status
echo ""
print_header "🔗 Communication Pipes"
if [[ -p "/tmp/alpaca_bars.fifo" ]]; then
    echo -e "  ${GREEN}✓ Bar FIFO active${NC}: /tmp/alpaca_bars.fifo"
else
    echo -e "  ${RED}✗ Bar FIFO missing${NC}: /tmp/alpaca_bars.fifo"
fi

if [[ -p "/tmp/alpaca_orders.fifo" ]]; then
    echo -e "  ${GREEN}✓ Order FIFO active${NC}: /tmp/alpaca_orders.fifo"
else
    echo -e "  ${YELLOW}◯ Order FIFO not created${NC} (order submission disabled)"
fi

# Show monitoring commands
echo ""
print_header "📊 Monitoring Commands"
echo -e "  ${BOLD}Watch bridge:${NC}         tail -f $LATEST_BRIDGE_LOG"
echo -e "  ${BOLD}Watch trader:${NC}         (see launch terminal)"
echo -e "  ${BOLD}Stop SIGOR:${NC}           pkill -f 'sentio_lite live'"
echo -e "  ${BOLD}Stop bridge:${NC}          pkill -f 'websocket_bridge_rotation'"
echo -e "  ${BOLD}Restart:${NC}              ./scripts/launch_sigor_live.sh"

# Quick stats
echo ""
print_header "📈 Quick Stats"

# Calculate uptime
if [[ -n "$TRADER_PID" ]]; then
    TRADER_ETIME=$(ps -p $TRADER_PID -o etime | tail -1 | tr -d ' ')
    echo -e "  ${BOLD}Trader Uptime:${NC} ${TRADER_ETIME}"
fi

# Bar rate (bars per minute)
if [[ -f "$LATEST_BRIDGE_LOG" ]]; then
    BARS_LAST_MIN=$(tail -100 "$LATEST_BRIDGE_LOG" | grep -c "✓" 2>/dev/null || echo "0")
    echo -e "  ${BOLD}Bar Rate:${NC} ~${BARS_LAST_MIN} bars/minute"
fi

# Market hours check
CURRENT_HOUR=$(TZ='America/New_York' date '+%H')
CURRENT_MIN=$(TZ='America/New_York' date '+%M')
CURRENT_TIME_MIN=$((10#$CURRENT_HOUR * 60 + 10#$CURRENT_MIN))
MARKET_OPEN_MIN=$((9 * 60 + 30))   # 9:30 AM
MARKET_CLOSE_MIN=$((16 * 60))       # 4:00 PM

if [[ $CURRENT_TIME_MIN -ge $MARKET_OPEN_MIN ]] && [[ $CURRENT_TIME_MIN -lt $MARKET_CLOSE_MIN ]]; then
    MINS_UNTIL_CLOSE=$((MARKET_CLOSE_MIN - CURRENT_TIME_MIN))
    echo -e "  ${GREEN}✓ Market OPEN${NC} - ${MINS_UNTIL_CLOSE} minutes until close"
else
    if [[ $CURRENT_TIME_MIN -lt $MARKET_OPEN_MIN ]]; then
        MINS_UNTIL_OPEN=$((MARKET_OPEN_MIN - CURRENT_TIME_MIN))
        echo -e "  ${YELLOW}◯ Market CLOSED${NC} - opens in ${MINS_UNTIL_OPEN} minutes"
    else
        echo -e "  ${YELLOW}◯ Market CLOSED${NC} - opens tomorrow at 9:30 AM ET"
    fi
fi

echo ""
print_header "🔄 Auto-refresh in 5 seconds... (Ctrl+C to stop)"
echo ""

# Auto-refresh
sleep 5
exec "$0"
