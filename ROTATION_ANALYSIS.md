# Rotation Strategy Analysis - Issues Found

## Problem: Fixed ~104 Trades Per Day Regardless of Market Conditions

### Root Cause Analysis

#### 1. **Min Bars to Hold Dominates Over Signal Quality**

**Current Behavior** (`multi_symbol_trader.h:137-139`):
```cpp
filter_config.min_bars_to_hold = 5;   // FORCING exits every 5 bars!
filter_config.typical_hold_period = 20;
filter_config.max_bars_to_hold = 60;
```

**Problem:** After holding for 5 bars, positions exit on **FIRST opportunity** regardless of:
- Signal strength
- Whether there's a better alternative
- Current profitability
- Market conditions

**Math:**
- Trading period: 391 bars per day
- Minimum hold: 5 bars
- Theoretical max: 391 / 5 = 78 trades per symbol
- With 3 positions rotating: ~234 total decisions
- Actual: ~104 trades suggests 50% are exits

#### 2. **No Rank-Based Rotation Logic**

**Sentio Lite Current Approach** (`multi_symbol_trader.cpp:454-459`):
```cpp
// ROTATION LOGIC REMOVED: Positions are now only exited when signals deteriorate
// This is handled in update_positions() which checks:
// - Emergency stop loss
// - Profit target reached
// - Signal quality degraded
// - Signal reversed direction
```

**Problem:** The comment says "rotation logic removed" but then exits happen anyway due to time-based rules!

**Online Trader Rotation Approach** (`rotation_position_manager.cpp:264-315`):
```cpp
// Step 3: Check if rotation needed (better signal available)
if (available_slots == 0 && should_rotate(ranked_signals)) {
    // Find weakest current position
    std::string weakest = find_weakest_position();

    // Check if significantly stronger
    double strength_delta = ranked_signal.strength - weakest_pos.current_strength;

    if (strength_delta >= config_.rotation_strength_delta) {
        // ONLY THEN rotate out weakest for stronger signal
        decisions.push_back(exit_pd);  // Rotate out
        decisions.push_back(enter_pd); // Rotate in
    }
}
```

**Key Differences:**
- ✅ **Online Trader**: Rotates ONLY when better signal (rank-based, strength delta required)
- ❌ **Sentio Lite**: Exits after 5 bars regardless of alternatives

#### 3. **Exit Reasons Are Misleading**

**Current Exit Logic** (`multi_symbol_trader.cpp:559-570`):
```cpp
if (pnl_pct < config_.filter_config.emergency_stop_loss_pct) {
    reason = "EmergencyStop";
} else if (pnl_pct > config_.filter_config.profit_target_multiple * 0.01) {
    reason = "ProfitTarget";
} else if (bars_held >= config_.filter_config.max_bars_to_hold) {
    reason = "MaxHold";
} else if (bars_held >= config_.filter_config.min_bars_to_hold) {
    reason = "SignalExit";  // ← MISLEADING! Should be "TimeBasedExit"
}
```

**Problem:** "SignalExit" implies the signal deteriorated, but it's actually just "held for 5+ bars".

#### 4. **Trade Filter Exit Logic** (`trade_filter.cpp:79-93`)

After minimum hold period (5 bars), exits trigger on:
- Signal confidence drops (any amount)
- Signal reverses direction (any amount)
- Approaching typical hold period (20 bars)

**Problem:** These thresholds are too sensitive, causing premature exits.

## Comparison Table

| Aspect | Online Trader | Sentio Lite (Current) |
|--------|---------------|----------------------|
| **Entry Decision** | Rank-based (top N by strength) | Rank-based (similar) ✅ |
| **Exit Decision** | Rotation only if better signal | Time-based (5+ bars) ❌ |
| **Rotation Trigger** | `strength_delta >= threshold` | None ❌ |
| **Weakest Position** | `find_weakest_position()` | None ❌ |
| **Exit Cooldown** | Prevents re-entry churning | None ❌ |
| **Signal Quality** | Primary driver | Secondary to time ❌ |
| **Trades/Day** | Variable (signal-dependent) | Fixed (~104) ❌ |

## Recommended Fixes

### Priority 1: Implement Rank-Based Rotation

**Add to `multi_symbol_trader.cpp`:**

