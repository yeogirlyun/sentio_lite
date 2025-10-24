#!/usr/bin/env python3
"""
Fetch Today's Historical Bars for SIGOR Warmup (Polygon API)

SIGOR is rule-based (no learning needed) but requires lookback bars
to calculate indicators (RSI, Bollinger, etc.). This script fetches
today's bars from market open (9:30 ET) up to now.

Uses Polygon API for complete market coverage (ALL US exchanges),
unlike Alpaca IEX which only covers ~2-3% of market volume.

Usage:
    python3 scripts/fetch_today_bars.py [--symbols TQQQ,SQQQ,...]
"""

import os
import sys
import json
from datetime import datetime, timedelta
from zoneinfo import ZoneInfo
import requests
import argparse

# Polygon API
POLYGON_BASE_URL = "https://api.polygon.io"

def fetch_today_bars(symbols, polygon_api_key):
    """
    Fetch 1-minute bars for today (9:30 ET to now) from Polygon

    Returns:
        dict: {symbol: [bars]}
    """
    # Calculate today's date range in ET timezone
    et_tz = ZoneInfo("America/New_York")
    now_et = datetime.now(et_tz)
    today_et = now_et.date()

    # Market opens at 9:30 AM ET, closes at 4:00 PM ET
    start_time = datetime.combine(today_et, datetime.min.time().replace(hour=9, minute=30)).replace(tzinfo=et_tz)
    market_close = datetime.combine(today_et, datetime.min.time().replace(hour=16, minute=0)).replace(tzinfo=et_tz)

    # End time: current time or market close, whichever is earlier
    end_time = min(now_et, market_close)

    print(f"Current ET time: {now_et.strftime('%Y-%m-%d %H:%M:%S %Z')}")
    print(f"Fetching bars from {start_time.strftime('%H:%M')} to {end_time.strftime('%H:%M')} ET")
    print(f"Data Source: Polygon (ALL US exchanges, comprehensive coverage)")
    print()

    all_bars = {}

    # Convert to milliseconds for Polygon API
    start_ms = int(start_time.timestamp() * 1000)
    end_ms = int(end_time.timestamp() * 1000)

    for symbol in symbols:
        try:
            # Polygon aggregates (bars) API v2
            url = f"{POLYGON_BASE_URL}/v2/aggs/ticker/{symbol}/range/1/minute/{start_ms}/{end_ms}"
            params = {
                "apiKey": polygon_api_key,
                "adjusted": "false",  # Raw prices (no corporate action adjustments)
                "sort": "asc",        # Ascending time order
                "limit": 50000        # Max limit
            }

            response = requests.get(url, headers={}, params=params, timeout=10)

            if response.status_code == 200:
                data = response.json()

                if data.get("status") == "OK" and data.get("resultsCount", 0) > 0:
                    raw_bars = data.get("results", [])
                    bars = []

                    # Convert Polygon format to C++ compatible format
                    for raw_bar in raw_bars:
                        # Polygon bar fields:
                        # v: volume, vw: volume weighted average, o: open, c: close, h: high, l: low, t: timestamp (ms)

                        # Convert timestamp to datetime for bar_id calculation
                        ts_ms = raw_bar['t']
                        dt = datetime.fromtimestamp(ts_ms / 1000.0, tz=et_tz)

                        # Calculate bar_id (minutes since 9:30 AM ET)
                        minutes_since_midnight = dt.hour * 60 + dt.minute
                        bar_id = minutes_since_midnight - 570  # 570 = 9:30 AM start

                        # Create bar in format expected by C++
                        bar = {
                            't': dt.isoformat(),           # ISO timestamp string
                            't_ms': ts_ms,                 # Milliseconds since epoch
                            'o': raw_bar['o'],             # Open
                            'h': raw_bar['h'],             # High
                            'l': raw_bar['l'],             # Low
                            'c': raw_bar['c'],             # Close
                            'v': raw_bar['v'],             # Volume
                            'vw': raw_bar.get('vw', raw_bar['c']),  # VWAP (use close if not available)
                            'bar_id': bar_id               # Bar index for C++
                        }
                        bars.append(bar)

                    all_bars[symbol] = bars
                    print(f"  ✓ {symbol:5s}: {len(bars):3d} bars")
                else:
                    print(f"  ✗ {symbol:5s}: No data available (status: {data.get('status', 'unknown')})")
                    all_bars[symbol] = []

            else:
                print(f"  ✗ {symbol:5s}: Error {response.status_code}")
                if response.status_code == 403:
                    print(f"      API key may be invalid or rate limited")
                all_bars[symbol] = []

        except Exception as e:
            print(f"  ✗ {symbol:5s}: {e}")
            all_bars[symbol] = []

    return all_bars


def save_warmup_bars(bars, output_file):
    """Save bars to JSON file"""
    with open(output_file, 'w') as f:
        json.dump(bars, f, indent=2)
    print()
    print(f"✓ Saved to: {output_file}")


def main():
    parser = argparse.ArgumentParser(description="Fetch today's bars for SIGOR warmup (Polygon)")
    parser.add_argument("--symbols", type=str,
                       help="Comma-separated symbols (default: from config/symbols.conf)")
    parser.add_argument("--output", type=str, default="warmup_bars.json",
                       help="Output JSON file (default: warmup_bars.json)")
    args = parser.parse_args()

    print("═" * 70)
    print("Fetch Today's Historical Bars - SIGOR Warmup (Polygon)")
    print("═" * 70)
    print()

    # Get Polygon API key
    polygon_api_key = os.getenv('POLYGON_API_KEY')

    if not polygon_api_key:
        print("❌ Error: POLYGON_API_KEY must be set")
        print()
        print("Run: source config.env")
        sys.exit(1)

    print(f"Polygon API Key: {polygon_api_key[:8]}...")
    print()

    # Load symbols
    if args.symbols:
        symbols = [s.strip() for s in args.symbols.split(',')]
    else:
        # Read from config/symbols.conf
        try:
            with open('config/symbols.conf', 'r') as f:
                symbols = [line.strip() for line in f if line.strip() and not line.startswith('#')]
        except FileNotFoundError:
            print("❌ Error: config/symbols.conf not found and --symbols not provided")
            sys.exit(1)

    print(f"Symbols ({len(symbols)}): {', '.join(symbols)}")
    print()

    # Fetch bars from Polygon
    bars = fetch_today_bars(symbols, polygon_api_key)

    # Save to file
    save_warmup_bars(bars, args.output)

    # Summary
    total_bars = sum(len(b) for b in bars.values())
    print()
    print(f"Total bars fetched: {total_bars}")
    print()
    print("✓ Ready for live trading!")
    print("  Start: ./scripts/launch_sigor_live.sh")


if __name__ == "__main__":
    main()
