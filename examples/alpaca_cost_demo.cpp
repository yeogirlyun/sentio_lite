/**
 * Alpaca Cost Model Demonstration
 *
 * This program demonstrates the Alpaca transaction cost model with various
 * trade scenarios to illustrate the impact of:
 * - Regulatory fees (SEC, FINRA TAF)
 * - Slippage
 * - Market impact
 * - Order size
 * - Time of day
 * - Volatility
 */

#include "trading/alpaca_cost_model.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using namespace trading;

void print_section_header(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

void print_cost_breakdown(const std::string& scenario,
                         const AlpacaCostModel::TradeCosts& costs,
                         double trade_value) {
    std::cout << "\n" << scenario << ":\n";
    std::cout << "  Trade Value:       $" << std::fixed << std::setprecision(2)
              << trade_value << "\n";
    std::cout << "  SEC Fee:           $" << std::setprecision(4) << costs.sec_fee << "\n";
    std::cout << "  FINRA TAF:         $" << std::setprecision(4) << costs.finra_taf << "\n";
    std::cout << "  Commission:        $" << std::setprecision(2) << costs.commission << "\n";
    std::cout << "  Slippage:          $" << std::setprecision(4) << costs.slippage << "\n";
    std::cout << "  Market Impact:     $" << std::setprecision(4) << costs.market_impact << "\n";
    std::cout << "  Borrow Cost:       $" << std::setprecision(4) << costs.short_borrow_cost << "\n";
    std::cout << "  -----------------\n";
    std::cout << "  TOTAL COST:        $" << std::setprecision(4) << costs.total_cost;
    std::cout << " (" << std::setprecision(3)
              << (costs.total_cost / trade_value * 100.0) << "% of trade)\n";
}

void example_1_basic_trades() {
    print_section_header("Example 1: Basic Buy/Sell Comparison");

    std::cout << "\nComparing costs for buying vs selling 100 shares of AAPL at $150\n";

    // Buy order
    auto buy_costs = AlpacaCostModel::calculate_trade_cost(
        "AAPL",
        150.0,      // price
        100,        // shares
        true,       // is_buy
        50000000,   // avg_daily_volume (50M shares)
        0.015,      // volatility (1.5%)
        60          // 60 minutes from open
    );

    print_cost_breakdown("BUY 100 AAPL @ $150", buy_costs, 150.0 * 100);

    // Sell order
    auto sell_costs = AlpacaCostModel::calculate_trade_cost(
        "AAPL",
        151.0,      // price
        100,        // shares
        false,      // is_buy (selling)
        50000000,
        0.015,
        60
    );

    print_cost_breakdown("SELL 100 AAPL @ $151", sell_costs, 151.0 * 100);

    double gross_profit = (151.0 - 150.0) * 100;
    double net_profit = gross_profit - buy_costs.total_cost - sell_costs.total_cost;

    std::cout << "\n  Gross P&L:         $" << std::fixed << std::setprecision(2)
              << gross_profit << "\n";
    std::cout << "  Net P&L:           $" << std::setprecision(2) << net_profit << "\n";
    std::cout << "  Cost Impact:       -$" << std::setprecision(2)
              << (buy_costs.total_cost + sell_costs.total_cost)
              << " (-" << std::setprecision(1)
              << ((buy_costs.total_cost + sell_costs.total_cost) / gross_profit * 100.0)
              << "% of gross profit)\n";
}

void example_2_order_size_impact() {
    print_section_header("Example 2: Order Size Impact");

    std::cout << "\nComparing costs for different order sizes (TQQQ @ $50)\n";
    std::cout << "Average Daily Volume: 100M shares\n";

    std::vector<int> sizes = {100, 1000, 10000, 100000};

    for (int shares : sizes) {
        auto costs = AlpacaCostModel::calculate_trade_cost(
            "TQQQ", 50.0, shares, true,
            100000000,  // 100M ADV
            0.03,       // 3% volatility (leveraged ETF)
            120         // 2 hours from open
        );

        std::string scenario = "BUY " + std::to_string(shares) + " TQQQ @ $50";
        std::cout << "\n" << scenario << ":";
        std::cout << "\n  Size as % of ADV:  " << std::fixed << std::setprecision(4)
                  << (static_cast<double>(shares) / 100000000.0 * 100.0) << "%";
        std::cout << "\n  Total Cost:        $" << std::setprecision(2) << costs.total_cost;
        std::cout << "\n  Cost per share:    $" << std::setprecision(4)
                  << (costs.total_cost / shares);
        std::cout << "\n  Cost as % of value: " << std::setprecision(3)
                  << (costs.total_cost / (shares * 50.0) * 100.0) << "%\n";
    }
}

void example_3_time_of_day() {
    print_section_header("Example 3: Time of Day Impact");

    std::cout << "\nComparing costs at different times (1000 shares SQQQ @ $30)\n";

    struct TimePoint {
        int minutes;
        std::string label;
    };

    std::vector<TimePoint> times = {
        {5, "Market Open (9:35 AM)"},
        {30, "30 min after open (10:00 AM)"},
        {120, "Mid-Morning (11:30 AM)"},
        {195, "Lunch (12:45 PM)"},
        {300, "Mid-Afternoon (2:30 PM)"},
        {370, "Near Close (3:50 PM)"}
    };

    for (const auto& tp : times) {
        auto costs = AlpacaCostModel::calculate_trade_cost(
            "SQQQ", 30.0, 1000, true,
            80000000,   // 80M ADV
            0.035,      // 3.5% volatility
            tp.minutes
        );

        std::cout << "\n" << tp.label << ":";
        std::cout << "\n  Slippage:          $" << std::fixed << std::setprecision(4)
                  << costs.slippage;
        std::cout << "\n  Total Cost:        $" << std::setprecision(4)
                  << costs.total_cost << "\n";
    }
}

void example_4_volatility_impact() {
    print_section_header("Example 4: Volatility Impact");

    std::cout << "\nComparing costs under different volatility conditions\n";
    std::cout << "Trade: BUY 5000 shares @ $25\n";

    std::vector<std::pair<double, std::string>> vol_scenarios = {
        {0.01, "Low Volatility (1%)"},
        {0.02, "Normal Volatility (2%)"},
        {0.04, "High Volatility (4%)"},
        {0.08, "Extreme Volatility (8%)"}
    };

    for (const auto& [vol, label] : vol_scenarios) {
        auto costs = AlpacaCostModel::calculate_trade_cost(
            "XYZ", 25.0, 5000, true,
            10000000,   // 10M ADV
            vol,
            90          // 1.5 hours from open
        );

        std::cout << "\n" << label << ":";
        std::cout << "\n  Slippage:          $" << std::fixed << std::setprecision(4)
                  << costs.slippage;
        std::cout << "\n  Market Impact:     $" << std::setprecision(4)
                  << costs.market_impact;
        std::cout << "\n  Total Cost:        $" << std::setprecision(4)
                  << costs.total_cost << "\n";
    }
}

void example_5_leveraged_etf_portfolio() {
    print_section_header("Example 5: Leveraged ETF Rotation Portfolio");

    std::cout << "\nSimulating a rotation trade with $100,000 capital\n";
    std::cout << "Exit position in TQQQ, enter position in SQQQ\n";

    double capital = 100000.0;

    // Exit TQQQ position
    double tqqq_price = 48.50;
    int tqqq_shares = static_cast<int>(capital / tqqq_price);

    auto tqqq_exit = AlpacaCostModel::calculate_trade_cost(
        "TQQQ", tqqq_price, tqqq_shares, false,
        95000000,   // High volume
        0.028,      // 2.8% volatility
        150         // Mid-day
    );

    print_cost_breakdown("EXIT " + std::to_string(tqqq_shares) + " TQQQ @ $" +
                        std::to_string(tqqq_price),
                        tqqq_exit, tqqq_shares * tqqq_price);

    // Enter SQQQ position
    double sqqq_price = 31.75;
    double available_cash = capital - tqqq_exit.total_cost;
    int sqqq_shares = static_cast<int>(available_cash / sqqq_price);

    auto sqqq_enter = AlpacaCostModel::calculate_trade_cost(
        "SQQQ", sqqq_price, sqqq_shares, true,
        72000000,   // High volume
        0.032,      // 3.2% volatility
        152         // Mid-day
    );

    print_cost_breakdown("ENTER " + std::to_string(sqqq_shares) + " SQQQ @ $" +
                        std::to_string(sqqq_price),
                        sqqq_enter, sqqq_shares * sqqq_price);

    double total_rotation_cost = tqqq_exit.total_cost + sqqq_enter.total_cost;
    std::cout << "\n  ROTATION COST:     $" << std::fixed << std::setprecision(2)
              << total_rotation_cost;
    std::cout << " (" << std::setprecision(3)
              << (total_rotation_cost / capital * 100.0) << "% of capital)\n";
}

void example_6_cost_optimization() {
    print_section_header("Example 6: Cost Optimization Strategies");

    std::cout << "\nLarge order: 50,000 shares of TNA @ $40 (ADV: 5M shares)\n";

    // Strategy 1: Single order
    auto single_order = AlpacaCostModel::calculate_trade_cost(
        "TNA", 40.0, 50000, true,
        5000000,    // 5M ADV (order is 1% of ADV!)
        0.025,
        120
    );

    std::cout << "\nStrategy 1: Single Order\n";
    std::cout << "  Order Size:        " << std::fixed << std::setprecision(2)
              << (50000.0 / 5000000.0 * 100.0) << "% of ADV\n";
    std::cout << "  Total Cost:        $" << std::setprecision(2)
              << single_order.total_cost << "\n";
    std::cout << "  Cost per share:    $" << std::setprecision(4)
              << (single_order.total_cost / 50000) << "\n";

    // Strategy 2: Split into chunks
    auto chunks = AlpacaCostModel::split_order(50000, 5000000, 0.001);  // 0.1% of ADV

    std::cout << "\nStrategy 2: Split Order (max 0.1% of ADV per chunk)\n";
    std::cout << "  Number of chunks:  " << chunks.size() << "\n";

    double total_chunked_cost = 0.0;
    for (size_t i = 0; i < std::min(size_t(3), chunks.size()); ++i) {
        auto chunk_cost = AlpacaCostModel::calculate_trade_cost(
            "TNA", 40.0, chunks[i], true, 5000000, 0.025, 120
        );
        total_chunked_cost += chunk_cost.total_cost;
        std::cout << "  Chunk " << (i+1) << " (" << chunks[i] << " shares): $"
                  << std::fixed << std::setprecision(2) << chunk_cost.total_cost << "\n";
    }
    std::cout << "  ... (showing first 3 chunks only)\n";

    // Estimate total for all chunks
    double avg_chunk_cost = total_chunked_cost / std::min(size_t(3), chunks.size());
    double estimated_total = avg_chunk_cost * chunks.size();

    std::cout << "  Estimated Total:   $" << std::fixed << std::setprecision(2)
              << estimated_total << "\n";
    std::cout << "  Savings vs Single: $" << std::setprecision(2)
              << (single_order.total_cost - estimated_total) << " ("
              << std::setprecision(1)
              << ((single_order.total_cost - estimated_total) / single_order.total_cost * 100.0)
              << "%)\n";
}

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ALPACA TRANSACTION COST MODEL DEMONSTRATION            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";

    try {
        example_1_basic_trades();
        example_2_order_size_impact();
        example_3_time_of_day();
        example_4_volatility_impact();
        example_5_leveraged_etf_portfolio();
        example_6_cost_optimization();

        print_section_header("Summary: Key Takeaways");
        std::cout << "\n";
        std::cout << "1. Buy orders only pay slippage/impact - no regulatory fees\n";
        std::cout << "2. Sell orders pay SEC ($27.80 per $1M) and FINRA TAF fees\n";
        std::cout << "3. Alpaca has ZERO commissions\n";
        std::cout << "4. Market impact grows with sqrt(order size)\n";
        std::cout << "5. Avoid trading at market open/close (higher slippage)\n";
        std::cout << "6. Higher volatility = higher slippage costs\n";
        std::cout << "7. Split large orders to reduce market impact\n";
        std::cout << "8. Total costs typically 0.01% - 0.10% for liquid stocks\n";
        std::cout << "\n";

        print_section_header("Integration Notes");
        std::cout << "\n";
        std::cout << "The cost model is fully integrated into MultiSymbolTrader:\n";
        std::cout << "- Set config.enable_cost_tracking = true (default)\n";
        std::cout << "- Costs are automatically calculated and deducted\n";
        std::cout << "- BacktestResults includes detailed cost metrics\n";
        std::cout << "- Use MarketContext to track ADV and volatility\n";
        std::cout << "- Cost tracking works for both backtesting and live trading\n";
        std::cout << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
