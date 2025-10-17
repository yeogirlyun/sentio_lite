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
     * Multi-horizon prediction result
     */
    struct MultiHorizonPrediction {
        PredictionQuality pred_1bar;    // Next bar prediction
        PredictionQuality pred_5bar;    // 5-bar cumulative prediction
        PredictionQuality pred_10bar;   // 10-bar cumulative prediction

        int optimal_horizon = 1;        // Best risk/reward horizon (1, 5, or 10)
        double expected_return = 0.0;   // Expected return at optimal horizon
        double expected_volatility = 0.0;  // Expected volatility over hold period

        /**
         * Check if multiple horizons agree on direction
         * Returns true if at least 2 out of 3 horizons agree
         */
        bool horizons_agree() const {
            int positive_count = 0;
            int negative_count = 0;

            if (pred_1bar.prediction > 0) positive_count++; else negative_count++;
            if (pred_5bar.prediction > 0) positive_count++; else negative_count++;
            if (pred_10bar.prediction > 0) positive_count++; else negative_count++;

            // At least 2 out of 3 must agree
            return positive_count >= 2 || negative_count >= 2;
        }

        /**
         * Check if signal is strong enough to enter
         * RELAXED CRITERIA: Only requires strong 5-bar signal and directional agreement
         */
        bool should_enter(double min_prediction = 0.002,
                         double min_confidence = 0.6) const {
            // Require 5-bar prediction to exceed minimum threshold
            if (std::abs(pred_5bar.prediction) < min_prediction) {
                return false;
            }

            // Require good confidence at 5-bar horizon
            if (pred_5bar.confidence < min_confidence) {
                return false;
            }

            // Only require directional agreement between 1-bar and 5-bar
            // (removed requirement for all 3 horizons to agree)
            if ((pred_1bar.prediction > 0) != (pred_5bar.prediction > 0)) {
                return false;
            }

            // Removed 3x ratio requirement - was too restrictive

            return true;
        }

        /**
         * Suggest optimal holding period based on predictions
         */
        int suggested_hold_period() const {
            // Use optimal_horizon as base, but add buffer
            if (optimal_horizon == 10) {
                return 10;
            } else if (optimal_horizon == 5) {
                return 5;
            } else {
                return 3;  // Minimum meaningful hold
            }
        }
    };

    /**
     * Configuration for multi-horizon predictor
     */
    struct Config {
        // EWRLS parameters per horizon
        double lambda_1bar;      // Fast adaptation for 1-bar
        double lambda_5bar;     // Medium adaptation for 5-bar
        double lambda_10bar;    // Slow adaptation for 10-bar

        // Uncertainty estimation (for confidence calculations)
        double initial_uncertainty;  // 1% initial uncertainty
        double uncertainty_decay;    // Decay factor for uncertainty

        // Quality thresholds
        double min_confidence;
        double min_z_score;
        double min_signal_to_noise;

        Config()
            : lambda_1bar(0.99)
            , lambda_5bar(0.995)
            , lambda_10bar(0.998)
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
     * Update predictors with realized returns
     * @param features Feature vector used for prediction
     * @param return_1bar Actual 1-bar return
     * @param return_5bar Actual 5-bar cumulative return (if available)
     * @param return_10bar Actual 10-bar cumulative return (if available)
     *
     * Note: For 5-bar and 10-bar updates, pass NaN if not yet available
     */
    void update(const Eigen::VectorXd& features,
                double return_1bar,
                double return_5bar = std::numeric_limits<double>::quiet_NaN(),
                double return_10bar = std::numeric_limits<double>::quiet_NaN());

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
     * Get update counts for each horizon
     */
    std::array<size_t, 3> update_counts() const;

private:
    std::string symbol_;
    Config config_;

    // Separate predictors for each horizon
    std::unique_ptr<OnlinePredictor> predictor_1bar_;
    std::unique_ptr<OnlinePredictor> predictor_5bar_;
    std::unique_ptr<OnlinePredictor> predictor_10bar_;

    // Uncertainty tracking (simple exponentially weighted variance)
    std::array<double, 3> prediction_errors_;  // Running prediction errors
    std::array<double, 3> uncertainties_;      // Estimated uncertainties

    /**
     * Calculate prediction quality metrics
     */
    PredictionQuality calculate_quality(double prediction, double uncertainty) const;

    /**
     * Update uncertainty estimate based on prediction error
     */
    void update_uncertainty(int horizon_idx, double error);

    /**
     * Determine optimal horizon based on Sharpe-like metric
     */
    int determine_optimal_horizon(const MultiHorizonPrediction& pred) const;
};

} // namespace trading
