#include "predictor/regime_features.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

namespace trading {

RegimeFeatures::RegimeFeatures() {
    reset();
}

void RegimeFeatures::reset() {
    bar_count_ = 0;
    last_hmm_state_ = -1;
    last_vol_regime_ = -1;
    hmm_state_duration_ = 0;
    vol_regime_duration_ = 0;
}

Eigen::VectorXd RegimeFeatures::extract(const std::vector<Bar>& bars) {
    Eigen::VectorXd features(NUM_REGIME_FEATURES);

    bar_count_ = bars.size();

    // Not enough data - return neutral features
    if (bars.size() < 30) {
        features << 0.33, 0.33, 0.34,  // HMM states
                    0.33, 0.33, 0.34,  // Vol regimes
                    0, 0,              // Durations
                    1.0, 0.0, 0.0, 0.0; // Microstructure
        return features;
    }

    // Extract prices and volumes
    std::vector<double> prices, volumes;
    for (const auto& bar : bars) {
        prices.push_back(bar.close);
        volumes.push_back(bar.volume);
    }

    // Calculate log returns
    std::vector<double> returns;
    for (size_t i = 1; i < prices.size(); ++i) {
        double ret = std::log(prices[i] / (prices[i-1] + 1e-10));
        returns.push_back(ret);
    }

    if (returns.empty()) {
        features << 0.33, 0.33, 0.34, 0.33, 0.33, 0.34, 0, 0, 1.0, 0.0, 0.0, 0.0;
        return features;
    }

    // ===================================================================
    // 1. HMM-like Market State Detection (3 features)
    // ===================================================================
    auto hmm_probs = detect_market_state(returns);

    // Track state duration
    int current_hmm_state = std::distance(hmm_probs.begin(),
                                         std::max_element(hmm_probs.begin(), hmm_probs.end()));
    if (current_hmm_state != last_hmm_state_) {
        hmm_state_duration_ = 0;
        last_hmm_state_ = current_hmm_state;
    } else {
        hmm_state_duration_++;
    }

    // ===================================================================
    // 2. Volatility Regime Detection (3 features)
    // ===================================================================
    auto vol_probs = detect_volatility_regime(returns);

    // Track vol regime duration
    int current_vol_regime = std::distance(vol_probs.begin(),
                                          std::max_element(vol_probs.begin(), vol_probs.end()));
    if (current_vol_regime != last_vol_regime_) {
        vol_regime_duration_ = 0;
        last_vol_regime_ = current_vol_regime;
    } else {
        vol_regime_duration_++;
    }

    // ===================================================================
    // 3. Microstructure Features (4 features)
    // ===================================================================

    // 3a. Volatility ratio (20-bar / 60-bar)
    auto rolling_vol = calculate_rolling_volatility(returns, 20);
    double vol_ratio = 1.0;
    if (rolling_vol.size() >= 60 && rolling_vol.back() > 1e-10) {
        double vol_20 = rolling_vol.back();
        double vol_60_avg = 0.0;
        for (size_t i = rolling_vol.size() - 60; i < rolling_vol.size(); ++i) {
            vol_60_avg += rolling_vol[i];
        }
        vol_60_avg /= 60.0;
        if (vol_60_avg > 1e-10) {
            vol_ratio = vol_20 / vol_60_avg;
        }
    }
    vol_ratio = std::clamp(vol_ratio, -3.0, 3.0);

    // 3b. Volatility z-score
    double vol_zscore = 0.0;
    if (rolling_vol.size() >= 60) {
        std::vector<double> recent_vol(rolling_vol.end() - 60, rolling_vol.end());
        vol_zscore = calculate_zscore(rolling_vol.back(), recent_vol);
    }
    vol_zscore = std::clamp(vol_zscore, -3.0, 3.0);

    // 3c. Price-volume correlation (20 bars)
    double price_vol_corr = 0.0;
    if (prices.size() >= 20 && volumes.size() >= 20) {
        std::vector<double> recent_prices(prices.end() - 20, prices.end());
        std::vector<double> recent_volumes(volumes.end() - 20, volumes.end());
        price_vol_corr = calculate_correlation(recent_prices, recent_volumes);
    }
    price_vol_corr = std::clamp(price_vol_corr, -1.0, 1.0);

    // 3d. Volume z-score
    double volume_zscore = 0.0;
    if (volumes.size() >= 60) {
        std::vector<double> recent_volumes(volumes.end() - 60, volumes.end());
        volume_zscore = calculate_zscore(volumes.back(), recent_volumes);
    }
    volume_zscore = std::clamp(volume_zscore, -3.0, 3.0);

    // Assemble feature vector
    features << hmm_probs[0], hmm_probs[1], hmm_probs[2],
                vol_probs[0], vol_probs[1], vol_probs[2],
                std::min(hmm_state_duration_, 120),
                std::min(vol_regime_duration_, 120),
                vol_ratio, vol_zscore, price_vol_corr, volume_zscore;

    return features;
}

std::array<double, 3> RegimeFeatures::detect_market_state(const std::vector<double>& returns) {
    if (returns.size() < 30) {
        return {0.33, 0.33, 0.34};
    }

    // Use k-means clustering on returns to detect 3 states
    auto clusters = kmeans_cluster(returns, 3);

    // Calculate soft probabilities for current return
    double current_return = returns.back();
    auto probs = calculate_cluster_probabilities(current_return, returns, clusters);

    return probs;
}

std::array<double, 3> RegimeFeatures::detect_volatility_regime(const std::vector<double>& returns) {
    if (returns.size() < 30) {
        return {0.33, 0.33, 0.34};
    }

    // Calculate rolling volatility
    auto rolling_vol = calculate_rolling_volatility(returns, 20);

    if (rolling_vol.size() < 30) {
        return {0.33, 0.33, 0.34};
    }

    // Use k-means clustering on volatility to detect 3 regimes
    auto clusters = kmeans_cluster(rolling_vol, 3);

    // Calculate soft probabilities for current volatility
    double current_vol = rolling_vol.back();
    auto probs = calculate_cluster_probabilities(current_vol, rolling_vol, clusters);

    return probs;
}

std::vector<double> RegimeFeatures::calculate_rolling_volatility(
    const std::vector<double>& returns, int window) {

    std::vector<double> rolling_vol;

    for (size_t i = window; i <= returns.size(); ++i) {
        // Calculate std dev of last 'window' returns
        double sum = 0.0;
        double sum_sq = 0.0;
        int count = 0;

        for (size_t j = i - window; j < i; ++j) {
            sum += returns[j];
            sum_sq += returns[j] * returns[j];
            count++;
        }

        double mean = sum / count;
        double variance = (sum_sq / count) - (mean * mean);
        double std_dev = std::sqrt(std::max(0.0, variance));

        rolling_vol.push_back(std_dev);
    }

    return rolling_vol;
}

double RegimeFeatures::calculate_correlation(
    const std::vector<double>& x,
    const std::vector<double>& y) {

    if (x.size() != y.size() || x.size() < 2) {
        return 0.0;
    }

    size_t n = x.size();

    // Calculate means
    double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / n;
    double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / n;

    // Calculate covariance and standard deviations
    double cov = 0.0, var_x = 0.0, var_y = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;
        cov += dx * dy;
        var_x += dx * dx;
        var_y += dy * dy;
    }

