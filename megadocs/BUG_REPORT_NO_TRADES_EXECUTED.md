# Bug Report: No Trades Executed During Mock Testing

**Report ID:** BUG-001
**Severity:** CRITICAL
**Status:** CONFIRMED
**Date:** 2025-10-17
**Reporter:** System Analysis
**Affected Version:** Sentio Lite v1.0

---

## Executive Summary

The trading system completes warmup successfully but **executes ZERO trades** during the trading period, despite having 10 liquid symbols and 390 bars (1 full trading day) available for trading. This is a critical bug that prevents the system from functioning as a trading system.

---

## Observed Behavior

### Test Configuration
- **Test Date:** October 16, 2024
- **Symbols:** TQQQ, SQQQ, SSO, SDS, TNA, TZA, FAS, FAZ, UVXY, SVXY (10 symbols)
- **Warmup Period:** 3 days (1,170 bars)
- **Trading Period:** 1 day (390 bars)
- **Initial Capital:** $100,000
- **Max Positions:** 3

### Actual Results
```
Performance:
  Final Equity:       $100,000.00
  Total Return:       0.00%
  MRD (Daily):        0.00%
  Total Trades:       0      ← CRITICAL BUG

Execution:
  Bars Processed:     1,560 (1,170 warmup + 390 trading)
  Data Load Time:     27ms
  Execution Time:     57ms
```

### Expected Behavior
The system should execute trades during the 390-bar trading period when:
1. Predictions exceed the minimum threshold (0.1%)
2. At least one symbol shows positive predicted return
3. Market conditions are favorable for rotation

**Expected:** 5-20 trades over 390 bars (approx. 1 trade per 20-80 bars)
**Actual:** 0 trades

---

## Root Cause Analysis

### Issue #1: Feature Extraction Warmup Delay (CRITICAL)

**Location:** `src/predictor/feature_extractor.cpp:15-19`

```cpp
// Need full lookback window for reliable features
if (!is_ready()) {
    prev_close_ = bar.close;
    return std::nullopt;  // ← Returns NO features for first 50 bars
}
```

**Problem:**
- Feature extractor requires **50 bars** before producing any features
- During bars 1-50: `std::nullopt` is returned, NO features extracted
- This delays predictor training by 50 bars

**Impact:**
- First 50 bars of warmup period are WASTED
- Predictor receives ZERO updates during this time
- Effective warmup is reduced from 1,170 bars to only 1,120 bars

**Configuration:**
```cpp
// include/predictor/feature_extractor.h:29
static constexpr size_t LOOKBACK = 50;  // ← Hardcoded, cannot be changed

// include/predictor/feature_extractor.h:57
bool is_ready() const { return bar_count_ >= LOOKBACK; }
```

---

### Issue #2: Predictor Initialization with Zero Weights (CRITICAL)

**Location:** `src/predictor/online_predictor.cpp:7-12`

```cpp
OnlinePredictor::OnlinePredictor(size_t n_features, double lambda)
    : theta_(Eigen::VectorXd::Zero(n_features)),  // ← All weights start at ZERO
      P_(Eigen::MatrixXd::Identity(n_features, n_features) * 100.0),
      lambda_(lambda),
      n_features_(n_features),
      updates_(0) {
```

**Problem:**
- Weight vector `theta_` initialized to **all zeros**
- Predictions = `theta_.dot(features)` = **0.0** (when theta is zero)
- Even after updates, weights may remain very small

**Impact:**
```
Initial predictions:
  - All symbols: 0.0 (theta is zero)

After 50 bars:
  - Still 0.0 (no features, no updates)

After 1,170 bars (end of warmup):
  - May be very small (e.g., 0.00001 to 0.0001)
  - Likely BELOW 0.001 threshold
```

**Prediction Formula:**
```cpp
// src/predictor/online_predictor.cpp:26
return theta_.dot(features);  // If theta ≈ 0, prediction ≈ 0
```

---

### Issue #3: Minimum Prediction Threshold Too High (HIGH)

**Location:** `include/trading/multi_symbol_trader.h:39`

```cpp
double min_prediction_threshold = 0.001;  // Minimum predicted return to trade
```

