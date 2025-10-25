#include "strategy/williams_rsi_strategy.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace trading {

WilliamsRsiStrategy::WilliamsRsiStrategy(const WilliamsRsiConfig& config)
    : config_(config) {}

WilliamsRsiSignal WilliamsRsiStrategy::generate_signal(const Bar& bar, const std::string& symbol) {
    // Update price history
    closes_.push_back(bar.close);
    highs_.push_back(bar.high);
    lows_.push_back(bar.low);
    bar_count_++;

    // Trim history to reasonable size (2048 bars max)
    const size_t max_history = 2048;
    auto trim = [max_history](auto& vec) {
        if (vec.size() > max_history) {
            vec.erase(vec.begin(), vec.begin() + (vec.size() - max_history));
        }
    };
    trim(closes_);
    trim(highs_);
    trim(lows_);

    WilliamsRsiSignal signal;
    signal.timestamp = bar.timestamp;
    signal.symbol = symbol;

    // Calculate indicators
    signal.williams_r = calculate_williams_r(config_.williams_period);
    signal.rsi = calculate_rsi(config_.rsi_period);
    calculate_bollinger_bands(config_.bb_period, config_.bb_stddev,
                             signal.bb_upper, signal.bb_middle, signal.bb_lower);
    signal.price_percentile = calculate_price_percentile(bar.close, signal.bb_lower, signal.bb_upper);

    // Store indicator history
    williams_history_.push_back(signal.williams_r);
    rsi_history_.push_back(signal.rsi);
    trim(williams_history_);
    trim(rsi_history_);

    // Detect crossover patterns
    detect_crossovers(signal.williams_r, signal.rsi,
                     signal.is_crossing_up, signal.is_crossing_down,
                     signal.is_approaching_up, signal.is_approaching_down);

    // Check for fresh crosses
    signal.is_fresh_cross_up = (bars_since_cross_up_ > 0 && bars_since_cross_up_ <= config_.fresh_bars);
    signal.is_fresh_cross_down = (bars_since_cross_down_ > 0 && bars_since_cross_down_ <= config_.fresh_bars);

    // Update cross counters
    if (signal.is_crossing_up) {
        bars_since_cross_up_ = 1;
        bars_since_cross_down_ = 999;
    } else if (signal.is_crossing_down) {
        bars_since_cross_down_ = 1;
        bars_since_cross_up_ = 999;
    } else {
        if (bars_since_cross_up_ < 999) bars_since_cross_up_++;
        if (bars_since_cross_down_ < 999) bars_since_cross_down_++;
    }

    // Calculate final signal
    signal.probability = calculate_probability(signal.williams_r, signal.rsi, signal.price_percentile,
                                              signal.is_crossing_up, signal.is_crossing_down,
                                              signal.is_approaching_up, signal.is_approaching_down,
                                              signal.is_fresh_cross_up, signal.is_fresh_cross_down);

    signal.confidence = calculate_confidence(signal.price_percentile,
                                            signal.is_crossing_up, signal.is_crossing_down,
                                            signal.is_approaching_up, signal.is_approaching_down,
                                            signal.is_fresh_cross_up, signal.is_fresh_cross_down);

    // Determine direction
    signal.is_long = signal.probability > 0.52;
    signal.is_short = signal.probability < 0.48;
    signal.is_neutral = !signal.is_long && !signal.is_short;

    return signal;
}

void WilliamsRsiStrategy::reset() {
    closes_.clear();
    highs_.clear();
    lows_.clear();
    williams_history_.clear();
    rsi_history_.clear();
    avg_gain_ = 0.0;
    avg_loss_ = 0.0;
    rsi_initialized_ = false;
    bars_since_cross_up_ = 999;
    bars_since_cross_down_ = 999;
    bar_count_ = 0;
}

// ===== INDICATOR CALCULATIONS =====

double WilliamsRsiStrategy::calculate_williams_r(int period) const {
    if (static_cast<int>(highs_.size()) < period) return -50.0;  // Neutral

    // Find highest high and lowest low over period
    double highest = -std::numeric_limits<double>::infinity();
    double lowest = std::numeric_limits<double>::infinity();

    for (int i = static_cast<int>(highs_.size()) - period; i < static_cast<int>(highs_.size()); ++i) {
        highest = std::max(highest, highs_[i]);
        lowest = std::min(lowest, lows_[i]);
    }

    if (highest - lowest < 1e-8) return -50.0;  // No range

    // Williams %R = (Highest High - Close) / (Highest High - Lowest Low) * -100
    double williams = ((highest - closes_.back()) / (highest - lowest)) * -100.0;

    return std::max(-100.0, std::min(0.0, williams));  // Clamp to [-100, 0]
}

