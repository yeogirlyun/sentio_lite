# Requirements: Outsmart Brute Force Trading Method

## Executive Summary

**Problem Statement:**
The "brute force" method (5-bar minimum hold, time-based exits, ~104 trades/day) achieved **-0.13% MRD** on Oct 13-17, 2025, while our sophisticated rotation logic (100 bps threshold, direction checks, variable hold) achieved **-0.28% MRD** - WORSE performance despite being "smarter".

**Goal:**
Design and implement an intelligent trading scheme that consistently outperforms the brute force baseline while maintaining signal-driven (not time-driven) decision-making.

**Success Criteria:**
- MRD > -0.13% (better than brute force baseline)
- Trade count variable (not fixed ~104/day)
- Win rate > 13%
- Profit factor > 1.0
- Sharpe ratio > 0.5

---

## Current State Analysis

### Brute Force Method Performance (Baseline)
| Date | MRD | Trades | Win Rate | Profit Factor | Hold Period |
|------|-----|--------|----------|---------------|-------------|
| Oct 13 | +0.01% | 104 | 12.5% | 0.76 | 5 bars min |
| Oct 14 | +0.32% | 103 | 8.7% | 0.50 | 5 bars min |
| Oct 15 | -0.02% | 104 | 11.5% | 0.84 | 5 bars min |
| Oct 16 | -0.11% | 104 | 13.5% | 1.03 | 5 bars min |
| Oct 17 | -0.86% | 104 | 13.5% | 0.82 | 5 bars min |
| **Avg** | **-0.13%** | **103.8** | **11.9%** | **0.79** | Fixed |

### Smart Rotation Method Performance (Current)
| Date | MRD | Trades | Win Rate | Profit Factor | Rotations |
|------|-----|--------|----------|---------------|-----------|
| Oct 13 | -0.37% | 59 | 15.3% | 0.45 | 2 |
| Oct 14 | +0.44% | 63 | 11.1% | 0.38 | 2 |
| Oct 15 | -0.44% | 69 | 11.6% | 0.45 | 3 |
| Oct 16 | -0.57% | 75 | 10.7% | 0.58 | 4 |
| Oct 17 | -0.46% | 72 | 16.7% | 0.58 | 4 |
| **Avg** | **-0.28%** | **67.6** | **13.1%** | **0.49** | 3 |

### Key Observations

#### What Brute Force Does RIGHT:
1. **Consistent cadence** - Trades happen at predictable intervals
2. **Quick exits** - 5 bars = 5 minutes, capturing micro-moves
3. **High frequency** - More opportunities to be right (104 trades/day)
4. **Simple logic** - Less chance for bugs/complexity failures
5. **Mean reversion capture** - 5-minute holds match intraday volatility cycles

#### What Smart Rotation Does WRONG:
1. **Holds too long** - 20-120 bars misses momentum reversals
2. **Waits for 100 bps delta** - By the time signal is "strong enough", move may be over
3. **Fewer trades** - 67 trades/day = fewer chances to profit
4. **Low profit factor** - 0.49 vs 0.79 (loses more per trade)
5. **Direction filter** - May be preventing profitable counter-trend trades

---

## Root Cause Analysis

### Hypothesis 1: Market Regime Mismatch
**Oct 13-17 market characteristics:**
- 3x Leveraged ETFs (TQQQ, SOXL, etc.)
- High intraday volatility
- Mean-reverting on 5-minute timeframe
- Momentum exhausts quickly (< 10 bars)

**Brute force match:**
- ✅ 5-bar hold captures mean reversion perfectly
- ✅ High frequency catches micro-cycles

**Smart rotation mismatch:**
- ❌ 20-bar minimum misses quick reversals
- ❌ Waiting for 100 bps confirmation = late entry
- ❌ Fewer trades = fewer mean reversion captures

### Hypothesis 2: Prediction Decay
**EWRLS prediction accuracy degrades over time:**
- Strong at entry (0 bars held)
- Weakens by 10 bars
- Unreliable by 20+ bars

