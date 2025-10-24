#pragma once

#include "core/bar.h"
#include <vector>
#include <cstdint>
#include <string>

namespace trading {

/**
 * Sigor Configuration - 7-Detector Ensemble
 */
struct SigorConfig {
    // Fusion parameters
    double k = 1.5;  // Sharpness in log-odds fusion

    // Detector weights (reliability)
    double w_boll = 1.0;   // Bollinger Bands
    double w_rsi  = 1.0;   // RSI(14)
    double w_mom  = 1.0;   // Momentum
    double w_vwap = 1.0;   // VWAP reversion
    double w_orb  = 0.5;   // Opening Range Breakout
    double w_ofi  = 0.5;   // Order Flow Imbalance proxy
    double w_vol  = 0.5;   // Volume surge

    // Window parameters
    int win_boll = 20;
    int win_rsi  = 14;
    int win_mom  = 10;
    int win_vwap = 20;
    int orb_opening_bars = 30;
    int vol_window = 20;

    // Warmup period
    int warmup_bars = 50;
};

/**
 * Sigor Strategy Signal Output
 */
struct SigorSignal {
    Timestamp timestamp;
    std::string symbol;
    double probability;      // 0..1 (0.5 = neutral)
    double confidence;       // 0..1 detector agreement
    bool is_long;           // true if probability > 0.5
    bool is_short;          // true if probability < 0.5
    bool is_neutral;        // true if near 0.5

    // Detector breakdown (for debugging)
    double prob_boll;
    double prob_rsi;
    double prob_mom;
    double prob_vwap;
    double prob_orb;
    double prob_ofi;
    double prob_vol;
};

/**
 * SigorStrategy - Rule-Based Ensemble (Signal-OR)
 *
 * Pure technical strategy that combines 7 detectors:
 * 1. Bollinger Z-Score (mean reversion)
 * 2. RSI(14)
 * 3. Momentum (10-bar)
 * 4. VWAP reversion
 * 5. Opening Range Breakout (daily)
 * 6. Order Flow Imbalance proxy
 * 7. Volume surge scaled by momentum
 *
 * Aggregation: Log-odds fusion with weighted voting
 * Target: 0.3% MRD (stable, consistent)
 */
class SigorStrategy {
public:
    explicit SigorStrategy(const SigorConfig& config = SigorConfig());

    /**
     * Generate signal from new bar
     */
    SigorSignal generate_signal(const Bar& bar, const std::string& symbol);

    /**
     * Check if warmup period is complete
     */
    bool is_warmed_up() const { return bar_count_ >= config_.warmup_bars; }

    /**
     * Reset strategy state
     */
    void reset();

private:
    SigorConfig config_;

    // Price/volume history
    std::vector<double> closes_;
    std::vector<double> highs_;
    std::vector<double> lows_;
    std::vector<double> volumes_;
    std::vector<int64_t> timestamps_;
    std::vector<double> gains_;    // For RSI
    std::vector<double> losses_;   // For RSI

    int bar_count_ = 0;

    // ===== 7 Detector Probability Functions =====
    double prob_bollinger_(const Bar& bar) const;
    double prob_rsi_14_() const;
    double prob_momentum_(int window, double scale) const;
    double prob_vwap_reversion_(int window) const;
    double prob_orb_daily_(int opening_window_bars) const;
    double prob_ofi_proxy_(const Bar& bar) const;
    double prob_volume_surge_scaled_(int window) const;

    // ===== Aggregation Functions =====
    double aggregate_probability(double p1, double p2, double p3,
                                 double p4, double p5, double p6, double p7) const;
    double calculate_confidence(double p1, double p2, double p3,
                               double p4, double p5, double p6, double p7) const;

    // ===== Helper Functions =====
    double compute_sma(const std::vector<double>& v, int window) const;
    double compute_stddev(const std::vector<double>& v, int window, double mean) const;
    double compute_rsi(int window) const;
    double clamp01(double v) const { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
};

} // namespace trading
