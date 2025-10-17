#pragma once
#include "core/types.h"
#include "trading/position.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <string>

namespace trading {

/**
 * Alpaca Transaction Cost Model
 *
 * Comprehensive cost modeling for Alpaca trading including:
 * - Regulatory fees (SEC, FINRA TAF) on sell orders
 * - Zero commission structure
 * - Market impact based on order size and liquidity
 * - Slippage modeling with volatility and time-of-day adjustments
 * - Short selling borrow costs
 *
 * Cost Components:
 * 1. SEC Fee: $27.80 per $1M notional (sell only)
 * 2. FINRA TAF: $0.000145 per share, capped at $7.27 (sell only)
 * 3. Commission: $0 (Alpaca is zero-commission)
 * 4. Slippage: Based on volatility, order size, and time of day
 * 5. Market Impact: Square-root model for temporary and permanent impact
 * 6. Borrow Costs: For short positions (varies by symbol)
 */
class AlpacaCostModel {
public:
    /**
     * Fee Constants (as of 2024)
     */
    struct Fees {
        // SEC fee applied to sell orders only
        static constexpr double SEC_FEE_RATE = 0.0000278;  // $27.80 per $1M notional

        // FINRA Trading Activity Fee (TAF) applied to sell orders only
        static constexpr double FINRA_TAF = 0.000145;      // $0.000145 per share
        static constexpr double FINRA_TAF_MAX = 7.27;      // Maximum TAF per trade

        // Alpaca commission
        static constexpr double COMMISSION = 0.0;          // Zero commission

        // Short selling default borrow rate (annualized)
        static constexpr double DEFAULT_BORROW_RATE = 0.005;  // 0.5%
        static constexpr double HARD_TO_BORROW_RATE = 0.01;   // 1% for HTB stocks
    };

    /**
     * Slippage Model Parameters
     */
    struct SlippageModel {
        double base_slippage_bps = 1.0;         // Base slippage for liquid stocks (basis points)
        double size_impact_factor = 0.5;         // Additional bps per 1% of ADV
        double volatility_multiplier = 1.5;      // Multiplier based on volatility
        double time_of_day_factor = 1.0;         // Increase at market open/close

        SlippageModel() = default;

        SlippageModel(double base, double size_factor, double vol_mult, double time_factor)
            : base_slippage_bps(base), size_impact_factor(size_factor),
              volatility_multiplier(vol_mult), time_of_day_factor(time_factor) {}
    };

    /**
     * Breakdown of all transaction costs
     */
    struct TradeCosts {
        double sec_fee = 0.0;              // SEC regulatory fee
        double finra_taf = 0.0;            // FINRA TAF fee
        double commission = 0.0;           // Broker commission (always 0 for Alpaca)
        double slippage = 0.0;             // Slippage cost
        double market_impact = 0.0;        // Market impact cost
        double short_borrow_cost = 0.0;    // Daily borrow cost for short positions
        double total_cost = 0.0;           // Sum of all costs

        // Helper to display costs
        std::string to_string() const;
    };

    /**
     * Calculate total transaction costs for a trade
     *
     * @param symbol Stock symbol
     * @param price Execution price per share
     * @param shares Number of shares (positive)
     * @param is_buy True for buy orders, false for sell orders
     * @param avg_daily_volume Average daily volume (for market impact)
     * @param current_volatility Current daily volatility (default 2%)
     * @param minutes_from_open Minutes since market open (0-390 for RTH)
     * @param is_short_sale True if this is a short sale (not just closing a long)
     * @return TradeCosts structure with detailed cost breakdown
     */
    static TradeCosts calculate_trade_cost(
        const Symbol& symbol,
        double price,
        int shares,
        bool is_buy,
        double avg_daily_volume = 1000000,
        double current_volatility = 0.02,  // 2% daily volatility default
        int minutes_from_open = 30,
        bool is_short_sale = false
    );

    /**
     * Calculate costs using custom slippage model
     */
    static TradeCosts calculate_trade_cost_with_model(
        const Symbol& symbol,
        double price,
        int shares,
        bool is_buy,
        double avg_daily_volume,
        double current_volatility,
        int minutes_from_open,
        bool is_short_sale,
        const SlippageModel& model
    );

    /**
     * Get borrow rate for a symbol (annualized)
     * Override this in production to query Alpaca's actual borrow rates
     */
    static double get_borrow_rate(const Symbol& symbol);

    /**
     * Set custom borrow rate for a symbol
     */
    static void set_borrow_rate(const Symbol& symbol, double rate);

    /**
     * Clear custom borrow rates
     */
    static void clear_borrow_rates();

