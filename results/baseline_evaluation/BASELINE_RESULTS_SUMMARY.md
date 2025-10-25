# SIGOR Baseline Performance Summary (Oct 20-24, 2025)

## Test Results

| Date | MRD % | Win Rate % | Total Trades | Status |
|------|-------|------------|--------------|--------|
| 2025-10-20 | N/A | N/A | 0 | ❌ Insufficient data (only 391 bars, need 782) |
| 2025-10-21 | -0.67 | 39.5 | 195 | ✅ Complete |
| 2025-10-22 | +0.39 | 43.3 | 203 | ✅ Complete |
| 2025-10-23 | +1.26 | 46.1 | 204 | ✅ Complete |
| 2025-10-24 | -1.54 | 39.2 | 204 | ✅ Complete |

## Aggregate Metrics (4 valid days)

- **Average MRD**: -0.14% per day
- **Total P&L**: -0.56% (4 days)
- **Average Win Rate**: 42.0%
- **Average Trades/Day**: 201.5

## Configuration Used

Loaded from `config/sigor_params.json`:

### Fusion Parameter
- k (sharpness): 1.8

### Detector Weights
- Bollinger Bands: 0.7
- RSI: 1.8
- Momentum: 0.8
- VWAP: 1.6
- ORB: 0.1
- OFI: 1.1
- Volume: 0.1

### Window Parameters
- Bollinger Window: 26
- RSI Window: 14
- Momentum Window: 13
- VWAP Window: 20
- ORB Opening Bars: 39
- Volume Window: 28
- Warmup Bars: 50

## Issues Identified

1. **Oct 20 Data Issue**: First day needs prior day data for warmup - only 391 bars available
2. **Negative Overall Performance**: -0.14% avg MRD is below breakeven
3. **Low Win Rate**: 42.0% is below the 50% threshold for profitability
4. **High Trade Frequency**: ~200 trades/day may indicate overtrading

## Next Steps

### Current Request vs. Technical Limitation

**User Request**:
- Run optuna optimization (200 trials) on baseline SIGOR
- Find best configuration
- Save as `baseline_best_config.json`

**Technical Challenge**:
The `sentio_lite` binary in mock mode does NOT support dynamic config overrides via command line. Parameters are loaded from fixed config files (`config/sigor_params.json`, `config/trading_params.json`).

### Proposed Approaches

#### Option 1: Manual Parameter Sweep (Simpler)
Test 5-10 manually selected parameter combinations:
- Adjust fusion k: [1.5, 1.8, 2.0, 2.2]
- Adjust detector weights (especially high-performing ones)
- Test each config manually, track results
- Select best performer

#### Option 2: Add Config Override Support to C++ (More Work)
Modify `src/main.cpp` and strategy code to:
- Accept `--config` parameter
- Override params from JSON file
- Then run optuna optimization as requested

#### Option 3: Use Existing Optuna Infrastructure (If Available)
Check if EWRLS optuna code can be adapted for SIGOR parameter tuning

### Recommended Immediate Action

Given current limitations, recommend:

1. **Keep baseline config as-is** (save current `sigor_params.json` as `baseline_best_config.json`)
2. **Proceed with VWAP Bands integration** (primary goal)
3. **Compare VWAP-enhanced vs baseline** on same 5-day period
4. **If VWAP shows improvement, then invest time in optuna infrastructure**

This approach prioritizes detector enhancement over parameter optimization, which aligns with the "simple rules, strong edges" philosophy.

---

**Generated**: 2025-10-24
**Test Period**: Oct 21-24, 2025 (4 valid days)
**Configuration**: config/sigor_params.json (current baseline)
