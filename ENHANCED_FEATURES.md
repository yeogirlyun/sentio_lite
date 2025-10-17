# Enhanced Features - sentio_lite v2.0

## What's New

Enhanced sentio_lite with production-ready features:

✅ **12-Symbol Support** - Default symbol lists (6, 12, or 14 symbols)
✅ **Mock & Live Modes** - Backtest or paper trade
✅ **Date Range Filtering** - Test specific periods
✅ **Warmup Period** - Configurable warmup (default 3 days)
✅ **Dashboard Generation** - HTML report with visualizations
✅ **Results Export** - JSON output for analysis

---

## Quick Start

### 1. Use Default 12 Symbols (Recommended)

```bash
cd build
./sentio_lite --symbols 12
```

**12 Default Symbols:**
- TQQQ, SQQQ (3x QQQ)
- SSO, SDS (2x SPY)
- UPRO, SPXS (3x SPY)
- TNA, TZA (3x Russell)
- FAS, FAZ (3x Finance)
- UVXY, SVXY (Volatility)

### 2. Test Specific Date Range

```bash
./sentio_lite --symbols 12 \
  --start-date 2024-10-01 \
  --end-date 2024-10-31
```

### 3. Generate Dashboard Report

```bash
./sentio_lite --symbols 12 \
  --generate-dashboard \
  --warmup-days 5
```

This will:
1. Run backtest with 12 symbols
2. Use 5-day warmup period
3. Export results to `results.json`
4. Generate `dashboard_report.html`

---

## New Command-Line Options

### Mode Options

#### --mode MODE
Trading mode: `mock` (default) or `live`

```bash
# Mock mode (backtesting)
./sentio_lite --symbols 12 --mode mock

# Live mode (paper trading - not yet implemented)
./sentio_lite --symbols 6 --mode live
```

#### --start-date DATE
Start date in YYYY-MM-DD format

```bash
./sentio_lite --symbols 12 --start-date 2024-10-01
```

#### --end-date DATE
End date in YYYY-MM-DD format

```bash
./sentio_lite --symbols 12 --end-date 2024-10-31
```

#### --warmup-days N
Warmup period before trading starts (default: 3 days)

```bash
# 5-day warmup (1950 bars)
./sentio_lite --symbols 12 --warmup-days 5

# 1-day warmup (390 bars)
./sentio_lite --symbols 12 --warmup-days 1
```

**Note:** Warmup bars = warmup_days × 390 (bars per day)

### Symbol Lists

#### --symbols 6|12|14|SYM1,SYM2,...
Use predefined list or custom symbols

```bash
# Default 6 symbols
./sentio_lite --symbols 6

# Default 12 symbols
./sentio_lite --symbols 12

# Default 14 symbols
./sentio_lite --symbols 14

# Custom list
./sentio_lite --symbols TQQQ,SQQQ,UPRO,SDS
```

**Predefined Lists:**

**6 Symbols (Core):**
- TQQQ, SQQQ - 3x QQQ
- UPRO, SDS - 3x/2x SPY
- UVXY, SVXY - Volatility

**12 Symbols (Extended):**
- All 6 above, plus:
- SSO, SDS - 2x SPY
- UPRO, SPXS - 3x SPY
- TNA, TZA - 3x Russell
- FAS, FAZ - 3x Finance

**14 Symbols (Maximum):**
- All 12 above, plus:
- ERX, ERY - 3x Energy
- NUGT, DUST - 3x Gold Miners

### Dashboard Options

#### --generate-dashboard
Generate HTML dashboard after backtest

```bash
./sentio_lite --symbols 12 --generate-dashboard
```

Creates:
- `results.json` - Results in JSON format
- `dashboard_report.html` - Interactive dashboard

#### --results-file FILE
Specify output JSON file (default: results.json)

```bash
./sentio_lite --symbols 12 \
  --generate-dashboard \
  --results-file backtest_oct_2024.json
```

---

## Usage Examples

### Example 1: Quick Backtest with 12 Symbols

```bash
./sentio_lite --symbols 12
```

**Output:**
```
╔════════════════════════════════════════════════════════════╗
║         Sentio Lite - Online Trading System               ║
╚════════════════════════════════════════════════════════════╝

Configuration:
  Mode: MOCK
  Symbols (12): TQQQ, SQQQ, SSO, SDS, UPRO, SPXS, TNA, TZA, FAS, FAZ, UVXY, SVXY
  Warmup Period: 3 days (1170 bars)
  Initial Capital: $100000.00
  Max Positions: 3

Loading market data...
Loaded 98304 bars from data/TQQQ.bin
...
Data loaded in 876ms

Running MOCK mode with 98304 bars...
  Warmup: 1170 bars
  Trading: 97134 bars

✅ Warmup complete, starting trading...
...
```

### Example 2: October 2024 Backtest

```bash
./sentio_lite --symbols 12 \
  --start-date 2024-10-01 \
  --end-date 2024-10-31 \
  --warmup-days 5 \
  --verbose
```

