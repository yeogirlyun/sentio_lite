# Williams RSI Strategy Analysis - Sentio Lite Project

## Executive Summary

After thorough exploration of the sentio_lite project, I've identified that:

1. **There is NO "sentio_trader" directory** - you were testing for a reference that doesn't exist in the current workspace
2. **Williams RSI is NOT currently implemented** as a dedicated strategy in sentio_lite
3. **RSI(2) Connors Pullback** has been proposed as a potential detector (not yet integrated)
4. **The project uses SIGOR** - a 7-detector ensemble strategy with RSI(14) as one component
5. **Current performance**: 0.384% MRD (Mean Reversion Drawdown) - Target: 0.5% MRD

## Project Structure Overview

### Current Architecture
**File**: `/Volumes/MyBookDuo/Projects/sentio_lite/`

```
sentio_lite/
├── src/
│   ├── main.cpp                          # CLI entry point
│   ├── strategy/sigor_strategy.cpp       # 7-detector ensemble (primary strategy)
│   ├── predictor/feature_extractor.cpp   # 75-feature ML input
│   └── trading/                          # Execution layer
├── include/
│   ├── strategy/sigor_strategy.h         # SIGOR interface
│   └── predictor/feature_extractor.h     # Feature definitions
├── tests/
│   ├── rsi2_detector.h/.cpp              # RSI(2) Connors implementation
│   └── test_rsi2_pullback.cpp            # RSI(2) test harness
├── docs/
│   ├── PROPOSED_DETECTORS_ANALYSIS.md    # Strategy roadmap
│   └── FEATURE_ENGINE_COMPARISON_CRITICAL.md
└── config/
    └── sigor_params.json                 # Optimized parameters
```

---

## RSI Implementation Analysis

### 1. Current RSI Usage in SIGOR (prod_rsi_14_())

**Location**: `/Volumes/MyBookDuo/Projects/sentio_lite/src/strategy/sigor_strategy.cpp` (lines 112-118)

```cpp
double SigorStrategy::prob_rsi_14_() const {
    const int w = config_.win_rsi;  // 14 (configurable, default 14)
    if (static_cast<int>(gains_.size()) < w + 1) return 0.5;
    
    double rsi = compute_rsi(w); // 0..100
    return clamp01((rsi - 50.0) / 100.0 * 1.0 + 0.5);  // Maps to [0..1] probability
}
```

**RSI Computation** (lines 276-292):
```cpp
double SigorStrategy::compute_rsi(int window) const {
    if (window <= 0 || static_cast<int>(gains_.size()) < window + 1) return 50.0;
    
    double avg_gain = 0.0, avg_loss = 0.0;
    for (int i = static_cast<int>(gains_.size()) - window; i < static_cast<int>(gains_.size()); ++i) {
        avg_gain += gains_[static_cast<size_t>(i)];
        avg_loss += losses_[static_cast<size_t>(i)];
    }
    
    avg_gain /= static_cast<double>(window);
    avg_loss /= static_cast<double>(window);
    
    if (avg_loss <= 1e-12) return 100.0;
    
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));  // Standard Wilder's RSI formula
}
```

**Key Characteristics**:
- Uses **Wilder's RSI formula**: RSI = 100 - (100 / (1 + RS))
- Tracks gains/losses at bar entry: `closes_.push_back(bar.close); gains/losses updated from delta`
- Window: 14 bars (default, optimized to 14 via Optuna)
- **NOT anticipatory** - purely reactive based on past price changes
- Output normalized to [0, 1] probability space

**Current Weight**: `w_rsi = 1.8` (out of 7 detectors, weight scale 0.1-1.8)

---

### 2. RSI(2) Connors Pullback Implementation (Proposed)

**Location**: 
- Header: `/Volumes/MyBookDuo/Projects/sentio_lite/tests/rsi2_detector.h`
- Implementation: `/Volumes/MyBookDuo/Projects/sentio_lite/tests/rsi2_detector.cpp`
- Test harness: `/Volumes/MyBookDuo/Projects/sentio_lite/tests/test_rsi2_pullback.cpp`

**Algorithm** (RSI2Detector::update):