**Brute force advantage:**
- Exits at 5 bars (peak accuracy)

**Smart rotation disadvantage:**
- Exits at 20-120 bars (degraded accuracy)

### Hypothesis 3: Over-optimization
**Smart rotation has too many filters:**
1. Probability threshold (55%)
2. Direction check (same direction only)
3. Strength delta (100 bps)
4. Confidence threshold (25%)
5. Signal reversal check (-10 bps)
6. Rotation cooldown (10 bars)

**Effect:** Missed profitable opportunities waiting for "perfect" setup

---

## Requirements

### REQ-1: Adaptive Hold Period Based on Signal Strength Decay

**Rationale:** Don't hold positions beyond predictor reliability window

**Implementation:**
```cpp
// Track prediction accuracy over hold duration
struct PredictionDecay {
    std::map<int, double> accuracy_by_bars_held;  // bars → win rate

    int optimal_exit_window() {
        // Find window where accuracy drops below threshold
        for (auto [bars, accuracy] : accuracy_by_bars_held) {
            if (accuracy < 0.5) return bars;
        }
        return 5;  // Default to brute force if unknown
    }
};
```

**Expected Outcome:** Exit before prediction accuracy degrades

---

### REQ-2: Multi-Timeframe Signal Confirmation

**Rationale:** Single 5-bar prediction is noisy; confirm with multiple horizons

**Implementation:**
```cpp
struct MultiTimeframeScore {
    double score_1bar;   // Weight: 0.2 (immediate)
    double score_5bar;   // Weight: 0.5 (primary)
    double score_10bar;  // Weight: 0.3 (trend)

    double composite_score() {
        return score_1bar * 0.2 + score_5bar * 0.5 + score_10bar * 0.3;
    }

    bool all_aligned() {
        // All horizons agree on direction
        return (score_1bar > 0 && score_5bar > 0 && score_10bar > 0) ||
               (score_1bar < 0 && score_5bar < 0 && score_10bar < 0);
    }
};
```

**Entry Rule:** Only enter when all 3 horizons align
**Exit Rule:** Exit when 1-bar diverges from 5-bar

**Expected Outcome:** Higher win rate through better entry quality

---

### REQ-3: Momentum Exhaustion Detection

**Rationale:** Exit when momentum weakens, not on fixed time

**Implementation:**
```cpp
struct MomentumTracker {
    std::deque<double> recent_returns;  // Last 5 bars

    bool momentum_exhausted() {
        if (recent_returns.size() < 5) return false;

        // Check if returns are diminishing
        double avg_first_3 = average(recent_returns.begin(), recent_returns.begin() + 3);
        double avg_last_2 = average(recent_returns.end() - 2, recent_returns.end());

        return avg_last_2 < avg_first_3 * 0.5;  // Returns halved
    }
};
```

**Exit Rule:** Exit when momentum drops 50% from peak

**Expected Outcome:** Capture full move, exit before reversal

---

### REQ-4: Market Regime Detection (Trending vs Mean-Reverting)

**Rationale:** Different strategies for different regimes

**Implementation:**
```cpp
enum class MarketRegime {
    TRENDING_UP,      // Hold longer, ride momentum
    TRENDING_DOWN,    // Hold longer, ride momentum
    MEAN_REVERTING,   // Quick in/out (brute force style)
    CHOPPY            // Reduce position size or skip
};

class RegimeDetector {
    MarketRegime detect(const Symbol& symbol) {
        // Use ADX, volatility, autocorrelation
        double adx = calculate_adx(symbol);
        double autocorr = calculate_autocorrelation(symbol, 20);

        if (adx > 25) return TRENDING_UP/DOWN;
        if (autocorr < -0.3) return MEAN_REVERTING;
        return CHOPPY;
    }
};
```