**What it does:**
1. Loads data for 12 symbols
2. Filters to October 2024 only
3. Uses 5-day warmup (from Sept 24-30)
4. Shows detailed progress every 1000 bars

### Example 3: Dashboard Generation

```bash
./sentio_lite --symbols 12 \
  --start-date 2024-10-01 \
  --end-date 2024-10-31 \
  --generate-dashboard \
  --results-file oct_2024_results.json
```

**Output files:**
- `oct_2024_results.json` - Results data
- `dashboard_report.html` - Visual dashboard

**To view:**
```bash
open dashboard_report.html  # macOS
xdg-open dashboard_report.html  # Linux
```

### Example 4: Conservative Trading (Lower Risk)

```bash
./sentio_lite --symbols 6 \
  --max-positions 2 \
  --stop-loss -0.03 \
  --profit-target 0.08 \
  --capital 50000
```

**Parameters:**
- Only 6 symbols (most liquid)
- Max 2 positions (lower exposure)
- Wider stops (3% vs 2%)
- Higher targets (8% vs 5%)
- Smaller capital

### Example 5: Aggressive Trading (Higher Risk)

```bash
./sentio_lite --symbols 14 \
  --max-positions 4 \
  --stop-loss -0.015 \
  --profit-target 0.03 \
  --lambda 0.95
```

**Parameters:**
- 14 symbols (maximum diversity)
- Max 4 positions (higher exposure)
- Tighter stops (1.5% vs 2%)
- Lower targets (3% vs 5%)
- Faster adaptation (lambda 0.95)

### Example 6: Parameter Tuning

```bash
# Test different warmup periods
for days in 1 3 5 7; do
  ./sentio_lite --symbols 12 --warmup-days $days \
    --start-date 2024-10-01 --end-date 2024-10-31 \
    --results-file warmup_${days}d.json
done

# Test different lambda values
for lambda in 0.95 0.97 0.98 0.99; do
  ./sentio_lite --symbols 12 --lambda $lambda \
    --results-file lambda_${lambda}.json
done

# Test different position counts
for n in 1 2 3 4 5; do
  ./sentio_lite --symbols 12 --max-positions $n \
    --results-file positions_${n}.json
done
```

---

## Dashboard Features

When using `--generate-dashboard`, you get:

### Results JSON Format

```json
{
  "metadata": {
    "timestamp": "2024-10-17 10:30:45",
    "mode": "MOCK",
    "symbols": "TQQQ,SQQQ,UPRO,SDS,...",
    "start_date": "2024-10-01",
    "end_date": "2024-10-31",
    "initial_capital": 100000.0
  },
  "performance": {
    "final_equity": 115234.56,
    "total_return": 0.1523,
    "mrd": 0.0061,
    "total_trades": 234,
    "winning_trades": 142,
    "losing_trades": 92,
    "win_rate": 0.607,
    "avg_win": 876.43,
    "avg_loss": 423.21,
    "profit_factor": 1.89
  },
  "config": {
    "max_positions": 3,
    "stop_loss_pct": -0.02,
    "profit_target_pct": 0.05,
    "lambda": 0.98
  }
}
```

### Dashboard HTML

The generated dashboard includes:
- Performance summary
- Equity curve chart
- Trade distribution
- Win/loss statistics
- Symbol performance breakdown

---

## Warmup Period

### What is Warmup?

The warmup period allows the predictor to learn from market data before making trading decisions.

**Default: 3 days (1170 bars)**

### How Warmup Works

1. **Bars 1-1170:** Warmup period
   - Feature extraction starts immediately
   - Predictor learns from data
   - NO trading decisions made

2. **Bars 1171+:** Trading period
   - Predictor makes predictions
   - Trading decisions executed
   - Continuous learning continues

### Choosing Warmup Period

**1 day (390 bars):** Fast adaptation, less stable
**3 days (1170 bars):** Balanced (default)
**5 days (1950 bars):** More stable, slower adaptation
**7 days (2730 bars):** Maximum stability

**Recommendation:** Use 3-5 days for most cases

---

## Date Filtering

### Use Cases

**1. Test specific months:**
```bash
./sentio_lite --symbols 12 \
  --start-date 2024-10-01 \
  --end-date 2024-10-31
```

**2. Test recent data only:**
```bash
./sentio_lite --symbols 12 \
  --start-date 2024-09-01
```

**3. Walk-forward testing:**
```bash
# Train on September
./sentio_lite --symbols 12 \
  --start-date 2024-09-01 \
  --end-date 2024-09-30

# Test on October
./sentio_lite --symbols 12 \
  --start-date 2024-10-01 \
  --end-date 2024-10-31
```

**4. Specific week:**
```bash
./sentio_lite --symbols 12 \
  --start-date 2024-10-07 \
  --end-date 2024-10-11
```

---

## Performance Tips

### 1. Use Binary Data

10-100x faster than CSV:

```bash
# Fast
./sentio_lite --symbols 12 --extension .bin

# Slow
./sentio_lite --symbols 12 --extension .csv
```

### 2. Optimize Symbols Count

More symbols = more diversification but slower:

