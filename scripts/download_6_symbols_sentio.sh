#!/bin/bash
#
# Download 6 Core Symbols for Sentio Lite
#
# Symbols: TQQQ, SQQQ, UPRO, SDS, UVXY, SVXY
# Usage: ./scripts/download_6_symbols_sentio.sh [start_date] [end_date]
#

set -e

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."
cd "$PROJECT_ROOT"

# Load API keys
if [ -f config.env ]; then
    source config.env
elif [ -f ../online_trader/config.env ]; then
    echo "Loading API keys from online_trader/config.env"
    source ../online_trader/config.env
fi

# Parameters
START_DATE=${1:-2024-09-15}
END_DATE=${2:-2024-10-31}
SYMBOLS="TQQQ SQQQ UPRO SDS UVXY SVXY"
OUTPUT_DIR="data"

echo "========================================================================"
echo "Sentio Lite - 6 Core Symbol Data Download"
echo "========================================================================"
echo "Symbols (6): $SYMBOLS"
echo "Date Range: $START_DATE to $END_DATE"
echo "Output: $OUTPUT_DIR"
echo ""

# Check for API key
if [ -z "$POLYGON_API_KEY" ]; then
    echo "❌ ERROR: POLYGON_API_KEY not set"
    echo ""
    echo "Please set your Polygon API key in config.env or environment"
    echo "See download_12_symbols.sh for details"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Download data
echo "Downloading data..."
echo ""

SUCCESS_COUNT=0
FAIL_COUNT=0

for SYMBOL in $SYMBOLS; do
    echo "[$((SUCCESS_COUNT + FAIL_COUNT + 1))/6] Downloading $SYMBOL..."

    if python3 tools/data_downloader.py \
        --start "$START_DATE" \
        --end "$END_DATE" \
        --outdir "$OUTPUT_DIR" \
        --timespan minute \
        --multiplier 1 \
        $SYMBOL 2>&1 | grep -q "SUCCESS\|Saved\|Downloaded"; then

        echo "  ✅ $SYMBOL downloaded"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        echo "  ❌ $SYMBOL failed"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
    echo ""
done

echo "========================================================================"
echo "Download Summary: $SUCCESS_COUNT/6 successful"
echo "========================================================================"

if [ $SUCCESS_COUNT -eq 6 ]; then
    echo "✅ All 6 symbols downloaded!"
    echo ""
    echo "To test:"
    echo "  cd build && ./sentio_lite --symbols 6"
fi

echo ""
exit $FAIL_COUNT
