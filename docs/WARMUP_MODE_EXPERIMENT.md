# Warmup Mode Comparison Experiment

## Objective
Compare the effectiveness of **prev-day warmup** vs **intraday warmup** for SIGOR strategy optimization.

## Hypothesis
Which warmup mode produces better-optimized parameters?

### Prev-Day Warmup (Historical Approach)
- **Source**: Last N bars of previous trading day
- **Example**: For Oct 24 test, use bars 292-391 of Oct 23 (last hour)
- **Pros**:
  - Indicators are "mature" and fully warmed up before trading begins
  - Reflects overnight gap/reset conditions
  - Natural continuation from previous day's market state
- **Cons**:
  - May not capture opening dynamics of test day
  - Previous day's patterns may not predict current day

### Intraday Warmup (Fresh Start Approach)
- **Source**: First N bars of test day itself
- **Example**: For Oct 24 test, use bars 1-60 of Oct 24 (first hour)
- **Pros**:
  - Indicators adapt to current day's opening conditions
  - Captures immediate intraday dynamics
  - No gap effects from previous day
- **Cons**:
  - Sacrifices early trading opportunities
  - Indicators start "cold" and build up during warmup period

## Experimental Setup

### Parameters
- **End Date**: 2025-10-24
- **Trials**: 200 (Optuna TPE sampler)
- **Warmup Bars**: 60 (1 hour, consistent across both modes)
- **Evaluation Set**: 5 most recent trading days (Oct 18-24)
- **Validation Set**: 10 prior trading days
- **Overfitting Threshold**: 20%

### Optimized Parameters (17 total)
**Detector Weights (8)**:
- w_boll, w_rsi, w_mom, w_vwap, w_orb, w_ofi, w_vol, w_awr

**Window Sizes (9)**:
- win_boll, win_rsi, win_mom, win_vwap, orb_opening_bars, vol_window
- win_awr_williams, win_awr_rsi, win_awr_bb

### Test Protocol
1. Run 200-trial Optuna optimization with **prev-day warmup**
2. Run 200-trial Optuna optimization with **intraday warmup**
3. Compare best evaluation MRD and validation MRD
4. Analyze parameter differences between winners

## Execution

```bash
# Run the comparison experiment (takes ~2-4 hours for 400 total trials)
./scripts/compare_warmup_modes.sh
```

## Implementation Details

### Python Script Changes
**File**: `scripts/optimize_sigor_combined.py`

Added arguments:
```python
--warmup-mode {prevday,intraday}  # Warmup source selection
--warmup-bars N                   # Number of warmup bars (default: 60)
```

Lines modified: 30-37, 48, 64-67, 352-355, 376-377

### C++ Engine Support
**File**: `src/main.cpp`

Existing flags:
```bash
--warmup-bars N        # Number of warmup bars
--intraday-warmup      # Flag to enable intraday warmup mode
```

Lines: 52, 199-200, 243-244, 628-644, 718-720

## Expected Insights

### If Prev-Day Wins:
- SIGOR benefits from mature indicators at market open
- Overnight state persistence is valuable
- Previous day's patterns have predictive power

### If Intraday Wins:
- SIGOR performs better adapting to fresh market conditions
- Opening hour dynamics are more important than pre-warmed indicators
- Each day should be treated as independent

### If Results Are Similar:
- Warmup mode is not a critical factor for SIGOR
- The 8 detector ensemble is robust to warmup source
- Focus optimization efforts elsewhere

## Results

### Prev-Day Warmup
- **Best Eval MRD**: TBD%
- **Best Val MRD**: TBD%
- **Best Parameters**: See `results/warmup_comparison/prevday_warmup_results.json`

### Intraday Warmup
- **Best Eval MRD**: TBD%
- **Best Val MRD**: TBD%
- **Best Parameters**: See `results/warmup_comparison/intraday_warmup_results.json`

### Winner
TBD after experiment completion

## Analysis Questions

1. **Performance Gap**: How large is the MRD difference between modes?
2. **Parameter Divergence**: Do the two modes favor different detector weights?
3. **Window Preferences**: Do warmup modes affect optimal window sizes?
4. **Validation Stability**: Which mode shows better eval/val consistency?
5. **Trade Count**: Does warmup mode affect trading frequency?

## Recommendations

TBD based on experimental results

## Files Generated

```
results/warmup_comparison/
├── prevday_warmup_results.json          # Full Optuna results (prev-day)
├── intraday_warmup_results.json         # Full Optuna results (intraday)
├── prevday_warmup_200trials.log         # Console output (prev-day)
└── intraday_warmup_200trials.log        # Console output (intraday)
```

## Historical Context

Previously, all SIGOR optimizations used **natural intraday warmup** (first N bars of test day). This experiment is the first systematic comparison to evaluate if prev-day warmup offers advantages for the 8-detector ensemble strategy.

## Next Steps

After results:
1. Update production config with winning mode
2. Document warmup mode choice in `config/sigor_params.json`
3. Consider hybrid approaches if results are inconclusive
4. Test on additional date ranges for robustness
