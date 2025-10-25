# SIGOR Live Trading - Quick Start

Get SIGOR running in live paper trading mode in 5 minutes!

## Prerequisites

- Alpaca paper trading account (free): https://alpaca.markets
- Python 3.8+
- macOS or Linux

## Setup (One-Time)

### 1. Get Alpaca API Keys

1. Sign up at https://alpaca.markets
2. Go to paper trading dashboard
3. Generate API keys (save them!)

### 2. Configure Credentials

```bash
# Create config file
cat > config.env << 'EOF'
export ALPACA_PAPER_API_KEY="your_key_here"
export ALPACA_PAPER_SECRET_KEY="your_secret_here"
EOF

# Load credentials
source config.env
```

### 3. Install Dependencies

```bash
# Python packages
pip3 install alpaca-py certifi

# Build C++ components
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run Live Trading

```bash
# Start live trading (observation mode)
./scripts/launch_sigor_live.sh
```

That's it! SIGOR will:
- Connect to Alpaca WebSocket
- Receive real-time 1-minute bars
- Generate trading signals
- Display performance metrics

## What You'll See

```
═══════════════════════════════════════════════════════════════
  SIGOR Live Trading - Alpaca Paper Account
═══════════════════════════════════════════════════════════════

✓ Alpaca credentials found
   API Key: PKX...

Checking dependencies...
✓ python3 found
✓ alpaca-py installed
✓ certifi installed

Starting components...

[1/3] Starting Alpaca WebSocket bridge...
✓ WebSocket bridge started (PID: 12345)
      Log: logs/live/websocket_bridge.log

[2/3] Order client: DISABLED (observation mode only)
      Enable in script to submit real orders

[3/3] Starting SIGOR trading engine...

═══════════════════════════════════════════════════════════════

╔════════════════════════════════════════════════════════════╗
║         Sentio Lite - Rotation Trading System             ║
╚════════════════════════════════════════════════════════════╝

Configuration:
  Data Source:     Alpaca WebSocket (IEX)
  Order Submission: Alpaca REST API
  Bar FIFO:        /tmp/alpaca_bars.fifo

Initializing trader...
✅ Trader initialized

📡 Opening FIFO pipe for incoming bars...
✅ Connected to websocket bridge
🚀 LIVE TRADING ACTIVE - Processing real-time bars
   Press Ctrl+C to stop

═══════════════════════════════════════════════════════════════

[10:15:23] TQQQ @ 45.23 | Bars: 120 | Snapshots: 45

📊 [Status Update] Snapshot 20
   Equity: $100,542.15 (+0.54%)
   Trades: 8 | Positions: 2
   Win Rate: 62.5%
```

## Trading Hours

- **Start:** 9:25 AM ET (5 min before market open)
- **Market Open:** 9:30 AM ET
- **Market Close:** 4:00 PM ET

## Stop Trading

Press `Ctrl+C` to stop gracefully. All positions will be closed at market close automatically.

## Monitor Performance

```bash
# View websocket bridge logs
tail -f logs/live/websocket_bridge.log

# Check Alpaca dashboard
open https://app.alpaca.markets/paper/dashboard
```

## Observation Mode vs. Order Submission

**Default: Observation Mode**
- Generates signals ✅
- Shows what it would trade ✅
- Does NOT submit orders ✅
- Zero risk, pure monitoring ✅

**Enable Order Submission:**

Edit `scripts/launch_sigor_live.sh` and uncomment:

```bash
# 2. Start Alpaca Order Client
python3 scripts/alpaca_order_client.py > logs/live/order_client.log 2>&1 &
ORDER_CLIENT_PID=$!
```

⚠️ **Only enable after observing performance for several days!**

## Expected Performance

Based on backtest (2025-10-22):
- **Daily Return:** +1.05%
- **Trades:** 50-150 per day
- **Win Rate:** 50-60%
- **Max Positions:** 3 concurrent

Live results may vary due to slippage and market conditions.

## Configuration Files

- `config/symbols.conf` - Trading symbols (12 leveraged ETFs)
- `config/sigor_params.json` - SIGOR detector parameters
- `config/sigor_trading_params.json` - Risk management settings

## Troubleshooting

### "Authentication failed"

```bash
# Check credentials are set
echo $ALPACA_PAPER_API_KEY

# Re-load config
source config.env
```

### "Failed to open bar FIFO"

```bash
# Check if bridge is running
ps aux | grep alpaca_websocket_bridge

# Restart the system
pkill -f alpaca_websocket_bridge
./scripts/launch_sigor_live.sh
```

### "No bars received"

- Market must be open (9:30-4:00 PM ET, weekdays)
- Check internet connection
- Verify symbols are valid

## Full Documentation

See `docs/SIGOR_LIVE_TRADING_GUIDE.md` for:
- Architecture details
- Advanced configuration
- Performance optimization
- Safety features
- FAQ

## Next Steps

1. ✅ Run in observation mode for 3-5 days
2. ✅ Analyze results and performance
3. ⏭️ Adjust parameters if needed
4. ⏭️ Enable order submission when confident
5. ⏭️ Scale up capital gradually

## Support

- **Documentation:** `docs/SIGOR_LIVE_TRADING_GUIDE.md`
- **Issues:** GitHub issues page
- **Alpaca Support:** https://alpaca.markets/support

---

**Happy Trading! 📈**

*This is paper trading with virtual money. Practice extensively before considering real capital.*
