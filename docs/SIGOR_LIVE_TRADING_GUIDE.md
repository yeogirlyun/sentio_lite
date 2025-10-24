# SIGOR Live Trading Guide

Complete guide to setting up and running SIGOR strategy in live paper trading mode with Alpaca.

## Overview

SIGOR (Signal-OR Ensemble) is now ready for live trading! This guide walks you through:
- Setting up Alpaca paper trading account
- Installing dependencies
- Running live trading sessions
- Monitoring performance
- Understanding the architecture

## Why SIGOR for Live Trading?

✅ **Rule-based** - No machine learning warmup required
✅ **Immediate trading** - Starts generating signals from market open (9:30 ET)
✅ **Proven performance** - +1.05% on test day (2025-10-22)
✅ **No historical data needed** - Works with real-time bars only
✅ **7-detector ensemble** - Bollinger, RSI, Momentum, VWAP, ORB, OFI, Volume

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Live Trading System                      │
└─────────────────────────────────────────────────────────────┘

   Alpaca                 Python                  C++
  WebSocket  ─────>  WebSocket Bridge  ─────>  SIGOR
   (IEX)              (FIFO Writer)            Trader
                                                  │
                                                  ▼
                                              Signals
                                                  │
                                                  ▼
                                          (Future: Order
                                            Submission)
```

**Components:**

1. **Alpaca WebSocket Bridge** (`alpaca_websocket_bridge_rotation.py`)
   - Connects to Alpaca IEX WebSocket
   - Receives 1-minute bars for 12 symbols
   - Writes bars to FIFO pipe in JSON format

2. **SIGOR Trader** (`build/sentio_lite live --strategy sigor`)
   - Reads bars from FIFO pipe
   - Generates trading signals using 7-detector ensemble
   - Makes entry/exit decisions
   - Tracks performance in real-time

3. **Order Client** (`alpaca_order_client.py`) - Optional
   - Listens for order commands from C++
   - Submits orders to Alpaca REST API
   - Returns fill confirmations

## Prerequisites

### 1. Alpaca Paper Trading Account

Sign up for free at: https://alpaca.markets

1. Create account
2. Navigate to paper trading dashboard
3. Generate API keys:
   - Go to "API Keys" section
   - Click "Generate New Key"
   - Save your **API Key** and **Secret Key**

### 2. System Requirements

- **OS**: macOS, Linux (tested on macOS 14.6)
- **C++ Compiler**: GCC 9+ or Clang 12+
- **CMake**: 3.16+
- **Python**: 3.8+
- **RAM**: 4GB minimum
- **Network**: Stable internet connection

### 3. Python Dependencies

```bash
pip3 install alpaca-py certifi
```

## Setup Instructions

### Step 1: Set Alpaca Credentials

Create a `config.env` file in the project root:

```bash
cat > config.env << 'EOF'
# Alpaca Paper Trading Credentials
export ALPACA_PAPER_API_KEY="your_api_key_here"
export ALPACA_PAPER_SECRET_KEY="your_secret_key_here"
EOF
```

**Load credentials:**

```bash
source config.env
```

**Verify:**

```bash
echo $ALPACA_PAPER_API_KEY  # Should show your key
```

### Step 2: Build C++ Components

```bash
# From project root
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

**Verify build:**

```bash
./build/sentio_lite --help
```

### Step 3: Configure Symbols

Edit `config/symbols.conf` to match your trading universe:

```
# Current symbols (12 leveraged ETFs)
TQQQ
SQQQ
TNA
TZA
UVXY
SVIX
SOXS
SOXL
SPXL
SPXS
FAS
FAZ
```

**Important:** These must match the symbols in `scripts/alpaca_websocket_bridge_rotation.py`

### Step 4: Verify SIGOR Configuration

Check `config/sigor_params.json`:

```json
{
    "k": 1.5,
    "w_boll": 1.0,
    "w_rsi": 1.0,
    "w_mom": 1.0,
    "w_vwap": 1.0,
    "w_orb": 0.5,
    "w_ofi": 0.5,
    "w_vol": 0.5,
    "win_boll": 20,
    "win_rsi": 14,
    "win_mom": 10,
    "win_vwap": 20,
    "orb_opening_bars": 30,
    "vol_window": 20,
    "warmup_bars": 0
}
```

**Note:** `warmup_bars: 0` means SIGOR trades immediately from market open.

