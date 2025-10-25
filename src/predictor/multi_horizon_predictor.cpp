#include "predictor/multi_horizon_predictor.h"
#include "predictor/ewrls_predictor.h"
#include <cmath>
#include <algorithm>

namespace trading {

MultiHorizonPredictor::MultiHorizonPredictor(const std::string& symbol, const Config& config)
    : symbol_(symbol)
    , config_(config)
    , prediction_error_(0.0)
    , uncertainty_(config_.initial_uncertainty) {

    // Create predictor with simplified online model config derived from lambda
    sentio::EWRLSPredictor::Config ewrls_config_2bar;
    ewrls_config_2bar.lambda = config_.lambda_2bar;
    predictor_2bar_ = std::make_unique<OnlinePredictor>(OnlinePredictor::NUM_FEATURES, ewrls_config_2bar);
}

MultiHorizonPredictor::MultiHorizonPrediction MultiHorizonPredictor::predict(
    const Eigen::VectorXd& features) {

    MultiHorizonPrediction result;

    // Get prediction from 2-bar horizon
    double pred_2 = predictor_2bar_->predict(features);

    // Calculate quality metrics
    result.pred_2bar = calculate_quality(pred_2, uncertainty_);

    // Set expected return and volatility (fixed at 2-bar horizon)
    result.expected_return = result.pred_2bar.prediction;
    result.expected_volatility = result.pred_2bar.uncertainty;
    result.optimal_horizon = 2;

    return result;
}

void MultiHorizonPredictor::update(const Eigen::VectorXd& features,
                                   double return_2bar) {

    // Update 2-bar predictor
    if (std::isfinite(return_2bar)) {
        double pred_2 = predictor_2bar_->predict(features);
        double error_2 = return_2bar - pred_2;
        predictor_2bar_->update(features, return_2bar);
        update_uncertainty(error_2);
    }
}

void MultiHorizonPredictor::reset() {
    predictor_2bar_->reset();
    prediction_error_ = 0.0;
    uncertainty_ = config_.initial_uncertainty;
}

size_t MultiHorizonPredictor::update_count() const {
    return predictor_2bar_->update_count();
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

void MultiHorizonPredictor::update_uncertainty(double error) {
    // Exponentially weighted moving average of squared errors
    double squared_error = error * error;
    prediction_error_ = config_.uncertainty_decay * prediction_error_ +
                       (1.0 - config_.uncertainty_decay) * squared_error;

    // Uncertainty is the square root of the EWMA of squared errors
    uncertainty_ = std::sqrt(prediction_error_ +
                            config_.initial_uncertainty * config_.initial_uncertainty);
}

} // namespace trading
