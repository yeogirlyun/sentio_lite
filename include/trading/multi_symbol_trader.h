#pragma once
#include "core/types.h"
#include "core/bar.h"
#include "predictor/multi_horizon_predictor.h"
#include "predictor/feature_extractor.h"
#include "trading/position.h"
#include "trading/trade_history.h"
#include "trading/alpaca_cost_model.h"
#include "trading/trade_filter.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <deque>

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
    double initial_capital = 100000.0;
    size_t max_positions = 3;
    double stop_loss_pct = -0.02;      // -2%
    double profit_target_pct = 0.05;   // 5%
    size_t min_bars_to_learn = 50;     // Warmup period
    size_t lookback_window = 50;
    int bars_per_day = 391;            // 9:30 AM - 4:00 PM inclusive (391 bars)
    bool eod_liquidation = true;
    double win_multiplier = 1.3;
    double loss_multiplier = 0.7;
    size_t trade_history_size = 3;     // Track last N trades for adaptive sizing

    // Multi-horizon prediction settings
    MultiHorizonPredictor::Config horizon_config;

    // Trade filter settings
    TradeFilter::Config filter_config;

    // Cost model settings
    bool enable_cost_tracking = true;  // Enable Alpaca cost model
    double default_avg_volume = 1000000.0;  // Default average daily volume
    double default_volatility = 0.02;  // Default 2% daily volatility

    // Probability-based trading (from online_trader)
    bool enable_probability_scaling = true;   // Convert predictions to probabilities
    double probability_scaling_factor = 50.0; // Tanh scaling factor
    double buy_threshold = 0.60;              // Probability threshold for entry (increased for selectivity)
    double sell_threshold = 0.40;             // Probability threshold for exit (wider spread)

    // Bollinger Band amplification (from online_trader)
    bool enable_bb_amplification = true;      // Boost signals near BB bands
    int bb_period = 20;                       // BB period
    double bb_std_dev = 2.0;                  // BB standard deviations
    double bb_proximity_threshold = 0.30;     // Within 30% of band for boost
    double bb_amplification_factor = 0.10;    // Boost probability by this much

    TradingConfig() {
        // Set reasonable defaults for multi-horizon (MORE ADAPTIVE)
        horizon_config.lambda_1bar = 0.98;   // Was 0.99 - faster adaptation
        horizon_config.lambda_5bar = 0.99;   // Was 0.995 - faster adaptation
        horizon_config.lambda_10bar = 0.995; // Was 0.998 - faster adaptation
        horizon_config.min_confidence = 0.5;  // Was 0.6 - lower threshold

        // Set reasonable defaults for trade filter (SELECTIVE for probability-based trading)
        filter_config.min_bars_to_hold = 10;  // Increased: predictions need time to realize
        filter_config.typical_hold_period = 20;
        filter_config.max_bars_to_hold = 60;
        filter_config.min_prediction_for_entry = 0.0;     // Disabled (use probability threshold)
        filter_config.min_confidence_for_entry = 0.0;     // Disabled (use probability threshold)
    }
};

/**
 * Multi-Symbol Online Trading System
 *
 * Features:
 * - Online learning per symbol (EWRLS with 25 features)
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

    // Per-symbol components
    std::unordered_map<Symbol, std::unique_ptr<MultiHorizonPredictor>> predictors_;
    std::unordered_map<Symbol, std::unique_ptr<FeatureExtractor>> extractors_;
    std::unordered_map<Symbol, PositionWithCosts> positions_;
    std::unordered_map<Symbol, std::unique_ptr<TradeHistory>> trade_history_;
    std::unordered_map<Symbol, MarketContext> market_context_;  // Market microstructure data

    // Multi-horizon return tracking for predictor updates
    std::unordered_map<Symbol, std::deque<double>> price_history_;  // Track for multi-bar returns

    // Trade filtering and frequency management
    std::unique_ptr<TradeFilter> trade_filter_;

    // Complete trade log for export (not circular, keeps all trades)
    std::vector<TradeRecord> all_trades_log_;

    size_t bars_seen_;
    int total_trades_;
    double total_transaction_costs_;  // Track cumulative costs

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
    BBands calculate_bollinger_bands(const Symbol& symbol, const Bar& current_bar) const;
};

} // namespace trading
