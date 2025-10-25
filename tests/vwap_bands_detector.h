/**
 * VWAP Bands Mean-Reversion Detector Header
 */

#ifndef VWAP_BANDS_DETECTOR_H
#define VWAP_BANDS_DETECTOR_H

#include <vector>
#include <deque>
#include "../src/types.h"

struct VWAPBandsState {
    double current_vwap;
    double vwap_std;
    double z_score;
    double multi_session_vwap;
    double multi_session_bias;
    bool overextended_long;
    bool overextended_short;
    bool in_no_go_zone;
};

class VWAPBandsDetector {
private:
    double entry_z_threshold = 2.0;
    double exit_z_threshold = 0.5;
    double no_go_threshold = 1.5;

    std::deque<double> intraday_pv;
    std::deque<double> intraday_vol;
    std::deque<double> daily_vwaps;

    VWAPBandsState state;

    double calculate_vwap(const std::deque<double>& pv, const std::deque<double>& vol);
    double calculate_vwap_std(const std::deque<double>& pv, const std::deque<double>& vol, double vwap);

public:
    VWAPBandsDetector();
    void update(const Bar& bar, const Bar* prev_bar, const std::vector<Bar>& history);
    void end_of_day(double final_vwap);
    int get_signal() const;
    bool should_exit() const;
    double get_confidence() const;
    const VWAPBandsState& get_state() const { return state; }
};

#endif // VWAP_BANDS_DETECTOR_H
