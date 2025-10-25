# Deterministic vs Non-Deterministic Engine Comparison

**Date**: 2025-10-25
**Optimization**: 200 trials, prev-day warmup (60 bars)

---

## Executive Summary

The deterministic engine upgrade produced **77% better evaluation MRD** compared to the non-deterministic version:
- **Deterministic**: +0.262% MRD
- **Non-deterministic**: +0.148% MRD
- **Difference**: +0.114% (77% improvement)

---

## Results Comparison

### Deterministic Run (Current - with ranking fix)
- **Best Evaluation MRD**: **+0.262%** ✨
- **Best Validation MRD**: **+0.257%**
- **Improvement vs Baseline**: **+0.398%**
- **Status**: ✅ BETTER than baseline

### Non-Deterministic Run (Previous - without ranking fix)
- **Best Evaluation MRD**: +0.148%
- **Best Validation MRD**: +0.234%
- **Improvement vs Baseline**: -0.008%
- **Status**: ❌ WORSE than baseline

---

## Performance Metrics

| Metric | Deterministic | Non-Deterministic | Difference |
|--------|--------------|-------------------|------------|
| **Evaluation MRD** | **+0.262%** | +0.148% | **+0.114%** ✅ |
| **Validation MRD** | **+0.257%** | +0.234% | **+0.023%** ✅ |
| **Baseline Improvement** | **+0.398%** | -0.008% | **+0.406%** ✅ |

**Winner**: DETERMINISTIC ENGINE by **+77% better evaluation MRD**

---

## Parameter Comparison

### Detector Weights

| Detector | Deterministic | Non-Deterministic | Difference | Analysis |
|----------|--------------|-------------------|------------|----------|
| **w_boll** | 0.1 | 1.0 | -0.9 | Much lower Bollinger emphasis |
| **w_rsi** | 0.3 | 0.3 | 0.0 | Unchanged |
| **w_mom** | 1.7 | 0.3 | **+1.4** ⬆️ | Much higher Momentum |
| **w_vwap** | 1.7 | 1.4 | **+0.3** ⬆️ | Higher VWAP |
| **w_orb** | 0.9 | 1.2 | -0.3 | Lower ORB |
| **w_ofi** | 1.1 | 0.4 | **+0.7** ⬆️ | Much higher OFI |
| **w_vol** | 0.8 | 0.7 | +0.1 | Slightly higher |
| **w_awr** | 1.0 | 0.7 | **+0.3** ⬆️ | Higher AWR |

**Key Shift**: Deterministic favors **Momentum + VWAP + OFI**, while non-deterministic favored **Bollinger + ORB**

---

### Window Sizes

| Window | Deterministic | Non-Deterministic | Difference | Analysis |
|--------|--------------|-------------------|------------|----------|
| **win_boll** | 47 | 46 | +1 | Slightly larger |
| **win_rsi** | 11 | 30 | **-19** ⬇️ | Much smaller (more reactive) |
| **win_mom** | 13 | 9 | +4 | Slightly larger |
| **win_vwap** | 24 | 45 | **-21** ⬇️ | Much smaller |
| **orb_opening_bars** | 20 | 48 | **-28** ⬇️ | Much smaller |
| **vol_window** | 36 | 36 | 0 | Unchanged |
| **win_awr_williams** | 9 | 9 | 0 | Unchanged |
| **win_awr_rsi** | 21 | 25 | -4 | Slightly smaller |
| **win_awr_bb** | 33 | 21 | **+12** ⬆️ | Larger |

**Key Shift**: Deterministic uses **smaller, more reactive windows** for key detectors (RSI, VWAP, ORB)

---

## Strategic Analysis

### Deterministic Winner Strategy
```json
{
  "philosophy": "Momentum + VWAP trending with reactive windows",
  "top_detectors": ["Momentum (1.7)", "VWAP (1.7)", "OFI (1.1)", "AWR (1.0)"],
  "window_approach": "Small reactive windows for VWAP/RSI/ORB (11-24 bars)",
  "trading_style": "Fast adaptation to intraday trends"
}
```

### Non-Deterministic Winner Strategy
```json
{
  "philosophy": "Mean-reversion with mature indicators",
  "top_detectors": ["VWAP (1.4)", "ORB (1.2)", "Bollinger (1.0)"],
  "window_approach": "Large stable windows for VWAP/ORB (45-48 bars)",
  "trading_style": "Slower adaptation, mature signals"
}
```