    double std_x = std::sqrt(var_x / n);
    double std_y = std::sqrt(var_y / n);

    if (std_x < 1e-10 || std_y < 1e-10) {
        return 0.0;
    }

    return cov / (n * std_x * std_y);
}

double RegimeFeatures::calculate_zscore(double value, const std::vector<double>& history) {
    if (history.size() < 2) {
        return 0.0;
    }

    double mean = std::accumulate(history.begin(), history.end(), 0.0) / history.size();

    double sum_sq_diff = 0.0;
    for (double v : history) {
        double diff = v - mean;
        sum_sq_diff += diff * diff;
    }
    double std_dev = std::sqrt(sum_sq_diff / history.size());

    if (std_dev < 1e-10) {
        return 0.0;
    }

    return (value - mean) / std_dev;
}

std::vector<int> RegimeFeatures::kmeans_cluster(const std::vector<double>& data, int k) {
    if (data.size() < static_cast<size_t>(k)) {
        return std::vector<int>(data.size(), 0);
    }

    // Initialize centroids using quantiles
    std::vector<double> sorted_data = data;
    std::sort(sorted_data.begin(), sorted_data.end());

    std::vector<double> centroids(k);
    for (int i = 0; i < k; ++i) {
        size_t idx = (sorted_data.size() * (i + 1)) / (k + 1);
        centroids[i] = sorted_data[std::min(idx, sorted_data.size() - 1)];
    }

    // Run k-means for 10 iterations (fast convergence)
    std::vector<int> assignments(data.size());

    for (int iter = 0; iter < 10; ++iter) {
        // Assignment step
        for (size_t i = 0; i < data.size(); ++i) {
            double min_dist = std::numeric_limits<double>::max();
            int best_cluster = 0;

            for (int c = 0; c < k; ++c) {
                double dist = std::abs(data[i] - centroids[c]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = c;
                }
            }

            assignments[i] = best_cluster;
        }

        // Update step
        std::vector<double> new_centroids(k, 0.0);
        std::vector<int> counts(k, 0);

        for (size_t i = 0; i < data.size(); ++i) {
            int c = assignments[i];
            new_centroids[c] += data[i];
            counts[c]++;
        }

        for (int c = 0; c < k; ++c) {
            if (counts[c] > 0) {
                centroids[c] = new_centroids[c] / counts[c];
            }
        }
    }

    return assignments;
}

