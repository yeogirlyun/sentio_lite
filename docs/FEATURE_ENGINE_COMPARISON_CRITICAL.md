# CRITICAL: Feature Engine Comparison - Current vs Baseline v2.0

**Date**: 2025-10-16
**Baseline Commit**: `fe1e952` (Rotation Trading System v2.0 - 5.41% MRD Performance)
**Issue**: Current feature engine uses ONLY normalized/ratio features, while baseline uses raw absolute values + ratios

---

## EXECUTIVE SUMMARY

**ROOT CAUSE IDENTIFIED**: The current feature engine has been fundamentally changed from the baseline v2.0 that achieved 5.41% MRD performance. The current version uses **ONLY normalized/ratio-based features**, while the baseline used **RAW ABSOLUTE VALUES plus some normalized features**.

### Critical Differences:

| Feature Category | Baseline v2.0 (5.41% MRD) | Current Version | Impact |
|------------------|---------------------------|-----------------|--------|
| **Core Price** | `price.close`, `price.open`, `price.high`, `price.low` | `price.range_ratio`, `price.body_ratio`, `price.upper_wick_ratio`, `price.lower_wick_ratio` | **SEVERE** - Lost absolute price information |
| **Moving Averages** | RAW values (`sma10`, `sma20`, etc.) + deviations | ONLY deviations (`sma10_dev`, `sma20_dev`) | **SEVERE** - Lost MA trend information |
| **Volatility** | RAW (`atr14`, `bb20.mean`, `bb20.sd`, `bb20.upper`, `bb20.lower`) + normalized | ONLY normalized (`atr14_pct`, `bb20.mean_dev`, `bb20.sd_pct`) | **SEVERE** - Lost absolute volatility scale |
| **Volume** | RAW (`obv`, `vwap`) + normalized (`vwap_dist`) | ONLY normalized (`obv_scaled`, `vwap_dist`) | **CRITICAL** - Lost volume magnitude information |
| **Donchian** | RAW (`don20.up`, `don20.mid`, `don20.dn`) + position | ONLY deviations (`don20.up_dev`, `don20.mid_dev`, `don20.dn_dev`) | **SEVERE** - Lost channel width information |
| **Feature Count** | **~65 features** (time + raw + normalized) | **~50 features** (time + normalized only) | **CRITICAL** - 23% fewer features |

---

## DETAILED FEATURE-BY-FEATURE COMPARISON

### 1. Core Price Features

#### BASELINE v2.0 (Lines 80-86):
```cpp
// Core price/volume features (always included)
n.push_back("price.close");      // ABSOLUTE VALUE
n.push_back("price.open");       // ABSOLUTE VALUE
n.push_back("price.high");       // ABSOLUTE VALUE
n.push_back("price.low");        // ABSOLUTE VALUE
n.push_back("price.return_1");   // NORMALIZED (same in both)
n.push_back("volume.raw");       // ABSOLUTE VALUE
```

**Values in recompute_vector_()** (Lines 270-276):
```cpp
feats_[k++] = prevClose_;   // Raw price (e.g., 450.25)
feats_[k++] = prevOpen_;    // Raw price (e.g., 449.80)
feats_[k++] = prevHigh_;    // Raw price (e.g., 451.00)
feats_[k++] = prevLow_;     // Raw price (e.g., 449.50)
feats_[k++] = safe_return(prevClose_, prevPrevClose_);  // Return
feats_[k++] = prevVolume_;  // Raw volume (e.g., 1,250,000)
```

#### CURRENT VERSION (Lines 82-90):
```cpp
// Core price/volume features (NORMALIZED - always included)
n.push_back("price.range_ratio");      // (high - low) / close
n.push_back("price.body_ratio");       // (close - open) / close
n.push_back("price.upper_wick_ratio"); // (high - close) / close
n.push_back("price.lower_wick_ratio"); // (close - low) / close
n.push_back("price.return_1");         // Same
n.push_back("volume.change_ratio");    // volume change vs previous
```

**Values in recompute_vector_()** (Lines 288-308):
```cpp
double range = prevHigh_ - prevLow_;
feats_[k++] = (prevClose_ != 0) ? range / prevClose_ : 0.0;  // Ratio (e.g., 0.0033 = 0.33%)
feats_[k++] = (prevClose_ != 0) ? (prevClose_ - prevOpen_) / prevClose_ : 0.0;  // Ratio
feats_[k++] = (prevClose_ != 0) ? (prevHigh_ - prevClose_) / prevClose_ : 0.0;  // Ratio
feats_[k++] = (prevClose_ != 0) ? (prevClose_ - prevLow_) / prevClose_ : 0.0;  // Ratio
feats_[k++] = safe_return(prevClose_, prevPrevClose_);
feats_[k++] = (!std::isnan(prevPrevVolume_) && prevPrevVolume_ > 0)
              ? (prevVolume_ / prevPrevVolume_) - 1.0
              : 0.0;  // Volume change ratio
```

