#!/bin/bash
#
# Launch live trading with optimized parameters
#
# Usage:
#   ./scripts/launch_live.sh [params_file]
#
# Example:
#   ./scripts/launch_live.sh optimal_params.json
#

set -e

PARAMS_FILE=${1:-optimal_params.json}

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║   Sentio Lite - Live Trading Launch                      ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Check if parameters file exists
if [ ! -f "$PARAMS_FILE" ]; then
    echo "❌ Error: Parameters file not found: $PARAMS_FILE"
    echo "   Run optimization first: ./scripts/optimize_for_tomorrow.sh"
    exit 1
fi

# Check if jq is available
if ! command -v jq &> /dev/null; then
    echo "❌ Error: jq not installed (required for JSON parsing)"
    echo "   macOS: brew install jq"
    echo "   Linux: sudo apt-get install jq"
    exit 1
fi

# Extract parameters
echo "📂 Loading parameters from: $PARAMS_FILE"
echo ""

TEST_DATE=$(jq -r '.test_date' "$PARAMS_FILE")
MAX_POS=$(jq -r '.parameters.max_positions' "$PARAMS_FILE")
STOP_LOSS=$(jq -r '.parameters.stop_loss' "$PARAMS_FILE")
PROFIT=$(jq -r '.parameters.profit_target' "$PARAMS_FILE")
LAMBDA=$(jq -r '.parameters.lambda' "$PARAMS_FILE")
ENABLE_WARMUP=$(jq -r '.parameters.enable_warmup' "$PARAMS_FILE")
WARMUP_OBS=$(jq -r '.parameters.warmup_obs_days // 2' "$PARAMS_FILE")
WARMUP_SIM=$(jq -r '.parameters.warmup_sim_days // 5' "$PARAMS_FILE")

echo "Parameters for $TEST_DATE:"
echo "  Max Positions: $MAX_POS"
echo "  Stop Loss: $STOP_LOSS"
echo "  Profit Target: $PROFIT"
echo "  Lambda: $LAMBDA"
echo "  Warmup Enabled: $ENABLE_WARMUP"
if [ "$ENABLE_WARMUP" == "true" ]; then
    echo "  Warmup Observation Days: $WARMUP_OBS"
    echo "  Warmup Simulation Days: $WARMUP_SIM"
fi
echo ""

# Show expected metrics
echo "📈 Expected Performance (from optimization):"
RETURN=$(jq -r '.metrics.total_return // "N/A"' "$PARAMS_FILE")
PF=$(jq -r '.metrics.profit_factor // "N/A"' "$PARAMS_FILE")
WR=$(jq -r '.metrics.win_rate // "N/A"' "$PARAMS_FILE")

echo "  Return: ${RETURN}%"
echo "  Profit Factor: ${PF}"
echo "  Win Rate: ${WR}%"
echo ""

# Safety check
read -p "⚠️  Ready to launch LIVE trading? (yes/no): " CONFIRM
if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted by user."
    exit 0
fi

echo ""
echo "🚀 Launching live trading..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Build command
CMD="./build/sentio_lite live \
    --max-positions $MAX_POS \
    --stop-loss $STOP_LOSS \
    --profit-target $PROFIT \
    --lambda $LAMBDA \
    --warmup-days 1"

# Add warmup flags if enabled
if [ "$ENABLE_WARMUP" == "true" ]; then
    CMD="$CMD --enable-warmup --warmup-obs-days $WARMUP_OBS --warmup-sim-days $WARMUP_SIM"
fi

# Execute
echo "Command: $CMD"
echo ""

$CMD
