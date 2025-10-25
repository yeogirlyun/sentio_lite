# Requirements Document: Achieving 0.5% MRD Target

**Document Version:** 1.0
**Date:** 2025-10-25
**Status:** DRAFT
**Target:** Mean Return per Day (MRD) ≥ 0.5%
**Current Performance:** ~0.334% MRD (Evaluation), ~0.504% MRD (Validation)

---

## Executive Summary

This document outlines the requirements and strategic direction for improving the SIGOR (Signal-OR) trading system to consistently achieve a Mean Return per Day (MRD) of 0.5% or higher. We currently achieve 0.334% MRD on evaluation data and 0.504% MRD on validation data through a 7-detector rule-based ensemble with walk-forward validation. This document analyzes our current approach, identifies performance gaps, and proposes concrete pathways to reach the 0.5% MRD target on both evaluation and validation sets.

**Key Achievement:** We have demonstrated the ability to exceed 0.5% MRD on validation data (0.504%), proving the target is achievable. The challenge is to close the evaluation-validation gap and consistently achieve 0.5%+ across both sets.

---

## 1. Current System Architecture

### 1.1 SIGOR Strategy Overview

SIGOR is a **rule-based ensemble** that combines 7 technical detectors using log-odds fusion:

1. **Bollinger Z-Score** - Mean reversion detector
2. **RSI(14)** - Momentum oscillator
3. **Momentum** - Price rate-of-change
4. **VWAP Reversion** - Volume-weighted price deviation
5. **Opening Range Breakout (ORB)** - Daily breakout detector
6. **Order Flow Imbalance (OFI)** - Proxy using bar geometry
7. **Volume Surge** - Volume anomaly scaled by momentum direction

Each detector outputs a probability [0,1] representing directional bias. These are aggregated via weighted log-odds fusion with a sharpness parameter `k`.

### 1.2 Multi-Symbol Rotation Framework

- **Universe:** 12 leveraged ETFs (TQQQ, SQQQ, TNA, TZA, UVXY, SVIX, FAS, FAZ, SPXL, SPXS, SOXL, SOXS)
- **Position Limit:** Top 2 positions based on signal strength
- **Holding Period:** Minimum 6 bars (prevents churning)
- **Capital:** $100,000 initial capital
- **Timeframe:** 1-minute bars, RTH only (391 bars/day)

### 1.3 Optimization Methodology

**Walk-Forward Validation Framework:**
- **Training Window:** 5 days (5 × 391 = 1,955 bars)
- **Evaluation Set:** Next 5 days after training
- **Validation Set:** 5 days after evaluation (out-of-sample)
- **Optimizer:** Optuna TPE (Tree-structured Parzen Estimator)
- **Trials:** 200 trials (current), targeting 2,000 trials
- **Objective:** Maximize evaluation MRD with validation pass rate ≥ 70%

### 1.4 Performance Metrics

**Current Best Configuration (Trial 112, 200-trial optimization):**

| Metric | Evaluation Set | Validation Set |
|--------|---------------|----------------|
| **MRD** | +0.334% | +0.504% |
| **Pass Rate** | 100% (5/5 days) | 80% (4/5 days) |
| **Overall Pass Rate** | 88.5% | - |

**Parameter Configuration:**
```json
{
  "k": 1.8,
  "w_boll": 0.293, "w_rsi": 0.442, "w_mom": 1.445,
  "w_vwap": 0.814, "w_orb": 1.382, "w_ofi": 1.722, "w_vol": 1.895,
  "win_boll": 23, "win_rsi": 7, "win_mom": 6,
  "win_vwap": 12, "orb_opening_bars": 48, "vol_window": 12
}
```

---

## 2. What We Did: Journey to 0.334% MRD

### 2.1 System Evolution

**Phase 1: Initial SIGOR Implementation**
- Implemented 7-detector ensemble with equal weights
- Basic log-odds fusion without optimization
- Initial performance: ~0.15-0.20% MRD

**Phase 2: Optuna Optimization Infrastructure**
- Built walk-forward validation framework (`scripts/optimize_with_validation.py`)
- Implemented 5-day training, 5-day eval, 5-day validation splits
- Added pass rate constraints (≥70% validation pass rate)
- Performance improved to: ~0.25-0.30% MRD

