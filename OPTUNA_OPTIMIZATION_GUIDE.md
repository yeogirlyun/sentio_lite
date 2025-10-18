# Optuna Optimization Guide for Sentio Lite

**Date:** 2025-10-18
**Purpose:** Automated parameter optimization for warmup and trading strategy

---

## Overview

The Optuna optimization system automatically finds optimal parameters for:
1. **Trading parameters** (max positions, stop loss, profit target)
2. **EWRLS learning rate** (lambda values)
3. **Warmup configuration** (observation days, simulation days)

### Workflow

```
Historical Data (30 days) → Optuna Optimization (100 trials) → Optimal Parameters → Test/Live Trading
```

**Key Concept:** Optimize on past data, apply to future test date.

---

## Installation

### Prerequisites

```bash
# Install Python dependencies
pip3 install optuna pandas numpy
```

### Verify Setup

```bash
# Check binary exists
ls -l ./build/sentio_lite

# Test optimization script
python3 tools/optimize_warmup.py --help
```

---

## Quick Start

### Example 1: Optimize for Tomorrow's Live Trading

```bash
# Optimize parameters using last 30 days of data
# Then prepare for tomorrow's live trading
python3 tools/optimize_warmup.py \
    --test-date 2025-10-19 \
    --n-trials 100 \
    --mode live
```

**Output:**
- Runs 100 optimization trials on Oct 1-18 data
- Saves best parameters to `optimal_params.json`
- Ready for live trading on Oct 19

---

### Example 2: Optimize and Test on Historical Date

```bash
# Optimize for October 15, test immediately
python3 tools/optimize_warmup.py \
    --test-date 2025-10-15 \
    --n-trials 50 \
    --mode mock
```

**Output:**
- Optimizes on Sept 15 - Oct 14 data
- Tests on Oct 15 with optimal parameters
- Shows results immediately

---

### Example 3: Multi-Day Test with Optimization

```bash
# Optimize for a 5-day test period
python3 tools/optimize_warmup.py \
    --start-date 2025-10-15 \
    --end-date 2025-10-18 \
    --n-trials 100
```

**Output:**
- Optimizes on Sept 15 - Oct 14
- Tests on Oct 15-18 (5 days)
- Validates consistency

---

### Example 4: Use Previously Optimized Parameters

```bash
# Skip optimization, use saved parameters
python3 tools/optimize_warmup.py \
    --test-date 2025-10-19 \
    --use-best \
    --mode live
```

**Output:**
- Loads `optimal_params.json`
- Applies to Oct 19 live trading
- Fast execution (no optimization)

---

## Parameter Search Space

The optimizer explores the following ranges:

### Trading Parameters
```python
max_positions: 2 to 8              # Concurrent positions
stop_loss: -3.0% to -1.0%          # Stop loss threshold
profit_target: 2.0% to 8.0%        # Profit target threshold
```

### Learning Parameters
```python
lambda: 0.980 to 0.998             # EWRLS forgetting factor
```

### Warmup Parameters
```python
enable_warmup: True or False       # Enable warmup system
warmup_obs_days: 1 to 5            # Observation phase days
warmup_sim_days: 3 to 10           # Simulation phase days
```

---

## Optimization Objective

The optimizer maximizes a composite score:

```python
score = (
    sharpe_proxy * 0.5 +                          # 50%: Risk-adjusted return
    (profit_factor - 1.0) * 10 * 0.25 +           # 25%: Win/loss ratio
    (win_rate - 50) * 0.15 +                      # 15%: Win percentage
    (min(total_trades, 100) / 100) * 10 * 0.1     # 10%: Trade activity
)
```

**Components:**
1. **Sharpe Proxy** (MRD): Higher is better
2. **Profit Factor**: > 1.0 is profitable
3. **Win Rate**: Target ~50%
4. **Trade Count**: Ensures sufficient activity

---

## Output Files

### `optimal_params.json`

Contains the best parameters found:

```json
{
  "test_date": "2025-10-19",
  "optimization_period": {
    "start": "2025-09-05",
    "end": "2025-10-18"
  },
  "parameters": {
    "max_positions": 5,
    "stop_loss": -0.018,
    "profit_target": 0.035,
    "lambda": 0.992,
    "enable_warmup": true,
    "warmup_obs_days": 3,
    "warmup_sim_days": 7
  },
  "metrics": {
    "total_return": 2.45,
    "profit_factor": 1.32,
    "win_rate": 52.3,
    "sharpe_proxy": 0.35,
    "total_trades": 456
  },
  "score": 5.67,
  "trial_number": 87,
  "timestamp": "2025-10-18T14:30:45.123456"
}
```

