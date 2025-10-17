#!/bin/bash

# Test different initial_variance values to restore UVXY confidence
# Target: UVXY confidence should be 0.025-0.089 (2.5-8.9%)
# Current: UVXY confidence is 0.003-0.009 (0.3-0.9%)

set -e

# Test parameters
TEST_DATE="2025-10-07"
CONFIG_FILE="config/rotation_strategy.json"
BACKUP_CONFIG="config/rotation_strategy.json.backup"

# Variance values to test (progressively lower to increase confidence)
VARIANCE_VALUES=(0.01 0.1 0.5 1.0 10.0)

echo "========================================="
echo "INITIAL_VARIANCE SWEEP TEST"
echo "========================================="
echo "Test Date: $TEST_DATE"
echo "Target UVXY Confidence: 0.025-0.089 (baseline)"
echo "Current UVXY Confidence: 0.003-0.009 (broken)"
echo ""

# Backup original config
cp $CONFIG_FILE $BACKUP_CONFIG
echo "✓ Backed up config to $BACKUP_CONFIG"
echo ""

# Results summary
declare -a RESULTS

for variance in "${VARIANCE_VALUES[@]}"; do
    echo "========================================="
    echo "Testing initial_variance = $variance"
    echo "========================================="

    # Update config using Python
    python3 << EOF
import json

with open('$CONFIG_FILE', 'r') as f:
    config = json.load(f)

config['oes_config']['initial_variance'] = $variance
config['oes_config']['regularization'] = 0.01
config['oes_config']['ewrls_lambda'] = 0.995

with open('$CONFIG_FILE', 'w') as f:
    json.dump(config, f, indent=2)

print(f"✓ Updated config: initial_variance = $variance")
EOF

    # Run single-day test
    LOG_DIR="logs/variance_test_${variance}"
    mkdir -p "$LOG_DIR"

    echo "Running test..."
    ./build/sentio_cli mock \
        --mode mock \
        --start-date $TEST_DATE \
        --end-date $TEST_DATE \
        --warmup-days 4 \
        --data-dir data/equities \
        --log-dir "$LOG_DIR" \
        2>&1 | tee "${LOG_DIR}/test.log" | grep -E "Final Equity|Total P&L"

    # Extract UVXY confidence values
    if [ -f "${LOG_DIR}/${TEST_DATE}/signals.jsonl" ]; then
        echo ""
        echo "UVXY Signal Confidence Analysis:"
        echo "--------------------------------"

        # Get first few non-neutral UVXY signals
        UVXY_SIGNALS=$(grep '"symbol":"UVXY"' "${LOG_DIR}/${TEST_DATE}/signals.jsonl" | \
                       grep -v '"signal":0' | \
                       head -10 | \
                       python3 -c "
import sys
import json
conf_values = []
for line in sys.stdin:
    data = json.loads(line)
    conf_values.append(data.get('confidence', 0.0))
if conf_values:
    print(f'Min: {min(conf_values):.6f}, Max: {max(conf_values):.6f}, Avg: {sum(conf_values)/len(conf_values):.6f}')
    print(f'Count: {len(conf_values)} non-neutral signals')
else:
    print('No non-neutral UVXY signals found')
")

        echo "$UVXY_SIGNALS"

        # Get UVXY decision strength
        if [ -f "${LOG_DIR}/${TEST_DATE}/decisions.jsonl" ]; then
            UVXY_STRENGTH=$(grep '"symbol":"UVXY"' "${LOG_DIR}/${TEST_DATE}/decisions.jsonl" | \
                           head -1 | \
                           python3 -c "
import sys
import json
try:
    line = sys.stdin.readline()
    data = json.loads(line)
    strength = data.get('strength', 0.0)
    rank = data.get('rank', 99)
    print(f'First decision: strength={strength:.6f}, rank={rank}')
except:
    print('No UVXY decisions found')
")
            echo "$UVXY_STRENGTH"
        fi

        # Store result
        RESULTS+=("variance=$variance | $UVXY_SIGNALS | $UVXY_STRENGTH")
    else
        echo "⚠️  No signal file generated"
        RESULTS+=("variance=$variance | ERROR: No signals generated")
    fi

    echo ""
done

# Restore original config
cp $BACKUP_CONFIG $CONFIG_FILE
rm $BACKUP_CONFIG
echo "✓ Restored original config"

# Print summary
echo ""
echo "========================================="
echo "TEST RESULTS SUMMARY"
echo "========================================="
echo ""
echo "Baseline (october_12symbols):"
echo "  UVXY Confidence: 0.025699 → 0.089534 (2.57% → 8.95%)"
echo "  UVXY Strength: 0.023511 → 0.077057"
echo "  UVXY Rank: #1 (top performer)"
echo ""
echo "Current Tests:"
for result in "${RESULTS[@]}"; do
    echo "  $result"
done
echo ""
echo "========================================="
echo "RECOMMENDATION"
echo "========================================="
echo ""
echo "Look for the initial_variance value that produces:"
echo "  ✓ UVXY confidence: 0.025-0.089 range"
echo "  ✓ UVXY strength: >0.020"
echo "  ✓ UVXY rank: 1 or 2"
echo ""
echo "Test logs saved in: logs/variance_test_*/"
echo "========================================="