**Phase 3: Detector Weight Optimization**
- Expanded search space to optimize detector weights (w_boll, w_rsi, etc.)
- Optimized window parameters for each detector
- Key finding: OFI (1.722) and Volume (1.895) had highest optimal weights
- Performance improved to: **0.334% MRD (eval), 0.504% MRD (val)**

**Phase 4: Detector Exploration (Failed Experiments)**
- **Williams %R (8th detector):** Degraded performance to 0.180% MRD (-46%)
  - Root cause: Redundancy with existing momentum detectors
  - Action: Completely removed from codebase
- **Bollinger Amplification:** Attempted to boost mean reversion signals
  - Result: Marginal improvement, increased complexity
  - Action: Removed for simplicity

### 2.2 Key Success Factors

1. **Walk-Forward Validation:** Prevents overfitting by enforcing out-of-sample validation
2. **Weighted Fusion:** Allows optimization to emphasize reliable detectors
3. **Multi-Symbol Rotation:** Diversification across 12 ETFs reduces idiosyncratic risk
4. **Holding Period Constraint:** Minimum 6-bar hold prevents churning losses
5. **Pass Rate Constraint:** Ensures consistency across multiple days

### 2.3 Current Limitations

1. **Evaluation-Validation Gap:** 0.334% vs 0.504% indicates potential overfitting or regime differences
2. **Sample Size:** 200 trials may be insufficient to explore 13-dimensional parameter space
3. **Fixed Detector Set:** Current 7 detectors may not capture all market regimes
4. **Window Constraints:** Fixed search ranges may exclude optimal windows
5. **No Regime Adaptation:** Single parameter set for all market conditions

---

## 3. Requirements: Pathways to 0.5% MRD

### 3.1 Primary Requirement: 2,000-Trial Optimization

**Objective:** Expand search space exploration to find better parameter combinations.

**Rationale:**
- Current 200 trials provide ~15 samples per parameter dimension (200/13 ≈ 15)
- 2,000 trials would provide ~154 samples per dimension (2,000/13 ≈ 154)
- TPE algorithm benefits from larger trial counts for hyperparameter optimization
- Validation MRD of 0.504% suggests better configurations exist

**Implementation:**
```bash
python3 scripts/optimize_with_validation.py \
  --end-date 10-24 \
  --trials 2000 \
  --output results/validated_optimization_2000.json \
  2>&1 | tee logs/validated_optimization_2000.log
```

**Expected Outcome:**
- Target: ≥0.45% MRD on evaluation set
- Constraint: ≥70% validation pass rate maintained
- Timeline: ~8-12 hours on current hardware

**Success Criteria:**
- [ ] Evaluation MRD ≥ 0.45%
- [ ] Validation MRD ≥ 0.50%
- [ ] Pass rate ≥ 70% on validation set
- [ ] No catastrophic failures (MRD < 0%)

### 3.2 Secondary Requirements: Optimization Enhancements

#### 3.2.1 Expanded Search Space

**Current Constraints:**
```python
# scripts/optimize_with_validation.py (approximate ranges)
k: [1.0, 3.0]
w_*: [0.1, 2.0]  # Detector weights
win_*: [5, 30]    # Window parameters
```

**Proposed Enhancements:**
1. **Detector Weight Range:** Expand to [0.05, 3.0] to allow near-zero weights for weak detectors
2. **Window Range Extension:**
   - Bollinger: [10, 40] (capture longer mean reversion cycles)
   - ORB Opening Bars: [20, 78] (test first 20min to 2hr of day)
3. **Sharpness Parameter:** Test [0.5, 4.0] for more aggressive/conservative fusion

**Implementation Impact:**
- Allows optimization to discover detector combinations we haven't tested
- Risk: Larger search space may require >2,000 trials for convergence

#### 3.2.2 Multi-Objective Optimization

**Current Objective:** Maximize evaluation MRD only

**Proposed Multi-Objective:**
```python
def objective(trial):
    eval_mrd = compute_mrd(eval_set)
    val_mrd = compute_mrd(val_set)

    # Penalize eval-val gap to reduce overfitting
    gap_penalty = abs(eval_mrd - val_mrd) * 0.2

    # Reward consistency
    eval_pass_rate = sum(daily_returns > 0) / len(daily_returns)

    return eval_mrd - gap_penalty + 0.1 * eval_pass_rate
```

