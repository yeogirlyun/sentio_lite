#!/bin/bash
set -e

echo "════════════════════════════════════════════════════════════"
echo "  SIGOR Mock-Live Replay"
echo "════════════════════════════════════════════════════════════"

FIFO=/tmp/alpaca_bars.fifo
RESULTS=""
DATE=""
SPEED_MS=${SPEED_MS:-60}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --results) RESULTS="$2"; shift 2 ;;
    --date) DATE="$2"; shift 2 ;;
    --speed-ms) SPEED_MS="$2"; shift 2 ;;
    *) shift ;;
  esac
done

echo "Using results: ${RESULTS:-<from date>}"
echo "FIFO: $FIFO"
echo "Speed: $SPEED_MS ms per market minute"

# Create FIFO
rm -f "$FIFO"
mkfifo "$FIFO"

# Start replay into FIFO
if [[ -n "$RESULTS" ]]; then
  python3 scripts/replay_fifo_from_results.py --results "$RESULTS" --fifo "$FIFO" --speed-ms "$SPEED_MS" \
  > logs/live/websocket_bridge.log 2>&1 &
else
  python3 scripts/replay_fifo_from_results.py --date "$DATE" --fifo "$FIFO" --speed-ms "$SPEED_MS" \
  > logs/live/websocket_bridge.log 2>&1 &
fi
BRIDGE_PID=$!
echo "Replay bridge PID: $BRIDGE_PID"

# Start live trader (consumes FIFO exactly like real live)
./build/sentio_lite mock-live --date "${DATE:-2000-01-01}" || true

# Build dashboard from exported results
if [[ -f results.json ]]; then
  python3 scripts/rotation_trading_dashboard_html.py \
    --trades trades.jsonl \
    --output logs/dashboard/mocklive_$(date +%Y%m%d_%H%M%S).html \
    --data-dir data \
    --results results.json \
    --start-equity 100000
fi


