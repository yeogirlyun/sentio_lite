#include "predictor/ewrls_predictor.h"
#include <Eigen/Eigenvalues>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace sentio {

EWRLSPredictor::EWRLSPredictor(size_t n_features, double lambda)
    : EWRLSPredictor(n_features, Config{lambda}) {}

EWRLSPredictor::EWRLSPredictor(size_t n_features, const Config& config)
    : theta_(Eigen::VectorXd::Zero(n_features))
    , P_(Eigen::MatrixXd::Identity(n_features, n_features) * config.initial_variance)
    , config_(config)
    , n_features_(n_features)
    , updates_(0)
    , min_eigenvalue_(config.initial_variance)
    , max_eigenvalue_(config.initial_variance) {

    if (config_.lambda <= 0.0 || config_.lambda > 1.0) {
        throw std::invalid_argument("Lambda must be in (0, 1], got " + std::to_string(config_.lambda));
    }

    if (n_features == 0) {
        throw std::invalid_argument("Number of features must be > 0");
    }
}

double EWRLSPredictor::predict(const Eigen::VectorXd& features) const {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error(
            "Feature size mismatch: expected " + std::to_string(n_features_) +
            " but got " + std::to_string(features.size())
        );
    }
    return theta_.dot(features);
}

void EWRLSPredictor::update(const Eigen::VectorXd& features, double actual_return) {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error(
            "Feature size mismatch: expected " + std::to_string(n_features_) +
            " but got " + std::to_string(features.size())
        );
    }

    // Input validation - skip invalid inputs
    for (Eigen::Index i = 0; i < features.size(); ++i) {
        if (std::isnan(features(i)) || std::isinf(features(i))) {
            return;  // Skip this update
        }
    }

    if (std::isnan(actual_return) || std::isinf(actual_return)) {
        return;  // Skip invalid returns
    }

    // Clamp extreme returns to prevent numerical issues
    // Most single-bar returns should be < 100%
    actual_return = std::max(-1.0, std::min(1.0, actual_return));

    // EWRLS update with numerical stability
    double error = actual_return - predict(features);

    // Calculate gain vector with regularization
    Eigen::VectorXd Px = P_ * features;
    double denominator = config_.lambda + features.dot(Px);

    // Add small regularization to prevent division issues
    denominator += config_.regularization;

    if (denominator < 1e-10) {
        // Severe numerical issue - apply stronger regularization
        apply_regularization();
        return;
    }

    // Compute Kalman gain
    Eigen::VectorXd k = Px / denominator;

    // Update weights with gradient clipping
    Eigen::VectorXd weight_update = k * error;
    double update_norm = weight_update.norm();
    if (update_norm > config_.max_gradient_norm) {
        // Gradient clipping to prevent weight explosion
        weight_update = weight_update * (config_.max_gradient_norm / update_norm);
    }
    theta_ += weight_update;

    // Validate theta after update
    for (Eigen::Index i = 0; i < theta_.size(); ++i) {
        if (std::isnan(theta_(i)) || std::isinf(theta_(i))) {
            // Weight corruption detected - reset
            std::cerr << "Warning: Weight corruption detected, resetting EWRLS predictor" << std::endl;
            reset();
            return;
        }
    }

    // Update covariance matrix using Joseph form for better numerical stability
    // P_new = (I - k*x') * P * (I - k*x')' / lambda + k*k' * R
    // Simplified form: P = (P - k * x' * P) / lambda
    Eigen::MatrixXd P_new = (P_ - k * features.transpose() * P_) / config_.lambda;

    // Ensure symmetry (critical for numerical stability)
    P_ = (P_new + P_new.transpose()) / 2.0;

    // Validate P after update
    bool p_valid = true;
    for (Eigen::Index i = 0; i < P_.rows() && p_valid; ++i) {
        for (Eigen::Index j = 0; j < P_.cols() && p_valid; ++j) {
            if (std::isnan(P_(i, j)) || std::isinf(P_(i, j))) {
                p_valid = false;
            }
        }
    }

    if (!p_valid) {
        // Covariance matrix corruption - reset
        std::cerr << "Warning: Covariance matrix corruption detected, resetting" << std::endl;
        reset();
        return;
    }

    // Periodic stability check
    updates_++;
    if (updates_ % config_.stability_check_interval == 0) {
        ensure_numerical_stability();
    }
}

