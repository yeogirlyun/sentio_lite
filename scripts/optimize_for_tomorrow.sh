#!/bin/bash
#
# Daily optimization script for next day's trading
#
# Usage:
#   ./scripts/optimize_for_tomorrow.sh [n_trials]
#
# Example:
#   ./scripts/optimize_for_tomorrow.sh 100
#

set -e  # Exit on error

# Configuration
N_TRIALS=${1:-100}  # Default 100 trials if not specified
N_JOBS=1            # Number of parallel jobs (set to number of CPU cores for speed)
OPT_DAYS=30         # Days of historical data for optimization

# Calculate tomorrow's date
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    TOMORROW=$(date -v+1d +"%Y-%m-%d")
else
    # Linux
    TOMORROW=$(date -d "tomorrow" +"%Y-%m-%d")
fi

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║   Sentio Lite - Daily Parameter Optimization             ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "Target Date: $TOMORROW"
echo "Optimization Trials: $N_TRIALS"
echo "Historical Data: $OPT_DAYS days"
echo "Parallel Jobs: $N_JOBS"
echo ""

# Check if binary exists
if [ ! -f "./build/sentio_lite" ]; then
    echo "❌ Error: Binary not found at ./build/sentio_lite"
    echo "   Run: cmake --build build"
    exit 1
fi

# Check if Python script exists
if [ ! -f "tools/optimize_warmup.py" ]; then
    echo "❌ Error: Optimization script not found"
    exit 1
fi

# Check if required Python packages are installed
python3 -c "import optuna" 2>/dev/null || {
    echo "❌ Error: Optuna not installed"
    echo "   Run: pip3 install optuna"
    exit 1
}

# Run optimization
echo "🔍 Starting optimization..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

python3 tools/optimize_warmup.py \
    --test-date "$TOMORROW" \
    --n-trials "$N_TRIALS" \
    --optimization-days "$OPT_DAYS" \
    --n-jobs "$N_JOBS" \
    --mode live

# Check if optimization succeeded
if [ $? -eq 0 ]; then
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "✅ Optimization Complete!"
    echo ""

    # Display best parameters
    if [ -f "optimal_params.json" ]; then
        echo "📊 Best Parameters for $TOMORROW:"
        echo ""

        # Check if jq is available for pretty printing
        if command -v jq &> /dev/null; then
            jq '.parameters' optimal_params.json
            echo ""
            echo "📈 Expected Metrics:"
            jq '.metrics' optimal_params.json
        else
            # Fallback to cat if jq not available
            cat optimal_params.json
        fi

        echo ""
        echo "💾 Parameters saved to: optimal_params.json"
        echo ""

        # Extract key metrics for quick review
        if command -v jq &> /dev/null; then
            RETURN=$(jq -r '.metrics.total_return // 0' optimal_params.json)
            PF=$(jq -r '.metrics.profit_factor // 0' optimal_params.json)
            WR=$(jq -r '.metrics.win_rate // 0' optimal_params.json)
            SCORE=$(jq -r '.score // 0' optimal_params.json)

            echo "Quick Assessment:"
            echo "  Return: ${RETURN}%"
            echo "  Profit Factor: ${PF}"
            echo "  Win Rate: ${WR}%"
            echo "  Score: ${SCORE}"
            echo ""

            # Decision recommendation
            if (( $(echo "$RETURN > 0" | bc -l) )) && (( $(echo "$PF > 1.2" | bc -l) )); then
                echo "✅ RECOMMENDATION: Deploy to live trading"
                echo "   Strong positive metrics. Safe to proceed."
            elif (( $(echo "$RETURN > 0" | bc -l) )) && (( $(echo "$PF > 1.0" | bc -l) )); then
                echo "⚠️  RECOMMENDATION: Paper trade first"
                echo "   Marginal positive metrics. Monitor closely."
            else
                echo "❌ RECOMMENDATION: Skip trading day"
                echo "   Negative or weak metrics. Do not deploy."
            fi
        fi

        echo ""
        echo "Next Steps:"
        echo "  1. Review parameters above"
        echo "  2. Check recommendation"
        echo "  3. If good, deploy to live at 9:30 AM:"
        echo ""
        echo "     ./scripts/launch_live.sh"
        echo ""

    else
        echo "⚠️  Warning: optimal_params.json not found"
    fi
else
    echo ""
    echo "❌ Optimization failed. Check errors above."
    exit 1
fi
