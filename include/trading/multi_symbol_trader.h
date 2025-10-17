#pragma once
#include "core/types.h"
#include "core/bar.h"
#include "predictor/online_predictor.h"
#include "predictor/feature_extractor.h"
#include "trading/position.h"
#include "trading/trade_history.h"
#include <unordered_map>
#include <memory>
#include <vector>

namespace trading {

/**
 * Prediction Data - Stores prediction and associated information
 */
struct PredictionData {
    double predicted_return;      // Predicted next-bar return
    Eigen::VectorXd features;     // Feature vector (25 dimensions)
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
    size_t min_bars_to_learn = 50;     // CHANGED: Reduced from 100
    size_t lookback_window = 50;
    double lambda = 0.98;
    int bars_per_day = 390;
    bool eod_liquidation = true;
    double win_multiplier = 1.3;
    double loss_multiplier = 0.7;
    size_t trade_history_size = 3;     // Track last N trades for adaptive sizing
    double min_prediction_threshold = 0.0001;  // CHANGED: Reduced from 0.001 (0.01% per bar)
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
    std::unordered_map<Symbol, std::unique_ptr<OnlinePredictor>> predictors_;
    std::unordered_map<Symbol, std::unique_ptr<FeatureExtractor>> extractors_;
    std::unordered_map<Symbol, Position> positions_;
    std::unordered_map<Symbol, std::unique_ptr<TradeHistory>> trade_history_;

    // Complete trade log for export (not circular, keeps all trades)
    std::vector<TradeRecord> all_trades_log_;

    size_t bars_seen_;
    int total_trades_;

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
    };

    /**
     * Get backtest results
     */
    BacktestResults get_results() const;

    /**
     * Get current positions (for monitoring)
     */
    const std::unordered_map<Symbol, Position>& positions() const { return positions_; }

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
     * Update existing positions (check stop-loss/profit targets)
     */
    void update_positions(const std::unordered_map<Symbol, Bar>& market_data);

    /**
     * Calculate position size for a symbol using adaptive sizing
     */
    double calculate_position_size(const Symbol& symbol);

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
};

} // namespace trading
