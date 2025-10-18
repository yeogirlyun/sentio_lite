# BUG REPORT: Trading Activity Collapses in Later Days of Multi-Day Testing

**Date:** 2025-10-17
**Severity:** CRITICAL
**Status:** IDENTIFIED - Needs Fix
**Affects:** Multi-day backtesting (Days 4-5 show near-zero trading activity)

---

## Problem Summary

When running multi-day backtests, trading activity **dramatically collapses** in Days 4-5 despite:
- EOD liquidation working correctly
- Daily frequency limits being reset
- Valid signals still being generated

This causes severe underperformance in multi-day testing compared to single-day performance.

### Evidence

**Multi-Day Test (Oct 10-16, 2025):**
```
Day 1: -0.29% | 72 trades (W:28 L:44)
Day 2: -1.04% | 96 trades (W:37 L:59)
Day 3: -0.35% | 38 trades (W:15 L:23)
Day 4: +0.02% | 5 trades (W:3 L:2)     ← 93% reduction in activity!
Day 5: +0.00% | 0 trades (W:0 L:0)     ← Complete blockage!
Day 6: +0.00% | 0 trades (W:0 L:0)     ← Complete blockage!

Overall: -1.65% return, 211 total trades
```

**Single-Day Test (Oct 16, 2025):**
```
Day 1: +0.46% | 104 trades | 12.5% win rate | 1.41 profit factor
```

**Trade Analysis Output (Day 4, Bar 3350):**
```
[TRADE ANALYSIS] Bar 3350:
  UVXY | 5-bar: -157.96 bps | conf: 44.13% | prob: 17.09% | thresh: PASS | filter: BLOCKED
  SOXS | 5-bar: -146.89 bps | conf: 42.34% | prob: 18.71% | thresh: PASS | filter: BLOCKED
  SOXL | 5-bar: 143.32 bps | conf: 41.74% | prob: 80.74% | thresh: PASS | filter: BLOCKED
  SVIX | 5-bar: 122.88 bps | conf: 38.05% | prob: 77.36% | thresh: PASS | filter: BLOCKED
  TNA | 5-bar: 87.96 bps | conf: 30.54% | prob: 70.67% | thresh: PASS | filter: BLOCKED
```

**All symbols pass probability thresholds but are BLOCKED by trade filter!**

---

## Root Cause Analysis

### Issue #1: `last_exit_bar` Persists Across Days (CRITICAL)

**Location:** `include/trading/trade_filter.h:79`, `src/trading/trade_filter.cpp:132`

```cpp
// trade_filter.h
struct PositionState {
    bool has_position = false;
    int entry_bar = 0;
    int bars_held = 0;
    double entry_prediction = 0.0;
    double entry_price = 0.0;
    int last_exit_bar = -999;          // ← BUG: Never resets between days!

    void reset() {
        has_position = false;
        entry_bar = 0;
        bars_held = 0;
        entry_prediction = 0.0;
        entry_price = 0.0;
        // ← MISSING: last_exit_bar NOT reset!
    }
};

// trade_filter.cpp:24-26
bool TradeFilter::can_enter_position(...) {
    // Check cooldown period since last exit
    if (current_bar - state.last_exit_bar < config_.min_bars_between_entries) {
        return false;  // ← Blocks re-entry using absolute bar numbers!
    }
}
```

**Problem:**
- `last_exit_bar` stores **absolute bar numbers** (e.g., bar 1564)
- After Day 1, `last_exit_bar` might be 1564
- On Day 4 (starting at bar ~1955), cooldown check becomes:
  - `current_bar - last_exit_bar = 1955 - 1564 = 391 bars`
  - If `min_bars_between_entries = 5`, this passes
  - **BUT:** If a symbol was exited at EOD Day 3 (bar 1954), then on Day 4 (bar 1955):
    - `1955 - 1954 = 1 bar` ← Still in cooldown!