### `results.json`

Backtest results from each trial (overwritten each trial).

### `final_results.json`

Results from the final test with optimized parameters.

---

## Advanced Usage

### Custom Optimization Period

```bash
# Use 60 days of history instead of default 30
python3 tools/optimize_warmup.py \
    --test-date 2025-10-19 \
    --optimization-days 60 \
    --n-trials 200
```

### Parallel Optimization (Faster)

```bash
# Run 4 trials in parallel
python3 tools/optimize_warmup.py \
    --test-date 2025-10-19 \
    --n-trials 100 \
    --n-jobs 4
```

**Note:** Requires sufficient CPU cores.

### Quick Optimization (Development)

```bash
# Fast optimization for testing (10 trials)
python3 tools/optimize_warmup.py \
    --test-date 2025-10-15 \
    --n-trials 10
```

---

## Production Deployment Workflow

### Daily Pre-Market Routine

```bash
#!/bin/bash
# File: scripts/daily_optimization.sh

# Get tomorrow's date
TOMORROW=$(date -v+1d +"%Y-%m-%d")

echo "Optimizing for $TOMORROW..."

# Run optimization
python3 tools/optimize_warmup.py \
    --test-date "$TOMORROW" \
    --n-trials 100 \
    --optimization-days 30 \
    --mode live \
    --n-jobs 4

# Check if optimization succeeded
if [ $? -eq 0 ]; then
    echo "✅ Optimization complete. Parameters ready for $TOMORROW"
    cat optimal_params.json | jq '.parameters'
else
    echo "❌ Optimization failed"
    exit 1
fi
```

**Schedule:** Run at 6:00 AM ET daily (cron job).

---

### Manual Live Trading Launch

After optimization completes:

```bash
# Review optimized parameters
cat optimal_params.json | jq

# Extract parameters
MAX_POS=$(jq -r '.parameters.max_positions' optimal_params.json)
STOP_LOSS=$(jq -r '.parameters.stop_loss' optimal_params.json)
PROFIT=$(jq -r '.parameters.profit_target' optimal_params.json)
LAMBDA=$(jq -r '.parameters.lambda' optimal_params.json)
WARMUP=$(jq -r '.parameters.enable_warmup' optimal_params.json)

# Launch live trading at 9:30 AM
./build/sentio_lite live \
    --max-positions "$MAX_POS" \
    --stop-loss "$STOP_LOSS" \
    --profit-target "$PROFIT" \
    --lambda "$LAMBDA" \
    $([ "$WARMUP" == "true" ] && echo "--enable-warmup") \
    --warmup-days 1
```

---

## Interpreting Results

### Good Optimization Results

```
Best score: 8.50
Best parameters:
  max_positions: 5
  stop_loss: -0.015
  profit_target: 0.04
  lambda: 0.990
  enable_warmup: true

Best metrics:
  total_return: 3.2%
  profit_factor: 1.45
  win_rate: 54.2%
  sharpe_proxy: 0.45
```

**Indicators:**
- ✅ Positive return (> 0%)
- ✅ Profit factor > 1.2
- ✅ Win rate > 50%
- ✅ Sharpe proxy > 0.3

**Action:** Deploy with confidence

---

### Marginal Results

```
Best score: 2.10
Best parameters:
  ...

Best metrics:
  total_return: 0.8%
  profit_factor: 1.05
  win_rate: 48.5%
  sharpe_proxy: 0.12
```

**Indicators:**
- ⚠️ Small positive return
- ⚠️ Profit factor barely > 1
- ⚠️ Win rate < 50%
- ⚠️ Sharpe < 0.3

**Action:** Paper trade first, monitor closely

---

### Poor Results

```
Best score: -5.30
Best parameters:
  ...

Best metrics:
  total_return: -2.1%
  profit_factor: 0.75
  win_rate: 42.0%
  sharpe_proxy: -0.35
```

**Indicators:**
- ❌ Negative return
- ❌ Profit factor < 1
- ❌ Low win rate
- ❌ Negative Sharpe

**Action:** **DO NOT** deploy. Review strategy, extend optimization period, or skip trading day.

---

## Troubleshooting

### Issue: "Binary not found"

```bash
# Solution: Build the binary first
cmake --build build
```

### Issue: "No data for optimization period"

```bash
# Solution: Download more data
cd /Volumes/ExternalSSD/Dev/C++/online_trader
export POLYGON_API_KEY=your_key
python3 tools/data_downloader.py TQQQ SQQQ SSO SDS TNA TZA UVXY SVIX SOXS SOXL \
    --start 2025-09-01 --end 2025-10-18 --outdir data/equities
```

