#pragma once
#include "core/types.h"
#include "core/bar.h"
#include "predictor/multi_horizon_predictor.h"
#include "trading/position.h"
#include "trading/trade_history.h"
#include "trading/alpaca_cost_model.h"
#include "trading/trade_filter.h"
#include "trading/trading_strategy.h"
#include "strategy/sigor_strategy.h"
#include "strategy/williams_rsi_strategy.h"
#include "predictor/awr_predictor_adapter.h"
#include "predictor/sigor_predictor_adapter.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <deque>
#include <numeric>

namespace trading {

/**
 * Prediction Data - Stores multi-horizon prediction and associated information
 */
struct PredictionData {
    MultiHorizonPredictor::MultiHorizonPrediction prediction;  // Multi-horizon prediction
    Eigen::VectorXd features;     // Feature vector (33 dimensions)
    Price current_price;          // Current price
};

/**
 * Trading Configuration
 */
struct TradingConfig {
    // Strategy selection
    StrategyType strategy = StrategyType::SIGOR;  // Default SIGOR
    SigorConfig sigor_config;            // SIGOR strategy parameters
    WilliamsRsiConfig awr_config;        // AWR strategy parameters

    double initial_capital = 100000.0;
    size_t max_positions = 3;
    size_t min_bars_to_learn = 50;     // Warmup period
    // Removed: EWRLS lookback window (not used in SIGOR)
    int bars_per_day = 391;            // 9:30 AM - 4:00 PM inclusive (391 bars)
    bool eod_liquidation = true;
    double win_multiplier = 1.3;
    double loss_multiplier = 0.7;
    size_t trade_history_size = 3;     // Track last N trades for adaptive sizing

    // Adaptive minimum prediction threshold
    double min_prediction_for_entry = 0.002;  // Starting threshold (0.2%)
    double min_prediction_increase_on_trade = 0.0005;  // +0.05% per trade
    double min_prediction_decrease_on_no_trade = 0.0001;  // -0.01% per no-trade bar

    // Trade filter settings
    TradeFilter::Config filter_config;

    // Cost model settings
    bool enable_cost_tracking = true;  // Enable Alpaca cost model
    double default_avg_volume = 1000000.0;  // Default average daily volume
    double default_volatility = 0.02;  // Default 2% daily volatility

    // Probability-based trading (from online_trader)
    bool enable_probability_scaling = true;   // Convert predictions to probabilities
    double probability_scaling_factor = 50.0; // Tanh scaling factor
    double buy_threshold = 0.55;              // Probability threshold for entry (more reasonable)
    double sell_threshold = 0.45;             // Probability threshold for exit (symmetric)

    // Rotation strategy configuration (from online_trader)
    bool enable_rotation = true;              // Enable rank-based rotation
    double rotation_strength_delta = 0.01;    // Minimum improvement (100 bps) to rotate - was 0.002 (too aggressive)
    int rotation_cooldown_bars = 10;          // Prevent re-entry after rotation
    double min_rank_strength = 0.001;         // Minimum signal strength (10 bps) to hold

    // Price-based exit configuration (mean reversion completion)
    bool enable_price_based_exits = true;         // Exit when mean reversion completes
    bool exit_on_ma_crossover = true;             // Exit when price crosses back through MA
    double trailing_stop_percentage = 0.50;       // Trail stop at 50% of max profit
    int ma_exit_period = 10;                      // MA period for exit crossover detection

    // ===== PROFIT TARGET & STOP LOSS (from online_trader v2.0 - CRITICAL) =====
    // These create asymmetric risk/reward and lock in profits
    bool enable_profit_target = true;      // Take profit at target %
    double profit_target_pct = 0.03;       // +3% profit target (online_trader value)
    bool enable_stop_loss = true;          // Cut losses at stop %
    double stop_loss_pct = 0.015;          // -1.5% stop loss (online_trader value, 2:1 reward:risk)

    // ===== POSITION SIZING (Kelly Criterion with Volatility Adjustment) =====
    struct PositionSizingConfig {
        double expected_win_pct = 0.02;       // Expected win percentage (2%)
        double expected_loss_pct = 0.015;     // Expected loss percentage (1.5%)
        double fractional_kelly = 0.25;       // Use 25% of full Kelly (conservative)
        double min_position_pct = 0.05;       // Minimum position size (5% of capital)
        double max_position_pct = 0.25;       // Maximum position size (25% of capital)
        bool enable_volatility_adjustment = true;  // Reduce size for volatile symbols
        int volatility_lookback = 20;         // Bars to calculate volatility
        double max_volatility_reduce = 0.5;   // Max reduction factor (50% of base size)
    } position_sizing;