**Trading Rules by Regime:**
| Regime | Hold Period | Position Size | Exit Strategy |
|--------|-------------|---------------|---------------|
| Trending | 20-60 bars | 100% | Trailing stop |
| Mean-Reverting | 3-8 bars | 100% | Quick profit target |
| Choppy | 5-10 bars | 50% | Tight stop |

**Expected Outcome:** Match strategy to market conditions

---

### REQ-5: Dynamic Rotation Threshold Based on Volatility

**Rationale:** 100 bps threshold is static; should adapt to market volatility

**Implementation:**
```cpp
class AdaptiveRotationThreshold {
    double calculate_threshold(const Symbol& symbol) {
        double recent_volatility = calculate_volatility(symbol, 20);  // 20-bar ATR

        // Scale threshold with volatility
        // Low vol (0.5%): 50 bps threshold
        // High vol (2.0%): 200 bps threshold
        return recent_volatility * 100;  // Volatility in fraction → bps
    }
};
```

**Example:**
- TQQQ volatility = 1.5% → threshold = 150 bps
- UVXY volatility = 3.0% → threshold = 300 bps

**Expected Outcome:** Don't over-rotate in volatile markets

---

### REQ-6: Position Sizing Based on Prediction Confidence

**Rationale:** Risk less on uncertain signals

**Implementation:**
```cpp
double calculate_position_size_v2(const Symbol& symbol, const PredictionData& pred) {
    double base_size = calculate_position_size(symbol, pred);  // Existing Kelly logic

    // Scale by multi-horizon agreement
    double alignment = calculate_horizon_alignment(pred);  // 0.0 to 1.0

    // Scale by recent win rate
    double recent_win_rate = trade_history_[symbol]->get_recent_win_rate(10);

    // Combined scaling
    double confidence_multiplier = alignment * 0.5 + recent_win_rate * 0.5;

    return base_size * std::max(0.25, confidence_multiplier);  // 25% to 100% size
}
```

**Expected Outcome:** Reduce losses on uncertain trades

---

### REQ-7: Fast Exit on Failed Predictions

**Rationale:** Don't wait 20 bars if prediction is clearly wrong

**Implementation:**
```cpp
bool should_fast_exit(const Symbol& symbol, int bars_held) {
    if (bars_held < 3) return false;  // Give it a chance

    double entry_prediction = get_entry_prediction(symbol);
    double current_pnl = positions_[symbol].pnl_percentage();

    // If prediction was +1% but we're down -0.5% after 3 bars, exit
    if (entry_prediction > 0 && current_pnl < -entry_prediction * 0.5) {
        return true;
    }
    if (entry_prediction < 0 && current_pnl > -entry_prediction * 0.5) {
        return true;
    }

    return false;
}
```

**Expected Outcome:** Cut losses faster than stops

---

### REQ-8: Trade Frequency Balancing

**Rationale:** Brute force's 104 trades/day may be optimal for this market

**Implementation:**
```cpp
struct FrequencyTargeting {
    int target_trades_per_day = 90;  // Slightly below brute force
    int trades_today = 0;

    bool should_reduce_entries() {
        int hours_remaining = calculate_hours_to_close();
        int expected_final_count = trades_today + (trades_today / hours_elapsed) * hours_remaining;

        return expected_final_count > target_trades_per_day * 1.2;  // 20% buffer
    }

    bool should_increase_entries() {
        // Similar logic for under-trading
        return expected_final_count < target_trades_per_day * 0.8;
    }
};
```

**Adjustment:** Raise/lower entry thresholds dynamically

**Expected Outcome:** Optimal trade frequency for market conditions

---

### REQ-9: Learning from Brute Force Success Patterns

**Rationale:** Analyze WHICH brute force trades won, replicate pattern

**Implementation:**
```cpp
class BruteForceAnalyzer {
    void analyze_winning_trades() {
        // Parse brute force trade logs
        for (auto& trade : brute_force_trades) {
            if (trade.pnl > 0) {
                // What made it win?
                log_pattern({
                    .entry_hour = trade.entry_time.hour(),
                    .symbol = trade.symbol,
                    .signal_strength = trade.prediction,
                    .hold_duration = trade.bars_held,
                    .volatility_at_entry = calculate_vol(trade.entry_time)
                });
            }
        }
    }

    double probability_of_success(const Symbol& symbol, const Bar& bar) {
        // Lookup historical pattern match
        return pattern_database.match_score(symbol, bar);
    }
};
```

