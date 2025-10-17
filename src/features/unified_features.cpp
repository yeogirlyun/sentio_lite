#include "features/unified_features.h"
#include <algorithm>
#include <numeric>

namespace sentio {
namespace features {

UnifiedFeatures::UnifiedFeatures()
    : bar_count_(0)
    , prev_close_(std::numeric_limits<double>::quiet_NaN()) {
    features_.fill(std::numeric_limits<double>::quiet_NaN());
}

bool UnifiedFeatures::update(const trading::Bar& bar) {
    history_.push_back(bar);
    if (history_.size() > LOOKBACK) {
        history_.pop_front();
    }

    bar_count_++;

    // Calculate 1-bar return for volatility tracking
    if (!std::isnan(prev_close_) && prev_close_ > 0) {
        double ret = (bar.close - prev_close_) / prev_close_;
        returns_.push_back(ret);
        if (returns_.size() > LOOKBACK) {
            returns_.pop_front();
        }
    }
    prev_close_ = bar.close;

    // Only compute features if we have enough history
    if (history_.size() < 2) {
        return false;
    }

    size_t idx = 0;

    // === MOMENTUM FEATURES (4 features) ===
    features_[idx++] = calculate_momentum(1);   // 1-bar return
    features_[idx++] = calculate_momentum(5);   // 5-bar return
    features_[idx++] = calculate_momentum(10);  // 10-bar return
    features_[idx++] = calculate_momentum(20);  // 20-bar return

    // === VOLATILITY FEATURES (3 features) ===
    features_[idx++] = calculate_volatility(10);  // 10-bar volatility
    features_[idx++] = calculate_volatility(20);  // 20-bar volatility
    features_[idx++] = calculate_atr(14);         // Average True Range

    // === VOLUME FEATURES (2 features) ===
    features_[idx++] = calculate_volume_surge();  // Volume ratio
    double avg_vol = 0.0;
    for (size_t i = std::max(0, static_cast<int>(history_.size()) - 20); i < history_.size(); ++i) {
        avg_vol += static_cast<double>(history_[i].volume);
    }
    avg_vol /= std::min(20.0, static_cast<double>(history_.size()));
    features_[idx++] = (avg_vol > 0) ? (static_cast<double>(bar.volume) / avg_vol) : 1.0;

    // === PRICE POSITION FEATURES (3 features) ===
    features_[idx++] = calculate_price_position(10);  // 10-bar position
    features_[idx++] = calculate_price_position(20);  // 20-bar position
    features_[idx++] = calculate_price_position(50);  // 50-bar position

    // === TREND STRENGTH (2 features) ===
    features_[idx++] = calculate_rsi(14);  // RSI indicator

    // Price vs moving average
    double ma20 = 0.0;
    int count = std::min(20, static_cast<int>(history_.size()));
    for (int i = history_.size() - count; i < static_cast<int>(history_.size()); ++i) {
        ma20 += history_[i].close;
    }
    ma20 /= count;
    features_[idx++] = (ma20 > 0) ? (bar.close / ma20 - 1.0) : 0.0;

    // === RANGE INDICATORS (3 features) ===
    // High-Low range as % of close
    features_[idx++] = (bar.close > 0) ? ((bar.high - bar.low) / bar.close) : 0.0;

    // Close position within bar (0 = at low, 1 = at high)
    double bar_range = bar.high - bar.low;
    features_[idx++] = (bar_range > 0) ? ((bar.close - bar.low) / bar_range) : 0.5;

    // Gap from previous close
    if (history_.size() >= 2) {
        double prev_c = history_[history_.size() - 2].close;
        features_[idx++] = (prev_c > 0) ? ((bar.open - prev_c) / prev_c) : 0.0;
    } else {
        features_[idx++] = 0.0;
    }

    // === INTERACTION TERMS (4 features) ===
    features_[idx++] = features_[0] * features_[4];  // 1-bar return * volatility
    features_[idx++] = features_[3] * features_[7];  // 20-bar return * volume
    features_[idx++] = features_[11] * features_[14]; // RSI * price position
    features_[idx++] = features_[4] * features_[9];  // Volatility * price position

    // === STATISTICAL FEATURES (2 features) ===
    // Skewness of recent returns
    if (returns_.size() >= 20) {
        double mean = std::accumulate(returns_.begin(), returns_.end(), 0.0) / returns_.size();
        double m2 = 0, m3 = 0;
        for (double r : returns_) {
            double diff = r - mean;
            m2 += diff * diff;
            m3 += diff * diff * diff;
        }
        m2 /= returns_.size();
        m3 /= returns_.size();
        double stddev = std::sqrt(m2);
        features_[idx++] = (stddev > 0) ? (m3 / (stddev * stddev * stddev)) : 0.0;
    } else {
        features_[idx++] = 0.0;
    }

    // Kurtosis indicator (excess kurtosis)
    if (returns_.size() >= 20) {
        double mean = std::accumulate(returns_.begin(), returns_.end(), 0.0) / returns_.size();
        double m2 = 0, m4 = 0;
        for (double r : returns_) {
            double diff = r - mean;
            double diff2 = diff * diff;
            m2 += diff2;
            m4 += diff2 * diff2;
        }
        m2 /= returns_.size();
        m4 /= returns_.size();
        double variance = m2;
        features_[idx++] = (variance > 0) ? (m4 / (variance * variance) - 3.0) : 0.0;
    } else {
        features_[idx++] = 0.0;
    }

    // === BIAS TERM (1 feature) ===
    features_[idx++] = 1.0;

    return true;
}

Eigen::VectorXd UnifiedFeatures::get_features() const {
    Eigen::VectorXd vec(NUM_FEATURES);
    for (size_t i = 0; i < NUM_FEATURES; ++i) {
        vec(i) = features_[i];
    }
    return vec;
}

void UnifiedFeatures::reset() {
    history_.clear();
    returns_.clear();
    bar_count_ = 0;
    prev_close_ = std::numeric_limits<double>::quiet_NaN();
    features_.fill(std::numeric_limits<double>::quiet_NaN());
}

double UnifiedFeatures::calculate_momentum(int period) const {
    if (history_.size() <= static_cast<size_t>(period)) {
        return 0.0;
    }

    double current = history_.back().close;
    double past = history_[history_.size() - 1 - period].close;

    return (past > 0) ? ((current - past) / past) : 0.0;
}

double UnifiedFeatures::calculate_volatility(int period) const {
    if (returns_.size() < static_cast<size_t>(period)) {
        return 0.0;
    }

    // Use last 'period' returns
    std::vector<double> recent;
    int start_idx = std::max(0, static_cast<int>(returns_.size()) - period);
    for (int i = start_idx; i < static_cast<int>(returns_.size()); ++i) {
        recent.push_back(returns_[i]);
    }

    double mean = std::accumulate(recent.begin(), recent.end(), 0.0) / recent.size();
    double sq_sum = 0.0;
    for (double r : recent) {
        sq_sum += (r - mean) * (r - mean);
    }

    return std::sqrt(sq_sum / recent.size());
}

double UnifiedFeatures::calculate_volume_surge() const {
    if (history_.size() < 2) {
        return 1.0;
    }

    double current_vol = static_cast<double>(history_.back().volume);
    double avg_vol = 0.0;
    int count = std::min(20, static_cast<int>(history_.size()) - 1);

    for (int i = history_.size() - 1 - count; i < static_cast<int>(history_.size()) - 1; ++i) {
        avg_vol += static_cast<double>(history_[i].volume);
    }
    avg_vol /= count;

    return (avg_vol > 0) ? (current_vol / avg_vol) : 1.0;
}

double UnifiedFeatures::calculate_price_position(int period) const {
    if (history_.size() < static_cast<size_t>(period)) {
        return 0.5;
    }

    double high = std::numeric_limits<double>::lowest();
    double low = std::numeric_limits<double>::max();

    int start_idx = std::max(0, static_cast<int>(history_.size()) - period);
    for (int i = start_idx; i < static_cast<int>(history_.size()); ++i) {
        high = std::max(high, history_[i].high);
        low = std::min(low, history_[i].low);
    }

    double range = high - low;
    if (range < 1e-8) {
        return 0.5;
    }

    double current = history_.back().close;
    return (current - low) / range;
}

double UnifiedFeatures::calculate_rsi(int period) const {
    if (returns_.size() < static_cast<size_t>(period)) {
        return 0.5;
    }

    double gain = 0.0, loss = 0.0;
    int start_idx = std::max(0, static_cast<int>(returns_.size()) - period);

    for (int i = start_idx; i < static_cast<int>(returns_.size()); ++i) {
        if (returns_[i] > 0) {
            gain += returns_[i];
        } else {
            loss += -returns_[i];
        }
    }

    gain /= period;
    loss /= period;

    if (loss < 1e-8) {
        return 1.0;
    }

    double rs = gain / loss;
    return rs / (1.0 + rs);
}

double UnifiedFeatures::calculate_atr(int period) const {
    if (history_.size() < static_cast<size_t>(period + 1)) {
        return 0.0;
    }

    double sum_tr = 0.0;
    int start_idx = std::max(1, static_cast<int>(history_.size()) - period);

    for (int i = start_idx; i < static_cast<int>(history_.size()); ++i) {
        const auto& bar = history_[i];
        const auto& prev_bar = history_[i - 1];

        double tr = std::max({
            bar.high - bar.low,
            std::abs(bar.high - prev_bar.close),
            std::abs(bar.low - prev_bar.close)
        });

        sum_tr += tr;
    }

    double atr = sum_tr / std::min(period, static_cast<int>(history_.size()) - 1);
    double current_close = history_.back().close;

    // Return ATR as percentage of price
    return (current_close > 0) ? (atr / current_close) : 0.0;
}

std::vector<std::string> UnifiedFeatures::get_feature_names() {
    return {
        "momentum_1", "momentum_5", "momentum_10", "momentum_20",
        "volatility_10", "volatility_20", "atr_14",
        "volume_surge", "volume_ratio",
        "price_pos_10", "price_pos_20", "price_pos_50",
        "rsi_14", "price_vs_ma20",
        "bar_range", "close_position", "gap",
        "mom_vol", "mom20_vol", "rsi_pos", "vol_pos",
        "skewness", "kurtosis",
        "bias"
    };
}

} // namespace features
} // namespace sentio
