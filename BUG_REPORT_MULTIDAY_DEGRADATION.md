# BUG REPORT: Multi-Day Trading Performance Degradation

**Date:** 2025-10-18
**Reporter:** System Analysis
**Severity:** HIGH
**Priority:** P1
**Status:** CONFIRMED

---

## Executive Summary

Multi-day trading shows two critical bugs:
1. **Trade Count Discrepancy**: Day 1 single-day (101 trades) ≠ Day 1 multi-day (105 trades)
2. **Performance Degradation**: Multi-day performance degrades over time (-0.17% Day 1 → -0.45% Day 5), contrary to expectation that accumulated learning should improve performance

**Impact**: Multi-day testing produces -0.42% worse returns over 5 days compared to single-day testing (-1.63% vs -1.21%)

---

## Issue #1: Trade Count and Equity Discrepancy

### Observed Behavior

| Metric | Single-Day Oct 6 | Multi-Day Day 1 (Oct 6) | Difference |
|--------|-----------------|----------------------|------------|
| Trades | 101 | 105 | +4 ❌ |
| Final Equity | $99,831.36 | $99,825.07 | -$6.29 ❌ |
| Return | -0.17% | -0.17% | Same |

### Root Cause

**EOD Liquidation Inconsistency**

```
Multi-day Day 1 EOD positions:
- UVXY: HOLDING (3 bars)
- SVIX: HOLDING (entered right before EOD)
- SOXL: HOLDING (3 bars)

Actions taken:
- Liquidated all 3 positions (3 exits)
- SVIX was entered just before EOD (1 entry)
- Total: 4 additional trades

Single-day behavior:
- Same 3 positions entered
- Test ends WITHOUT liquidation
- Positions remain open
- Equity includes unrealized P&L
```

**Code Location:**
- `src/trading/multi_symbol_trader.cpp:700-713` - `liquidate_all()` function
- Single-day mode doesn't force EOD liquidation
- Multi-day mode forces liquidation at each day boundary

### Why This Is Wrong

**Theoretical Expectation:**
- Day 1 of multi-day test should be IDENTICAL to standalone single-day test
- Same warmup data (confirmed: both use Oct 3)
- Same trading data (confirmed: both have 391 bars)
- Same initial conditions (confirmed: $100K starting capital)

**Actual Behavior:**
- Different final states due to EOD liquidation policy
- Creates artificial trades that wouldn't occur in real trading
- Distorts equity calculations

---

## Issue #2: Multi-Day Performance Degradation

### Observed Behavior

**Day-by-Day Comparison:**

| Day | Date | Single-Day | Multi-Day | Δ | Winner |
|-----|------|-----------|----------|---|--------|
| 1 | Oct 6 | -0.17% | -0.17% | 0.00% | TIE |
| 2 | Oct 7 | -0.45% | -0.26% | +0.19% | Multi ✓ |
| 3 | Oct 8 | -0.21% | -0.28% | -0.07% | Single |
| 4 | Oct 9 | -0.30% | -0.46% | -0.16% | Single |
| 5 | Oct 10 | -0.08% | -0.45% | -0.37% | Single |

**Multi-Day Performance Decay:**
- Day 1-2: Good (-0.17%, -0.26% daily)
- Day 3-5: **Degrading** (-0.28%, -0.46%, -0.45% daily)
- Daily losses **increase** from $175 (Day 1) to $447-458 (Days 4-5)
- Win rate drops from 47.6% (Day 2) to 36.8% (Day 4)

### Root Cause

**Adaptive Position Sizing with Accumulated Loss History**

**Code Location:** `src/trading/multi_symbol_trader.cpp:578-594`

```cpp
// STEP 8: Adaptive sizing based on recent trade history
auto& history = *trade_history_[symbol];
if (history.size() >= config_.trade_history_size) {
    bool all_wins = true;
    bool all_losses = true;

    for (size_t i = 0; i < history.size(); ++i) {
        if (history[i].pnl <= 0) all_wins = false;
        if (history[i].pnl >= 0) all_losses = false;
    }

    if (all_wins) {
        position_capital *= config_.win_multiplier;  // 1.3x
    } else if (all_losses) {
        position_capital *= config_.loss_multiplier;  // 0.7x ❌
    }
}
```

**Configuration:** `include/trading/multi_symbol_trader.h:38-40`
```cpp
double loss_multiplier = 0.7;      // Reduces to 70% after losses
size_t trade_history_size = 3;     // Last 3 trades per symbol
```

**Problem Mechanism:**

1. **Day 1-2**: Fresh start, normal position sizing
2. **Day 3**: Some symbols accumulate 3 losing trades → position size reduced to 70%
3. **Day 4-5**: More symbols hit loss threshold → widespread 70% sizing
4. **Result**:
   - Smaller positions = reduced profit potential
   - Can't recover from drawdowns with tiny positions
   - System becomes overly conservative
   - Performance degrades further

