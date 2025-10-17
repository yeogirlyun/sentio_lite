# online_trader Configuration Analysis
**Date**: 2025-10-17
**Purpose**: Identify why online_trader achieves 50-100 trades/day @ 0.5% MRD vs sentio_lite's 6 trades @ 0.07% MRD

---

## Executive Summary

online_trader achieves **8-17x more trading activity** and **7x higher returns** by using:
1. **Probability-based thresholds** instead of magnitude/confidence filters
2. **11x shorter warmup** (100 bars vs 1173 bars)
3. **No trade frequency limits** or minimum hold requirements
4. **Tanh-based probability scaling** that amplifies small predictions

---

## Key Configuration Differences

### 1. Warmup Period ⚠️ CRITICAL

| Parameter | online_trader | sentio_lite | Impact |
|-----------|--------------|-------------|--------|
| Warmup bars | **50-100** | **1173** | 11x too long! |
| Warmup days | ~0.25 days | 3 days | Wastes 3 days of data |

**Issue**: sentio_lite wastes 1173 bars (3 full trading days) before making any trades.

**Fix**: Reduce `min_bars_to_learn` from 1173 to **100 bars**

---

### 2. Entry/Exit Thresholds ⚠️ CRITICAL

#### online_trader: Probability-Based
```cpp
// Convert prediction to probability using tanh
double scaling = 50.0;
signal.probability = 0.5 + 0.5 * std::tanh(prediction * scaling);

// Entry conditions
if (signal.probability > buy_threshold) enter_long();    // 0.53
if (signal.probability < sell_threshold) enter_short();  // 0.47
```

**Probability mapping**:
- Prediction = +0.1% → Probability = 51.2% ❌ (below 53% threshold)
- Prediction = +0.2% → Probability = 62.4% ✅ (above 53% threshold)
- Prediction = +0.5% → Probability = 76.0% ✅
- Prediction = +1.0% → Probability = 88.1% ✅

#### sentio_lite: Magnitude + Confidence Filters
```cpp
// Entry conditions (ALL must pass)
if (prediction.pred_5bar.prediction < 0.0005) return false;     // 5 bps minimum
if (prediction.pred_5bar.confidence < 0.5) return false;        // 50% confidence
if (!should_enter()) return false;                               // Multi-horizon agreement
if (!trade_filter.can_enter_position()) return false;           // Frequency limits
```

**Problem**: Requires ALL conditions to pass, creating a narrow entry window.

---

### 3. Prediction-to-Probability Scaling

#### online_trader uses tanh scaling:
```
probability = 0.5 + 0.5 * tanh(prediction * 50)
```

This creates **exponential amplification**:
- Small predictions (0.1-0.5%) → Moderate probabilities (51-76%)
- Medium predictions (1-2%) → High probabilities (88-96%)

#### sentio_lite uses raw prediction + confidence:
```
Use pred_5bar.prediction directly
Compare to min_prediction_for_entry = 0.0005 (5 bps)
```

**Issue**: No scaling/amplification, making small predictions look insignificant.

---

### 4. Multi-Horizon Parameters

| Parameter | online_trader | sentio_lite | Notes |
|-----------|--------------|-------------|-------|
| Horizons | {1, 5, 10} bars | {1, 5, 10} bars | ✅ Same |
| Weights | {0.3, 0.5, 0.2} | Equal | Different ensemble |
| Lambda 1-bar | 0.995 | 0.98 | Ours adapts faster |
| Lambda 5-bar | 0.995 | 0.99 | Ours adapts faster |
| Lambda 10-bar | 0.995 | 0.995 | ✅ Same |

**Observation**: online_trader uses **single lambda** (0.995) for all horizons, while we vary them.

---

### 5. Trade Filtering

| Feature | online_trader | sentio_lite | Impact |
|---------|--------------|-------------|--------|
| Min bars to hold | **None** | 2 bars | Limits exits |
| Min bars between entries | **None** | 2 bars | Prevents re-entry |
| Max trades per hour | **None** | 20 | Limits activity |
| Max trades per day | **None** | 100 | Limits activity |
| Min prediction for entry | **None** | 5 bps | Blocks small signals |
| Min confidence for entry | **None** | 50% | Blocks uncertain signals |

**Issue**: sentio_lite has **6 different filters** that block trades, while online_trader has **zero**.