    /**
     * Check if a symbol is liquid enough for trading
     * @param avg_daily_volume Average daily volume
     * @param min_volume Minimum acceptable volume (default 1M shares)
     * @return True if symbol meets liquidity threshold
     */
    static bool is_liquid(double avg_daily_volume, double min_volume = 1000000.0);

    /**
     * Check if current time is good for trading (avoid market open/close)
     * @param minutes_from_open Minutes since market open
     * @param buffer_minutes Avoid first/last N minutes (default 15)
     * @return True if within acceptable trading window
     */
    static bool is_good_time_to_trade(int minutes_from_open, int buffer_minutes = 15);

    /**
     * Split large order into smaller chunks to minimize market impact
     * @param total_shares Total order size
     * @param avg_daily_volume Average daily volume
     * @param max_pct_adv Maximum percentage of ADV per chunk (default 0.1%)
     * @return Vector of share amounts for each chunk
     */
    static std::vector<int> split_order(int total_shares, double avg_daily_volume,
                                        double max_pct_adv = 0.001);

private:
    /**
     * Calculate slippage cost
     */
    static double calculate_slippage(
        double price,
        int shares,
        double avg_daily_volume,
        double volatility,
        int minutes_from_open,
        const SlippageModel& model
    );

    /**
     * Calculate market impact cost using square-root model
     */
    static double calculate_market_impact(
        double price,
        int shares,
        double avg_daily_volume,
        bool is_buy
    );

    // Static storage for custom borrow rates
    static std::unordered_map<Symbol, double> custom_borrow_rates_;
};

/**
 * Enhanced Position with Cost Tracking
 *
 * Extends the base Position struct to include:
 * - Entry transaction costs
 * - Estimated exit costs
 * - Daily borrow costs for short positions
 * - Net P&L calculations accounting for all costs
 */
struct PositionWithCosts : public Position {
    AlpacaCostModel::TradeCosts entry_costs;       // Costs paid on entry
    AlpacaCostModel::TradeCosts estimated_exit_costs;  // Estimated exit costs
    double accumulated_borrow_costs = 0.0;         // Total borrow costs (shorts only)
    int days_held = 0;                             // Days position has been held

    PositionWithCosts() = default;

    PositionWithCosts(int s, Price p, Timestamp t, uint64_t bid = 0)
        : Position(s, p, t, bid) {}

    /**
     * Calculate gross P&L (before costs)
     */
    double gross_pnl(Price current_price) const {
        return shares * (current_price - entry_price);
    }

    /**
     * Calculate net P&L (after all costs)
     * Includes entry costs, estimated exit costs, and accumulated borrow costs
     */
    double net_pnl(Price exit_price) const {
        double gross = shares * (exit_price - entry_price);
        return gross - entry_costs.total_cost -
               estimated_exit_costs.total_cost -
               accumulated_borrow_costs;
    }

    /**
     * Calculate net P&L percentage of initial investment
     */
    double net_pnl_percentage(Price exit_price) const {
        double initial_investment = std::abs(shares * entry_price) + entry_costs.total_cost;
        if (initial_investment == 0.0) return 0.0;
        return net_pnl(exit_price) / initial_investment;
    }

    /**
     * Update accumulated borrow costs (call daily for short positions)
     */
    void update_borrow_costs(double daily_borrow_cost) {
        accumulated_borrow_costs += daily_borrow_cost;
        days_held++;
    }

    /**
     * Get total costs incurred so far
     */
    double total_costs_to_date() const {
        return entry_costs.total_cost + accumulated_borrow_costs;
    }

    /**
     * Get estimated total costs if closed now
     */
    double estimated_total_costs() const {
        return entry_costs.total_cost +
               estimated_exit_costs.total_cost +
               accumulated_borrow_costs;
    }
};

/**
 * Market Microstructure Context
 *
 * Stores real-time market data needed for accurate cost calculations
 */
struct MarketContext {
    double avg_daily_volume = 1000000;      // Average daily volume (shares)
    double current_volatility = 0.02;       // Current daily volatility (default 2%)
    int minutes_from_open = 30;             // Minutes since market open
    double bid_ask_spread = 0.01;           // Current bid-ask spread
    double bid_price = 0.0;                 // Current bid
    double ask_price = 0.0;                 // Current ask

    MarketContext() = default;

    MarketContext(double adv, double vol, int minutes)
        : avg_daily_volume(adv), current_volatility(vol),
          minutes_from_open(minutes) {}

    /**
     * Update spread information
     */
    void update_spread(double bid, double ask) {
        bid_price = bid;
        ask_price = ask;
        bid_ask_spread = ask - bid;
    }

    /**
     * Check if market data is stale (need update)
     */
    bool is_stale(int max_minutes = 60) const {
        // In production, compare against last update timestamp
        return false;
    }
};

} // namespace trading
