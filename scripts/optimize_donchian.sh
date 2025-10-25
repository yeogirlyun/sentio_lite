#!/bin/bash
#
# Donchian Detector Parameter Optimization
# Tests different weight and parameter combinations to find optimal settings
#

set -e

DATES=("2025-10-20" "2025-10-21" "2025-10-22" "2025-10-23" "2025-10-24")
OUTPUT_DIR="results/donchian_optimization"
LOG_FILE="${OUTPUT_DIR}/optimization.log"

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Clear previous log
> "${LOG_FILE}"

echo "=================================================================" | tee -a "${LOG_FILE}"
echo "  Donchian Detector Parameter Optimization" | tee -a "${LOG_FILE}"
echo "  Test Period: Oct 20-24, 2025 (5 days)" | tee -a "${LOG_FILE}"
echo "=================================================================" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

# Parameter grid
WEIGHTS=(0.0 0.2 0.5 0.8 1.0 1.5)
ATR_MULTS=(0.3 0.5 0.7 1.0)
CONFIRM_BARS=(2 3 5)

BEST_MRD=-999999
BEST_CONFIG=""

CONFIG_NUM=0
TOTAL_CONFIGS=$((${#WEIGHTS[@]} * ${#ATR_MULTS[@]} * ${#CONFIRM_BARS[@]}))

echo "Testing ${TOTAL_CONFIGS} configurations..." | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

for WEIGHT in "${WEIGHTS[@]}"; do
    for ATR_MULT in "${ATR_MULTS[@]}"; do
        for CONFIRM in "${CONFIRM_BARS[@]}"; do
            CONFIG_NUM=$((CONFIG_NUM + 1))

            echo "=================================================================" | tee -a "${LOG_FILE}"
            echo "Config ${CONFIG_NUM}/${TOTAL_CONFIGS}: w_donchian=${WEIGHT}, atr_mult=${ATR_MULT}, confirm_bars=${CONFIRM}" | tee -a "${LOG_FILE}"
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
                RESULT=$(./build/sentio_lite mock --date "${DATE}" 2>&1)

                MRD=$(echo "$RESULT" | grep "MRD (Daily):" | head -1 | awk '{print $3}' | tr -d '%')
                WINRATE=$(echo "$RESULT" | grep "Win Rate:" | head -1 | awk '{print $3}' | tr -d '%')
                TRADES=$(echo "$RESULT" | grep "Total Trades:" | head -1 | awk '{print $3}')

                TOTAL_MRD=$(python3 -c "print(${TOTAL_MRD} + ${MRD})")
                TOTAL_WINRATE=$(python3 -c "print(${TOTAL_WINRATE} + ${WINRATE})")
                TOTAL_TRADES=$((TOTAL_TRADES + TRADES))

                echo "  ${DATE}: MRD=${MRD}%, WR=${WINRATE}%, Trades=${TRADES}" | tee -a "${LOG_FILE}"
            done

            AVG_MRD=$(python3 -c "print(${TOTAL_MRD} / 5.0)")
            AVG_WINRATE=$(python3 -c "print(${TOTAL_WINRATE} / 5.0)")

            echo "  ─────────────────────────────────────────────────" | tee -a "${LOG_FILE}"
            echo "  AVERAGE: MRD=${AVG_MRD}%, WR=${AVG_WINRATE}%, Total Trades=${TOTAL_TRADES}" | tee -a "${LOG_FILE}"
            echo "" | tee -a "${LOG_FILE}"

            # Check if this is the best config
            IS_BETTER=$(python3 -c "print(1 if ${AVG_MRD} > ${BEST_MRD} else 0)")
            if [ "${IS_BETTER}" -eq 1 ]; then
                BEST_MRD=${AVG_MRD}
                BEST_CONFIG="w_donchian=${WEIGHT}, atr_mult=${ATR_MULT}, confirm_bars=${CONFIRM}"
                echo "  🎯 NEW BEST CONFIG!" | tee -a "${LOG_FILE}"
            fi

            # Restore backup
            mv "${BACKUP_FILE}" "${CONFIG_FILE}"

            echo "" | tee -a "${LOG_FILE}"
        done
    done
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

IMPROVEMENT=$(python3 -c "print(${BEST_MRD} - (-0.072))")
echo "Improvement: ${IMPROVEMENT}%" | tee -a "${LOG_FILE}"
echo "" | tee -a "${LOG_FILE}"

if [ $(python3 -c "print(1 if ${BEST_MRD} > -0.072 else 0)") -eq 1 ]; then
    echo "✅ Donchian detector IMPROVED performance!" | tee -a "${LOG_FILE}"
else
    echo "❌ Donchian detector did NOT improve performance." | tee -a "${LOG_FILE}"
    echo "   Recommend reverting to baseline 7-detector version." | tee -a "${LOG_FILE}"
fi

echo "=================================================================" | tee -a "${LOG_FILE}"
