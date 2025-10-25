/**
 * Test: Ultra-Short RSI(2) / Connors Pullback Detector
 *
 * Logic: RSI(2) < 10 (oversold) or > 90 (overbought) with
 *        volume and VWAP distance guardrails
 *
 * Hypothesis: Mean-reversion edge on extreme RSI(2) readings
 *             when filtered by volume confirmation and price distance
 */

#include <iostream>
#include <vector>
#include <deque>
#include <cmath>
#include "../src/types.h"

struct RSI2State {
    double rsi_value;
    bool oversold;  // RSI < 10
    bool overbought;  // RSI > 90
    double distance_from_vwap;  // In standard deviations
    double volume_ratio;  // Current volume vs 20-bar average
    bool signal_valid;
};

class RSI2Detector {
private:
    int rsi_period = 2;
    double oversold_threshold = 10.0;
    double overbought_threshold = 90.0;
    double max_vwap_distance = 2.0;  // Max Z-score from VWAP
    double min_volume_ratio = 0.8;   // Min volume vs average

    std::deque<double> gains;
    std::deque<double> losses;
    std::deque<double> volumes;
    std::deque<double> price_x_volume;
    std::deque<double> volume_cumsum;

    RSI2State state;

    double calculate_rsi() {
        if (gains.size() < rsi_period) return 50.0;

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

    double calculate_vwap() {
        if (price_x_volume.empty() || volume_cumsum.empty()) return 0.0;

        double total_pv = 0.0;
        double total_vol = 0.0;

        for (size_t i = 0; i < price_x_volume.size(); i++) {
            total_pv += price_x_volume[i];
            total_vol += volumes[i];
        }

        return total_vol > 0 ? total_pv / total_vol : 0.0;
    }

    double calculate_vwap_std(double vwap) {
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

public:
    RSI2Detector() {
        state.rsi_value = 50.0;
        state.oversold = false;
        state.overbought = false;
        state.distance_from_vwap = 0.0;
        state.volume_ratio = 1.0;
        state.signal_valid = false;
    }

    void update(const Bar& bar, const Bar* prev_bar) {
        // Track price changes for RSI
        if (prev_bar) {
            double change = bar.close - prev_bar->close;
            gains.push_back(change > 0 ? change : 0.0);
            losses.push_back(change < 0 ? -change : 0.0);

            if (gains.size() > rsi_period + 10) {
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

    // Returns: -1 (short/fade rally), 0 (neutral), +1 (long/fade dip)
    int get_signal() const {
        if (!state.signal_valid) return 0;

        if (state.oversold) {
            // Mean-reversion: buy the dip
            return 1;
        } else if (state.overbought) {
            // Mean-reversion: sell the rip
            return -1;
        }

        return 0;
    }

    double get_confidence() const {
        if (!state.signal_valid) return 0.0;

        // Higher confidence for more extreme RSI values
        if (state.oversold) {
            return std::min(1.0, (oversold_threshold - state.rsi_value) / oversold_threshold);
        } else if (state.overbought) {
            return std::min(1.0, (state.rsi_value - overbought_threshold) / (100.0 - overbought_threshold));
        }

        return 0.0;
    }

    const RSI2State& get_state() const { return state; }
};

// Test function
void test_rsi2_detector() {
    std::cout << "\n=== RSI(2) / Connors Pullback Detector Test ===\n";
    std::cout << "Hypothesis: Extreme RSI(2) readings offer mean-reversion edge\n";
    std::cout << "            when filtered by volume and VWAP distance\n\n";

    // TODO: Load historical data and test
    // Metrics to track:
    // - Signal frequency
    // - Win rate on oversold (buy signal)
    // - Win rate on overbought (sell signal)
    // - Average reversion time
    // - Impact of VWAP/volume filters

    std::cout << "Test metrics:\n";
    std::cout << "1. Oversold signals/day: __\n";
    std::cout << "2. Overbought signals/day: __\n";
    std::cout << "3. Buy signal win rate: __%\n";
    std::cout << "4. Sell signal win rate: __%\n";
    std::cout << "5. Avg bars to reversion: __\n";
    std::cout << "6. Filter effectiveness: __%\n";
}

int main() {
    test_rsi2_detector();
    return 0;
}
