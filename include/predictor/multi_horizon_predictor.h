#pragma once
#include "predictor/online_predictor.h"
#include "predictor/feature_extractor.h"
#include <memory>
#include <array>
#include <string>
#include <Eigen/Dense>

namespace trading {

/**
 * Multi-Horizon Predictor - Predicts returns at 1, 5, and 10 bar horizons
 *
 * Maintains separate EWRLS predictors for different time horizons to capture
 * both short-term momentum and longer-term trends. This allows the trading
 * system to:
 * - Enter positions when multiple horizons align
 * - Determine optimal holding periods based on predicted return paths
 * - Exit when signal quality degrades at relevant horizon
 *
 * Key features:
 * - Three independent predictors (1, 5, 10 bars ahead)
 * - Different lambda values for each horizon (faster for short-term)
 * - Uncertainty quantification for each prediction
 * - Signal quality metrics (confidence, z-score, signal-to-noise)
 */
class MultiHorizonPredictor {
public:
    /**
     * Prediction with quality metrics
     */
    struct PredictionQuality {
        double prediction = 0.0;        // Predicted return
        double uncertainty = 0.0;       // Prediction uncertainty (std dev)
        double confidence = 0.0;        // Confidence score [0, 1]
        double z_score = 0.0;          // Prediction / uncertainty
        double signal_to_noise = 0.0;  // |prediction| / uncertainty

        bool is_high_quality(double min_confidence = 0.6,
                           double min_z_score = 1.2,
                           double min_sn_ratio = 2.0) const {
            return confidence >= min_confidence &&
                   std::abs(z_score) >= min_z_score &&
                   signal_to_noise >= min_sn_ratio;
        }
    };

    /**
     * Multi-horizon prediction result (simplified for 2-bar only)
     */
    struct MultiHorizonPrediction {
        PredictionQuality pred_2bar;    // 2-bar cumulative prediction

        int optimal_horizon = 2;        // Fixed to 2 bars
        double expected_return = 0.0;   // Expected return at 2-bar horizon
        double expected_volatility = 0.0;  // Expected volatility over hold period

        /**
         * Check if signal agrees (simplified - always true for single horizon)
         */
        bool horizons_agree() const {
            return true;  // Only one horizon, always agrees with itself
        }

        /**
         * Check if signal is strong enough to enter
         * Simplified for 2-bar only horizon
         */
        bool should_enter(double min_prediction = 0.002,
                         double min_confidence = 0.6) const {
            // Require 2-bar prediction to exceed minimum threshold
            if (std::abs(pred_2bar.prediction) < min_prediction) {
                return false;
            }

            // Require good confidence at 2-bar horizon
            if (pred_2bar.confidence < min_confidence) {
                return false;
            }

            return true;
        }

        /**
         * Suggest optimal holding period (fixed at 2 bars)
         */
        int suggested_hold_period() const {
            return 2;  // Fixed 2-bar hold
        }
    };

    /**
     * Configuration for multi-horizon predictor (simplified for 2-bar)
     */
    struct Config {
        // EWRLS parameters for 2-bar horizon
        double lambda_2bar;      // Adaptation rate for 2-bar

        // Uncertainty estimation (for confidence calculations)
        double initial_uncertainty;  // 1% initial uncertainty
        double uncertainty_decay;    // Decay factor for uncertainty

        // Quality thresholds
        double min_confidence;
        double min_z_score;
        double min_signal_to_noise;

        Config()
            : lambda_2bar(0.98)
            , initial_uncertainty(0.01)
            , uncertainty_decay(0.95)
            , min_confidence(0.6)
            , min_z_score(1.2)
            , min_signal_to_noise(2.0) {}
    };

    /**
     * Constructor
     * @param symbol Symbol identifier (for debugging)
     * @param config Configuration parameters
     */
    explicit MultiHorizonPredictor(const std::string& symbol, const Config& config = Config());

    /**
     * Make predictions at all horizons
     * @param features Input feature vector (33 dimensions)
     * @return Multi-horizon prediction with quality metrics
     */
    MultiHorizonPrediction predict(const Eigen::VectorXd& features);

    /**
     * Update predictor with realized 2-bar return
     * @param features Feature vector used for prediction
     * @param return_2bar Actual 2-bar cumulative return
     */
    void update(const Eigen::VectorXd& features,
                double return_2bar);

    /**
     * Reset all predictors
     */
    void reset();

    /**
     * Get configuration
     */
    const Config& config() const { return config_; }

    /**
     * Get symbol identifier
     */
    const std::string& symbol() const { return symbol_; }

    /**
     * Get update count for 2-bar horizon
     */
    size_t update_count() const;

private:
    std::string symbol_;
    Config config_;

    // Single predictor for 2-bar horizon
    std::unique_ptr<OnlinePredictor> predictor_2bar_;

    // Uncertainty tracking (simple exponentially weighted variance)
    double prediction_error_;  // Running prediction error
    double uncertainty_;       // Estimated uncertainty

    /**
     * Calculate prediction quality metrics
     */
    PredictionQuality calculate_quality(double prediction, double uncertainty) const;

    /**
     * Update uncertainty estimate based on prediction error
     */
    void update_uncertainty(double error);
};

} // namespace trading
