#include "predictor/ewrls_predictor.h"
#include <stdexcept>

namespace sentio {

EWRLSPredictor::EWRLSPredictor(size_t n_features, double lambda)
    : theta_(Eigen::VectorXd::Zero(n_features))
    , P_(Eigen::MatrixXd::Identity(n_features, n_features) * 100.0)
    , lambda_(lambda)
    , n_features_(n_features)
    , updates_(0) {

    if (lambda <= 0.0 || lambda > 1.0) {
        throw std::invalid_argument("Lambda must be in (0, 1]");
    }
}

double EWRLSPredictor::predict(const Eigen::VectorXd& features) const {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error("Feature size mismatch");
    }
    return theta_.dot(features);
}

void EWRLSPredictor::update(const Eigen::VectorXd& features, double actual_return) {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error("Feature size mismatch");
    }

    // Skip if features contain NaN (during warmup)
    for (Eigen::Index i = 0; i < features.size(); ++i) {
        if (std::isnan(features(i)) || std::isinf(features(i))) {
            return;  // Skip this update
        }
    }

    // EWRLS update equations
    // 1. Compute prediction error
    double error = actual_return - predict(features);

    // 2. Calculate gain vector k = P * x / (lambda + x' * P * x)
    Eigen::VectorXd Px = P_ * features;
    double denominator = lambda_ + features.dot(Px);

    if (denominator < 1e-10) {
        // Numerical stability: reset if denominator too small
        P_ = Eigen::MatrixXd::Identity(n_features_, n_features_) * 100.0;
        return;
    }

    Eigen::VectorXd k = Px / denominator;

    // 3. Update weights: theta = theta + k * error
    theta_ += k * error;

    // 4. Update covariance matrix: P = (P - k * x' * P) / lambda
    P_ = (P_ - k * features.transpose() * P_) / lambda_;

    // Ensure P remains symmetric (numerical stability)
    P_ = (P_ + P_.transpose()) / 2.0;

    updates_++;
}

void EWRLSPredictor::reset() {
    theta_.setZero();
    P_ = Eigen::MatrixXd::Identity(n_features_, n_features_) * 100.0;
    updates_ = 0;
}

} // namespace sentio
