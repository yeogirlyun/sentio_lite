#!/bin/bash
#
# Baseline SIGOR Performance Test (Oct 20-24, 2025)
#
# Runs current SIGOR on 5 consecutive days and collects metrics
# for baseline performance evaluation before detector enhancements
#

set -e

DATES=("2025-10-20" "2025-10-21" "2025-10-22" "2025-10-23" "2025-10-24")
OUTPUT_DIR="results/baseline_evaluation"
LOG_FILE="${OUTPUT_DIR}/baseline_test.log"

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Clear previous log
> "${LOG_FILE}"

echo "=====================================================================" | tee -a "${LOG_FILE}"
echo "  SIGOR Baseline Performance Evaluation" | tee -a "${LOG_FILE}"
echo "  Test Period: Oct 20-24, 2025 (5 days)" | tee -a "${LOG_FILE}"
echo "=====================================================================" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

# Run SIGOR on each date
for DATE in "${DATES[@]}"; do
    echo "---------------------------------------------------------------" | tee -a "${LOG_FILE}"
    echo "Testing: $DATE" | tee -a "${LOG_FILE}"
    echo "---------------------------------------------------------------" | tee -a "${LOG_FILE}"

    # Run SIGOR mock mode
    ./build/sentio_lite mock \
        --date "$DATE" \
        2>&1 | tee -a "${LOG_FILE}"

    echo "" | tee -a "${LOG_FILE}"
done

echo "=====================================================================" | tee -a "${LOG_FILE}"
echo "  Baseline Test Complete" | tee -a "${LOG_FILE}"
echo "  Results saved to: ${OUTPUT_DIR}" | tee -a "${LOG_FILE}"
echo "=====================================================================" | tee -a "${LOG_FILE}"

# Extract summary metrics
echo "" | tee -a "${LOG_FILE}"
echo "SUMMARY EXTRACTION:" | tee -a "${LOG_FILE}"
echo "-------------------" | tee -a "${LOG_FILE}"

python3 << 'EOF' | tee -a "${LOG_FILE}"
import re
import sys

log_file = "results/baseline_evaluation/baseline_test.log"

with open(log_file) as f:
    content = f.read()

# Extract metrics for each date
dates = ["2025-10-20", "2025-10-21", "2025-10-22", "2025-10-23", "2025-10-24"]
metrics = []

for date in dates:
    # Find section for this date
    date_match = re.search(rf"Testing: {date}.*?(?=Testing:|Baseline Test Complete)", content, re.DOTALL)
    if not date_match:
        continue

    section = date_match.group(0)

    # Extract MRD (handles both "MRD: -0.30%" and "MRD (Daily): -0.30%")
    mrd_match = re.search(r"MRD[:\s\(Daily\)]+([+-]?[0-9.]+)%", section)
    mrd = float(mrd_match.group(1)) if mrd_match else 0.0

    # Extract Sharpe
    sharpe_match = re.search(r"Sharpe[:\s]+([0-9.-]+)", section)
    sharpe = float(sharpe_match.group(1)) if sharpe_match else 0.0

    # Extract Total P&L
    pnl_match = re.search(r"Total P&L[:\s]+([0-9.-]+)%?", section)
    pnl = float(pnl_match.group(1)) if pnl_match else 0.0

    # Extract Win Rate
    winrate_match = re.search(r"Win Rate[:\s]+([0-9.]+)%?", section)
    winrate = float(winrate_match.group(1)) if winrate_match else 0.0

    # Extract Trades
    trades_match = re.search(r"(?:Total )?Trades[:\s]+([0-9]+)", section)
    trades = int(trades_match.group(1)) if trades_match else 0

    metrics.append({
        "date": date,
        "mrd": mrd,
        "sharpe": sharpe,
        "pnl": pnl,
        "winrate": winrate,
        "trades": trades
    })

# Print summary table
print("\n" + "="*80)
print("BASELINE SIGOR PERFORMANCE SUMMARY")
print("="*80)
print(f"{'Date':<15} {'MRD %':<10} {'Sharpe':<10} {'P&L %':<10} {'Win Rate %':<12} {'Trades':<8}")
print("-"*80)

total_mrd = 0
total_sharpe = 0
total_pnl = 0
total_winrate = 0
total_trades = 0

for m in metrics:
    print(f"{m['date']:<15} {m['mrd']:<10.2f} {m['sharpe']:<10.2f} {m['pnl']:<10.2f} {m['winrate']:<12.1f} {m['trades']:<8}")
    total_mrd += m['mrd']
    total_sharpe += m['sharpe']
    total_pnl += m['pnl']
    total_winrate += m['winrate']
    total_trades += m['trades']

print("-"*80)
print(f"{'AVERAGE':<15} {total_mrd/5:<10.2f} {total_sharpe/5:<10.2f} {total_pnl:<10.2f} {total_winrate/5:<12.1f} {total_trades:<8}")
print("="*80)

# Save to JSON
import json
summary = {
    "test_period": {
        "start_date": "2025-10-20",
        "end_date": "2025-10-24",
        "num_days": 5
    },
    "baseline_metrics": {
        "avg_mrd": total_mrd / 5,
        "avg_sharpe": total_sharpe / 5,
        "total_pnl": total_pnl,
        "avg_winrate": total_winrate / 5,
        "total_trades": total_trades
    },
    "daily_metrics": metrics
}

with open("results/baseline_evaluation/baseline_summary.json", 'w') as f:
    json.dump(summary, f, indent=2)

print("\n✅ Summary saved to: results/baseline_evaluation/baseline_summary.json\n")

EOF

echo "=====================================================================" | tee -a "${LOG_FILE}"
echo "✅ BASELINE EVALUATION COMPLETE" | tee -a "${LOG_FILE}"
echo ""  | tee -a "${LOG_FILE}"
echo "Next steps:" | tee -a "${LOG_FILE}"
echo "1. Review baseline metrics above" | tee -a "${LOG_FILE}"
echo "2. Integrate VWAP Bands detector into SIGOR" | tee -a "${LOG_FILE}"
echo "3. Re-run this test with enhanced SIGOR" | tee -a "${LOG_FILE}"
echo "4. Compare results" | tee -a "${LOG_FILE}"
echo "=====================================================================" | tee -a "${LOG_FILE}"