**IMPACT**:
- **LOST**: Absolute price levels (critical for price-dependent strategies)
- **LOST**: Absolute volume magnitude (critical for volume analysis)
- **LOST**: Ability to distinguish between $50 stock vs $500 stock behavior

---

### 2. Moving Averages

#### BASELINE v2.0 (Lines 88-96):
```cpp
// Moving Averages (always included for baseline)
n.push_back("sma10");           // ABSOLUTE VALUE
n.push_back("sma20");           // ABSOLUTE VALUE
n.push_back("sma50");           // ABSOLUTE VALUE
n.push_back("ema10");           // ABSOLUTE VALUE
n.push_back("ema20");           // ABSOLUTE VALUE
n.push_back("ema50");           // ABSOLUTE VALUE
n.push_back("price_vs_sma20");  // (close - sma20) / sma20 (RATIO)
n.push_back("price_vs_ema20");  // (close - ema20) / ema20 (RATIO)
```

**Values in recompute_vector_()** (Lines 286-300):
```cpp
double sma10 = sma10_ring_.full() ? sma10_ring_.mean() : NaN;
double sma20 = sma20_ring_.full() ? sma20_ring_.mean() : NaN;
double sma50 = sma50_ring_.full() ? sma50_ring_.mean() : NaN;
double ema10 = ema10_.get_value();
double ema20 = ema20_.get_value();
double ema50 = ema50_.get_value();

feats_[k++] = sma10;  // Raw MA (e.g., 448.50)
feats_[k++] = sma20;  // Raw MA (e.g., 445.20)
feats_[k++] = sma50;  // Raw MA (e.g., 442.80)
feats_[k++] = ema10;  // Raw EMA
feats_[k++] = ema20;  // Raw EMA
feats_[k++] = ema50;  // Raw EMA

// Price vs MA ratios
feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : NaN;
feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : NaN;
```

#### CURRENT VERSION (Lines 92-101):
```cpp
// Moving Averages (DEVIATION RATIOS - always included for baseline)
n.push_back("sma10_dev");       // (close - sma10) / sma10
n.push_back("sma20_dev");       // (close - sma20) / sma20
n.push_back("sma50_dev");       // (close - sma50) / sma50
n.push_back("ema10_dev");       // (close - ema10) / ema10
n.push_back("ema20_dev");       // (close - ema20) / ema20
n.push_back("ema50_dev");       // (close - ema50) / ema50
n.push_back("price_vs_sma20");  // (close - sma20) / sma20 (duplicate)
n.push_back("price_vs_ema20");  // (close - ema20) / ema20 (duplicate)
```

**Values in recompute_vector_()** (Lines 313-330):
```cpp
double sma10 = sma10_ring_.full() ? sma10_ring_.mean() : NaN;
double sma20 = sma20_ring_.full() ? sma20_ring_.mean() : NaN;
double sma50 = sma50_ring_.full() ? sma50_ring_.mean() : NaN;
double ema10 = ema10_.get_value();
double ema20 = ema20_.get_value();
double ema50 = ema50_.get_value();

// ONLY DEVIATIONS - NO RAW VALUES!
feats_[k++] = (!std::isnan(sma10) && sma10 != 0) ? (prevClose_ - sma10) / sma10 : 0.0;
feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : 0.0;
feats_[k++] = (!std::isnan(sma50) && sma50 != 0) ? (prevClose_ - sma50) / sma50 : 0.0;
feats_[k++] = (!std::isnan(ema10) && ema10 != 0) ? (prevClose_ - ema10) / ema10 : 0.0;
feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : 0.0;
feats_[k++] = (!std::isnan(ema50) && ema50 != 0) ? (prevClose_ - ema50) / ema50 : 0.0;

feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : NaN;
feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : NaN;
```

**IMPACT**:
- **LOST**: Absolute MA values (critical for trend strength and direction)
- **LOST**: MA convergence/divergence patterns (MA crossovers)
- **LOST**: Ability to detect strong vs weak trends based on MA slope

---

### 3. Volatility Features

