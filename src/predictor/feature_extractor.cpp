#include "predictor/feature_extractor.h"
#include "core/math_utils.h"
#include <algorithm>
#include <cmath>

namespace trading {

FeatureExtractor::FeatureExtractor()
    : history_(LOOKBACK), prev_close_(0.0), bar_count_(0) {}

std::optional<Eigen::VectorXd> FeatureExtractor::extract(const Bar& bar) {
    history_.push_back(bar);
    bar_count_++;

    // Need full lookback window for reliable features
    if (!is_ready()) {
        prev_close_ = bar.close;
        return std::nullopt;
    }

    // Get historical data
    auto prices = get_closes();
    auto volumes = get_volumes();
    auto bars = get_bars();

    Eigen::VectorXd features(NUM_FEATURES);
    int idx = 0;

    // ===== TIME FEATURES (0-7) =====
    // Cyclical encoding for intraday patterns (from online_trader)
    calculate_time_features(bar.timestamp, features, idx);

    // ===== MOMENTUM FEATURES (8-11) =====
    // Short-term to longer-term momentum
    features(idx++) = calculate_momentum(prices, 1);   // 1-bar return
    features(idx++) = calculate_momentum(prices, 3);   // 3-bar return
    features(idx++) = calculate_momentum(prices, 5);   // 5-bar return
    features(idx++) = calculate_momentum(prices, 10);  // 10-bar return

    // ===== VOLATILITY FEATURES (12-14) =====
    int vol_idx = idx;
    features(idx++) = calculate_volatility(prices, 10);  // 10-bar realized vol
    features(idx++) = calculate_volatility(prices, 20);  // 20-bar realized vol
    features(idx++) = calculate_atr(bars, 14);           // Average True Range

    // ===== VOLUME FEATURES (15-16) =====
    int vol_surge_idx = idx;
    features(idx++) = calculate_volume_surge(volumes);           // Recent vs average
    features(idx++) = calculate_relative_volume(volumes, 20);    // Normalized volume

    // ===== PRICE POSITION FEATURES (17-19) =====
    int price_pos_idx = idx;
    features(idx++) = calculate_price_position(bars, bar.close);    // Position in 50-bar range
    features(idx++) = calculate_channel_position(bars, 20);         // Position in 20-bar range
    features(idx++) = calculate_channel_position(bars, 10);         // Position in 10-bar range

    // ===== TREND STRENGTH FEATURES (20-22) =====
    int rsi_idx = idx;
    features(idx++) = calculate_rsi_like(prices, 14);              // 14-period RSI-like
    int dir_mom_idx = idx;
    features(idx++) = calculate_directional_momentum(prices, 10);  // Directional strength
    features(idx++) = calculate_directional_momentum(prices, 20);  // Longer-term direction

    // ===== INTERACTION TERMS (23-27) =====
    // These capture non-linear relationships
    features(idx++) = features(8) * features(vol_idx);       // 1-bar momentum * 10-bar volatility
    features(idx++) = features(10) * features(vol_idx);      // 5-bar momentum * volatility
    features(idx++) = features(11) * features(vol_surge_idx); // 10-bar momentum * volume surge
    features(idx++) = features(rsi_idx) * features(vol_idx); // RSI * volatility
    features(idx++) = features(price_pos_idx) * features(dir_mom_idx); // Price position * direction

    // ===== ACCELERATION FEATURES (28-30) =====
    // Rate of change of momentum
    features(idx++) = calculate_momentum(prices, 2) - calculate_momentum(prices, 5);
    features(idx++) = calculate_momentum(prices, 5) - calculate_momentum(prices, 10);
    features(idx++) = features(vol_idx) - features(vol_idx + 1);  // Vol change (10-bar vs 20-bar)

    // ===== DERIVED FEATURES (31-32) =====
    features(idx++) = std::log(1.0 + std::abs(features(11)));  // Log-scaled momentum (10-bar)
    features(idx++) = 1.0;  // Bias term (always 1.0)

    prev_close_ = bar.close;
    return features;
}

void FeatureExtractor::calculate_time_features(Timestamp timestamp, Eigen::VectorXd& features, int& idx) const {
    // Convert timestamp to time_t (seconds since epoch)
    auto duration = timestamp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    time_t time = static_cast<time_t>(seconds);

    // Convert to GMT/UTC time structure
    struct tm* time_info = gmtime(&time);

    if (time_info) {
        double hour = time_info->tm_hour;           // 0-23
        double minute = time_info->tm_min;          // 0-59
        double day_of_week = time_info->tm_wday;    // 0-6 (Sunday=0)
        double day_of_month = time_info->tm_mday;   // 1-31

        // Cyclical encoding using sine/cosine to preserve continuity
        // E.g., 23:59 is close to 00:00
        constexpr double PI = 3.14159265358979323846;

        features(idx++) = std::sin(2.0 * PI * hour / 24.0);           // hour_sin
        features(idx++) = std::cos(2.0 * PI * hour / 24.0);           // hour_cos
        features(idx++) = std::sin(2.0 * PI * minute / 60.0);         // minute_sin
        features(idx++) = std::cos(2.0 * PI * minute / 60.0);         // minute_cos
        features(idx++) = std::sin(2.0 * PI * day_of_week / 7.0);     // dow_sin
        features(idx++) = std::cos(2.0 * PI * day_of_week / 7.0);     // dow_cos
        features(idx++) = std::sin(2.0 * PI * day_of_month / 31.0);   // dom_sin
        features(idx++) = std::cos(2.0 * PI * day_of_month / 31.0);   // dom_cos
    } else {
        // If time parsing fails, fill with zeros (neutral cyclical values)
        for (int i = 0; i < 8; ++i) {
            features(idx++) = 0.0;
        }
    }
}

double FeatureExtractor::calculate_momentum(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n <= static_cast<size_t>(period)) return 0.0;