void EWRLSPredictor::ensure_numerical_stability() {
    // Update eigenvalue bounds for condition number
    update_eigenvalue_bounds();

    // Check condition number
    double condition_number = get_condition_number();

    if (condition_number > 1e6 || min_eigenvalue_ < config_.regularization) {
        // Matrix is ill-conditioned, apply regularization
        apply_regularization();

        // Log warning (but limit spam)
        static size_t warning_count = 0;
        if (warning_count < 10) {
            std::cerr << "Warning [" << warning_count << "]: EWRLS covariance matrix ill-conditioned. "
                     << "Condition number: " << condition_number
                     << ", min eigenvalue: " << min_eigenvalue_
                     << ". Applying regularization." << std::endl;
            warning_count++;
        }
    }

    // Prevent variance explosion
    double max_diagonal = P_.diagonal().maxCoeff();
    if (max_diagonal > config_.max_variance) {
        // Scale down the entire matrix to keep maximum variance bounded
        double scale = config_.max_variance / max_diagonal;
        P_ *= scale;

        static size_t scale_warning_count = 0;
        if (scale_warning_count < 5) {
            std::cerr << "Warning: Variance explosion detected (max=" << max_diagonal
                     << "), scaling P by " << scale << std::endl;
            scale_warning_count++;
        }
    }
}

void EWRLSPredictor::apply_regularization() {
    if (config_.use_adaptive_regularization) {
        // Adaptive regularization based on condition
        double reg_strength = config_.regularization;

        if (min_eigenvalue_ < 1e-6) {
            // Matrix is severely ill-conditioned, use stronger regularization
            reg_strength = 0.01;
        } else if (min_eigenvalue_ < 1e-4) {
            // Moderate ill-conditioning
            reg_strength = 0.001;
        }

        // Add regularization to diagonal (Ridge regression)
        P_ += Eigen::MatrixXd::Identity(n_features_, n_features_) * reg_strength;
    } else {
        // Fixed regularization
        P_ += Eigen::MatrixXd::Identity(n_features_, n_features_) * config_.regularization;
    }

    // Update eigenvalue bounds after regularization
    update_eigenvalue_bounds();
}

void EWRLSPredictor::update_eigenvalue_bounds() {
    // Compute eigenvalues to check condition
    // Note: This is O(n^3) but only called periodically
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(P_);

    if (solver.info() == Eigen::Success) {
        auto eigenvalues = solver.eigenvalues();
        min_eigenvalue_ = eigenvalues.minCoeff();
        max_eigenvalue_ = eigenvalues.maxCoeff();
    } else {
        // Eigenvalue computation failed - this is a serious issue
        std::cerr << "Error: Failed to compute eigenvalues, resetting predictor" << std::endl;
        reset();
    }
}

double EWRLSPredictor::get_condition_number() const {
    return max_eigenvalue_ / (min_eigenvalue_ + 1e-10);
}

bool EWRLSPredictor::is_numerically_stable() const {
    double cond = get_condition_number();
    return cond < 1e6 && min_eigenvalue_ > 1e-8;
}

void EWRLSPredictor::reset() {
    theta_.setZero();
    P_ = Eigen::MatrixXd::Identity(n_features_, n_features_) * config_.initial_variance;
    updates_ = 0;
    min_eigenvalue_ = config_.initial_variance;
    max_eigenvalue_ = config_.initial_variance;
}

} // namespace sentio
