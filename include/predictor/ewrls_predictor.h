#pragma once
#include <Eigen/Dense>
#include <vector>

namespace sentio {

/**
 * Exponentially Weighted Recursive Least Squares (EWRLS) Predictor
 *
 * Online learning algorithm with forgetting factor for non-stationary time series.
 * O(n^2) update complexity where n = number of features.
 *
 * Key properties:
 * - Adapts to changing market conditions via lambda (forgetting factor)
 * - No batch training required - learns incrementally
 * - Provably convergent under standard assumptions
 */
class EWRLSPredictor {
private:
    Eigen::VectorXd theta_;      // Model weights
    Eigen::MatrixXd P_;          // Covariance matrix
    double lambda_;              // Forgetting factor (0.95-0.99)
    size_t n_features_;
    size_t updates_;             // Track number of updates

public:
    /**
     * Constructor
     * @param n_features Number of input features
     * @param lambda Forgetting factor (default 0.98)
     *               - Higher (closer to 1.0) = more memory, slower adaptation
     *               - Lower = faster adaptation, less stable
     */
    EWRLSPredictor(size_t n_features = 25, double lambda = 0.98);

    /**
     * Make prediction for given feature vector.
     * Returns predicted return (can be negative).
     */
    double predict(const Eigen::VectorXd& features) const;

    /**
     * Update model with observed outcome.
     * @param features Input feature vector
     * @param actual_return Realized return (target variable)
     */
    void update(const Eigen::VectorXd& features, double actual_return);

    /**
     * Get current model weights (for inspection/debugging).
     */
    const Eigen::VectorXd& weights() const { return theta_; }

    /**
     * Get number of updates performed.
     */
    size_t update_count() const { return updates_; }

    /**
     * Reset predictor to initial state.
     */
    void reset();
};

} // namespace sentio
