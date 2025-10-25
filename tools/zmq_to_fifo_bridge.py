#!/usr/bin/env python3
import argparse
import os
import sys
import json

try:
    import zmq
except Exception:
    print("❌ pyzmq not available: pip install pyzmq", file=sys.stderr)
    sys.exit(2)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--zmq-url', default='tcp://127.0.0.1:5555')
    p.add_argument('--topic', default='BARS')
    p.add_argument('--fifo', default='/tmp/alpaca_bars.fifo')
    args = p.parse_args()

    # Ensure FIFO exists
    if not os.path.exists(args.fifo):
        os.mkfifo(args.fifo)

    # Open FIFO for writing in blocking mode; reopen on BrokenPipe
    def open_fifo_writer(path: str):
        while True:
            try:
                return open(path, 'w')
            except Exception:
                pass

    fifo = open_fifo_writer(args.fifo)

    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    sub.setsockopt(zmq.RCVHWM, 1000)
    sub.setsockopt_string(zmq.SUBSCRIBE, args.topic)
    sub.connect(args.zmq-url if hasattr(args, 'zmq-url') else args.zmq_url)
    print(f"✅ ZMQ Subscriber connected to {args.zmq_url} topic={args.topic}")

    try:
        while True:
            msg = sub.recv_string()
            # Expect: "TOPIC {json}"
            try:
                space = msg.find(' ')
                if space <= 0:
                    continue
                payload = msg[space+1:]
                # Validate JSON
                json.loads(payload)
                while True:
                    try:
                        fifo.write(payload + '\n')
                        fifo.flush()
                        break
                    except BrokenPipeError:
                        try:
                            fifo.close()
                        except Exception:
                            pass
                        fifo = open_fifo_writer(args.fifo)
            except Exception:
                continue
    except KeyboardInterrupt:
        pass
    finally:
        try:
            fifo.close()
        except Exception:
            pass
        sub.close(0)
        ctx.term()

    return 0


if __name__ == '__main__':
    sys.exit(main())


