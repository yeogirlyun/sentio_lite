#!/usr/bin/env python3
"""
Alpaca Order Client for C++ Trading System

Listens on a FIFO pipe for order commands from C++ and submits them to Alpaca.
Provides feedback on order status via a response FIFO.

Order Format (JSON):
{
    "action": "BUY" | "SELL",
    "symbol": "TQQQ",
    "shares": 100,
    "order_id": "unique_id"  // For tracking
}

Response Format (JSON):
{
    "order_id": "unique_id",
    "status": "filled" | "rejected" | "pending",
    "filled_price": 45.23,
    "message": "Order filled successfully"
}
"""

import os
import sys
import json
import signal
import requests
from datetime import datetime

# FIFO pipes for C++ communication
ORDER_FIFO = "/tmp/alpaca_orders.fifo"       # C++ writes orders here
RESPONSE_FIFO = "/tmp/alpaca_responses.fifo"  # We write responses here

# Alpaca Paper Trading API
BASE_URL = "https://paper-api.alpaca.markets"

# Track connection
running = True
orders_processed = 0


def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global running
    print(f"\n[ORDER CLIENT] Shutdown signal received")
    print(f"[ORDER CLIENT] Orders processed: {orders_processed}")
    running = False
    sys.exit(0)


def create_fifos():
    """Create named pipes (FIFOs) if they don't exist"""
    for fifo_path in [ORDER_FIFO, RESPONSE_FIFO]:
        if os.path.exists(fifo_path):
            os.remove(fifo_path)
        os.mkfifo(fifo_path)
        print(f"[ORDER CLIENT] Created FIFO: {fifo_path}")


def submit_order(api_key, secret_key, order_data):
    """
    Submit order to Alpaca and return response

    Args:
        api_key: Alpaca API key
        secret_key: Alpaca secret key
        order_data: Order details from C++

    Returns:
        dict: Response with status, filled_price, etc.
    """
    headers = {
        "APCA-API-KEY-ID": api_key,
        "APCA-API-SECRET-KEY": secret_key,
        "Content-Type": "application/json"
    }

    action = order_data["action"]
    symbol = order_data["symbol"]
    shares = int(order_data["shares"])
    order_id = order_data.get("order_id", "unknown")

    # Prepare Alpaca order
    alpaca_order = {
        "symbol": symbol,
        "qty": shares,
        "side": "buy" if action == "BUY" else "sell",
        "type": "market",
        "time_in_force": "day"
    }

    try:
        # Submit order
        response = requests.post(
            f"{BASE_URL}/v2/orders",
            headers=headers,
            json=alpaca_order,
            timeout=5
        )

        if response.status_code == 200 or response.status_code == 201:
            result = response.json()

            # Wait briefly for fill (market orders usually fill fast)
            import time
            time.sleep(0.5)

            # Check order status
            alpaca_order_id = result["id"]
            status_response = requests.get(
                f"{BASE_URL}/v2/orders/{alpaca_order_id}",
                headers=headers,
                timeout=5
            )

            if status_response.status_code == 200:
                status_data = status_response.json()
                filled_avg_price = float(status_data.get("filled_avg_price", 0))
                order_status = status_data["status"]

                return {
                    "order_id": order_id,
                    "status": "filled" if order_status == "filled" else "pending",
                    "filled_price": filled_avg_price,
                    "alpaca_order_id": alpaca_order_id,
                    "message": f"{action} {shares} {symbol} - {order_status}"
                }
            else:
                return {
                    "order_id": order_id,
                    "status": "pending",
                    "filled_price": 0.0,
                    "alpaca_order_id": alpaca_order_id,
                    "message": f"Order submitted, status check failed"
                }
        else:
            error_msg = response.json().get("message", "Unknown error")
            return {
                "order_id": order_id,
                "status": "rejected",
                "filled_price": 0.0,
                "message": f"Order rejected: {error_msg}"
            }

    except Exception as e:
        return {
            "order_id": order_id,
            "status": "rejected",
            "filled_price": 0.0,
            "message": f"Error: {str(e)}"
        }


def main():
    """Main order processing loop"""
    global running, orders_processed

    # Set up signal handler
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    print("=" * 70)
    print("Alpaca Order Client for C++ Trading System")
    print("=" * 70)

    # Get credentials from environment
    api_key = os.getenv('ALPACA_PAPER_API_KEY')
    api_secret = os.getenv('ALPACA_PAPER_SECRET_KEY')

    if not api_key or not api_secret:
        print("[ORDER CLIENT] ❌ ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set")
        sys.exit(1)

    print(f"[ORDER CLIENT] API Key: {api_key[:8]}...")
    print(f"[ORDER CLIENT] Using Alpaca Paper Trading")
    print()

    # Create FIFO pipes
    create_fifos()
    print()

    print("[ORDER CLIENT] ✓ Ready to process orders from C++")
    print(f"[ORDER CLIENT] Listening on: {ORDER_FIFO}")
    print(f"[ORDER CLIENT] Responding on: {RESPONSE_FIFO}")
    print("[ORDER CLIENT] Press Ctrl+C to stop")
    print("=" * 70)
    print()

    try:
        while running:
            # Open FIFO for reading (blocks until C++ writes)
            with open(ORDER_FIFO, 'r') as order_fifo:
                for line in order_fifo:
                    if not line.strip():
                        continue

                    try:
                        # Parse order from C++
                        order_data = json.loads(line)

                        timestamp = datetime.now().strftime('%H:%M:%S')
                        print(f"[ORDER CLIENT] {timestamp} | Received: {order_data['action']} "
                              f"{order_data['shares']} {order_data['symbol']}")

                        # Submit to Alpaca
                        response = submit_order(api_key, api_secret, order_data)

                        # Log result
                        status_symbol = "✓" if response["status"] == "filled" else "⚠"
                        print(f"[ORDER CLIENT] {timestamp} | {status_symbol} {response['message']}")

                        if response["status"] == "filled":
                            print(f"[ORDER CLIENT] {timestamp} | Fill Price: ${response['filled_price']:.2f}")

                        # Send response back to C++
                        with open(RESPONSE_FIFO, 'w') as response_fifo:
                            json.dump(response, response_fifo)
                            response_fifo.write('\n')
                            response_fifo.flush()

                        orders_processed += 1
                        print()

                    except json.JSONDecodeError as e:
                        print(f"[ORDER CLIENT] ❌ Invalid JSON: {e}")
                    except Exception as e:
                        print(f"[ORDER CLIENT] ❌ Error processing order: {e}")

    except KeyboardInterrupt:
        print("\n[ORDER CLIENT] Stopped by user")
    except Exception as e:
        print(f"\n[ORDER CLIENT] ❌ Fatal error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        # Cleanup
        for fifo_path in [ORDER_FIFO, RESPONSE_FIFO]:
            if os.path.exists(fifo_path):
                os.remove(fifo_path)
                print(f"[ORDER CLIENT] Removed FIFO: {fifo_path}")


if __name__ == "__main__":
    main()