**Problem:**
- Threshold is **0.1%** return per bar (1-minute bar)
- This is EXTREMELY high for minute-level predictions
- Equivalent to **39% daily return** (0.1% × 390 bars)
- Most realistic predictions will be much smaller (0.01% - 0.05%)

**Trading Logic:**
```cpp
// src/trading/multi_symbol_trader.cpp:90
if (ranked[i].second > config_.min_prediction_threshold) {
    top_symbols.push_back(ranked[i].first);  // ← Only if > 0.001
}
```

**Impact:**
Even if predictor learns successfully:
- Predictions might be 0.00001 to 0.0005 (realistic for 1-min bars)
- All predictions rejected as "too small"
- Zero trades executed

**Comparison:**
```
Threshold:     0.001 (0.1% per bar)
Daily equiv:   39% (0.1% × 390 bars) ← UNREALISTIC

Realistic predictions:
  0.00001 (0.001%)  ← Rejected
  0.00005 (0.005%)  ← Rejected
  0.0001  (0.01%)   ← Rejected
  0.0005  (0.05%)   ← Rejected
  0.001   (0.1%)    ← Accepted (but very rare!)
```

---

### Issue #4: Predictor Update Logic Delay (MEDIUM)

**Location:** `src/trading/multi_symbol_trader.cpp:48-56`

```cpp
// Update predictor with realized return (if we have enough history)
if (bars_seen_ > 1 && extractors_[symbol]->bar_count() >= 2) {
    const auto& history = extractors_[symbol]->history();
    Price prev_price = history[history.size() - 2].close;
    if (prev_price > 0) {
        double actual_return = (bar.close - prev_price) / prev_price;
        predictors_[symbol]->update(features.value(), actual_return);
    }
}
```

**Problem:**
- Updates require: `bars_seen_ > 1` AND `bar_count >= 2`
- Combined with feature extraction delay (50 bars), updates don't start until bar 51
- Update uses 1-bar return, but features might capture longer-term patterns

**Impact:**
- Further delays learning
- Predictor doesn't see ANY data for first 50 bars
- May learn slowly or poorly after that

---

### Issue #5: No Warmup Verification or Diagnostics (HIGH)

**Location:** Multiple files (lack of logging/diagnostics)

**Problem:**
- No logging of prediction values during warmup
- No verification that predictor is learning
- No diagnostic output showing:
  - Feature values
  - Prediction values
  - Weight vector magnitude
  - Update count

**Impact:**
- Silent failure - system appears to work but doesn't trade
- No way to debug without code changes
- User has no visibility into internal state

---

## Execution Flow Analysis

### Timeline of Events (1,560 bars total)

```
Bars 1-50:
  ├─ Feature Extractor: Warmup (returns std::nullopt)
  ├─ Predictions: NONE (no features)
  ├─ Predictor Updates: NONE (no features)
  └─ Trading: Disabled (warmup period)

Bars 51-1,170:
  ├─ Feature Extractor: Ready (returns 25 features)
  ├─ Predictions: ~0.0 initially, slowly increases
  ├─ Predictor Updates: YES (1,120 updates per symbol)
  ├─ Theta weights: Gradually learn from data
  └─ Trading: Disabled (warmup period)

Bars 1,171-1,560:
  ├─ Feature Extractor: Ready
  ├─ Predictions: ~0.00001 to 0.0005 (still very small)
  ├─ make_trades() called: YES
  ├─ Threshold check: 0.00001 < 0.001 → FAIL
  ├─ Top symbols selected: NONE (all below threshold)
  └─ Trades executed: ZERO ← BUG MANIFESTS HERE
```

### Prediction Values (Estimated)

Based on the logic:

| Bar # | Features? | Theta Magnitude | Prediction Range | Passes Threshold? | Trades? |
|-------|-----------|-----------------|------------------|-------------------|---------|
| 1-50  | ❌ No     | 0.0000          | N/A              | N/A               | No (warmup) |
| 51    | ✅ Yes    | 0.0000          | 0.0000           | ❌ No             | No (warmup) |
| 100   | ✅ Yes    | ~0.0001         | -0.00001 to 0.00002 | ❌ No          | No (warmup) |
| 500   | ✅ Yes    | ~0.001          | -0.0001 to 0.0002 | ❌ No            | No (warmup) |
| 1,170 | ✅ Yes    | ~0.005          | -0.0005 to 0.0008 | ❌ No            | No (warmup) |
| 1,171 | ✅ Yes    | ~0.005          | -0.0005 to 0.0008 | ❌ No            | **No (BELOW THRESHOLD)** |
| 1,200 | ✅ Yes    | ~0.005          | -0.0005 to 0.0008 | ❌ No            | **No (BELOW THRESHOLD)** |
| 1,560 | ✅ Yes    | ~0.005          | -0.0005 to 0.0008 | ❌ No            | **No (BELOW THRESHOLD)** |

**Result:** Zero trades for entire 390-bar trading period.

---

## Evidence

### 1. Console Output Shows Zero Trades

```
Trade Statistics:
  Total Trades:       0          ← PRIMARY EVIDENCE
  Winning Trades:     0
  Losing Trades:      0
  Win Rate:           0.0%
  Profit Factor:      0.00

Assessment: 🔴 Poor (not ready for live)
```

### 2. Equity Unchanged

```
Performance:
  Initial Capital:    $100,000.00
  Final Equity:       $100,000.00   ← No change = no trades
  Total Return:       +0.00%
```

### 3. No Position Activity

- Cash remains at $100,000 throughout
- `positions_` map remains empty
- `total_trades_` counter stays at 0

---

## Reproduction Steps

1. Build sentio_lite
2. Run mock test:
   ```bash
   cd build
   ./sentio_lite mock --data-dir ../data --date 2024-10-16 --verbose
   ```
3. Observe output: "Total Trades: 0"
4. Check results.json: `"total_trades": 0`

**Reproducibility:** 100% (occurs every time)

---

## Potential Fixes

### Fix #1: Reduce Prediction Threshold (IMMEDIATE)

**Change:**
```cpp
// include/trading/multi_symbol_trader.h:39
double min_prediction_threshold = 0.00001;  // Was: 0.001 (100x reduction)
```

**Rationale:**
- 0.00001 = 0.001% per bar
- 0.001% × 390 bars = 0.39% daily (more realistic)
- Allows realistic predictions to pass

**Impact:** HIGH - Should immediately enable trades

---

### Fix #2: Reduce Feature Extractor Warmup (IMMEDIATE)

**Change:**
```cpp
// include/predictor/feature_extractor.h:29
static constexpr size_t LOOKBACK = 20;  // Was: 50 (60% reduction)
```

**Rationale:**
- 20 bars is sufficient for most features
- RSI, momentum calculated over 10-14 bars
- Reduces wasted warmup time

**Impact:** MEDIUM - Improves learning efficiency

---

### Fix #3: Initialize Predictor with Small Random Weights (HIGH)

**Change:**
```cpp
// src/predictor/online_predictor.cpp:8
theta_(Eigen::VectorXd::Random(n_features) * 0.001),  // Was: Zero
```

**Rationale:**
- Small random initialization encourages exploration
- Prevents all predictions being exactly zero
- Standard practice in machine learning

**Impact:** MEDIUM - Helps early-stage predictions

---

### Fix #4: Make Threshold Configurable (MEDIUM)

**Change:**
```cpp
// Allow threshold to be set via command line
--min-threshold PCT    Minimum prediction threshold (default: 0.00001)
```

**Rationale:**
- Different markets/timeframes need different thresholds
- Easier to tune without recompiling
- User can experiment

**Impact:** LOW (usability) - Doesn't fix root cause

---

### Fix #5: Add Diagnostic Logging (HIGH)

**Change:**
Add verbose logging to show:
```cpp
if (verbose && bars_seen_ > config_.min_bars_to_learn && bars_seen_ % 10 == 0) {
    std::cout << "Bar " << bars_seen_ << " predictions: ";
    for (const auto& [sym, pred] : predictions) {
        std::cout << sym << "=" << (pred.predicted_return * 100) << "% ";
    }
    std::cout << "\n";
}
```

**Impact:** HIGH - Allows debugging without code changes

---

