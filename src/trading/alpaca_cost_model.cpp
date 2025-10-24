#include "trading/alpaca_cost_model.h"
#include <sstream>
#include <iomanip>

namespace trading {

// Static member initialization
std::unordered_map<Symbol, double> AlpacaCostModel::custom_borrow_rates_;

std::string AlpacaCostModel::TradeCosts::to_string() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "SEC: $" << sec_fee
        << ", TAF: $" << finra_taf
        << ", Comm: $" << commission
        << ", Slip: $" << slippage
        << ", Impact: $" << market_impact
        << ", Borrow: $" << short_borrow_cost
        << " => Total: $" << total_cost;
    return oss.str();
}

AlpacaCostModel::TradeCosts AlpacaCostModel::calculate_trade_cost(
    const Symbol& symbol,
    double price,
    int shares,
    bool is_buy,
    double avg_daily_volume,
    double current_volatility,
    int minutes_from_open,
    bool is_short_sale
) {
    SlippageModel default_model;
    return calculate_trade_cost_with_model(
        symbol, price, shares, is_buy,
        avg_daily_volume, current_volatility,
        minutes_from_open, is_short_sale,
        default_model
    );
}

AlpacaCostModel::TradeCosts AlpacaCostModel::calculate_trade_cost_with_model(
    const Symbol& symbol,
    double price,
    int shares,
    bool is_buy,
    double avg_daily_volume,
    double current_volatility,
    int minutes_from_open,
    bool is_short_sale,
    const SlippageModel& model
) {
    TradeCosts costs;
    double trade_value = price * shares;

    // 1. Commission (always zero for Alpaca)
    costs.commission = Fees::COMMISSION;

    // 2. SEC Fee (SELL only) - Applied to notional value
    if (!is_buy) {
        costs.sec_fee = trade_value * Fees::SEC_FEE_RATE;
    }

    // 3. FINRA TAF (SELL only) - Per share with cap
    if (!is_buy) {
        costs.finra_taf = std::min(
            shares * Fees::FINRA_TAF,
            Fees::FINRA_TAF_MAX
        );
    }

    // 4. Slippage
    costs.slippage = calculate_slippage(
        price, shares, avg_daily_volume,
        current_volatility, minutes_from_open, model
    );

    // 5. Market impact (temporary and permanent)
    costs.market_impact = calculate_market_impact(
        price, shares, avg_daily_volume, is_buy
    );

    // 6. Short borrow costs (if applicable)
    // This is the DAILY cost for holding the short position
    if (is_short_sale && !is_buy) {
        // Daily borrow cost (annualized rate / 252 trading days)
        double annual_rate = get_borrow_rate(symbol);
        costs.short_borrow_cost = trade_value * annual_rate / 252.0;
    }

    // Total costs
    costs.total_cost = costs.sec_fee + costs.finra_taf +
                       costs.commission + costs.slippage +
                       costs.market_impact + costs.short_borrow_cost;

    return costs;
}

double AlpacaCostModel::calculate_slippage(
    double price,
    int shares,
    double avg_daily_volume,
    double volatility,
    int minutes_from_open,
    const SlippageModel& model
) {
    // REALISTIC ALPACA SLIPPAGE FOR LIQUID ETFs
    // For market orders on highly liquid ETFs (TQQQ, etc.), slippage is minimal
    // Typical bid-ask spread: 1-2 cents on $100 stock = 1-2 bps

    // Base slippage: 0.5 bps for liquid ETFs (half bid-ask spread)
    double base_slip_per_share = price * 0.5 / 10000.0;

    // Size impact: only matters for large orders (>0.1% ADV)
    double trade_size_pct = static_cast<double>(shares) / avg_daily_volume;
    double size_impact_per_share = 0.0;
    if (trade_size_pct > 0.001) {  // Only apply if >0.1% of ADV
        size_impact_per_share = price * (trade_size_pct * 100.0 * 0.1) / 10000.0;
    }

    // Volatility adjustment: MINIMAL for ETFs (they're highly liquid)
    // Only apply small adjustment for extreme volatility
    double vol_adjustment = 1.0;
    if (volatility > 0.05) {  // Only if vol >5%
        vol_adjustment = 1.0 + (volatility - 0.05) * 0.5;  // Much smaller multiplier
    }
    vol_adjustment = std::max(1.0, std::min(1.5, vol_adjustment));  // Cap at 1.5x

    // Time of day: REMOVED - liquid ETFs trade well all day with tight spreads
    // Alpaca routes to multiple venues with smart order routing
    double time_factor = 1.0;

    // Total slippage (realistic for Alpaca)
    double slippage_per_share = (base_slip_per_share + size_impact_per_share) *
                                vol_adjustment * time_factor;

    return slippage_per_share * shares;
}