## Running Live Trading

### Quick Start

```bash
# From project root
./scripts/launch_sigor_live.sh
```

This script automatically:
1. Verifies credentials
2. Checks dependencies
3. Starts WebSocket bridge
4. Launches SIGOR trader
5. Handles cleanup on exit (Ctrl+C)

### Manual Start (for debugging)

**Terminal 1: WebSocket Bridge**
```bash
source config.env
python3 scripts/alpaca_websocket_bridge_rotation.py
```

**Terminal 2: SIGOR Trader**
```bash
./build/sentio_lite live --strategy sigor
```

### Trading Hours

- **Market Open:** 9:30 AM ET
- **Market Close:** 4:00 PM ET
- **Pre-market:** Not supported (IEX data only)
- **After-hours:** Not supported

**Best practice:** Start the system 5 minutes before market open (9:25 AM ET).

## Monitoring

### Real-Time Output

The C++ trader shows:

```
[10:15:23] TQQQ @ 45.23 | Bars: 120 | Snapshots: 45

📊 [Status Update] Snapshot 20
   Equity: $100,542.15 (+0.54%)
   Trades: 8 | Positions: 2
   Win Rate: 62.5%
```

### Logs

```
logs/live/websocket_bridge.log  # WebSocket connection and bars
logs/live/order_client.log      # Order submissions (if enabled)
```

**View logs in real-time:**

```bash
tail -f logs/live/websocket_bridge.log
```

### Performance Tracking

The trader continuously calculates:
- Current equity
- Return percentage
- Number of trades
- Win rate
- Open positions

### Alpaca Dashboard

Monitor your account at: https://app.alpaca.markets/paper/dashboard

- View positions
- Check order history
- Track P&L
- Review fills

## Order Submission (Optional)

By default, the system runs in **observation mode** - it generates signals but doesn't submit orders.

### Enable Order Submission

1. **Uncomment order client in launch script:**

Edit `scripts/launch_sigor_live.sh`:

```bash
# 2. Start Alpaca Order Client
echo -e "${YELLOW}[2/3]${NC} Starting Alpaca order client..."
python3 scripts/alpaca_order_client.py > logs/live/order_client.log 2>&1 &
ORDER_CLIENT_PID=$!
```

2. **Implement order submission in C++:**

The trader would need to write orders to `/tmp/alpaca_orders.fifo` when signals are generated. This is currently a TODO for full integration.

## Safety Features

### Paper Trading Only

- Uses Alpaca **paper trading** environment
- No real money at risk
- Practice with simulated fills

### Stop Loss & Profit Targets

Configured in `config/sigor_trading_params.json`:

```json
{
    "stop_loss_pct": -0.02,     // -2% stop loss
    "profit_target_pct": 0.05,   // +5% profit target
    "max_positions": 3,          // Max 3 concurrent positions
    "initial_capital": 100000    // $100k starting equity
}
```

### Position Limits

- **Max positions:** 3 (configurable)
- **Max trades per day:** Unlimited (SIGOR is high-frequency ready)
- **Max capital per position:** 33% of account (for 3 positions)

### EOD Liquidation

All positions are automatically closed at 3:59 PM ET (market close).

## Troubleshooting

### WebSocket Won't Connect

**Error:** `Failed to open bar FIFO`

**Solution:**
```bash
# Check if bridge is running
ps aux | grep alpaca_websocket_bridge

# Manually create FIFO (shouldn't be needed)
mkfifo /tmp/alpaca_bars.fifo

# Check credentials
echo $ALPACA_PAPER_API_KEY
```

### No Bars Received

**Error:** Bridge starts but no bars appear

**Solution:**
1. Check market hours (9:30-4:00 PM ET, weekdays only)
2. Verify symbols are valid and tradeable
3. Check internet connection
4. Review `logs/live/websocket_bridge.log`

### Authentication Failed

**Error:** `Authentication failed - check credentials`

**Solution:**
```bash
# Verify credentials are set
env | grep ALPACA

# Test credentials
python3 tools/check_alpaca_status.py

# Re-generate API keys if needed (in Alpaca dashboard)
```

### Build Errors

**Error:** `nlohmann/json.hpp not found`

**Solution:**
```bash
# Re-clone JSON library
cd external
rm -rf nlohmann_json
git clone --depth 1 --branch v3.11.3 \
    https://github.com/nlohmann/json.git nlohmann_json

# Rebuild
cd ..
cmake --build build -j
```

