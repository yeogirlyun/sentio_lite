#pragma once

#include "predictor/multi_horizon_predictor.h"
#include "strategy/williams_rsi_strategy.h"
#include "core/bar.h"
#include <Eigen/Dense>
#include <string>

namespace trading {

/**
 * AWR Predictor Adapter
 *
 * Adapts WilliamsRsiStrategy (AWR) to the MultiHorizonPredictor interface
 * expected by the rotation engine. Maps AWR probability/confidence to a
 * 2-bar prediction structure used by the trading logic.
 */
class AwrPredictorAdapter {
public:
    AwrPredictorAdapter(const std::string& symbol, const WilliamsRsiConfig& config)
        : symbol_(symbol), awr_(config) {}

    MultiHorizonPredictor::MultiHorizonPrediction predict(const Eigen::VectorXd& features) {
        (void)features;  // Unused for AWR

        MultiHorizonPredictor::MultiHorizonPrediction result;
        if (!has_signal_) return result;  // Neutral until a signal exists

        // Map AWR probability [0,1] around 0.5 to prediction percentage
        double deviation = last_signal_.probability - 0.5;  // -0.5..+0.5
        double prediction_pct = deviation * 0.08;  // ±4% max

        double confidence = last_signal_.confidence;
        double uncertainty = 0.01 * (1.0 - confidence);

        result.pred_2bar.prediction = prediction_pct;
        result.pred_2bar.confidence = confidence;
        result.pred_2bar.uncertainty = uncertainty;
        result.pred_2bar.z_score = (uncertainty > 0) ? (prediction_pct / uncertainty) : 0.0;
        result.pred_2bar.signal_to_noise = (uncertainty > 0) ? (std::abs(prediction_pct) / uncertainty) : 0.0;
        result.optimal_horizon = 2;
        result.expected_return = result.pred_2bar.prediction;
        result.expected_volatility = uncertainty;
        return result;
    }

    void update_with_bar(const Bar& bar) {
        last_signal_ = awr_.generate_signal(bar, symbol_);
        has_signal_ = true;
    }

    bool is_warmed_up() const { return awr_.is_warmed_up(); }
    void reset() { awr_.reset(); has_signal_ = false; }

    const WilliamsRsiSignal& get_last_signal() const { return last_signal_; }

private:
    std::string symbol_;
    WilliamsRsiStrategy awr_;
    WilliamsRsiSignal last_signal_{};
    bool has_signal_ = false;
};

} // namespace trading


