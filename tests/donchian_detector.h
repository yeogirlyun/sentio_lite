/**
 * Donchian/Prior-Day Breakout Detector Header
 */

#ifndef DONCHIAN_DETECTOR_H
#define DONCHIAN_DETECTOR_H

#include <vector>
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
    int lookback_days = 1;
    double atr_filter_mult = 0.5;
    int confirmation_bars = 3;

    DonchianState state;
    std::vector<double> daily_highs;
    std::vector<double> daily_lows;

    double calculate_atr(const std::vector<Bar>& bars, int period);

public:
    DonchianDetector();
    void update_daily_levels(double high, double low);
    void update(const Bar& bar, const Bar* prev_bar, const std::vector<Bar>& history);
    int get_signal() const;
    double get_confidence() const;
    const DonchianState& get_state() const { return state; }
};

#endif // DONCHIAN_DETECTOR_H
