#!/bin/bash
# Quick demo of prev-day vs intraday warmup modes

echo "=========================================="
echo "WARMUP MODE DEMONSTRATION"
echo "=========================================="
echo ""
echo "Testing TWO warmup modes on 2025-10-24:"
echo ""

echo "🔵 MODE 1: PREV-DAY WARMUP (60 bars from previous day)"
echo "   - Warmup on bars 332-391 of Oct 23 (last hour)"
echo "   - Trade on ALL bars 1-391 of Oct 24"
echo ""
build/sentio_lite mock --date 10-24 --warmup-bars 60 --no-dashboard 2>&1 | \
    grep -E "Warmup:|Trading:|MRD \(Daily\):|Win Rate:|Total Trades:"

echo ""
echo "=========================================="
echo ""

echo "🟢 MODE 2: INTRADAY WARMUP (60 bars from test day)"
echo "   - Warmup on bars 1-60 of Oct 24 (first hour)"
echo "   - Trade on bars 61-391 of Oct 24"
echo ""
build/sentio_lite mock --date 10-24 --warmup-bars 60 --intraday-warmup --no-dashboard 2>&1 | \
    grep -E "Warmup:|Trading:|MRD \(Daily\):|Win Rate:|Total Trades:"

echo ""
echo "=========================================="
echo ""
echo "KEY DIFFERENCES:"
echo "  - Prev-day: Mature indicators from prior day, trades from bar 1"
echo "  - Intraday: Fresh indicators from test day, trades from bar 61"
echo ""
