/**
 * TTM Squeeze/Expansion Detector Implementation
 */

#include "squeeze_detector.h"
#include <algorithm>

double SqueezeDetector::calculate_sma(const std::vector<double>& data, int period) {
    if (data.size() < (size_t)period) return 0.0;
    double sum = 0.0;
    for (size_t i = data.size() - period; i < data.size(); i++) {
        sum += data[i];
    }
    return sum / period;
}

double SqueezeDetector::calculate_std(const std::vector<double>& data, int period, double mean) {
    if (data.size() < (size_t)period) return 0.0;
    double sum_sq = 0.0;
    for (size_t i = data.size() - period; i < data.size(); i++) {
        sum_sq += (data[i] - mean) * (data[i] - mean);
    }
    return std::sqrt(sum_sq / period);
}

double SqueezeDetector::calculate_atr(const std::vector<Bar>& bars, int period) {
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

SqueezeDetector::SqueezeDetector() {
    state.is_squeezed = false;
    state.just_fired = false;
    state.bars_in_squeeze = 0;
    state.bb_width = 0.0;
    state.keltner_width = 0.0;
}

void SqueezeDetector::update(const Bar& bar, const Bar* prev_bar, const std::vector<Bar>& history) {
    price_buffer.push_back(bar.close);
    if (price_buffer.size() > (size_t)(bb_period * 2)) {
        price_buffer.erase(price_buffer.begin());
    }

    if (price_buffer.size() < (size_t)bb_period) {
        return;
    }

    // Calculate Bollinger Bands width
    double sma = calculate_sma(price_buffer, bb_period);
    double std = calculate_std(price_buffer, bb_period, sma);
    state.bb_width = sma > 0 ? (bb_std * std * 2.0) / sma : 0.0; // Normalized width

    // Calculate Keltner Channel width
    double atr = calculate_atr(history, keltner_period);
    state.keltner_width = sma > 0 ? (keltner_mult * atr * 2.0) / sma : 0.0; // Normalized width

    // Detect squeeze
    bool was_squeezed = state.is_squeezed;
    state.is_squeezed = (state.bb_width < state.keltner_width);

    if (state.is_squeezed) {
        state.bars_in_squeeze++;
        state.just_fired = false;
    } else if (was_squeezed) {
        // Just fired out of squeeze
        state.just_fired = true;
    } else {
        state.just_fired = false;
    }
}

double SqueezeDetector::get_compression_score() const {
    if (!state.is_squeezed || state.keltner_width == 0) return 0.0;
    double ratio = state.bb_width / state.keltner_width;
    return 1.0 - ratio; // Lower ratio = more compressed
}

int SqueezeDetector::get_signal() const {
    if (!state.just_fired) return 0;

    // Require minimum squeeze duration
    if (state.bars_in_squeeze < 6) return 0;

    // Signal is determined by price action after squeeze fires
    // This will be refined in backtesting
    return 0; // Neutral until we see breakout direction
}

double SqueezeDetector::get_confidence() const {
    if (!state.just_fired) return 0.0;

    // Higher confidence for longer squeeze durations
    double duration_score = std::min(1.0, state.bars_in_squeeze / 20.0);

    // Higher confidence for tighter squeezes
    double compression_score = get_compression_score();

    return (duration_score + compression_score) / 2.0;
}
