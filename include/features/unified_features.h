#pragma once

#include "../core/bar.h"
#include <vector>
#include <deque>
#include <array>
#include <cmath>
#include <limits>
#include <Eigen/Dense>

namespace sentio {
namespace features {

/**
 * Simplified Unified Feature Engine
 *
 * Extracts 25 proven technical indicators for online learning:
 * - Momentum indicators (multiple timeframes)
 * - Volatility measures
 * - Volume analysis
 * - Price position indicators
 * - Trend strength
 *
 * Optimized for:
 * - O(1) incremental updates
 * - Minimal memory footprint
 * - Production-ready stability
 */
class UnifiedFeatures {
public:
    static constexpr size_t NUM_FEATURES = 25;
    static constexpr size_t LOOKBACK = 50;  // Maximum lookback window

    UnifiedFeatures();

    /**
     * Update with new bar. Returns true if features are valid.
     * Features may contain NaN during warmup period (~50 bars).
     */
    bool update(const trading::Bar& bar);

    /**
     * Get feature vector as Eigen::VectorXd for predictor input.
     * Returns vector with NaN values during warmup.
     */
    Eigen::VectorXd get_features() const;

    /**
     * Check if warmup is complete and all features are valid.
     */
    bool is_ready() const { return bar_count_ >= LOOKBACK; }

    /**
     * Get number of bars processed.
     */
    size_t bar_count() const { return bar_count_; }

    /**
     * Reset to initial state.
     */
    void reset();

    /**
     * Get feature names (for debugging/logging).
     */
    static std::vector<std::string> get_feature_names();

private:
    // Core calculations
    double calculate_momentum(int period) const;
    double calculate_volatility(int period) const;
    double calculate_volume_surge() const;
    double calculate_price_position(int period) const;
    double calculate_rsi(int period) const;
    double calculate_atr(int period) const;

    // State tracking
    std::deque<trading::Bar> history_;
    size_t bar_count_;

    // Cached calculations (updated incrementally)
    double prev_close_;
    std::deque<double> returns_;  // For volatility

    // Pre-allocated feature array (avoid heap allocations)
    std::array<double, NUM_FEATURES> features_;
};

} // namespace features
} // namespace sentio
