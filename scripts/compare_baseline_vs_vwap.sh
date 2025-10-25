#!/bin/bash
#
# Baseline vs VWAP-Enhanced SIGOR Comparison (Oct 20-24, 2025)
#
# Compares performance of:
# 1. Baseline SIGOR (simple VWAP detector)
# 2. VWAP-enhanced SIGOR (statistical bands detector)
#

set -e

DATES=("2025-10-20" "2025-10-21" "2025-10-22" "2025-10-23" "2025-10-24")
OUTPUT_DIR="results/vwap_comparison"
LOG_FILE="${OUTPUT_DIR}/comparison_test.log"

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Clear previous log
> "${LOG_FILE}"

echo "=====================================================================" | tee -a "${LOG_FILE}"
echo "  Baseline vs VWAP-Enhanced SIGOR Comparison" | tee -a "${LOG_FILE}"
echo "  Test Period: Oct 20-24, 2025 (5 days)" | tee -a "${LOG_FILE}"
echo "=====================================================================" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

# Build with current code (VWAP-enhanced)
echo "Building VWAP-enhanced version..." | tee -a "${LOG_FILE}"
cmake --build build > /dev/null 2>&1

echo "" | tee -a "${LOG_FILE}"
echo "====================================================================="  | tee -a "${LOG_FILE}"
echo "  PART 1: VWAP-ENHANCED SIGOR (Statistical Bands)"  | tee -a "${LOG_FILE}"
echo "====================================================================="  | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

# Run VWAP-enhanced version on each date
for DATE in "${DATES[@]}"; do
    echo "---------------------------------------------------------------" | tee -a "${LOG_FILE}"
    echo "Testing VWAP-Enhanced: $DATE" | tee -a "${LOG_FILE}"
    echo "---------------------------------------------------------------" | tee -a "${LOG_FILE}"

    ./build/sentio_lite mock \
        --date "$DATE" \
        2>&1 | tee -a "${LOG_FILE}"

    echo "" | tee -a "${LOG_FILE}"
done

echo "=====================================================================" | tee -a "${LOG_FILE}"
echo "  Comparison Complete" | tee -a "${LOG_FILE}"
echo "  Results saved to: ${OUTPUT_DIR}" | tee -a "${LOG_FILE}"
echo "=====================================================================" | tee -a "${LOG_FILE}"

# Extract summary metrics
echo "" | tee -a "${LOG_FILE}"
echo "SUMMARY EXTRACTION:" | tee -a "${LOG_FILE}"
echo "-------------------" | tee -a "${LOG_FILE}"

python3 << 'EOF' | tee -a "${LOG_FILE}"
import re
import sys

log_file = "results/vwap_comparison/comparison_test.log"

with open(log_file) as f:
    content = f.read()

# Extract metrics for each date
dates = ["2025-10-20", "2025-10-21", "2025-10-22", "2025-10-23", "2025-10-24"]
vwap_metrics = []

for date in dates:
    # Find VWAP-enhanced section for this date
    date_match = re.search(rf"Testing VWAP-Enhanced: {date}.*?(?=Testing VWAP-Enhanced:|Comparison Complete)", content, re.DOTALL)
    if not date_match:
        continue

    section = date_match.group(0)

    # Extract MRD
    mrd_match = re.search(r"MRD[:\s\(Daily\)]+([+-]?[0-9.]+)%", section)
    mrd = float(mrd_match.group(1)) if mrd_match else 0.0

    # Extract Sharpe
    sharpe_match = re.search(r"Sharpe[:\s]+([0-9.-]+)", section)
    sharpe = float(sharpe_match.group(1)) if sharpe_match else 0.0

    # Extract Total P&L
    pnl_match = re.search(r"Total Return[:\s]+([+-]?[0-9.]+)%", section)
    pnl = float(pnl_match.group(1)) if pnl_match else 0.0

    # Extract Win Rate
    winrate_match = re.search(r"Win Rate[:\s]+([0-9.]+)%", section)
    winrate = float(winrate_match.group(1)) if winrate_match else 0.0

    # Extract Trades
    trades_match = re.search(r"Total Trades[:\s]+([0-9]+)", section)
    trades = int(trades_match.group(1)) if trades_match else 0

    vwap_metrics.append({
        "date": date,
        "mrd": mrd,
        "sharpe": sharpe,
        "pnl": pnl,
        "winrate": winrate,
        "trades": trades
    })

