/**
 * RSI(2) Pullback Detector Implementation
 */

#include "rsi2_detector.h"
#include <cmath>
#include <algorithm>

double RSI2Detector::calculate_rsi() {
    if (gains.size() < (size_t)rsi_period) return 50.0;

    double avg_gain = 0.0;
    double avg_loss = 0.0;

    for (size_t i = gains.size() - rsi_period; i < gains.size(); i++) {
        avg_gain += gains[i];
        avg_loss += losses[i];
    }

    avg_gain /= rsi_period;
    avg_loss /= rsi_period;

    if (avg_loss == 0.0) return 100.0;

    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

double RSI2Detector::calculate_vwap() {
    if (price_x_volume.empty() || volumes.empty()) return 0.0;

    double total_pv = 0.0;
    double total_vol = 0.0;

    for (size_t i = 0; i < price_x_volume.size(); i++) {
        total_pv += price_x_volume[i];
        total_vol += volumes[i];
    }

    return total_vol > 0 ? total_pv / total_vol : 0.0;
}

double RSI2Detector::calculate_vwap_std(double vwap) {
    if (price_x_volume.empty()) return 0.0;

    double sum_sq = 0.0;
    double total_vol = 0.0;

    for (size_t i = 0; i < price_x_volume.size(); i++) {
        double price = price_x_volume[i] / volumes[i];
        sum_sq += volumes[i] * (price - vwap) * (price - vwap);
        total_vol += volumes[i];
    }

    return total_vol > 0 ? std::sqrt(sum_sq / total_vol) : 0.0;
}

RSI2Detector::RSI2Detector() {
    state.rsi_value = 50.0;
    state.oversold = false;
    state.overbought = false;
    state.distance_from_vwap = 0.0;
    state.volume_ratio = 1.0;
    state.signal_valid = false;
}

void RSI2Detector::update(const Bar& bar, const Bar* prev_bar, const std::vector<Bar>& history) {
    // Track price changes for RSI
    if (prev_bar) {
        double change = bar.close - prev_bar->close;
        gains.push_back(change > 0 ? change : 0.0);
        losses.push_back(change < 0 ? -change : 0.0);

        if (gains.size() > (size_t)(rsi_period + 10)) {
            gains.pop_front();
            losses.pop_front();
        }
    }

    // Track volume
    volumes.push_back(bar.volume);
    price_x_volume.push_back(bar.close * bar.volume);

    if (volumes.size() > 20) {
        volumes.pop_front();
        price_x_volume.pop_front();
    }

    // Calculate RSI(2)
    state.rsi_value = calculate_rsi();
    state.oversold = (state.rsi_value < oversold_threshold);
    state.overbought = (state.rsi_value > overbought_threshold);

    // Calculate VWAP distance
    double vwap = calculate_vwap();
    if (vwap > 0) {
        double vwap_std = calculate_vwap_std(vwap);
        state.distance_from_vwap = vwap_std > 0 ?
            (bar.close - vwap) / vwap_std : 0.0;
    }

    // Calculate volume ratio
    if (volumes.size() >= 20) {
        double avg_volume = 0.0;
        for (const auto& vol : volumes) {
            avg_volume += vol;
        }
        avg_volume /= volumes.size();
        state.volume_ratio = bar.volume / avg_volume;
    }

    // Validate signal with guardrails
    state.signal_valid = (std::abs(state.distance_from_vwap) < max_vwap_distance) &&
                        (state.volume_ratio >= min_volume_ratio);
}

int RSI2Detector::get_signal() const {
    if (!state.signal_valid) return 0;

    if (state.oversold) {
        return 1;  // Buy the dip
    } else if (state.overbought) {
        return -1;  // Sell the rip
    }

    return 0;
}

double RSI2Detector::get_confidence() const {
    if (!state.signal_valid) return 0.0;

    // Higher confidence for more extreme RSI values
    if (state.oversold) {
        return std::min(1.0, (oversold_threshold - state.rsi_value) / oversold_threshold);
    } else if (state.overbought) {
        return std::min(1.0, (state.rsi_value - overbought_threshold) / (100.0 - overbought_threshold));
    }

    return 0.0;
}
