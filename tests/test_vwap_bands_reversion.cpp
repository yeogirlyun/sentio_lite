/**
 * Test: VWAP Bands Mean-Reversion Detector
 *
 * Logic: Price stretches to ±Z × VWAP_StdDev → mean revert
 *        No-go zone: price significantly above/below multi-session VWAP
 *
 * Hypothesis: Extreme deviations from VWAP revert to mean,
 *             but avoid counter-trend when multi-session bias present
 */

#include <iostream>
#include <vector>
#include <deque>
#include <cmath>
#include "../src/types.h"

struct VWAPBandsState {
    double current_vwap;
    double vwap_std;
    double z_score;  // Current distance in std devs
    double multi_session_vwap;  // 5-day rolling VWAP
    double multi_session_bias;  // % above/below multi-session
    bool overextended_long;
    bool overextended_short;
    bool in_no_go_zone;
};

class VWAPBandsDetector {
private:
    double entry_z_threshold = 2.0;  // Enter at ±2 std devs
    double exit_z_threshold = 0.5;   // Exit near VWAP
    double no_go_threshold = 1.5;    // % away from multi-session VWAP

    std::deque<double> intraday_pv;  // Price × Volume
    std::deque<double> intraday_vol;
    std::deque<double> daily_vwaps;  // For multi-session VWAP

    VWAPBandsState state;

    double calculate_vwap(const std::deque<double>& pv, const std::deque<double>& vol) {
        if (pv.empty() || vol.empty()) return 0.0;

        double total_pv = 0.0;
        double total_vol = 0.0;

        for (size_t i = 0; i < pv.size(); i++) {
            total_pv += pv[i];
            total_vol += vol[i];
        }

        return total_vol > 0 ? total_pv / total_vol : 0.0;
    }

    double calculate_vwap_std(const std::deque<double>& pv, const std::deque<double>& vol, double vwap) {
        if (pv.empty() || vol.empty()) return 0.0;

        double sum_sq = 0.0;
        double total_vol = 0.0;

        for (size_t i = 0; i < pv.size(); i++) {
            double price = pv[i] / vol[i];
            sum_sq += vol[i] * (price - vwap) * (price - vwap);
            total_vol += vol[i];
        }

        return total_vol > 0 ? std::sqrt(sum_sq / total_vol) : 0.0;
    }

public:
    VWAPBandsDetector() {
        state.current_vwap = 0.0;
        state.vwap_std = 0.0;
        state.z_score = 0.0;
        state.multi_session_vwap = 0.0;
        state.multi_session_bias = 0.0;
        state.overextended_long = false;
        state.overextended_short = false;
        state.in_no_go_zone = false;
    }

    void update(const Bar& bar) {
        // Intraday VWAP tracking
        intraday_pv.push_back(bar.close * bar.volume);
        intraday_vol.push_back(bar.volume);

        state.current_vwap = calculate_vwap(intraday_pv, intraday_vol);
        state.vwap_std = calculate_vwap_std(intraday_pv, intraday_vol, state.current_vwap);

        // Calculate Z-score
        if (state.vwap_std > 0) {
            state.z_score = (bar.close - state.current_vwap) / state.vwap_std;
        }

        // Update multi-session VWAP
        if (daily_vwaps.size() > 0) {
            double sum = 0.0;
            for (const auto& vwap : daily_vwaps) {
                sum += vwap;
            }
            state.multi_session_vwap = sum / daily_vwaps.size();

            // Calculate bias
            if (state.multi_session_vwap > 0) {
                state.multi_session_bias =
                    ((bar.close - state.multi_session_vwap) / state.multi_session_vwap) * 100.0;
            }
        }

        // Determine overextension state
        state.overextended_long = (state.z_score > entry_z_threshold);
        state.overextended_short = (state.z_score < -entry_z_threshold);

        // No-go zone: don't fade if strong multi-session bias
        state.in_no_go_zone = (std::abs(state.multi_session_bias) > no_go_threshold);
    }

    void end_of_day(double final_vwap) {
        daily_vwaps.push_back(final_vwap);
        if (daily_vwaps.size() > 5) {
            daily_vwaps.pop_front();
        }

        // Reset intraday buffers
        intraday_pv.clear();
        intraday_vol.clear();
    }

    // Returns: -1 (fade/short), 0 (neutral), +1 (fade/long)
    int get_signal() const {
        if (state.in_no_go_zone) return 0;  // Don't trade against strong bias

        if (state.overextended_long) {
            // Price too high → fade (mean revert down)
            return -1;
        } else if (state.overextended_short) {
            // Price too low → fade (mean revert up)
            return 1;
        }

        return 0;
    }

    bool should_exit() const {
        // Exit when price returns near VWAP
        return std::abs(state.z_score) < exit_z_threshold;
    }

    double get_confidence() const {
        if (state.in_no_go_zone) return 0.0;

        // Higher confidence for larger deviations
        double excess_z = std::abs(state.z_score) - entry_z_threshold;
        return std::min(1.0, excess_z / entry_z_threshold);
    }

    const VWAPBandsState& get_state() const { return state; }
};

// Test function
void test_vwap_bands_detector() {
    std::cout << "\n=== VWAP Bands Mean-Reversion Detector Test ===\n";
    std::cout << "Hypothesis: Extreme VWAP deviations revert, but respect multi-session bias\n\n";

    // TODO: Load historical data and test
    // Metrics to track:
    // - Signal frequency at different Z thresholds
    // - Win rate on mean-reversion trades
    // - Impact of no-go zone filter
    // - Average reversion time
    // - Optimal Z-score thresholds

    std::cout << "Test metrics:\n";
    std::cout << "1. Signals/day at Z=2.0: __\n";
    std::cout << "2. Mean-reversion win rate: __%\n";
    std::cout << "3. Avg bars to reversion: __\n";
    std::cout << "4. No-go filter effectiveness: __%\n";
    std::cout << "5. Optimal entry Z: __\n";
    std::cout << "6. Optimal exit Z: __\n";
}

int main() {
    test_vwap_bands_detector();
    return 0;
}
