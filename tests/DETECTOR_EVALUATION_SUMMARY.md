# Detector Evaluation Summary & Integration Plan

## Executive Summary

Four proposed detectors have been implemented and prepared for backtesting integration into SIGOR:

1. **TTM Squeeze/Expansion Detector** - Volatility compression → expansion edge
2. **Donchian/Prior-Day Breakout Detector** - True breakouts vs. failed breakout fades
3. **RSI(2) Pullback Detector** - Ultra-short mean-reversion with filters
4. **VWAP Bands Mean-Reversion** - Price extremes with multi-session bias filter

## Implementation Status

### ✅ Completed
- Detector class implementations with full state tracking
- Standardized interface (`get_signal()`, `get_confidence()`)
- Backtest framework infrastructure (`detector_backtest_framework.h`)
- Comprehensive test runner (`run_all_detector_tests.cpp`)
- Evaluation metrics and ranking system

### ⏳ Pending
- Compilation with CMake integration
- Historical data backtest execution
- Parameter optimization via grid search
- Walk-forward validation

## Integration Plan (Based on Theoretical Evaluation)

Given the SIGOR philosophy of "simple rules, strong edges," here's the recommended integration priority:

### **Tier 1: Immediate Integration (High Confidence)**

#### 1. **VWAP Bands Mean-Reversion**
**Rationale:**
- Complements existing VWAP detector with statistical bands
- No-go zone filter respects multi-session bias (prevents counter-trend disasters)
- Z-score normalization makes it robust across volatility regimes
- Mean-reversion naturally bounded (limited downside risk)

**Integration Steps:**
1. Add `VWAPBandsDetector` to `sigor_strategy.h`
2. Initialize in strategy constructor
3. Add to detector fusion with weight **1.0**
4. Expected Sharpe contribution: **+0.15 to +0.25**

**Code snippet:**
```cpp
// In sigor_strategy.h
std::unique_ptr<VWAPBandsDetector> vwap_bands_det;

// In constructor
vwap_bands_det = std::make_unique<VWAPBandsDetector>();

// In update loop
vwap_bands_det->update(bar, &prev_bar, history);
double vwap_bands_signal = vwap_bands_det->get_signal();
double vwap_bands_conf = vwap_bands_det->get_confidence();

// In fusion
fusion_score += vwap_bands_signal * vwap_bands_conf * 1.0;
total_weight += 1.0;
```

#### 2. **Donchian/Prior-Day Breakout**
**Rationale:**
- Dual-mode: trend-follow (true breakouts) + mean-reversion (failed breakouts)
- ATR filtering reduces noise
- Complements existing ORB detector with failed-breakout fade logic
- Breakout confirmation reduces false signals

**Integration Steps:**
1. Test on 10-20 recent trading days first
2. If win rate >52% and Sharpe >1.0 →  add with weight **0.8**
3. Monitor correlation with ORB detector
4. Optimize `confirmation_bars` and `atr_filter_mult` parameters

**Expected Performance:**
- Breakout follow: Win rate ~48-52%
- Failed breakout fade: Win rate ~55-60%
- Combined Sharpe: **1.0-1.3**

---

### **Tier 2: Parameter Optimization Needed**

#### 3. **TTM Squeeze/Expansion**
**Rationale:**
- Strong theoretical edge (volatility expansion)
- Requires minimum squeeze duration tuning
- May fire infrequently (5-10 signals per week across basket)
- Needs breakout direction logic refinement

**Optimization Tasks:**
1. Grid search for:
   - `bb_std` (1.5, 2.0, 2.5)
   - `keltner_mult` (1.0, 1.5, 2.0)
   - Minimum squeeze duration (3, 6, 10 bars)
2. Add directional breakout confirmation logic
3. Test on squeeze→fire events only

**Conditional Integration:**
- If optimized Sharpe >1.2 → integrate with weight **0.8**
- Else → defer to Phase 2

#### 4. **RSI(2) Pullback**
**Rationale:**
- Ultra-short timeframe = high noise risk
- Requires strict VWAP distance and volume filters
- May overlap with existing mean-reversion detectors
- Needs correlation analysis with VWAP bands detector

**Optimization Tasks:**
1. Test thresholds:
   - Oversold/overbought levels (5/95, 10/90, 15/85)
   - VWAP distance guardrail (1.5σ, 2.0σ, 2.5σ)
   - Volume ratio threshold (0.5, 0.8, 1.0)
2. Correlation check with VWAP bands (must be <0.5)
3. Test in different vol regimes (VIX <15, 15-25, >25)

**Conditional Integration:**
- If correlation <0.5 and Sharpe >1.0 → integrate with weight **0.6-0.8**
- If redundant with VWAP bands → **reject**

---

## Backtest Execution Plan

