#pragma once
#include "core/bar.h"
#include "utils/circular_buffer.h"
#include "predictor/regime_features.h"
#include <Eigen/Dense>
#include <optional>
#include <array>

namespace trading {

/**
 * Enhanced Feature Extractor - 75 Features (Based on online_trader v2.0 - 5.41% MRD)
 *
 * CRITICAL: Uses RAW ABSOLUTE VALUES + normalized ratios (not ratios only!)
 * This matches online_trader v2.0's winning feature set that achieved 5.41% MRD.
 *
 * Feature composition:
 * - 8 Time features (cyclical encoding)
 * - 21 RAW ABSOLUTE features (CRITICAL - added from online_trader v2.0):
 *   * 4 Raw OHLC (close, open, high, low)
 *   * 6 Raw Moving Averages (SMA10, SMA20, SMA50, EMA10, EMA20, EMA50)
 *   * 4 Raw Bollinger Bands (mean, upper, lower, std_dev)
 *   * 1 Raw ATR (absolute volatility)
 *   * 2 Raw Volume (volume, OBV approximation)
 *   * 4 Raw Price Metrics (range, body, upper_wick, lower_wick)
 * - 34 Normalized/ratio features (existing features - kept for compatibility)
 * - 12 Regime features (HMM states, vol regimes, microstructure)
 *
 * Optimized for:
 * - EWRLS learning with price-dependent patterns
 * - O(1) incremental updates via CircularBuffer
 * - Minimal memory footprint (50-bar lookback)
 */
class FeatureExtractor {
public:
    // Public constants for feature dimensions
    static constexpr size_t LOOKBACK = 50;      // Lookback window size
    static constexpr size_t NUM_FEATURES = 75;  // 8 time + 21 raw + 34 normalized + 12 regime

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

    // Regime feature extractor
    RegimeFeatures regime_features_;

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

    // Mean reversion features
    double calculate_ma_deviation(const std::vector<Price>& prices, int period) const;

    // Bollinger Bands features
    struct BollingerBands {
        double mean = 0.0;
        double sd = 0.0;
        double upper = 0.0;
        double lower = 0.0;
        double percent_b = 0.5;
        double bandwidth = 0.0;
    };
    BollingerBands calculate_bollinger_bands(const std::vector<Price>& prices, int period = 20, double k = 2.0) const;

    // RAW ABSOLUTE VALUE CALCULATIONS (from online_trader v2.0)
    double calculate_sma(const std::vector<Price>& prices, int period) const;
    double calculate_ema(const std::vector<Price>& prices, int period) const;
    double calculate_obv_approx(const std::vector<Bar>& bars) const;

    // Utility helpers
    std::vector<Price> get_closes() const;
    std::vector<Volume> get_volumes() const;
    std::vector<Bar> get_bars() const;
};

} // namespace trading
