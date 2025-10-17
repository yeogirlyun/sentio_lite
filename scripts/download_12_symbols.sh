#!/bin/bash
#
# Download 12 Leveraged ETFs for Sentio Lite Multi-Symbol Trading
#
# Symbols: TQQQ, SQQQ, SSO, SDS, UPRO, SPXS, TNA, TZA, FAS, FAZ, UVXY, SVXY
# Usage: ./scripts/download_12_symbols.sh [start_date] [end_date]
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
START_DATE=${1:-2024-09-15}  # Default: ~1 month before Oct
END_DATE=${2:-2024-10-31}     # Default: End of October
SYMBOLS="TQQQ SQQQ SSO SDS UPRO SPXS TNA TZA FAS FAZ UVXY SVXY"
OUTPUT_DIR="data"

echo "========================================================================"
echo "Sentio Lite - 12 Symbol Data Download"
echo "========================================================================"
echo "Symbols (12): $SYMBOLS"
echo "Date Range: $START_DATE to $END_DATE"
echo "Output: $OUTPUT_DIR"
echo ""

# Check for API key
if [ -z "$POLYGON_API_KEY" ]; then
    echo "❌ ERROR: POLYGON_API_KEY not set"
    echo ""
    echo "Please set your Polygon API key:"
    echo "  1. Create config.env in project root:"
    echo "     echo 'export POLYGON_API_KEY=\"your_key_here\"' > config.env"
    echo ""
    echo "  2. Or copy from online_trader:"
    echo "     cp ../online_trader/config.env ."
    echo ""
    echo "  3. Or set in environment:"
    echo "     export POLYGON_API_KEY=\"your_key_here\""
    echo ""
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Check if data_downloader.py exists
if [ ! -f "tools/data_downloader.py" ]; then
    echo "❌ ERROR: tools/data_downloader.py not found"
    echo "Please copy from online_trader:"
    echo "  cp ../online_trader/tools/data_downloader.py tools/"
    exit 1
fi

# Download data for each symbol
echo "Downloading data..."
echo ""

SUCCESS_COUNT=0
FAIL_COUNT=0
FAILED_SYMBOLS=""

for SYMBOL in $SYMBOLS; do
    echo "[$((SUCCESS_COUNT + FAIL_COUNT + 1))/12] Downloading $SYMBOL..."

    if python3 tools/data_downloader.py \
        --start "$START_DATE" \
        --end "$END_DATE" \
        --outdir "$OUTPUT_DIR" \
        --timespan minute \
        --multiplier 1 \
        $SYMBOL 2>&1 | grep -q "SUCCESS\|Saved\|Downloaded"; then

        echo "  ✅ $SYMBOL downloaded successfully"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        echo "  ❌ $SYMBOL download failed"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        FAILED_SYMBOLS="$FAILED_SYMBOLS $SYMBOL"
    fi
    echo ""
done

echo "========================================================================"
echo "Download Summary"
echo "========================================================================"
echo "Successful: $SUCCESS_COUNT/12"
echo "Failed: $FAIL_COUNT/12"

if [ $FAIL_COUNT -gt 0 ]; then
    echo "Failed symbols:$FAILED_SYMBOLS"
fi

echo ""
echo "Files created in $OUTPUT_DIR/:"
ls -lh "$OUTPUT_DIR"/*.bin 2>/dev/null | tail -12 || ls -lh "$OUTPUT_DIR"/*.csv 2>/dev/null | tail -12 || echo "  No data files found"

echo ""
if [ $SUCCESS_COUNT -eq 12 ]; then
    echo "✅ All symbols downloaded successfully!"
    echo ""
    echo "To test with sentio_lite:"
    echo "  cd build"
    echo "  ./sentio_lite --symbols 12"
    echo ""
    echo "Or specify date range:"
    echo "  ./sentio_lite --symbols 12 --start-date $START_DATE --end-date $END_DATE"
elif [ $SUCCESS_COUNT -gt 0 ]; then
    echo "⚠️  Partial download complete ($SUCCESS_COUNT/12 symbols)"
    echo ""
    echo "To test with available symbols:"
    echo "  cd build"
    echo "  ./sentio_lite --symbols TQQQ,SQQQ,UPRO  # or whichever downloaded"
else
    echo "❌ Download failed for all symbols"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Check your POLYGON_API_KEY is valid"
    echo "  2. Check your API rate limits"
    echo "  3. Try downloading one symbol manually:"
    echo "     python3 tools/data_downloader.py --start $START_DATE --end $END_DATE TQQQ"
fi

echo ""
exit $FAIL_COUNT
