#!/bin/bash
#
# Generate Sigor Strategy Dashboard for sentio_lite
# Usage: ./generate_sigor_dashboard.sh YYYY-MM-DD
#

DATE=${1:-"2024-10-23"}
SYMBOL="SPXL"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  SIGOR STRATEGY DASHBOARD GENERATOR"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Date: $DATE"
echo "  Symbol: $SYMBOL"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Create results directory
mkdir -p results

# Load config
CONFIG_FILE="config/sigor_2024-10-16_to_2024-10-22_20251024_configs.json"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "❌ Config file not found: $CONFIG_FILE"
    exit 1
fi

echo "Running Sigor tests with TOP, BEST, and BALANCED configs..."
echo ""

# Test TOP config
echo "1/3 Testing TOP config..."
cat << EOF > /tmp/sigor_top_params.json
{
  "k": 2.3,
  "w_boll": 1.5,
  "w_rsi": 0.9,
  "w_mom": 1.2,
  "w_vwap": 0.9,
  "w_orb": 0.6,
  "w_ofi": 0.7,
  "w_vol": 0.2,
  "win_boll": 26,
  "win_rsi": 12,
  "win_mom": 11,
  "win_vwap": 25,
  "orb_opening_bars": 24,
  "vol_window": 23,
  "warmup_bars": 50
}
EOF

SIGOR_CONFIG=/tmp/sigor_top_params.json ./build/test_sigor $DATE --symbol $SYMBOL > results/sigor_top_${DATE}.txt 2>&1
echo "  ✅ TOP config complete"

# Test BEST config
echo "2/3 Testing BEST config..."
cat << EOF > /tmp/sigor_best_params.json
{
  "k": 2.1,
  "w_boll": 1.4,
  "w_rsi": 0.9,
  "w_mom": 1.2,
  "w_vwap": 0.9,
  "w_orb": 0.6,
  "w_ofi": 0.8,
  "w_vol": 0.3,
  "win_boll": 27,
  "win_rsi": 12,
  "win_mom": 11,
  "win_vwap": 25,
  "orb_opening_bars": 25,
  "vol_window": 23,
  "warmup_bars": 50
}
EOF

SIGOR_CONFIG=/tmp/sigor_best_params.json ./build/test_sigor $DATE --symbol $SYMBOL > results/sigor_best_${DATE}.txt 2>&1
echo "  ✅ BEST config complete"

# Test BALANCED config
echo "3/3 Testing BALANCED config..."
cat << EOF > /tmp/sigor_balanced_params.json
{
  "k": 2.2,
  "w_boll": 1.4,
  "w_rsi": 0.9,
  "w_mom": 1.2,
  "w_vwap": 0.9,
  "w_orb": 0.6,
  "w_ofi": 0.8,
  "w_vol": 0.2,
  "win_boll": 26,
  "win_rsi": 12,
  "win_mom": 11,
  "win_vwap": 25,
  "orb_opening_bars": 24,
  "vol_window": 23,
  "warmup_bars": 50
}
EOF

SIGOR_CONFIG=/tmp/sigor_balanced_params.json ./build/test_sigor $DATE --symbol $SYMBOL > results/sigor_balanced_${DATE}.txt 2>&1
echo "  ✅ BALANCED config complete"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  RESULTS SUMMARY"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Extract and display results
for config in top best balanced; do
    result_file="results/sigor_${config}_${DATE}.txt"
    echo "📊 ${config^^} CONFIG:"
    grep "MRD:" $result_file | tail -1
    grep "Total trades:" $result_file | tail -1
    grep "Win rate:" $result_file | tail -1
    grep "Profit factor:" $result_file | tail -1
    echo ""
done

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ Dashboard data saved to results/ directory"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
