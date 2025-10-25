#pragma once
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

    // Removed legacy EWRLS implementation; kept only data structures for adapters
};

} // namespace trading