**Benefits:**
- Directly optimizes for generalization (smaller eval-val gap)
- Encourages consistent daily performance
- May sacrifice peak eval MRD for better overall robustness

#### 3.2.3 Regime-Adaptive Parameters

**Problem:** Single parameter set for all market conditions (trending, mean-reverting, volatile)

**Proposed Solution:** Regime detection + conditional parameters

**Regime Detection:**
- **Trending Regime:** ADX > 25, strong directional momentum
- **Mean Reverting:** Bollinger %B oscillating, low ADX
- **High Volatility:** VIX > 20, large intraday ranges

**Implementation:**
```cpp
// In SigorStrategy::generate_signal()
MarketRegime regime = detect_regime(market_data);

if (regime == TRENDING) {
    // Boost momentum, ORB weights
    config_adjusted.w_mom *= 1.5;
    config_adjusted.w_orb *= 1.5;
} else if (regime == MEAN_REVERTING) {
    // Boost Bollinger, VWAP weights
    config_adjusted.w_boll *= 1.5;
    config_adjusted.w_vwap *= 1.5;
}
```

**Benefits:**
- Adapts to changing market conditions
- May improve consistency across diverse market regimes
- Could close eval-val gap if sets have different regime distributions

**Challenges:**
- Requires regime detection logic (complexity)
- Optimization becomes 2-3x larger (regime-specific parameters)

### 3.3 Tertiary Requirements: Position Sizing & Risk Management

#### 3.3.1 Dynamic Position Sizing

**Current:** Fixed 50% equity per position (max 2 positions = 100% utilization)

**Proposed:** Confidence-weighted sizing

```cpp
double position_size = base_size * signal.confidence;

// Example:
// confidence = 0.9 → 45% equity
// confidence = 0.6 → 30% equity
```

**Benefits:**
- Reduces risk on weak signals
- Amplifies strong signals
- May improve risk-adjusted returns

#### 3.3.2 Stop-Loss & Profit Target Optimization

**Current:** No intraday stops (positions held until rotation or EOD)

**Proposed:** Optimize stop-loss and profit targets

**Search Space:**
```python
stop_loss_pct: [0.005, 0.02]      # 0.5% - 2.0%
profit_target_pct: [0.01, 0.05]   # 1.0% - 5.0%
```

**Rationale:**
- Leveraged ETFs have high intraday volatility
- Profit targets could lock in gains on strong moves
- Stops could limit losses on failed signals

**Risk:**
- May increase trade frequency (more costs)
- Could exit winning positions too early

### 3.4 Quaternary Requirements: Data & Features

#### 3.4.1 Extended Training Window

**Current:** 5-day training (1,955 bars)

**Proposed:** Test 10-day and 20-day training windows

**Rationale:**
- Longer training may capture more market regimes
- Detectors with long windows (Bollinger=23) may need more history
- Trade-off: Older data may be less relevant

**Implementation:**
```bash
python3 scripts/optimize_with_validation.py \
  --training-days 10 \
  --eval-days 5 \
  --val-days 5
```

#### 3.4.2 Additional Detectors (Carefully Vetted)

**Candidates for Future Testing:**
1. **ATR-Based Volatility Filter:** Exit positions when volatility spikes
2. **Time-of-Day Bias:** Weight detectors by time (e.g., boost ORB in first hour)
3. **Cross-Symbol Correlation:** Detect sector rotation (e.g., TQQQ up, SQQQ down)

**Vetting Process:**
1. Implement detector in isolation
2. Backtest on 20+ days
3. Measure correlation with existing detectors
4. Only add if correlation < 0.7 and standalone MRD > 0.2%

**Note:** Williams %R failed because it was redundant with Momentum detector. All new detectors must provide **orthogonal information**.

---

## 4. Implementation Plan

### Phase 1: Immediate (Week 1)
- [x] Complete current 200-trial optimization baseline
- [ ] Launch 2,000-trial optimization with current search space
- [ ] Monitor optimization progress, verify no crashes
- [ ] Document top 10 trials for analysis

