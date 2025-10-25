/**
 * VWAP Bands Mean-Reversion Detector Implementation
 */

#include "vwap_bands_detector.h"
#include <cmath>
#include <algorithm>

double VWAPBandsDetector::calculate_vwap(const std::deque<double>& pv, const std::deque<double>& vol) {
    if (pv.empty() || vol.empty()) return 0.0;

    double total_pv = 0.0;
    double total_vol = 0.0;

    for (size_t i = 0; i < pv.size(); i++) {
        total_pv += pv[i];
        total_vol += vol[i];
    }

    return total_vol > 0 ? total_pv / total_vol : 0.0;
}

double VWAPBandsDetector::calculate_vwap_std(const std::deque<double>& pv, const std::deque<double>& vol, double vwap) {
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

VWAPBandsDetector::VWAPBandsDetector() {
    state.current_vwap = 0.0;
    state.vwap_std = 0.0;
    state.z_score = 0.0;
    state.multi_session_vwap = 0.0;
    state.multi_session_bias = 0.0;
    state.overextended_long = false;
    state.overextended_short = false;
    state.in_no_go_zone = false;
}

void VWAPBandsDetector::update(const Bar& bar, const Bar* prev_bar, const std::vector<Bar>& history) {
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

void VWAPBandsDetector::end_of_day(double final_vwap) {
    daily_vwaps.push_back(final_vwap);
    if (daily_vwaps.size() > 5) {
        daily_vwaps.pop_front();
    }

    // Reset intraday buffers
    intraday_pv.clear();
    intraday_vol.clear();
}

int VWAPBandsDetector::get_signal() const {
    if (state.in_no_go_zone) return 0;

    if (state.overextended_long) {
        return -1;  // Fade high
    } else if (state.overextended_short) {
        return 1;  // Fade low
    }

    return 0;
}

bool VWAPBandsDetector::should_exit() const {
    return std::abs(state.z_score) < exit_z_threshold;
}

double VWAPBandsDetector::get_confidence() const {
    if (state.in_no_go_zone) return 0.0;

    // Higher confidence for larger deviations
    double excess_z = std::abs(state.z_score) - entry_z_threshold;
    return std::min(1.0, excess_z / entry_z_threshold);
}
