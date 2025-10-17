#pragma once
#include <Eigen/Dense>
#include <cstddef>
#include <cmath>

namespace sentio {

/**
 * Exponentially Weighted Recursive Least Squares (EWRLS) Predictor
 *
 * Robust online learning algorithm with numerical stability enhancements:
 * - Adaptive regularization to prevent matrix ill-conditioning
 * - Condition number monitoring and automatic correction
 * - Gradient clipping to prevent weight explosions
 * - Variance explosion prevention
 * - Periodic stability checks
 *
 * Key properties:
 * - O(n^2) update complexity where n = number of features
 * - Adapts to changing market conditions via lambda (forgetting factor)
 * - No batch training required - learns incrementally
 * - Production-ready with long-running stability
 */
class EWRLSPredictor {
public:
    /**
     * Configuration for EWRLS with stability controls
     */
    struct Config {
        double lambda = 0.99;               // Forgetting factor (0.95-0.999)
        double regularization = 1e-6;       // Ridge regularization strength
        double initial_variance = 100.0;    // Initial P diagonal value
        double max_variance = 1000.0;       // Maximum P diagonal value (prevent explosion)
        double max_gradient_norm = 1.0;     // Gradient clipping threshold
        bool use_adaptive_regularization = true;  // Enable adaptive regularization
        size_t stability_check_interval = 100;    // Check every N updates
    };

    /**
     * Constructor with lambda only (uses default config)
     */
    EWRLSPredictor(size_t n_features, double lambda = 0.99);

    /**
     * Constructor with full configuration
     */
    EWRLSPredictor(size_t n_features, const Config& config);

    /**
     * Make prediction for given feature vector
     * @param features Input feature vector (must match n_features)
     * @return Predicted return (can be positive or negative)
     */
    double predict(const Eigen::VectorXd& features) const;

    /**
     * Update model with observed outcome
     * @param features Input feature vector
     * @param actual_return Realized return (will be clamped to [-1, 1])
     *
     * Includes automatic numerical stability checks and corrections
     */
    void update(const Eigen::VectorXd& features, double actual_return);

    /**
     * Reset predictor to initial state
     */
    void reset();

    /**
     * Get current model weights (for inspection/debugging)
     */
    const Eigen::VectorXd& weights() const { return theta_; }

    /**
     * Get number of updates performed
     */
    size_t update_count() const { return updates_; }

    /**
     * Get condition number of covariance matrix
     * Values > 1e6 indicate potential numerical issues
     */
    double get_condition_number() const;

    /**
     * Check if predictor is numerically stable
     * Returns false if intervention is needed
     */
    bool is_numerically_stable() const;

    /**
     * Get configuration
     */
    const Config& config() const { return config_; }

private:
    Eigen::VectorXd theta_;         // Model weights (n_features)
    Eigen::MatrixXd P_;             // Covariance matrix (n_features x n_features)
    Config config_;                 // Configuration parameters
    size_t n_features_;             // Number of features
    size_t updates_;                // Number of updates performed

    // Stability tracking
    double min_eigenvalue_;         // Minimum eigenvalue of P
    double max_eigenvalue_;         // Maximum eigenvalue of P

    /**
     * Ensure numerical stability of covariance matrix
     * Checks condition number and applies regularization if needed
     */
    void ensure_numerical_stability();

    /**
     * Apply regularization to covariance matrix
     * Adds identity matrix scaled by regularization parameter
     */
    void apply_regularization();

    /**
     * Compute eigenvalues for stability monitoring
     */
    void update_eigenvalue_bounds();
};

} // namespace sentio