double WilliamsRsiStrategy::calculate_rsi(int period) {
    // Fixed Wilder's RSI using exponential smoothing
    if (static_cast<int>(closes_.size()) < period + 1) {
        return 50.0;  // Not enough data
    }

    // Calculate current price change
    double current_close = closes_.back();
    double prev_close = closes_[closes_.size() - 2];
    double change = current_close - prev_close;
    double gain = (change > 0) ? change : 0.0;
    double loss = (change < 0) ? -change : 0.0;

    if (!rsi_initialized_) {
        // First-time initialization: use SMA for initial average
        if (static_cast<int>(closes_.size()) < period + 1) {
            return 50.0;
        }

        double total_gain = 0.0;
        double total_loss = 0.0;
        for (size_t i = closes_.size() - period; i < closes_.size(); ++i) {
            double chg = closes_[i] - closes_[i - 1];
            total_gain += (chg > 0) ? chg : 0.0;
            total_loss += (chg < 0) ? -chg : 0.0;
        }

        avg_gain_ = total_gain / period;
        avg_loss_ = total_loss / period;
        rsi_initialized_ = true;
    } else {
        // Wilder's EMA smoothing
        avg_gain_ = (avg_gain_ * (period - 1) + gain) / period;
        avg_loss_ = (avg_loss_ * (period - 1) + loss) / period;
    }

    if (avg_loss_ == 0.0) return 100.0;

    double rs = avg_gain_ / avg_loss_;
    return 100.0 - (100.0 / (1.0 + rs));
}

void WilliamsRsiStrategy::calculate_bollinger_bands(int period, double stddev,
                                                     double& upper, double& middle, double& lower) const {
    if (static_cast<int>(closes_.size()) < period) {
        middle = closes_.empty() ? 0.0 : closes_.back();
        upper = middle;
        lower = middle;
        return;
    }

    middle = compute_sma(closes_, period);
    double sd = compute_stddev(closes_, period, middle);

    upper = middle + (stddev * sd);
    lower = middle - (stddev * sd);
}

double WilliamsRsiStrategy::calculate_price_percentile(double price, double lower, double upper) const {
    if (upper - lower < 1e-8) return 50.0;  // No range

    // Convert price position to 0-100 percentile
    double percentile = ((price - lower) / (upper - lower)) * 100.0;
    return std::max(0.0, std::min(100.0, percentile));
}

// ===== CROSSOVER DETECTION =====

void WilliamsRsiStrategy::detect_crossovers(double williams, double rsi,
                                            bool& crossing_up, bool& crossing_down,
                                            bool& approaching_up, bool& approaching_down) {
    crossing_up = false;
    crossing_down = false;
    approaching_up = false;
    approaching_down = false;

    if (williams_history_.size() < 2 || rsi_history_.size() < 2) return;

    // Get previous values
    double prev_williams = williams_history_[williams_history_.size() - 2];
    double prev_rsi = rsi_history_[rsi_history_.size() - 2];

    // Convert Williams %R from [-100, 0] to [0, 100] for easier comparison with RSI
    double williams_scaled = williams + 100.0;  // Now [0, 100]
    double prev_williams_scaled = prev_williams + 100.0;

    // CROSSING DETECTION: Williams crosses RSI
    // Bullish cross: Williams was below RSI, now above
    if (prev_williams_scaled < prev_rsi && williams_scaled >= rsi) {
        crossing_up = true;
    }
    // Bearish cross: Williams was above RSI, now below
    else if (prev_williams_scaled > prev_rsi && williams_scaled <= rsi) {
        crossing_down = true;
    }

    // APPROACHING DETECTION: Converging but not yet crossed
    if (!crossing_up && !crossing_down) {
        double distance = std::abs(williams_scaled - rsi);
        double prev_distance = std::abs(prev_williams_scaled - prev_rsi);

        // Approaching if distance is decreasing and within threshold
        if (distance < prev_distance && distance < config_.approach_threshold) {
            // Determine direction of approach
            if (williams_scaled < rsi && (prev_williams_scaled < prev_rsi)) {
                // Both below, converging upward
                if ((rsi - williams_scaled) < (prev_rsi - prev_williams_scaled)) {
                    approaching_up = true;
                }
            } else if (williams_scaled > rsi && (prev_williams_scaled > prev_rsi)) {
                // Both above, converging downward
                if ((williams_scaled - rsi) < (prev_williams_scaled - prev_rsi)) {
                    approaching_down = true;
                }
            }
        }
    }
}

