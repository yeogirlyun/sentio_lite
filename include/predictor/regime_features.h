#pragma once
#include "core/bar.h"
#include "utils/circular_buffer.h"
#include <Eigen/Dense>
#include <array>
#include <vector>

namespace trading {

/**
 * Fast Regime Feature Generator - 12 additional features for EWRLS
 *
 * Adds regime-aware features to help EWRLS make better predictions:
 * - 3 HMM-like state probabilities (trending/ranging detection)
 * - 3 volatility regime probabilities (low/med/high)
 * - 2 regime stability features (duration)
 * - 4 microstructure features (vol ratios, correlations)
 *
 * Fast implementation without Python dependencies:
 * - Simple k-means clustering instead of full HMM/GMM
 * - Rolling window statistics
 * - O(1) incremental updates where possible
 */
class RegimeFeatures {
public:
    static constexpr size_t NUM_REGIME_FEATURES = 12;
    static constexpr size_t WINDOW_SIZE = 90;  // Rolling window for regime detection

    /**
     * Regime feature indices (added to existing 42 features)
     */
    enum RegimeFeatureIndex {
        HMM_State_0_Prob = 0,        // Probability of state 0 (trending up)
        HMM_State_1_Prob = 1,        // Probability of state 1 (ranging)
        HMM_State_2_Prob = 2,        // Probability of state 2 (trending down)

        GMM_Vol_Low_Prob = 3,        // Probability of low volatility
        GMM_Vol_Med_Prob = 4,        // Probability of medium volatility
        GMM_Vol_High_Prob = 5,       // Probability of high volatility

        HMM_State_Duration = 6,      // Bars since last state change
        Vol_Regime_Duration = 7,     // Bars since last vol regime change

        Vol_Ratio_20_60 = 8,         // 20-bar vol / 60-bar vol
        Vol_ZScore = 9,              // Vol z-score vs 60-bar history
        Price_Vol_Correlation = 10,   // 20-bar price-volume correlation
        Volume_ZScore = 11           // Volume z-score vs 60-bar history
    };

    RegimeFeatures();

    /**
     * Extract regime features from price/volume history
     * @param bars Recent bar history (at least 90 bars for reliable detection)
     * @return 12-dimensional feature vector
     *
     * Returns neutral features if not enough data
     */
    Eigen::VectorXd extract(const std::vector<Bar>& bars);

    /**
     * Reset state
     */
    void reset();

    /**
     * Check if ready (enough data)
     */
    bool is_ready() const { return bar_count_ >= 30; }

    /**
     * Get feature names for logging
     */
    static std::vector<std::string> get_feature_names();

private:
    size_t bar_count_ = 0;

    // State tracking
    int last_hmm_state_ = -1;
    int last_vol_regime_ = -1;
    int hmm_state_duration_ = 0;
    int vol_regime_duration_ = 0;

    // Internal feature calculations

    /**
     * Fast HMM-like state detection using k-means on returns
     * Returns: [prob_state_0, prob_state_1, prob_state_2]
     */
    std::array<double, 3> detect_market_state(const std::vector<double>& returns);

    /**
     * Fast volatility regime detection using k-means on rolling vol
     * Returns: [prob_low, prob_med, prob_high]
     */
    std::array<double, 3> detect_volatility_regime(const std::vector<double>& returns);

    /**
     * Calculate rolling volatility
     */
    std::vector<double> calculate_rolling_volatility(const std::vector<double>& returns, int window);

    /**
     * Calculate correlation between two series
     */
    double calculate_correlation(const std::vector<double>& x, const std::vector<double>& y);

    /**
     * Calculate z-score
     */
    double calculate_zscore(double value, const std::vector<double>& history);

    /**
     * Simple k-means clustering (k=3)
     * Returns: cluster assignments [0, 1, 2]
     */
    std::vector<int> kmeans_cluster(const std::vector<double>& data, int k = 3);

    /**
     * Calculate soft probabilities from cluster assignment
     * Uses distance to cluster centers
     */
    std::array<double, 3> calculate_cluster_probabilities(
        double value,
        const std::vector<double>& data,
        const std::vector<int>& clusters
    );
};

} // namespace trading
