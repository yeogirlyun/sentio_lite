#!/usr/bin/env python3
import argparse
import json
import os
import sys
import time

try:
    import zmq
except Exception as e:
    print("❌ pyzmq not available: pip install pyzmq", file=sys.stderr)
    sys.exit(2)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--results', required=True)
    p.add_argument('--topic', default='BARS')
    p.add_argument('--bind', default='tcp://127.0.0.1:5555')
    p.add_argument('--speed-ms', type=int, default=1000)
    args = p.parse_args()

    with open(args.results, 'r') as f:
        results = json.load(f)

    price_data = results.get('price_data', {})
    if not price_data:
        print('No price_data in results', file=sys.stderr)
        return 1

    # Build a sorted minute timeline (ms) from first symbol
    ref_sym = next(iter(price_data))
    timeline = sorted({int(bar.get('timestamp_ms', 0)) // 60000 * 60000 for bar in price_data.get(ref_sym, []) if bar.get('timestamp_ms')})

    ctx = zmq.Context()
    sock = ctx.socket(zmq.PUB)
    sock.setsockopt(zmq.SNDHWM, 1000)
    sock.setsockopt(zmq.LINGER, 0)
    sock.bind(args.bind)
    print(f"✅ ZMQ Publisher bound to {args.bind}")

    topic = args.topic

    for minute in timeline:
        for sym, bars in price_data.items():
            found = None
            for b in bars:
                ms = int(b.get('timestamp_ms', 0))
                if (ms // 60000) * 60000 == minute:
                    found = b
                    break
            if not found:
                continue

            payload = {
                'symbol': sym,
                'timestamp_ms': int(found['timestamp_ms']),
                'open': float(found['open']),
                'high': float(found['high']),
                'low': float(found['low']),
                'close': float(found['close']),
                'volume': int(found['volume'])
            }
            line = json.dumps(payload)
            sock.send_string(f"{topic} {line}")
        time.sleep(max(0, args.speed_ms) / 1000.0)

    return 0


if __name__ == '__main__':
    sys.exit(main())


