/**
 * RSI(2) Pullback Detector Header
 */

#ifndef RSI2_DETECTOR_H
#define RSI2_DETECTOR_H

#include <vector>
#include <deque>
#include "../src/types.h"

struct RSI2State {
    double rsi_value;
    bool oversold;
    bool overbought;
    double distance_from_vwap;
    double volume_ratio;
    bool signal_valid;
};

class RSI2Detector {
private:
    int rsi_period = 2;
    double oversold_threshold = 10.0;
    double overbought_threshold = 90.0;
    double max_vwap_distance = 2.0;
    double min_volume_ratio = 0.8;

    std::deque<double> gains;
    std::deque<double> losses;
    std::deque<double> volumes;
    std::deque<double> price_x_volume;

    RSI2State state;

    double calculate_rsi();
    double calculate_vwap();
    double calculate_vwap_std(double vwap);

public:
    RSI2Detector();
    void update(const Bar& bar, const Bar* prev_bar, const std::vector<Bar>& history);
    int get_signal() const;
    double get_confidence() const;
    const RSI2State& get_state() const { return state; }
};

#endif // RSI2_DETECTOR_H