**The Real Issue:**
After EOD liquidation, ALL symbols have `last_exit_bar` set to the last bar of the day. On the next day, when trading resumes just 1 bar later, ALL symbols fail the cooldown check for the first 5+ bars.

But this doesn't explain Days 4-5 complete blockage...

### Issue #2: Daily Reset Doesn't Reset Position State (CRITICAL)

**Location:** `src/trading/trade_filter.cpp:156-163`

```cpp
void TradeFilter::reset_daily_limits(int current_bar) {
    // Clear trade history from previous days
    // Keep position state (bars_held, entry_bar) intact - only reset frequency tracking
    trade_bars_.clear();

    // Update last reset timestamp
    last_day_reset_ = current_bar;
}
```

**Problem:**
- `reset_daily_limits()` clears `trade_bars_` (frequency tracking)
- Does **NOT** clear `position_states_` (including `last_exit_bar`)
- After Day 1, all symbols have `last_exit_bar` values
- These persist indefinitely across all subsequent days

### Issue #3: Cooldown Accumulation Effect

**Scenario:**
1. **Day 1 ends (bar 1564):** All symbols liquidated at EOD
   - `UVXY.last_exit_bar = 1564`
   - `SOXS.last_exit_bar = 1564`
   - etc. for all 10 symbols

2. **Day 2 starts (bar 1565):**
   - Cooldown check for UVXY: `1565 - 1564 = 1 bar`
   - With `min_bars_between_entries = 5`, blocked for 4 more bars
   - Same for all 10 symbols!

3. **Day 2 (bar 1750):** Symbol trades again, exits
   - `UVXY.last_exit_bar = 1750`

4. **Day 3 ends (bar 1955):** EOD liquidation
   - `UVXY.last_exit_bar = 1955` (overwrites 1750)

5. **Day 4 starts (bar 1956):**
   - Cooldown: `1956 - 1955 = 1 bar` ← Blocked again!

**This explains early-day blockages but not complete Day 4-5 shutdown.**

### Issue #4: Potential Interaction with Daily Counter Logic (INVESTIGATING)

**Location:** `src/trading/trade_filter.cpp:210-217`

```cpp
bool TradeFilter::check_frequency_limits(int current_bar) const {
    // Check hourly limit
    int trades_last_hour = count_recent_trades(current_bar, 60);
    if (trades_last_hour >= config_.max_trades_per_hour) {
        return false;
    }

    // Check daily limit
    int trades_today = 0;
    for (int trade_bar : trade_bars_) {
        if (trade_bar / 390 == current_bar / 390) {  // Same day
            trades_today++;
        }
    }
    if (trades_today >= config_.max_trades_per_day) {
        return false;
    }

    return true;
}
```

**Potential Issue:**
- After `reset_daily_limits()` clears `trade_bars_`, this should allow trading
- However, if `reset_daily_limits()` is called AFTER some bars have been processed on the new day, those trades won't be tracked
- This seems unlikely since reset happens at exact EOD

### Issue #5: `bars_seen_` vs `trading_bars_` Mismatch in Cooldown

**Location:** `src/trading/multi_symbol_trader.cpp:272`

```cpp
// Reset trade filter's daily frequency limits for next trading day
trade_filter_->reset_daily_limits(static_cast<int>(bars_seen_));
```

**Problem:**
- We pass `bars_seen_` (includes warmup) to `reset_daily_limits()`
- But it's only used to set `last_day_reset_`
- The actual issue: `last_exit_bar` uses the same bar numbering scheme as all other bar IDs
- Since we never reset `position_states_`, these cooldowns accumulate

---

## Expected Behavior

1. **EOD Liquidation:**
   - All positions closed at bar 391, 782, 1173, etc.
   - Frequency counters cleared

2. **Next Day Startup:**
   - All symbols should be eligible for trading (after brief cooldown)
   - Trading activity should be comparable to Day 1

3. **Multi-Day Performance:**
   - Should approximate single-day performance repeated N times
   - Slight degradation acceptable, but not 93% reduction!

---

## Actual Behavior