### Phase 2: Analysis (Week 2)
- [ ] Analyze 2,000-trial results vs 200-trial baseline
- [ ] Identify parameter patterns (which weights converge, which vary)
- [ ] Measure eval-val gap across trials
- [ ] Determine if 0.5% MRD target achieved

### Phase 3: Enhancement (Week 3-4, if needed)
- [ ] If 0.5% not achieved, implement expanded search space (§3.2.1)
- [ ] Test multi-objective optimization (§3.2.2)
- [ ] Implement regime detection prototype (§3.2.3)
- [ ] Run additional 500-1,000 trials with enhancements

### Phase 4: Validation & Deployment (Week 5)
- [ ] Validate best configuration on fresh out-of-sample data (2025-10-25 onward)
- [ ] Run paper trading simulation for 5 days
- [ ] Document final configuration and performance
- [ ] Deploy to live trading (if validation successful)

---

## 5. Risk Analysis

### 5.1 Overfitting Risk

**Symptom:** High eval MRD, low val MRD (eval-val gap > 0.3%)

**Mitigation:**
- Enforce validation pass rate ≥ 70%
- Implement multi-objective optimization (§3.2.2)
- Use regularization: Penalize extreme parameter values

### 5.2 Computational Risk

**Challenge:** 2,000 trials × 15-day simulation ≈ 8-12 hours

**Mitigation:**
- Run optimization overnight or on dedicated machine
- Implement checkpointing (Optuna supports resume from DB)
- Monitor for crashes, OOM errors

### 5.3 Regime Shift Risk

**Symptom:** Historical optimization doesn't translate to live performance

**Mitigation:**
- Test on recent data (within 1 month of deployment)
- Implement live monitoring with kill-switch (MRD < -0.5% → halt trading)
- Use paper trading validation before real capital

### 5.4 Parameter Instability

**Symptom:** Top trials have wildly different parameters (no convergence)

**Mitigation:**
- Increase trial count to 3,000-5,000 if needed
- Use ensemble of top 5 configurations (average predictions)
- Implement parameter constraints based on domain knowledge

---

## 6. Success Metrics

### 6.1 Primary Metrics

| Metric | Current | Target | Stretch Goal |
|--------|---------|--------|--------------|
| Evaluation MRD | 0.334% | ≥0.45% | ≥0.50% |
| Validation MRD | 0.504% | ≥0.50% | ≥0.60% |
| Eval Pass Rate | 100% | ≥80% | ≥90% |
| Val Pass Rate | 80% | ≥70% | ≥80% |
| Eval-Val Gap | 0.17% | <0.10% | <0.05% |

### 6.2 Secondary Metrics

- **Sharpe Ratio:** Target ≥ 2.0 (annualized, assuming 252 trading days)
- **Max Drawdown:** Target < 10% over 5-day period
- **Win Rate:** Target ≥ 55% of daily returns > 0%
- **Trade Frequency:** Target 3-6 trades/day (avoid overtrading)

### 6.3 Live Trading Validation

**Before Deployment:**
- [ ] 5-day paper trading with real-time data
- [ ] MRD ≥ 0.4% on paper trading period
- [ ] No catastrophic losses (single day < -2%)
- [ ] Transaction costs < 0.1% of gross returns

---

## 7. Open Questions & Discussion

### 7.1 Why is Validation MRD Higher than Evaluation MRD?

**Observation:** 0.504% (val) > 0.334% (eval)

**Hypotheses:**
1. **Regime Luck:** Validation period had more favorable market conditions
2. **Overfitting to Eval:** Optimization indirectly learned eval-specific patterns
3. **Sample Variance:** 5-day validation is small sample, could be statistical noise

**Investigation:**
- Compare market regimes (volatility, trend strength) across eval/val periods
- Test if validation MRD holds on additional out-of-sample days
- Analyze if specific symbols drove validation performance

### 7.2 Should We Optimize Position Sizing Separately?

**Current:** Position sizing parameters are in `trading_params.json`, not optimized by SIGOR Optuna

**Proposal:** Run separate optimization for:
- `max_positions` (currently fixed at 2)
- Kelly criterion parameters
- Rotation threshold (`min_rank_strength`)

