#!/usr/bin/env python3 -u
"""
Alpaca WebSocket Bridge for C++ Rotation Trading (12 Symbols)

Connects to Alpaca IEX WebSocket and writes bars for ALL 12 symbols
to a named pipe (FIFO) for consumption by the C++ rotation trading system.

Uses official alpaca-py SDK with built-in reconnection.
"""

import os
import sys
import json
import time
import signal
import ssl
from datetime import datetime
from alpaca.data.live import StockDataStream
from alpaca.data.models import Bar
from alpaca.data.enums import DataFeed

# Fix SSL certificate verification
# Use system CA certificates or disable verification as fallback
import certifi
os.environ['SSL_CERT_FILE'] = certifi.where()
os.environ['REQUESTS_CA_BUNDLE'] = certifi.where()

# FIFO pipe path for C++ communication
FIFO_PATH = "/tmp/alpaca_bars.fifo"
fifo_file = None  # Keep FIFO open to avoid EOF on reader

# SIGOR Standard 12-Symbol Universe
SYMBOLS = [
    'TQQQ', 'SQQQ',  # Nasdaq 3x long/short
    'TNA', 'TZA',    # Russell 2000 3x long/short
    'UVXY', 'SVXY',  # VIX 1.5x long / 0.5x short
    'FAS', 'FAZ',    # Financials 3x long/short
    'SSO', 'SDS',    # S&P 500 2x long/short
    'SOXL', 'SOXS'   # Semiconductors 3x long/short
]

# Track connection health
last_bar_time = None
running = True
bar_counts = {sym: 0 for sym in SYMBOLS}


def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global running
    print("\n[BRIDGE] Shutdown signal received - closing connection...")
    print(f"[BRIDGE] Bars received: {bar_counts}")
    running = False
    sys.exit(0)


def create_fifo():
    """Create named pipe (FIFO) if it doesn't exist"""
    # Create if missing; don't remove an existing FIFO that's possibly in use
    if not os.path.exists(FIFO_PATH):
        os.mkfifo(FIFO_PATH)
        print(f"[BRIDGE] Created FIFO pipe: {FIFO_PATH}")


async def bar_handler(bar: Bar):
    """
    Handle incoming bar from Alpaca WebSocket
    Forward ALL symbols to C++ rotation trader
    """
    global last_bar_time

    try:
        # Only process subscribed symbols
        if bar.symbol not in SYMBOLS:
            return

        # Convert Alpaca Bar to our JSON format
        bar_data = {
            "symbol": bar.symbol,
            "timestamp_ms": int(bar.timestamp.timestamp() * 1000),
            "open": float(bar.open),
            "high": float(bar.high),
            "low": float(bar.low),
            "close": float(bar.close),
            "volume": int(bar.volume),
            "vwap": float(bar.vwap) if bar.vwap else 0.0,
            "trade_count": int(bar.trade_count) if bar.trade_count else 0
        }

        # Log received bar
        timestamp_str = bar.timestamp.strftime('%H:%M:%S')
        bar_counts[bar.symbol] += 1
        print(f"[BRIDGE] ✓ {bar.symbol:5s} @ {timestamp_str} | "
              f"C:{bar.close:7.2f} V:{int(bar.volume):8d} (#{bar_counts[bar.symbol]:3d})", flush=True)

        # Send bar to FIFO (keep FD open to prevent EOF on the reader)
        global fifo_file
        try:
            if fifo_file is None or fifo_file.closed:
                # Open for write; this will block until a reader opens
                fifo_file = open(FIFO_PATH, 'w')
            json.dump(bar_data, fifo_file)
            fifo_file.write('\n')
            fifo_file.flush()
        except BrokenPipeError:
            # Reader disappeared; close and retry next bar
            try:
                fifo_file.close()
            except Exception:
                pass
            fifo_file = None
        except Exception as e:
            # Log but continue
            print(f"[BRIDGE] ❌ FIFO write error: {e}", file=sys.stderr, flush=True)

        last_bar_time = time.time()

    except Exception as e:
        print(f"[BRIDGE] ❌ Error processing bar: {e}", file=sys.stderr, flush=True)


async def connection_handler(conn_status):
    """Handle WebSocket connection status changes"""
    if conn_status == "connected":
        print("[BRIDGE] ✓ WebSocket connected to Alpaca IEX")
    elif conn_status == "disconnected":
        print("[BRIDGE] ⚠️  WebSocket disconnected - auto-reconnecting...")
    elif conn_status == "auth_success":
        print("[BRIDGE] ✓ Authentication successful")
    elif conn_status == "auth_failed":
        print("[BRIDGE] ❌ Authentication failed - check credentials", file=sys.stderr)
    else:
        print(f"[BRIDGE] Connection status: {conn_status}")


def main():
    """Main bridge loop"""
    global running

    # Set up signal handler
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    print("=" * 70)
    print("Alpaca WebSocket Bridge for C++ Rotation Trading")
    print("=" * 70)

    # Get credentials from environment
    api_key = os.getenv('ALPACA_PAPER_API_KEY')
    api_secret = os.getenv('ALPACA_PAPER_SECRET_KEY')

    if not api_key or not api_secret:
        print("[BRIDGE] ❌ ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set")
        sys.exit(1)

    print(f"[BRIDGE] API Key: {api_key[:8]}...")
    print(f"[BRIDGE] Using Alpaca Paper Trading (IEX data)")
    print()

    # Create FIFO pipe
    create_fifo()
    print()

    # Create WebSocket client
    print("[BRIDGE] Initializing Alpaca WebSocket client...")
    wss_client = StockDataStream(api_key, api_secret, feed=DataFeed.IEX)  # IEX = free tier

    # Subscribe to ALL 12 rotation symbols
    print(f"[BRIDGE] Subscribing to {len(SYMBOLS)} symbols:")
    for i in range(0, len(SYMBOLS), 6):
        print(f"[BRIDGE]   {' '.join(SYMBOLS[i:i+6])}")

    # Subscribe to bars for all symbols
    for symbol in SYMBOLS:
        wss_client.subscribe_bars(bar_handler, symbol)

    print()
    print("[BRIDGE] ✓ Bridge active - forwarding bars to C++ via FIFO")
    print(f"[BRIDGE] FIFO path: {FIFO_PATH}")
    print("[BRIDGE] Press Ctrl+C to stop")
    print("=" * 70)
    print()

    try:
        # Run WebSocket client (blocks until stopped)
        # Built-in reconnection handled by SDK
        wss_client.run()

    except KeyboardInterrupt:
        print("\n[BRIDGE] Stopped by user")
        print(f"[BRIDGE] Final bar counts: {bar_counts}")
    except Exception as e:
        print(f"\n[BRIDGE] ❌ Fatal error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        # Cleanup
        if os.path.exists(FIFO_PATH):
            os.remove(FIFO_PATH)
            print(f"[BRIDGE] Removed FIFO: {FIFO_PATH}")


if __name__ == "__main__":
    main()