    Price current = prices[n - 1];
    Price past = prices[n - 1 - period];

    if (past == 0 || std::abs(past) < 1e-10) return 0.0;
    return (current - past) / past;
}

double FeatureExtractor::calculate_volatility(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2 || static_cast<size_t>(period) > n) return 0.0;

    std::vector<double> returns;
    size_t start = n - period;
    for (size_t i = start + 1; i < n; ++i) {
        if (prices[i-1] != 0 && std::abs(prices[i-1]) > 1e-10) {
            returns.push_back((prices[i] - prices[i-1]) / prices[i-1]);
        }
    }

    if (returns.empty()) return 0.0;
    return MathUtils::stddev(returns);
}

double FeatureExtractor::calculate_atr(const std::vector<Bar>& bars, int period) const {
    size_t n = bars.size();
    if (n < 2 || static_cast<size_t>(period) > n) return 0.0;

    std::vector<double> true_ranges;
    size_t start = n - period;

    for (size_t i = start; i < n; ++i) {
        double high_low = bars[i].high - bars[i].low;
        double high_close = (i > 0) ? std::abs(bars[i].high - bars[i-1].close) : 0.0;
        double low_close = (i > 0) ? std::abs(bars[i].low - bars[i-1].close) : 0.0;
        double tr = std::max({high_low, high_close, low_close});
        true_ranges.push_back(tr);
    }

    if (true_ranges.empty()) return 0.0;

    // Normalize by current price
    Price current_price = bars[n-1].close;
    if (current_price == 0 || std::abs(current_price) < 1e-10) return 0.0;

    return MathUtils::mean(true_ranges) / current_price;
}

double FeatureExtractor::calculate_volume_surge(const std::vector<Volume>& volumes) const {
    if (volumes.empty()) return 1.0;

    // Compare recent volume (last 5 bars) to average
    size_t n = volumes.size();
    size_t recent_window = std::min(static_cast<size_t>(5), n);

    double recent_avg = 0.0;
    for (size_t i = n - recent_window; i < n; ++i) {
        recent_avg += static_cast<double>(volumes[i]);
    }
    recent_avg /= recent_window;

    double total_avg = 0.0;
    for (const auto& v : volumes) {
        total_avg += static_cast<double>(v);
    }
    total_avg /= volumes.size();

    if (total_avg == 0 || std::abs(total_avg) < 1e-10) return 1.0;
    return recent_avg / total_avg;
}

double FeatureExtractor::calculate_relative_volume(const std::vector<Volume>& volumes, int period) const {
    size_t n = volumes.size();
    if (n == 0) return 0.0;

    size_t window = std::min(static_cast<size_t>(period), n);
    double avg_volume = 0.0;

    for (size_t i = n - window; i < n; ++i) {
        avg_volume += static_cast<double>(volumes[i]);
    }
    avg_volume /= window;

    double current_volume = static_cast<double>(volumes[n-1]);

    if (avg_volume == 0 || std::abs(avg_volume) < 1e-10) return 0.0;
    return (current_volume - avg_volume) / avg_volume;
}

