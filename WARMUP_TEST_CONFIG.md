# Warmup System Testing Configuration

**Date:** 2025-10-18
**Purpose:** Document optimal test configurations for validating warmup system functionality

---

## Data Coverage

**Current Data Range:** April 1 - October 18, 2025 (139 trading days, 54,349 bars)

**Download Command Used:**
```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader
export POLYGON_API_KEY=fE68VnU8xUR7NQFMAM4yl3cULTHbigrb
python3 tools/data_downloader.py TQQQ SQQQ SSO SDS TNA TZA UVXY SVIX SOXS SOXL \
    --start 2025-04-01 --end 2025-10-18 --outdir data/equities
```

**Symbols:** TQQQ, SQQQ, SSO, SDS, TNA, TZA, UVXY, SVIX, SOXS, SOXL

---

## Performance Analysis Across Multiple Periods

### Tested Periods (7-8 day windows)

| Period | Total Return | MRD | Profit Factor | Win Rate | Trades | Assessment |
|--------|-------------|-----|---------------|----------|--------|------------|
| **Apr 14-23** | **-0.95%** | **-0.14%** | **1.18** | 2.5% | 714 | **BEST** |
| Apr 21-30 | -0.80% | -0.10% | 0.75 | 1.7% | 815 | Good PF |
| Apr 7-16 | -2.74% | -0.34% | 0.99 | 1.1% | 816 | Poor |
| May 5-14 | -1.96% | -0.25% | 0.46 | 1.5% | 815 | Poor |
| Jun 23-Jul 2 | -1.82% | -0.23% | 0.42 | 1.2% | 814 | Poor |
| Aug 11-20 | -1.40% | -0.18% | 0.85 | 1.8% | 815 | Moderate |
| Sep 2-11 | -2.49% | -0.31% | 0.65 | 1.6% | 816 | Poor |
| Oct 6-14 | -2.50% | -0.42% | 0.48 | 2.1% | 613 | Poor |

### Key Findings

1. **No Profitable Periods Found**: All tested periods in 2025 show negative returns
2. **Best Performance**: April 14-23 with smallest loss (-0.95%) and highest profit factor (1.18)
3. **Strategy Needs Tuning**: Current parameters not optimized for 2025 market conditions
4. **Consistent Behavior**: Win rates consistently low (1-3%), suggesting high selectivity

---

## Testing Recommendations

### Option 1: Test Warmup REJECTION Logic (Current)

Use any period with default strict criteria to verify warmup correctly REJECTS poor performance:

**Configuration:**
```cpp
warmup.min_sharpe_ratio = 0.3;           // Strict
warmup.max_drawdown = 0.15;              // 15% max
warmup.min_trades = 20;                  // Reasonable
warmup.require_positive_return = true;   // Must be profitable
```

**Test Command:**
```bash
./build/sentio_lite mock --start-date 2025-10-06 --end-date 2025-10-14 \
    --warmup-days 1 --enable-warmup --no-dashboard
```

**Expected Result:** ❌ Warmup criteria not met, extends simulation

---

### Option 2: Test Warmup APPROVAL Logic (For Development/Testing)

Use relaxed criteria to verify warmup can correctly PASS and transition to live:

**Modified Configuration** (in `include/trading/multi_symbol_trader.h:73-76`):
```cpp
warmup.min_sharpe_ratio = -2.0;          // Very lenient (for testing only)
warmup.max_drawdown = 0.30;              // 30% max (lenient)
warmup.min_trades = 20;                  // Keep reasonable
warmup.require_positive_return = false;  // Allow negative for testing
```

**Best Test Period:** April 14-23, 2025

**Test Command:**
```bash
./build/sentio_lite mock --start-date 2025-04-14 --end-date 2025-04-23 \
    --warmup-days 1 --enable-warmup --no-dashboard \
    --warmup-obs-days 2 --warmup-sim-days 5
```

**Expected Result:** ✅ Warmup criteria met, transitions to LIVE

---

### Option 3: Find Optimal Strategy Parameters

Instead of relaxing warmup criteria, tune the trading strategy parameters to achieve profitability:

**Parameters to Adjust:**
```cpp
// In TradingConfig (include/trading/multi_symbol_trader.h)
max_positions = 5;                      // Try more positions
stop_loss_pct = -0.015;                 // Tighter stop (1.5%)
profit_target_pct = 0.03;               // Lower target (3%)

// EWRLS learning rates
lambda_1bar = 0.990;                    // Faster adaptation
lambda_5bar = 0.995;                    // Faster adaptation
lambda_10bar = 0.997;                   // Faster adaptation

// Trade filter
min_bars_to_hold = 3;                   // Shorter holding period
max_bars_to_hold = 30;                  // Exit faster

// Probability thresholds
buy_threshold = 0.60;                   // More selective (was 0.55)
sell_threshold = 0.40;                  // More selective (was 0.45)
```

**Process:**
1. Adjust one parameter at a time
2. Test on April 14-23 (best period)
3. Validate on other periods
4. Iterate until profitable

---

## Recommended Production Configuration

### Go-Live Criteria (After Strategy Optimization)

Once strategy is tuned to profitability:

```cpp
warmup.min_sharpe_ratio = 0.5;           // Higher bar (was 0.3)
warmup.max_drawdown = 0.10;              // Tighter (10% max)
warmup.min_trades = 30;                  // More data
warmup.require_positive_return = true;   // Must be profitable
```

### Warmup Duration

```cpp
warmup.observation_days = 3;             // More learning (was 2)
warmup.simulation_days = 10;             // Longer validation (was 5)
```

**Rationale:**
- 3 days observation = ~1200 bars of learning
- 10 days simulation = ~3900 bars of validation
- Total warmup = 13 days before live trading

---

## Testing Workflow for Warmup System Validation

### Phase 1: Test Rejection Logic ✅
```bash
# Use current data and strict criteria
./build/sentio_lite mock --start-date 2025-10-06 --end-date 2025-10-14 \
    --warmup-days 1 --enable-warmup --no-dashboard

# Verify: Should see "❌ Warmup criteria not met"
```

### Phase 2: Test Approval Logic (Using Relaxed Criteria)
```bash
# Modify warmup.min_sharpe_ratio to -2.0 in multi_symbol_trader.h
# Rebuild: cmake --build build

# Test on best performing period
./build/sentio_lite mock --start-date 2025-04-14 --end-date 2025-04-23 \
    --warmup-days 1 --enable-warmup --no-dashboard

# Verify: Should see "✅ Warmup complete - transitioning to LIVE trading"
```

### Phase 3: Test State Preservation
```bash
# Run warmup and verify predictor state is preserved
# Check that trade history carries forward
# Validate equity continuity across phases
```

### Phase 4: Optimize Strategy (Future Work)
```bash
# Systematically test parameter combinations
# Find configuration that achieves Sharpe > 0.3
# Validate on out-of-sample periods
# Document optimal parameters
```

---

## Current Warmup System Status

### Working Features ✅
- [x] Observation phase (learning without trading)
- [x] Simulation phase (paper trading with metrics)
- [x] Automatic phase transitions
- [x] Go-live criteria evaluation
- [x] Rejection logic when criteria not met
- [x] Sharpe ratio calculation
- [x] Drawdown tracking
- [x] Trade count validation
- [x] Profitability check

### Pending Validation ⏳
- [ ] Approval logic (requires relaxed criteria or optimized strategy)
- [ ] State preservation verification
- [ ] Extended period testing (20+ days)
- [ ] Different market regime testing
- [ ] Production deployment validation

---

## Next Steps

### Immediate (Testing Focus)
1. **Modify go-live criteria** to -2.0 Sharpe for testing approval logic
2. **Test on April 14-23** to verify warmup passes and transitions
3. **Document results** in warmup validation report
4. **Restore strict criteria** after testing

### Short-Term (Strategy Optimization)
1. **Parameter sweep** on April period to find profitable config
2. **Validate** on multiple out-of-sample periods
3. **Test warmup** with newly profitable strategy
4. **Document** optimal production parameters

### Long-Term (Production Readiness)
1. **Walk-forward analysis** across all available data
2. **Regime detection** to adapt parameters by market conditions
3. **Ensemble approach** with multiple parameter sets
4. **Live paper trading** with warmup system
5. **Risk management** integration with position sizing

---

## Testing Commands Summary

### Quick Tests (Current Data)

```bash
# Test warmup rejection (current criteria)
./build/sentio_lite mock --start-date 2025-10-06 --end-date 2025-10-14 \
    --warmup-days 1 --enable-warmup --no-dashboard

# Test best performing period (no warmup)
./build/sentio_lite mock --start-date 2025-04-14 --end-date 2025-04-23 \
    --warmup-days 1 --no-dashboard

# Test with warmup approval (after criteria relaxation)
./build/sentio_lite mock --start-date 2025-04-14 --end-date 2025-04-23 \
    --warmup-days 1 --enable-warmup --no-dashboard
```

### Parameter Optimization Template

```bash
# Test different max_positions
for N in 3 5 7 10; do
    echo "Testing max_positions=$N"
    ./build/sentio_lite mock --start-date 2025-04-14 --end-date 2025-04-23 \
        --warmup-days 1 --max-positions $N --no-dashboard \
        | grep "Total Return"
done

# Test different lambda values
for L in 0.980 0.985 0.990 0.995; do
    echo "Testing lambda=$L"
    ./build/sentio_lite mock --start-date 2025-04-14 --end-date 2025-04-23 \
        --warmup-days 1 --lambda $L --no-dashboard \
        | grep "Total Return"
done
```

---

## Conclusion

**For Warmup System Testing:**
- Use **April 14-23, 2025** as primary test period (best performance)
- Use **relaxed criteria** (-2.0 Sharpe) temporarily to test approval logic
- Restore **strict criteria** (0.3 Sharpe) for production

**For Strategy Optimization:**
- Current strategy shows consistent losses in 2025
- Needs parameter tuning to achieve profitability
- Once profitable, warmup system will work with default strict criteria

**Status:** ✅ Warmup system implementation complete, ready for validation testing
**Data:** ✅ 139 days of historical data available (Apr-Oct 2025)
**Next:** Modify criteria to test approval logic, then optimize strategy parameters