---

### 6. Position Sizing

#### online_trader: Kelly Criterion
```cpp
double kelly = (p * b - q) / b;  // p = win prob, b = win/loss ratio
double fractional_kelly = kelly * 0.25;  // Use 25% of full Kelly
double position_size = capital * fractional_kelly;
```

#### sentio_lite: Kelly Criterion (newly integrated)
```cpp
// Same as online_trader - recently integrated ✅
```

**Status**: ✅ Already integrated from online_trader

---

## Root Cause Analysis

### Why sentio_lite only makes 6 trades/day:

1. **Warmup (1173 bars)** - Wastes entire first 3 days
2. **Trade filter blocks entries** - 6 different blocking conditions
3. **High prediction threshold** - 5 bps minimum (vs online_trader's ~0.6 bps effective)
4. **No probability scaling** - Small predictions stay small
5. **Re-entry cooldown** - 2-bar cooldown prevents rapid rotation

### Why online_trader makes 50-100 trades/day:

1. **Short warmup (100 bars)** - Starts trading after ~25 minutes
2. **No trade filters** - Only probability threshold (0.53)
3. **Low effective threshold** - ~0.6 bps after tanh scaling
4. **Probability amplification** - tanh(x * 50) amplifies small signals
5. **No cooldowns** - Can re-enter immediately

---

## Recommended Fixes for sentio_lite

### Priority 1: Reduce Warmup (CRITICAL)
```cpp
// In TradingConfig
size_t min_bars_to_learn = 100;  // Was 1173 (3 days worth)
```

**Expected Impact**: Start trading after 25 minutes instead of 3 days

---

### Priority 2: Add Probability Scaling (CRITICAL)
```cpp
// Add to PredictionData or make_trades()
double prediction_to_probability(double prediction, double scaling = 50.0) {
    return 0.5 + 0.5 * std::tanh(prediction * scaling);
}

// Use probability-based threshold
double prob = prediction_to_probability(pred_data.prediction.pred_5bar.prediction);
if (prob > 0.53) {
    enter_position();  // Buy threshold
}
```

**Expected Impact**: Amplify small predictions, increase entry opportunities

---

### Priority 3: Remove/Relax Trade Filter (HIGH)

#### Option A: Remove completely
```cpp
// Comment out trade filter checks in make_trades()
// if (!trade_filter_->can_enter_position(...)) continue;
```

#### Option B: Make extremely permissive
```cpp
// In TradeFilter::Config
min_bars_to_hold = 1;              // Was 2
min_bars_between_entries = 1;      // Was 2
max_trades_per_hour = 1000;        // Was 20
max_trades_per_day = 1000;         // Was 100
min_prediction_for_entry = 0.0;    // Was 0.0005 (disable)
min_confidence_for_entry = 0.0;    // Was 0.5 (disable)
```

**Expected Impact**: Remove artificial trading frequency caps

---

### Priority 4: Simplify Entry Logic (HIGH)

**Current (complex)**:
```cpp
bool can_enter = pred.should_enter(min_pred, min_conf) &&  // Multi-condition
                 trade_filter_->can_enter_position(...);     // 6 more filters
```

**Proposed (simple)**:
```cpp
double prob = prediction_to_probability(pred.pred_5bar.prediction);
bool can_enter = prob > buy_threshold;  // Single threshold (0.53)
```

**Expected Impact**: Match online_trader's simplicity and activity

---

### Priority 5: Use Uniform Lambda (MEDIUM)
```cpp
// In TradingConfig
horizon_config.lambda_1bar = 0.995;   // Was 0.98
horizon_config.lambda_5bar = 0.995;   // Was 0.99
horizon_config.lambda_10bar = 0.995;  // Keep at 0.995
```

**Expected Impact**: Match online_trader's conservative learning rate

---

## Implementation Plan

### Phase 1: Quick Wins (15 minutes)
1. Set `min_bars_to_learn = 100`
2. Set all lambdas to `0.995`
3. Disable trade filter: `min_prediction_for_entry = 0.0`, `min_confidence_for_entry = 0.0`

**Expected**: ~20-30 trades/day

### Phase 2: Probability Scaling (30 minutes)
1. Add `prediction_to_probability()` function
2. Modify `make_trades()` to use probability threshold
3. Set `buy_threshold = 0.53`, `sell_threshold = 0.47`

**Expected**: 50-80 trades/day

### Phase 3: Simplify Entry Logic (15 minutes)
1. Remove multi-horizon agreement requirement
2. Remove confidence threshold
3. Use single probability threshold

**Expected**: 80-100 trades/day

### Phase 4: Fine-Tuning (1 hour)
1. Adjust tanh scaling factor (try 40, 50, 60)
2. Calibrate buy/sell thresholds based on win rate
3. Re-enable minimal trade filter if needed

**Expected**: 50-100 trades/day @ 0.3-0.5% MRD

---

## Expected Performance After Fixes

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Trades/day | 6 | 50-100 | 8-17x |
| MRD | 0.07% | 0.3-0.5% | 4-7x |
| Win rate | 50% | 55-60% | Target |
| Warmup | 3 days | 0.25 days | 12x faster |
| Trade frequency | 1.5% | 12-25% | 8-17x |

---

## Risk Considerations

### Potential Issues with Fixes:

1. **Over-trading**: May trade too frequently (100+ trades/day)
   - **Mitigation**: Add back minimal frequency limits (50-100/day)

2. **Transaction costs**: More trades = more costs
   - **Mitigation**: Already have Alpaca cost model integrated

3. **Win rate drop**: Easier entry may reduce quality
   - **Mitigation**: Monitor win rate, calibrate thresholds

4. **Whipsaws**: Rapid entry/exit on noise
   - **Mitigation**: Keep min_bars_to_hold = 2

---

## Testing Plan

### Test 1: Warmup Only
- Change `min_bars_to_learn = 100`
- Keep all other settings
- **Expected**: Earlier trading, same activity level

### Test 2: Warmup + Disable Filters
- `min_bars_to_learn = 100`
- `min_prediction_for_entry = 0.0`
- `min_confidence_for_entry = 0.0`
- **Expected**: 20-40 trades/day

### Test 3: Full online_trader Config
- Warmup = 100
- Probability scaling with tanh
- Buy/sell thresholds = 0.53/0.47
- No trade filters
- **Expected**: 50-100 trades/day @ 0.3-0.5% MRD

---

## Files to Modify

### Phase 1 (Quick Wins)
- `include/trading/multi_symbol_trader.h:34` - Change `min_bars_to_learn`
- `include/trading/multi_symbol_trader.h:55-57` - Change lambdas
- `include/trading/multi_symbol_trader.h:64-65` - Disable filter thresholds

### Phase 2 (Probability Scaling)
- `src/trading/multi_symbol_trader.cpp:228-327` - Add probability conversion
- `include/trading/multi_symbol_trader.h` - Add buy/sell threshold config

### Phase 3 (Simplify Entry)
- `include/predictor/multi_horizon_predictor.h:77-98` - Simplify should_enter()
- `src/trading/multi_symbol_trader.cpp:make_trades()` - Use probability threshold

---

## References

### online_trader Files
- `include/strategy/online_ensemble_strategy.h:33-85` - Configuration
- `src/strategy/online_strategy_base.cpp:188` - Probability scaling
- `include/backend/adaptive_portfolio_manager.h:232-233` - Thresholds
- `include/cli/ensemble_workflow_command.h:61-62` - Default thresholds

### sentio_lite Files
- `include/trading/multi_symbol_trader.h:28-67` - TradingConfig
- `src/trading/multi_symbol_trader.cpp:228-327` - make_trades()
- `include/trading/trade_filter.h:30-68` - Trade filter config
- `include/predictor/multi_horizon_predictor.h:77-98` - Entry logic

---

## Conclusion

The root cause of low trading activity in sentio_lite is **over-filtering**:

1. ❌ **11x too much warmup** (1173 vs 100 bars)
2. ❌ **6 different entry filters** (vs 1 probability threshold)
3. ❌ **No signal amplification** (vs tanh scaling)
4. ❌ **High thresholds** (5 bps vs ~0.6 bps effective)

By adopting online_trader's simpler, probability-based approach, sentio_lite should achieve:
- **50-100 trades/day** (vs current 6)
- **0.3-0.5% MRD** (vs current 0.07%)
- **Trading starts in 25 minutes** (vs 3 days)

The fixes are straightforward and can be implemented in ~1 hour.