#### BASELINE v2.0 (Lines 98-116):
```cpp
if (cfg_.volatility) {
    n.push_back("atr14");             // ABSOLUTE VALUE
    n.push_back("atr14_pct");         // ATR / close (RATIO)
    n.push_back("bb20.mean");         // ABSOLUTE VALUE
    n.push_back("bb20.sd");           // ABSOLUTE VALUE
    n.push_back("bb20.upper");        // ABSOLUTE VALUE
    n.push_back("bb20.lower");        // ABSOLUTE VALUE
    n.push_back("bb20.percent_b");    // RATIO (0-1)
    n.push_back("bb20.bandwidth");    // RATIO
    n.push_back("keltner.middle");    // ABSOLUTE VALUE
    n.push_back("keltner.upper");     // ABSOLUTE VALUE
    n.push_back("keltner.lower");     // ABSOLUTE VALUE
}
```

**Values in recompute_vector_()** (Lines 315-336):
```cpp
feats_[k++] = atr14_.value;  // Raw ATR (e.g., 2.50)
feats_[k++] = (prevClose_ != 0 && !std::isnan(atr14_.value)) ? atr14_.value / prevClose_ : NaN;

feats_[k++] = bb20_.mean;   // Raw BB mean (e.g., 445.20)
feats_[k++] = bb20_.sd;     // Raw BB std dev (e.g., 3.80)
feats_[k++] = bb20_.upper;  // Raw BB upper (e.g., 452.80)
feats_[k++] = bb20_.lower;  // Raw BB lower (e.g., 437.60)
feats_[k++] = bb20_.percent_b;
feats_[k++] = bb20_.bandwidth;

feats_[k++] = keltner_.middle;  // Raw Keltner middle
feats_[k++] = keltner_.upper;   // Raw Keltner upper
feats_[k++] = keltner_.lower;   // Raw Keltner lower
```

#### CURRENT VERSION (Lines 104-117):
```cpp
if (cfg_.volatility) {
    n.push_back("atr14_pct");              // ATR / close
    n.push_back("bb20.mean_dev");          // (close - bb_mean) / close
    n.push_back("bb20.sd_pct");            // bb_sd / close
    n.push_back("bb20.upper_dev");         // (close - bb_upper) / close
    n.push_back("bb20.lower_dev");         // (close - bb_lower) / close
    n.push_back("bb20.percent_b");         // Already ratio
    n.push_back("bb20.bandwidth");         // Already ratio
    n.push_back("keltner.middle_dev");     // (close - keltner_mid) / close
    n.push_back("keltner.upper_dev");      // (close - keltner_up) / close
    n.push_back("keltner.lower_dev");      // (close - keltner_dn) / close
}
```

**Values in recompute_vector_()** (Lines 335-385):
```cpp
// ONLY NORMALIZED - NO RAW VALUES!
feats_[k++] = (prevClose_ != 0 && !std::isnan(atr14_.value)) ? atr14_.value / prevClose_ : NaN;

feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.mean)) ? (prevClose_ - bb20_.mean) / prevClose_ : NaN;
feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.sd)) ? bb20_.sd / prevClose_ : NaN;
feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.upper)) ? (prevClose_ - bb20_.upper) / prevClose_ : NaN;
feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.lower)) ? (prevClose_ - bb20_.lower) / prevClose_ : NaN;
feats_[k++] = bb20_.percent_b;
feats_[k++] = bb20_.bandwidth;

feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.middle)) ? (prevClose_ - keltner_.middle) / prevClose_ : NaN;
feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.upper)) ? (prevClose_ - keltner_.upper) / prevClose_ : NaN;
feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.lower)) ? (prevClose_ - keltner_.lower) / prevClose_ : NaN;
```

**IMPACT**:
- **LOST**: Absolute ATR (critical for position sizing and risk management)
- **LOST**: Absolute BB band widths (critical for volatility regime detection)
- **LOST**: Ability to compare volatility across different price levels

---

### 4. Volume Features

#### BASELINE v2.0 (Lines 118-122):
```cpp
if (cfg_.volume) {
    n.push_back("obv");        // ABSOLUTE VALUE
    n.push_back("vwap");       // ABSOLUTE VALUE
    n.push_back("vwap_dist");  // (close - vwap) / vwap (RATIO)
}
```

**Values in recompute_vector_()** (Lines 341-348):
```cpp
feats_[k++] = obv_.value;   // Raw OBV (e.g., 15,234,500)
feats_[k++] = vwap_.value;  // Raw VWAP (e.g., 448.75)
double vwap_dist = (!std::isnan(vwap_.value) && vwap_.value != 0)
                   ? (prevClose_ - vwap_.value) / vwap_.value
                   : NaN;
feats_[k++] = vwap_dist;
```

