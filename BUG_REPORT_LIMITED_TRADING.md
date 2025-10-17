# BUG REPORT: Severely Limited Trading Activity

**Date**: 2025-10-17
**Severity**: CRITICAL
**Status**: Open

---

## Problem Summary

The trading system exhibits severely limited trading activity with only 3 trades executed over 394 trading bars (full trading day), with all entries occurring in a narrow morning window and exits happening immediately after the minimum 2-bar holding period. This does not represent a functional trading algorithm.

---

## Observed Behavior

### Test Configuration
- **Test Date**: 2025-10-16
- **Symbols**: 10 (TQQQ, SQQQ, SSO, SDS, TNA, TZA, UVXY, SVIX, SOXS, SOXL)
- **Trading Period**: 394 bars (9:30 AM - 4:00 PM, after 1173 bar warmup)
- **Max Positions**: 3

### Actual Results
```
Total Trades:       3 (only 3 entries + 3 exits)
Trading Period:     394 bars
Trade Frequency:    0.76% (3/394)
Win Rate:           33.3% (1 win, 2 losses)
Total Return:       -0.42%
```

### Trade Timeline
```
Bar 1171: Entry SOXL at $41.62 (1-bar: 0.99%, 5-bar: 4.13%, conf: 67%)
Bar 1171: Entry SOXS at $3.92  (1-bar: -0.99%, 5-bar: -3.99%, conf: 67%)
Bar ~1173: Exit SOXL (held ~2 bars) - LOSS
Bar ~1173: Exit SOXS (held ~2 bars) - WIN

Bar 1319: Entry UVXY at $13.37 (1-bar: 0.30%, 5-bar: 3.54%, conf: 64%)
Bar ~1321: Exit UVXY (held ~2 bars) - LOSS

Bars 1322-1564: NO TRADING ACTIVITY (243 bars with zero trades)
```

### Critical Observations
1. **Extremely Limited Entry Window**: All 3 trades occur within first 148 bars (bars 1171-1319)
2. **No Mid-Day or Afternoon Trading**: 243 bars (62% of trading day) with ZERO activity
3. **Minimum Hold Period Exits**: All positions exited after exactly 2 bars (minimum allowed)
4. **No Re-entries**: System never re-enters positions after initial exits
5. **Strong Predictions Ignored**: Debug logs show predictions available but not acted upon

---

## Expected Behavior

For a 394-bar trading day with 10 symbols and 3 max positions:
- **Expected Trades**: 50-100 per day (per original design spec)
- **Expected Activity**: Continuous evaluation and rotation throughout the day
- **Expected Hold Times**: Variable (2-60 bars), not always minimum
- **Expected Re-entries**: System should re-enter when signals improve

---

## Root Cause Analysis

### Hypothesis 1: Overly Restrictive Entry Criteria ⚠️ HIGH PRIORITY

**Multi-Horizon Agreement Requirement** (`multi_horizon_predictor.h:89-91`):
```cpp
bool should_enter() const {
    // Require horizons to agree
    if (!horizons_agree()) {
        return false;  // ← BLOCKS ENTRY if 1/5/10 bar don't all agree
    }

    // Require 5-bar prediction exceeds threshold
    if (std::abs(pred_5bar.prediction) < min_prediction) {
        return false;
    }

    // Require strong 5-bar to 1-bar ratio
    if (std::abs(pred_5bar.prediction) < std::abs(pred_1bar.prediction) * 3.0) {
        return false;  // ← BLOCKS if 5-bar not 3x better than 1-bar
    }
}
```

**Issue**: Requiring ALL THREE horizons (1-bar, 5-bar, 10-bar) to agree on direction AND requiring 5-bar prediction to be 3x stronger than 1-bar is extremely restrictive. Market conditions rarely produce such perfect alignment.