**Evidence from logs:**
```
Day 1: 105 trades, -0.17%
Day 2: 103 trades, -0.26%  (similar trading activity)
Day 3: 107 trades, -0.29%  (still active but worse results)
Day 4: 106 trades, -0.46%  (trade count same but losses DOUBLE)
Day 5: ~105 trades, -0.45% (consistent degradation)
```

Trade counts remain consistent, but **returns degrade** → position sizing issue, not trade selection issue.

### Why Single-Day Outperforms

**Single-Day Advantage:**
- Each day starts with **empty trade history**
- No accumulated losses to reduce position sizing
- Fresh position sizing based only on current signal quality
- Can take full-size positions on good signals

**Multi-Day Disadvantage:**
- Carries forward loss history across days
- Position sizing shrinks over time in losing periods
- Compounding conservatism prevents recovery
- Accumulated "learning" actually hurts performance

---

## Hypothesis: Why Accumulated Learning Fails

**User's Expectation:** Accumulated transaction history should improve performance through:
- Better risk management
- Learning from recent wins/losses
- Adaptive position sizing

**Reality:** In volatile/losing periods:
- Adaptive sizing becomes **overly conservative**
- System "learns" to trade smaller and smaller
- Reduces profit potential on good signals
- Cannot recover from drawdowns
- Creates a **downward spiral**

---

## Proposed Solution: Trade Warmup Scheme

### Concept

Instead of pure multi-day accumulation, implement a **hybrid approach**:

```
┌─────────────────┐
│ Predictor Warmup │ → 1 day of price data (train EWRLS)
└─────────────────┘
         ↓
┌─────────────────┐
│  Trade Warmup    │ → N recent days of actual trading
└─────────────────┘   (build realistic trade history)
         ↓
┌─────────────────┐
│   Test Date      │ → Final test on target date
└─────────────────┘   (with warmed up predictor + trade history)
```

### Benefits

1. **Realistic trade history** without long-term degradation
2. **Controlled warmup period** (e.g., 2-3 days max)
3. **Fresh enough** to avoid overfitting
4. **Mature enough** to have adaptive sizing calibrated

### Implementation Plan

#### Phase 1: Add Trade Warmup Configuration

```cpp
// In multi_symbol_trader.h
struct TradingConfig {
    // ... existing fields ...

    // Trade warmup configuration
    int trade_warmup_days = 0;   // 0 = disabled, >0 = warmup N days before test
    bool use_trade_warmup = false;
};
```

#### Phase 2: Implement Trade Warmup Mode

```cpp
// In main.cpp
if (config.use_trade_warmup && config.trade_warmup_days > 0) {
    // Calculate warmup trading period
    std::string warmup_start = calculate_trading_day_before(
        test_date,
        predictor_warmup_days + trade_warmup_days
    );
    std::string warmup_end = calculate_trading_day_before(
        test_date,
        1  // Day before test
    );

    // Run warmup trading (silent mode, no output)
    run_warmup_trading(warmup_start, warmup_end, trader);

    // Reset to test date for actual test
    run_test_date(test_date, trader);  // Keeps accumulated history
}
```

#### Phase 3: Prevent Long-Term Degradation

```cpp
// In multi_symbol_trader.cpp
// Option 1: Cap trade history window
if (trading_bars_ > 0 && trading_bars_ % (bars_per_day * 3) == 0) {
    // Every 3 days, clear trade history older than 5 days
    prune_old_trade_history(5);
}

// Option 2: Use exponential decay for adaptive sizing
// Recent trades have more weight than old trades
double calculate_adaptive_multiplier(const TradeHistory& history) {
    double weighted_sum = 0.0;
    double weight_sum = 0.0;
    double decay = 0.8;  // Recent trades weighted more

    for (size_t i = 0; i < history.size(); ++i) {
        double weight = std::pow(decay, history.size() - 1 - i);
        weighted_sum += (history[i].pnl > 0 ? 1.0 : -1.0) * weight;
        weight_sum += weight;
    }

    double score = weighted_sum / weight_sum;
    return score > 0.5 ? win_multiplier :
           score < -0.5 ? loss_multiplier : 1.0;
}
```

### Production Deployment Strategy

**Pre-Market Warmup Routine** (Daily at 9:00 AM ET):

```bash
#!/bin/bash
# Pre-market warmup script

TODAY=$(date +%Y-%m-%d)

# Step 1: Warmup predictor (1 day)
# Step 2: Warmup trades (2 days)
# Step 3: Ready for live trading at 9:30 AM

./sentio_lite live \
    --predictor-warmup-days 1 \
    --trade-warmup-days 2 \
    --start-time "09:30" \
    --mode LIVE
```

**Advantages for Live Trading:**
1. **Calibrated predictor**: Trained on recent market conditions
2. **Realistic position sizing**: Based on last 2 days of actual trades
3. **Fresh enough**: Avoids staleness from week-old data
4. **Consistent**: Same warmup routine every morning

---

## Testing Plan

### Test #1: Verify Trade Warmup Implementation

