/**
 * Test: Donchian/Prior-Day High-Low Breakout Detector
 *
 * Logic: Break above prior-day high or below prior-day low
 *        with volatility-scaled filters. Fade failed breakouts.
 *
 * Hypothesis: True breakouts continue, false breakouts revert.
 *             ATR scaling reduces noise in low-volatility periods.
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include "../src/types.h"

struct DonchianState {
    double prior_day_high;
    double prior_day_low;
    double current_atr;
    bool bullish_breakout;
    bool bearish_breakout;
    bool failed_breakout;
    int bars_since_breakout;
};

class DonchianDetector {
private:
    int lookback_days = 1;  // Prior N days
    double atr_filter_mult = 0.5;  // Minimum move as % of ATR
    int confirmation_bars = 3;  // Bars to confirm/fail breakout

    DonchianState state;
    std::vector<double> daily_highs;
    std::vector<double> daily_lows;

    double calculate_atr(const std::vector<Bar>& bars, int period) {
        if (bars.size() < period + 1) return 0.0;
        double sum_tr = 0.0;
        for (size_t i = bars.size() - period; i < bars.size(); i++) {
            double high_low = bars[i].high - bars[i].low;
            double high_close = std::abs(bars[i].high - bars[i-1].close);
            double low_close = std::abs(bars[i].low - bars[i-1].close);
            sum_tr += std::max({high_low, high_close, low_close});
        }
        return sum_tr / period;
    }

public:
    DonchianDetector() {
        state.prior_day_high = 0.0;
        state.prior_day_low = 0.0;
        state.current_atr = 0.0;
        state.bullish_breakout = false;
        state.bearish_breakout = false;
        state.failed_breakout = false;
        state.bars_since_breakout = 0;
    }

    void update_daily_levels(double high, double low) {
        daily_highs.push_back(high);
        daily_lows.push_back(low);

        if (daily_highs.size() > lookback_days + 1) {
            daily_highs.erase(daily_highs.begin());
            daily_lows.erase(daily_lows.begin());
        }

        if (daily_highs.size() > lookback_days) {
            // Calculate prior N days high/low (excluding today)
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

    void update(const Bar& bar, const std::vector<Bar>& history) {
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

    // Returns: -1 (fade/short), 0 (neutral), +1 (breakout/long)
    int get_signal() const {
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

    double get_confidence() const {
        if (state.current_atr == 0.0) return 0.0;

        // Higher confidence for larger moves relative to ATR
        double move_size = 0.0;
        if (state.bullish_breakout) {
            // Approximation - would need current price
            move_size = 1.0;  // Placeholder
        } else if (state.bearish_breakout) {
            move_size = 1.0;  // Placeholder
        }

        return std::min(1.0, move_size / state.current_atr);
    }

    const DonchianState& get_state() const { return state; }
};

// Test function
void test_donchian_detector() {
    std::cout << "\n=== Donchian/Prior-Day Breakout Detector Test ===\n";
    std::cout << "Hypothesis: True breakouts continue, failed breakouts revert\n\n";

    // TODO: Load historical data and test
    // Metrics to track:
    // - Breakout frequency
    // - True vs false breakout ratio
    // - Win rate on breakout follows
    // - Win rate on failed breakout fades
    // - ATR filter effectiveness

    std::cout << "Test metrics:\n";
    std::cout << "1. Breakout frequency: __/day\n";
    std::cout << "2. False breakout rate: __%\n";
    std::cout << "3. Trend follow win rate: __%\n";
    std::cout << "4. Fade win rate: __%\n";
    std::cout << "5. Avg move on true breakout: __bps\n";
    std::cout << "6. ATR filter impact: __% noise reduction\n";
}

int main() {
    test_donchian_detector();
    return 0;
}