**Entry Rule:** Only enter if pattern matches historical winners

**Expected Outcome:** Replicate brute force success, avoid its failures

---

### REQ-10: Ensemble Strategy Switching

**Rationale:** Use brute force WHEN it works, smart rotation when it doesn't

**Implementation:**
```cpp
enum class ActiveStrategy {
    BRUTE_FORCE,      // 5-bar holds, high frequency
    SMART_ROTATION,   // Variable holds, signal-driven
    HYBRID            // Blend of both
};

class StrategySelector {
    ActiveStrategy select_strategy() {
        // Measure recent performance
        double brute_force_mrd_10day = backtest_brute_force(10);
        double smart_rotation_mrd_10day = backtest_smart_rotation(10);

        if (brute_force_mrd_10day > smart_rotation_mrd_10day * 1.5) {
            return BRUTE_FORCE;
        } else if (smart_rotation_mrd_10day > brute_force_mrd_10day * 1.5) {
            return SMART_ROTATION;
        } else {
            return HYBRID;  // Use 50/50 blend
        }
    }
};
```

**Expected Outcome:** Always use best-performing strategy for current regime

---

## Implementation Plan

### Phase 1: Data Collection & Analysis (Week 1)
**Tasks:**
1. Export all brute force trades with full details (entry/exit time, signal, PnL)
2. Export all smart rotation trades with same details
3. Analyze winning trade patterns for both strategies
4. Calculate prediction accuracy by hold duration (1, 5, 10, 20, 50 bars)
5. Identify market regime during Oct 13-17

**Deliverables:**
- `brute_force_trade_analysis.csv`
- `smart_rotation_trade_analysis.csv`
- `prediction_decay_curve.json`
- `market_regime_classification.json`

### Phase 2: Feature Development (Week 2-3)
**Priority Order:**
1. **REQ-1:** Adaptive hold period (CRITICAL)
2. **REQ-3:** Momentum exhaustion detection (CRITICAL)
3. **REQ-7:** Fast exit on failed predictions (HIGH)
4. **REQ-2:** Multi-timeframe confirmation (HIGH)
5. **REQ-4:** Market regime detection (MEDIUM)
6. **REQ-5:** Dynamic rotation threshold (MEDIUM)
7. **REQ-6:** Confidence-based sizing (LOW)
8. **REQ-8:** Trade frequency targeting (LOW)

### Phase 3: Backtesting & Validation (Week 4)
**Test Scenarios:**
1. Re-run Oct 13-17 with each REQ individually
2. Re-run Oct 13-17 with REQ combos (1+3, 1+3+7, etc.)
3. Run on out-of-sample dates (Oct 1-10)
4. Compare all variants vs brute force baseline

**Success Metrics:**
- Must beat -0.13% MRD baseline
- Must maintain variable trade count
- Sharpe ratio > brute force
- Max drawdown < brute force

### Phase 4: Production Deployment (Week 5)
**Validation:**
- Run 5-day paper trading
- Monitor real-time performance
- A/B test: 50% capital brute force, 50% smart hybrid

---

## Technical Architecture

### New Classes to Implement

```cpp
// 1. Prediction decay tracker
class PredictionDecayAnalyzer {
    void record_prediction_outcome(Symbol, int bars_held, double prediction, double actual);
    double get_accuracy_at_horizon(int bars);
    int optimal_exit_horizon();
};

// 2. Multi-timeframe signal
class MultiTimeframeSignal {
    MultiHorizonPrediction prediction;
    double composite_score();
    bool all_horizons_aligned();
    int strongest_horizon();
};

// 3. Momentum tracker
class MomentumTracker {
    void update(double return_pct);
    bool is_exhausted();
    double momentum_strength();
};

// 4. Regime detector
class MarketRegimeDetector {
    MarketRegime detect(Symbol);
    TradingParameters get_regime_params(MarketRegime);
};

// 5. Strategy selector
class StrategySelector {
    ActiveStrategy select();
    void record_performance(ActiveStrategy, double mrd);
};
```

