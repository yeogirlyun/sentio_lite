/**
 * TTM Squeeze/Expansion Detector Header
 *
 * Logic: BB width < Keltner width → squeeze state
 *        Break from squeeze + retest → continuation entry
 */

#ifndef SQUEEZE_DETECTOR_H
#define SQUEEZE_DETECTOR_H

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

    double calculate_sma(const std::vector<double>& data, int period);
    double calculate_std(const std::vector<double>& data, int period, double mean);
    double calculate_atr(const std::vector<Bar>& bars, int period);

public:
    SqueezeDetector();
    void update(const Bar& bar, const Bar* prev_bar, const std::vector<Bar>& history);
    double get_compression_score() const;
    int get_signal() const;
    double get_confidence() const;
    const SqueezeState& get_state() const { return state; }
};

#endif // SQUEEZE_DETECTOR_H
