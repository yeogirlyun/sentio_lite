# Production Winner - 2000 Trial Optimization

**Generated:** Sat Oct 25 05:51:33 EDT 2025
**For Deployment:** Monday Market Open (2025-10-27)

## Performance Metrics

- **Best Trial:** 868
- **Evaluation MRD:** +0.384%
- **Validation MRD:** +0.327%
- **Degradation:** +14.8% (PASS - within 20% threshold)
- **Verdict:** ACCEPT

## Optimization Details

- **Total Trials:** 2000
- **Passed:** 1,609 (80.5%)
- **Failed:** 391 (19.6%)
- **Runtime:** 25 minutes 34 seconds
- **End Date:** 2025-10-24

## Data Sets

- **Evaluation Set:** 5 most recent trading days (Oct 20-24, 2025)
- **Validation Set:** 10 prior trading days (Oct 6-17, 2025)

## Best Parameters

### Detector Weights
- w_boll: 0.397 (Bollinger Bands)
- w_rsi: 0.213 (RSI)
- w_mom: 0.137 (Momentum)
- w_vwap: 0.556 (VWAP)
- w_orb: 1.861 (Opening Range Breakout) ⭐
- w_ofi: 0.530 (Order Flow Imbalance)
- w_vol: 1.910 (Volume) ⭐

### Window Sizes
- win_boll: 12 bars
- win_rsi: 7 bars
- win_mom: 3 bars
- win_vwap: 10 bars
- orb_opening_bars: 49 bars ⭐
- vol_window: 10 bars

## Key Insights

1. **ORB and Volume Dominate:** Highest weights suggest opening range breakout and volume signals are most predictive
2. **Short Windows Preferred:** Most indicators use 10 bars or less
3. **Strong Generalization:** Only 14.8% degradation shows excellent out-of-sample performance
4. **60% Improvement:** 0.384% vs baseline 0.240%

## Deployment Instructions

1. This config is already applied to `config/sigor_params.json`
2. Rebuild binary before Monday: `cmake --build build -j`
3. Launch for live trading: `./scripts/launch_sigor_live.sh`

## Files Included

- `sigor_params.json` - Production-ready parameters
- `optimization_results.json` - Full 2000-trial results
- `optimization.log` - Complete optimization log
- `README.md` - This file