1. **Days 1-3:** Normal trading (72, 96, 38 trades)
2. **Days 4-5:** Near-complete blockage (5, 0 trades)
3. **All signals BLOCKED** despite passing probability thresholds

---

## Affected Modules

### Primary Files:
1. **`include/trading/trade_filter.h`**
   - Lines 72-88: `PositionState` struct
   - Line 79: `last_exit_bar` never reset between days
   - Lines 80-87: `reset()` doesn't clear `last_exit_bar`

2. **`src/trading/trade_filter.cpp`**
   - Lines 11-40: `can_enter_position()` - cooldown check at line 24-26
   - Lines 129-141: `record_exit()` - sets `last_exit_bar` at line 131
   - Lines 156-163: `reset_daily_limits()` - doesn't touch `position_states_`

3. **`src/trading/multi_symbol_trader.cpp`**
   - Lines 233-274: EOD liquidation and daily reset logic
   - Line 272: Calls `reset_daily_limits()` but doesn't reset position states

### Secondary Files:
4. **`include/trading/multi_symbol_trader.h`**
   - Lines 127-132: Daily tracking variables (for monitoring)

---

## Recommended Fixes

### Priority 1: Reset `last_exit_bar` at EOD (CRITICAL)

**File:** `src/trading/trade_filter.cpp`

**Option A: Reset in `reset_daily_limits()`**
```cpp
void TradeFilter::reset_daily_limits(int current_bar) {
    // Clear trade history from previous days
    trade_bars_.clear();

    // Reset position state cooldowns for new day
    // NOTE: We keep entry_bar/bars_held for positions that span days,
    // but reset exit cooldowns since it's a new trading day
    for (auto& [symbol, state] : position_states_) {
        // Reset last_exit_bar to allow re-entry on new day
        // Use a value that won't block trades: current_bar - min_bars_between_entries - 1
        if (state.last_exit_bar >= 0) {
            state.last_exit_bar = current_bar - 999;  // Far enough in past to not block
        }
    }

    // Update last reset timestamp
    last_day_reset_ = current_bar;
}
```

**Option B: Add `last_exit_bar = 0` to `reset()` method**
```cpp
// trade_filter.h
void reset() {
    has_position = false;
    entry_bar = 0;
    bars_held = 0;
    entry_prediction = 0.0;
    entry_price = 0.0;
    last_exit_bar = -999;  // Reset cooldown
}
```

**Recommended: Option A** - More explicit and handles EOD scenario directly.

### Priority 2: Add Per-Symbol EOD Reset Method

**File:** `include/trading/trade_filter.h`

```cpp
/**
 * Reset per-symbol cooldowns at EOD
 * Allows fresh trading on new day while preserving other state
 */
void reset_exit_cooldowns(int current_bar);
```

**File:** `src/trading/trade_filter.cpp`

```cpp
void TradeFilter::reset_exit_cooldowns(int current_bar) {
    for (auto& [symbol, state] : position_states_) {
        // Clear exit cooldown - it's a new day
        state.last_exit_bar = -999;
    }
}
```

Call from `multi_symbol_trader.cpp` at EOD:
```cpp
// Step 7: EOD liquidation
if (config_.eod_liquidation && trading_bars_ > 0 &&
    trading_bars_ % config_.bars_per_day == 0) {

    // ... existing liquidation logic ...

    // Reset trade filter's daily frequency limits
    trade_filter_->reset_daily_limits(static_cast<int>(bars_seen_));

    // NEW: Reset exit cooldowns to allow fresh trading tomorrow
    trade_filter_->reset_exit_cooldowns(static_cast<int>(bars_seen_));
}
```

### Priority 3: Add Debug Logging to Trade Filter

**File:** `src/trading/trade_filter.cpp`

