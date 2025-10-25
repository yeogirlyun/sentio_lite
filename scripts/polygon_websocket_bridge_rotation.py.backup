#!/usr/bin/env python3 -u
"""
Polygon WebSocket Bridge for C++ Rotation Trading (12 Symbols)

Connects to Polygon WebSocket (comprehensive market data from ALL exchanges)
and writes minute aggregate bars for all 12 symbols to a named pipe (FIFO)
for consumption by the C++ rotation trading system.

Why Polygon instead of Alpaca IEX:
- Polygon aggregates data from ALL US exchanges (~100% coverage)
- Alpaca IEX only covers IEX exchange (~2-3% of volume)
- Critical for low-volume symbols like ERX, FAS, FAZ, ERY
"""

import os
import sys
import json
import time
import signal
import websocket
from datetime import datetime
import threading

# FIFO pipe path for C++ communication
FIFO_PATH = "/tmp/alpaca_bars.fifo"  # Keep same path for compatibility

# Rotation trading symbols (12 instruments)
SYMBOLS = [
    'ERX', 'ERY', 'FAS', 'FAZ',
    'SDS', 'SSO', 'SQQQ', 'SVXY',
    'TNA', 'TQQQ', 'TZA', 'UVXY'
]

# Track connection health
last_bar_time = None
running = True
bar_counts = {sym: 0 for sym in SYMBOLS}
ws = None


def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global running, ws
    print("\n[BRIDGE] Shutdown signal received - closing connection...")
    print(f"[BRIDGE] Bars received: {bar_counts}")
    running = False
    if ws:
        ws.close()
    sys.exit(0)


def create_fifo():
    """Create named pipe (FIFO) if it doesn't exist"""
    if os.path.exists(FIFO_PATH):
        os.remove(FIFO_PATH)

    os.mkfifo(FIFO_PATH)
    print(f"[BRIDGE] Created FIFO pipe: {FIFO_PATH}")


def on_message(ws, message):
    """
    Handle incoming message from Polygon WebSocket
    Process minute aggregate bars (AM messages)
    """
    global last_bar_time

    try:
        data = json.loads(message)

        # Handle connection status messages
        if isinstance(data, list):
            for item in data:
                if item.get('ev') == 'status':
                    status = item.get('status')
                    msg = item.get('message', '')
                    if status == 'connected':
                        print(f"[BRIDGE] ✓ {msg}")
                    elif status == 'auth_success':
                        print(f"[BRIDGE] ✓ {msg}")
                    elif status == 'success':
                        print(f"[BRIDGE] ✓ Subscription successful")
                    else:
                        print(f"[BRIDGE] Status: {status} - {msg}")

                # Process aggregate minute bars (AM)
                elif item.get('ev') == 'AM':
                    symbol = item.get('sym')

                    if symbol not in SYMBOLS:
                        continue

                    # Polygon AM (Aggregate Minute) format:
                    # sym: symbol
                    # v: volume
                    # av: accumulated volume (day)
                    # op: open
                    # vw: VWAP
                    # o: open (same as op)
                    # c: close
                    # h: high
                    # l: low
                    # a: accumulated volume
                    # s: start timestamp (ms)
                    # e: end timestamp (ms)

                    # Convert to our format
                    bar_data = {
                        "symbol": symbol,
                        "timestamp_ms": item.get('e'),  # Use end timestamp
                        "open": float(item.get('o', 0)),
                        "high": float(item.get('h', 0)),
                        "low": float(item.get('l', 0)),
                        "close": float(item.get('c', 0)),
                        "volume": int(item.get('v', 0)),
                        "vwap": float(item.get('vw', 0)),
                        "trade_count": int(item.get('n', 0)) if 'n' in item else 0
                    }

                    # Log received bar
                    timestamp = datetime.fromtimestamp(bar_data['timestamp_ms'] / 1000)
                    timestamp_str = timestamp.strftime('%H:%M:%S')
                    bar_counts[symbol] += 1

                    print(f"[BRIDGE] ✓ {symbol:5s} @ {timestamp_str} | "
                          f"C:{bar_data['close']:7.2f} V:{bar_data['volume']:8d} "
                          f"(#{bar_counts[symbol]:3d})", flush=True)

                    # Send bar to FIFO
                    try:
                        with open(FIFO_PATH, 'w') as fifo:
                            json.dump(bar_data, fifo)
                            fifo.write('\n')
                            fifo.flush()
                    except Exception as e:
                        # If C++ not reading, skip (don't block)
                        pass

                    last_bar_time = time.time()

    except json.JSONDecodeError as e:
        print(f"[BRIDGE] ⚠️  JSON decode error: {e}", file=sys.stderr)
    except Exception as e:
        print(f"[BRIDGE] ❌ Error processing message: {e}", file=sys.stderr)