#### CURRENT VERSION (Lines 139-144):
```cpp
if (cfg_.volume) {
    n.push_back("obv_scaled");       // OBV / (close * 1M)
    n.push_back("vwap_dist");        // (close - vwap) / vwap
}
```

**Values in recompute_vector_()** (Lines 409-418):
```cpp
// ONLY NORMALIZED - NO RAW OBV OR VWAP!
feats_[k++] = (prevClose_ != 0 && !std::isnan(obv_.value)) ? obv_.value / (prevClose_ * 1000000.0) : NaN;

double vwap_dist = (!std::isnan(vwap_.value) && vwap_.value != 0)
                   ? (prevClose_ - vwap_.value) / vwap_.value
                   : NaN;
feats_[k++] = vwap_dist;
```

**IMPACT**:
- **LOST**: Raw OBV accumulation (critical for volume trend analysis)
- **LOST**: Raw VWAP (critical for institutional order flow detection)
- **LOST**: Ability to compare volume patterns across different symbols

---

### 5. Donchian Channels

#### BASELINE v2.0 (Lines 124-128):
```cpp
// Donchian Channels (pattern/breakout detection)
n.push_back("don20.up");        // ABSOLUTE VALUE
n.push_back("don20.mid");       // ABSOLUTE VALUE
n.push_back("don20.dn");        // ABSOLUTE VALUE
n.push_back("don20.position");  // (close - dn) / (up - dn) (RATIO)
```

**Values in recompute_vector_()** (Lines 350-360):
```cpp
feats_[k++] = don20_.up;   // Raw upper (e.g., 455.00)
feats_[k++] = don20_.mid;  // Raw middle (e.g., 445.00)
feats_[k++] = don20_.dn;   // Raw lower (e.g., 435.00)

double don_range = don20_.up - don20_.dn;
double don_pos = (don_range != 0 && !std::isnan(don20_.up) && !std::isnan(don20_.dn))
                 ? (prevClose_ - don20_.dn) / don_range
                 : NaN;
feats_[k++] = don_pos;
```

#### CURRENT VERSION (Lines 147-152):
```cpp
// Donchian Channels (NORMALIZED as deviations)
n.push_back("don20.up_dev");         // (close - don_up) / close
n.push_back("don20.mid_dev");        // (close - don_mid) / close
n.push_back("don20.dn_dev");         // (close - don_dn) / close
n.push_back("don20.position");       // Already ratio
```

**Values in recompute_vector_()** (Lines 421-432):
```cpp
// ONLY NORMALIZED - NO RAW DONCHIAN VALUES!
feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.up)) ? (prevClose_ - don20_.up) / prevClose_ : NaN;
feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.mid)) ? (prevClose_ - don20_.mid) / prevClose_ : NaN;
feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.dn)) ? (prevClose_ - don20_.dn) / prevClose_ : NaN;

double don_range = don20_.up - don20_.dn;
double don_pos = (don_range != 0 && !std::isnan(don20_.up) && !std::isnan(don20_.dn))
                 ? (prevClose_ - don20_.dn) / don_range
                 : NaN;
feats_[k++] = don_pos;
```

**IMPACT**:
- **LOST**: Absolute channel width (critical for breakout detection)
- **LOST**: Ability to detect expanding/contracting volatility regimes
- **LOST**: Absolute position levels for support/resistance

---

### 6. Momentum Features (SAME in both versions)

Both versions have identical momentum features:
- `rsi14`, `rsi21`
- `stoch14.k`, `stoch14.d`, `stoch14.slow`
- `will14`
- `macd.line`, `macd.signal`, `macd.hist`
- `roc5`, `roc10`, `roc20`
- `cci20`

**NO DIFFERENCES** in momentum features.

---

### 7. Candlestick Patterns (SAME in both versions)

Both versions have identical pattern detection:
- `pattern.doji`
- `pattern.hammer`
- `pattern.shooting_star`
- `pattern.engulfing_bull`
- `pattern.engulfing_bear`

**NO DIFFERENCES** in pattern features.

---

## SCHEMA HASH DIFFERENCES

Due to the different feature sets, the schema hashes will be **INCOMPATIBLE**:

- **Baseline v2.0**: Hash computed from ~65 features including raw values
- **Current Version**: Hash computed from ~50 features with only normalized values

This means:
1. **Models trained on baseline cannot be used with current feature engine**
2. **Any saved model states will be rejected due to hash mismatch**
3. **Performance cannot be directly compared** due to different input spaces

---

## WHY THIS MATTERS: MACHINE LEARNING PERSPECTIVE

### 1. Information Loss
**Raw values contain information that cannot be recovered from ratios alone:**