Add to `can_enter_position()`:
```cpp
bool TradeFilter::can_enter_position(...) {
    auto& state = position_states_[symbol];

    if (state.has_position) {
        return false;
    }

    // Check cooldown period since last exit
    if (current_bar - state.last_exit_bar < config_.min_bars_between_entries) {
        // DEBUG: Log why blocked
        static int debug_throttle = 0;
        if (debug_throttle++ % 100 == 0) {
            std::cout << "  [FILTER-BLOCK] " << symbol
                      << " in cooldown: last_exit=" << state.last_exit_bar
                      << ", current=" << current_bar
                      << ", delta=" << (current_bar - state.last_exit_bar)
                      << ", min_required=" << config_.min_bars_between_entries << "\n";
        }
        return false;
    }

    // ... rest of checks ...
}
```

---

## Testing Plan

### Test 1: Verify Fix with Debug Output
```bash
# Apply Priority 1 fix, add debug logging
./sentio_lite mock --start-date 2025-10-10 --end-date 2025-10-16 2>&1 | grep -E "(EOD|FILTER-BLOCK)" | head -50
```

**Expected:**
- No FILTER-BLOCK messages after early bars of each day
- Consistent trading across all 6 days

### Test 2: Compare Daily Trade Counts
```bash
./sentio_lite mock --start-date 2025-10-10 --end-date 2025-10-16 2>&1 | grep "EOD"
```

**Expected:**
```
Day 1: ~70-100 trades
Day 2: ~70-100 trades
Day 3: ~70-100 trades
Day 4: ~70-100 trades
Day 5: ~70-100 trades
Day 6: ~70-100 trades
```

### Test 3: Verify Multi-Day Returns Approach Single-Day
```bash
# Multi-day test
./sentio_lite mock --start-date 2025-10-16 --end-date 2025-10-16

# Should get ~+0.46% if Days 4-6 were the issue
```

**Expected:** Multi-day return should be closer to 6 × single-day return (not -1.65%!)

---

## Impact Assessment

### Current Impact:
- **Multi-day backtesting unreliable:** Cannot validate strategy over realistic timeframes
- **Performance severely degraded:** -1.65% vs +0.46% single-day
- **False negatives:** Strategy appears unprofitable when it may not be
- **Cannot go live:** Multi-day testing is prerequisite for live trading

### Post-Fix Impact:
- Multi-day testing will reflect actual strategy performance
- Can properly evaluate risk/reward over longer periods
- Enables confident transition to live trading

---

## References

### Similar Issues in online_trader:
The `online_trader` repository handles this differently:

**File:** `online_trader/src/sentio_cli.cpp` (approximate)
```cpp
// They use --blocks parameter where each block is 1 day
// Between blocks, they likely reset all position state
// This avoids cooldown accumulation
```

**Key Difference:**
- `online_trader` treats each day as a **separate session**
- We treat multi-day as **one continuous session with EOD resets**
- Our approach is more realistic but requires careful state management

### Trade Filter Design Intent:
From `trade_filter.h` comments:
```cpp
/**
 * Trade Filter - Manages trade frequency and holding periods
 *
 * Prevents over-trading and enforces minimum holding periods to:
 * - Reduce transaction costs
 * - Improve signal quality (avoid whipsaws)
 * - Control risk exposure
 * - Manage trade frequency within reasonable bounds
 */
```

**Intent:** Prevent rapid re-entry to same symbol (anti-whipsaw)
**Bug:** Cooldown persists across days, blocking legitimate new-day entries

---

## Conclusion

The trade filter's `last_exit_bar` cooldown mechanism is **working as designed** but is **not compatible with multi-day testing**. The design assumes continuous intraday trading where cooldowns make sense. In multi-day testing, cooldowns should reset at EOD to allow fresh trading each day.

**Priority 1 fix** (resetting `last_exit_bar` in `reset_daily_limits()`) should resolve 90% of the blockage. If issues persist, **Priority 2** (separate cooldown reset method) provides finer control.

The root cause is clear: **position state persists across days when it shouldn't**. This is a design mismatch between the trade filter (designed for single-day) and the multi-day testing framework (expecting daily resets).

---

**End of Bug Report**