**Evidence from Logs**:
```
[DEBUG] Top 5-bar predictions at bar 1272:
  SOXL: 0.004359 (5-bar)   ← Good prediction but no entry
  UVXY: -0.001995 (5-bar)

[DEBUG] Top 5-bar predictions at bar 1373:
  UVXY: 0.004217 (5-bar)   ← Good prediction but no entry
  SOXS: -0.002691 (5-bar)

[DEBUG] Top 5-bar predictions at bar 1473:
  TNA: -0.005078 (5-bar)   ← Strong prediction but no entry
  TZA: 0.004295 (5-bar)
```

### Hypothesis 2: Premature Exits (Always Minimum Hold) ⚠️ HIGH PRIORITY

**Trade Filter Exit Logic** (`trade_filter.cpp`):
All exits happening at exactly 2 bars suggests the exit criteria is too aggressive.

**Possible Causes**:
- `exit_signal_reversed_threshold = -0.0005` too sensitive
- `exit_confidence_threshold = 0.4` too high
- `profit_target_multiple = 2.0` unrealistic for short hold periods
- Trade filter not properly tracking bars held

### Hypothesis 3: Re-Entry Cooldown Too Long ⚠️ MEDIUM PRIORITY

**Re-Entry Constraint** (`trade_filter.h:37`):
```cpp
int min_bars_between_entries = 5;  // Cooldown after exit
```

After exiting SOXL/SOXS at bar ~1173, system cannot re-enter until bar ~1178. This prevents rapid position rotation.

### Hypothesis 4: Lambda Values Too Conservative ⚠️ MEDIUM PRIORITY

**EWRLS Learning Rates** (`multi_symbol_trader.h:55-58`):
```cpp
lambda_1bar = 0.99   // Forgets only 1% per bar
lambda_5bar = 0.995  // Forgets only 0.5% per bar
lambda_10bar = 0.998 // Forgets only 0.2% per bar
```

These very high lambda values mean the predictor has a VERY long memory and adapts VERY slowly to new information. After the initial period, predictions may become stale.

### Hypothesis 5: Minimum Prediction Threshold Too High ⚠️ LOW PRIORITY

**Entry Threshold** (`multi_symbol_trader.h:64`):
```cpp
min_prediction_for_entry = 0.002;  // 20 bps (0.2%)
```

While 20 bps seems reasonable, combined with other filters it may be too restrictive.

---

## Debug Evidence

### Log Analysis

**Warmup Period (Bars 1-1170)**:
- ✅ Feature extraction working
- ✅ Predictions being made
- ✅ EWRLS updating properly

**Early Trading (Bars 1171-1319)**:
- ✅ 3 entries executed
- ✅ Strong predictions (4.13%, 3.99%, 3.54%)
- ✅ Good confidence (64-67%)
- ⚠️ All exits at minimum 2-bar hold

**Late Trading (Bars 1320-1564)**:
- ⚠️ Zero trades despite predictions being made
- ⚠️ Debug logs show predictions (0.43%, 0.42%, 0.51%) but no entries
- ⚠️ No position changes for 243 bars (62% of day)

### Prediction Debug Output
```
[DEBUG] Top 5-bar predictions at bar 1171:
  SOXL: 0.005678 (5-bar)  ← ENTERED ✓
  SOXS: -0.005372 (5-bar) ← ENTERED ✓
  TQQQ: 0.004437 (5-bar)  ← NOT ENTERED (why?)

[DEBUG] Top 5-bar predictions at bar 1272:
  SOXL: 0.004359 (5-bar)  ← NOT ENTERED (why?)
  UVXY: -0.001995 (5-bar)
  SVIX: 0.001623 (5-bar)

[DEBUG] Top 5-bar predictions at bar 1473:
  TNA: -0.005078 (5-bar)  ← STRONG SIGNAL, NOT ENTERED (why?)
  TZA: 0.004295 (5-bar)   ← STRONG SIGNAL, NOT ENTERED (why?)
  SOXL: -0.003817 (5-bar)
```

---

## Impact Assessment