Example: Two scenarios with identical ratios but different absolute values:
- Symbol A: close=$50, high=$51, low=$49 → range_ratio = 4%
- Symbol B: close=$500, high=$510, low=$490 → range_ratio = 4%

The **EWRLS predictor** sees identical ratios but these are fundamentally different market conditions:
- Symbol A: $1 range = high volatility for $50 stock
- Symbol B: $10 range = low volatility for $500 stock

**The baseline model could distinguish these. The current model cannot.**

### 2. Feature Interactions
Machine learning models learn complex interactions between features. With raw values:

**Baseline could learn**: "When close > sma20 AND sma20 - sma50 > 5.0, BUY"
- This captures both direction AND trend strength

**Current can only learn**: "When sma20_dev > 0 AND sma50_dev > 0, BUY"
- This only captures direction, NOT strength

### 3. Non-linear Relationships
EWRLS is a **linear model**, but it can learn non-linear relationships through:
1. **Feature combinations**: close * atr14 (price-scaled risk)
2. **Feature squares**: (close - sma20)^2 (quadratic deviation)

**With only ratios, these interactions are severely limited.**

### 4. Leverage-Specific Behavior
For leveraged ETFs (TQQQ, SQQQ, etc.), absolute price changes matter:
- TQQQ at $50 moving $2 = 4% (3x leveraged move)
- TQQQ at $20 moving $2 = 10% (same absolute move, different ratio)

**Baseline captures both absolute and relative moves. Current only captures relative.**

---

## PERFORMANCE IMPACT HYPOTHESIS

**Baseline v2.0 Performance**: 5.41% MRD (October 2024, 12 symbols)
**Current Performance**: Unknown (but likely degraded)

### Why Performance Likely Degraded:

1. **Lost Signal Strength Information**: Ratios normalize away the magnitude of moves
2. **Lost Cross-Symbol Comparability**: Can't compare $50 stock vs $500 stock behavior
3. **Lost Volatility Regime Detection**: Can't distinguish high-vol vs low-vol periods by absolute measures
4. **Lost Volume Pattern Recognition**: Can't detect institutional accumulation/distribution
5. **Reduced Feature Space**: 23% fewer features = less information for EWRLS to learn from

---

## RECOMMENDATION

### Option 1: REVERT TO BASELINE (RECOMMENDED)
**Action**: Revert `src/features/unified_feature_engine.cpp` to baseline version from commit `fe1e952`

**Pros**:
- Restores proven 5.41% MRD performance
- Maintains compatibility with baseline models
- Preserves all information content

**Cons**:
- None (this is a strict improvement)

**Implementation**:
```bash
# Revert feature engine to baseline
git show fe1e952:src/features/unified_feature_engine.cpp > src/features/unified_feature_engine.cpp

# Rebuild
make clean && make
```

### Option 2: HYBRID APPROACH (NOT RECOMMENDED)
Add normalized features to baseline (both raw + normalized)

**Pros**:
- Maximum feature coverage
- Potentially better performance

**Cons**:
- Increases feature dimensionality (~80 features)
- May cause overfitting
- Requires retraining and validation

---

## NEXT STEPS

1. **Immediate**: Confirm with user whether to revert to baseline
2. **After revert**: Run 10-day backtest to verify performance restoration
3. **Document**: Create git commit documenting the reversion
4. **Monitor**: Compare live trading performance with baseline expectations

---

## TECHNICAL NOTES

### Baseline Feature Count:
- Time: 8 features (if enabled)
- Core price: 6 features (raw OHLC + return + volume)
- Moving averages: 8 features (6 raw + 2 ratios)
- Volatility: 12 features (8 raw + 4 ratios)
- Momentum: 13 features (same as current)
- Volume: 3 features (2 raw + 1 ratio)
- Donchian: 4 features (3 raw + 1 ratio)
- Patterns: 5 features (same as current)
**Total**: ~59 features (with all flags enabled)

### Current Feature Count:
- Time: 8 features (if enabled)
- Core price: 6 features (all ratios)
- Moving averages: 8 features (all ratios)
- Volatility: 10 features (all ratios)
- Momentum: 13 features (same as baseline)
- Volume: 2 features (all ratios)
- Donchian: 4 features (all ratios)
- Patterns: 5 features (same as baseline)
**Total**: ~56 features (with all flags enabled)

### Feature Schema Hash Impact:
The schema hash is computed from feature names + config parameters. Since feature names are completely different, the hashes will be incompatible. This means:
- Any saved EWRLS model states from baseline will be rejected
- Any calibration data from baseline will be invalidated
- Any performance metrics from baseline cannot be directly compared

---

**Generated**: 2025-10-16 by Claude Code
