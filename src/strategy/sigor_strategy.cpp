#include "strategy/sigor_strategy.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <chrono>

namespace trading {

SigorStrategy::SigorStrategy(const SigorConfig& config)
    : config_(config) {}

SigorSignal SigorStrategy::generate_signal(const Bar& bar, const std::string& symbol) {
    // Update history
    closes_.push_back(bar.close);
    highs_.push_back(bar.high);
    lows_.push_back(bar.low);
    volumes_.push_back(static_cast<double>(bar.volume));

    // Convert Timestamp to milliseconds since epoch
    auto duration = bar.timestamp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    timestamps_.push_back(millis);

    // Update gains/losses for RSI
    if (closes_.size() > 1) {
        double delta = closes_.back() - closes_[closes_.size() - 2];
        gains_.push_back(std::max(0.0, delta));
        losses_.push_back(std::max(0.0, -delta));
    } else {
        gains_.push_back(0.0);
        losses_.push_back(0.0);
    }

    bar_count_++;

    // Keep buffers bounded (2048 bars max)
    const size_t max_history = 2048;
    auto trim = [max_history](auto& vec) {
        if (vec.size() > max_history) {
            vec.erase(vec.begin(), vec.begin() + (vec.size() - max_history));
        }
    };
    trim(closes_);
    trim(highs_);
    trim(lows_);
    trim(volumes_);
    trim(timestamps_);
    trim(gains_);
    trim(losses_);

    // Compute detector probabilities
    double p1 = prob_bollinger_(bar);
    double p2 = prob_rsi_14_();
    double p3 = prob_momentum_(config_.win_mom, 50.0);
    double p4 = prob_vwap_reversion_(config_.win_vwap);
    double p5 = prob_orb_daily_(config_.orb_opening_bars);
    double p6 = prob_ofi_proxy_(bar);
    double p7 = prob_volume_surge_scaled_(config_.vol_window);

    // Aggregate probabilities
    double p_final = aggregate_probability(p1, p2, p3, p4, p5, p6, p7);
    double c_final = calculate_confidence(p1, p2, p3, p4, p5, p6, p7);

    // Create signal
    SigorSignal signal;
    signal.timestamp = bar.timestamp;
    signal.symbol = symbol;
    signal.probability = p_final;
    signal.confidence = c_final;
    signal.is_long = p_final > 0.52;      // Slight threshold above 0.5
    signal.is_short = p_final < 0.48;     // Slight threshold below 0.5
    signal.is_neutral = !signal.is_long && !signal.is_short;

    // Store detector breakdown
    signal.prob_boll = p1;
    signal.prob_rsi = p2;
    signal.prob_mom = p3;
    signal.prob_vwap = p4;
    signal.prob_orb = p5;
    signal.prob_ofi = p6;
    signal.prob_vol = p7;

    return signal;
}

void SigorStrategy::reset() {
    closes_.clear();
    highs_.clear();
    lows_.clear();
    volumes_.clear();
    timestamps_.clear();
    gains_.clear();
    losses_.clear();
    bar_count_ = 0;
}

// ===== DETECTOR IMPLEMENTATIONS =====

double SigorStrategy::prob_bollinger_(const Bar& bar) const {
    const int w = config_.win_boll;
    if (static_cast<int>(closes_.size()) < w) return 0.5;

    double mean = compute_sma(closes_, w);
    double sd = compute_stddev(closes_, w, mean);

    if (sd <= 1e-12) return 0.5;

    double z = (bar.close - mean) / sd;
    return clamp01(0.5 + 0.5 * std::tanh(z / 2.0));
}

double SigorStrategy::prob_rsi_14_() const {
    const int w = config_.win_rsi;
    if (static_cast<int>(gains_.size()) < w + 1) return 0.5;

    double rsi = compute_rsi(w); // 0..100
    return clamp01((rsi - 50.0) / 100.0 * 1.0 + 0.5);
}

double SigorStrategy::prob_momentum_(int window, double scale) const {
    if (window <= 0 || static_cast<int>(closes_.size()) <= window) return 0.5;

    double curr = closes_.back();
    double prev = closes_[closes_.size() - static_cast<size_t>(window) - 1];

    if (prev <= 1e-12) return 0.5;

    double ret = (curr - prev) / prev;
    return clamp01(0.5 + 0.5 * std::tanh(ret * scale));
}

double SigorStrategy::prob_vwap_reversion_(int window) const {
    if (window <= 0 || static_cast<int>(closes_.size()) < window) return 0.5;

    double num = 0.0, den = 0.0;
    for (int i = static_cast<int>(closes_.size()) - window; i < static_cast<int>(closes_.size()); ++i) {
        double tp = (highs_[i] + lows_[i] + closes_[i]) / 3.0;
        double v = volumes_[i];
        num += tp * v;
        den += v;
    }

    if (den <= 1e-12) return 0.5;

    double vwap = num / den;
    double z = (closes_.back() - vwap) / std::max(1e-8, std::fabs(vwap));

    // Above VWAP -> mean-revert bias (probability < 0.5)
    return clamp01(0.5 - 0.5 * std::tanh(z));
}

double SigorStrategy::prob_orb_daily_(int opening_window_bars) const {
    if (timestamps_.empty()) return 0.5;

    // Compute day bucket from epoch milliseconds to days
    int64_t day = timestamps_.back() / 86400000LL;

    // Find start index of current day
    int start = static_cast<int>(timestamps_.size()) - 1;
    while (start > 0 && (timestamps_[static_cast<size_t>(start - 1)] / 86400000LL) == day) {
        --start;
    }

    int end_open = std::min(static_cast<int>(timestamps_.size()), start + opening_window_bars);

    double hi = -std::numeric_limits<double>::infinity();
    double lo =  std::numeric_limits<double>::infinity();

    for (int i = start; i < end_open; ++i) {
        hi = std::max(hi, highs_[static_cast<size_t>(i)]);
        lo = std::min(lo, lows_[static_cast<size_t>(i)]);
    }

    if (!std::isfinite(hi) || !std::isfinite(lo)) return 0.5;

    double c = closes_.back();

    if (c > hi) return 0.7;  // Breakout long bias
    if (c < lo) return 0.3;  // Breakout short bias
    return 0.5;               // Inside range
}

double SigorStrategy::prob_ofi_proxy_(const Bar& bar) const {
    // Proxy OFI using bar geometry: (close-open)/(high-low) weighted by volume
    double range = std::max(1e-8, bar.high - bar.low);
    double ofi = ((bar.close - bar.open) / range) * std::tanh(static_cast<double>(bar.volume) / 1e6);
    return clamp01(0.5 + 0.25 * ofi);
}

double SigorStrategy::prob_volume_surge_scaled_(int window) const {
    if (window <= 0 || static_cast<int>(volumes_.size()) < window) return 0.5;

    double v_now = volumes_.back();
    double v_ma = compute_sma(volumes_, window);

    if (v_ma <= 1e-12) return 0.5;

    double ratio = v_now / v_ma; // >1 indicates surge
    double adj = std::tanh((ratio - 1.0) * 1.0); // [-1,1]

    // Scale towards current momentum side
    double p_m = prob_momentum_(10, 50.0);
    double dir = (p_m >= 0.5) ? 1.0 : -1.0;

    return clamp01(0.5 + 0.25 * adj * dir);
}

// ===== AGGREGATION FUNCTIONS =====

double SigorStrategy::aggregate_probability(double p1, double p2, double p3,
                                           double p4, double p5, double p6, double p7) const {
    // Log-odds fusion with weights and sharpness k
    const double probs[7] = {p1, p2, p3, p4, p5, p6, p7};
    const double ws[7] = {
        config_.w_boll, config_.w_rsi, config_.w_mom,
        config_.w_vwap, config_.w_orb, config_.w_ofi, config_.w_vol
    };

    double num = 0.0, den = 0.0;
    for (int i = 0; i < 7; ++i) {
        double p = std::clamp(probs[i], 1e-6, 1.0 - 1e-6);
        double l = std::log(p / (1.0 - p));
        num += ws[i] * l;
        den += ws[i];
    }

    double L = (den > 1e-12) ? (num / den) : 0.0;
    double k = config_.k;
    double P = 1.0 / (1.0 + std::exp(-k * L));

    return P;
}

double SigorStrategy::calculate_confidence(double p1, double p2, double p3,
                                          double p4, double p5, double p6, double p7) const {
    double arr[7] = {p1, p2, p3, p4, p5, p6, p7};

    int long_votes = 0, short_votes = 0;
    double max_strength = 0.0;

    for (double p : arr) {
        if (p > 0.5) ++long_votes;
        else if (p < 0.5) ++short_votes;
        max_strength = std::max(max_strength, std::fabs(p - 0.5));
    }

    double agreement = std::max(long_votes, short_votes) / 7.0; // 0..1
    return clamp01(0.4 + 0.6 * std::max(agreement, max_strength));
}

// ===== HELPER FUNCTIONS =====

double SigorStrategy::compute_sma(const std::vector<double>& v, int window) const {
    if (window <= 0 || static_cast<int>(v.size()) < window) return 0.0;

    double sum = 0.0;
    for (int i = static_cast<int>(v.size()) - window; i < static_cast<int>(v.size()); ++i) {
        sum += v[static_cast<size_t>(i)];
    }

    return sum / static_cast<double>(window);
}

double SigorStrategy::compute_stddev(const std::vector<double>& v, int window, double mean) const {
    if (window <= 0 || static_cast<int>(v.size()) < window) return 0.0;

    double acc = 0.0;
    for (int i = static_cast<int>(v.size()) - window; i < static_cast<int>(v.size()); ++i) {
        double d = v[static_cast<size_t>(i)] - mean;
        acc += d * d;
    }

    return std::sqrt(acc / static_cast<double>(window));
}

double SigorStrategy::compute_rsi(int window) const {
    if (window <= 0 || static_cast<int>(gains_.size()) < window + 1) return 50.0;

    double avg_gain = 0.0, avg_loss = 0.0;
    for (int i = static_cast<int>(gains_.size()) - window; i < static_cast<int>(gains_.size()); ++i) {
        avg_gain += gains_[static_cast<size_t>(i)];
        avg_loss += losses_[static_cast<size_t>(i)];
    }

    avg_gain /= static_cast<double>(window);
    avg_loss /= static_cast<double>(window);

    if (avg_loss <= 1e-12) return 100.0;

    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

} // namespace trading
