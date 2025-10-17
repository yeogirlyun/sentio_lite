#include "predictor/online_predictor.h"
#include <iostream>
#include <stdexcept>

namespace trading {

OnlinePredictor::OnlinePredictor(size_t n_features, double lambda)
    : theta_(Eigen::VectorXd::Zero(n_features)),
      P_(Eigen::MatrixXd::Identity(n_features, n_features) * 100.0),
      lambda_(lambda),
      n_features_(n_features),
      updates_(0) {

    if (lambda <= 0.0 || lambda > 1.0) {
        throw std::invalid_argument("Lambda must be in (0, 1]");
    }
}

double OnlinePredictor::predict(const Eigen::VectorXd& features) const {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error(
            "Feature size mismatch: expected " + std::to_string(n_features_) +
            " but got " + std::to_string(features.size())
        );
    }
    return theta_.dot(features);
}

void OnlinePredictor::update(const Eigen::VectorXd& features, double actual_return) {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error(
            "Feature size mismatch: expected " + std::to_string(n_features_) +
            " but got " + std::to_string(features.size())
        );
    }

    // Check for NaN/Inf in inputs
    if (!std::isfinite(actual_return)) {
        return;  // Skip update with invalid data
    }
    for (int i = 0; i < features.size(); ++i) {
        if (!std::isfinite(features(i))) {
            return;  // Skip update with invalid features
        }
    }

    // EWRLS update equations
    // error = y - y_pred
    double error = actual_return - predict(features);

    if (!std::isfinite(error)) {
        return;  // Skip if error is invalid
    }

    // Calculate gain vector k = P * x / (lambda + x' * P * x)
    Eigen::VectorXd Px = P_ * features;
    double denominator = lambda_ + features.dot(Px);

    // Avoid division by zero
    if (std::abs(denominator) < 1e-10) {
        denominator = 1e-10;
    }

    Eigen::VectorXd k = Px / denominator;

    // Update weights: theta += k * error
    theta_ += k * error;

    // Check for NaN in theta after update
    for (int i = 0; i < theta_.size(); ++i) {
        if (!std::isfinite(theta_(i))) {
            // Reset to safe state if NaN detected
            reset();
            return;
        }
    }

    // Update covariance matrix: P = (P - k * x' * P) / lambda
    // Use more numerically stable formulation
    Eigen::MatrixXd P_new = (P_ - k * (features.transpose() * P_)) / lambda_;

    // Check for NaN/Inf in P_new
    bool p_valid = true;
    for (int i = 0; i < P_new.rows() && p_valid; ++i) {
        for (int j = 0; j < P_new.cols() && p_valid; ++j) {
            if (!std::isfinite(P_new(i, j))) {
                p_valid = false;
            }
        }
    }

    if (p_valid) {
        P_ = P_new;
    } else {
        // If P becomes invalid, reset to safe initial state
        P_.setIdentity();
        P_ *= 100.0;
    }

    updates_++;
}

void OnlinePredictor::reset() {
    theta_.setZero();
    P_.setIdentity();
    P_ *= 100.0;
    updates_ = 0;
}

} // namespace trading
