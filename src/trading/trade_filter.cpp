#include "trading/trade_filter.h"
#include <algorithm>
#include <cmath>

namespace trading {

TradeFilter::TradeFilter(const Config& config)
    : config_(config)
    , last_day_reset_(0) {}

bool TradeFilter::can_enter_position(
    const Symbol& symbol,
    int current_bar,
    const MultiHorizonPredictor::MultiHorizonPrediction& prediction) {

    auto& state = position_states_[symbol];

    // Already have a position
    if (state.has_position) {
        return false;
    }

    // Check cooldown period since last exit
    if (current_bar - state.last_exit_bar < config_.min_bars_between_entries) {
        return false;
    }

    // Check global frequency limits
    if (!check_frequency_limits(current_bar)) {
        return false;
    }

    // Check prediction quality using multi-horizon logic
    if (!prediction.should_enter(config_.min_prediction_for_entry,
                                 config_.min_confidence_for_entry)) {
        return false;
    }

    return true;
}

bool TradeFilter::should_exit_position(
    const Symbol& symbol,
    int current_bar,
    const MultiHorizonPredictor::MultiHorizonPrediction& prediction,
    double current_price) {

    auto it = position_states_.find(symbol);
    if (it == position_states_.end() || !it->second.has_position) {
        return false;  // No position to exit
    }

    const auto& state = it->second;
    int bars_held = current_bar - state.entry_bar;
    double current_pnl_pct = calculate_pnl_pct(symbol, current_price);

    // 1. Emergency stop loss (overrides minimum hold)
    if (current_pnl_pct < config_.emergency_stop_loss_pct) {
        return true;
    }

    // 2. Enforce minimum holding period (except for emergency)
    if (bars_held < config_.min_bars_to_hold) {
        return false;
    }

    // 3. Maximum holding period reached
    if (bars_held >= config_.max_bars_to_hold) {
        return true;
    }

    // 4. Profit target reached
    // Expected return = entry_prediction * bars_held (simple linear model)
    double expected_return = state.entry_prediction * bars_held;
    if (current_pnl_pct > expected_return * config_.profit_target_multiple) {
        return true;
    }

    // 5. Signal quality degraded significantly
    // Use the 5-bar prediction as primary signal
    if (prediction.pred_5bar.confidence < config_.exit_confidence_threshold) {
        return true;
    }

    // 6. Signal reversed direction
    if (state.entry_prediction > 0 &&
        prediction.pred_5bar.prediction < config_.exit_signal_reversed_threshold) {
        return true;  // Was bullish, now bearish
    }
    if (state.entry_prediction < 0 &&
        prediction.pred_5bar.prediction > -config_.exit_signal_reversed_threshold) {
        return true;  // Was bearish, now bullish
    }

    // 7. Adaptive exit threshold based on holding duration
    // As we approach typical_hold_period, lower the bar for exit
    if (bars_held >= config_.typical_hold_period) {
        double progress = static_cast<double>(bars_held - config_.typical_hold_period) /
                         (config_.max_bars_to_hold - config_.typical_hold_period);
        double adaptive_threshold = config_.min_confidence_for_entry *
                                   (1.0 - 0.3 * progress);  // Reduce by up to 30%

        if (prediction.pred_5bar.confidence < adaptive_threshold) {
            return true;
        }
    }

    return false;
}

void TradeFilter::record_entry(const Symbol& symbol, int entry_bar,
                               double entry_prediction, double entry_price) {
    auto& state = position_states_[symbol];
    state.has_position = true;
    state.entry_bar = entry_bar;
    state.bars_held = 0;
    state.entry_prediction = entry_prediction;
    state.entry_price = entry_price;

    // Record trade for frequency tracking
    trade_bars_.push_back(entry_bar);

    // Keep only recent trades (last 500 bars ~ 1 day of minute data)
    while (trade_bars_.size() > 500) {
        trade_bars_.pop_front();
    }
}

void TradeFilter::record_exit(const Symbol& symbol, int exit_bar) {
    auto& state = position_states_[symbol];
    state.last_exit_bar = exit_bar;
    state.reset();

    // Record trade for frequency tracking
    trade_bars_.push_back(exit_bar);

    // Keep only recent trades
    while (trade_bars_.size() > 500) {
        trade_bars_.pop_front();
    }
}

void TradeFilter::update_bars_held(int current_bar) {
    for (auto& [symbol, state] : position_states_) {
        if (state.has_position) {
            state.bars_held = current_bar - state.entry_bar;
        }
    }

    // Reset daily counter (assuming 390 bars per day)
    if (current_bar / 390 > last_day_reset_ / 390) {
        last_day_reset_ = current_bar;
    }
}

void TradeFilter::reset_daily_limits(int current_bar) {
    // Keep recent history, only remove trades older than 1 day
    // This preserves frequency limits while allowing fresh daily starts
    int cutoff_bar = current_bar - 390;  // Keep last ~1 day of trades
    while (!trade_bars_.empty() && trade_bars_.front() < cutoff_bar) {
        trade_bars_.pop_front();
    }

    // Only reset exit bars if cooldown has truly expired
    // Don't unconditionally reset - preserve cross-day cooldowns if recent
    for (auto& [symbol, state] : position_states_) {
        if (!state.has_position) {
            int bars_since_exit = current_bar - state.last_exit_bar;
            // Only reset if exit was more than 2x the minimum cooldown ago
            // This prevents immediate re-entry after recent EOD liquidation
            if (bars_since_exit > config_.min_bars_between_entries * 2) {
                state.last_exit_bar = -999;
            }
            // Otherwise keep the actual exit bar to enforce proper cooldown
        }
        // Keep position state intact for active positions (bars_held, entry_bar, etc.)
    }

    // Update last reset timestamp
    last_day_reset_ = current_bar;
}

const TradeFilter::PositionState& TradeFilter::get_position_state(const Symbol& symbol) const {
    static PositionState empty_state;
    auto it = position_states_.find(symbol);
    if (it != position_states_.end()) {
        return it->second;
    }
    return empty_state;
}

bool TradeFilter::has_position(const Symbol& symbol) const {
    auto it = position_states_.find(symbol);
    return it != position_states_.end() && it->second.has_position;
}

int TradeFilter::get_bars_held(const Symbol& symbol) const {
    return get_position_state(symbol).bars_held;
}

TradeFilter::TradeStats TradeFilter::get_trade_stats(int current_bar) const {
    TradeStats stats;
    stats.total_entries = 0;  // Could track separately if needed
    stats.total_exits = 0;
    stats.trades_last_hour = count_recent_trades(current_bar, 60);

    // Count trades today (since last daily reset)
    stats.trades_today = 0;
    for (int trade_bar : trade_bars_) {
        if (trade_bar / 390 == current_bar / 390) {
            stats.trades_today++;
        }
    }

    return stats;
}

int TradeFilter::count_recent_trades(int current_bar, int window_bars) const {
    int count = 0;
    for (int trade_bar : trade_bars_) {
        if (current_bar - trade_bar <= window_bars) {
            count++;
        }
    }
    return count;
}

bool TradeFilter::check_frequency_limits(int current_bar) const {
    // For multi-day testing, only count trades from current day
    int current_day = current_bar / 390;

    // Count trades today only
    int trades_today = 0;
    for (int trade_bar : trade_bars_) {
        if (trade_bar / 390 == current_day) {
            trades_today++;
        }
    }

    // Hourly check should also be day-aware to prevent cross-day counting
    int trades_last_hour = 0;
    for (int trade_bar : trade_bars_) {
        if (trade_bar / 390 == current_day &&    // Same day
            current_bar - trade_bar <= 60) {     // Within hour
            trades_last_hour++;
        }
    }

    // Check limits
    if (trades_today >= config_.max_trades_per_day) {
        return false;
    }
    if (trades_last_hour >= config_.max_trades_per_hour) {
        return false;
    }

    return true;
}

double TradeFilter::calculate_pnl_pct(const Symbol& symbol, double current_price) const {
    auto it = position_states_.find(symbol);
    if (it == position_states_.end() || !it->second.has_position) {
        return 0.0;
    }

    const auto& state = it->second;
    if (state.entry_price == 0.0) {
        return 0.0;
    }

    return (current_price - state.entry_price) / state.entry_price;
}

} // namespace trading