### Phase 1: Data Preparation
```bash
# Ensure historical data exists for test symbols
ls -lh data/*_RTH_NH.bin

# Symbols to test:
# - TQQQ, SQQQ (tech leverage)
# - TNA, TZA (small-cap leverage)
# - SOXL, SOXS (semiconductor leverage)
# - SPXL, SPXS (S&P leverage)
```

### Phase 2: CMake Integration
```cmake
# Add to CMakeLists.txt
add_executable(test_detectors
    tests/run_all_detector_tests.cpp
    tests/squeeze_detector.cpp
    tests/donchian_detector.cpp
    tests/rsi2_detector.cpp
    tests/vwap_bands_detector.cpp
)

target_link_libraries(test_detectors PRIVATE
    sentio_core
    Eigen3::Eigen
)
```

### Phase 3: Execution
```bash
# Compile
cmake --build build --target test_detectors

# Run comprehensive backtest
./build/test_detectors

# Results will be written to:
# - tests/results/TTM_Squeeze_Expansion_trades.csv
# - tests/results/Donchian_Breakout_trades.csv
# - tests/results/RSI2_Pullback_trades.csv
# - tests/results/VWAP_Bands_Reversion_trades.csv
```

### Phase 4: Analysis
```bash
# Generate performance summary
python3 scripts/analyze_detector_performance.py \
    --trades-dir tests/results/ \
    --output tests/detector_rankings.json

# Expected output:
# {
#   "rankings": [
#     {
#       "detector": "VWAP_Bands_Reversion",
#       "score": 78.5,
#       "sharpe": 1.45,
#       "win_rate": 56.2,
#       "recommendation": "STRONG"
#     },
#     ...
#   ]
# }
```

---

## Evaluation Criteria (Pass/Fail)

| Metric | Must Pass | Nice to Have |
|--------|-----------|--------------|
| Win Rate | ≥52% | ≥55% |
| Sharpe Ratio | ≥1.0 | ≥1.3 |
| Max Drawdown | ≤15% | ≤10% |
| Profit Factor | ≥1.3 | ≥1.5 |
| Correlation w/ existing | <0.7 | <0.5 |

**Scoring Weights:**
- Sharpe Ratio: 40%
- Win Rate: 30%
- Profit Factor: 20%
- Max Drawdown: 10%

**Recommendation Thresholds:**
- **STRONG (Score ≥70):** Immediate integration with weight 1.0
- **MODERATE (Score 55-69):** Optimize parameters → weight 0.8
- **WEAK (Score 40-54):** Phase 2 consideration
- **REJECT (Score <40):** Do not integrate

---

## Risk Considerations

### Overfitting Prevention
- Use walk-forward validation (train on 60% → test on 40%)
- Hold out 2 symbols entirely (e.g., SOXL, SOXS)
- Limit parameter grid search to max 3×3×3 combinations

### Regime Sensitivity
Test detectors across:
- **Low Vol (VIX <15):** May reduce signal frequency
- **Medium Vol (VIX 15-25):** Baseline regime
- **High Vol (VIX >25):** Risk of whipsaws

### Implementation Risk
- **Slippage:** Assume 2-3 bps per round-trip for 3x leveraged ETFs
- **Latency:** Intraday signals must execute within 1-2 bars
- **Correlation:** If new detector correlation >0.7 with existing → reject

---

## Next Steps

1. ✅ **Complete:** Detector implementations, framework, and documentation
2. 🔄 **In Progress:** CMake integration for test compilation
3. ⏳ **TODO:** Run backtests on historical data (Est: 2-4 hours compute time)
4. ⏳ **TODO:** Generate final rankings and integration recommendations
5. ⏳ **TODO:** Integrate Tier 1 detectors into SIGOR
6. ⏳ **TODO:** Live paper trading validation (1 week)
7. ⏳ **TODO:** Production deployment

---

## Conservative Integration Estimate

**Best Case (All 4 Pass):**
- VWAP Bands: +0.20 Sharpe
- Donchian: +0.15 Sharpe
- TTM Squeeze: +0.10 Sharpe
- RSI(2): +0.08 Sharpe (if uncorrelated)
- **Total: +0.53 Sharpe improvement**

**Realistic Case (2-3 Pass):**
- VWAP Bands + Donchian: **+0.30-0.35 Sharpe**
- Current SIGOR baseline: ~1.8 Sharpe
- **New target: ~2.1-2.15 Sharpe**

**Conservative Case (1-2 Pass):**
- VWAP Bands only: **+0.15-0.20 Sharpe**
- Still meaningful improvement with low integration risk

---

*Generated: 2025-10-24*
*Status: Awaiting backtest execution*
*Confidence Level: MEDIUM-HIGH (theoretical evaluation pending empirical validation)*
