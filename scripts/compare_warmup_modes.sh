#!/bin/bash
# Compare prev-day warmup vs intraday warmup effectiveness
# Both use 60 bars warmup (1 hour) to ensure fair comparison

set -e

END_DATE="10-24"
TRIALS=200
WARMUP_BARS=60

echo "=========================================="
echo "WARMUP MODE COMPARISON EXPERIMENT"
echo "=========================================="
echo "  End Date: 2025-${END_DATE}"
echo "  Trials: ${TRIALS}"
echo "  Warmup Bars: ${WARMUP_BARS}"
echo ""
echo "Testing TWO warmup strategies:"
echo "  1. PREV-DAY: Warmup on last ${WARMUP_BARS} bars of previous day (mature indicators)"
echo "  2. INTRADAY: Warmup on first ${WARMUP_BARS} bars of test day (fresh indicators)"
echo ""
echo "=========================================="
echo ""

# Ensure results directory exists
mkdir -p results/warmup_comparison

# Run PREV-DAY warmup optimization
echo "🔵 RUNNING PREV-DAY WARMUP OPTIMIZATION (${TRIALS} trials)..."
echo "   Warmup source: Last ${WARMUP_BARS} bars of previous day"
echo ""

python3 scripts/optimize_sigor_combined.py \
    --end-date "${END_DATE}" \
    --trials ${TRIALS} \
    --warmup-mode prevday \
    --warmup-bars ${WARMUP_BARS} \
    2>&1 | tee results/warmup_comparison/prevday_warmup_${TRIALS}trials.log

# Save the results
cp results/combined_optimization/optuna_results.json \
   results/warmup_comparison/prevday_warmup_results.json

echo ""
echo "✅ Prev-day warmup optimization complete!"
echo ""
echo "=========================================="
echo ""

# Run INTRADAY warmup optimization
echo "🟢 RUNNING INTRADAY WARMUP OPTIMIZATION (${TRIALS} trials)..."
echo "   Warmup source: First ${WARMUP_BARS} bars of test day"
echo ""

python3 scripts/optimize_sigor_combined.py \
    --end-date "${END_DATE}" \
    --trials ${TRIALS} \
    --warmup-mode intraday \
    --warmup-bars ${WARMUP_BARS} \
    2>&1 | tee results/warmup_comparison/intraday_warmup_${TRIALS}trials.log

# Save the results
cp results/combined_optimization/optuna_results.json \
   results/warmup_comparison/intraday_warmup_results.json

echo ""
echo "✅ Intraday warmup optimization complete!"
echo ""
echo "=========================================="
echo ""

# Compare results
echo "📊 WARMUP MODE COMPARISON RESULTS"
echo "=========================================="
echo ""

PREVDAY_BEST=$(jq -r '.best_eval_mrd' results/warmup_comparison/prevday_warmup_results.json)
PREVDAY_VAL=$(jq -r '.best_val_mrd' results/warmup_comparison/prevday_warmup_results.json)
INTRADAY_BEST=$(jq -r '.best_eval_mrd' results/warmup_comparison/intraday_warmup_results.json)
INTRADAY_VAL=$(jq -r '.best_val_mrd' results/warmup_comparison/intraday_warmup_results.json)

echo "PREV-DAY WARMUP (last ${WARMUP_BARS} bars of previous day):"
echo "  Best Eval MRD:  ${PREVDAY_BEST}%"
echo "  Best Val MRD:   ${PREVDAY_VAL}%"
echo ""
echo "INTRADAY WARMUP (first ${WARMUP_BARS} bars of test day):"
echo "  Best Eval MRD:  ${INTRADAY_BEST}%"
echo "  Best Val MRD:   ${INTRADAY_VAL}%"
echo ""

# Calculate difference using bc
EVAL_DIFF=$(echo "${INTRADAY_BEST} - ${PREVDAY_BEST}" | bc)
VAL_DIFF=$(echo "${INTRADAY_VAL} - ${PREVDAY_VAL}" | bc)

echo "DIFFERENCE (Intraday - Prevday):"
echo "  Eval MRD: ${EVAL_DIFF}%"
echo "  Val MRD:  ${VAL_DIFF}%"
echo ""

# Determine winner
if (( $(echo "${INTRADAY_BEST} > ${PREVDAY_BEST}" | bc -l) )); then
    echo "🏆 WINNER: INTRADAY WARMUP"
    echo "   Intraday warmup achieved ${EVAL_DIFF}% better evaluation MRD"
elif (( $(echo "${PREVDAY_BEST} > ${INTRADAY_BEST}" | bc -l) )); then
    echo "🏆 WINNER: PREV-DAY WARMUP"
    EVAL_DIFF_ABS=$(echo "${EVAL_DIFF} * -1" | bc)
    echo "   Prev-day warmup achieved ${EVAL_DIFF_ABS}% better evaluation MRD"
else
    echo "🤝 TIE: Both modes achieved identical evaluation MRD"
fi

echo ""
echo "=========================================="
echo ""
echo "Full results saved to:"
echo "  - results/warmup_comparison/prevday_warmup_results.json"
echo "  - results/warmup_comparison/intraday_warmup_results.json"
echo "  - results/warmup_comparison/prevday_warmup_${TRIALS}trials.log"
echo "  - results/warmup_comparison/intraday_warmup_${TRIALS}trials.log"
echo ""