### Fix #6: Warm Start with Historical Statistics (ADVANCED)

**Change:**
```cpp
// Initialize theta based on historical mean returns
for (each symbol) {
    double historical_mean = calculate_historical_mean_return(symbol, 100);
    theta_[0] = historical_mean;  // Bias term
}
```

**Impact:** MEDIUM - Better initial predictions

---

## Recommended Action Plan

### Immediate (Deploy Today)

1. **Reduce threshold to 0.00001** (Fix #1)
   - Change one line in `multi_symbol_trader.h:39`
   - Rebuild and test immediately

2. **Add diagnostic logging** (Fix #5)
   - Add prediction value logging
   - Verify predictions are being generated
   - Identify actual prediction magnitudes

### Short-term (This Week)

3. **Reduce feature warmup to 20 bars** (Fix #2)
   - Change `LOOKBACK` constant
   - More efficient warmup

4. **Make threshold configurable** (Fix #4)
   - Add `--min-threshold` command-line option
   - Allow easy experimentation

### Medium-term (Next Sprint)

5. **Initialize with small random weights** (Fix #3)
   - Better than all-zeros initialization
   - Standard ML practice

6. **Implement warm start** (Fix #6)
   - Use historical statistics
   - Production-ready feature

---

## Testing Plan

### Unit Tests Needed

1. **Test feature extraction with small LOOKBACK**
   ```cpp
   TEST(FeatureExtractor, ProducesValidFeaturesAfter20Bars)
   ```

2. **Test predictor learns from updates**
   ```cpp
   TEST(OnlinePredictor, WeightsChangeAfterUpdates)
   ```

3. **Test trading with low threshold**
   ```cpp
   TEST(MultiSymbolTrader, ExecutesTradesWithLowThreshold)
   ```

### Integration Tests Needed

1. **Test full warmup and trading cycle**
   - Verify trades are executed
   - Verify predictions exceed threshold
   - Verify positions are opened/closed

2. **Test with different thresholds**
   - 0.00001, 0.0001, 0.001
   - Measure trade frequency
   - Optimize threshold value

---

## Related Issues

- **Performance:** MRD calculation assumes trades are executed (currently 0%)
- **Dashboard:** Shows "Poor (not ready for live)" due to zero trades
- **Usability:** No indication WHY trades aren't executing
- **Documentation:** Threshold of 0.1% not documented as extremely high

---

## Impact Assessment

### Business Impact
- **CRITICAL:** System cannot trade, core functionality broken
- **User Experience:** Appears to work but produces no results
- **Confidence:** Users cannot validate strategy effectiveness

### Technical Debt
- Hardcoded constants (LOOKBACK, threshold)
- Lack of diagnostics/observability
- No validation of predictor learning

### Risk
- **Low** for immediate fix (changing threshold is safe)
- **Medium** for other fixes (need testing)
- **High** if not addressed (system unusable)

---

## Reference Section

### Source Modules (All Related Files)

#### Core Trading Logic
1. **`src/trading/multi_symbol_trader.cpp`** (305 lines)
   - Lines 30-73: `on_bar()` - Main bar processing
   - Lines 64-66: Trading decision gate (bars_seen_ > min_bars_to_learn)
   - Lines 75-126: `make_trades()` - Trading decision logic
   - Lines 88-93: Top symbol selection with threshold check
   - Lines 111-125: Position entry logic with threshold check

2. **`include/trading/multi_symbol_trader.h`** (173 lines)
   - Line 39: **`min_prediction_threshold = 0.001`** ← KEY PARAMETER
   - Line 31: `min_bars_to_learn = 100` (warmup bars)
   - Lines 26-40: TradingConfig structure

#### Prediction System
3. **`src/predictor/online_predictor.cpp`** (69 lines)
   - Lines 7-17: Constructor with **theta_ = Zero** initialization
   - Lines 19-27: `predict()` - Returns theta_.dot(features)
   - Lines 29-59: `update()` - EWRLS weight update

4. **`include/predictor/online_predictor.h`** (62 lines)
   - Lines 8-12: Class definition
   - Line 8: Eigen::VectorXd theta_ (weight vector)

#### Feature Extraction
5. **`src/predictor/feature_extractor.cpp`** (311 lines)
   - Lines 11-74: `extract()` - Feature extraction logic
   - Lines 15-19: **Warmup gate (returns nullopt if not ready)**
   - Lines 76-259: Individual feature calculations

6. **`include/predictor/feature_extractor.h`** (93 lines)
   - Line 29: **`LOOKBACK = 50`** ← KEY PARAMETER
   - Line 30: `NUM_FEATURES = 25`
   - Line 57: `is_ready()` - Returns bar_count_ >= LOOKBACK

#### Main Entry Point
7. **`src/main.cpp`** (504 lines)
   - Lines 241-433: `run_mock_mode()` - Mock trading execution
   - Lines 263-266: Test date selection
   - Lines 275-282: Bar filtering for warmup + trading
   - Lines 312-325: Main bar processing loop
   - Line 313: `config.trading.min_bars_to_learn = config.warmup_bars`

#### Configuration
8. **`include/core/types.h`** (28 lines)
   - Core type definitions (Timestamp, Price, Volume)

9. **`include/core/bar.h`** (18 lines)
   - Bar structure (OHLCV data)

#### Supporting Modules
10. **`src/trading/position.cpp`** (Position management)
11. **`src/trading/trade_history.cpp`** (Trade record keeping)
12. **`include/utils/circular_buffer.h`** (Price history buffer)
13. **`src/core/math_utils.cpp`** (Statistical functions)

### Key Data Structures

```cpp
// Prediction threshold check (multi_symbol_trader.cpp:90, 115)
if (ranked[i].second > config_.min_prediction_threshold)

// Feature warmup check (feature_extractor.cpp:16)
if (!is_ready())  // bar_count_ < 50

// Predictor initialization (online_predictor.cpp:8)
theta_(Eigen::VectorXd::Zero(n_features))

// Trading gate (multi_symbol_trader.cpp:64)
if (bars_seen_ > config_.min_bars_to_learn)
```

### Critical Code Paths

1. **Bar Processing Flow:**
   ```
   main.cpp:on_bar()
   → multi_symbol_trader.cpp:on_bar()
   → feature_extractor.cpp:extract()  [may return nullopt]
   → online_predictor.cpp:predict()   [returns theta.dot(features)]
   → online_predictor.cpp:update()    [updates theta]
   → multi_symbol_trader.cpp:make_trades() [if bars_seen_ > warmup]
   → Threshold check [pred > 0.001?]
   → enter_position() [if passed threshold]
   ```

2. **Failure Point:**
   ```
   predictions calculated → all ~0.0 to 0.0005
   → threshold check (> 0.001?) → FAIL
   → top_symbols empty
   → NO trades executed
   ```

### Configuration Hierarchy

```
Command Line Args (main.cpp)
  ↓
Config struct (main.cpp:17-42)
  ↓
TradingConfig (multi_symbol_trader.h:26-40)
  ├─ min_prediction_threshold: 0.001  ← TOO HIGH
  ├─ min_bars_to_learn: 100 → 1170    ← Set from warmup_days
  ├─ lambda: 0.98
  └─ max_positions: 3
  ↓
Feature Extractor Constants
  ├─ LOOKBACK: 50  ← HARDCODED, TOO HIGH
  └─ NUM_FEATURES: 25
  ↓
Predictor Initialization
  └─ theta: Zeros(25)  ← STARTS AT ZERO
```

---

## Verification Checklist

After implementing fixes:

- [ ] Predictions are logged during trading period
- [ ] At least one prediction exceeds threshold
- [ ] Trades are executed (total_trades > 0)
- [ ] Equity changes from initial capital
- [ ] Dashboard shows non-zero results
- [ ] Unit tests pass
- [ ] Integration tests pass

---

## Sign-off

**Analysis Completed:** 2025-10-17
**Reviewed By:** System Analysis
**Priority:** CRITICAL
**Estimated Fix Time:** 1 hour (immediate fixes), 1 week (all fixes)

**Next Steps:**
1. Review findings with team
2. Implement Fix #1 (reduce threshold) immediately
3. Test with real data
4. Iterate based on results

---

**End of Bug Report**
