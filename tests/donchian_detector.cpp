/**
 * Donchian/Prior-Day Breakout Detector Implementation
 */

#include "donchian_detector.h"
#include <algorithm>
#include <cmath>

double DonchianDetector::calculate_atr(const std::vector<Bar>& bars, int period) {
    if (bars.size() < (size_t)(period + 1)) return 0.0;
    double sum_tr = 0.0;
    for (size_t i = bars.size() - period; i < bars.size(); i++) {
        double high_low = bars[i].high - bars[i].low;
        double high_close = std::abs(bars[i].high - bars[i-1].close);
        double low_close = std::abs(bars[i].low - bars[i-1].close);
        sum_tr += std::max({high_low, high_close, low_close});
    }
    return sum_tr / period;
}

DonchianDetector::DonchianDetector() {
    state.prior_day_high = 0.0;
    state.prior_day_low = 0.0;
    state.current_atr = 0.0;
    state.bullish_breakout = false;
    state.bearish_breakout = false;
    state.failed_breakout = false;
    state.bars_since_breakout = 0;
}

void DonchianDetector::update_daily_levels(double high, double low) {
    daily_highs.push_back(high);
    daily_lows.push_back(low);

    if (daily_highs.size() > (size_t)(lookback_days + 1)) {
        daily_highs.erase(daily_highs.begin());
        daily_lows.erase(daily_lows.begin());
    }

    if (daily_highs.size() > (size_t)lookback_days) {
        state.prior_day_high = *std::max_element(
            daily_highs.begin(),
            daily_highs.end() - 1
        );
        state.prior_day_low = *std::min_element(
            daily_lows.begin(),
            daily_lows.end() - 1
        );
    }
}

void DonchianDetector::update(const Bar& bar, const Bar* prev_bar, const std::vector<Bar>& history) {
    state.current_atr = calculate_atr(history, 20);

    if (state.prior_day_high == 0.0) return;

    double atr_threshold = state.current_atr * atr_filter_mult;

    // Check for new breakouts
    bool new_bullish = bar.close > (state.prior_day_high + atr_threshold);
    bool new_bearish = bar.close < (state.prior_day_low - atr_threshold);

    // Track breakout state
    if (new_bullish && !state.bullish_breakout) {
        state.bullish_breakout = true;
        state.bearish_breakout = false;
        state.bars_since_breakout = 0;
        state.failed_breakout = false;
    } else if (new_bearish && !state.bearish_breakout) {
        state.bearish_breakout = true;
        state.bullish_breakout = false;
        state.bars_since_breakout = 0;
        state.failed_breakout = false;
    }

    // Check for failed breakouts (reversion back into range)
    if (state.bullish_breakout) {
        state.bars_since_breakout++;
        if (bar.close < state.prior_day_high) {
            state.failed_breakout = true;
            state.bullish_breakout = false;
        }
    } else if (state.bearish_breakout) {
        state.bars_since_breakout++;
        if (bar.close > state.prior_day_low) {
            state.failed_breakout = true;
            state.bearish_breakout = false;
        }
    }
}

int DonchianDetector::get_signal() const {
    // Fade failed breakouts (contrarian)
    if (state.failed_breakout) {
        return state.bullish_breakout ? -1 : 1;  // Reversed
    }

    // Follow confirmed breakouts (trend)
    if (state.bullish_breakout && state.bars_since_breakout >= confirmation_bars) {
        return 1;
    }
    if (state.bearish_breakout && state.bars_since_breakout >= confirmation_bars) {
        return -1;
    }

    return 0;
}

double DonchianDetector::get_confidence() const {
    if (state.current_atr == 0.0) return 0.0;

    // Higher confidence for longer breakout durations
    double duration_factor = std::min(1.0, state.bars_since_breakout / 10.0);

    // Failed breakouts have high confidence for mean reversion
    if (state.failed_breakout) {
        return 0.8;
    }

    return duration_factor;
}
