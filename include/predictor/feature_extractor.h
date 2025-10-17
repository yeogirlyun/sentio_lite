#pragma once
#include "core/bar.h"
#include "utils/circular_buffer.h"
#include <Eigen/Dense>
#include <optional>
#include <array>

namespace trading {

/**
 * Enhanced Feature Extractor - 33 Technical + Time Indicators
 *
 * Extracts comprehensive set of proven technical indicators for online learning:
 * - 8 Time features (cyclical encoding: hour, minute, day-of-week, day-of-month)
 * - Multi-timeframe momentum (1, 3, 5, 10 bars)
 * - Volatility measures (realized vol, ATR)
 * - Volume analysis (surge, relative volume)
 * - Price position indicators (range position, channel position)
 * - Trend strength (RSI-like, directional momentum)
 * - Interaction terms (momentum * volatility, etc.)
 *
 * Optimized for:
 * - O(1) incremental updates via CircularBuffer
 * - Minimal memory footprint (50-bar lookback)
 * - Production-ready stability (handles edge cases)
 */
class FeatureExtractor {
public:
    // Public constants for feature dimensions
    static constexpr size_t LOOKBACK = 50;      // Lookback window size
    static constexpr size_t NUM_FEATURES = 33;  // 8 time + 25 technical features

    FeatureExtractor();

    /**
     * Extract features from new bar
     * @param bar New OHLCV bar
     * @return Feature vector (std::nullopt during warmup period)
     *
     * Returns std::nullopt if less than LOOKBACK bars have been seen.
     * Once warmed up, always returns valid 33-dimensional feature vector.
     */
    std::optional<Eigen::VectorXd> extract(const Bar& bar);

    /**
     * Access price history (for debugging/inspection)
     */
    const CircularBuffer<Bar>& history() const { return history_; }

    /**
     * Check if warmup period is complete
     */
    bool is_ready() const { return bar_count_ >= LOOKBACK; }

    /**
     * Get number of bars processed
     */
    size_t bar_count() const { return bar_count_; }

    /**
     * Reset to initial state
     */
    void reset();

    /**
     * Get feature names (for debugging/logging)
     */
    static std::vector<std::string> get_feature_names();

private:
    // Member variables
    CircularBuffer<Bar> history_;
    double prev_close_;
    size_t bar_count_;

    // Time feature calculations (cyclical encoding)
    void calculate_time_features(Timestamp timestamp, Eigen::VectorXd& features, int& idx) const;

    // Core feature calculations
    double calculate_momentum(const std::vector<Price>& prices, int period) const;
    double calculate_volatility(const std::vector<Price>& prices, int period) const;
    double calculate_atr(const std::vector<Bar>& bars, int period) const;
    double calculate_volume_surge(const std::vector<Volume>& volumes) const;
    double calculate_relative_volume(const std::vector<Volume>& volumes, int period) const;
    double calculate_price_position(const std::vector<Bar>& bars, Price current_price) const;
    double calculate_channel_position(const std::vector<Bar>& bars, int period) const;
    double calculate_rsi_like(const std::vector<Price>& prices, int period) const;
    double calculate_directional_momentum(const std::vector<Price>& prices, int period) const;

    // Utility helpers
    std::vector<Price> get_closes() const;
    std::vector<Volume> get_volumes() const;
    std::vector<Bar> get_bars() const;
};

} // namespace trading
