/**
 * Test: TTM Squeeze/Expansion Detector
 *
 * Logic: BB width < Keltner width → squeeze state
 *        Break from squeeze + retest → continuation entry
 *
 * Hypothesis: Volatility compression followed by expansion
 *             provides directional edge on breakouts
 */

#include <iostream>
#include <vector>
#include <cmath>
#include "../src/types.h"

struct SqueezeState {
    bool is_squeezed;
    bool just_fired;
    double bb_width;
    double keltner_width;
    int bars_in_squeeze;
};

class SqueezeDetector {
private:
    int bb_period = 20;
    int keltner_period = 20;
    double bb_std = 2.0;
    double keltner_mult = 1.5;

    std::vector<double> price_buffer;
    SqueezeState state;

    double calculate_sma(const std::vector<double>& data, int period) {
        if (data.size() < period) return 0.0;
        double sum = 0.0;
        for (int i = data.size() - period; i < data.size(); i++) {
            sum += data[i];
        }
        return sum / period;
    }

    double calculate_std(const std::vector<double>& data, int period, double mean) {
        if (data.size() < period) return 0.0;
        double sum_sq = 0.0;
        for (int i = data.size() - period; i < data.size(); i++) {
            sum_sq += (data[i] - mean) * (data[i] - mean);
        }
        return std::sqrt(sum_sq / period);
    }

    double calculate_atr(const std::vector<Bar>& bars, int period) {
        if (bars.size() < period + 1) return 0.0;
        double sum_tr = 0.0;
        for (int i = bars.size() - period; i < bars.size(); i++) {
            double high_low = bars[i].high - bars[i].low;
            double high_close = std::abs(bars[i].high - bars[i-1].close);
            double low_close = std::abs(bars[i].low - bars[i-1].close);
            sum_tr += std::max({high_low, high_close, low_close});
        }
        return sum_tr / period;
    }

public:
    SqueezeDetector() {
        state.is_squeezed = false;
        state.just_fired = false;
        state.bars_in_squeeze = 0;
    }

    void update(const Bar& bar, const std::vector<Bar>& history) {
        price_buffer.push_back(bar.close);
        if (price_buffer.size() > bb_period * 2) {
            price_buffer.erase(price_buffer.begin());
        }

        if (price_buffer.size() < bb_period) {
            return;
        }

        // Calculate Bollinger Bands width
        double sma = calculate_sma(price_buffer, bb_period);
        double std = calculate_std(price_buffer, bb_period, sma);
        state.bb_width = (bb_std * std * 2.0) / sma; // Normalized width

        // Calculate Keltner Channel width
        double atr = calculate_atr(history, keltner_period);
        state.keltner_width = (keltner_mult * atr * 2.0) / sma; // Normalized width

        // Detect squeeze
        bool was_squeezed = state.is_squeezed;
        state.is_squeezed = (state.bb_width < state.keltner_width);

        if (state.is_squeezed) {
            state.bars_in_squeeze++;
        } else if (was_squeezed) {
            // Just fired out of squeeze
            state.just_fired = true;
            state.bars_in_squeeze = 0;
        } else {
            state.just_fired = false;
        }
    }

    // Signal strength: 0 (no squeeze) to 1 (max compression)
    double get_compression_score() const {
        if (!state.is_squeezed) return 0.0;
        double ratio = state.bb_width / state.keltner_width;
        return 1.0 - ratio; // Lower ratio = more compressed
    }

    // Returns: -1 (short), 0 (neutral), +1 (long)
    int get_breakout_signal(double current_price, double prev_high, double prev_low) const {
        if (!state.just_fired) return 0;

        // Require minimum squeeze duration
        if (state.bars_in_squeeze < 6) return 0;

        // Breakout direction based on which bound was broken
        if (current_price > prev_high) return 1;
        if (current_price < prev_low) return -1;

        return 0;
    }

    const SqueezeState& get_state() const { return state; }
};

// Test function
void test_squeeze_detector() {
    std::cout << "\n=== TTM Squeeze/Expansion Detector Test ===\n";
    std::cout << "Hypothesis: Volatility compression → expansion provides edge\n\n";

    // TODO: Load historical data and test
    // Metrics to track:
    // - Average squeeze duration before breakout
    // - Win rate on breakout signals
    // - Average move size after breakout
    // - False breakout rate

    std::cout << "Test metrics:\n";
    std::cout << "1. Squeeze frequency: __%\n";
    std::cout << "2. Avg bars in squeeze: __\n";
    std::cout << "3. Breakout win rate: __%\n";
    std::cout << "4. Avg breakout move: __bps\n";
    std::cout << "5. False breakout rate: __%\n";
}

int main() {
    test_squeeze_detector();
    return 0;
}
