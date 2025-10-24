#pragma once

#include "predictor/multi_horizon_predictor.h"
#include "strategy/sigor_strategy.h"
#include "core/bar.h"
#include <Eigen/Dense>
#include <string>
#include <memory>

namespace trading {

/**
 * SIGOR Predictor Adapter
 *
 * Adapts SigorStrategy to work with MultiSymbolTrader by implementing
 * the same interface as MultiHorizonPredictor.
 *
 * Maps SIGOR probability signals (0..1, centered at 0.5) to prediction format:
 * - probability > 0.5 → positive prediction (bullish)
 * - probability < 0.5 → negative prediction (bearish)
 * - confidence → used as signal quality metric
 */
class SigorPredictorAdapter {
public:
    /**
     * Constructor
     * @param symbol Symbol this predictor is for
     * @param config SIGOR configuration
     */
    SigorPredictorAdapter(const std::string& symbol, const SigorConfig& config)
        : symbol_(symbol), sigor_(config) {}

    /**
     * Generate multi-horizon prediction from features
     *
     * For SIGOR:
     * - Features are ignored (SIGOR uses bar data directly)
     * - Current bar must be provided via update()
     * - Returns cached signal from last update()
     *
     * @param features Ignored (SIGOR doesn't use EWRLS features)
     * @return Multi-horizon prediction structure
     */
    MultiHorizonPredictor::MultiHorizonPrediction predict(const Eigen::VectorXd& features) {
        (void)features;  // Unused - SIGOR uses bar data directly

        MultiHorizonPredictor::MultiHorizonPrediction result;

        if (!has_signal_) {
            // No signal yet - return neutral prediction
            return result;
        }

        // Convert SIGOR probability (0..1, center=0.5) to prediction (percentage return)
        // Mapping:
        //   probability = 0.5 → prediction = 0.0 (neutral)
        //   probability = 0.6 → prediction = +0.01 (1% bullish)
        //   probability = 0.4 → prediction = -0.01 (1% bearish)
        //   probability = 0.7 → prediction = +0.02 (2% bullish)
        //   probability = 0.3 → prediction = -0.02 (2% bearish)

        double deviation = last_signal_.probability - 0.5;  // -0.5 to +0.5
        double prediction_pct = deviation * 0.10;  // Scale to ±5% max (0.5 * 0.10 = 0.05)

        // Use confidence as quality metric
        double confidence = last_signal_.confidence;
        double uncertainty = 0.01 * (1.0 - confidence);  // Lower confidence = higher uncertainty

        // Populate all horizons with same signal (SIGOR doesn't predict multi-horizon)
        // The rotation system will use the prediction values directly
        result.pred_1bar.prediction = prediction_pct;
        result.pred_1bar.confidence = confidence;
        result.pred_1bar.uncertainty = uncertainty;
        result.pred_1bar.z_score = (uncertainty > 0) ? (prediction_pct / uncertainty) : 0.0;
        result.pred_1bar.signal_to_noise = (uncertainty > 0) ? (std::abs(prediction_pct) / uncertainty) : 0.0;

        // For simplicity, use same prediction for all horizons without scaling
        // (Avoid saturation in probability scaling for rotation)
        result.pred_5bar = result.pred_1bar;
        result.pred_10bar = result.pred_1bar;
        result.pred_20bar = result.pred_1bar;

        // Optimal horizon is always 1-bar for SIGOR (it's a tactical system)
        result.optimal_horizon = 1;
        result.expected_return = result.pred_1bar.prediction;
        result.expected_volatility = uncertainty;

        return result;
    }

    /**
     * Update with new bar data and generate SIGOR signal
     *
     * Unlike EWRLS which updates with targets, SIGOR generates signals
     * directly from bar data using technical indicators.
     *
     * @param features Ignored
     * @param target_1bar Ignored
     * @param target_5bar Ignored
     * @param target_10bar Ignored
     */
    void update(const Eigen::VectorXd& features, double target_1bar,
                double target_5bar, double target_10bar) {
        (void)features;
        (void)target_1bar;
        (void)target_5bar;
        (void)target_10bar;
        // SIGOR doesn't learn from targets - it's rule-based
        // Signal generation happens in update_with_bar()
    }

    /**
     * Update SIGOR with bar data and generate signal
     * This must be called before predict() to have current signal
     *
     * @param bar Current bar data
     */
    void update_with_bar(const Bar& bar) {
        last_signal_ = sigor_.generate_signal(bar, symbol_);
        has_signal_ = true;
    }

    /**
     * Check if warmup period is complete
     */
    bool is_warmed_up() const {
        return sigor_.is_warmed_up();
    }

    /**
     * Reset predictor state
     */
    void reset() {
        sigor_.reset();
        has_signal_ = false;
    }

    /**
     * Get last SIGOR signal (for debugging/monitoring)
     */
    const SigorSignal& get_last_signal() const {
        return last_signal_;
    }

private:
    std::string symbol_;
    SigorStrategy sigor_;
    SigorSignal last_signal_;
    bool has_signal_ = false;
};

} // namespace trading
