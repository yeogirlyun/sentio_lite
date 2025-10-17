#pragma once
#include "predictor/ewrls_predictor.h"
#include "predictor/feature_extractor.h"
#include <memory>
#include <stdexcept>

namespace trading {

/**
 * OnlinePredictor - Wrapper for EWRLS with Feature Dimension Enforcement
 *
 * This class wraps sentio::EWRLSPredictor and enforces the use of exactly
 * 33 features (as produced by FeatureExtractor). It provides a clean interface
 * for the trading system while delegating all learning logic to the robust
 * EWRLS implementation.
 *
 * Key features:
 * - Enforces 33-feature constraint at compile time
 * - Delegates to robust EWRLS implementation with stability enhancements
 * - Provides simple predict/update interface for trading system
 * - Automatic feature dimension validation
 *
 * Usage:
 *   OnlinePredictor predictor(lambda);  // Uses 33 features automatically
 *   double pred = predictor.predict(features);
 *   predictor.update(features, actual_return);
 */
class OnlinePredictor {
public:
    // Feature dimension must match FeatureExtractor
    static constexpr size_t NUM_FEATURES = FeatureExtractor::NUM_FEATURES;  // 33

    /**
     * Constructor with default lambda
     * @param n_features Number of features (must be NUM_FEATURES=33)
     * @param lambda Forgetting factor (default 0.98)
     *               - Higher (closer to 1.0) = more memory, slower adaptation
     *               - Lower = faster adaptation, less stable
     *               - Typical range: 0.95-0.995
     *
     * @throws std::runtime_error if n_features != NUM_FEATURES
     */
    explicit OnlinePredictor(size_t n_features = NUM_FEATURES, double lambda = 0.98)
        : ewrls_(std::make_unique<sentio::EWRLSPredictor>(n_features, lambda)) {

        if (n_features != NUM_FEATURES) {
            throw std::runtime_error(
                "OnlinePredictor expects exactly " + std::to_string(NUM_FEATURES) +
                " features (from FeatureExtractor), but got " + std::to_string(n_features) +
                ". This indicates a feature dimension mismatch in your system."
            );
        }
    }

    /**
     * Constructor with full EWRLS configuration
     * @param n_features Number of features (must be NUM_FEATURES=33)
     * @param config EWRLS configuration for fine-tuned control
     */
    OnlinePredictor(size_t n_features, const sentio::EWRLSPredictor::Config& config)
        : ewrls_(std::make_unique<sentio::EWRLSPredictor>(n_features, config)) {

        if (n_features != NUM_FEATURES) {
            throw std::runtime_error(
                "OnlinePredictor expects exactly " + std::to_string(NUM_FEATURES) +
                " features, but got " + std::to_string(n_features)
            );
        }
    }

    /**
     * Make prediction for given feature vector
     * @param features Input feature vector (must be 33-dimensional)
     * @return Predicted return (can be positive or negative)
     */
    double predict(const Eigen::VectorXd& features) const {
        return ewrls_->predict(features);
    }

    /**
     * Update model with observed outcome
     * @param features Input feature vector used for prediction
     * @param actual_return Realized return (target variable)
     *
     * Includes automatic numerical stability checks and corrections
     */
    void update(const Eigen::VectorXd& features, double actual_return) {
        ewrls_->update(features, actual_return);
    }

    /**
     * Reset predictor to initial state
     */
    void reset() {
        ewrls_->reset();
    }

    /**
     * Get current model weights (for inspection/debugging)
     */
    const Eigen::VectorXd& weights() const {
        return ewrls_->weights();
    }

    /**
     * Get number of updates performed
     */
    size_t update_count() const {
        return ewrls_->update_count();
    }

    /**
     * Get condition number of covariance matrix
     * Values > 1e6 indicate potential numerical issues
     */
    double get_condition_number() const {
        return ewrls_->get_condition_number();
    }

    /**
     * Check if predictor is numerically stable
     */
    bool is_numerically_stable() const {
        return ewrls_->is_numerically_stable();
    }

    /**
     * Access underlying EWRLS predictor (for advanced use)
     */
    const sentio::EWRLSPredictor& ewrls() const {
        return *ewrls_;
    }

private:
    std::unique_ptr<sentio::EWRLSPredictor> ewrls_;
};

} // namespace trading