```cpp
// Calculate RSI(2) - ultra-short term momentum
double calculate_rsi() {
    if (gains.size() < 2) return 50.0;
    
    double avg_gain = (gains[0] + gains[1]) / 2.0;
    double avg_loss = (losses[0] + losses[1]) / 2.0;
    
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// State tracking
state.oversold = (rsi < 10.0);       // Extreme bearish
state.overbought = (rsi > 90.0);     // Extreme bullish
```

**Entry Conditions**:
- **Long signal**: RSI(2) < 10 (oversold) → "Buy the dip"
- **Short signal**: RSI(2) > 90 (overbought) → "Sell the rip"
- **Confidence scaling**: `min(1.0, (10 - RSI) / 10)` for oversold, `min(1.0, (RSI - 90) / 10)` for overbought

**Filters**:
```
1. VWAP Distance Guard
   - Max deviation: 2.0 standard deviations from VWAP
   - Prevents counter-trend entries far from fair value
   
2. Volume Guard
   - Min volume ratio: 0.8x (vs 20-bar average)
   - Ensures signal has participation
```

**Characteristics**:
- **IS anticipatory**: Uses only 2-bar window, reacts faster than RSI(14)
- More sensitive to short-term reversals
- High false positive rate without filters
- Integration potential: MEDIUM (RSI(14) exists; RSI(2) is different timeframe)

---

### 3. Feature-Level RSI Usage in ML Pipeline

**Location**: `/Volumes/MyBookDuo/Projects/sentio_lite/src/predictor/feature_extractor.cpp`

The ML pipeline (75 features) includes RSI-like calculations:

```cpp
// Feature index 40 (from raw/absolute features section)
features(idx++) = calculate_rsi_like(prices, 14);  // RSI-like metric
```

**RSI-Like Calculation** (lines 339-370):
```cpp
double calculate_rsi_like(const std::vector<Price>& prices, int period) const {
    // Percentage-based gains/losses (NOT absolute)
    for (size_t i = n - window; i < n; ++i) {
        double ret = (prices[i] - prices[i-1]) / prices[i-1];  // % return
        gains.push_back(ret > 0 ? ret : 0.0);
        losses.push_back(ret < 0 ? -ret : 0.0);
    }
    
    double avg_gain = mean(gains);
    double avg_loss = mean(losses);
    
    double rs = avg_gain / avg_loss;
    return rs / (1.0 + rs);  // Normalized to [0, 1]
}
```

**Key Difference from SIGOR RSI**:
- Uses **percentage returns** instead of absolute price changes
- Enables scale-invariant learning (critical for multi-asset ML)
- Already proven: baseline v2.0 (5.41% MRD) used similar approach

---

## SIGOR Strategy Architecture (7 Detectors)

**File**: `/Volumes/MyBookDuo/Projects/sentio_lite/include/strategy/sigor_strategy.h`

```
SIGOR = Log-Odds Fusion of 7 Detectors:
├── 1. Bollinger Bands (Z-score mean reversion)
├── 2. RSI(14) - THIS IS THE RSI COMPONENT
├── 3. Momentum (10-bar)
├── 4. VWAP Reversion
├── 5. Opening Range Breakout (ORB)
├── 6. Order Flow Imbalance (OFI) proxy
└── 7. Volume Surge (scaled by momentum)

Aggregation: Log-odds voting with configurable weights
Output: P(long) in [0, 1] probability space
```

**Log-Odds Fusion** (lines 210-232):
```cpp
double aggregate_probability(...) {
    double num = 0.0, den = 0.0;
    
    for (int i = 0; i < 7; ++i) {
        double p = clamp(probs[i], 1e-6, 1-1e-6);
        double l = log(p / (1-p));  // Log-odds transform
        num += weights[i] * l;
        den += weights[i];
    }
    
    double L = num / den;
    double P = 1.0 / (1.0 + exp(-k * L));  // Inverse logit with sharpness k
    
    return P;
}
```

**Current Optimized Parameters** (from Optuna run):
```json
{
  "k": 1.8,              // Fusion sharpness
  "w_boll": 0.7,         // Bollinger weight
  "w_rsi": 1.8,          // RSI(14) weight - HIGHEST!
  "w_mom": 0.8,
  "w_vwap": 1.6,
  "w_orb": 0.1,          // ORB weight - LOWEST (unreliable)
  "w_ofi": 1.1,
  "w_vol": 0.1,
  "win_rsi": 14,         // RSI window
  "warmup_bars": 50
}
```

