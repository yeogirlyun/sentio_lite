# Proposed SIGOR Detectors - Analysis Framework

## Overview

This document outlines additional detector candidates for SIGOR based on "simple rules, strong edges" philosophy. Each detector has been implemented as a standalone test module to evaluate effectiveness before integration.

## Test Modules Created

### 1. TTM Squeeze/Expansion Detector
**File:** `tests/test_squeeze_expansion.cpp`

**Logic:**
- Bollinger Bands width < Keltner Channel width → squeeze state
- Track compression duration
- Signal on breakout from squeeze + retest for continuation

**Hypothesis:** Volatility compression followed by expansion provides directional edge

**Key Metrics to Evaluate:**
- Squeeze frequency and duration
- Breakout win rate
- False breakout rate
- Average move size after valid breakout
- Optimal minimum squeeze duration

**Integration Potential:** HIGH - Complements existing volatility-based detectors

---

### 2. Donchian/Prior-Day Breakout Detector
**File:** `tests/test_donchian_breakout.cpp`

**Logic:**
- Break above prior-day high or below prior-day low
- ATR-scaled filters to reduce noise
- Fade failed breakouts (reversion back into range)

**Hypothesis:** True breakouts continue; false breakouts provide counter-trend edge

**Key Metrics to Evaluate:**
- Breakout frequency
- True vs false breakout ratio
- Trend-follow win rate on confirmed breakouts
- Fade win rate on failed breakouts
- ATR filter noise reduction effectiveness

**Integration Potential:** MEDIUM-HIGH - ORB detector already exists; this adds failed-breakout fade logic

---

### 3. Ultra-Short RSI(2) / Connors Pullback
**File:** `tests/test_rsi2_pullback.cpp`

**Logic:**
- RSI(2) < 10 (oversold) or > 90 (overbought)
- Volume guardrail: minimum volume vs 20-bar average
- VWAP distance guardrail: max Z-score from VWAP

**Hypothesis:** Extreme short-term RSI readings offer mean-reversion edge when filtered

**Key Metrics to Evaluate:**
- Signal frequency
- Win rate on oversold (buy) signals
- Win rate on overbought (sell) signals
- Average reversion time
- Filter effectiveness (volume + VWAP distance)

**Integration Potential:** MEDIUM - RSI detector exists but uses RSI(14); RSI(2) is different timeframe

---

### 4. VWAP Bands Mean-Reversion
**File:** `tests/test_vwap_bands_reversion.cpp`

**Logic:**
- Enter at price = VWAP ± Z × VWAP_StdDev
- No-go zone: avoid counter-trend when price significantly away from multi-session VWAP
- Exit near VWAP (Z-score < threshold)

**Hypothesis:** Extreme intraday deviations revert, but respect multi-day trend bias

**Key Metrics to Evaluate:**
- Signal frequency at various Z thresholds
- Mean-reversion win rate
- Average reversion time
- No-go zone filter effectiveness
- Optimal entry/exit Z-scores

**Integration Potential:** HIGH - VWAP detector exists; this adds bands/std-dev logic

---

## Additional Proposed Detectors (Not Yet Implemented)

### 5. First Pullback Entry (ORB Enhancement)
**Logic:**
- After initial ORB breakout, wait for first pullback
- Enter on retest of breakout level with ATR/Keltner-based stop
- Targets: 2-3× ATR

**Integration:** Enhance existing ORB detector
**Complexity:** LOW - add pullback tracking to existing ORB

---

### 6. Cross-Sectional Intraday Rotation
**Logic:**
- Calculate 5-15min relative strength vs basket
- Enter only if symbol ranks in top N for strength
- Pair/inverse gating: TQQQ vs SQQQ based on detector consensus

**Integration:** Enhance existing rotation logic
**Complexity:** MEDIUM - requires cross-symbol comparison

---

### 7. Vol-of-Vol / Time-of-Day Risk Management
**Logic:**
- Reduce size during 9:30±5min and 15:55+
- Tighten stops when volatility-of-volatility spikes
- Time decay penalty for positions held across volatile periods

**Integration:** Risk management layer (not a detector)
**Complexity:** LOW - add time-based penalties to sizing

---

### 8. Kill-Switch / Regime Filter
**Logic:**
- After K losses within M bars, pause trading
- Resume when squeeze ends or volatility normalizes
- Time stop: close if no progress after X bars

**Integration:** Risk management layer
**Complexity:** LOW - add loss tracking and circuit breaker

---

## Testing Methodology

For each detector test module:

1. **Load Historical Data**
   - Use existing binary data files (data/*.bin)
   - Test across multiple dates and market conditions

2. **Backtesting Metrics**
   - Win rate (% profitable signals)
   - Average profit/loss per signal
   - Signal frequency (signals per day)
   - Sharpe ratio / risk-adjusted returns
   - Maximum drawdown

3. **Filter Effectiveness**
   - Compare filtered vs unfiltered signals
   - Measure false positive reduction
   - Evaluate parameter sensitivity

4. **Integration Analysis**
   - Correlation with existing detectors
   - Complementary vs redundant signals
   - Portfolio-level impact

## Next Steps

1. **Implement remaining test modules** (detectors 5-8)

2. **Run comprehensive backtests** on historical data
   ```bash
   # Compile and run each test
   g++ -std=c++17 tests/test_squeeze_expansion.cpp -o tests/test_squeeze
   ./tests/test_squeeze --data data/ --start-date 2024-01-01 --end-date 2024-10-24
   ```

3. **Comparative analysis**
   - Rank detectors by Sharpe ratio
   - Identify best combinations
   - Determine optimal fusion weights

4. **Parameter optimization**
   - Grid search for each detector's parameters
   - Walk-forward validation to avoid overfitting

5. **Integration decision**
   - Select top 2-3 detectors based on:
     - Uncorrelated with existing detectors
     - Robust across market regimes
     - Simple implementation (maintainability)

## Evaluation Criteria

**Must Pass:**
- Win rate > 52% on out-of-sample data
- Sharpe ratio > 1.0
- Max drawdown < 15%
- Statistically significant edge (p < 0.05)

**Nice to Have:**
- Low correlation (<0.5) with existing detectors
- Consistent across different vol regimes
- Clear economic rationale

## Risk Considerations

- **Overfitting:** Use walk-forward validation
- **Data snooping:** Test on held-out data
- **Regime change:** Verify robustness in different market conditions
- **Implementation cost:** Consider slippage and latency for HFT-style signals

---

*Status: Test framework created, awaiting historical data runs*
*Last Updated: 2025-10-24*
