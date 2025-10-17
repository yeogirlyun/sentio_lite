#include "predictor/multi_horizon_predictor.h"
#include "predictor/ewrls_predictor.h"
#include <cmath>
#include <algorithm>

namespace trading {

MultiHorizonPredictor::MultiHorizonPredictor(const std::string& symbol, const Config& config)
    : symbol_(symbol)
    , config_(config) {

    // Create EWRLS config for each horizon with appropriate lambda
    sentio::EWRLSPredictor::Config ewrls_config_1bar;
    ewrls_config_1bar.lambda = config_.lambda_1bar;
    ewrls_config_1bar.regularization = 1e-6;
    ewrls_config_1bar.use_adaptive_regularization = true;

    sentio::EWRLSPredictor::Config ewrls_config_5bar;
    ewrls_config_5bar.lambda = config_.lambda_5bar;
    ewrls_config_5bar.regularization = 1e-6;
    ewrls_config_5bar.use_adaptive_regularization = true;

    sentio::EWRLSPredictor::Config ewrls_config_10bar;
    ewrls_config_10bar.lambda = config_.lambda_10bar;
    ewrls_config_10bar.regularization = 1e-6;
    ewrls_config_10bar.use_adaptive_regularization = true;

    // Create predictors
    predictor_1bar_ = std::make_unique<OnlinePredictor>(
        OnlinePredictor::NUM_FEATURES, ewrls_config_1bar);
    predictor_5bar_ = std::make_unique<OnlinePredictor>(
        OnlinePredictor::NUM_FEATURES, ewrls_config_5bar);
    predictor_10bar_ = std::make_unique<OnlinePredictor>(
        OnlinePredictor::NUM_FEATURES, ewrls_config_10bar);

    // Initialize uncertainties
    prediction_errors_.fill(0.0);
    uncertainties_.fill(config_.initial_uncertainty);
}

MultiHorizonPredictor::MultiHorizonPrediction MultiHorizonPredictor::predict(
    const Eigen::VectorXd& features) {

    MultiHorizonPrediction result;

    // Get predictions from each horizon
    double pred_1 = predictor_1bar_->predict(features);
    double pred_5 = predictor_5bar_->predict(features);
    double pred_10 = predictor_10bar_->predict(features);

    // Calculate quality metrics
    result.pred_1bar = calculate_quality(pred_1, uncertainties_[0]);
    result.pred_5bar = calculate_quality(pred_5, uncertainties_[1]);
    result.pred_10bar = calculate_quality(pred_10, uncertainties_[2]);

    // Determine optimal horizon
    result.optimal_horizon = determine_optimal_horizon(result);

    // Set expected return and volatility based on optimal horizon
    if (result.optimal_horizon == 1) {
        result.expected_return = result.pred_1bar.prediction;
        result.expected_volatility = result.pred_1bar.uncertainty;
    } else if (result.optimal_horizon == 5) {
        result.expected_return = result.pred_5bar.prediction;
        result.expected_volatility = result.pred_5bar.uncertainty;
    } else {  // 10
        result.expected_return = result.pred_10bar.prediction;
        result.expected_volatility = result.pred_10bar.uncertainty;
    }

    return result;
}

void MultiHorizonPredictor::update(const Eigen::VectorXd& features,
                                   double return_1bar,
                                   double return_5bar,
                                   double return_10bar) {

    // Update 1-bar predictor (always available)
    if (std::isfinite(return_1bar)) {
        double pred_1 = predictor_1bar_->predict(features);
        double error_1 = return_1bar - pred_1;
        predictor_1bar_->update(features, return_1bar);
        update_uncertainty(0, error_1);
    }

    // Update 5-bar predictor (if 5 bars have passed)
    if (std::isfinite(return_5bar)) {
        double pred_5 = predictor_5bar_->predict(features);
        double error_5 = return_5bar - pred_5;
        predictor_5bar_->update(features, return_5bar);
        update_uncertainty(1, error_5);
    }

    // Update 10-bar predictor (if 10 bars have passed)
    if (std::isfinite(return_10bar)) {
        double pred_10 = predictor_10bar_->predict(features);
        double error_10 = return_10bar - pred_10;
        predictor_10bar_->update(features, return_10bar);
        update_uncertainty(2, error_10);
    }
}

void MultiHorizonPredictor::reset() {
    predictor_1bar_->reset();
    predictor_5bar_->reset();
    predictor_10bar_->reset();
    prediction_errors_.fill(0.0);
    uncertainties_.fill(config_.initial_uncertainty);
}

std::array<size_t, 3> MultiHorizonPredictor::update_counts() const {
    return {
        predictor_1bar_->update_count(),
        predictor_5bar_->update_count(),
        predictor_10bar_->update_count()
    };
}

MultiHorizonPredictor::PredictionQuality MultiHorizonPredictor::calculate_quality(
    double prediction, double uncertainty) const {

    PredictionQuality quality;
    quality.prediction = prediction;
    quality.uncertainty = std::max(uncertainty, 1e-6);  // Avoid division by zero

    // Z-score: standardized prediction
    quality.z_score = prediction / quality.uncertainty;

    // Signal-to-noise ratio
    quality.signal_to_noise = std::abs(prediction) / quality.uncertainty;

    // Confidence: sigmoid-like function based on signal-to-noise ratio
    // Maps [0, inf) -> [0, 1] with inflection around SNR = 2.0
    double snr_normalized = quality.signal_to_noise / 2.0;
    quality.confidence = snr_normalized / (1.0 + snr_normalized);

    return quality;
}

void MultiHorizonPredictor::update_uncertainty(int horizon_idx, double error) {
    // Exponentially weighted moving average of squared errors
    double squared_error = error * error;
    prediction_errors_[horizon_idx] = config_.uncertainty_decay * prediction_errors_[horizon_idx] +
                                      (1.0 - config_.uncertainty_decay) * squared_error;

    // Uncertainty is the square root of the EWMA of squared errors
    uncertainties_[horizon_idx] = std::sqrt(prediction_errors_[horizon_idx] +
                                           config_.initial_uncertainty * config_.initial_uncertainty);
}

int MultiHorizonPredictor::determine_optimal_horizon(const MultiHorizonPrediction& pred) const {
    // Calculate Sharpe-like ratio for each horizon
    // Return / uncertainty, adjusted for horizon length

    double sharpe_1 = pred.pred_1bar.prediction / (pred.pred_1bar.uncertainty + 1e-6);
    double sharpe_5 = pred.pred_5bar.prediction / (pred.pred_5bar.uncertainty * std::sqrt(5.0) + 1e-6);
    double sharpe_10 = pred.pred_10bar.prediction / (pred.pred_10bar.uncertainty * std::sqrt(10.0) + 1e-6);

    // Also consider signal quality
    double score_1 = sharpe_1 * pred.pred_1bar.confidence;
    double score_5 = sharpe_5 * pred.pred_5bar.confidence;
    double score_10 = sharpe_10 * pred.pred_10bar.confidence;

    // Prefer 5-bar horizon as default (good balance)
    // Only choose others if significantly better
    if (score_5 >= score_1 * 0.9 && score_5 >= score_10 * 0.9) {
        return 5;
    } else if (score_10 > score_1 && score_10 > score_5) {
        return 10;
    } else {
        return 1;
    }
}

} // namespace trading