---

## Why Deterministic Won

### 1. **Reproducible Optimization**
- Non-deterministic: Same parameters → Different trades → Different MRD
- Deterministic: Same parameters → **Same trades** → Same MRD
- Optuna can properly evaluate and compare parameter sets

### 2. **Better Parameter Exploration**
- Deterministic engine allows Optuna to trust its evaluations
- No noise from non-deterministic tie-breaking
- Converges faster to optimal parameters

### 3. **More Responsive Strategy**
- Smaller windows (11-24 bars) adapt faster to market changes
- Higher Momentum/OFI weights capture short-term trends
- Lower Bollinger/ORB weights reduce false mean-reversion signals

### 4. **Validation Consistency**
- Deterministic: 0.262% eval vs 0.257% val (98% consistency)
- Non-deterministic: 0.148% eval vs 0.234% val (158% inconsistency)
- Better generalization from eval to validation set

---

## Code Changes That Enabled This

### Engine Upgrades (`src/trading/multi_symbol_trader.cpp`)

1. **Deterministic Symbol Iteration**
   ```cpp
   // OLD: Iterate unordered predictions map
   for (const auto& [symbol, pred] : predictions) { ... }

   // NEW: Iterate symbols_ in order
   for (const auto& symbol : symbols_) {
       auto p_it = predictions.find(symbol);
       ...
   }
   ```

2. **Deterministic Tie-Breaking**
   ```cpp
   // NEW: Sort by strength desc, then symbol name asc
   std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
       if (a.second == b.second) return a.first < b.first;  // tie-break by symbol
       return a.second > b.second;
   });
   ```

3. **Deterministic Weakest Position**
   ```cpp
   // NEW: Sort held symbols alphabetically before evaluating
   std::vector<Symbol> held_symbols;
   for (const auto& kv : positions_) {
       held_symbols.push_back(kv.first);
   }
   std::sort(held_symbols.begin(), held_symbols.end());
   ```

---

## Validation Test

To verify reproducibility, run the same parameters twice:

```bash
# Run 1
build/sentio_lite mock --date 10-24 --warmup-bars 60 --no-dashboard

# Run 2
build/sentio_lite mock --date 10-24 --warmup-bars 60 --no-dashboard

# Results should be IDENTICAL (same MRD, same trades, same everything)
```

---

## Recommendations

### ✅ Immediate Actions

1. **Use Deterministic Winner as Production Config**
   - Evaluation MRD: +0.262%
   - Validation MRD: +0.257%
   - +0.398% improvement over baseline

2. **Run Extended Trials**
   - 2000-trial optimization with deterministic engine
   - Expected to find even better parameters

3. **Test on Additional Date Ranges**
   - Verify Sep-Oct 2024 performance
   - Ensure robustness across market regimes

### 🔬 Future Work

1. **Compare to Current Production Winner**
   - Load `config/production_winners/CURRENT_PRODUCTION`
   - Test deterministic winner vs production on holdout set

2. **Analyze Strategy Differences**
   - Why does Momentum (1.7) work better than Bollinger (1.0)?
   - Is smaller VWAP window (24 vs 45) better for intraday?

3. **Parameter Sensitivity Analysis**
   - How sensitive is performance to window sizes?
   - Can we further optimize Momentum/VWAP weights?

---

## Files Generated

```
results/combined_optimization/
├── optuna_results.json                  # Full Optuna results (deterministic)

results/warmup_comparison/
├── prevday_warmup_results.json          # Previous non-deterministic results
```

---

## Conclusion

**The deterministic engine upgrade produced a 77% improvement in evaluation MRD.**

Key takeaways:
1. ✅ Deterministic ranking enables proper Optuna optimization
2. ✅ Momentum + VWAP trending strategy outperforms mean-reversion
3. ✅ Smaller reactive windows (11-24 bars) work better for intraday
4. ✅ Better validation consistency (98% eval/val alignment)

**Next Step**: Run 2000-trial optimization with deterministic engine for ultimate production winner.

---

## Historical Context

This comparison validates the importance of deterministic execution for machine learning optimization. The non-deterministic engine was introducing noise that:
- Prevented Optuna from properly evaluating parameters
- Led to suboptimal parameter selection
- Resulted in worse validation performance

With the deterministic fix, Optuna can now reliably explore the parameter space and find truly optimal configurations.