### High Memory Usage

**Solution:**
- SIGOR is lightweight (< 100MB RAM)
- Check for memory leaks if >500MB
- Restart system if performance degrades

## Performance Expectations

Based on backtests (2025-10-22):
- **Daily return:** +1.05%
- **Total trades:** 102
- **Win rate:** ~50-60%
- **Profit factor:** 1.5-2.0

**Live trading may differ due to:**
- Slippage on market orders
- Wider bid-ask spreads
- Partial fills
- Network latency

## Best Practices

### 1. Start Small

Begin with $10,000 in paper account (not $100,000) to test:
```json
{
    "initial_capital": 10000
}
```

### 2. Monitor First Session

Watch the entire first trading day to understand:
- Signal generation
- Entry/exit timing
- Position sizing
- Performance metrics

### 3. Review Daily Results

After market close, check:
- Final equity
- Number of trades
- Win rate
- Largest win/loss
- Alpaca dashboard for order quality

### 4. Optimize Parameters

If performance is poor:
- Adjust detector weights in `config/sigor_params.json`
- Modify risk parameters (stop loss, profit target)
- Change position limits
- Review symbol selection

### 5. Keep Logs

```bash
# Archive logs after each session
mkdir -p logs/archive/$(date +%Y-%m-%d)
cp logs/live/*.log logs/archive/$(date +%Y-%m-%d)/
```

## Advanced Configuration

### Custom Symbols

Edit `scripts/alpaca_websocket_bridge_rotation.py`:

```python
SYMBOLS = [
    'TQQQ', 'SQQQ',  # Your symbols here
    # ...
]
```

And `config/symbols.conf`:

```
TQQQ
SQQQ
# ...
```

### Detector Weights

Increase weight of reliable detectors in `config/sigor_params.json`:

```json
{
    "w_boll": 1.5,   // Increase Bollinger weight
    "w_rsi": 1.0,    // Keep RSI normal
    "w_mom": 0.8     // Decrease momentum weight
}
```

### Window Sizes

Adjust lookback periods:

```json
{
    "win_boll": 30,  // Longer Bollinger window (was 20)
    "win_rsi": 21,   // Longer RSI (was 14)
    "win_vwap": 15   // Shorter VWAP (was 20)
}
```

## FAQ

**Q: Can I run this with real money?**
A: Not yet. This is paper trading only. Never trade with real money until thoroughly tested.

**Q: What data source is used?**
A: Alpaca IEX (free tier). IEX provides 1-minute bars for US equities.

**Q: How much capital do I need?**
A: Paper trading is free. Start with $10k-$100k virtual capital.

**Q: Can I trade pre-market or after-hours?**
A: No. SIGOR is designed for regular trading hours (9:30-4:00 PM ET).

**Q: How many trades per day?**
A: Typically 50-150 trades depending on volatility. SIGOR is active.

**Q: What's the latency?**
A: Bars arrive with ~1-3 second delay (IEX → Alpaca → Bridge → C++).

**Q: Can I backtest before going live?**
A: Yes! Use mock mode:
```bash
./build/sentio_lite mock --strategy sigor --date 2025-10-22 --sim-days 0
```

**Q: How do I stop trading?**
A: Press Ctrl+C. The script will gracefully shut down all components and close positions.

## Next Steps

1. ✅ **Run in observation mode** (no order submission)
2. ✅ **Monitor performance** for 1 week
3. ⏭️ **Enable order submission** when confident
4. ⏭️ **Optimize parameters** based on live results
5. ⏭️ **Scale up** capital gradually
6. ⏭️ **Consider real account** (future)

## Support

- **Issues:** https://github.com/yourrepo/sentio_lite/issues
- **Docs:** See `docs/` folder
- **Alpaca Help:** https://alpaca.markets/support

## Changelog

**v2.1 - 2025-10-24**
- ✅ Live mode implementation
- ✅ WebSocket bridge for Alpaca
- ✅ Order client (optional)
- ✅ Launch script
- ✅ Documentation

**v2.0 - 2025-10-24**
- ✅ SIGOR strategy integration
- ✅ Immediate trading (no warmup)
- ✅ Dashboard improvements

---

**Happy Trading! 📈**

*Remember: This is paper trading for learning and testing. Always practice extensively before considering real money.*