### Performance Impact
- **Trade Frequency**: 0.76% vs target 12-25% (50-100 trades/day)
- **Capital Utilization**: Poor - most of capital idle most of the day
- **Opportunity Cost**: Missing 95%+ of potential trading opportunities
- **Risk Management**: Cannot manage risk if not trading

### System Viability
❌ **System is NOT VIABLE in current state**
- Fails to meet design specification (50-100 trades/day)
- Does not rotate positions as intended
- Exits all positions immediately at minimum hold
- Leaves capital idle for majority of trading day

---

## Recommended Investigation Steps

### Priority 1: Entry Criteria Analysis
1. **Instrument multi-horizon agreement check**
   - Log when `horizons_agree()` returns false
   - Count how often 1/5/10 bar predictions disagree
   - Consider relaxing requirement (maybe 2 of 3 agree?)

2. **Instrument 5-bar/1-bar ratio check**
   - Log when `pred_5bar / pred_1bar < 3.0` blocks entry
   - This 3x requirement may be too strict
   - Consider reducing to 2x or 1.5x

3. **Add detailed entry rejection logging**
   ```cpp
   if (!can_enter_position()) {
       std::cout << "[ENTRY BLOCKED] " << symbol
                << " | Reason: " << get_block_reason() << "\n";
   }
   ```

### Priority 2: Exit Logic Investigation
1. **Add exit reason tracking**
   - Why are ALL exits happening at exactly 2 bars?
   - Is trade filter incorrectly signaling exits?
   - Are positions being force-rotated by make_trades()?

2. **Verify trade filter exit logic**
   - Check `should_exit_position()` implementation
   - Verify bars_held counting
   - Test exit thresholds

3. **Add detailed exit logging**
   ```cpp
   std::cout << "[EXIT] " << symbol
            << " | Bars held: " << bars_held
            << " | Reason: " << exit_reason
            << " | Current pred: " << current_prediction
            << " | Entry pred: " << entry_prediction << "\n";
   ```

### Priority 3: Re-Entry Analysis
1. **Track re-entry attempts**
   - Log when cooldown prevents entry
   - Consider reducing `min_bars_between_entries` from 5 to 2-3

2. **Verify position rotation logic**
   - System should rotate out of weak positions into strong ones
   - Check if rotation is being blocked

### Priority 4: Parameter Tuning
1. **Reduce lambda values** (faster adaptation)
   ```cpp
   lambda_1bar = 0.98   (was 0.99)
   lambda_5bar = 0.99   (was 0.995)
   lambda_10bar = 0.995 (was 0.998)
   ```

2. **Relax entry criteria**
   - Reduce 5-bar/1-bar ratio from 3.0 to 2.0
   - Consider allowing entry when 2 of 3 horizons agree
   - Reduce min_prediction from 0.002 to 0.0015

3. **Adjust exit thresholds**
   - Review all exit threshold parameters
   - Ensure positions can be held longer than minimum

---

## Reference: Key Modules & File Paths

### Trading Engine Core
- **`include/trading/multi_symbol_trader.h`** (Lines 28-67)
  - TradingConfig structure with all parameters
  - Main trader class definition

- **`src/trading/multi_symbol_trader.cpp`**
  - Line 46-226: `on_bar()` - Main trading loop
  - Line 228-327: `make_trades()` - Entry decision logic
  - Line 329-402: `update_positions()` - Exit decision logic

### Multi-Horizon Prediction
- **`include/predictor/multi_horizon_predictor.h`**
  - Lines 49-115: `MultiHorizonPrediction` structure
  - Lines 76-100: `should_enter()` - Critical entry criteria
  - Lines 120-144: `Config` - Lambda parameters

- **`src/predictor/multi_horizon_predictor.cpp`**
  - Prediction generation and quality assessment
  - Horizon agreement logic
  - Uncertainty tracking