**Performance**: 0.384% MRD (evaluation set)

---

## What Makes RSI "Anticipatory" vs Reactive?

### Current SIGOR RSI(14): REACTIVE
- Computes based on past 14 bars of price changes
- No forward-looking component
- Lag inherent in 14-bar window on 1-minute bars ≈ 14 minutes

### RSI(2) Connors: SEMI-ANTICIPATORY
- 2-bar window = ≈2 minute reaction time
- Still reactive to price action, but detects extremes faster
- **"Anticipatory" aspect**: Catches oversold/overbought before reversion occurs
- Threshold design (RSI < 10, RSI > 90) anticipates mean-reversion

### Truly Anticipatory Indicators (Not in SIGOR):
- **Williams %R** - Similar to RSI but different calculation (HIGH - LOW) / (52-week HIGH - LOW)
- **Predictive indicators**: Linear regression slope, MACD divergence
- **Machine learning predictions**: Logistic regression on feature set (used in ML pipeline)

---

## Performance Analysis

### Current Metrics
```
Test Date:    2025-10-24 (single day optimization)
Eval MRD:     +0.384%
Val MRD:      +0.327%
Degradation:  14.8% (from eval to validation)
Pass Rate:    80.5% (809 configs passed basic checks)
Trials:       868/2000 Optuna trials
```

### Why Not Reaching 0.5% MRD Target?

**Root Causes Identified**:

1. **Feature Engine Issue** (CRITICAL - resolved in code)
   - Earlier analysis found current version used ONLY normalized features
   - Baseline v2.0 (5.41% MRD) used RAW ABSOLUTE VALUES + normalized
   - Fix: Now includes 21 raw features + 34 normalized + 12 regime features
   - **Status**: Already fixed in current codebase ✅