```bash
# Fastest
./sentio_lite --symbols 6

# Balanced
./sentio_lite --symbols 12

# Slowest but most diverse
./sentio_lite --symbols 14
```

### 3. Reduce Warmup for Testing

For quick tests:

```bash
./sentio_lite --symbols 12 --warmup-days 1
```

### 4. Use Date Filtering

Test smaller periods for faster results:

```bash
./sentio_lite --symbols 12 \
  --start-date 2024-10-01 \
  --end-date 2024-10-07
```

---

## Integration with online_trader

### Use online_trader Data

```bash
./sentio_lite --symbols 12 \
  --data-dir ../../online_trader/data/equities
```

### Use online_trader Tools

After generating results:

```bash
# Analyze with online_trader tools
cd ../../online_trader/tools
python compare_strategies.py ../../sentio_lite/build/results.json

# Generate advanced dashboard
cd ../scripts
python rotation_trading_dashboard.py ../../sentio_lite/build/results.json
```

---

## Troubleshooting

### Issue: Not enough bars for warmup

```
⚠️  Warning: Only 500 bars available, but 1170 needed for warmup
   Reducing warmup to 250 bars
```

**Solutions:**
1. Reduce warmup: `--warmup-days 1`
2. Get more data (extend date range)
3. Download more historical data

### Issue: Date filter returns no data

```
Error: No data after filtering
```

**Solutions:**
1. Check date format (must be YYYY-MM-DD)
2. Verify data exists for that period
3. Check data file timestamps

### Issue: Dashboard generation fails

```
⚠️  Dashboard generation failed (code: 127)
```

**Solutions:**
1. Install Python 3: `brew install python3`
2. Check script path: `ls ../scripts/rotation_trading_dashboard_html.py`
3. Install dependencies: `pip3 install matplotlib pandas`

---

## Best Practices

### 1. Start with Defaults

```bash
./sentio_lite --symbols 12
```

Test with default settings first to establish baseline.

### 2. Use Representative Periods

Test full months or quarters, not arbitrary dates:

```bash
# Good: Full month
--start-date 2024-10-01 --end-date 2024-10-31

# Bad: Arbitrary dates
--start-date 2024-10-05 --end-date 2024-10-23
```

### 3. Include Warmup in Date Range

If testing October, include September for warmup:

```bash
./sentio_lite --symbols 12 \
  --start-date 2024-09-24 \
  --end-date 2024-10-31 \
  --warmup-days 5
```

### 4. Save Results for Comparison

```bash
./sentio_lite --symbols 12 \
  --results-file baseline.json

./sentio_lite --symbols 12 --max-positions 4 \
  --results-file positions_4.json

# Compare
diff baseline.json positions_4.json
```

### 5. Generate Dashboards for Analysis

Always use `--generate-dashboard` for important tests:

```bash
./sentio_lite --symbols 12 \
  --start-date 2024-10-01 \
  --end-date 2024-10-31 \
  --generate-dashboard \
  --results-file oct_2024_production.json
```

---

## Command Reference

### Complete Example

```bash
./sentio_lite \
  --mode mock \
  --symbols 12 \
  --data-dir ../data \
  --extension .bin \
  --start-date 2024-10-01 \
  --end-date 2024-10-31 \
  --warmup-days 5 \
  --capital 100000 \
  --max-positions 3 \
  --stop-loss -0.02 \
  --profit-target 0.05 \
  --lambda 0.98 \
  --generate-dashboard \
  --results-file oct_2024.json \
  --verbose
```

### All Options Summary

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `--mode` | string | mock | Trading mode (mock/live) |
| `--symbols` | string | *required* | Symbol list (6/12/14 or custom) |
| `--data-dir` | string | data | Data directory |
| `--extension` | string | .bin | File extension |
| `--start-date` | date | earliest | Start date (YYYY-MM-DD) |
| `--end-date` | date | latest | End date (YYYY-MM-DD) |
| `--warmup-days` | int | 3 | Warmup days |
| `--capital` | double | 100000 | Initial capital |
| `--max-positions` | int | 3 | Max positions |
| `--stop-loss` | double | -0.02 | Stop loss % |
| `--profit-target` | double | 0.05 | Profit target % |
| `--lambda` | double | 0.98 | EWRLS lambda |
| `--generate-dashboard` | flag | false | Generate dashboard |
| `--results-file` | string | results.json | Results file |
| `--verbose` | flag | false | Verbose output |

---

## Next Steps

1. **Run first backtest:**
   ```bash
   ./sentio_lite --symbols 12
   ```

2. **Test specific period:**
   ```bash
   ./sentio_lite --symbols 12 \
     --start-date 2024-10-01 --end-date 2024-10-31
   ```

3. **Generate dashboard:**
   ```bash
   ./sentio_lite --symbols 12 --generate-dashboard
   ```

4. **Experiment with parameters:**
   ```bash
   ./sentio_lite --symbols 12 --max-positions 4 --lambda 0.95
   ```

5. **Compare results:**
   - Save different configurations
   - Analyze JSON outputs
   - Review dashboard charts

---

**Ready to trade with enhanced features! 📈**
