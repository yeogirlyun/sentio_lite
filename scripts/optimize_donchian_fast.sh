#!/bin/bash
#
# Fast Donchian Detector Parameter Optimization
# Tests key parameter combinations for quick assessment
#

set -e

DATES=("2025-10-20" "2025-10-21" "2025-10-22" "2025-10-23" "2025-10-24")
OUTPUT_DIR="results/donchian_optimization"
LOG_FILE="${OUTPUT_DIR}/fast_optimization.log"

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Clear previous log
> "${LOG_FILE}"

echo "=================================================================" | tee -a "${LOG_FILE}"
echo "  Fast Donchian Detector Parameter Optimization" | tee -a "${LOG_FILE}"
echo "  Test Period: Oct 20-24, 2025 (5 days)" | tee -a "${LOG_FILE}"
echo "=================================================================" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

# Key configurations to test:
# Format: "weight,atr_mult,confirm_bars,description"
CONFIGS=(
    "0.0,0.5,3,Disabled (baseline comparison)"
    "0.2,0.5,3,Very low weight"
    "0.5,0.5,3,Medium weight"
    "1.0,0.5,3,High weight"
    "1.5,0.5,3,Very high weight"
    "0.8,0.3,3,Tight ATR filter"
    "0.8,0.7,3,Loose ATR filter"
    "0.8,1.0,3,Very loose ATR filter"
    "0.8,0.5,2,Quick confirmation"
    "0.8,0.5,5,Slow confirmation"
    "0.2,0.3,2,Conservative setup"
    "1.5,1.0,5,Aggressive setup"
)

BEST_MRD=-999999
BEST_CONFIG=""
CONFIG_NUM=0
TOTAL_CONFIGS=${#CONFIGS[@]}

echo "Testing ${TOTAL_CONFIGS} key configurations..." | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

for CONFIG_LINE in "${CONFIGS[@]}"; do
    CONFIG_NUM=$((CONFIG_NUM + 1))

    IFS=',' read -r WEIGHT ATR_MULT CONFIRM DESC <<< "${CONFIG_LINE}"

    echo "=================================================================" | tee -a "${LOG_FILE}"
    echo "Config ${CONFIG_NUM}/${TOTAL_CONFIGS}: ${DESC}" | tee -a "${LOG_FILE}"
    echo "  w_donchian=${WEIGHT}, atr_mult=${ATR_MULT}, confirm_bars=${CONFIRM}" | tee -a "${LOG_FILE}"
    echo "=================================================================" | tee -a "${LOG_FILE}"

    # Update config file temporarily
    CONFIG_FILE="config/sigor_params.json"
    BACKUP_FILE="config/sigor_params.json.bak"
    cp "${CONFIG_FILE}" "${BACKUP_FILE}"

    # Use Python to update JSON
    python3 << EOF
import json

with open("${CONFIG_FILE}") as f:
    config = json.load(f)

config["parameters"]["w_donchian"] = ${WEIGHT}
config["parameters"]["donchian_atr_mult"] = ${ATR_MULT}
config["parameters"]["donchian_confirm_bars"] = ${CONFIRM}

with open("${CONFIG_FILE}", 'w') as f:
    json.dump(config, f, indent=2)
EOF

    # Rebuild with new config
    cmake --build build > /dev/null 2>&1

    # Test on all dates
    TOTAL_MRD=0
    TOTAL_WINRATE=0
    TOTAL_TRADES=0

    for DATE in "${DATES[@]}"; do
        RESULT=$(./build/sentio_lite mock --date "${DATE}" --no-dashboard 2>&1)

        MRD=$(echo "$RESULT" | grep "MRD (Daily):" | head -1 | awk '{print $3}' | tr -d '%')
        WINRATE=$(echo "$RESULT" | grep "Win Rate:" | head -1 | awk '{print $3}' | tr -d '%')
        TRADES=$(echo "$RESULT" | grep "Total Trades:" | head -1 | awk '{print $3}')

        TOTAL_MRD=$(python3 -c "print(${TOTAL_MRD} + ${MRD})")
        TOTAL_WINRATE=$(python3 -c "print(${TOTAL_WINRATE} + ${WINRATE})")
        TOTAL_TRADES=$((TOTAL_TRADES + TRADES))

        echo "  ${DATE}: MRD=${MRD}%, WR=${WINRATE}%, Trades=${TRADES}" | tee -a "${LOG_FILE}"
    done

    AVG_MRD=$(python3 -c "print(round(${TOTAL_MRD} / 5.0, 3))")
    AVG_WINRATE=$(python3 -c "print(round(${TOTAL_WINRATE} / 5.0, 1))")

    echo "  ─────────────────────────────────────────────────" | tee -a "${LOG_FILE}"
    echo "  AVERAGE: MRD=${AVG_MRD}%, WR=${AVG_WINRATE}%, Total Trades=${TOTAL_TRADES}" | tee -a "${LOG_FILE}"
    echo "" | tee -a "${LOG_FILE}"

    # Check if this is the best config
    IS_BETTER=$(python3 -c "print(1 if ${AVG_MRD} > ${BEST_MRD} else 0)")
    if [ "${IS_BETTER}" -eq 1 ]; then
        BEST_MRD=${AVG_MRD}
        BEST_CONFIG="${DESC} (w=${WEIGHT}, atr=${ATR_MULT}, confirm=${CONFIRM})"
        echo "  🎯 NEW BEST CONFIG!" | tee -a "${LOG_FILE}"
    fi

    # Restore backup
    mv "${BACKUP_FILE}" "${CONFIG_FILE}"

    echo "" | tee -a "${LOG_FILE}"
done

echo "=================================================================" | tee -a "${LOG_FILE}"
echo "  OPTIMIZATION COMPLETE" | tee -a "${LOG_FILE}"
echo "=================================================================" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"
echo "BEST CONFIGURATION:" | tee -a "${LOG_FILE}"
echo "  ${BEST_CONFIG}" | tee -a "${LOG_FILE}"
echo "  Average MRD: ${BEST_MRD}%" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"
echo "Baseline (7-detector) MRD: -0.072%" | tee -a "${LOG_FILE}"

IMPROVEMENT=$(python3 -c "print(round(${BEST_MRD} - (-0.072), 3))")
echo "Improvement vs Baseline: ${IMPROVEMENT}%" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

if [ $(python3 -c "print(1 if ${BEST_MRD} > -0.072 else 0)") -eq 1 ]; then
    echo "✅ Donchian detector IMPROVED performance!" | tee -a "${LOG_FILE}"
    echo "   Recommend keeping this configuration." | tee -a "${LOG_FILE}"
else
    echo "❌ Donchian detector did NOT improve performance." | tee -a "${LOG_FILE}"
    echo "   Recommend reverting to baseline 7-detector version." | tee -a "${LOG_FILE}"
fi

echo "=================================================================" | tee -a "${LOG_FILE}"