2. **RSI Window Tuning**
   - Current: 14 bars (standard Wilder's)
   - Optimal may differ for intraday 1-minute bars
   - Optuna search space: [5..20] bars
   - **Hypothesis**: RSI(2) or RSI(3) might outperform RSI(14)

3. **RSI Weight Imbalance**
   - w_rsi = 1.8 is HIGHEST weight
   - But pure RSI(14) may not be optimal detector by itself
   - Suggestion: Test RSI(2) or hybrid RSI(2) + RSI(14)

4. **Missing Detector: Williams %R**
   - Not currently implemented
   - Different calculation: (HIGH - CLOSE) / (HIGH - LOW) over N bars
   - May capture different aspect than standard RSI
   - **Proposed addition**: Williams %R(14) as 8th detector

5. **Anticipatory Gap**
   - Current SIGOR is 100% reactive
   - All 7 detectors based on historical price action
   - ML pipeline adds predictive component, but only during live trading
   - **Improvement opportunity**: Add forward-looking signals

---

## Proposed Strategy: Integrating RSI(2) or Williams %R for 0.5% MRD

### Option 1: Replace RSI(14) with RSI(2)
```
Pro:
- Faster reaction time (2 min vs 14 min)
- Better catch of oversold/overbought extremes
- Already implemented and tested (RSI2Detector)

Con:
- Higher false positive rate (needs filters)
- Less stable in trending markets
- May require parameter re-optimization
```

### Option 2: Hybrid RSI - Use Both RSI(2) and RSI(14)
```
New 8-Detector SIGOR:
├── RSI(2) Connors - catches short-term extremes
├── RSI(14) - confirms longer-term momentum
├── (keep other 6 detectors)

Weighting suggestion:
w_rsi_2: 1.0-1.5   (new ultra-short term)
w_rsi_14: 1.0-1.2  (reduce from 1.8)
w_others: keep same
```

### Option 3: Add Williams %R
```
New 8th detector using Williams %R formula:
%R = (HIGH[n] - CLOSE) / (HIGH[n] - LOW[n])
where HIGH[n], LOW[n] are over lookback period

Similar to RSI but:
- Directly uses price extremes (not smoothed returns)
- May respond differently to gap moves
- Range [0..100], inverse of traditional %R (-100..0)
```

### Option 4: Predictive RSI Divergence
```
Not just current RSI, but:
- RSI(14) slope/momentum (is RSI accelerating?)
- RSI divergence (price makes new high, RSI doesn't)
- Bullish/bearish divergence patterns

This IS anticipatory because divergences precede reversals.
```

---

## Implementation Roadmap

### Phase 1: Baseline Measurement (2-3 days)
```
1. Measure current RSI(14) contribution to SIGOR
   - Run SIGOR with w_rsi = 0 (disable RSI)
   - Compare MRD vs baseline
   - Quantify RSI value: (baseline - no_rsi) / baseline × 100%

2. Test RSI(2) as standalone detector
   - Run 100 Optuna trials with RSI(2) replacing RSI(14)
   - Compare Eval MRD: RSI(2) vs RSI(14)
   
3. Analyze RSI extremes in historical data
   - What % of bars have RSI < 10 or > 90?
   - What % of those lead to profitable reversions?
```

### Phase 2: Integration (2-3 days)
```
1. Implement Williams %R if outperforms RSI(2)
2. Create 8-detector SIGOR with optimal RSI combination
3. Run Optuna 2000-trial optimization
   - Search space: w_rsi_2, w_rsi_14, window sizes
   - Objective: Minimize eval MRD, maximize Sharpe ratio
```

### Phase 3: Validation (1-2 days)
```
1. Walk-forward testing (avoid overfitting)
2. Stress test across different market regimes
3. Live paper trading validation
```

---

## Technical Implementation Details

### To Add RSI(2) to SIGOR:

**File**: `include/strategy/sigor_strategy.h`

Add to `SigorConfig`:
```cpp
int win_rsi_2 = 2;        // RSI(2) window
double w_rsi_2 = 1.0;     // RSI(2) weight
double rsi_2_oversold = 10.0;
double rsi_2_overbought = 90.0;
```

Add to `SigorStrategy` private methods:
```cpp
double prob_rsi_2_() const;  // New detector
```

**File**: `src/strategy/sigor_strategy.cpp`

In `generate_signal()`:
```cpp
double p2_rsi14 = prob_rsi_14_();     // Existing
double p2_rsi2 = prob_rsi_2_();       // New
double p2_combined = /* weighted blend */;

// Update aggregation
aggregate_probability(p1, p2_combined, p3, p4, p5, p6, p7);
```

In configuration `sigor_params.json`:
```json
{
  "win_rsi_2": 2,
  "w_rsi_2": 1.2,
  "rsi_2_oversold": 10.0,
  "rsi_2_overbought": 90.0
}
```

---

## Key Files for Integration

| File | Purpose | Status |
|------|---------|--------|
| `/Volumes/MyBookDuo/Projects/sentio_lite/include/strategy/sigor_strategy.h` | SIGOR interface | Modify to add RSI(2) |
| `/Volumes/MyBookDuo/Projects/sentio_lite/src/strategy/sigor_strategy.cpp` | Implementation | Add `prob_rsi_2_()` |
| `/Volumes/MyBookDuo/Projects/sentio_lite/tests/rsi2_detector.h` | RSI(2) detector | Already implemented ✅ |
| `/Volumes/MyBookDuo/Projects/sentio_lite/config/sigor_params.json` | Parameters | Update for optimization |
| `/Volumes/MyBookDuo/Projects/sentio_lite/src/predictor/feature_extractor.cpp` | ML features | Already has RSI-like ✅ |

---

## Conclusion

**There is NO separate Williams RSI strategy in sentio_lite**. The project uses SIGOR (7-detector ensemble) with RSI(14) as one component.

**To reach 0.5% MRD from current 0.384%** (30% improvement), the most promising approaches are:

1. **Add RSI(2) as 8th detector** - Already implemented, just needs integration
2. **Implement Williams %R** - Different calculation, potentially complementary
3. **Enable RSI divergence detection** - Truly anticipatory, requires additional code
4. **Optimize RSI window** - Test RSI(3), RSI(4), etc. via Optuna

**Recommendation**: Start with RSI(2) integration + Optuna re-optimization. This is lowest-risk, highest-probability improvement path, with 2-3 days to implementation and testing.

