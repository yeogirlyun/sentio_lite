#!/usr/bin/env python3
"""
Replay results.json + trades.jsonl into the live FIFO for mock-live debugging.

Usage:
  python3 scripts/replay_fifo_from_results.py --results results.json --fifo /tmp/alpaca_bars.fifo --speed-ms 60
  python3 scripts/replay_fifo_from_results.py --date 2025-10-22 --data-dir data --fifo /tmp/alpaca_bars.fifo --speed-ms 60000

This writes minute-bucketed bars for all symbols to the FIFO at accelerated pace.
"""

import argparse
import json
import os
import time
from datetime import datetime


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--results', default='')
    p.add_argument('--date', default='')
    p.add_argument('--data-dir', default='data')
    p.add_argument('--fifo', default='/tmp/alpaca_bars.fifo')
    p.add_argument('--speed-ms', type=int, default=60)
    args = p.parse_args()

    def load_from_results(path: str):
        with open(path, 'r') as f:
            return json.load(f)

    def load_from_data(day: str, data_dir: str):
        # Minimal loader: read per-symbol day slice from binary/csv is out of scope here;
        # assume a prebuilt results-like JSON exists at logs/mock_results_<date>.json
        fallback = os.path.join('logs', f'mock_results_{day}.json')
        if os.path.exists(fallback):
            return load_from_results(fallback)
        raise SystemExit("No results provided and no mock_results_<date>.json found.")

    if args.results:
        results = load_from_results(args.results)
    elif args.date:
        results = load_from_data(args.date, args.data_dir)
    else:
        raise SystemExit("Provide --results or --date")

    price_data = results.get('price_data', {})
    # Build a timeline of minute timestamps from first symbol
    if not price_data:
        print('No price_data in results.json')
        return 1

    ref_sym = next(iter(price_data))
    timeline = []
    for bar in price_data.get(ref_sym, []):
        ms = int(bar.get('timestamp_ms', 0))
        if ms:
            timeline.append(int(ms // 60000 * 60000))
    timeline = sorted(dict.fromkeys(timeline))

    if not os.path.exists(args.fifo):
        os.mkfifo(args.fifo)
    fifo = open(args.fifo, 'w')

    # Emit bars in order
    for minute in timeline:
        for sym, bars in price_data.items():
            # Find bar at this minute
            found = None
            for b in bars:
                ms = int(b.get('timestamp_ms', 0))
                if int(ms // 60000 * 60000) == minute:
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
            fifo.write(json.dumps(payload) + '\n')
            fifo.flush()
        time.sleep(max(0, args.speed_ms) / 1000.0)

    fifo.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())