### Modified Classes

```cpp
// MultiSymbolTrader enhancements
class MultiSymbolTrader {
    // Add new members
    std::unique_ptr<PredictionDecayAnalyzer> decay_analyzer_;
    std::unique_ptr<MarketRegimeDetector> regime_detector_;
    std::unique_ptr<StrategySelector> strategy_selector_;
    std::unordered_map<Symbol, MomentumTracker> momentum_trackers_;

    // Add new methods
    bool should_fast_exit_v2(const Symbol& symbol);
    int calculate_adaptive_hold_period(const Symbol& symbol);
    MarketRegime get_current_regime(const Symbol& symbol);
    double calculate_dynamic_rotation_threshold(const Symbol& symbol);
};
```

---

## Success Criteria

### Primary (Must Achieve):
1. **MRD > -0.13%** on Oct 13-17 test period
2. **Trade count variable** (not fixed 104/day)
3. **Profit factor > 0.79** (better than brute force)

### Secondary (Should Achieve):
4. **Win rate > 12%**
5. **Sharpe ratio > 0.5**
6. **Max drawdown < 2%**

### Stretch Goals:
7. **MRD > 0%** (actually profitable)
8. **Profit factor > 1.0** (more wins than losses)
9. **Outperform on 30-day backtest** (not just 5 days)

---

## Risk Analysis

### Risk 1: Over-fitting to Oct 13-17
**Mitigation:** Test on multiple date ranges before production

### Risk 2: Increased complexity → more bugs
**Mitigation:** Comprehensive unit tests, phased rollout

### Risk 3: Computational overhead
**Mitigation:** Profile performance, optimize hot paths

### Risk 4: Market regime changes
**Mitigation:** Adaptive strategy selection (REQ-10)

---

## References - Source Modules

### Core Trading Logic
1. **`include/trading/multi_symbol_trader.h`** (lines 30-150)
   - TradingConfig struct
   - Rotation parameters
   - Warmup configuration

2. **`src/trading/multi_symbol_trader.cpp`** (lines 80-613)
   - `on_bar()` - Main trading loop
   - `make_trades()` - Entry and rotation logic (lines 360-613)
   - `update_positions()` - Exit logic (lines 615-650)
   - `find_weakest_position()` - Rotation helper (lines 1271-1298)
   - `update_rotation_cooldowns()` - Cooldown management (lines 1300-1321)

### Trade Filtering & Timing
3. **`include/trading/trade_filter.h`** (lines 20-70)
   - Config struct with exit thresholds
   - min_bars_to_hold, typical_hold_period, max_bars_to_hold
   - exit_confidence_threshold, exit_signal_reversed_threshold

4. **`src/trading/trade_filter.cpp`** (lines 30-110)
   - `should_exit_position()` - Exit decision logic (lines 43-108)
   - `can_enter_position()` - Entry filtering (lines 30-42)

### Prediction System
5. **`include/predictor/multi_horizon_predictor.h`**
   - Multi-horizon prediction structure
   - 1-bar, 5-bar, 10-bar predictions
   - Confidence scoring

6. **`src/predictor/multi_horizon_predictor.cpp`**
   - EWRLS implementation
   - Lambda decay parameters
   - Prediction generation

### Feature Engineering
7. **`include/predictor/feature_extractor.h`**
   - 33 feature definitions
   - 8 time features + 25 technical features

8. **`src/predictor/feature_extractor.cpp`**
   - Feature calculation methods
   - Normalization logic

### Position Management
9. **`include/trading/position.h`**
   - Position struct
   - PnL calculation
   - Entry/exit tracking