```cpp
// After entry logic in make_trades(), add rotation check:
if (top_symbols.size() >= config_.max_positions) {
    // All slots filled - check if rotation is warranted

    for (const auto& [symbol, pred_data] : predictions) {
        if (top_symbols.size() >= ranked.size()) break;

        const auto& new_signal = ranked[top_symbols.size()]; // Next best signal

        // Find weakest current position by signal strength
        Symbol weakest_symbol;
        double min_strength = std::numeric_limits<double>::max();

        for (const auto& held_symbol : top_symbols) {
            if (positions_.count(held_symbol)) {
                auto& pred = predictions.at(held_symbol);
                double strength = std::abs(pred.prediction.pred_5bar.prediction);
                if (strength < min_strength) {
                    min_strength = strength;
                    weakest_symbol = held_symbol;
                }
            }
        }

        // Rotate ONLY if new signal significantly stronger
        double strength_delta = new_signal.second - min_strength;
        double rotation_threshold = 0.002; // 20 bps minimum improvement

        if (strength_delta >= rotation_threshold) {
            // Exit weakest
            exit_position(weakest_symbol, market_data.at(weakest_symbol).close,
                         market_data.at(weakest_symbol).timestamp,
                         market_data.at(weakest_symbol).bar_id);

            // Enter stronger signal
            enter_position(new_signal.first, market_data.at(new_signal.first).close,
                          market_data.at(new_signal.first).timestamp,
                          position_capital,
                          market_data.at(new_signal.first).bar_id);
        }
    }
}
```

### Priority 2: Increase Minimum Hold Period

**Change** `multi_symbol_trader.h:137`:
```cpp
filter_config.min_bars_to_hold = 20;  // Was 5 - increase to reduce churn
filter_config.typical_hold_period = 60;  // Was 20
filter_config.max_bars_to_hold = 120;  // Was 60
```

**Rationale:** With 1-minute bars, 5 minutes is too short for signal validation.

### Priority 3: Add Exit Cooldown

**Add to `trade_filter.h`:**
```cpp
int exit_cooldown_bars = 10;  // Prevent re-entry for 10 bars after exit
std::map<Symbol, int> exit_cooldowns_;
```

### Priority 4: Fix Exit Reasons

**Update** `multi_symbol_trader.cpp:559-570`:
```cpp
std::string reason = "Unknown";
if (pnl_pct < config_.filter_config.emergency_stop_loss_pct) {
    reason = "EmergencyStop";
} else if (pnl_pct > config_.filter_config.profit_target_multiple * 0.01) {
    reason = "ProfitTarget";
} else if (bars_held >= config_.filter_config.max_bars_to_hold) {
    reason = "MaxHold";
} else if (was_rotated) {  // NEW: track if rotation triggered exit
    reason = "RotatedOut";
} else if (bars_held >= config_.filter_config.min_bars_to_hold) {
    reason = "SignalDeteriorated";  // More accurate!
}
```

### Priority 5: Adjust Signal Quality Thresholds

**Update** `trade_filter.cpp` exit conditions to be less sensitive:

```cpp
// Before: Exit on ANY confidence drop
if (prediction.pred_5bar.confidence < config_.exit_confidence_threshold) {
    return true;
}

// After: Exit only on SIGNIFICANT confidence drop (30%+)
double confidence_drop = state.entry_confidence - prediction.pred_5bar.confidence;
if (confidence_drop > 0.30) {  // Require 30% drop, not any drop
    return true;
}
```

## Expected Impact

After implementing these fixes:

| Metric | Before | After (Expected) |
|--------|--------|------------------|
| Trades/Day | ~104 (fixed) | 20-60 (variable) |
| Exit Reason | 80% "SignalExit" | 60% "RotatedOut", 20% real deterioration |
| Avg Hold Period | ~5-10 bars | 20-40 bars |
| Signal Quality Impact | Low (time dominates) | High (primary driver) |
| Churning | High | Low (cooldown prevents) |

## Testing Plan

1. **Backttest with fixes** on Oct 13-17, 2025
2. **Compare trade counts** - should be variable, not fixed
3. **Analyze exit reasons** - should show rotation dominance
4. **Check hold periods** - should cluster around signal quality, not time limits
5. **Verify performance** - should improve with better signal-driven decisions