**Trade-off:**
- Pro: May unlock additional alpha
- Con: Increases optimization complexity (more parameters)

**Recommendation:** Defer until SIGOR detector optimization reaches 0.5% MRD. Then optimize position sizing as Phase 2.

### 7.3 Is 7 Detectors Optimal, or Should We Add More?

**Evidence:**
- 8 detectors (Williams %R) degraded performance (-46%)
- 7 detectors appears to balance signal diversity with overfitting risk

**Recommendation:**
- Stick with 7 detectors for 2,000-trial optimization
- Only add detectors if they meet strict criteria (§3.4.2):
  - Standalone MRD > 0.2%
  - Correlation with existing detectors < 0.7
  - Passes 20-day backtest validation

---

## 8. References

### 8.1 Core Strategy Implementation

- **`include/strategy/sigor_strategy.h`** - SIGOR strategy interface, configuration structs (SigorConfig, SigorSignal)
- **`src/strategy/sigor_strategy.cpp`** - 7-detector implementations, log-odds fusion logic
- **`include/utils/config_loader.h`** - Configuration loading from JSON (SigorConfigLoader)
- **`config/sigor_params.json`** - Current best parameters (Trial 112, 200-trial optimization)
- **`config/sigor_params.json.backup`** - Backup configuration (pre-Williams %R removal)

### 8.2 Trading Framework

- **`include/trading/multi_symbol_trader.h`** - Multi-symbol rotation trading logic
- **`src/trading/multi_symbol_trader.cpp`** - Position management, rotation execution
- **`include/trading/trade_filter.h`** - Holding period constraints, trade frequency management
- **`src/trading/trade_filter.cpp`** - Filter implementation (min_bars_to_hold)
- **`include/trading/alpaca_cost_model.h`** - Transaction cost modeling (commissions, slippage)
- **`src/trading/alpaca_cost_model.cpp`** - Cost calculation implementation

### 8.3 Data Infrastructure

- **`include/utils/data_loader.h`** - Binary/CSV data loading interface
- **`src/utils/data_loader.cpp`** - Data loading implementation, RTH filtering
- **`include/core/bar.h`** - Bar data structure (OHLCV + timestamp)
- **`config/symbols.conf`** - Symbol universe configuration (12 leveraged ETFs)

### 8.4 Optimization Scripts

- **`scripts/optimize_with_validation.py`** - Walk-forward validation optimization (primary tool)
- **`tools/optuna_5day_search.py`** - Legacy 5-day optimization script
- **`scripts/optimize_position_sizing.py`** - Position sizing parameter optimization (separate)

### 8.5 Execution & Main Loop

- **`src/main.cpp`** - Main entry point, CLI argument parsing, mock/live mode routing
- **`CMakeLists.txt`** - Build configuration, compiler flags, dependencies

### 8.6 Testing & Analysis

- **`src/test_sigor.cpp`** - SIGOR detector unit tests
- **`tools/analyze_optuna_results.py`** - Optuna trial analysis and visualization
- **`scripts/compare_baseline_vs_vwap.py`** - Baseline performance comparison tool

### 8.7 Live Trading Infrastructure

- **`scripts/launch_sigor_live.sh`** - Live trading launcher with Kafka/websocket bridges
- **`tools/monitor_sigor_live.sh`** - Real-time monitoring script
- **`scripts/alpaca_websocket_bridge_rotation.py`** - Alpaca market data bridge
- **`scripts/polygon_websocket_bridge_rotation.py`** - Polygon market data bridge
- **`tools/kafka_sidecar.py`** - Kafka producer for live data streaming

### 8.8 Documentation

- **`docs/SIGOR_LIVE_TRADING_GUIDE.md`** - Live trading setup and operation guide
- **`SIGOR_LIVE_QUICKSTART.md`** - Quick start guide for SIGOR live trading
- **`RELEASE_NOTES.md`** - Version 2.0 release notes (SIGOR integration)
- **`HANDOVER_KAFKA.md`** - Kafka infrastructure handover documentation

### 8.9 Configuration Files

- **`config/trading_params.json`** - Trading framework parameters (position sizing, rotation)
- **`config.env`** - Environment variables (API keys, credentials)

### 8.10 Build & Deployment