### Issue: "Optimization takes too long"

```bash
# Solution: Reduce trials or use parallel processing
python3 tools/optimize_warmup.py \
    --test-date 2025-10-19 \
    --n-trials 50 \        # Reduced from 100
    --n-jobs 4             # Parallel execution
```

### Issue: "All trials fail"

Check:
1. Data availability for optimization period
2. Binary runs successfully: `./build/sentio_lite mock --date 2025-10-15 --warmup-days 1 --no-dashboard`
3. Results file is created: `ls -l results.json`

---

## Best Practices

### 1. Optimization Frequency

**Daily:** Run optimization each evening for next day
**Weekly:** Re-optimize if market regime changes
**Monthly:** Full parameter review and walk-forward validation

### 2. Optimization Period

**Recommended:** 30 days (default)
**Minimum:** 20 days (ensure statistical significance)
**Maximum:** 60 days (avoid overfitting to old data)

### 3. Number of Trials

**Quick test:** 10-20 trials (~2-5 minutes)
**Development:** 50 trials (~10-15 minutes)
**Production:** 100-200 trials (~20-40 minutes)

### 4. Validation

After optimization, always:
1. **Review metrics** (return, profit factor, Sharpe)
2. **Check parameter reasonableness** (not at extremes)
3. **Test on out-of-sample data** if possible
4. **Start with paper trading** for new configurations

---

## Integration with Warmup System

### Optimizing Warmup Criteria

The optimizer automatically searches for optimal warmup configuration:
- Whether to enable warmup (True/False)
- Observation days (1-5)
- Simulation days (3-10)

### Example: Warmup-Optimized Parameters

```json
{
  "parameters": {
    "enable_warmup": true,
    "warmup_obs_days": 3,      // Optimal: 3 days observation
    "warmup_sim_days": 7,      // Optimal: 7 days simulation
    "max_positions": 6,
    "stop_loss": -0.020,
    "profit_target": 0.045,
    "lambda": 0.993
  }
}
```

**Deployment:**
```bash
./build/sentio_lite live \
    --enable-warmup \
    --warmup-obs-days 3 \
    --warmup-sim-days 7 \
    --max-positions 6 \
    --stop-loss -0.020 \
    --profit-target 0.045 \
    --lambda 0.993
```

---

## Walk-Forward Optimization (Advanced)

For robust validation:

```bash
# Optimize on Sept 1-30
python3 tools/optimize_warmup.py \
    --test-date 2025-10-01 \
    --optimization-days 30 \
    --n-trials 100

# Test on Oct 1-15 (out-of-sample)
python3 tools/optimize_warmup.py \
    --start-date 2025-10-01 \
    --end-date 2025-10-15 \
    --use-best \
    --mode mock

# If results good, deploy to live
```

---

## Performance Tips

### 1. Use Cached Data
Data loading is slow. Keep data files in fast storage (SSD).

### 2. Parallel Trials
Use `--n-jobs 4` on multi-core machines for 4x speedup.

### 3. Prune Bad Trials
Optuna automatically prunes trials that underperform (built-in).

### 4. Resume Optimization
```python
# In optimize_warmup.py, modify to use persistent storage:
study = optuna.create_study(
    storage="sqlite:///optuna.db",  # Persistent
    study_name="sentio_optimization",
    load_if_exists=True  # Resume if exists
)
```

---

## Conclusion

The Optuna optimization system provides:
- ✅ **Automated parameter tuning** (no manual trial-and-error)
- ✅ **Data-driven decisions** (optimize on recent market data)
- ✅ **Production ready** (daily optimization workflow)
- ✅ **Warmup integration** (automatic warmup configuration)

**Recommended Workflow:**
1. Run optimization daily (evening)
2. Review results (check metrics)
3. Deploy if good (morning)
4. Monitor performance (intraday)
5. Iterate weekly (walk-forward validation)

---

**Status:** ✅ Production Ready
**Next Steps:** Test on historical data, validate, deploy to live trading

---

## Quick Reference

```bash
# Daily optimization (production)
python3 tools/optimize_warmup.py --test-date 2025-10-19 --n-trials 100 --mode live

# Fast test (development)
python3 tools/optimize_warmup.py --test-date 2025-10-15 --n-trials 10

# Use saved parameters
python3 tools/optimize_warmup.py --test-date 2025-10-19 --use-best

# Multi-day validation
python3 tools/optimize_warmup.py --start-date 2025-10-15 --end-date 2025-10-18

# Help
python3 tools/optimize_warmup.py --help
```

**Support:** Review `tools/optimize_warmup.py` for full implementation details.