    // Warmup configuration mode
    enum class WarmupMode {
        PRODUCTION,  // Strict criteria - SAFE FOR LIVE TRADING
        TESTING      // Relaxed criteria - DEVELOPMENT/TESTING ONLY
    };

    // Warmup configuration for improved pre-live validation
    struct WarmupConfig {
        bool enabled = true;                     // Enable warmup phase (DEFAULT)
        int observation_days = 1;                // Learn without trading
        int simulation_days = 2;                 // Paper trade before live

        // Configuration mode (CRITICAL: Set to PRODUCTION before live trading!)
        WarmupMode mode = WarmupMode::PRODUCTION;

        // Skip validation for MOCK/backtesting mode (always proceed to test day)
        bool skip_validation = false;            // If true, skip warmup quality checks (for MOCK mode)

        // Go-live criteria (values set based on mode)
        double min_sharpe_ratio;                 // Minimum Sharpe ratio to go live
        double max_drawdown;                     // Maximum drawdown allowed
        int min_trades = 20;                     // Minimum trades to evaluate
        bool require_positive_return;            // Require positive return to go live

        // State preservation
        bool preserve_predictor_state = true;    // Keep EWRLS weights
        bool preserve_trade_history = true;      // Keep trade history for sizing
        double history_decay_factor = 0.7;       // Weight historical trades at 70%

        // Constructor: Initialize based on mode
        WarmupConfig() {
            set_mode(WarmupMode::PRODUCTION);  // DEFAULT TO PRODUCTION (SAFE)
        }

        // Set mode and apply corresponding criteria
        void set_mode(WarmupMode m) {
            mode = m;
            if (mode == WarmupMode::PRODUCTION) {
                // PRODUCTION: Strict criteria - SAFE FOR LIVE TRADING
                min_sharpe_ratio = 0.3;          // Minimum 0.3 Sharpe ratio
                max_drawdown = 0.15;             // Maximum 15% drawdown
                require_positive_return = true;  // Must be profitable
            } else {
                // TESTING: Relaxed criteria - DEVELOPMENT/TESTING ONLY
                min_sharpe_ratio = -2.0;         // Very lenient (allows testing approval logic)
                max_drawdown = 0.30;             // Lenient 30% drawdown
                require_positive_return = false; // Allow negative returns
            }
        }

        // Get mode name for logging
        std::string get_mode_name() const {
            return mode == WarmupMode::PRODUCTION ? "PRODUCTION (STRICT)" : "TESTING (RELAXED)";
        }
    } warmup;

    // Trading phase tracking
    enum Phase {
        WARMUP_OBSERVATION,   // Days 1-2: Learning only
        WARMUP_SIMULATION,    // Days 3-7: Paper trading
        WARMUP_COMPLETE,      // Warmup done, ready for live
        LIVE_TRADING          // Actually trading
    };
    Phase current_phase = LIVE_TRADING;  // Default to live (warmup disabled); SIGOR forces LIVE

    TradingConfig() {
        // Set reasonable defaults for trade filter (SELECTIVE for probability-based trading)
        // INCREASED to reduce churning - signal quality should drive exits, not time
        filter_config.min_bars_to_hold = 20;   // Was 5 - too aggressive
        filter_config.typical_hold_period = 60;  // Was 20
        filter_config.max_bars_to_hold = 120;    // Was 60
        filter_config.min_prediction_for_entry = 0.0;     // Disabled (use probability threshold)
        filter_config.min_confidence_for_entry = 0.0;     // Disabled (use probability threshold)
    }
};

/**
 * Daily results structure for multi-day tracking
 */
struct DailyResults {
    int day_number;             // 1, 2, 3, ...
    double start_equity;        // Equity at start of day
    double end_equity;          // Equity at end of day
    double daily_return;        // Return for this day
    int trades_today;           // Trades completed today
    int winning_trades_today;   // Winning trades today
    int losing_trades_today;    // Losing trades today
};

/**
 * Multi-Symbol Online Trading System
 *
 * Features:
 * - Dynamic position management (max N concurrent positions)
 * - Automatic stop-loss and profit targets
 * - Adaptive position sizing based on recent performance
 * - EOD liquidation option
 * - Rotation strategy (top N by predicted return)
 *
 * Usage:
 *   MultiSymbolTrader trader(symbols, config);
 *   for (each bar) {
 *       trader.on_bar(market_data);
 *   }
 *   auto results = trader.get_results();
 */
class MultiSymbolTrader {
private:
    std::vector<Symbol> symbols_;
    TradingConfig config_;
    double cash_;

    // Exit tracking data for price-based exits
    struct ExitTrackingData {
        double entry_ma = 0.0;           // MA value at entry time
        double max_profit_pct = 0.0;     // Maximum profit % seen
        Price max_profit_price = 0.0;    // Price where max profit occurred
        bool is_long = true;             // Direction of position
    };

