#pragma once
#include "core/types.h"
#include "predictor/multi_horizon_predictor.h"
#include <string>
#include <unordered_map>
#include <deque>

namespace trading {

/**
 * Trade Filter - Manages trade frequency and holding periods
 *
 * Prevents over-trading and enforces minimum holding periods to:
 * - Reduce transaction costs
 * - Improve signal quality (avoid whipsaws)
 * - Control risk exposure
 * - Manage trade frequency within reasonable bounds
 *
 * Key features:
 * - Per-symbol minimum holding periods
 * - Global and per-symbol trade frequency limits
 * - Dynamic exit logic based on signal quality
 * - Track bars held and position history
 */
class TradeFilter {
public:
    /**
     * Configuration for trade filtering
     */
    struct Config {
        // Holding period constraints
        int min_bars_to_hold;           // Minimum hold period (prevent whipsaws)
        int typical_hold_period;        // Expected typical hold period
        int max_bars_to_hold;          // Maximum hold period (force exit)

        // Re-entry constraints
        int min_bars_between_entries;    // Cooldown after exit

        // Global frequency limits
        int max_trades_per_hour;        // Limit trading frequency
        int max_trades_per_day;        // Daily trade limit

        // Entry quality thresholds
        double min_prediction_for_entry;   // 20 bps minimum
        double min_confidence_for_entry;     // 60% confidence

        // Exit thresholds
        double exit_signal_reversed_threshold;  // Exit if signal reverses to -5bps
        double exit_confidence_threshold;            // Exit if confidence drops below 40%
        double profit_target_multiple;               // Exit at 2x expected return

        // Emergency stop loss (overrides min hold)
        double emergency_stop_loss_pct;    // -1% stop loss

        Config()
            : min_bars_to_hold(10)               // Increased to 10 bars (predictions need time to realize)
            , typical_hold_period(20)
            , max_bars_to_hold(60)
            , min_bars_between_entries(5)        // Cooldown to prevent rapid churn
            , max_trades_per_hour(50)            // Reasonable limit
            , max_trades_per_day(200)            // Reasonable limit
            , min_prediction_for_entry(0.0005)   // 5 bps (was 20 bps)
            , min_confidence_for_entry(0.5)      // 50% (was 60%)
            , exit_signal_reversed_threshold(-0.0005)
            , exit_confidence_threshold(0.4)
            , profit_target_multiple(2.0)
            , emergency_stop_loss_pct(-0.01) {}
    };

    /**
     * Position state tracking
     */
    struct PositionState {
        bool has_position = false;
        int entry_bar = 0;
        int bars_held = 0;
        double entry_prediction = 0.0;      // Initial prediction at entry
        double entry_price = 0.0;
        int last_exit_bar = -999;          // Last time we exited

        void reset() {
            has_position = false;
            entry_bar = 0;
            bars_held = 0;
            entry_prediction = 0.0;
            entry_price = 0.0;
        }
    };

    /**
     * Constructor
     * @param config Configuration parameters
     */
    explicit TradeFilter(const Config& config = Config());

    /**
     * Check if can enter new position
     * @param symbol Symbol identifier
     * @param current_bar Current bar number
     * @param prediction Multi-horizon prediction with quality metrics
     * @return True if entry is allowed
     */
    bool can_enter_position(const Symbol& symbol,
                           int current_bar,
                           const MultiHorizonPredictor::MultiHorizonPrediction& prediction);

    /**
     * Check if should exit position
     * @param symbol Symbol identifier
     * @param current_bar Current bar number
     * @param prediction Current multi-horizon prediction
     * @param current_price Current price
     * @return True if exit is recommended
     */
    bool should_exit_position(const Symbol& symbol,
                             int current_bar,
                             const MultiHorizonPredictor::MultiHorizonPrediction& prediction,
                             double current_price);

    /**
     * Record position entry
     * @param symbol Symbol identifier
     * @param entry_bar Bar number of entry
     * @param entry_prediction Initial prediction at entry
     * @param entry_price Entry price
     */
    void record_entry(const Symbol& symbol, int entry_bar,
                     double entry_prediction, double entry_price);

    /**
     * Record position exit
     * @param symbol Symbol identifier
     * @param exit_bar Bar number of exit
     */
    void record_exit(const Symbol& symbol, int exit_bar);

    /**
     * Update bars held counter
     * Called at each bar to track position duration
     */
    void update_bars_held(int current_bar);

    /**
     * Get position state for symbol
     */
    const PositionState& get_position_state(const Symbol& symbol) const;

    /**
     * Check if symbol has active position
     */
    bool has_position(const Symbol& symbol) const;

    /**
     * Get bars held for symbol
     */
    int get_bars_held(const Symbol& symbol) const;

    /**
     * Get configuration
     */
    const Config& config() const { return config_; }

    /**
     * Get trade statistics
     */
    struct TradeStats {
        int total_entries = 0;
        int total_exits = 0;
        int trades_last_hour = 0;
        int trades_today = 0;
    };

    TradeStats get_trade_stats(int current_bar) const;

private:
    Config config_;
    std::unordered_map<Symbol, PositionState> position_states_;

    // Trade history for frequency management
    std::deque<int> trade_bars_;    // Bar numbers when trades occurred
    int last_day_reset_ = 0;        // Last bar when daily counter was reset

    /**
     * Count recent trades in time window
     */
    int count_recent_trades(int current_bar, int window_bars) const;

    /**
     * Check if trade frequency limits are respected
     */
    bool check_frequency_limits(int current_bar) const;

    /**
     * Calculate current P&L percentage
     */
    double calculate_pnl_pct(const Symbol& symbol, double current_price) const;
};

} // namespace trading