// ===== SIGNAL CALCULATION =====

double WilliamsRsiStrategy::calculate_probability(double williams, double rsi, double price_percentile,
                                                  bool crossing_up, bool crossing_down,
                                                  bool approaching_up, bool approaching_down,
                                                  bool fresh_up, bool fresh_down) const {
    double base_prob = 0.5;  // Neutral

    // Band proximity factor: stronger signals near bands
    double lower_proximity = 0.0;
    double upper_proximity = 0.0;

    if (price_percentile < config_.lower_band_zone) {
        // Near lower band - bullish reversal zone
        lower_proximity = (config_.lower_band_zone - price_percentile) / config_.lower_band_zone;
    } else if (price_percentile > config_.upper_band_zone) {
        // Near upper band - bearish reversal zone
        upper_proximity = (price_percentile - config_.upper_band_zone) / (100.0 - config_.upper_band_zone);
    }

    // Calculate bullish component
    double bullish_signal = 0.0;
    if (crossing_up) {
        bullish_signal = config_.crossing_strength * lower_proximity;
    } else if (approaching_up) {
        bullish_signal = config_.approaching_strength * lower_proximity;
    } else if (fresh_up) {
        // Decay strength linearly over fresh_bars
        double freshness = 1.0 - (static_cast<double>(bars_since_cross_up_) / config_.fresh_bars);
        bullish_signal = config_.fresh_strength * freshness * lower_proximity;
    }

    // Calculate bearish component
    double bearish_signal = 0.0;
    if (crossing_down) {
        bearish_signal = config_.crossing_strength * upper_proximity;
    } else if (approaching_down) {
        bearish_signal = config_.approaching_strength * upper_proximity;
    } else if (fresh_down) {
        double freshness = 1.0 - (static_cast<double>(bars_since_cross_down_) / config_.fresh_bars);
        bearish_signal = config_.fresh_strength * freshness * upper_proximity;
    }

    // Combine signals: bullish pushes above 0.5, bearish pushes below 0.5
    double probability = base_prob + (bullish_signal * 0.3) - (bearish_signal * 0.3);

    return clamp01(probability);
}

double WilliamsRsiStrategy::calculate_confidence(double price_percentile,
                                                bool crossing_up, bool crossing_down,
                                                bool approaching_up, bool approaching_down,
                                                bool fresh_up, bool fresh_down) const {
    double confidence = 0.4;  // Base confidence

    // Crossover strength contributes most
    if (crossing_up || crossing_down) {
        confidence += 0.4;
    } else if (approaching_up || approaching_down) {
        confidence += 0.3;
    } else if (fresh_up || fresh_down) {
        confidence += 0.2;
    }

    // Band proximity adds confidence
    if (price_percentile < config_.lower_band_zone) {
        double proximity = (config_.lower_band_zone - price_percentile) / config_.lower_band_zone;
        confidence += 0.2 * proximity;
    } else if (price_percentile > config_.upper_band_zone) {
        double proximity = (price_percentile - config_.upper_band_zone) / (100.0 - config_.upper_band_zone);
        confidence += 0.2 * proximity;
    }

    return clamp01(confidence);
}

// ===== HELPER FUNCTIONS =====

double WilliamsRsiStrategy::compute_sma(const std::vector<double>& v, int window) const {
    if (window <= 0 || static_cast<int>(v.size()) < window) return 0.0;

    double sum = 0.0;
    for (int i = static_cast<int>(v.size()) - window; i < static_cast<int>(v.size()); ++i) {
        sum += v[static_cast<size_t>(i)];
    }

    return sum / static_cast<double>(window);
}

double WilliamsRsiStrategy::compute_stddev(const std::vector<double>& v, int window, double mean) const {
    if (window <= 0 || static_cast<int>(v.size()) < window) return 0.0;

    double acc = 0.0;
    for (int i = static_cast<int>(v.size()) - window; i < static_cast<int>(v.size()); ++i) {
        double d = v[static_cast<size_t>(i)] - mean;
        acc += d * d;
    }

    return std::sqrt(acc / static_cast<double>(window));
}

} // namespace trading