    // Per-symbol components (SIGOR only)
    std::unordered_map<Symbol, std::unique_ptr<SigorPredictorAdapter>> sigor_predictors_;

    // Shared components (both strategies)
    std::unordered_map<Symbol, PositionWithCosts> positions_;
    std::unordered_map<Symbol, ExitTrackingData> exit_tracking_;  // Price-based exit tracking
    std::unordered_map<Symbol, std::unique_ptr<TradeHistory>> trade_history_;
    std::unordered_map<Symbol, MarketContext> market_context_;  // Market microstructure data

    // Multi-horizon return tracking for predictor updates
    std::unordered_map<Symbol, std::deque<double>> price_history_;  // Track for multi-bar returns

    // Trade filtering and frequency management
    std::unique_ptr<TradeFilter> trade_filter_;

    // Complete trade log for export (not circular, keeps all trades)
    std::vector<TradeRecord> all_trades_log_;

    size_t bars_seen_;       // Total bars including warmup
    size_t trading_bars_;    // Trading bars only (excludes warmup) - used for EOD timing
    size_t test_day_start_bar_;  // Bar index where test day begins (for filtering test-only metrics)
    int total_trades_;
    double total_transaction_costs_;  // Track cumulative costs

    // Daily tracking (for multi-day testing)
    std::vector<DailyResults> daily_results_;
    double daily_start_equity_;       // Equity at start of current day
    int daily_start_trades_;          // Trades at start of current day
    int daily_winning_trades_;        // Winning trades today
    int daily_losing_trades_;         // Losing trades today

    // Warmup phase tracking
    struct SimulationMetrics {
        std::vector<TradeRecord> simulated_trades;
        double starting_equity = 0.0;
        double current_equity = 0.0;
        double max_equity = 0.0;
        double max_drawdown = 0.0;
        int observation_bars_complete = 0;
        int simulation_bars_complete = 0;

        void update_drawdown() {
            max_equity = std::max(max_equity, current_equity);
            double drawdown = (max_equity > 0) ?
                (max_equity - current_equity) / max_equity : 0.0;
            max_drawdown = std::max(max_drawdown, drawdown);
        }

        double calculate_sharpe() const {
            if (simulated_trades.size() < 2) return 0.0;

            std::vector<double> returns;
            for (const auto& trade : simulated_trades) {
                returns.push_back(trade.pnl_pct);
            }

            double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
            double sq_sum = std::inner_product(returns.begin(), returns.end(), returns.begin(), 0.0);
            double stdev = std::sqrt(sq_sum / returns.size() - mean * mean);

            return (stdev > 0) ? (mean / stdev) * std::sqrt(252) : 0.0; // Annualized
        }
    };

    SimulationMetrics warmup_metrics_;

    // Rotation tracking (from online_trader)
    std::unordered_map<Symbol, int> rotation_cooldowns_;  // Bars until can re-enter after rotation

    // Phase management methods
    void update_phase();
    void handle_observation_phase(const std::unordered_map<Symbol, Bar>& market_data);
    void handle_simulation_phase(const std::unordered_map<Symbol, PredictionData>& predictions,
                                const std::unordered_map<Symbol, Bar>& market_data);
    void handle_live_phase(const std::unordered_map<Symbol, PredictionData>& predictions,
                          const std::unordered_map<Symbol, Bar>& market_data);
    bool evaluate_warmup_complete();
    void print_warmup_summary();

public:
    /**
     * Constructor
     * @param symbols List of symbols to trade
     * @param config Trading configuration (optional, uses defaults if not provided)
     */
    explicit MultiSymbolTrader(const std::vector<Symbol>& symbols,
                              const TradingConfig& config = TradingConfig());

    /**
     * Process new market data bar
     * @param market_data Map of symbol -> bar for current timestamp
     *
     * Steps:
     * 1. Extract features and make predictions for each symbol
     * 2. Update predictors with realized returns
     * 3. Update existing positions (check stop-loss/profit targets)
     * 4. Make trading decisions (rotation to top N predicted symbols)
     * 5. EOD liquidation if enabled
     */
    void on_bar(const std::unordered_map<Symbol, Bar>& market_data);

    /**
     * Get current equity (cash + position values)
     */
    double get_equity(const std::unordered_map<Symbol, Bar>& market_data) const;

    /**
     * Backtest results structure
     */
    struct BacktestResults {
        double total_return;        // Total return as fraction
        double mrd;                 // Mean Return per Day
        double final_equity;        // Final equity value
        int total_trades;           // Number of completed trades
        int winning_trades;         // Number of profitable trades
        int losing_trades;          // Number of losing trades
        double win_rate;            // Fraction of winning trades
        double avg_win;             // Average win amount
        double avg_loss;            // Average loss amount
        double profit_factor;       // Gross profit / gross loss
        double max_drawdown;        // Maximum drawdown (not yet implemented)