- **`build.sh`** - Build script wrapper (if exists)
- **`Dockerfile`** - Docker container configuration for deployment
- **`docker/`** - Docker-related configuration files

### 8.11 External Dependencies

- **`external/`** - Third-party libraries (Eigen, JSON parsers, etc.)

### 8.12 Results & Logs

- **`results/validated_optimization.json`** - Current 200-trial optimization results
- **`logs/validated_optimization.log`** - Optimization execution log
- **`logs/validated_optimization_2000.log`** - 2,000-trial optimization log (pending)
- **`results/validated_optimization_2000.json`** - 2,000-trial results (pending)

---

## 9. Appendix: Detector Details

### A.1 Bollinger Z-Score
```cpp
// src/strategy/sigor_strategy.cpp:99
double z = (close - mean) / stddev;
return 0.5 + 0.5 * tanh(z / 2.0);
```
- **Window:** 23 bars (optimized)
- **Interpretation:** z > 0 → above mean (long bias), z < 0 → below mean (short bias)

### A.2 RSI(14)
```cpp
// src/strategy/sigor_strategy.cpp:112
double rsi = compute_rsi(7);  // 7-bar window (optimized)
return (rsi - 50.0) / 100.0 + 0.5;
```
- **Window:** 7 bars (optimized, not standard 14)
- **Range:** 0-100, normalized to [0,1]

### A.3 Momentum
```cpp
// src/strategy/sigor_strategy.cpp:120
double ret = (curr - prev) / prev;
return 0.5 + 0.5 * tanh(ret * 50.0);
```
- **Window:** 6 bars (optimized)
- **Scaling:** 50.0 (fixed, could be optimized)

### A.4 VWAP Reversion
```cpp
// src/strategy/sigor_strategy.cpp:132
double vwap = sum(typical_price * volume) / sum(volume);
double z = (close - vwap) / abs(vwap);
return 0.5 - 0.5 * tanh(z);  // Mean reversion: above VWAP → short bias
```
- **Window:** 12 bars (optimized)
- **Behavior:** Inverse to price deviation (mean reversion)

### A.5 Opening Range Breakout
```cpp
// src/strategy/sigor_strategy.cpp:152
if (close > opening_high) return 0.7;   // Breakout long
if (close < opening_low) return 0.3;    // Breakout short
return 0.5;                              // Inside range
```
- **Opening Bars:** 48 bars = first ~1.2 hours (optimized)
- **Behavior:** Binary breakout signal

### A.6 Order Flow Imbalance (Proxy)
```cpp
// src/strategy/sigor_strategy.cpp:183
double ofi = ((close - open) / (high - low)) * tanh(volume / 1e6);
return 0.5 + 0.25 * ofi;
```
- **No window:** Uses current bar only
- **Interpretation:** Bar geometry + volume strength

### A.7 Volume Surge
```cpp
// src/strategy/sigor_strategy.cpp:190
double ratio = volume_now / volume_ma;
double adj = tanh((ratio - 1.0));
double dir = (momentum >= 0.5) ? 1.0 : -1.0;
return 0.5 + 0.25 * adj * dir;
```
- **Window:** 12 bars (optimized)
- **Behavior:** Volume surge amplifies momentum direction

---

## 10. Conclusion

We have built a robust SIGOR trading system that achieves **0.334% MRD** on evaluation data and **0.504% MRD** on validation data through careful optimization of a 7-detector ensemble. The validation performance demonstrates that our 0.5% MRD target is achievable.

**Primary Action:** Execute 2,000-trial optimization to close the evaluation gap and consistently achieve 0.5%+ MRD.

**If 2,000 trials are insufficient:** Implement secondary enhancements (expanded search space, multi-objective optimization, regime adaptation) as outlined in §3.2-§3.4.

**Success Criteria:**
- Evaluation MRD ≥ 0.45% (0.50% stretch)
- Validation MRD ≥ 0.50% (0.60% stretch)
- Pass rate ≥ 70% on validation set
- Eval-val gap < 0.10%

**Timeline:** 4-5 weeks from optimization launch to deployment validation.

---

**Document Prepared By:** SIGOR Development Team
**Review Status:** Awaiting stakeholder review
**Next Review Date:** After 2,000-trial optimization completion