std::array<double, 3> RegimeFeatures::calculate_cluster_probabilities(
    double value,
    const std::vector<double>& data,
    const std::vector<int>& clusters) {

    // Calculate cluster centers
    std::vector<double> centroids(3, 0.0);
    std::vector<int> counts(3, 0);

    for (size_t i = 0; i < data.size(); ++i) {
        int c = clusters[i];
        centroids[c] += data[i];
        counts[c]++;
    }

    for (int c = 0; c < 3; ++c) {
        if (counts[c] > 0) {
            centroids[c] /= counts[c];
        }
    }

    // Calculate distances to each cluster center
    std::array<double, 3> distances;
    for (int c = 0; c < 3; ++c) {
        distances[c] = std::abs(value - centroids[c]);
    }

    // Convert distances to probabilities (inverse distance weighting)
    // Use softmax-like transformation
    std::array<double, 3> probs;
    double sum_exp = 0.0;
    constexpr double temperature = 0.1;  // Controls sharpness of probabilities

    for (int c = 0; c < 3; ++c) {
        probs[c] = std::exp(-distances[c] / (temperature + 1e-10));
        sum_exp += probs[c];
    }

    // Normalize
    if (sum_exp > 1e-10) {
        for (int c = 0; c < 3; ++c) {
            probs[c] /= sum_exp;
        }
    } else {
        probs = {0.33, 0.33, 0.34};
    }

    return probs;
}

std::vector<std::string> RegimeFeatures::get_feature_names() {
    return {
        "hmm_state_0_prob",
        "hmm_state_1_prob",
        "hmm_state_2_prob",
        "gmm_vol_low_prob",
        "gmm_vol_med_prob",
        "gmm_vol_high_prob",
        "hmm_state_duration",
        "vol_regime_duration",
        "vol_ratio_20_60",
        "vol_zscore",
        "price_vol_correlation",
        "volume_zscore"
    };
}

} // namespace trading