```bash
# Test with 2-day trade warmup
./build/sentio_lite mock \
    --date 2025-10-10 \
    --predictor-warmup-days 1 \
    --trade-warmup-days 2

Expected:
- Predictor trained on Oct 9 (1 day)
- Trade warmup on Oct 6-9 (3 days total for history)
- Test on Oct 10
- Trade history should have ~300 recent trades
```

### Test #2: Compare Performance

Run same 5-day period with different schemes:

1. **Pure Single-Day** (baseline)
2. **Pure Multi-Day** (current, degrading)
3. **Trade Warmup (2 days)** (proposed)
4. **Trade Warmup (3 days)** (proposed)

**Success Criteria:**
- Trade warmup should perform better than pure multi-day
- Trade warmup should match or beat single-day
- No performance degradation over 5 days

### Test #3: Long-Term Stability

Run 20-day test to verify no degradation:
- Days 1-5: Should be profitable
- Days 6-10: Should maintain performance
- Days 11-15: Should not degrade
- Days 16-20: Should remain stable

---

## References

### Source Modules

#### Core Trading Logic
- `src/trading/multi_symbol_trader.cpp` - Main trading engine
  - Lines 534-600: `calculate_position_size()` - Adaptive sizing with trade history
  - Lines 700-713: `liquidate_all()` - EOD liquidation
  - Lines 652-698: `exit_position()` - Trade counting
  - Lines 258-287: EOD reset and daily results tracking

- `include/trading/multi_symbol_trader.h` - Configuration
  - Lines 38-40: Adaptive sizing parameters (`loss_multiplier`, `win_multiplier`, `trade_history_size`)
  - Lines 85-95: Trading configuration struct

#### Trade Filtering
- `src/trading/trade_filter.cpp` - Trade frequency and holding periods
  - Lines 111-127: `record_entry()` - Position entry tracking
  - Lines 129-141: `record_exit()` - Position exit tracking
  - Lines 143-170: `reset_daily_limits()` - EOD reset logic

- `include/trading/trade_filter.h` - Filter configuration
  - Lines 55-67: Default configuration parameters
  - Lines 72-88: PositionState struct

#### Date Filtering
- `src/main.cpp` - Test orchestration
  - Lines 270-285: `get_trading_days()` - Market calendar extraction
  - Lines 287-302: `find_warmup_start_date()` - Market-aware warmup calculation
  - Lines 304-372: `filter_to_date_range()` - Multi-day date filtering
  - Lines 374-440: `filter_to_date()` - Single-day date filtering

#### Predictor
- `src/predictor/multi_horizon_predictor.cpp` - EWRLS prediction
  - Accumulates learning across all bars
  - No explicit reset mechanism

#### Cost Model
- `src/trading/alpaca_cost_model.cpp` - Transaction cost calculation
  - Used in position entry/exit
  - Affects final P&L calculations

---

## Recommendations

### Immediate Actions (P1)

1. **Fix EOD Liquidation Inconsistency**
   - Make single-day and multi-day Day 1 identical
   - Option A: Force single-day to also liquidate at EOD
   - Option B: Don't force multi-day to liquidate (keep positions overnight)
   - **Recommended**: Option A for consistency

2. **Implement Trade Warmup Scheme**
   - Add trade warmup configuration
   - Implement warmup mode (silent trading)
   - Test on 5-day period

3. **Add Exponential Decay to Adaptive Sizing**
   - Weight recent trades more heavily
   - Prevent long-term degradation
   - Keep trade history window short (3-5 days max)

### Medium-Term Actions (P2)

1. **Add Performance Monitoring**
   - Track position sizing trends over time
   - Alert if average position size drops below threshold
   - Log trade history statistics at EOD

2. **Calibrate Adaptive Sizing Parameters**
   - Test different `loss_multiplier` values (0.7 → 0.85?)
   - Test different `trade_history_size` (3 → 5?)
   - Test exponential decay factors

3. **Implement Trade History Pruning**
   - Clear trades older than N days
   - Prevent indefinite accumulation
   - Keep system "fresh"

### Long-Term Actions (P3)

1. **Machine Learning for Adaptive Sizing**
   - Replace hard-coded multipliers with learned weights
   - Context-aware adjustments (volatility, trend, etc.)
   - Separate multipliers per market regime

2. **Multi-Strategy Ensemble**
   - Run parallel strategies with different warmup periods
   - Aggregate signals with confidence weighting
   - Diversify across time horizons

---

## Conclusion

The multi-day trading system has two confirmed bugs:

1. **EOD liquidation inconsistency** causing trade count discrepancies
2. **Adaptive sizing degradation** due to accumulated loss history

The proposed **trade warmup scheme** addresses both issues by:
- Providing realistic trade history without long-term accumulation
- Keeping the system "fresh" with rolling warmup windows
- Enabling production deployment with daily pre-market warmup

**Expected Outcome:** Trade warmup scheme should match or exceed single-day performance while providing the benefits of accumulated learning.

---

**Status:** Awaiting approval to implement trade warmup scheme and test on historical data.
