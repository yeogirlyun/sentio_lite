#pragma once

#include "core/bar.h"
#include <vector>
#include <string>
#include <chrono>

namespace trading {

/**
 * Williams %R + RSI Anticipatory Crossover Strategy
 *
 * Core Concept:
 * - Bullish: Williams %R crosses up over RSI near Bollinger Lower Band
 * - Bearish: Williams %R crosses down under RSI near Bollinger Upper Band
 *
 * Anticipatory Signal Strength:
 * 1. STRONGEST: Currently crossing (Williams %R crossing RSI at this bar)
 * 2. STRONG: Approaching crossover (converging but not yet crossed)
 * 3. STRONG: Just crossed (1-3 bars after crossover, still fresh)
 * 4. NEUTRAL: No crossover pattern detected
 *
 * Signal strength is proportional to proximity to Bollinger Bands and
 * the stage of the crossover (approaching, crossing, or just-crossed).
 */

struct WilliamsRsiConfig {
    // Indicator periods
    int williams_period = 14;      // Williams %R lookback period
    int rsi_period = 14;            // RSI period (now using correct Wilder's EMA)
    int bb_period = 20;             // Bollinger Bands period
    double bb_stddev = 2.0;         // Bollinger Bands standard deviations

    // Crossover detection
    int approach_threshold = 5;     // Distance threshold for "approaching" (in percentage points)
    int fresh_bars = 3;             // Bars after cross still considered "fresh"

    // Band proximity thresholds (0-100 scale)
    double lower_band_zone = 30.0;  // Price below this percentile = near lower band
    double upper_band_zone = 70.0;  // Price above this percentile = near upper band

    // Signal strength multipliers
    double crossing_strength = 1.0;     // Maximum strength when crossing
    double approaching_strength = 0.7;  // Strength when approaching
    double fresh_strength = 0.7;        // Strength when recently crossed
};

struct WilliamsRsiSignal {
    std::chrono::system_clock::time_point timestamp;
    std::string symbol;

    // Indicator values
    double williams_r;      // Williams %R (-100 to 0)
    double rsi;             // RSI (0 to 100)
    double bb_upper;        // Bollinger Upper Band
    double bb_middle;       // Bollinger Middle Band (SMA)
    double bb_lower;        // Bollinger Lower Band
    double price_percentile; // Current price position in BB (0-100)

    // Crossover state
    bool is_crossing_up;    // Williams %R crossing up over RSI right now
    bool is_crossing_down;  // Williams %R crossing down under RSI right now
    bool is_approaching_up; // Converging towards upward cross
    bool is_approaching_down; // Converging towards downward cross
    bool is_fresh_cross_up;  // Recently crossed up (within fresh_bars)
    bool is_fresh_cross_down; // Recently crossed down (within fresh_bars)

    // Signal output
    double probability;     // 0.0 to 1.0 (0.5 = neutral, >0.5 = bullish, <0.5 = bearish)
    double confidence;      // 0.0 to 1.0 (strength of signal)
    bool is_long;          // Final bullish signal
    bool is_short;         // Final bearish signal
    bool is_neutral;       // No clear signal
};

class WilliamsRsiStrategy {
public:
    explicit WilliamsRsiStrategy(const WilliamsRsiConfig& config = WilliamsRsiConfig());

    /**
     * Generate trading signal from bar data
     * @param bar Current bar
     * @param symbol Symbol being traded
     * @return Williams RSI signal with anticipatory crossover analysis
     */
    WilliamsRsiSignal generate_signal(const Bar& bar, const std::string& symbol);

    /**
     * Check if strategy has enough data (warmup complete)
     */
    bool is_warmed_up() const {
        return bar_count_ >= std::max({config_.williams_period, config_.rsi_period, config_.bb_period}) + config_.fresh_bars;
    }

    /**
     * Reset strategy state
     */
    void reset();

private:
    WilliamsRsiConfig config_;

    // Price history
    std::vector<double> closes_;
    std::vector<double> highs_;
    std::vector<double> lows_;

    // RSI state (Wilder's EMA)
    double avg_gain_ = 0.0;
    double avg_loss_ = 0.0;
    bool rsi_initialized_ = false;

    // Crossover tracking
    std::vector<double> williams_history_;
    std::vector<double> rsi_history_;
    int bars_since_cross_up_ = 999;    // Bars since last upward cross
    int bars_since_cross_down_ = 999;  // Bars since last downward cross

    int bar_count_ = 0;

    // Indicator calculations
    double calculate_williams_r(int period) const;
    double calculate_rsi(int period);  // Fixed Wilder's EMA version
    void calculate_bollinger_bands(int period, double stddev, double& upper, double& middle, double& lower) const;
    double calculate_price_percentile(double price, double lower, double upper) const;

    // Crossover detection
    void detect_crossovers(double williams, double rsi, bool& crossing_up, bool& crossing_down,
                          bool& approaching_up, bool& approaching_down);

    // Signal generation
    double calculate_probability(double williams, double rsi, double price_percentile,
                                 bool crossing_up, bool crossing_down,
                                 bool approaching_up, bool approaching_down,
                                 bool fresh_up, bool fresh_down) const;

    double calculate_confidence(double price_percentile,
                                bool crossing_up, bool crossing_down,
                                bool approaching_up, bool approaching_down,
                                bool fresh_up, bool fresh_down) const;

    // Helpers
    double compute_sma(const std::vector<double>& v, int window) const;
    double compute_stddev(const std::vector<double>& v, int window, double mean) const;
    double clamp01(double x) const { return std::max(0.0, std::min(1.0, x)); }
};

} // namespace trading