10. **`src/trading/position.cpp`**
    - Position lifecycle management

### Trade History
11. **`include/trading/trade_history.h`**
    - Circular buffer for recent trades
    - Win/loss tracking

12. **`src/trading/trade_history.cpp`**
    - Performance analytics

### Main Entry Point
13. **`src/main.cpp`** (lines 400-800)
    - Command-line argument parsing
    - Configuration setup
    - Mock mode execution
    - Results export

### Configuration
14. **`include/trading/multi_symbol_trader.h`** (lines 67-71)
    - Rotation strategy configuration
    - `rotation_strength_delta = 0.01` (100 bps)
    - `rotation_cooldown_bars = 10`

### Analysis & Results
15. **`ROTATION_ANALYSIS.md`**
    - Initial problem identification
    - Comparison with online_trader

16. **`ROTATION_TEST_RESULTS.md`**
    - First iteration results (20 bps)
    - Performance comparison

17. **`ROTATION_TUNING_RESULTS.md`**
    - Tuned parameters (100 bps)
    - Final comparison vs brute force

### Related Online Trader Code (Reference)
18. **`/Volumes/ExternalSSD/Dev/C++/online_trader/src/strategy/rotation_position_manager.cpp`** (lines 200-339)
    - Original rotation logic implementation
    - Rank-based decision making
    - Strength delta calculation

19. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/strategy/rotation_position_manager.h`**
    - RotationPositionManager interface
    - Position ranking logic

---

## Appendix A: Performance Data

### Brute Force Detailed Results
```
Oct 13: +0.01% MRD, 104 trades, 12.5% WR, PF 0.76
Oct 14: +0.32% MRD, 103 trades,  8.7% WR, PF 0.50
Oct 15: -0.02% MRD, 104 trades, 11.5% WR, PF 0.84
Oct 16: -0.11% MRD, 104 trades, 13.5% WR, PF 1.03
Oct 17: -0.86% MRD, 104 trades, 13.5% WR, PF 0.82
```

### Smart Rotation Detailed Results
```
Oct 13: -0.37% MRD, 59 trades, 15.3% WR, PF 0.45, 2 rotations
Oct 14: +0.44% MRD, 63 trades, 11.1% WR, PF 0.38, 2 rotations
Oct 15: -0.44% MRD, 69 trades, 11.6% WR, PF 0.45, 3 rotations
Oct 16: -0.57% MRD, 75 trades, 10.7% WR, PF 0.58, 4 rotations
Oct 17: -0.46% MRD, 72 trades, 16.7% WR, PF 0.58, 4 rotations
```

### Delta Analysis
```
Average MRD Delta: -0.15% (smart rotation worse)
Average Trade Count Delta: -36.2 trades/day (smart rotation fewer)
Average Win Rate Delta: +1.2% (smart rotation slightly better)
Average Profit Factor Delta: -0.30 (smart rotation much worse)
```

**Conclusion:** Brute force's higher trade frequency compensates for lower win rate through more opportunities.

---

## Appendix B: Hypothesis Validation Plan

### Test 1: Hold Period Sweep
Run Oct 13-17 with varying min_bars_to_hold:
- 3 bars (aggressive)
- 5 bars (brute force)
- 10 bars
- 20 bars (current smart)
- 40 bars (conservative)

**Expected:** Peak performance around 5-8 bars

### Test 2: Rotation Threshold Sweep
Run with varying rotation_strength_delta:
- 25 bps (aggressive)
- 50 bps
- 100 bps (current)
- 200 bps (conservative)
- Infinity (no rotation)

**Expected:** Optimal around 50-75 bps

### Test 3: Frequency Targeting
Force trade count to match brute force (104/day) by adjusting thresholds

**Expected:** If performance improves, confirms frequency is key

---

## Version History
- **v1.0** - 2025-10-18 - Initial requirements document
- **Author:** Claude Code
- **Stakeholder:** User (trading system owner)
- **Priority:** P0 (Critical - current system underperforming)