# Print comparison table
print("\n" + "="*95)
print("VWAP-ENHANCED SIGOR PERFORMANCE")
print("="*95)
print(f"{'Date':<15} {'MRD %':<10} {'Sharpe':<10} {'P&L %':<10} {'Win Rate %':<12} {'Trades':<8}")
print("-"*95)

total_mrd = 0
total_sharpe = 0
total_pnl = 0
total_winrate = 0
total_trades = 0

for m in vwap_metrics:
    print(f"{m['date']:<15} {m['mrd']:<10.2f} {m['sharpe']:<10.2f} {m['pnl']:<10.2f} {m['winrate']:<12.1f} {m['trades']:<8}")
    total_mrd += m['mrd']
    total_sharpe += m['sharpe']
    total_pnl += m['pnl']
    total_winrate += m['winrate']
    total_trades += m['trades']

print("-"*95)
print(f"{'AVERAGE':<15} {total_mrd/5:<10.2f} {total_sharpe/5:<10.2f} {total_pnl:<10.2f} {total_winrate/5:<12.1f} {total_trades:<8}")
print("="*95)

# Load baseline for comparison
import json
with open("results/baseline_evaluation/baseline_summary.json") as f:
    baseline = json.load(f)

print("\n" + "="*95)
print("COMPARISON: VWAP-ENHANCED vs BASELINE")
print("="*95)
print(f"{'Metric':<20} {'Baseline':<15} {'VWAP-Enhanced':<15} {'Difference':<15} {'%Change':<10}")
print("-"*95)

avg_mrd_vwap = total_mrd / 5
avg_mrd_baseline = baseline['baseline_metrics']['avg_mrd']
print(f"{'Avg MRD (daily %)':<20} {avg_mrd_baseline:<15.3f} {avg_mrd_vwap:<15.3f} {avg_mrd_vwap - avg_mrd_baseline:<15.3f} {((avg_mrd_vwap - avg_mrd_baseline) / abs(avg_mrd_baseline) * 100) if avg_mrd_baseline != 0 else 0:<10.1f}%")

avg_winrate_vwap = total_winrate / 5
avg_winrate_baseline = baseline['baseline_metrics']['avg_winrate']
print(f"{'Avg Win Rate (%)':<20} {avg_winrate_baseline:<15.1f} {avg_winrate_vwap:<15.1f} {avg_winrate_vwap - avg_winrate_baseline:<15.1f} {((avg_winrate_vwap - avg_winrate_baseline) / avg_winrate_baseline * 100):<10.1f}%")

total_pnl_baseline = baseline['baseline_metrics']['total_pnl']
print(f"{'Total P&L (%)':<20} {total_pnl_baseline:<15.2f} {total_pnl:<15.2f} {total_pnl - total_pnl_baseline:<15.2f} {((total_pnl - total_pnl_baseline) / abs(total_pnl_baseline) * 100) if total_pnl_baseline != 0 else 0:<10.1f}%")

print("="*95)

# Save VWAP-enhanced summary
summary = {
    "test_period": {
        "start_date": "2025-10-20",
        "end_date": "2025-10-24",
        "num_days": 5
    },
    "vwap_enhanced_metrics": {
        "avg_mrd": avg_mrd_vwap,
        "avg_sharpe": total_sharpe / 5,
        "total_pnl": total_pnl,
        "avg_winrate": avg_winrate_vwap,
        "total_trades": total_trades
    },
    "daily_metrics": vwap_metrics,
    "comparison_vs_baseline": {
        "mrd_improvement": avg_mrd_vwap - avg_mrd_baseline,
        "winrate_improvement": avg_winrate_vwap - avg_winrate_baseline,
        "pnl_improvement": total_pnl - total_pnl_baseline
    }
}

with open("results/vwap_comparison/vwap_enhanced_summary.json", 'w') as f:
    json.dump(summary, f, indent=2)

print("\n✅ VWAP-enhanced summary saved to: results/vwap_comparison/vwap_enhanced_summary.json\n")

EOF

echo "=====================================================================" | tee -a "${LOG_FILE}"
echo "✅ COMPARISON COMPLETE" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"
echo "Next steps:" | tee -a "${LOG_FILE}"
echo "1. Review comparison metrics above" | tee -a "${LOG_FILE}"
echo "2. If VWAP-enhanced shows improvement, run optimization" | tee -a "${LOG_FILE}"
echo "3. Otherwise, adjust VWAP Bands parameters and retry" | tee -a "${LOG_FILE}"
echo "=====================================================================" | tee -a "${LOG_FILE}"
