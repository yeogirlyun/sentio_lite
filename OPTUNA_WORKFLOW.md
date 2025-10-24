# Sentio Lite - Optuna Parameter Optimization Workflow

## Overview

Sentio Lite uses a robust 5-day validation strategy to find optimal trading parameters every day before market open.

## Philosophy

**Daily Re-optimization**: Every morning before market open, run Optuna to find the best parameters for TODAY's trading session based on recent performance.

## Workflow

### 1. Data Download (Night Before or Early Morning)

Ensure you have at least 40+ days of data for all symbols:

```bash
# Download latest data (run nightly or early morning)
python3 tools/data_downloader.py TQQQ SQQQ TNA TZA UVXY SVIX SOXS SOXL \
  --start 2025-09-01 \
  --end 2025-10-23 \
  --outdir data
```

### 2. Run 5-Day Optuna Search (Before Market Open)

**Time Required**: ~2-4 hours for 200 trials × 5 days = 1,000 backtests

```bash
# Run 5-day optimization (default: 200 trials, 5 sim days)
python3 tools/optuna_5day_search.py --end-date 2025-10-23

# Quick test (50 trials for faster results)
python3 tools/optuna_5day_search.py --end-date 2025-10-23 --trials 50

# Custom simulation period
python3 tools/optuna_5day_search.py --end-date 2025-10-23 --sim-days 10 --trials 100
```

### 3. What Optuna Does

For each of 200 trial configurations:

1. **Test on 5 Most Recent Trading Days**
   - Example: Oct 23, Oct 22, Oct 21, Oct 17, Oct 16
   - Automatically skips weekends

2. **Each Test Day Structure**:
   ```
   [1 warmup day] + [5 sim days] + [1 test day] = 7 days required
   ```
   - Warmup: Learn patterns (no trading)
   - Simulation: Practice trading (metrics ignored)
   - Test: Actual test day (ONLY these metrics count)

3. **Evaluation Metric**: Average MRD across all 5 test days

### 4. Parameter Search Space

Optuna optimizes 16 parameters:

| Parameter | Range | Step |
|-----------|-------|------|
| max_positions | 2-5 | 1 |
| stop_loss_pct | -3% to -1% | 0.2% |
| profit_target_pct | 2% to 8% | 0.5% |
| lambda_1bar | 0.95-0.995 | 0.005 |
| lambda_5bar | 0.98-0.999 | 0.001 |
| lambda_10bar | 0.99-0.9999 | 0.0001 |
| min_prediction_for_entry | 0.0-1% | 0.1% |
| min_bars_to_hold | 10-40 bars | 5 |
| lookback_window | 20-100 bars | 10 |
| win_multiplier | 1.0-1.5 | 0.1 |
| loss_multiplier | 0.5-1.0 | 0.1 |
| rotation_strength_delta | 0.1%-2% | 0.1% |
| min_rank_strength | 0.01%-0.5% | 0.01% |

### 5. Configuration Selection Strategy

From 200 trials, select top 5 configurations by average MRD.

From these top 5:

1. **Top MRD**: Configuration with highest average MRD
   - Pure performance focus

2. **Best MRD**: Configuration with best risk profile
   - 50% weight: Win rate (target ~55%)
   - 50% weight: Trade count (more trades = more confidence)

3. **Balanced Config**: Middle value between Top and Best
   - For each parameter: `balanced = (top_value + best_value) / 2`
   - This is the configuration used for trading

### 6. Results

Optuna saves two files:

1. **`config/trading_params.json`** - Balanced configuration for trading
   ```json
   {
     "description": "Sentio Lite Trading Parameters - Optuna 5-Day Optimized",
     "last_updated": "2025-10-23 08:30:00",
     "optimization_date": "2025-10-23",
     "optimization_summary": {
       "top_mrd": { "avg_mrd": 0.15, ... },
       "best_mrd": { "avg_mrd": 0.12, ... }
     },
     "parameters": { ... 16 parameters ... }
   }
   ```

2. **`logs/optuna_5day_results.json`** - Detailed results
   - Top 5 configurations with full metrics
   - All daily results for each configuration

### 7. Using the Optimized Config

**Dashboard Display**:
- Every dashboard shows: `Config File: /path/to/config/trading_params.json`
- Parameters displayed are from the actual runtime
- Compare dashboard results with config file to verify

**Future Enhancement** (TODO):
- Add `--config` flag to sentio_lite to load parameters from file
- Current version uses hardcoded defaults + CLI overrides

### 8. Daily Routine (Production)

```bash
# Night before (e.g., Sunday night or weekday 11 PM)
python3 tools/data_downloader.py TQQQ SQQQ TNA TZA UVXY SVIX SOXS SOXL \
  --start 2025-09-01 \
  --end $(date +%Y-%m-%d) \
  --outdir data

# Early morning (6-8 AM before market open at 9:30 AM)
python3 tools/optuna_5day_search.py --end-date $(date +%Y-%m-%d) --trials 200

# Review results
cat config/trading_params.json

# Use optimized parameters for live trading (manual or automated)
# TODO: Implement --config flag to load from file
```

### 9. Monitoring Performance

After running Optuna, review:

1. **Expected MRD**: Average of top_mrd and best_mrd
2. **Win Rate**: Should be 50-60% for balanced config
3. **Trade Count**: Should be 50-100 trades per day
4. **Risk Profile**: Balance between returns and consistency

### 10. Key Insights

✅ **Why 5 days?**
- Enough data to validate robustness
- Recent enough to capture current market regime
- Not too long (avoids overfitting to old data)

✅ **Why balanced config?**
- Top MRD might be too aggressive
- Best MRD might be too conservative
- Balanced provides best risk/reward trade-off

✅ **Why 200 trials?**
- Good coverage of parameter space
- ~2-4 hours is reasonable for daily optimization
- Can reduce to 50-100 for quick tests

✅ **Why average MRD?**
- Single-day metric (consistent with daily re-optimization)
- Accounts for daily variation
- More robust than max/min performance

## Example Output

```
================================================================================
TOP 5 CONFIGURATIONS BY MRD:
================================================================================

#1 - Trial 142
  MRD: +0.2345%
  Win Rate: 52.34%
  Avg Trades: 87.2
  Profit Factor: 1.15

#2 - Trial 89
  MRD: +0.2112%
  Win Rate: 58.67%
  Avg Trades: 92.8
  Profit Factor: 1.23

...

================================================================================
SELECTED CONFIGURATIONS:
================================================================================

1️⃣  TOP MRD (Trial 142): +0.2345%
2️⃣  BEST MRD (Trial 89): +0.2112%
    (Best risk profile: WR=58.67%, Trades=93)

3️⃣  BALANCED CONFIG (middle of Top and Best):
================================================================================

✅ Balanced configuration saved to: config/trading_params.json
✅ Detailed results saved to: logs/optuna_5day_results.json

================================================================================
OPTIMIZATION COMPLETE!
================================================================================
✅ Use config/trading_params.json for tomorrow's market open
✅ Expected MRD: +0.2229%
```

## Next Steps

1. **Implement Config Loading**: Add `--config` flag to load parameters from JSON
2. **Automated Execution**: Cron job to run optimization every morning
3. **Performance Tracking**: Track actual vs expected MRD over time
4. **Parameter Evolution**: Analyze how optimal parameters change over time
