#pragma once
#include "predictor/feature_extractor.h"
#include <memory>
#include <Eigen/Dense>

namespace sentio {
namespace features {

/**
 * UnifiedFeatures - Backward Compatibility Wrapper
 *
 * This class wraps trading::FeatureExtractor to provide backward compatibility
 * with existing code while ensuring everyone uses the same 33-feature system.
 *
 * IMPORTANT: The feature dimension is now 33 (not 25).
 * All code should migrate to use trading::FeatureExtractor directly.
 *
 * Features (33 total):
 * - 8 Time features (cyclical encoding)
 * - 25 Technical features (momentum, volatility, volume, price position, etc.)
 */
class UnifiedFeatures {
public:
    // Feature dimension updated to match FeatureExtractor
    static constexpr size_t NUM_FEATURES = trading::FeatureExtractor::NUM_FEATURES;  // 33
    static constexpr size_t LOOKBACK = 50;

    UnifiedFeatures()
        : extractor_(std::make_unique<trading::FeatureExtractor>()) {}

    /**
     * Update with new bar
     * @param bar New OHLCV bar
     * @return True if features are ready (after warmup)
     */
    bool update(const trading::Bar& bar) {
        auto features = extractor_->extract(bar);
        if (features.has_value()) {
            cached_features_ = features.value();
            return true;
        }
        return false;
    }

    /**
     * Get feature vector
     * Returns std::nullopt-equivalent empty vector during warmup
     */
    Eigen::VectorXd get_features() const {
        return cached_features_;
    }

    /**
     * Check if warmup is complete
     */
    bool is_ready() const {
        return extractor_->is_ready();
    }

    /**
     * Get number of bars processed
     */
    size_t bar_count() const {
        return extractor_->bar_count();
    }

    /**
     * Reset to initial state
     */
    void reset() {
        extractor_->reset();
        cached_features_ = Eigen::VectorXd::Zero(NUM_FEATURES);
    }

    /**
     * Get feature names (for debugging/logging)
     */
    static std::vector<std::string> get_feature_names() {
        return trading::FeatureExtractor::get_feature_names();
    }

    /**
     * Access underlying FeatureExtractor (for advanced use)
     */
    const trading::FeatureExtractor& extractor() const {
        return *extractor_;
    }

private:
    std::unique_ptr<trading::FeatureExtractor> extractor_;
    Eigen::VectorXd cached_features_;
};

} // namespace features
} // namespace sentio