        // Cost tracking
        double total_transaction_costs;  // Sum of all transaction costs
        double avg_cost_per_trade;       // Average cost per trade
        double cost_as_pct_of_volume;    // Costs as % of total volume traded
        double net_return_after_costs;   // Return after accounting for costs

        // Daily breakdown (for multi-day testing)
        std::vector<DailyResults> daily_breakdown;
    };

    /**
     * Get backtest results
     */
    BacktestResults get_results() const;

    /**
     * Get current positions (for monitoring)
     */
    const std::unordered_map<Symbol, PositionWithCosts>& positions() const { return positions_; }

    /**
     * Get current cash
     */
    double cash() const { return cash_; }

    /**
     * Get configuration
     */
    const TradingConfig& config() const { return config_; }

    /**
     * Get all trades (for export)
     */
    std::vector<TradeRecord> get_all_trades() const {
        return all_trades_log_;
    }

private:
    /**
     * Make trading decisions based on predictions
     */
    void make_trades(const std::unordered_map<Symbol, PredictionData>& predictions,
                    const std::unordered_map<Symbol, Bar>& market_data);

    /**
     * Update existing positions (check exit conditions with trade filter)
     */
    void update_positions(const std::unordered_map<Symbol, Bar>& market_data,
                         const std::unordered_map<Symbol, PredictionData>& predictions);

    /**
     * Calculate position size for a symbol using Kelly Criterion and adaptive sizing
     */
    double calculate_position_size(const Symbol& symbol, const PredictionData& pred_data);

    /**
     * Enter new position
     */
    void enter_position(const Symbol& symbol, Price price, Timestamp time, double capital, uint64_t bar_id);

    /**
     * Check if a new position is compatible with existing positions
     * (prevents inverse/contradictory positions like TQQQ + SQQQ)
     */
    bool is_position_compatible(const Symbol& new_symbol) const;

    /**
     * Exit existing position
     */
    double exit_position(const Symbol& symbol, Price price, Timestamp time, uint64_t bar_id);

    /**
     * Liquidate all positions
     */
    void liquidate_all(const std::unordered_map<Symbol, Bar>& market_data, const std::string& reason);

    /**
     * Update market context for cost calculations
     */
    void update_market_context(const Symbol& symbol, const Bar& bar);

    /**
     * Calculate minutes from market open (9:30 AM ET)
     */
    int calculate_minutes_from_open(Timestamp ts) const;

    /**
     * Convert prediction to probability using tanh scaling (from online_trader)
     */
    double prediction_to_probability(double prediction) const;

    /**
     * Apply Bollinger Band amplification to probability
     */
    double apply_bb_amplification(double probability, const Symbol& symbol,
                                  const Bar& bar, bool is_long) const;

    /**
     * Calculate Bollinger Bands for a symbol
     */
    struct BBands {
        double middle = 0.0;
        double upper = 0.0;
        double lower = 0.0;
    };
    BBands calculate_bollinger_bands(const Symbol& symbol) const;

    /**
     * Check signal confirmation using multiple indicators (expert recommendation)
     * @param symbol Symbol to check
     * @param bar Current bar
     * @param features Feature vector containing RSI, volume, etc.
     * @param is_long True for long signals, false for short signals
     * @return Number of confirmations (0-3: RSI, BB, Volume)
     */
    int check_signal_confirmations(const Symbol& symbol, const Bar& bar,
                                   const Eigen::VectorXd& features, bool is_long) const;

    /**
     * Calculate moving average for exit detection
     * @param symbol Symbol to calculate MA for
     * @return MA value, or 0.0 if insufficient data
     */
    double calculate_exit_ma(const Symbol& symbol) const;

    /**
     * Check if position should exit based on price-based logic
     * @param symbol Symbol to check
     * @param current_price Current price
     * @param exit_reason Output parameter for exit reason
     * @return True if should exit
     */
    bool should_exit_on_price(const Symbol& symbol, Price current_price, std::string& exit_reason);

    /**
     * Find weakest current position for rotation (from online_trader)
     * Returns symbol with lowest signal strength, or empty string if no positions
     */
    Symbol find_weakest_position(const std::unordered_map<Symbol, PredictionData>& predictions) const;

    /**
     * Update rotation cooldowns (decrement each bar)
     */
    void update_rotation_cooldowns();

    /**
     * Check if symbol is in rotation cooldown
     */
    bool in_rotation_cooldown(const Symbol& symbol) const;
};

} // namespace trading