double AlpacaCostModel::calculate_market_impact(
    double price,
    int shares,
    double avg_daily_volume,
    bool is_buy
) {
    // REALISTIC MARKET IMPACT FOR ALPACA ETF TRADING
    // For small retail orders (<$100k) in highly liquid ETFs, market impact is NEGLIGIBLE
    // Alpaca uses smart order routing across multiple venues

    double trade_size_pct = static_cast<double>(shares) / avg_daily_volume;
    double trade_value = price * shares;

    // Only apply market impact for large orders
    if (trade_value < 100000.0 || trade_size_pct < 0.01) {
        // Orders under $100k or <1% ADV have effectively zero market impact
        return 0.0;
    }

    // For larger orders, use conservative impact model
    // Temporary impact: much smaller coefficient for liquid ETFs
    double temp_impact_bps = 1.0 * std::sqrt(trade_size_pct * 100.0);

    // Permanent impact: also much smaller for ETFs (they're index-based, not price discovery)
    double perm_impact_bps = 0.5 * trade_size_pct * 100.0;

    // Total impact in basis points
    double total_impact_bps = temp_impact_bps + perm_impact_bps;

    // Direction: minimal difference for liquid ETFs
    double direction_multiplier = is_buy ? 1.0 : 0.8;

    return price * shares * (total_impact_bps / 10000.0) * direction_multiplier;
}

double AlpacaCostModel::get_borrow_rate(const Symbol& symbol) {
    // Check custom rates first
    auto it = custom_borrow_rates_.find(symbol);
    if (it != custom_borrow_rates_.end()) {
        return it->second;
    }

    // Default hard-to-borrow rates for common symbols
    // In production, query Alpaca's API for real-time rates
    static const std::unordered_map<Symbol, double> default_htb_rates = {
        {"TSLA", 0.02},   // 2% annualized
        {"GME", 0.15},    // 15% annualized (high borrow)
        {"AMC", 0.10},    // 10% annualized
        {"RIVN", 0.05},   // 5% annualized
        {"LCID", 0.05},   // 5% annualized
        // Leveraged ETFs typically easy to borrow
        {"TQQQ", 0.003},
        {"SQQQ", 0.003},
        {"UVXY", 0.01},
        {"SVXY", 0.008},
        {"TNA", 0.003},
        {"TZA", 0.003},
        {"FAS", 0.003},
        {"FAZ", 0.003},
    };

    auto default_it = default_htb_rates.find(symbol);
    if (default_it != default_htb_rates.end()) {
        return default_it->second;
    }

    // Default rate for all other symbols
    return Fees::DEFAULT_BORROW_RATE;  // 0.5%
}

void AlpacaCostModel::set_borrow_rate(const Symbol& symbol, double rate) {
    custom_borrow_rates_[symbol] = rate;
}

void AlpacaCostModel::clear_borrow_rates() {
    custom_borrow_rates_.clear();
}

bool AlpacaCostModel::is_liquid(double avg_daily_volume, double min_volume) {
    return avg_daily_volume >= min_volume;
}

bool AlpacaCostModel::is_good_time_to_trade(int minutes_from_open, int buffer_minutes) {
    // Avoid first and last buffer_minutes of trading day
    // Regular trading hours: 390 minutes (9:30 AM - 4:00 PM ET)
    return minutes_from_open >= buffer_minutes &&
           minutes_from_open <= (390 - buffer_minutes);
}

std::vector<int> AlpacaCostModel::split_order(
    int total_shares,
    double avg_daily_volume,
    double max_pct_adv
) {
    std::vector<int> chunks;

    if (total_shares <= 0) {
        return chunks;
    }

    // Calculate maximum chunk size as percentage of ADV
    int max_chunk = static_cast<int>(avg_daily_volume * max_pct_adv);
    max_chunk = std::max(1, max_chunk);  // At least 1 share per chunk

    int remaining = total_shares;
    while (remaining > 0) {
        int chunk = std::min(remaining, max_chunk);
        chunks.push_back(chunk);
        remaining -= chunk;
    }

    return chunks;
}

} // namespace trading