### Trade Filter (Frequency & Holding Management)
- **`include/trading/trade_filter.h`**
  - Lines 30-68: `Config` - All filtering parameters
  - Lines 102-105: `can_enter_position()` - Entry validation
  - Lines 114-118: `should_exit_position()` - Exit decision

- **`src/trading/trade_filter.cpp`**
  - Entry filtering implementation
  - Exit criteria evaluation (7-tier logic)
  - Frequency limit enforcement
  - Holding period tracking

### Feature Extraction & Prediction
- **`include/predictor/feature_extractor.h`**
  - 33-feature extraction (8 time + 25 technical)

- **`src/predictor/feature_extractor.cpp`**
  - Feature calculation implementation

- **`include/predictor/ewrls_predictor.h`**
  - EWRLS with numerical stability
  - Condition number monitoring

### Configuration
- **`config/symbols.conf`**
  - Symbol list (10 symbols)

- **`include/trading/multi_symbol_trader.h`** (Lines 53-66)
  - Default configuration values
  - Horizon config defaults
  - Trade filter defaults

### Data & Utilities
- **`src/utils/data_loader.cpp`**
  - Binary/CSV data loading

- **`include/utils/results_exporter.h`**
  - Results JSON export
  - Trade log export (trades.jsonl)

### Main Entry Point
- **`src/main.cpp`**
  - Lines 196-226: `generate_dashboard()` - Dashboard integration
  - Lines 245-615: `run_mock_mode()` - Test execution
  - Configuration parsing and setup

---

## Testing Plan

### Test 1: Entry Criteria Relaxation
```cpp
// Modify multi_horizon_predictor.h should_enter()
// Comment out horizon agreement requirement temporarily
bool should_enter() const {
    // if (!horizons_agree()) return false;  // ← DISABLED FOR TEST
    if (std::abs(pred_5bar.prediction) < min_prediction) return false;
    // if (5bar/1bar ratio check) return false;  // ← DISABLED FOR TEST
    return true;
}
```
**Expected**: More entries throughout the day

### Test 2: Reduce Lambda Values
```cpp
// In multi_symbol_trader.h TradingConfig constructor
horizon_config.lambda_1bar = 0.98;   // was 0.99
horizon_config.lambda_5bar = 0.99;   // was 0.995
horizon_config.lambda_10bar = 0.995; // was 0.998
```
**Expected**: Faster adaptation, more responsive predictions

### Test 3: Extended Holding
```cpp
// In trade_filter.h Config constructor
min_bars_to_hold = 5;  // was 2
min_bars_between_entries = 2;  // was 5
```
**Expected**: Longer position holds, faster re-entry

### Test 4: Verbose Logging
Add comprehensive logging to track:
- Entry rejections with reasons
- Exit triggers with details
- Prediction quality over time
- Trade filter state

---

## Success Criteria

A successful fix should produce:
- ✅ **Trade Frequency**: 50-100 trades per day (12-25% of bars)
- ✅ **Distribution**: Trades throughout the day, not just morning
- ✅ **Variable Holds**: Range of 2-60 bars, not always minimum
- ✅ **Re-entries**: System should re-enter positions when signals improve
- ✅ **Capital Utilization**: Positions active most of the time
- ✅ **Rotation**: Clear position rotation based on signal strength

---

## Notes

### Why This Matters
A trading system that:
1. Only trades in a narrow morning window
2. Exits all positions immediately at minimum hold
3. Stays idle for 62% of the trading day
4. Achieves 0.76% trade frequency vs 12-25% target

...is fundamentally broken and cannot be used for live trading.

### Next Steps
1. **Immediate**: Add verbose logging to identify exact blocking conditions
2. **Short-term**: Implement Test 1-3 above to isolate issues
3. **Medium-term**: Systematic parameter tuning based on findings
4. **Long-term**: Consider adaptive thresholds that adjust to market conditions

---

**Report Generated**: 2025-10-17
**System Version**: Sentio Lite v1.0.0 (Multi-Horizon with Trade Filter)
**Test Configuration**: config/symbols.conf (10 symbols)
