#!/bin/bash
# Compare baseline vs optimized position sizing on validation dates

VAL_DATES=("2025-10-20" "2025-10-21" "2025-10-22" "2025-10-23" "2025-10-24")

echo "════════════════════════════════════════════════════════════════"
echo "POSITION SIZING COMPARISON: BASELINE vs OPTIMIZED"
echo "════════════════════════════════════════════════════════════════"
echo ""

# Test OPTIMIZED
echo "Testing OPTIMIZED position sizing..."
cp config/trading_params_pos_sizing_20251020-1024.json config/trading_params.json
cmake --build build > /dev/null 2>&1

opt_total_mrd=0
opt_count=0

for date in "${VAL_DATES[@]}"; do
    result=$(./build/sentio_lite mock --date "$date" --no-dashboard 2>&1 | grep "MRD (Daily):" | awk '{print $3}' | sed 's/%//' | sed 's/+//')
    if [ ! -z "$result" ]; then
        echo "  $date: ${result}%"
        opt_total_mrd=$(echo "$opt_total_mrd + $result" | bc -l)
        opt_count=$((opt_count + 1))
    fi
done

opt_avg_mrd=$(echo "scale=3; $opt_total_mrd / $opt_count" | bc -l)
echo ""
echo "  OPTIMIZED Average MRD: ${opt_avg_mrd}%"
echo ""

# Test BASELINE
echo "Testing BASELINE position sizing..."
cp config/trading_params.json.baseline_pos_sizing config/trading_params.json
cmake --build build > /dev/null 2>&1

base_total_mrd=0
base_count=0

for date in "${VAL_DATES[@]}"; do
    result=$(./build/sentio_lite mock --date "$date" --no-dashboard 2>&1 | grep "MRD (Daily):" | awk '{print $3}' | sed 's/%//' | sed 's/+//')
    if [ ! -z "$result" ]; then
        echo "  $date: ${result}%"
        base_total_mrd=$(echo "$base_total_mrd + $result" | bc -l)
        base_count=$((base_count + 1))
    fi
done

base_avg_mrd=$(echo "scale=3; $base_total_mrd / $base_count" | bc -l)
echo ""
echo "  BASELINE Average MRD: ${base_avg_mrd}%"
echo ""

# Calculate improvement
improvement=$(echo "scale=3; $opt_avg_mrd - $base_avg_mrd" | bc -l)

echo "════════════════════════════════════════════════════════════════"
echo "SUMMARY"
echo "════════════════════════════════════════════════════════════════"
echo "  Baseline Average MRD:  ${base_avg_mrd}%"
echo "  Optimized Average MRD: ${opt_avg_mrd}%"
echo "  Improvement:           ${improvement}%"
echo ""

if (( $(echo "$improvement > 0" | bc -l) )); then
    echo "  ✅ OPTIMIZED parameters are BETTER"
else
    echo "  ⚠️  BASELINE parameters are BETTER"
fi

echo "════════════════════════════════════════════════════════════════"

# Restore optimized config
cp config/trading_params_pos_sizing_20251020-1024.json config/trading_params.json
cmake --build build > /dev/null 2>&1