double FeatureExtractor::calculate_price_position(const std::vector<Bar>& bars,
                                                   Price current_price) const {
    if (bars.empty()) return 0.5;

    std::vector<double> highs, lows;
    for (const auto& bar : bars) {
        highs.push_back(bar.high);
        lows.push_back(bar.low);
    }

    double high_n = MathUtils::max(highs);
    double low_n = MathUtils::min(lows);
    double range = high_n - low_n;

    if (range < 1e-8) return 0.5;
    return (current_price - low_n) / range;
}

double FeatureExtractor::calculate_channel_position(const std::vector<Bar>& bars, int period) const {
    size_t n = bars.size();
    if (n == 0) return 0.5;

    size_t window = std::min(static_cast<size_t>(period), n);
    std::vector<double> highs, lows;

    for (size_t i = n - window; i < n; ++i) {
        highs.push_back(bars[i].high);
        lows.push_back(bars[i].low);
    }

    double high_n = MathUtils::max(highs);
    double low_n = MathUtils::min(lows);
    double range = high_n - low_n;

    Price current_price = bars[n-1].close;

    if (range < 1e-8) return 0.5;
    return (current_price - low_n) / range;
}

double FeatureExtractor::calculate_rsi_like(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2) return 0.5;

    size_t window = std::min(static_cast<size_t>(period), n - 1);
    std::vector<double> gains, losses;

    for (size_t i = n - window; i < n; ++i) {
        if (prices[i-1] != 0 && std::abs(prices[i-1]) > 1e-10) {
            double ret = (prices[i] - prices[i-1]) / prices[i-1];
            if (ret > 0) {
                gains.push_back(ret);
                losses.push_back(0.0);
            } else {
                gains.push_back(0.0);
                losses.push_back(-ret);
            }
        }
    }

    if (gains.empty()) return 0.5;

    double avg_gain = MathUtils::mean(gains);
    double avg_loss = MathUtils::mean(losses);

    if (avg_loss < 1e-8) return 1.0;
    if (avg_gain < 1e-8) return 0.0;

    // Normalize to [0, 1] range like RSI
    double rs = avg_gain / avg_loss;
    return rs / (1.0 + rs);
}

double FeatureExtractor::calculate_directional_momentum(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2 || static_cast<size_t>(period) >= n) return 0.0;

    int up_moves = 0;
    int down_moves = 0;
    size_t start = n - period - 1;

    for (size_t i = start + 1; i < n; ++i) {
        if (prices[i] > prices[i-1]) up_moves++;
        else if (prices[i] < prices[i-1]) down_moves++;
    }

    int total_moves = up_moves + down_moves;
    if (total_moves == 0) return 0.0;

    // Return net directional bias: +1 all up, -1 all down, 0 neutral
    return static_cast<double>(up_moves - down_moves) / total_moves;
}

void FeatureExtractor::reset() {
    history_.clear();
    prev_close_ = 0.0;
    bar_count_ = 0;
}

std::vector<Price> FeatureExtractor::get_closes() const {
    std::vector<Price> closes;
    closes.reserve(history_.size());
    for (size_t i = 0; i < history_.size(); ++i) {
        closes.push_back(history_[i].close);
    }
    return closes;
}

std::vector<Volume> FeatureExtractor::get_volumes() const {
    std::vector<Volume> volumes;
    volumes.reserve(history_.size());
    for (size_t i = 0; i < history_.size(); ++i) {
        volumes.push_back(history_[i].volume);
    }
    return volumes;
}

std::vector<Bar> FeatureExtractor::get_bars() const {
    return history_.to_vector();
}

std::vector<std::string> FeatureExtractor::get_feature_names() {
    return {
        // Time (0-7) - Cyclical encoding
        "hour_sin", "hour_cos", "minute_sin", "minute_cos",
        "dow_sin", "dow_cos", "dom_sin", "dom_cos",
        // Momentum (8-11)
        "momentum_1", "momentum_3", "momentum_5", "momentum_10",
        // Volatility (12-14)
        "volatility_10", "volatility_20", "atr_14",
        // Volume (15-16)
        "volume_surge", "relative_volume_20",
        // Price Position (17-19)
        "price_position_50", "channel_position_20", "channel_position_10",
        // Trend Strength (20-22)
        "rsi_14", "directional_momentum_10", "directional_momentum_20",
        // Interactions (23-27)
        "mom1_x_vol10", "mom5_x_vol10", "mom10_x_volsurge", "rsi_x_vol", "pricepos_x_direction",
        // Acceleration (28-30)
        "momentum_accel_short", "momentum_accel_long", "volatility_change",
        // Derived (31-32)
        "log_momentum", "bias"
    };
}

} // namespace trading