def on_error(ws, error):
    """Handle WebSocket errors"""
    print(f"[BRIDGE] ❌ WebSocket error: {error}", file=sys.stderr)


def on_close(ws, close_status_code, close_msg):
    """Handle WebSocket close"""
    print(f"[BRIDGE] ⚠️  WebSocket closed: {close_status_code} - {close_msg}")
    if running:
        print("[BRIDGE] Attempting to reconnect in 5 seconds...")
        time.sleep(5)


def on_open(ws):
    """Handle WebSocket open - authenticate and subscribe"""
    api_key = os.getenv('POLYGON_API_KEY')

    print("[BRIDGE] ✓ WebSocket connection opened")
    print("[BRIDGE] Authenticating with Polygon...")

    # Authenticate
    auth_msg = {
        "action": "auth",
        "params": api_key
    }
    ws.send(json.dumps(auth_msg))

    # Wait for auth confirmation
    time.sleep(1)

    # Subscribe to minute aggregates for all symbols
    print(f"[BRIDGE] Subscribing to {len(SYMBOLS)} symbols...")
    subscribe_msg = {
        "action": "subscribe",
        "params": ','.join([f"AM.{sym}" for sym in SYMBOLS])
    }
    ws.send(json.dumps(subscribe_msg))

    print("[BRIDGE] ✓ Subscription request sent")
    print("[BRIDGE] Waiting for minute bars...")


def main():
    """Main bridge loop"""
    global running, ws

    # Set up signal handler
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    print("=" * 70)
    print("Polygon WebSocket Bridge for C++ Rotation Trading")
    print("=" * 70)

    # Get Polygon API key from environment
    api_key = os.getenv('POLYGON_API_KEY')

    if not api_key:
        print("[BRIDGE] ❌ ERROR: POLYGON_API_KEY must be set")
        sys.exit(1)

    print(f"[BRIDGE] API Key: {api_key[:10]}...")
    print(f"[BRIDGE] Using Polygon (ALL exchanges, comprehensive coverage)")
    print()

    # Create FIFO pipe
    create_fifo()
    print()

    # Polygon WebSocket URL
    ws_url = "wss://socket.polygon.io/stocks"

    print(f"[BRIDGE] Connecting to Polygon WebSocket...")
    print(f"[BRIDGE] Symbols: {', '.join(SYMBOLS)}")
    print()

    # Create WebSocket connection with auto-reconnect
    while running:
        try:
            ws = websocket.WebSocketApp(
                ws_url,
                on_open=on_open,
                on_message=on_message,
                on_error=on_error,
                on_close=on_close
            )

            print("[BRIDGE] ✓ Bridge active - forwarding bars to C++ via FIFO")
            print(f"[BRIDGE] FIFO path: {FIFO_PATH}")
            print("[BRIDGE] Press Ctrl+C to stop")
            print("=" * 70)
            print()

            # Run WebSocket (blocks until closed)
            ws.run_forever()

            if not running:
                break

            print("[BRIDGE] Reconnecting in 5 seconds...")
            time.sleep(5)

        except KeyboardInterrupt:
            print("\n[BRIDGE] Stopped by user")
            print(f"[BRIDGE] Final bar counts: {bar_counts}")
            break
        except Exception as e:
            print(f"\n[BRIDGE] ❌ Fatal error: {e}", file=sys.stderr)
            if not running:
                break
            print("[BRIDGE] Retrying in 10 seconds...")
            time.sleep(10)

    # Cleanup
    if os.path.exists(FIFO_PATH):
        os.remove(FIFO_PATH)
        print(f"[BRIDGE] Removed FIFO: {FIFO_PATH}")


if __name__ == "__main__":
    main()
