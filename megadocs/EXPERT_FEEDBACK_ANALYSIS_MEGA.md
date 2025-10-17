# EXPERT_FEEDBACK_ANALYSIS - Complete Analysis

**Generated**: 2025-10-16 09:40:41
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: /Volumes/ExternalSSD/Dev/C++/online_trader/EXPERT_FEEDBACK_ANALYSIS.md
**Total Files**: 6

---

## 📋 **TABLE OF CONTENTS**

1. [include/common/eod_state.h](#file-1)
2. [include/common/time_utils.h](#file-2)
3. [src/cli/analyze_trades_command.cpp](#file-3)
4. [src/cli/execute_trades_command.cpp](#file-4)
5. [src/cli/live_trade_command.cpp](#file-5)
6. [tools/adaptive_optuna.py](#file-6)

---

## 📄 **FILE 1 of 6**: include/common/eod_state.h

**File Information**:
- **Path**: `include/common/eod_state.h`
- **Size**: 83 lines
- **Modified**: 2025-10-16 04:24:40
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <string>
#include <optional>

namespace sentio {

/**
 * @brief EOD liquidation status
 */
enum class EodStatus {
    PENDING,      // No EOD action taken yet today
    IN_PROGRESS,  // EOD liquidation in progress
    DONE          // EOD liquidation completed
};

/**
 * @brief EOD state for a trading day
 */
struct EodState {
    EodStatus status{EodStatus::PENDING};
    std::string positions_hash;  // Hash of positions when DONE (for verification)
    int64_t last_attempt_epoch{0};  // Unix timestamp (seconds) of last liquidation attempt
};

/**
 * @brief Persistent state tracking for End-of-Day (EOD) liquidation
 *
 * Ensures idempotent EOD execution - prevents double liquidation on process restart
 * and enables detection of missed EOD events.
 *
 * State is persisted to a simple text file containing the last ET date (YYYY-MM-DD)
 * for which EOD liquidation was completed.
 */
class EodStateStore {
public:
    /**
     * @brief Construct state store with file path
     * @param state_file_path Full path to state file (e.g., "/tmp/sentio_eod_state.txt")
     */
    explicit EodStateStore(std::string state_file_path);

    /**
     * @brief Get the ET date (YYYY-MM-DD) of the last completed EOD
     * @return ET date string if available, nullopt if no EOD recorded
     */
    std::optional<std::string> last_eod_date() const;

    /**
     * @brief Mark EOD liquidation as complete for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     *
     * Atomically writes the date to the state file, overwriting previous value.
     * This ensures exactly-once semantics for EOD execution per trading day.
     */
    void mark_eod_complete(const std::string& et_date);

    /**
     * @brief Check if EOD already completed for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @return true if EOD already done for this date
     */
    bool is_eod_complete(const std::string& et_date) const;

    /**
     * @brief Load EOD state for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @return EodState (PENDING if no state exists)
     */
    EodState load(const std::string& et_date) const;

    /**
     * @brief Save EOD state for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @param state EOD state to save
     */
    void save(const std::string& et_date, const EodState& state);

private:
    std::string state_file_;
};

} // namespace sentio

```

## 📄 **FILE 2 of 6**: include/common/time_utils.h

**File Information**:
- **Path**: `include/common/time_utils.h`
- **Size**: 241 lines
- **Modified**: 2025-10-16 04:16:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <cstdio>

namespace sentio {

/**
 * @brief Trading session configuration with timezone support
 *
 * Handles market hours, weekends, and timezone conversions.
 * Uses system timezone API for DST-aware calculations.
 */
struct TradingSession {
    std::string timezone_name;  // IANA timezone (e.g., "America/New_York")
    int market_open_hour{9};
    int market_open_minute{30};
    int market_close_hour{16};
    int market_close_minute{0};

    TradingSession(const std::string& tz_name = "America/New_York")
        : timezone_name(tz_name) {}

    /**
     * @brief Check if given time is during regular trading hours
     * @param tp System clock time point
     * @return true if within market hours (9:30 AM - 4:00 PM ET)
     */
    bool is_regular_hours(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Check if given time is a weekday
     * @param tp System clock time point
     * @return true if Monday-Friday
     */
    bool is_weekday(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Check if given time is a trading day (weekday, not holiday)
     * @param tp System clock time point
     * @return true if trading day
     * @note Holiday calendar not yet implemented - returns weekday check only
     */
    bool is_trading_day(const std::chrono::system_clock::time_point& tp) const {
        // TODO: Add holiday calendar check
        return is_weekday(tp);
    }

    /**
     * @brief Get local time string in timezone
     * @param tp System clock time point
     * @return Formatted time string "YYYY-MM-DD HH:MM:SS TZ"
     */
    std::string to_local_string(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Convert system time to local time in configured timezone
     * @param tp System clock time point
     * @return Local time struct
     */
    std::tm to_local_time(const std::chrono::system_clock::time_point& tp) const;
};

/**
 * @brief Get current time (always uses system UTC, convert to ET via TradingSession)
 * @return System clock time point
 */
inline std::chrono::system_clock::time_point now() {
    return std::chrono::system_clock::now();
}

/**
 * @brief Format timestamp to ISO 8601 string
 * @param tp System clock time point
 * @return ISO formatted string "YYYY-MM-DDTHH:MM:SSZ"
 */
std::string to_iso_string(const std::chrono::system_clock::time_point& tp);

/**
 * @brief Centralized ET Time Manager - ALL time operations should use this
 *
 * This class ensures consistent ET timezone handling across the entire system.
 * No direct time conversions should be done elsewhere.
 */
class ETTimeManager {
public:
    ETTimeManager() : session_("America/New_York"), use_mock_time_(false) {}

    /**
     * @brief Enable mock time mode (for replay/testing)
     * @param timestamp_ms Simulated time in milliseconds
     */
    void set_mock_time(uint64_t timestamp_ms) {
        use_mock_time_ = true;
        mock_time_ = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms)
        );
    }

    /**
     * @brief Disable mock time mode (return to wall-clock time)
     */
    void disable_mock_time() {
        use_mock_time_ = false;
    }

    /**
     * @brief Get current ET time as formatted string
     * @return "YYYY-MM-DD HH:MM:SS ET"
     */
    std::string get_current_et_string() const {
        return session_.to_local_string(get_time());
    }

    /**
     * @brief Get current ET time components
     * @return struct tm in ET timezone
     */
    std::tm get_current_et_tm() const {
        return session_.to_local_time(get_time());
    }

    /**
     * @brief Get current ET date as string (YYYY-MM-DD format)
     * @return Date string in format "2025-10-09"
     */
    std::string get_current_et_date() const {
        auto et_tm = get_current_et_tm();
        char buffer[11];  // "YYYY-MM-DD\0"
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                     et_tm.tm_year + 1900,
                     et_tm.tm_mon + 1,
                     et_tm.tm_mday);
        return std::string(buffer);
    }

    /**
     * @brief Check if current time is during regular trading hours (9:30 AM - 4:00 PM ET)
     */
    bool is_regular_hours() const {
        return session_.is_regular_hours(get_time()) && session_.is_trading_day(get_time());
    }

    /**
     * @brief Check if current time is in EOD liquidation window (3:58 PM - 4:00 PM ET)
     * Uses a 2-minute window to liquidate positions before market close
     */
    bool is_eod_liquidation_window() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // EOD window: 3:58 PM - 4:00 PM ET
        if (hour == 15 && minute >= 58) return true;  // 3:58-3:59 PM
        if (hour == 16 && minute == 0) return true;   // 4:00 PM exactly

        return false;
    }

    /**
     * @brief Check if current time is mid-day optimization window (15:15 PM ET exactly)
     * Used for adaptive parameter tuning based on comprehensive data (historical + today's bars)
     */
    bool is_midday_optimization_time() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // Mid-day optimization: 15:15 PM ET (3:15pm) - during trading hours
        return (hour == 15 && minute == 15);
    }

    /**
     * @brief Check if we should liquidate positions on startup (started outside trading hours with open positions)
     */
    bool should_liquidate_on_startup(bool has_positions) const {
        if (!has_positions) return false;

        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;

        // If started after market close (after 4 PM) or before market open (before 9:30 AM),
        // and we have positions, we should liquidate
        bool after_hours = (hour >= 16) || (hour < 9) || (hour == 9 && et_tm.tm_min < 30);

        return after_hours;
    }

    /**
     * @brief Check if market has closed (>= 4:00 PM ET)
     * Used to trigger automatic shutdown after EOD liquidation
     */
    bool is_market_close_time() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // Market closes at 4:00 PM ET - shutdown at 4:00 PM or later
        return (hour >= 16);
    }

    /**
     * @brief Get minutes since midnight ET
     */
    int get_et_minutes_since_midnight() const {
        auto et_tm = get_current_et_tm();
        return et_tm.tm_hour * 60 + et_tm.tm_min;
    }

    /**
     * @brief Access to underlying TradingSession
     */
    const TradingSession& session() const { return session_; }

private:
    /**
     * @brief Get current time (mock or wall-clock)
     */
    std::chrono::system_clock::time_point get_time() const {
        return use_mock_time_ ? mock_time_ : now();
    }

    TradingSession session_;
    bool use_mock_time_;
    std::chrono::system_clock::time_point mock_time_;
};

/**
 * @brief Get Unix timestamp in microseconds
 * @param tp System clock time point
 * @return Microseconds since epoch
 */
inline uint64_t to_unix_micros(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        tp.time_since_epoch()
    ).count();
}

} // namespace sentio

```

## 📄 **FILE 3 of 6**: src/cli/analyze_trades_command.cpp

**File Information**:
- **Path**: `src/cli/analyze_trades_command.cpp`
- **Size**: 446 lines
- **Modified**: 2025-10-09 15:15:21
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/ensemble_workflow_command.h"
#include "common/utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace sentio {
namespace cli {

// Per-instrument performance tracking
struct InstrumentMetrics {
    std::string symbol;
    int num_trades = 0;
    int buy_count = 0;
    int sell_count = 0;
    double total_buy_value = 0.0;
    double total_sell_value = 0.0;
    double realized_pnl = 0.0;
    double avg_allocation_pct = 0.0;  // Average % of portfolio allocated
    double win_rate = 0.0;
    int winning_trades = 0;
    int losing_trades = 0;
};

int AnalyzeTradesCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string trades_path = get_arg(args, "--trades", "");
    std::string output_path = get_arg(args, "--output", "analysis_report.json");
    int num_blocks = std::stoi(get_arg(args, "--blocks", "0"));  // Number of blocks for MRB calculation
    bool show_detailed = !has_flag(args, "--summary-only");
    bool show_trades = has_flag(args, "--show-trades");
    bool export_csv = has_flag(args, "--csv");
    bool export_json = !has_flag(args, "--no-json");
    bool json_stdout = has_flag(args, "--json");  // Output JSON metrics to stdout for Optuna

    if (trades_path.empty()) {
        std::cerr << "Error: --trades is required\n";
        show_help();
        return 1;
    }

    if (!json_stdout) {
        std::cout << "=== OnlineEnsemble Trade Analysis ===\n";
        std::cout << "Trade file: " << trades_path << "\n\n";
    }

    // Load trades from JSONL
    if (!json_stdout) {
        std::cout << "Loading trade history...\n";
    }
    std::vector<ExecuteTradesCommand::TradeRecord> trades;

    std::ifstream file(trades_path);
    if (!file) {
        std::cerr << "Error: Could not open trade file\n";
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        try {
            json j = json::parse(line);
            ExecuteTradesCommand::TradeRecord trade;

            trade.bar_id = j["bar_id"];
            trade.timestamp_ms = j["timestamp_ms"];
            trade.bar_index = j["bar_index"];
            trade.symbol = j["symbol"];

            std::string action_str = j["action"];
            trade.action = (action_str == "BUY") ? TradeAction::BUY : TradeAction::SELL;

            trade.quantity = j["quantity"];
            trade.price = j["price"];
            trade.trade_value = j["trade_value"];
            trade.fees = j["fees"];
            trade.reason = j["reason"];

            trade.cash_balance = j["cash_balance"];
            trade.portfolio_value = j["portfolio_value"];
            trade.position_quantity = j["position_quantity"];
            trade.position_avg_price = j["position_avg_price"];

            trades.push_back(trade);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to parse line: " << e.what() << "\n";
        }
    }

    if (!json_stdout) {
        std::cout << "Loaded " << trades.size() << " trades\n\n";
    }

    if (trades.empty()) {
        std::cerr << "Error: No trades loaded\n";
        return 1;
    }

    // Calculate per-instrument metrics
    if (!json_stdout) {
        std::cout << "Calculating per-instrument metrics...\n";
    }
    std::map<std::string, InstrumentMetrics> instrument_metrics;
    std::map<std::string, std::vector<std::pair<double, double>>> position_tracking;  // symbol -> [(buy_price, quantity)]

    double starting_capital = 100000.0;  // Assume standard starting capital
    double total_allocation_samples = 0;

    for (const auto& trade : trades) {
        auto& metrics = instrument_metrics[trade.symbol];
        metrics.symbol = trade.symbol;
        metrics.num_trades++;

        if (trade.action == TradeAction::BUY) {
            metrics.buy_count++;
            metrics.total_buy_value += trade.trade_value;

            // Track position for P/L calculation
            position_tracking[trade.symbol].push_back({trade.price, trade.quantity});

            // Track allocation
            double allocation_pct = (trade.trade_value / trade.portfolio_value) * 100.0;
            metrics.avg_allocation_pct += allocation_pct;
            total_allocation_samples++;

        } else {  // SELL
            metrics.sell_count++;
            metrics.total_sell_value += trade.trade_value;

            // Calculate realized P/L using FIFO
            auto& positions = position_tracking[trade.symbol];
            double remaining_qty = trade.quantity;
            double trade_pnl = 0.0;

            while (remaining_qty > 0 && !positions.empty()) {
                auto& pos = positions.front();
                double qty_to_close = std::min(remaining_qty, pos.second);

                // P/L = (sell_price - buy_price) * quantity
                trade_pnl += (trade.price - pos.first) * qty_to_close;

                pos.second -= qty_to_close;
                remaining_qty -= qty_to_close;

                if (pos.second <= 0) {
                    positions.erase(positions.begin());
                }
            }

            metrics.realized_pnl += trade_pnl;

            // Track win/loss
            if (trade_pnl > 0) {
                metrics.winning_trades++;
            } else if (trade_pnl < 0) {
                metrics.losing_trades++;
            }
        }
    }

    // Calculate averages and win rates
    for (auto& [symbol, metrics] : instrument_metrics) {
        if (metrics.buy_count > 0) {
            metrics.avg_allocation_pct /= metrics.buy_count;
        }
        int completed_trades = metrics.winning_trades + metrics.losing_trades;
        if (completed_trades > 0) {
            metrics.win_rate = (double)metrics.winning_trades / completed_trades * 100.0;
        }
    }

    // Calculate overall metrics
    if (!json_stdout) {
        std::cout << "Calculating overall performance metrics...\n";
    }
    PerformanceReport report = calculate_metrics(trades);

    // Print instrument analysis
    if (!json_stdout) {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║         PER-INSTRUMENT PERFORMANCE ANALYSIS                ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }

    // Sort instruments by realized P/L (descending)
    std::vector<std::pair<std::string, InstrumentMetrics>> sorted_instruments;
    for (const auto& [symbol, metrics] : instrument_metrics) {
        sorted_instruments.push_back({symbol, metrics});
    }
    std::sort(sorted_instruments.begin(), sorted_instruments.end(),
              [](const auto& a, const auto& b) { return a.second.realized_pnl > b.second.realized_pnl; });

    if (!json_stdout) {
        std::cout << std::fixed << std::setprecision(2);

        for (const auto& [symbol, m] : sorted_instruments) {
            std::string pnl_indicator = (m.realized_pnl > 0) ? "✅" : (m.realized_pnl < 0) ? "❌" : "  ";

            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            std::cout << symbol << " " << pnl_indicator << "\n";
            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            std::cout << "  Trades:           " << m.num_trades << " (" << m.buy_count << " BUY, " << m.sell_count << " SELL)\n";
            std::cout << "  Total Buy Value:  $" << std::setw(12) << m.total_buy_value << "\n";
            std::cout << "  Total Sell Value: $" << std::setw(12) << m.total_sell_value << "\n";
            std::cout << "  Realized P/L:     $" << std::setw(12) << m.realized_pnl
                      << "  (" << std::showpos << (m.realized_pnl / starting_capital * 100.0)
                      << std::noshowpos << "% of capital)\n";
            std::cout << "  Avg Allocation:   " << std::setw(12) << m.avg_allocation_pct << "%\n";
            std::cout << "  Win Rate:         " << std::setw(12) << m.win_rate << "%  ("
                      << m.winning_trades << "W / " << m.losing_trades << "L)\n";
            std::cout << "\n";
        }

        // Summary table
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║              INSTRUMENT SUMMARY TABLE                      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        std::cout << std::left << std::setw(8) << "Symbol"
                  << std::right << std::setw(10) << "Trades"
                  << std::setw(12) << "Alloc %"
                  << std::setw(15) << "P/L ($)"
                  << std::setw(12) << "P/L (%)"
                  << std::setw(12) << "Win Rate"
                  << "\n";
        std::cout << "────────────────────────────────────────────────────────────────\n";

        for (const auto& [symbol, m] : sorted_instruments) {
            double pnl_pct = (m.realized_pnl / starting_capital) * 100.0;
            std::cout << std::left << std::setw(8) << symbol
                      << std::right << std::setw(10) << m.num_trades
                      << std::setw(12) << m.avg_allocation_pct
                      << std::setw(15) << m.realized_pnl
                      << std::setw(12) << std::showpos << pnl_pct << std::noshowpos
                      << std::setw(12) << m.win_rate
                      << "\n";
        }
    }

    // Calculate total realized P/L from instruments
    double total_realized_pnl = 0.0;
    for (const auto& [symbol, m] : instrument_metrics) {
        total_realized_pnl += m.realized_pnl;
    }

    if (!json_stdout) {
        std::cout << "────────────────────────────────────────────────────────────────\n";
        std::cout << std::left << std::setw(8) << "TOTAL"
                  << std::right << std::setw(10) << trades.size()
                  << std::setw(12) << ""
                  << std::setw(15) << total_realized_pnl
                  << std::setw(12) << std::showpos << (total_realized_pnl / starting_capital * 100.0) << std::noshowpos
                  << std::setw(12) << ""
                  << "\n\n";
    }

    // Calculate MRB (Mean Return per Block) - for strategies with overnight carry
    double total_return_pct = (total_realized_pnl / starting_capital) * 100.0;
    double mrb = 0.0;
    if (num_blocks > 0) {
        mrb = total_return_pct / num_blocks;
    }

    // Calculate MRD (Mean Return per Day) - for daily reset strategies
    // This is the more accurate metric for strategies with EOD liquidation
    double mrd = 0.0;
    int num_trading_days = 0;
    std::vector<double> daily_returns;

    if (!trades.empty()) {
        // Group trades by trading day
        std::map<std::string, std::vector<ExecuteTradesCommand::TradeRecord>> trades_by_day;

        for (const auto& trade : trades) {
            // Extract date from timestamp (YYYY-MM-DD)
            std::time_t trade_time = static_cast<std::time_t>(trade.timestamp_ms / 1000);
            std::tm tm_utc{};
            #ifdef _WIN32
                gmtime_s(&tm_utc, &trade_time);
            #else
                gmtime_r(&trade_time, &tm_utc);
            #endif

            // Convert to ET (subtract 4 hours for EDT)
            int et_hour = tm_utc.tm_hour - 4;
            if (et_hour < 0) et_hour += 24;

            // Format as YYYY-MM-DD
            char date_str[32];
            std::snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
                         tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday);

            trades_by_day[date_str].push_back(trade);
        }

        // Calculate daily returns
        double prev_day_end_value = starting_capital;

        for (const auto& [date, day_trades] : trades_by_day) {
            if (day_trades.empty()) continue;

            // Get final portfolio value of the day
            double day_end_value = day_trades.back().portfolio_value;

            // Calculate daily return
            double daily_return_pct = ((day_end_value - prev_day_end_value) / prev_day_end_value) * 100.0;
            daily_returns.push_back(daily_return_pct);

            // Update for next day
            prev_day_end_value = day_end_value;
        }

        num_trading_days = static_cast<int>(daily_returns.size());

        // MRD = mean of daily returns
        if (!daily_returns.empty()) {
            double sum = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0);
            mrd = sum / daily_returns.size();
        }
    }

    // Print metrics
    if (!json_stdout) {
        if (num_blocks > 0) {
            std::cout << "Mean Return per Block (MRB): " << std::showpos << std::fixed << std::setprecision(4)
                      << mrb << std::noshowpos << "% (" << num_blocks << " blocks of 391 bars)\n";
        }

        if (num_trading_days > 0) {
            std::cout << "Mean Return per Day (MRD):   " << std::showpos << std::fixed << std::setprecision(4)
                      << mrd << std::noshowpos << "% (" << num_trading_days << " trading days)\n";

            // Show annualized projection
            double annualized_mrd = mrd * 252.0;  // 252 trading days per year
            std::cout << "  Annualized (252 days):     " << std::showpos << std::fixed << std::setprecision(2)
                      << annualized_mrd << std::noshowpos << "%\n";
        }

        std::cout << "\n";
    }

    // Calculate overall win rate and trades per block
    int total_winning = 0, total_losing = 0;
    for (const auto& [symbol, m] : instrument_metrics) {
        total_winning += m.winning_trades;
        total_losing += m.losing_trades;
    }
    double overall_win_rate = (total_winning + total_losing > 0)
        ? (double)total_winning / (total_winning + total_losing) * 100.0 : 0.0;
    double trades_per_block = (num_blocks > 0) ? (double)trades.size() / num_blocks : 0.0;

    // If --json flag, output metrics as JSON to stdout and exit
    if (json_stdout) {
        json result;
        result["mrb"] = mrb;
        result["mrd"] = mrd;  // New: Mean Return per Day (primary metric for daily strategies)
        result["total_return_pct"] = total_return_pct;
        result["win_rate"] = overall_win_rate;
        result["total_trades"] = trades.size();
        result["trades_per_block"] = trades_per_block;
        result["num_blocks"] = num_blocks;
        result["num_trading_days"] = num_trading_days;

        // Output compact JSON (single line) for Optuna parsing
        std::cout << result.dump() << std::endl;
        return 0;
    }

    // Print overall report
    print_report(report);

    // Save report
    if (export_json) {
        std::cout << "\nSaving report to " << output_path << "...\n";
        save_report_json(report, output_path);
    }

    std::cout << "\n✅ Analysis complete!\n";
    return 0;
}

AnalyzeTradesCommand::PerformanceReport
AnalyzeTradesCommand::calculate_metrics(const std::vector<ExecuteTradesCommand::TradeRecord>& trades) {
    PerformanceReport report;

    if (trades.empty()) {
        return report;
    }

    // Basic counts
    report.total_trades = static_cast<int>(trades.size());

    // Extract equity curve from trades
    std::vector<double> equity;
    for (const auto& trade : trades) {
        equity.push_back(trade.portfolio_value);
    }

    // Calculate returns (stub - would need full implementation)
    report.total_return_pct = 0.0;
    report.annualized_return = 0.0;

    return report;
}

void AnalyzeTradesCommand::print_report(const PerformanceReport& report) {
    // Stub - basic implementation
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ONLINE ENSEMBLE PERFORMANCE REPORT                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Total Trades: " << report.total_trades << "\n";
}

void AnalyzeTradesCommand::save_report_json(const PerformanceReport& report, const std::string& path) {
    // Stub
}

void AnalyzeTradesCommand::show_help() const {
    std::cout << "Usage: sentio_cli analyze-trades --trades <file> [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --trades <file>     Trade history file (JSONL format)\n";
    std::cout << "  --output <file>     Output report file (default: analysis_report.json)\n";
    std::cout << "  --blocks <N>        Number of blocks traded (for MRB calculation)\n";
    std::cout << "  --json              Output metrics as JSON to stdout (for Optuna)\n";
    std::cout << "  --summary-only      Show only summary metrics\n";
    std::cout << "  --show-trades       Show individual trade details\n";
    std::cout << "  --csv               Export to CSV format\n";
    std::cout << "  --no-json           Disable JSON export\n";
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 4 of 6**: src/cli/execute_trades_command.cpp

**File Information**:
- **Path**: `src/cli/execute_trades_command.cpp`
- **Size**: 839 lines
- **Modified**: 2025-10-10 08:08:22
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/ensemble_workflow_command.h"
#include "backend/adaptive_portfolio_manager.h"
#include "backend/position_state_machine.h"
#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include "strategy/signal_output.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace sentio {
namespace cli {

// Helper: Get price for specific instrument at bar index
inline double get_instrument_price(
    const std::map<std::string, std::vector<Bar>>& instrument_bars,
    const std::string& symbol,
    size_t bar_index) {

    if (instrument_bars.count(symbol) > 0 && bar_index < instrument_bars.at(symbol).size()) {
        return instrument_bars.at(symbol)[bar_index].close;
    }
    return 0.0;  // Should never happen if data is properly loaded
}

// Helper: Create symbol mapping for PSM states based on base symbol
ExecuteTradesCommand::SymbolMap create_symbol_map(const std::string& base_symbol,
                                                   const std::vector<std::string>& symbols) {
    ExecuteTradesCommand::SymbolMap mapping;
    if (base_symbol == "QQQ") {
        mapping.base = "QQQ";
        mapping.bull_3x = "TQQQ";
        mapping.bear_1x = "PSQ";
        mapping.bear_nx = "SQQQ";
    } else if (base_symbol == "SPY") {
        mapping.base = "SPY";
        mapping.bull_3x = "SPXL";
        mapping.bear_1x = "SH";

        // Check if using SPXS (-3x) or SDS (-2x)
        if (std::find(symbols.begin(), symbols.end(), "SPXS") != symbols.end()) {
            mapping.bear_nx = "SPXS";  // -3x symmetric
        } else {
            mapping.bear_nx = "SDS";   // -2x asymmetric
        }
    }
    return mapping;
}

int ExecuteTradesCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string signal_path = get_arg(args, "--signals", "");
    std::string data_path = get_arg(args, "--data", "");
    std::string output_path = get_arg(args, "--output", "trades.jsonl");
    double starting_capital = std::stod(get_arg(args, "--capital", "100000"));
    double buy_threshold = std::stod(get_arg(args, "--buy-threshold", "0.53"));
    double sell_threshold = std::stod(get_arg(args, "--sell-threshold", "0.47"));
    bool enable_kelly = !has_flag(args, "--no-kelly");
    bool verbose = has_flag(args, "--verbose") || has_flag(args, "-v");
    bool csv_output = has_flag(args, "--csv");

    // PSM Risk Management Parameters (CLI overrides, defaults from v1.5 SPY calibration)
    double profit_target = std::stod(get_arg(args, "--profit-target", "0.003"));
    double stop_loss = std::stod(get_arg(args, "--stop-loss", "-0.004"));
    int min_hold_bars = std::stoi(get_arg(args, "--min-hold-bars", "0"));  // Changed from 3 to 0 for faster exits
    int max_hold_bars = std::stoi(get_arg(args, "--max-hold-bars", "100"));

    if (signal_path.empty() || data_path.empty()) {
        std::cerr << "Error: --signals and --data are required\n";
        show_help();
        return 1;
    }

    std::cout << "=== OnlineEnsemble Trade Execution ===\n";
    std::cout << "Signals: " << signal_path << "\n";
    std::cout << "Data: " << data_path << "\n";
    std::cout << "Output: " << output_path << "\n";
    std::cout << "Starting Capital: $" << std::fixed << std::setprecision(2) << starting_capital << "\n";
    std::cout << "Kelly Sizing: " << (enable_kelly ? "Enabled" : "Disabled") << "\n";
    std::cout << "PSM Parameters: profit=" << (profit_target*100) << "%, stop=" << (stop_loss*100)
              << "%, hold=" << min_hold_bars << "-" << max_hold_bars << " bars\n\n";

    // Load signals
    std::cout << "Loading signals...\n";
    std::vector<SignalOutput> signals;
    std::ifstream sig_file(signal_path);
    if (!sig_file) {
        std::cerr << "Error: Could not open signal file\n";
        return 1;
    }

    std::string line;
    while (std::getline(sig_file, line)) {
        // Parse JSONL (simplified)
        SignalOutput sig = SignalOutput::from_json(line);
        signals.push_back(sig);
    }
    std::cout << "Loaded " << signals.size() << " signals\n";

    // Load market data for ALL instruments
    // Auto-detect base symbol (QQQ or SPY) from data file path
    std::cout << "Loading market data for all instruments...\n";

    // Extract directory from data path (use same directory for all instrument files)
    size_t last_slash = data_path.find_last_of("/\\");
    std::string instruments_dir = (last_slash != std::string::npos)
        ? data_path.substr(0, last_slash)
        : "data/equities";  // Fallback if no directory in path

    std::cout << "Using instruments directory: " << instruments_dir << "\n";

    // Detect base symbol from filename (QQQ_RTH_NH.csv or SPY_RTH_NH.csv)
    std::string filename = data_path.substr(last_slash + 1);
    std::string base_symbol;
    std::vector<std::string> symbols;

    if (filename.find("QQQ") != std::string::npos) {
        base_symbol = "QQQ";
        symbols = {"QQQ", "TQQQ", "PSQ", "SQQQ"};
        std::cout << "Detected QQQ trading (3x bull: TQQQ, -1x: PSQ, -3x: SQQQ)\n";
    } else if (filename.find("SPY") != std::string::npos) {
        base_symbol = "SPY";

        // Check if SPXS (-3x) exists, otherwise use SDS (-2x)
        std::string spxs_path = instruments_dir + "/SPXS_RTH_NH.csv";
        std::ifstream spxs_check(spxs_path);

        if (spxs_check.good()) {
            symbols = {"SPY", "SPXL", "SH", "SPXS"};
            std::cout << "Detected SPY trading (3x bull: SPXL, -1x: SH, -3x: SPXS) [SYMMETRIC LEVERAGE]\n";
        } else {
            symbols = {"SPY", "SPXL", "SH", "SDS"};
            std::cout << "Detected SPY trading (3x bull: SPXL, -1x: SH, -2x: SDS) [ASYMMETRIC LEVERAGE]\n";
        }
        spxs_check.close();
    } else {
        std::cerr << "Error: Could not detect base symbol from " << filename << "\n";
        std::cerr << "Expected filename to contain 'QQQ' or 'SPY'\n";
        return 1;
    }

    // Load all 4 instruments from data/equities directory
    std::map<std::string, std::vector<Bar>> instrument_bars;

    for (const auto& symbol : symbols) {
        std::string instrument_path = instruments_dir + "/" + symbol + "_RTH_NH.csv";
        auto bars = utils::read_csv_data(instrument_path);
        if (bars.empty()) {
            std::cerr << "Error: Could not load " << symbol << " data from " << instrument_path << "\n";
            return 1;
        }
        instrument_bars[symbol] = std::move(bars);
        std::cout << "  Loaded " << instrument_bars[symbol].size() << " bars for " << symbol << "\n";
    }

    // Use base symbol bars as reference for bar count
    auto& bars = instrument_bars[base_symbol];
    std::cout << "Total bars: " << bars.size() << "\n\n";

    if (signals.size() != bars.size()) {
        std::cerr << "Warning: Signal count (" << signals.size() << ") != bar count (" << bars.size() << ")\n";
    }

    // Create symbol mapping for PSM
    SymbolMap symbol_map = create_symbol_map(base_symbol, symbols);

    // Create Position State Machine for 4-instrument strategy
    PositionStateMachine psm;

    // Portfolio state tracking
    PortfolioState portfolio;
    portfolio.cash_balance = starting_capital;
    portfolio.total_equity = starting_capital;

    // Trade history
    PortfolioHistory history;
    history.starting_capital = starting_capital;
    history.equity_curve.push_back(starting_capital);

    // Track position entry for profit-taking and stop-loss
    struct PositionTracking {
        double entry_price = 0.0;
        double entry_equity = 0.0;
        int bars_held = 0;
        PositionStateMachine::State state = PositionStateMachine::State::CASH_ONLY;
    };
    PositionTracking current_position;
    current_position.entry_equity = starting_capital;

    // Risk management parameters - Now configurable via CLI
    // Defaults from v1.5 SPY calibration (5-year analysis)
    // Use: --profit-target, --stop-loss, --min-hold-bars, --max-hold-bars
    const double PROFIT_TARGET = profit_target;
    const double STOP_LOSS = stop_loss;
    const int MIN_HOLD_BARS = min_hold_bars;
    const int MAX_HOLD_BARS = max_hold_bars;

    std::cout << "Executing trades with Position State Machine...\n";
    std::cout << "Version 1.5: SPY-CALIBRATED thresholds + 3-bar min hold + 0.3%/-0.4% targets\n";
    std::cout << "  (Calibrated from 5-year SPY data: 1,018 blocks, Oct 2020-Oct 2025)\n";
    std::cout << "  QQQ v1.0: 2%/-1.5% targets | SPY v1.5: 0.3%/-0.4% targets (6.7× reduction)\n\n";

    for (size_t i = 0; i < std::min(signals.size(), bars.size()); ++i) {
        const auto& signal = signals[i];
        const auto& bar = bars[i];

        // Check for End-of-Day (EOD) closing time: 15:58 ET (2 minutes before market close)
        // Convert timestamp_ms to ET and extract hour/minute
        std::time_t bar_time = static_cast<std::time_t>(bar.timestamp_ms / 1000);
        std::tm tm_utc{};
        #ifdef _WIN32
            gmtime_s(&tm_utc, &bar_time);
        #else
            gmtime_r(&bar_time, &tm_utc);
        #endif

        // Convert UTC to ET (subtract 4 hours for EDT, 5 for EST)
        // For simplicity, use 4 hours (EDT) since most trading happens in summer
        int et_hour = tm_utc.tm_hour - 4;
        if (et_hour < 0) et_hour += 24;
        int et_minute = tm_utc.tm_min;

        // Check if time >= 15:58 ET
        bool is_eod_close = (et_hour == 15 && et_minute >= 58) || (et_hour >= 16);

        // Update position tracking
        current_position.bars_held++;
        double current_equity = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
        double position_pnl_pct = (current_equity - current_position.entry_equity) / current_position.entry_equity;

        // Check profit-taking condition
        bool should_take_profit = (position_pnl_pct >= PROFIT_TARGET &&
                                   current_position.state != PositionStateMachine::State::CASH_ONLY);

        // Check stop-loss condition
        bool should_stop_loss = (position_pnl_pct <= STOP_LOSS &&
                                current_position.state != PositionStateMachine::State::CASH_ONLY);

        // Check maximum hold period
        bool should_reevaluate = (current_position.bars_held >= MAX_HOLD_BARS);

        // Force exit to cash if profit target hit or stop loss triggered
        PositionStateMachine::State forced_target_state = PositionStateMachine::State::INVALID;
        std::string exit_reason = "";

        if (is_eod_close && current_position.state != PositionStateMachine::State::CASH_ONLY) {
            // EOD close takes priority over all other conditions
            forced_target_state = PositionStateMachine::State::CASH_ONLY;
            exit_reason = "EOD_CLOSE (15:58 ET)";
        } else if (should_take_profit) {
            forced_target_state = PositionStateMachine::State::CASH_ONLY;
            exit_reason = "PROFIT_TARGET (" + std::to_string(position_pnl_pct * 100) + "%)";
        } else if (should_stop_loss) {
            forced_target_state = PositionStateMachine::State::CASH_ONLY;
            exit_reason = "STOP_LOSS (" + std::to_string(position_pnl_pct * 100) + "%)";
        } else if (should_reevaluate) {
            exit_reason = "MAX_HOLD_PERIOD";
            // Don't force cash, but allow PSM to reevaluate
        }

        // USE OPTIMIZED THRESHOLDS - Simple binary model
        // This allows Optuna to directly control trading behavior
        PositionStateMachine::State target_state;

        // Block new position entries after 15:58 ET (EOD close time)
        if (is_eod_close) {
            // Force CASH_ONLY - do not enter any new positions
            target_state = PositionStateMachine::State::CASH_ONLY;
        } else if (signal.probability >= 0.65) {
            // ULTRA BULLISH (>= 0.65) - Use 3x leverage bull ETF
            target_state = PositionStateMachine::State::TQQQ_ONLY;
        } else if (signal.probability >= buy_threshold) {
            // LONG signal (>= buy_threshold) - Use base instrument (1x)
            target_state = PositionStateMachine::State::QQQ_ONLY;
        } else if (signal.probability <= 0.35) {
            // ULTRA BEARISH (<= 0.35) - Use 3x leverage bear ETF
            target_state = PositionStateMachine::State::SQQQ_ONLY;
        } else if (signal.probability <= sell_threshold) {
            // SHORT signal (<= sell_threshold) - Use inverse ETF (-1x)
            target_state = PositionStateMachine::State::PSQ_ONLY;
        } else {
            // NEUTRAL (between thresholds) - stay in cash
            target_state = PositionStateMachine::State::CASH_ONLY;
        }

        // Prepare transition structure
        PositionStateMachine::StateTransition transition;
        transition.current_state = current_position.state;
        transition.target_state = target_state;

        // Override with forced exit if needed
        if (forced_target_state != PositionStateMachine::State::INVALID) {
            transition.target_state = forced_target_state;
            transition.optimal_action = exit_reason;
        }

        // Apply minimum hold period (prevent flip-flop)
        if (current_position.bars_held < MIN_HOLD_BARS &&
            transition.current_state != PositionStateMachine::State::CASH_ONLY &&
            forced_target_state == PositionStateMachine::State::INVALID) {
            // Keep current state
            transition.target_state = transition.current_state;
        }

        // Debug: Log state transitions
        if (verbose && i % 500 == 0) {
            std::cout << "  [" << i << "] Signal: " << signal.probability
                     << " | Current: " << psm.state_to_string(transition.current_state)
                     << " | Target: " << psm.state_to_string(transition.target_state)
                     << " | PnL: " << (position_pnl_pct * 100) << "%"
                     << " | Cash: $" << std::fixed << std::setprecision(2) << portfolio.cash_balance << "\n";
        }

        // Execute state transition
        if (transition.target_state != transition.current_state) {
            if (verbose && i % 100 == 0) {
                std::cerr << "DEBUG [" << i << "]: State transition detected\n"
                          << "  Current=" << static_cast<int>(transition.current_state)
                          << " (" << psm.state_to_string(transition.current_state) << ")\n"
                          << "  Target=" << static_cast<int>(transition.target_state)
                          << " (" << psm.state_to_string(transition.target_state) << ")\n"
                          << "  Cash=$" << portfolio.cash_balance << "\n";
            }

            // Calculate positions for target state (using multi-instrument prices)
            double total_capital = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
            std::map<std::string, double> target_positions =
                calculate_target_positions_multi(transition.target_state, total_capital, instrument_bars, i, symbol_map);

            // PHASE 1: Execute all SELL orders first to free up cash
            // First, sell any positions NOT in target state
            // Create a copy of position symbols to avoid iterator invalidation
            std::vector<std::string> current_symbols;
            for (const auto& [symbol, position] : portfolio.positions) {
                current_symbols.push_back(symbol);
            }

            for (const std::string& symbol : current_symbols) {
                if (portfolio.positions.count(symbol) == 0) continue;  // Already sold

                if (target_positions.count(symbol) == 0 || target_positions[symbol] == 0) {
                    // This position should be fully liquidated
                    double sell_quantity = portfolio.positions[symbol].quantity;

                    if (sell_quantity > 0) {
                        // Use correct instrument price
                        double instrument_price = get_instrument_price(instrument_bars, symbol, i);
                        portfolio.cash_balance += sell_quantity * instrument_price;

                        // Erase position FIRST
                        portfolio.positions.erase(symbol);

                        // Now record trade with correct portfolio value
                        TradeRecord trade;
                        trade.bar_id = bar.bar_id;
                        trade.timestamp_ms = bar.timestamp_ms;
                        trade.bar_index = i;
                        trade.symbol = symbol;
                        trade.action = TradeAction::SELL;
                        trade.quantity = sell_quantity;
                        trade.price = instrument_price;
                        trade.trade_value = sell_quantity * instrument_price;
                        trade.fees = 0.0;
                        trade.cash_balance = portfolio.cash_balance;
                        trade.portfolio_value = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
                        trade.position_quantity = 0.0;
                        trade.position_avg_price = 0.0;
                        // Use forced exit reason if set (EOD_CLOSE, PROFIT_TARGET, STOP_LOSS)
                        if (!transition.optimal_action.empty()) {
                            trade.reason = transition.optimal_action;
                        } else {
                            trade.reason = "PSM: " + psm.state_to_string(transition.current_state) +
                                         " -> " + psm.state_to_string(transition.target_state) +
                                         " (p=" + std::to_string(signal.probability).substr(0, 6) + ")";
                        }

                        history.trades.push_back(trade);

                        if (verbose) {
                            std::cout << "  [" << i << "] " << symbol << " SELL "
                                     << sell_quantity << " @ $" << instrument_price
                                     << " | Portfolio: $" << trade.portfolio_value << "\n";
                        }
                    }
                }
            }

            // Then, reduce positions that are in both current and target but need downsizing
            for (const auto& [symbol, target_shares] : target_positions) {
                double current_shares = portfolio.positions.count(symbol) ?
                                       portfolio.positions[symbol].quantity : 0.0;
                double delta_shares = target_shares - current_shares;

                // Only process SELL orders in this phase
                if (delta_shares < -0.01) {  // Selling (delta is negative)
                    double quantity = std::abs(delta_shares);
                    double sell_quantity = std::min(quantity, portfolio.positions[symbol].quantity);

                    if (sell_quantity > 0) {
                        double instrument_price = get_instrument_price(instrument_bars, symbol, i);
                        portfolio.cash_balance += sell_quantity * instrument_price;
                        portfolio.positions[symbol].quantity -= sell_quantity;

                        if (portfolio.positions[symbol].quantity < 0.01) {
                            portfolio.positions.erase(symbol);
                        }

                        // Record trade
                        TradeRecord trade;
                        trade.bar_id = bar.bar_id;
                        trade.timestamp_ms = bar.timestamp_ms;
                        trade.bar_index = i;
                        trade.symbol = symbol;
                        trade.action = TradeAction::SELL;
                        trade.quantity = sell_quantity;
                        trade.price = instrument_price;
                        trade.trade_value = sell_quantity * instrument_price;
                        trade.fees = 0.0;
                        trade.cash_balance = portfolio.cash_balance;
                        trade.portfolio_value = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
                        trade.position_quantity = portfolio.positions.count(symbol) ? portfolio.positions[symbol].quantity : 0.0;
                        trade.position_avg_price = portfolio.positions.count(symbol) ? portfolio.positions[symbol].avg_price : 0.0;
                        // Use forced exit reason if set (EOD_CLOSE, PROFIT_TARGET, STOP_LOSS)
                        if (!transition.optimal_action.empty()) {
                            trade.reason = transition.optimal_action;
                        } else {
                            trade.reason = "PSM: " + psm.state_to_string(transition.current_state) +
                                         " -> " + psm.state_to_string(transition.target_state) +
                                         " (p=" + std::to_string(signal.probability).substr(0, 6) + ")";
                        }

                        history.trades.push_back(trade);

                        if (verbose) {
                            std::cout << "  [" << i << "] " << symbol << " SELL "
                                     << sell_quantity << " @ $" << instrument_price
                                     << " | Portfolio: $" << trade.portfolio_value << "\n";
                        }
                    }
                }
            }

            // PHASE 2: Execute all BUY orders with freed-up cash
            for (const auto& [symbol, target_shares] : target_positions) {
                double current_shares = portfolio.positions.count(symbol) ?
                                       portfolio.positions[symbol].quantity : 0.0;
                double delta_shares = target_shares - current_shares;

                // Only process BUY orders in this phase
                if (delta_shares > 0.01) {  // Buying (delta is positive)
                    double quantity = std::abs(delta_shares);
                    double instrument_price = get_instrument_price(instrument_bars, symbol, i);
                    double trade_value = quantity * instrument_price;

                    // Execute BUY trade
                    if (trade_value <= portfolio.cash_balance) {
                        portfolio.cash_balance -= trade_value;
                        portfolio.positions[symbol].quantity += quantity;
                        portfolio.positions[symbol].avg_price = instrument_price;
                        portfolio.positions[symbol].symbol = symbol;

                        // Record trade
                        TradeRecord trade;
                        trade.bar_id = bar.bar_id;
                        trade.timestamp_ms = bar.timestamp_ms;
                        trade.bar_index = i;
                        trade.symbol = symbol;
                        trade.action = TradeAction::BUY;
                        trade.quantity = quantity;
                        trade.price = instrument_price;
                        trade.trade_value = trade_value;
                        trade.fees = 0.0;
                        trade.cash_balance = portfolio.cash_balance;
                        trade.portfolio_value = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
                        trade.position_quantity = portfolio.positions[symbol].quantity;
                        trade.position_avg_price = portfolio.positions[symbol].avg_price;
                        // Use forced exit reason if set (EOD_CLOSE, PROFIT_TARGET, STOP_LOSS)
                        if (!transition.optimal_action.empty()) {
                            trade.reason = transition.optimal_action;
                        } else {
                            trade.reason = "PSM: " + psm.state_to_string(transition.current_state) +
                                         " -> " + psm.state_to_string(transition.target_state) +
                                         " (p=" + std::to_string(signal.probability).substr(0, 6) + ")";
                        }

                        history.trades.push_back(trade);

                        if (verbose) {
                            std::cout << "  [" << i << "] " << symbol << " BUY "
                                     << quantity << " @ $" << instrument_price
                                     << " | Portfolio: $" << trade.portfolio_value << "\n";
                        }
                    } else {
                        // Cash balance insufficient - log the blocked trade
                        if (verbose) {
                            std::cerr << "  [" << i << "] " << symbol << " BUY BLOCKED"
                                      << " | Required: $" << std::fixed << std::setprecision(2) << trade_value
                                      << " | Available: $" << portfolio.cash_balance << "\n";
                        }
                    }
                }
            }

            // Reset position tracking on state change
            current_position.entry_price = bars[i].close;  // Use QQQ price as reference
            current_position.entry_equity = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
            current_position.bars_held = 0;
            current_position.state = transition.target_state;
        }

        // Update portfolio total equity
        portfolio.total_equity = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);

        // Record equity curve
        history.equity_curve.push_back(portfolio.total_equity);

        // Calculate drawdown
        double peak = *std::max_element(history.equity_curve.begin(), history.equity_curve.end());
        double drawdown = (peak - portfolio.total_equity) / peak;
        history.drawdown_curve.push_back(drawdown);
        history.max_drawdown = std::max(history.max_drawdown, drawdown);
    }

    history.final_capital = portfolio.total_equity;
    history.total_trades = static_cast<int>(history.trades.size());

    // Calculate win rate
    for (const auto& trade : history.trades) {
        if (trade.action == TradeAction::SELL) {
            double pnl = (trade.price - trade.position_avg_price) * trade.quantity;
            if (pnl > 0) history.winning_trades++;
        }
    }

    std::cout << "\nTrade execution complete!\n";
    std::cout << "Total trades: " << history.total_trades << "\n";
    std::cout << "Final capital: $" << std::fixed << std::setprecision(2) << history.final_capital << "\n";
    std::cout << "Total return: " << ((history.final_capital / history.starting_capital - 1.0) * 100) << "%\n";
    std::cout << "Max drawdown: " << (history.max_drawdown * 100) << "%\n\n";

    // Save trade history
    std::cout << "Saving trade history to " << output_path << "...\n";
    if (csv_output) {
        save_trades_csv(history, output_path);
    } else {
        save_trades_jsonl(history, output_path);
    }

    // Save equity curve
    std::string equity_path = output_path.substr(0, output_path.find_last_of('.')) + "_equity.csv";
    save_equity_curve(history, equity_path);

    std::cout << "✅ Trade execution complete!\n";
    return 0;
}

// Helper function: Calculate total value of all positions
double ExecuteTradesCommand::get_position_value(const PortfolioState& portfolio, double current_price) {
    // Legacy function - DO NOT USE for multi-instrument portfolios
    // Use get_position_value_multi() instead
    double total = 0.0;
    for (const auto& [symbol, position] : portfolio.positions) {
        total += position.quantity * current_price;
    }
    return total;
}

// Multi-instrument position value calculation
double ExecuteTradesCommand::get_position_value_multi(
    const PortfolioState& portfolio,
    const std::map<std::string, std::vector<Bar>>& instrument_bars,
    size_t bar_index) {

    double total = 0.0;
    for (const auto& [symbol, position] : portfolio.positions) {
        if (instrument_bars.count(symbol) > 0 && bar_index < instrument_bars.at(symbol).size()) {
            double current_price = instrument_bars.at(symbol)[bar_index].close;
            total += position.quantity * current_price;
        }
    }
    return total;
}

// Helper function: Calculate target positions for each PSM state (LEGACY - single price)
std::map<std::string, double> ExecuteTradesCommand::calculate_target_positions(
    PositionStateMachine::State state,
    double total_capital,
    double price) {

    std::map<std::string, double> positions;

    switch (state) {
        case PositionStateMachine::State::CASH_ONLY:
            // No positions - all cash
            break;

        case PositionStateMachine::State::QQQ_ONLY:
            // 100% in QQQ (moderate long)
            positions["QQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::TQQQ_ONLY:
            // 100% in TQQQ (strong long, 3x leverage)
            positions["TQQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::PSQ_ONLY:
            // 100% in PSQ (moderate short, -1x)
            positions["PSQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::SQQQ_ONLY:
            // 100% in SQQQ (strong short, -3x)
            positions["SQQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::QQQ_TQQQ:
            // Split: 50% QQQ + 50% TQQQ (blended long)
            positions["QQQ"] = (total_capital * 0.5) / price;
            positions["TQQQ"] = (total_capital * 0.5) / price;
            break;

        case PositionStateMachine::State::PSQ_SQQQ:
            // Split: 50% PSQ + 50% SQQQ (blended short)
            positions["PSQ"] = (total_capital * 0.5) / price;
            positions["SQQQ"] = (total_capital * 0.5) / price;
            break;

        default:
            // INVALID or unknown state - go to cash
            break;
    }

    return positions;
}

// Multi-instrument position calculation - uses correct price for each instrument
std::map<std::string, double> ExecuteTradesCommand::calculate_target_positions_multi(
    PositionStateMachine::State state,
    double total_capital,
    const std::map<std::string, std::vector<Bar>>& instrument_bars,
    size_t bar_index,
    const SymbolMap& symbol_map) {

    std::map<std::string, double> positions;

    switch (state) {
        case PositionStateMachine::State::CASH_ONLY:
            // No positions - all cash
            break;

        case PositionStateMachine::State::QQQ_ONLY:
            // 100% in base symbol (moderate long, 1x)
            if (instrument_bars.count(symbol_map.base) && bar_index < instrument_bars.at(symbol_map.base).size()) {
                positions[symbol_map.base] = total_capital / instrument_bars.at(symbol_map.base)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::TQQQ_ONLY:
            // 100% in leveraged bull (strong long, 3x leverage)
            if (instrument_bars.count(symbol_map.bull_3x) && bar_index < instrument_bars.at(symbol_map.bull_3x).size()) {
                positions[symbol_map.bull_3x] = total_capital / instrument_bars.at(symbol_map.bull_3x)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::PSQ_ONLY:
            // 100% in moderate bear (moderate short, -1x)
            if (instrument_bars.count(symbol_map.bear_1x) && bar_index < instrument_bars.at(symbol_map.bear_1x).size()) {
                positions[symbol_map.bear_1x] = total_capital / instrument_bars.at(symbol_map.bear_1x)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::SQQQ_ONLY:
            // 100% in leveraged bear (strong short, -2x or -3x)
            if (instrument_bars.count(symbol_map.bear_nx) && bar_index < instrument_bars.at(symbol_map.bear_nx).size()) {
                positions[symbol_map.bear_nx] = total_capital / instrument_bars.at(symbol_map.bear_nx)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::QQQ_TQQQ:
            // Split: 50% base + 50% leveraged bull (blended long)
            if (instrument_bars.count(symbol_map.base) && bar_index < instrument_bars.at(symbol_map.base).size()) {
                positions[symbol_map.base] = (total_capital * 0.5) / instrument_bars.at(symbol_map.base)[bar_index].close;
            }
            if (instrument_bars.count(symbol_map.bull_3x) && bar_index < instrument_bars.at(symbol_map.bull_3x).size()) {
                positions[symbol_map.bull_3x] = (total_capital * 0.5) / instrument_bars.at(symbol_map.bull_3x)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::PSQ_SQQQ:
            // Split: 50% moderate bear + 50% leveraged bear (blended short)
            if (instrument_bars.count(symbol_map.bear_1x) && bar_index < instrument_bars.at(symbol_map.bear_1x).size()) {
                positions[symbol_map.bear_1x] = (total_capital * 0.5) / instrument_bars.at(symbol_map.bear_1x)[bar_index].close;
            }
            if (instrument_bars.count(symbol_map.bear_nx) && bar_index < instrument_bars.at(symbol_map.bear_nx).size()) {
                positions[symbol_map.bear_nx] = (total_capital * 0.5) / instrument_bars.at(symbol_map.bear_nx)[bar_index].close;
            }
            break;

        default:
            // INVALID or unknown state - go to cash
            break;
    }

    return positions;
}

void ExecuteTradesCommand::save_trades_jsonl(const PortfolioHistory& history,
                                            const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    for (const auto& trade : history.trades) {
        out << "{"
            << "\"bar_id\":" << trade.bar_id << ","
            << "\"timestamp_ms\":" << trade.timestamp_ms << ","
            << "\"bar_index\":" << trade.bar_index << ","
            << "\"symbol\":\"" << trade.symbol << "\","
            << "\"action\":\"" << (trade.action == TradeAction::BUY ? "BUY" : "SELL") << "\","
            << "\"quantity\":" << std::fixed << std::setprecision(4) << trade.quantity << ","
            << "\"price\":" << std::setprecision(2) << trade.price << ","
            << "\"trade_value\":" << trade.trade_value << ","
            << "\"fees\":" << trade.fees << ","
            << "\"cash_balance\":" << trade.cash_balance << ","
            << "\"portfolio_value\":" << trade.portfolio_value << ","
            << "\"position_quantity\":" << trade.position_quantity << ","
            << "\"position_avg_price\":" << trade.position_avg_price << ","
            << "\"reason\":\"" << trade.reason << "\""
            << "}\n";
    }
}

void ExecuteTradesCommand::save_trades_csv(const PortfolioHistory& history,
                                          const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    // Header
    out << "bar_id,timestamp_ms,bar_index,symbol,action,quantity,price,trade_value,fees,"
        << "cash_balance,portfolio_value,position_quantity,position_avg_price,reason\n";

    // Data
    for (const auto& trade : history.trades) {
        out << trade.bar_id << ","
            << trade.timestamp_ms << ","
            << trade.bar_index << ","
            << trade.symbol << ","
            << (trade.action == TradeAction::BUY ? "BUY" : "SELL") << ","
            << std::fixed << std::setprecision(4) << trade.quantity << ","
            << std::setprecision(2) << trade.price << ","
            << trade.trade_value << ","
            << trade.fees << ","
            << trade.cash_balance << ","
            << trade.portfolio_value << ","
            << trade.position_quantity << ","
            << trade.position_avg_price << ","
            << "\"" << trade.reason << "\"\n";
    }
}

void ExecuteTradesCommand::save_equity_curve(const PortfolioHistory& history,
                                            const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open equity curve file: " + path);
    }

    // Header
    out << "bar_index,equity,drawdown\n";

    // Data
    for (size_t i = 0; i < history.equity_curve.size(); ++i) {
        double drawdown = (i < history.drawdown_curve.size()) ? history.drawdown_curve[i] : 0.0;
        out << i << ","
            << std::fixed << std::setprecision(2) << history.equity_curve[i] << ","
            << std::setprecision(4) << drawdown << "\n";
    }
}

void ExecuteTradesCommand::show_help() const {
    std::cout << R"(
Execute OnlineEnsemble Trades
==============================

Execute trades from signal file and generate portfolio history.

USAGE:
    sentio_cli execute-trades --signals <path> --data <path> [OPTIONS]

REQUIRED:
    --signals <path>           Path to signal file (JSONL or CSV)
    --data <path>              Path to market data file

OPTIONS:
    --output <path>            Output trade file (default: trades.jsonl)
    --capital <amount>         Starting capital (default: 100000)
    --buy-threshold <val>      Buy signal threshold (default: 0.53)
    --sell-threshold <val>     Sell signal threshold (default: 0.47)
    --no-kelly                 Disable Kelly criterion sizing
    --csv                      Output in CSV format
    --verbose, -v              Show each trade

PSM RISK MANAGEMENT (Optuna-optimizable):
    --profit-target <val>      Profit target % (default: 0.003 = 0.3%)
    --stop-loss <val>          Stop loss % (default: -0.004 = -0.4%)
    --min-hold-bars <n>        Min holding period (default: 3 bars)
    --max-hold-bars <n>        Max holding period (default: 100 bars)

EXAMPLES:
    # Execute trades with default settings
    sentio_cli execute-trades --signals signals.jsonl --data data/SPY.csv

    # Custom capital and thresholds
    sentio_cli execute-trades --signals signals.jsonl --data data/QQQ.bin \
        --capital 50000 --buy-threshold 0.55 --sell-threshold 0.45

    # Verbose mode with CSV output
    sentio_cli execute-trades --signals signals.jsonl --data data/futures.bin \
        --verbose --csv --output trades.csv

    # Custom PSM parameters (for Optuna optimization)
    sentio_cli execute-trades --signals signals.jsonl --data data/SPY.csv \
        --profit-target 0.005 --stop-loss -0.006 --min-hold-bars 5

OUTPUT FILES:
    - trades.jsonl (or .csv)   Trade-by-trade history
    - trades_equity.csv        Equity curve and drawdowns

)" << std::endl;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 5 of 6**: src/cli/live_trade_command.cpp

**File Information**:
- **Path**: `src/cli/live_trade_command.cpp`
- **Size**: 2345 lines
- **Modified**: 2025-10-16 06:18:20
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/live_trade_command.hpp"
#include "live/alpaca_client.hpp"
#include "live/polygon_client.hpp"
#include "live/position_book.h"
#include "live/broker_client_interface.h"
#include "live/bar_feed_interface.h"
#include "live/mock_broker.h"
#include "live/mock_bar_feed_replay.h"
#include "live/alpaca_client_adapter.h"
#include "live/polygon_client_adapter.h"
// #include "live/alpaca_rest_bar_feed.h"  // DISABLED: incomplete implementation
#include "live/mock_config.h"
#include "live/state_persistence.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/position_state_machine.h"
#include "common/time_utils.h"
#include "common/bar_validator.h"
#include "common/exceptions.h"
#include "common/eod_state.h"
#include "common/nyse_calendar.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <optional>
#include <memory>
#include <csignal>
#include <atomic>

namespace sentio {
namespace cli {

// Global pointer for signal handler (necessary for C-style signal handlers)
static std::atomic<bool> g_shutdown_requested{false};

/**
 * Create OnlineEnsemble v1.0 configuration with asymmetric thresholds
 * Target: 0.6086% MRB (10.5% monthly, 125% annual)
 *
 * Now loads optimized parameters from midday_selected_params.json if available
 */
static OnlineEnsembleStrategy::OnlineEnsembleConfig create_v1_config(bool is_mock = false) {
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;

    // Default v1.0 parameters
    config.buy_threshold = 0.55;
    config.sell_threshold = 0.45;
    config.neutral_zone = 0.10;
    config.ewrls_lambda = 0.995;
    config.warmup_samples = is_mock ? 780 : 7800;  // Mock: 2 blocks, Live: 20 blocks
    config.enable_bb_amplification = true;
    config.bb_amplification_factor = 0.10;
    config.bb_period = 20;
    config.bb_std_dev = 2.0;
    config.bb_proximity_threshold = 0.30;
    config.regularization = 0.01;
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;
    config.enable_regime_detection = false;
    config.regime_check_interval = 60;

    // Try to load optimized parameters from JSON file
    std::string json_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/midday_selected_params.json";
    std::ifstream file(json_file);

    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            file.close();

            // Load phase 1 parameters
            config.buy_threshold = j.value("buy_threshold", config.buy_threshold);
            config.sell_threshold = j.value("sell_threshold", config.sell_threshold);
            config.bb_amplification_factor = j.value("bb_amplification_factor", config.bb_amplification_factor);
            config.ewrls_lambda = j.value("ewrls_lambda", config.ewrls_lambda);

            // Load phase 2 parameters
            double h1 = j.value("h1_weight", 0.3);
            double h5 = j.value("h5_weight", 0.5);
            double h10 = j.value("h10_weight", 0.2);
            config.horizon_weights = {h1, h5, h10};
            config.bb_period = j.value("bb_period", config.bb_period);
            config.bb_std_dev = j.value("bb_std_dev", config.bb_std_dev);
            config.bb_proximity_threshold = j.value("bb_proximity", config.bb_proximity_threshold);
            config.regularization = j.value("regularization", config.regularization);

            std::cout << "✅ Loaded optimized parameters from: " << json_file << std::endl;
            std::cout << "   Source: " << j.value("source", "unknown") << std::endl;
            std::cout << "   MRB target: " << j.value("expected_mrb", 0.0) << "%" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Failed to load optimized parameters: " << e.what() << std::endl;
            std::cerr << "   Using default configuration" << std::endl;
        }
    }

    return config;
}

/**
 * Load leveraged ETF prices from CSV files for mock mode
 * Returns: map[timestamp_sec][symbol] -> close_price
 */
static std::unordered_map<uint64_t, std::unordered_map<std::string, double>>
load_leveraged_prices(const std::string& base_path) {
    std::unordered_map<uint64_t, std::unordered_map<std::string, double>> prices;

    std::vector<std::string> symbols = {"SH", "SDS", "SPXL"};

    for (const auto& symbol : symbols) {
        std::string filepath = base_path + "/" + symbol + "_yesterday.csv";
        std::ifstream file(filepath);

        if (!file.is_open()) {
            std::cerr << "⚠️  Warning: Could not load " << filepath << std::endl;
            continue;
        }

        std::string line;
        int line_count = 0;
        while (std::getline(file, line)) {
            // Skip empty lines or header-like lines
            if (line.empty() ||
                line.find("timestamp") != std::string::npos ||
                line.find("ts_utc") != std::string::npos ||
                line.find("ts_nyt_epoch") != std::string::npos) {
                continue;
            }

            std::istringstream iss(line);
            std::string date_str, ts_str, o, h, l, close_str, v;

            if (std::getline(iss, date_str, ',') &&
                std::getline(iss, ts_str, ',') &&
                std::getline(iss, o, ',') &&
                std::getline(iss, h, ',') &&
                std::getline(iss, l, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, v)) {

                uint64_t timestamp_sec = std::stoull(ts_str);
                double close_price = std::stod(close_str);

                prices[timestamp_sec][symbol] = close_price;
                line_count++;
            }
        }

        if (line_count > 0) {
            std::cout << "✅ Loaded " << line_count << " bars for " << symbol << std::endl;
        }
    }

    return prices;
}

/**
 * Micro-Adaptation State Tracker
 *
 * Tracks and applies small parameter adjustments during live trading
 * based on recent performance and market conditions.
 *
 * Key Features:
 * - Hourly threshold drift (±0.002 max per hour, ±1% max total drift)
 * - Volatility-based lambda adaptation (every 30 bars)
 * - Performance-based adjustments (win rate tracking)
 * - Saves baseline from morning optimization
 */
struct MicroAdaptationState {
    // Baseline parameters from morning optimization
    double baseline_buy_threshold{0.511};
    double baseline_sell_threshold{0.47};
    double baseline_lambda{0.984};

    // Current adapted values
    double current_buy_threshold{0.511};
    double current_sell_threshold{0.47};
    double current_lambda{0.984};

    // Adaptation tracking
    int bars_since_last_threshold_adapt{0};
    int bars_since_last_lambda_adapt{0};
    int adaptation_hour{0};  // Track which hour we're in

    // Performance tracking for adaptation
    int recent_wins{0};
    int recent_losses{0};
    int trades_this_period{0};
    std::vector<double> recent_returns;  // Last 30 bars for volatility calc

    // Adaptation limits
    static constexpr double MAX_HOURLY_DRIFT = 0.002;  // ±0.2% per hour
    static constexpr double MAX_TOTAL_DRIFT = 0.01;     // ±1% total drift from baseline
    static constexpr int THRESHOLD_ADAPT_INTERVAL = 60; // 60 bars = 1 hour
    static constexpr int LAMBDA_ADAPT_INTERVAL = 30;     // 30 bars = 30 minutes

    bool enabled{false};  // Micro-adaptations on/off

    /**
     * Load baseline parameters from morning optimization
     */
    bool load_baseline(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "⚠️  Could not load baseline params from: " << filepath << std::endl;
            return false;
        }

        try {
            nlohmann::json j;
            file >> j;
            file.close();

            baseline_buy_threshold = j.value("buy_threshold", 0.511);
            baseline_sell_threshold = j.value("sell_threshold", 0.47);
            baseline_lambda = j.value("ewrls_lambda", 0.984);

            // Initialize current values to baseline
            current_buy_threshold = baseline_buy_threshold;
            current_sell_threshold = baseline_sell_threshold;
            current_lambda = baseline_lambda;

            std::cout << "✅ Loaded baseline parameters for micro-adaptation:" << std::endl;
            std::cout << "   buy_threshold: " << baseline_buy_threshold << std::endl;
            std::cout << "   sell_threshold: " << baseline_sell_threshold << std::endl;
            std::cout << "   lambda: " << baseline_lambda << std::endl;

            return true;
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Failed to parse baseline params: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * Perform hourly threshold micro-adaptation
     * Adjusts thresholds by ±0.002 based on recent win rate
     */
    void adapt_thresholds_hourly() {
        if (!enabled) return;

        bars_since_last_threshold_adapt = 0;
        adaptation_hour++;

        // Calculate win rate from recent trades
        int total_trades = recent_wins + recent_losses;
        if (total_trades < 5) {
            // Not enough data - no adaptation
            return;
        }

        double win_rate = static_cast<double>(recent_wins) / total_trades;

        // Adaptation logic:
        // - High win rate (>60%) → tighten thresholds (more aggressive)
        // - Low win rate (<40%) → widen thresholds (more conservative)
        // - Neutral (40-60%) → small random walk

        double drift_amount = 0.0;
        if (win_rate > 0.60) {
            // Winning strategy - tighten thresholds
            drift_amount = -MAX_HOURLY_DRIFT;  // Move closer together
        } else if (win_rate < 0.40) {
            // Losing strategy - widen thresholds
            drift_amount = +MAX_HOURLY_DRIFT;  // Move further apart
        } else {
            // Neutral - small random exploration (±0.001)
            drift_amount = ((std::rand() % 2) * 2 - 1) * (MAX_HOURLY_DRIFT * 0.5);
        }

        // Apply drift with max total drift limit
        double new_buy = current_buy_threshold + drift_amount;
        double new_sell = current_sell_threshold - drift_amount;

        // Enforce max total drift from baseline
        if (std::abs(new_buy - baseline_buy_threshold) <= MAX_TOTAL_DRIFT) {
            current_buy_threshold = new_buy;
        }
        if (std::abs(new_sell - baseline_sell_threshold) <= MAX_TOTAL_DRIFT) {
            current_sell_threshold = new_sell;
        }

        // Ensure thresholds don't cross
        if (current_buy_threshold <= current_sell_threshold) {
            current_buy_threshold = baseline_buy_threshold;
            current_sell_threshold = baseline_sell_threshold;
        }

        // Reset performance counters for next period
        recent_wins = 0;
        recent_losses = 0;
        trades_this_period = 0;
    }

    /**
     * Perform volatility-based lambda adaptation
     * Higher volatility → lower lambda (faster adaptation)
     */
    void adapt_lambda_on_volatility() {
        if (!enabled) return;
        if (recent_returns.size() < 20) return;  // Need minimum data

        bars_since_last_lambda_adapt = 0;

        // Calculate recent return volatility
        double mean = 0.0;
        for (double ret : recent_returns) {
            mean += ret;
        }
        mean /= recent_returns.size();

        double variance = 0.0;
        for (double ret : recent_returns) {
            variance += (ret - mean) * (ret - mean);
        }
        double volatility = std::sqrt(variance / recent_returns.size());

        // Adaptation logic:
        // - High volatility (>0.015) → lower lambda (0.980-0.985) for fast adaptation
        // - Low volatility (<0.005) → higher lambda (0.990-0.995) for stability
        // - Normal volatility → baseline lambda

        if (volatility > 0.015) {
            // High volatility - fast adaptation
            current_lambda = std::max(baseline_lambda - 0.005, 0.980);
        } else if (volatility < 0.005) {
            // Low volatility - stable learning
            current_lambda = std::min(baseline_lambda + 0.005, 0.995);
        } else {
            // Normal volatility - use baseline
            current_lambda = baseline_lambda;
        }
    }

    /**
     * Update adaptation state after each bar
     */
    void update_on_bar(double bar_return) {
        bars_since_last_threshold_adapt++;
        bars_since_last_lambda_adapt++;

        // Track recent returns for volatility calculation
        recent_returns.push_back(bar_return);
        if (recent_returns.size() > 30) {
            recent_returns.erase(recent_returns.begin());
        }

        // Trigger hourly threshold adaptation
        if (bars_since_last_threshold_adapt >= THRESHOLD_ADAPT_INTERVAL) {
            adapt_thresholds_hourly();
        }

        // Trigger volatility-based lambda adaptation
        if (bars_since_last_lambda_adapt >= LAMBDA_ADAPT_INTERVAL) {
            adapt_lambda_on_volatility();
        }
    }

    /**
     * Record trade outcome for performance tracking
     */
    void record_trade(bool won) {
        if (won) {
            recent_wins++;
        } else {
            recent_losses++;
        }
        trades_this_period++;
    }
};

/**
 * Live Trading Runner for OnlineEnsemble Strategy v1.0
 *
 * - Trades SPY/SDS/SPXL/SH during regular hours (9:30am - 4:00pm ET)
 * - Uses OnlineEnsemble EWRLS with asymmetric thresholds
 * - Comprehensive logging of all decisions and trades
 */
class LiveTrader {
public:
    LiveTrader(std::unique_ptr<IBrokerClient> broker,
               std::unique_ptr<IBarFeed> bar_feed,
               const std::string& log_dir,
               bool is_mock_mode = false,
               const std::string& data_file = "")
        : broker_(std::move(broker))
        , bar_feed_(std::move(bar_feed))
        , log_dir_(log_dir)
        , is_mock_mode_(is_mock_mode)
        , data_file_(data_file)
        , strategy_(create_v1_config(is_mock_mode))
        , psm_()
        , current_state_(PositionStateMachine::State::CASH_ONLY)
        , bars_held_(0)
        , entry_equity_(100000.0)
        , previous_portfolio_value_(100000.0)  // Initialize to starting equity
        , et_time_()  // Initialize ET time manager
        , eod_state_(log_dir + "/eod_state.txt")  // Persistent EOD tracking
        , nyse_calendar_()  // NYSE holiday calendar
        , state_persistence_(std::make_unique<StatePersistence>(log_dir + "/state"))  // State persistence
    {
        // Initialize log files
        init_logs();

        // SPY trading configuration (maps to sentio PSM states)
        symbol_map_ = {
            {"SPY", "SPY"},      // Base 1x
            {"SPXL", "SPXL"},    // Bull 3x
            {"SH", "SH"},        // Bear -1x
            {"SDS", "SDS"}       // Bear -2x
        };
    }

    ~LiveTrader() {
        // Generate dashboard on exit
        generate_dashboard();
    }

    void run() {
        if (is_mock_mode_) {
            log_system("=== OnlineTrader v1.0 Mock Trading Started ===");
            log_system("Mode: MOCK REPLAY (39x speed)");
        } else {
            log_system("=== OnlineTrader v1.0 Live Paper Trading Started ===");
            log_system("Mode: LIVE TRADING");
        }
        log_system("Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)");
        log_system("Trading Hours: 9:30am - 4:00pm ET (Regular Hours Only)");
        log_system("Strategy: OnlineEnsemble EWRLS with Asymmetric Thresholds");
        log_system("");

        // Connect to broker (Alpaca or Mock)
        log_system(is_mock_mode_ ? "Initializing Mock Broker..." : "Connecting to Alpaca Paper Trading...");
        auto account = broker_->get_account();
        if (!account) {
            log_error("Failed to get account");
            return;
        }
        log_system("✓ Account ready - ID: " + account->account_number);
        log_system("  Starting Capital: $" + std::to_string(account->portfolio_value));
        entry_equity_ = account->portfolio_value;

        // Connect to bar feed (Polygon or Mock)
        log_system(is_mock_mode_ ? "Loading mock bar feed..." : "Connecting to Polygon proxy...");
        if (!bar_feed_->connect()) {
            log_error("Failed to connect to bar feed");
            return;
        }
        log_system(is_mock_mode_ ? "✓ Mock bars loaded" : "✓ Connected to Polygon");

        // In mock mode, load leveraged ETF prices
        if (is_mock_mode_) {
            log_system("Loading leveraged ETF prices for mock mode...");
            leveraged_prices_ = load_leveraged_prices("/tmp");
            if (!leveraged_prices_.empty()) {
                log_system("✓ Leveraged ETF prices loaded (SH, SDS, SPXL)");
            } else {
                log_system("⚠️  Warning: No leveraged ETF prices loaded - using fallback prices");
            }
            log_system("");
        }

        // Subscribe to symbols (SPY instruments)
        std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};
        if (!bar_feed_->subscribe(symbols)) {
            log_error("Failed to subscribe to symbols");
            return;
        }
        log_system("✓ Subscribed to SPY, SPXL, SH, SDS");
        log_system("");

        // Reconcile existing positions on startup (seamless continuation)
        reconcile_startup_positions();

        // Check for missed EOD and startup catch-up liquidation
        check_startup_eod_catch_up();

        // Initialize strategy with warmup
        log_system("Initializing OnlineEnsemble strategy...");
        warmup_strategy();
        log_system("✓ Strategy initialized and ready");
        log_system("");

        // Initialize micro-adaptation system (load baseline from morning optimization)
        std::string baseline_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/morning_baseline_params.json";
        if (std::ifstream(baseline_file).good()) {
            if (micro_adaptation_.load_baseline(baseline_file)) {
                micro_adaptation_.enabled = true;
                log_system("✅ Micro-adaptation system enabled");
                log_system("   Baseline buy: " + std::to_string(micro_adaptation_.baseline_buy_threshold));
                log_system("   Baseline sell: " + std::to_string(micro_adaptation_.baseline_sell_threshold));
                log_system("   Hourly drift: ±" + std::to_string(MicroAdaptationState::MAX_HOURLY_DRIFT * 100) + "%");
                log_system("   Max total drift: ±" + std::to_string(MicroAdaptationState::MAX_TOTAL_DRIFT * 100) + "%");
            } else {
                log_system("⚠️  Micro-adaptation baseline load failed - using static params");
            }
        } else {
            log_system("ℹ️  No baseline params found - micro-adaptation disabled");
            log_system("   (Run morning optimization to enable micro-adaptations)");
        }
        log_system("");

        // Start main trading loop
        bar_feed_->start([this](const std::string& symbol, const Bar& bar) {
            if (symbol == "SPY") {  // Only process on SPY bars (trigger for multi-instrument PSM)
                on_new_bar(bar);
            }
        });

        log_system("=== Live trading active - Press Ctrl+C to stop ===");
        log_system("");

        // Install signal handlers for graceful shutdown
        std::signal(SIGINT, [](int) { g_shutdown_requested = true; });
        std::signal(SIGTERM, [](int) { g_shutdown_requested = true; });

        // Keep running until shutdown requested
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Auto-shutdown at market close (4:00 PM ET) after EOD liquidation completes
            std::string today_et = et_time_.get_current_et_date();
            if (et_time_.is_market_close_time() && eod_state_.is_eod_complete(today_et)) {
                log_system("⏰ Market closed and EOD complete - initiating automatic shutdown");
                g_shutdown_requested = true;
            }
        }

        log_system("=== Shutdown requested - cleaning up ===");
    }

private:
    std::unique_ptr<IBrokerClient> broker_;
    std::unique_ptr<IBarFeed> bar_feed_;
    std::string log_dir_;
    bool is_mock_mode_;
    std::string data_file_;  // Path to market data CSV file for dashboard generation
    OnlineEnsembleStrategy strategy_;
    PositionStateMachine psm_;
    std::map<std::string, std::string> symbol_map_;

    // NEW: Production safety infrastructure
    PositionBook position_book_;
    ETTimeManager et_time_;  // Centralized ET time management
    EodStateStore eod_state_;  // Idempotent EOD tracking
    NyseCalendar nyse_calendar_;  // Holiday and half-day calendar
    std::unique_ptr<StatePersistence> state_persistence_;  // Atomic state persistence
    std::optional<Bar> previous_bar_;  // For bar-to-bar learning
    uint64_t bar_count_{0};

    // Mid-day optimization (15:15 PM ET / 3:15pm)
    std::vector<Bar> todays_bars_;  // Collect ALL bars from 9:30 onwards
    bool midday_optimization_done_{false};  // Flag to track if optimization ran today
    std::string midday_optimization_date_;  // Date of last optimization (YYYY-MM-DD)

    // State tracking
    PositionStateMachine::State current_state_;
    int bars_held_;
    double entry_equity_;
    double previous_portfolio_value_;  // Track portfolio value before trade for P&L calculation

    // Mock mode: Leveraged ETF prices loaded from CSV
    std::unordered_map<uint64_t, std::unordered_map<std::string, double>> leveraged_prices_;

    // Micro-adaptation system (NEW v2.6)
    MicroAdaptationState micro_adaptation_;

    // Log file streams
    std::ofstream log_system_;
    std::ofstream log_signals_;
    std::ofstream log_trades_;
    std::ofstream log_positions_;
    std::ofstream log_decisions_;
    std::string session_timestamp_;  // Store timestamp for dashboard generation

    // Risk management (v1.0 parameters)
    const double PROFIT_TARGET = 0.02;   // 2%
    const double STOP_LOSS = -0.015;     // -1.5%
    const int MIN_HOLD_BARS = 3;
    const int MAX_HOLD_BARS = 100;

    void init_logs() {
        // Create log directory if needed
        system(("mkdir -p " + log_dir_).c_str());

        session_timestamp_ = get_timestamp();

        log_system_.open(log_dir_ + "/system_" + session_timestamp_ + ".log");
        log_signals_.open(log_dir_ + "/signals_" + session_timestamp_ + ".jsonl");
        log_trades_.open(log_dir_ + "/trades_" + session_timestamp_ + ".jsonl");
        log_positions_.open(log_dir_ + "/positions_" + session_timestamp_ + ".jsonl");
        log_decisions_.open(log_dir_ + "/decisions_" + session_timestamp_ + ".jsonl");
    }

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    std::string get_timestamp_readable() const {
        return et_time_.get_current_et_string();
    }

    bool is_regular_hours() const {
        return et_time_.is_regular_hours();
    }

    bool is_end_of_day_liquidation_time() const {
        return et_time_.is_eod_liquidation_window();
    }

    void log_system(const std::string& message) {
        auto timestamp = get_timestamp_readable();
        std::cout << "[" << timestamp << "] " << message << std::endl;
        log_system_ << "[" << timestamp << "] " << message << std::endl;
        log_system_.flush();
    }

    void log_error(const std::string& message) {
        log_system("ERROR: " + message);
    }

    void generate_dashboard() {
        // Close log files to ensure all data is flushed
        log_system_.close();
        log_signals_.close();
        log_trades_.close();
        log_positions_.close();
        log_decisions_.close();

        std::cout << "\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "📊 Generating Trading Dashboard...\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

        // Construct file paths
        std::string trades_file = log_dir_ + "/trades_" + session_timestamp_ + ".jsonl";
        std::string signals_file = log_dir_ + "/signals_" + session_timestamp_ + ".jsonl";
        std::string dashboard_dir = "data/dashboards";
        std::string dashboard_file = dashboard_dir + "/session_" + session_timestamp_ + ".html";

        // Create dashboard directory
        system(("mkdir -p " + dashboard_dir).c_str());

        // Build Python command
        std::string python_cmd = "python3 tools/professional_trading_dashboard.py "
                                "--tradebook " + trades_file + " "
                                "--signals " + signals_file + " "
                                "--output " + dashboard_file + " "
                                "--start-equity 100000 ";

        // Add data file if available (for candlestick charts and trade markers)
        if (!data_file_.empty()) {
            python_cmd += "--data " + data_file_ + " ";
        }

        python_cmd += "> /dev/null 2>&1";

        std::cout << "  Tradebook: " << trades_file << "\n";
        std::cout << "  Signals: " << signals_file << "\n";
        if (!data_file_.empty()) {
            std::cout << "  Data: " + data_file_ + "\n";
        }
        std::cout << "  Output: " << dashboard_file << "\n";
        std::cout << "\n";

        // Execute Python dashboard generator
        int result = system(python_cmd.c_str());

        if (result == 0) {
            std::cout << "✅ Dashboard generated successfully!\n";
            std::cout << "   📂 Open: " << dashboard_file << "\n";
            std::cout << "\n";

            // Send email notification (works in both live and mock modes)
            std::cout << "📧 Sending email notification...\n";

            std::string email_cmd = "python3 tools/send_dashboard_email.py "
                                   "--dashboard " + dashboard_file + " "
                                   "--trades " + trades_file + " "
                                   "--recipient yeogirl@gmail.com "
                                   "> /dev/null 2>&1";

            int email_result = system(email_cmd.c_str());

            if (email_result == 0) {
                std::cout << "✅ Email sent to yeogirl@gmail.com\n";
            } else {
                std::cout << "⚠️  Email sending failed (check GMAIL_APP_PASSWORD)\n";
            }
        } else {
            std::cout << "⚠️  Dashboard generation failed (exit code: " << result << ")\n";
            std::cout << "   You can manually generate it with:\n";
            std::cout << "   python3 tools/professional_trading_dashboard.py \\\n";
            std::cout << "     --tradebook " << trades_file << " \\\n";
            std::cout << "     --signals " << signals_file << " \\\n";
            std::cout << "     --output " << dashboard_file << "\n";
        }

        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "\n";
    }

    void reconcile_startup_positions() {
        log_system("=== Startup Position Reconciliation ===");

        // Get current broker state
        auto account = broker_->get_account();
        if (!account) {
            log_error("Failed to get account info for startup reconciliation");
            return;
        }

        auto broker_positions = get_broker_positions();

        log_system("  Cash: $" + std::to_string(account->cash));
        log_system("  Portfolio Value: $" + std::to_string(account->portfolio_value));

        // ===================================================================
        // STEP 1: Try to load persisted state from previous session
        // ===================================================================
        if (auto persisted = state_persistence_->load_state()) {
            log_system("[STATE_PERSIST] ✓ Found persisted state from previous session");
            log_system("  Session ID: " + persisted->session_id);
            log_system("  Last save: " + persisted->last_bar_time_str);
            log_system("  PSM State: " + psm_.state_to_string(persisted->psm_state));
            log_system("  Bars held: " + std::to_string(persisted->bars_held));

            // Validate positions match broker
            bool positions_match = validate_positions_match(persisted->positions, broker_positions);

            if (positions_match) {
                log_system("[STATE_PERSIST] ✓ Positions match broker - restoring exact state");

                // Restore exact state
                current_state_ = persisted->psm_state;
                bars_held_ = persisted->bars_held;
                entry_equity_ = persisted->entry_equity;

                // Calculate bars elapsed since last save
                if (previous_bar_.has_value()) {
                    uint64_t bars_elapsed = calculate_bars_since(
                        persisted->last_bar_timestamp,
                        previous_bar_->timestamp_ms
                    );
                    bars_held_ += bars_elapsed;
                    log_system("  Adjusted bars held: " + std::to_string(bars_held_) +
                              " (+" + std::to_string(bars_elapsed) + " bars since save)");
                }

                // Initialize position book
                for (const auto& pos : broker_positions) {
                    position_book_.set_position(pos.symbol, pos.qty, pos.avg_entry_price);
                }

                log_system("✓ State fully recovered from persistence");
                log_system("");
                return;
            } else {
                log_system("[STATE_PERSIST] ⚠️  Position mismatch - falling back to broker reconciliation");
            }
        } else {
            log_system("[STATE_PERSIST] No persisted state found - using broker reconciliation");
        }

        // ===================================================================
        // STEP 2: Fall back to broker-based reconciliation
        // ===================================================================
        if (broker_positions.empty()) {
            log_system("  Current Positions: NONE (starting flat)");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;
            log_system("  Initial State: CASH_ONLY");
            log_system("  Bars Held: 0 (no positions)");
        } else {
            log_system("  Current Positions:");
            for (const auto& pos : broker_positions) {
                log_system("    " + pos.symbol + ": " +
                          std::to_string(pos.qty) + " shares @ $" +
                          std::to_string(pos.avg_entry_price) +
                          " (P&L: $" + std::to_string(pos.unrealized_pnl) + ")");

                // Initialize position book with existing positions
                position_book_.set_position(pos.symbol, pos.qty, pos.avg_entry_price);
            }

            // Infer current PSM state from positions
            current_state_ = infer_state_from_positions(broker_positions);

            // CRITICAL FIX: Set bars_held to MIN_HOLD_BARS to allow immediate exits
            // since we don't know how long the positions have been held
            bars_held_ = MIN_HOLD_BARS;

            log_system("  Inferred PSM State: " + psm_.state_to_string(current_state_));
            log_system("  Bars Held: " + std::to_string(bars_held_) +
                      " (set to MIN_HOLD to allow immediate exits on startup)");
            log_system("  NOTE: Positions were reconciled from broker - assuming min hold satisfied");
        }

        log_system("✓ Startup reconciliation complete - resuming trading seamlessly");
        log_system("");
    }

    void check_startup_eod_catch_up() {
        log_system("=== Startup EOD Catch-Up Check ===");

        auto et_tm = et_time_.get_current_et_tm();
        std::string today_et = format_et_date(et_tm);
        std::string prev_trading_day = get_previous_trading_day(et_tm);

        log_system("  Current ET Time: " + et_time_.get_current_et_string());
        log_system("  Today (ET): " + today_et);
        log_system("  Previous Trading Day: " + prev_trading_day);

        // Check 1: Did we miss previous trading day's EOD?
        if (!eod_state_.is_eod_complete(prev_trading_day)) {
            log_system("  ⚠️  WARNING: Previous trading day's EOD not completed");

            auto broker_positions = get_broker_positions();
            if (!broker_positions.empty()) {
                log_system("  ⚠️  Open positions detected - executing catch-up liquidation");
                liquidate_all_positions();
                eod_state_.mark_eod_complete(prev_trading_day);
                log_system("  ✓ Catch-up liquidation complete for " + prev_trading_day);
            } else {
                log_system("  ✓ No open positions - marking previous EOD as complete");
                eod_state_.mark_eod_complete(prev_trading_day);
            }
        } else {
            log_system("  ✓ Previous trading day EOD already complete");
        }

        // Check 2: Started outside trading hours with positions?
        if (et_time_.should_liquidate_on_startup(has_open_positions())) {
            log_system("  ⚠️  Started outside trading hours with open positions");
            log_system("  ⚠️  Executing immediate liquidation");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(today_et);
            log_system("  ✓ Startup liquidation complete");
        }

        log_system("✓ Startup EOD check complete");
        log_system("");
    }

    std::string format_et_date(const std::tm& tm) const {
        char buffer[11];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        return std::string(buffer);
    }

    std::string get_previous_trading_day(const std::tm& current_tm) const {
        // Walk back day-by-day until we find a trading day
        std::tm tm = current_tm;
        for (int i = 1; i <= 10; ++i) {
            // Subtract i days (approximate - good enough for recent history)
            std::time_t t = std::mktime(&tm) - (i * 86400);
            std::tm* prev_tm = std::localtime(&t);
            std::string prev_date = format_et_date(*prev_tm);

            // Check if weekday and not holiday
            if (prev_tm->tm_wday >= 1 && prev_tm->tm_wday <= 5) {
                if (nyse_calendar_.is_trading_day(prev_date)) {
                    return prev_date;
                }
            }
        }
        // Fallback: return today if can't find previous trading day
        return format_et_date(current_tm);
    }

    bool has_open_positions() {
        auto broker_positions = get_broker_positions();
        return !broker_positions.empty();
    }

    PositionStateMachine::State infer_state_from_positions(
        const std::vector<BrokerPosition>& positions) {

        // Map SPY instruments to equivalent QQQ PSM states
        // SPY/SPXL/SH/SDS → QQQ/TQQQ/PSQ/SQQQ
        bool has_base = false;   // SPY
        bool has_bull3x = false; // SPXL
        bool has_bear1x = false; // SH
        bool has_bear_nx = false; // SDS

        for (const auto& pos : positions) {
            if (pos.qty > 0) {
                if (pos.symbol == "SPXL") has_bull3x = true;
                if (pos.symbol == "SPY") has_base = true;
                if (pos.symbol == "SH") has_bear1x = true;
                if (pos.symbol == "SDS") has_bear_nx = true;
            }
        }

        // Check for dual-instrument states first
        if (has_base && has_bull3x) return PositionStateMachine::State::QQQ_TQQQ;    // BASE_BULL_3X
        if (has_bear1x && has_bear_nx) return PositionStateMachine::State::PSQ_SQQQ; // BEAR_1X_NX

        // Single instrument states
        if (has_bull3x) return PositionStateMachine::State::TQQQ_ONLY;  // BULL_3X_ONLY
        if (has_base) return PositionStateMachine::State::QQQ_ONLY;     // BASE_ONLY
        if (has_bear1x) return PositionStateMachine::State::PSQ_ONLY;   // BEAR_1X_ONLY
        if (has_bear_nx) return PositionStateMachine::State::SQQQ_ONLY; // BEAR_NX_ONLY

        return PositionStateMachine::State::CASH_ONLY;
    }

    // =====================================================================
    // State Persistence Helper Methods
    // =====================================================================

    /**
     * Calculate number of 1-minute bars elapsed between two timestamps
     */
    uint64_t calculate_bars_since(uint64_t from_ts_ms, uint64_t to_ts_ms) const {
        if (to_ts_ms <= from_ts_ms) return 0;
        uint64_t elapsed_ms = to_ts_ms - from_ts_ms;
        uint64_t elapsed_minutes = elapsed_ms / (60 * 1000);
        return elapsed_minutes;
    }

    /**
     * Validate that persisted positions match broker positions
     */
    bool validate_positions_match(
        const std::vector<StatePersistence::PositionDetail>& persisted,
        const std::vector<BrokerPosition>& broker) {

        // Quick check: same number of positions
        if (persisted.size() != broker.size()) {
            log_system("  Position count mismatch: persisted=" +
                      std::to_string(persisted.size()) +
                      " broker=" + std::to_string(broker.size()));
            return false;
        }

        // Build maps for easier comparison
        std::map<std::string, double> persisted_map;
        for (const auto& p : persisted) {
            persisted_map[p.symbol] = p.quantity;
        }

        std::map<std::string, double> broker_map;
        for (const auto& p : broker) {
            broker_map[p.symbol] = p.qty;
        }

        // Check each symbol
        for (const auto& [symbol, qty] : persisted_map) {
            if (broker_map.find(symbol) == broker_map.end()) {
                log_system("  Symbol mismatch: " + symbol + " in persisted but not in broker");
                return false;
            }
            if (std::abs(broker_map[symbol] - qty) > 0.01) {  // Allow tiny floating point difference
                log_system("  Quantity mismatch for " + symbol + ": persisted=" +
                          std::to_string(qty) + " broker=" + std::to_string(broker_map[symbol]));
                return false;
            }
        }

        return true;
    }

    /**
     * Persist current trading state to disk
     */
    void persist_current_state() {
        try {
            StatePersistence::TradingState state;
            state.psm_state = current_state_;
            state.bars_held = bars_held_;
            state.entry_equity = entry_equity_;

            if (previous_bar_.has_value()) {
                state.last_bar_timestamp = previous_bar_->timestamp_ms;
                state.last_bar_time_str = format_bar_time(*previous_bar_);
            }

            // Add current positions
            auto broker_positions = get_broker_positions();
            for (const auto& pos : broker_positions) {
                StatePersistence::PositionDetail detail;
                detail.symbol = pos.symbol;
                detail.quantity = pos.qty;
                detail.avg_entry_price = pos.avg_entry_price;
                detail.entry_timestamp = previous_bar_ ? previous_bar_->timestamp_ms : 0;
                state.positions.push_back(detail);
            }

            state.session_id = session_timestamp_;

            if (!state_persistence_->save_state(state)) {
                log_system("⚠️  State persistence failed (non-fatal - continuing)");
            }

        } catch (const std::exception& e) {
            log_system("⚠️  State persistence error: " + std::string(e.what()));
        }
    }

    void warmup_strategy() {
        // Load warmup data created by comprehensive_warmup.sh script
        // This file contains: 7864 warmup bars (20 blocks @ 390 bars/block + 64 feature bars) + all of today's bars up to now
        std::string warmup_file = "data/equities/SPY_warmup_latest.csv";

        // Try relative path first, then from parent directory
        std::ifstream file(warmup_file);
        if (!file.is_open()) {
            warmup_file = "../data/equities/SPY_warmup_latest.csv";
            file.open(warmup_file);
        }

        if (!file.is_open()) {
            log_system("WARNING: Could not open warmup file: " + warmup_file);
            log_system("         Run tools/warmup_live_trading.sh first!");
            log_system("         Strategy will learn from first few live bars");
            return;
        }

        // Read all bars from warmup file
        std::vector<Bar> all_bars;
        std::string line;
        std::getline(file, line); // Skip header

        while (std::getline(file, line)) {
            // Skip empty lines or header-like lines
            if (line.empty() ||
                line.find("timestamp") != std::string::npos ||
                line.find("ts_utc") != std::string::npos ||
                line.find("ts_nyt_epoch") != std::string::npos) {
                continue;
            }

            std::istringstream iss(line);
            std::string ts_utc_str, ts_epoch_str, open_str, high_str, low_str, close_str, volume_str;

            // CSV format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            if (std::getline(iss, ts_utc_str, ',') &&
                std::getline(iss, ts_epoch_str, ',') &&
                std::getline(iss, open_str, ',') &&
                std::getline(iss, high_str, ',') &&
                std::getline(iss, low_str, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, volume_str)) {

                Bar bar;
                bar.timestamp_ms = std::stoll(ts_epoch_str) * 1000ULL;  // Convert seconds to milliseconds
                bar.open = std::stod(open_str);
                bar.high = std::stod(high_str);
                bar.low = std::stod(low_str);
                bar.close = std::stod(close_str);
                bar.volume = std::stoll(volume_str);
                all_bars.push_back(bar);
            }
        }
        file.close();

        if (all_bars.empty()) {
            log_system("WARNING: No bars loaded from warmup file");
            return;
        }

        log_system("Loaded " + std::to_string(all_bars.size()) + " bars from warmup file");
        log_system("");

        // Feed ALL bars (3900 warmup + today's bars)
        // This ensures we're caught up to the current time
        log_system("=== Starting Warmup Process ===");
        log_system("  Target: 3900 bars (10 blocks @ 390 bars/block)");
        log_system("  Available: " + std::to_string(all_bars.size()) + " bars");
        log_system("");

        int predictor_training_count = 0;
        int feature_engine_ready_bar = 0;
        int strategy_ready_bar = 0;

        for (size_t i = 0; i < all_bars.size(); ++i) {
            strategy_.on_bar(all_bars[i]);

            // Report feature engine ready
            if (i == 64 && feature_engine_ready_bar == 0) {
                feature_engine_ready_bar = i;
                log_system("✓ Feature Engine Warmup Complete (64 bars)");
                log_system("  - All rolling windows initialized");
                log_system("  - Technical indicators ready");
                log_system("  - Starting predictor training...");
                log_system("");
            }

            // Train predictor on bar-to-bar returns (wait for strategy to be fully ready)
            if (strategy_.is_ready() && i + 1 < all_bars.size()) {
                auto features = strategy_.extract_features(all_bars[i]);
                if (!features.empty()) {
                    double current_close = all_bars[i].close;
                    double next_close = all_bars[i + 1].close;
                    double realized_return = (next_close - current_close) / current_close;

                    strategy_.train_predictor(features, realized_return);
                    predictor_training_count++;
                }
            }

            // Report strategy ready
            if (strategy_.is_ready() && strategy_ready_bar == 0) {
                strategy_ready_bar = i;
                log_system("✓ Strategy Warmup Complete (" + std::to_string(i) + " bars)");
                log_system("  - EWRLS predictor fully trained");
                log_system("  - Multi-horizon predictions ready");
                log_system("  - Strategy ready for live trading");
                log_system("");
            }

            // Progress indicator every 1000 bars
            if ((i + 1) % 1000 == 0) {
                log_system("  Progress: " + std::to_string(i + 1) + "/" + std::to_string(all_bars.size()) +
                          " bars (" + std::to_string(predictor_training_count) + " training samples)");
            }

            // Update bar_count_ and previous_bar_ for seamless transition to live
            bar_count_++;
            previous_bar_ = all_bars[i];
        }

        log_system("");
        log_system("=== Warmup Summary ===");
        log_system("✓ Total bars processed: " + std::to_string(all_bars.size()));
        log_system("✓ Feature engine ready: Bar " + std::to_string(feature_engine_ready_bar));
        log_system("✓ Strategy ready: Bar " + std::to_string(strategy_ready_bar));
        log_system("✓ Predictor trained: " + std::to_string(predictor_training_count) + " samples");
        log_system("✓ Last warmup bar: " + format_bar_time(all_bars.back()));
        log_system("✓ Strategy is_ready() = " + std::string(strategy_.is_ready() ? "YES" : "NO"));
        log_system("");
    }

    std::string format_bar_time(const Bar& bar) const {
        time_t time_t_val = static_cast<time_t>(bar.timestamp_ms / 1000);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void on_new_bar(const Bar& bar) {
        bar_count_++;

        // In mock mode, sync time manager to bar timestamp and update market prices
        if (is_mock_mode_) {
            et_time_.set_mock_time(bar.timestamp_ms);

            // Update MockBroker with current market prices
            auto* mock_broker = dynamic_cast<MockBroker*>(broker_.get());
            if (mock_broker) {
                // Update SPY price from bar
                mock_broker->update_market_price("SPY", bar.close);

                // Update leveraged ETF prices from loaded CSV data
                uint64_t bar_ts_sec = bar.timestamp_ms / 1000;

                // CRITICAL: Crash fast if no price data found (no silent fallbacks!)
                if (!leveraged_prices_.count(bar_ts_sec)) {
                    throw std::runtime_error(
                        "CRITICAL: No leveraged ETF price data for timestamp " +
                        std::to_string(bar_ts_sec) + " (bar time: " +
                        get_timestamp_readable() + ")");
                }

                const auto& prices_at_ts = leveraged_prices_[bar_ts_sec];

                // Validate all required symbols have prices
                std::vector<std::string> required_symbols = {"SPXL", "SH", "SDS"};
                for (const auto& symbol : required_symbols) {
                    if (!prices_at_ts.count(symbol)) {
                        throw std::runtime_error(
                            "CRITICAL: Missing price for " + symbol +
                            " at timestamp " + std::to_string(bar_ts_sec));
                    }
                    mock_broker->update_market_price(symbol, prices_at_ts.at(symbol));
                }
            }
        }

        auto timestamp = get_timestamp_readable();

        // Log bar received
        log_system("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        log_system("📊 BAR #" + std::to_string(bar_count_) + " Received from Polygon");
        log_system("  Time: " + timestamp);
        log_system("  OHLC: O=$" + std::to_string(bar.open) + " H=$" + std::to_string(bar.high) +
                  " L=$" + std::to_string(bar.low) + " C=$" + std::to_string(bar.close));
        log_system("  Volume: " + std::to_string(bar.volume));

        // =====================================================================
        // STEP 1: Bar Validation (NEW - P4)
        // =====================================================================
        if (!is_valid_bar(bar)) {
            log_error("❌ Invalid bar dropped: " + BarValidator::get_error_message(bar));
            return;
        }
        log_system("✓ Bar validation passed");

        // =====================================================================
        // STEP 2: Feed to Strategy (ALWAYS - for continuous learning)
        // =====================================================================
        log_system("⚙️  Feeding bar to strategy (updating indicators)...");
        strategy_.on_bar(bar);

        // =====================================================================
        // STEP 3: Continuous Bar-to-Bar Learning (NEW - P1-1 fix)
        // =====================================================================
        if (previous_bar_.has_value()) {
            auto features = strategy_.extract_features(*previous_bar_);
            if (!features.empty()) {
                double return_1bar = (bar.close - previous_bar_->close) /
                                    previous_bar_->close;
                strategy_.train_predictor(features, return_1bar);
                log_system("✓ Predictor updated (learning from previous bar return: " +
                          std::to_string(return_1bar * 100) + "%)");

                // Update micro-adaptation state with bar return
                if (micro_adaptation_.enabled) {
                    micro_adaptation_.update_on_bar(return_1bar);

                    // Log threshold adaptations when they occur
                    if (micro_adaptation_.bars_since_last_threshold_adapt == 1) {
                        log_system("🔧 MICRO-ADAPTATION: Hourly threshold update (hour " +
                                  std::to_string(micro_adaptation_.adaptation_hour) + ")");
                        log_system("   Adapted buy threshold: " +
                                  std::to_string(micro_adaptation_.current_buy_threshold) +
                                  " (drift: " +
                                  std::to_string((micro_adaptation_.current_buy_threshold -
                                                micro_adaptation_.baseline_buy_threshold) * 100) + "%)");
                        log_system("   Adapted sell threshold: " +
                                  std::to_string(micro_adaptation_.current_sell_threshold) +
                                  " (drift: " +
                                  std::to_string((micro_adaptation_.current_sell_threshold -
                                                micro_adaptation_.baseline_sell_threshold) * 100) + "%)");
                    }

                    // Log lambda adaptations when they occur
                    if (micro_adaptation_.bars_since_last_lambda_adapt == 1) {
                        log_system("🔧 MICRO-ADAPTATION: Lambda update");
                        log_system("   Adapted lambda: " +
                                  std::to_string(micro_adaptation_.current_lambda) +
                                  " (baseline: " +
                                  std::to_string(micro_adaptation_.baseline_lambda) + ")");
                    }
                }
            }
        }
        previous_bar_ = bar;

        // =====================================================================
        // STEP 3.5: Increment bars_held counter (CRITICAL for min hold period)
        // =====================================================================
        if (current_state_ != PositionStateMachine::State::CASH_ONLY) {
            bars_held_++;
            log_system("📊 Position holding duration: " + std::to_string(bars_held_) + " bars");
        }

        // =====================================================================
        // STEP 4: Periodic Position Reconciliation (NEW - P0-3)
        // Skip in mock mode - no external broker to drift from
        // =====================================================================
        if (!is_mock_mode_ && bar_count_ % 60 == 0) {  // Every 60 bars (60 minutes)
            try {
                auto broker_positions = get_broker_positions();
                position_book_.reconcile_with_broker(broker_positions);
            } catch (const PositionReconciliationError& e) {
                log_error("[" + timestamp + "] RECONCILIATION FAILED: " +
                         std::string(e.what()));
                log_error("[" + timestamp + "] Initiating emergency flatten");
                liquidate_all_positions();
                throw;  // Exit for supervisor restart
            }
        }

        // =====================================================================
        // STEP 4.5: Persist State (Every 10 bars for low overhead)
        // =====================================================================
        if (bar_count_ % 10 == 0) {
            persist_current_state();
        }

        // =====================================================================
        // STEP 5: Check End-of-Day Liquidation (IDEMPOTENT)
        // =====================================================================
        std::string today_et = timestamp.substr(0, 10);  // Extract YYYY-MM-DD from timestamp

        // Check if today is a trading day
        if (!nyse_calendar_.is_trading_day(today_et)) {
            log_system("⏸️  Holiday/Weekend - no trading (learning continues)");
            return;
        }

        // Idempotent EOD check: only liquidate once per trading day
        if (is_end_of_day_liquidation_time() && !eod_state_.is_eod_complete(today_et)) {
            log_system("🔔 END OF DAY - Liquidation window active");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(today_et);
            log_system("✓ EOD liquidation complete for " + today_et);
            return;
        }

        // =====================================================================
        // STEP 5.5: Mid-Day Optimization at 16:05 PM ET (NEW)
        // =====================================================================
        // Reset optimization flag for new trading day
        if (midday_optimization_date_ != today_et) {
            midday_optimization_done_ = false;
            midday_optimization_date_ = today_et;
            todays_bars_.clear();  // Clear today's bars for new day
        }

        // Collect ALL bars during regular hours (9:30-16:00) for optimization
        if (is_regular_hours()) {
            todays_bars_.push_back(bar);

            // Check if it's 15:15 PM ET and optimization hasn't been done yet
            if (et_time_.is_midday_optimization_time() && !midday_optimization_done_) {
                log_system("🔔 MID-DAY OPTIMIZATION TIME (15:15 PM ET / 3:15pm)");

                // Liquidate all positions before optimization
                log_system("Liquidating all positions before optimization...");
                liquidate_all_positions();
                log_system("✓ Positions liquidated - going 100% cash");

                // Run optimization
                run_midday_optimization();

                // Mark as done
                midday_optimization_done_ = true;

                // Skip trading for this bar (optimization takes time)
                return;
            }
        }

        // =====================================================================
        // STEP 6: Trading Hours Gate (NEW - only trade during RTH, before EOD)
        // =====================================================================
        if (!is_regular_hours()) {
            log_system("⏰ After-hours - learning only, no trading");
            return;  // Learning continues, but no trading
        }

        // CRITICAL: Block trading after EOD liquidation (3:58 PM - 4:00 PM)
        if (et_time_.is_eod_liquidation_window()) {
            log_system("🔴 EOD window active - learning only, no new trades");
            return;  // Learning continues, but no new positions
        }

        log_system("🕐 Regular Trading Hours - processing for signals and trades");

        // =====================================================================
        // STEP 7: Generate Signal and Trade (RTH only)
        // =====================================================================
        log_system("🧠 Generating signal from strategy...");
        auto signal = generate_signal(bar);

        // Log signal with detailed info
        log_system("📈 SIGNAL GENERATED:");
        log_system("  Prediction: " + signal.prediction);
        log_system("  Probability: " + std::to_string(signal.probability));
        log_system("  Confidence: " + std::to_string(signal.probability));
        log_system("  Strategy Ready: " + std::string(strategy_.is_ready() ? "YES" : "NO"));

        log_signal(bar, signal);

        // Make trading decision
        log_system("🎯 Evaluating trading decision...");
        auto decision = make_decision(signal, bar);

        // Enhanced decision logging with detailed explanation
        log_enhanced_decision(signal, decision);
        log_decision(decision);

        // Execute if needed
        if (decision.should_trade) {
            execute_transition(decision);
        } else {
            log_system("⏸️  NO TRADE: " + decision.reason);
        }

        // Log current portfolio state
        log_portfolio_state();
    }

    struct Signal {
        double probability;
        double confidence;
        std::string prediction;  // "LONG", "SHORT", "NEUTRAL"
        double prob_1bar;
        double prob_5bar;
        double prob_10bar;
    };

    Signal generate_signal(const Bar& bar) {
        // Call OnlineEnsemble strategy to generate real signal
        auto strategy_signal = strategy_.generate_signal(bar);

        // DEBUG: Check why we're getting 0.5
        if (strategy_signal.probability == 0.5) {
            std::string reason = "unknown";
            if (strategy_signal.metadata.count("skip_reason")) {
                reason = strategy_signal.metadata.at("skip_reason");
            }
            std::cout << "  [DBG: p=0.5 reason=" << reason << "]" << std::endl;
        }

        Signal signal;
        signal.probability = strategy_signal.probability;
        signal.probability = strategy_signal.probability;  // Use confidence from strategy

        // Map signal type to prediction string
        if (strategy_signal.signal_type == SignalType::LONG) {
            signal.prediction = "LONG";
        } else if (strategy_signal.signal_type == SignalType::SHORT) {
            signal.prediction = "SHORT";
        } else {
            signal.prediction = "NEUTRAL";
        }

        // Use same probability for all horizons (OnlineEnsemble provides single probability)
        signal.prob_1bar = strategy_signal.probability;
        signal.prob_5bar = strategy_signal.probability;
        signal.prob_10bar = strategy_signal.probability;

        return signal;
    }

    struct Decision {
        bool should_trade;
        PositionStateMachine::State target_state;
        std::string reason;
        double current_equity;
        double position_pnl_pct;
        bool profit_target_hit;
        bool stop_loss_hit;
        bool min_hold_violated;
    };

    Decision make_decision(const Signal& signal, const Bar& bar) {
        Decision decision;
        decision.should_trade = false;

        // Get current portfolio state
        auto account = broker_->get_account();
        if (!account) {
            decision.reason = "Failed to get account info";
            return decision;
        }

        decision.current_equity = account->portfolio_value;
        decision.position_pnl_pct = (decision.current_equity - entry_equity_) / entry_equity_;

        // Check profit target / stop loss
        decision.profit_target_hit = (decision.position_pnl_pct >= PROFIT_TARGET &&
                                      current_state_ != PositionStateMachine::State::CASH_ONLY);
        decision.stop_loss_hit = (decision.position_pnl_pct <= STOP_LOSS &&
                                  current_state_ != PositionStateMachine::State::CASH_ONLY);

        // Check minimum hold period
        decision.min_hold_violated = (bars_held_ < MIN_HOLD_BARS);

        // Force exit to cash if profit/stop hit
        if (decision.profit_target_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "PROFIT_TARGET (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        if (decision.stop_loss_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "STOP_LOSS (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        // Map signal probability to PSM state (v1.0 asymmetric thresholds)
        // Use micro-adapted thresholds if enabled, otherwise use defaults
        double buy_threshold = micro_adaptation_.enabled ? micro_adaptation_.current_buy_threshold : 0.55;
        double sell_threshold = micro_adaptation_.enabled ? micro_adaptation_.current_sell_threshold : 0.45;

        // Calculate derived thresholds (maintain relative spacing)
        double ultra_bull_threshold = buy_threshold + 0.13;  // 0.55 + 0.13 = 0.68
        double mixed_bull_threshold = buy_threshold + 0.05;  // 0.55 + 0.05 = 0.60
        double ultra_bear_threshold = sell_threshold - 0.10; // 0.45 - 0.10 = 0.35
        double mixed_bear_threshold = sell_threshold - 0.13; // 0.45 - 0.13 = 0.32
        double cash_high = 0.49;  // Fixed narrow band around 0.5

        PositionStateMachine::State target_state;

        if (signal.probability >= ultra_bull_threshold) {
            target_state = PositionStateMachine::State::TQQQ_ONLY;  // Maps to SPXL
        } else if (signal.probability >= mixed_bull_threshold) {
            target_state = PositionStateMachine::State::QQQ_TQQQ;   // Mixed
        } else if (signal.probability >= buy_threshold) {
            target_state = PositionStateMachine::State::QQQ_ONLY;   // Maps to SPY
        } else if (signal.probability >= cash_high) {
            target_state = PositionStateMachine::State::CASH_ONLY;
        } else if (signal.probability >= sell_threshold) {
            target_state = PositionStateMachine::State::PSQ_ONLY;   // Maps to SH
        } else if (signal.probability >= ultra_bear_threshold) {
            target_state = PositionStateMachine::State::PSQ_SQQQ;   // Mixed
        } else if (signal.probability < mixed_bear_threshold) {
            target_state = PositionStateMachine::State::SQQQ_ONLY;  // Maps to SDS
        } else {
            target_state = PositionStateMachine::State::CASH_ONLY;
        }

        decision.target_state = target_state;

        // Check if state transition needed
        if (target_state != current_state_) {
            // Check minimum hold period
            if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
                decision.should_trade = false;
                decision.reason = "MIN_HOLD_PERIOD (held " + std::to_string(bars_held_) + " bars)";
            } else {
                decision.should_trade = true;
                decision.reason = "STATE_TRANSITION (prob=" + std::to_string(signal.probability) + ")";
            }
        } else {
            decision.should_trade = false;
            decision.reason = "NO_CHANGE";
        }

        return decision;
    }

    void liquidate_all_positions() {
        log_system("Closing all positions for end of day...");

        if (broker_->close_all_positions()) {
            log_system("✓ All positions closed");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;

            auto account = broker_->get_account();
            if (account) {
                log_system("Final portfolio value: $" + std::to_string(account->portfolio_value));
                entry_equity_ = account->portfolio_value;
            }
        } else {
            log_error("Failed to close all positions");
        }

        log_portfolio_state();
    }

    void execute_transition(const Decision& decision) {
        log_system("");
        log_system("🚀 *** EXECUTING TRADE ***");
        log_system("  Current State: " + psm_.state_to_string(current_state_));
        log_system("  Target State: " + psm_.state_to_string(decision.target_state));
        log_system("  Reason: " + decision.reason);
        log_system("");

        // Step 1: Close all current positions
        log_system("📤 Step 1: Closing current positions...");

        // Get current positions before closing (for logging)
        auto positions_to_close = broker_->get_positions();

        if (!broker_->close_all_positions()) {
            log_error("❌ Failed to close positions - aborting transition");
            return;
        }

        // Get account info before closing for accurate P&L calculation
        auto account_before = broker_->get_account();
        double portfolio_before = account_before ? account_before->portfolio_value : previous_portfolio_value_;

        // Log the close orders
        if (!positions_to_close.empty()) {
            for (const auto& pos : positions_to_close) {
                if (std::abs(pos.qty) >= 0.001) {
                    // Create a synthetic Order object for logging
                    Order close_order;
                    close_order.symbol = pos.symbol;
                    close_order.quantity = -pos.qty;  // Negative to close
                    close_order.side = (pos.qty > 0) ? "sell" : "buy";
                    close_order.type = "market";
                    close_order.time_in_force = "gtc";
                    close_order.order_id = "CLOSE-" + pos.symbol;
                    close_order.status = "filled";
                    close_order.filled_qty = std::abs(pos.qty);
                    close_order.filled_avg_price = pos.current_price;

                    // Calculate realized P&L for this close
                    double trade_pnl = (pos.qty > 0) ?
                        pos.qty * (pos.current_price - pos.avg_entry_price) :  // Long close
                        pos.qty * (pos.avg_entry_price - pos.current_price);   // Short close

                    // Get updated account info
                    auto account_after = broker_->get_account();
                    double cash = account_after ? account_after->cash : 0.0;
                    double portfolio = account_after ? account_after->portfolio_value : portfolio_before;

                    log_trade(close_order, bar_count_, cash, portfolio, trade_pnl, "Close position");
                    log_system("  🔴 CLOSE " + std::to_string(std::abs(pos.qty)) + " " + pos.symbol +
                              " (P&L: $" + std::to_string(trade_pnl) + ")");

                    previous_portfolio_value_ = portfolio;
                }
            }
        }

        log_system("✓ All positions closed");

        // Wait a moment for orders to settle (only in live mode)
        // In mock mode, skip sleep to avoid deadlock with replay thread
        if (!is_mock_mode_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        // Step 2: Get current account info
        log_system("💰 Step 2: Fetching account balance from Alpaca...");
        auto account = broker_->get_account();
        if (!account) {
            log_error("❌ Failed to get account info - aborting transition");
            return;
        }

        double available_capital = account->cash;
        double portfolio_value = account->portfolio_value;
        log_system("✓ Account Status:");
        log_system("  Cash: $" + std::to_string(available_capital));
        log_system("  Portfolio Value: $" + std::to_string(portfolio_value));
        log_system("  Buying Power: $" + std::to_string(account->buying_power));

        // Step 3: Calculate target positions based on PSM state
        auto target_positions = calculate_target_allocations(decision.target_state, available_capital);

        // CRITICAL: If target is not CASH_ONLY but we got empty positions, something is wrong
        bool position_entry_failed = false;
        if (target_positions.empty() && decision.target_state != PositionStateMachine::State::CASH_ONLY) {
            log_error("❌ CRITICAL: Target state is " + psm_.state_to_string(decision.target_state) +
                     " but failed to calculate positions (likely price fetch failure)");
            log_error("   Staying in CASH_ONLY for safety");
            position_entry_failed = true;
        }

        // Step 4: Execute buy orders for target positions
        if (!target_positions.empty()) {
            log_system("");
            log_system("📥 Step 3: Opening new positions...");
            for (const auto& [symbol, quantity] : target_positions) {
                if (quantity > 0) {
                    log_system("  🔵 Sending BUY order to Alpaca:");
                    log_system("     Symbol: " + symbol);
                    log_system("     Quantity: " + std::to_string(quantity) + " shares");

                    auto order = broker_->place_market_order(symbol, quantity, "gtc");
                    if (order) {
                        log_system("  ✓ Order Confirmed:");
                        log_system("     Order ID: " + order->order_id);
                        log_system("     Status: " + order->status);

                        // Get updated account info for accurate logging
                        auto account_after = broker_->get_account();
                        double cash = account_after ? account_after->cash : 0.0;
                        double portfolio = account_after ? account_after->portfolio_value : previous_portfolio_value_;
                        double trade_pnl = portfolio - previous_portfolio_value_;  // Portfolio change from this trade

                        // Build reason string from decision
                        std::string reason = "Enter " + psm_.state_to_string(decision.target_state);
                        if (decision.profit_target_hit) reason += " (profit target)";
                        else if (decision.stop_loss_hit) reason += " (stop loss)";

                        log_trade(*order, bar_count_, cash, portfolio, trade_pnl, reason);
                        previous_portfolio_value_ = portfolio;
                    } else {
                        log_error("  ❌ Failed to place order for " + symbol);
                    }

                    // Small delay between orders
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
        } else {
            log_system("💵 Target state is CASH_ONLY - no positions to open");
        }

        // Update state - CRITICAL FIX: Only update to target state if we successfully entered positions
        // or if target was CASH_ONLY
        if (position_entry_failed) {
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            log_system("⚠️  State forced to CASH_ONLY due to position entry failure");
        } else {
            current_state_ = decision.target_state;
        }
        bars_held_ = 0;
        entry_equity_ = decision.current_equity;

        // Final account status
        log_system("");
        log_system("✓ Transition Complete!");
        log_system("  New State: " + psm_.state_to_string(current_state_));
        log_system("  Entry Equity: $" + std::to_string(entry_equity_));
        log_system("");

        // Persist state immediately after transition
        persist_current_state();
    }

    // Calculate position allocations based on PSM state
    std::map<std::string, double> calculate_target_allocations(
        PositionStateMachine::State state, double capital) {

        std::map<std::string, double> allocations;

        // Map PSM states to SPY instrument allocations
        switch (state) {
            case PositionStateMachine::State::TQQQ_ONLY:
                // 3x bull → SPXL only
                allocations["SPXL"] = capital;
                break;

            case PositionStateMachine::State::QQQ_TQQQ:
                // Blended long → SPY (50%) + SPXL (50%)
                allocations["SPY"] = capital * 0.5;
                allocations["SPXL"] = capital * 0.5;
                break;

            case PositionStateMachine::State::QQQ_ONLY:
                // 1x base → SPY only
                allocations["SPY"] = capital;
                break;

            case PositionStateMachine::State::CASH_ONLY:
                // No positions
                break;

            case PositionStateMachine::State::PSQ_ONLY:
                // -1x bear → SH only
                allocations["SH"] = capital;
                break;

            case PositionStateMachine::State::PSQ_SQQQ:
                // Blended short → SH (50%) + SDS (50%)
                allocations["SH"] = capital * 0.5;
                allocations["SDS"] = capital * 0.5;
                break;

            case PositionStateMachine::State::SQQQ_ONLY:
                // -2x bear → SDS only
                allocations["SDS"] = capital;
                break;

            default:
                break;
        }

        // Convert dollar allocations to share quantities
        std::map<std::string, double> quantities;
        for (const auto& [symbol, dollar_amount] : allocations) {
            double price = 0.0;

            // In mock mode, use leveraged_prices_ for SH, SDS, SPXL
            if (is_mock_mode_ && (symbol == "SH" || symbol == "SDS" || symbol == "SPXL")) {
                // Get current bar timestamp
                auto spy_bars = bar_feed_->get_recent_bars("SPY", 1);
                if (spy_bars.empty()) {
                    throw std::runtime_error("CRITICAL: No SPY bars available for timestamp lookup");
                }

                uint64_t bar_ts_sec = spy_bars[0].timestamp_ms / 1000;

                // Crash fast if no price data (no silent failures!)
                if (!leveraged_prices_.count(bar_ts_sec)) {
                    throw std::runtime_error(
                        "CRITICAL: No leveraged ETF price data for timestamp " +
                        std::to_string(bar_ts_sec) + " when calculating " + symbol + " position");
                }

                if (!leveraged_prices_[bar_ts_sec].count(symbol)) {
                    throw std::runtime_error(
                        "CRITICAL: No price for " + symbol + " at timestamp " +
                        std::to_string(bar_ts_sec));
                }

                price = leveraged_prices_[bar_ts_sec].at(symbol);
            } else {
                // Get price from bar feed (SPY or live mode)
                auto bars = bar_feed_->get_recent_bars(symbol, 1);
                if (bars.empty() || bars[0].close <= 0) {
                    throw std::runtime_error(
                        "CRITICAL: No valid price for " + symbol + " from bar feed");
                }
                price = bars[0].close;
            }

            // Calculate shares
            if (price <= 0) {
                throw std::runtime_error(
                    "CRITICAL: Invalid price " + std::to_string(price) + " for " + symbol);
            }

            double shares = std::floor(dollar_amount / price);
            if (shares > 0) {
                quantities[symbol] = shares;
            }
        }

        return quantities;
    }

    void log_trade(const Order& order, uint64_t bar_index = 0, double cash_balance = 0.0,
                   double portfolio_value = 0.0, double trade_pnl = 0.0, const std::string& reason = "") {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["bar_index"] = bar_index;
        j["order_id"] = order.order_id;
        j["symbol"] = order.symbol;
        j["side"] = order.side;
        j["quantity"] = order.quantity;
        j["type"] = order.type;
        j["time_in_force"] = order.time_in_force;
        j["status"] = order.status;
        j["filled_qty"] = order.filled_qty;
        j["filled_avg_price"] = order.filled_avg_price;
        j["cash_balance"] = cash_balance;
        j["portfolio_value"] = portfolio_value;
        j["trade_pnl"] = trade_pnl;
        if (!reason.empty()) {
            j["reason"] = reason;
        }

        log_trades_ << j.dump() << std::endl;
        log_trades_.flush();
    }

    void log_signal(const Bar& bar, const Signal& signal) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["bar_timestamp_ms"] = bar.timestamp_ms;
        j["probability"] = signal.probability;
        j["confidence"] = signal.probability;
        j["prediction"] = signal.prediction;
        j["prob_1bar"] = signal.prob_1bar;
        j["prob_5bar"] = signal.prob_5bar;
        j["prob_10bar"] = signal.prob_10bar;

        log_signals_ << j.dump() << std::endl;
        log_signals_.flush();
    }

    void log_enhanced_decision(const Signal& signal, const Decision& decision) {
        log_system("");
        log_system("╔═══════════════════════════════════════════════════════════════");
        log_system("║ 📋 DECISION ANALYSIS");
        log_system("╠═══════════════════════════════════════════════════════════════");

        // Current state
        log_system("║ Current State: " + psm_.state_to_string(current_state_));
        log_system("║   - Bars Held: " + std::to_string(bars_held_) + " bars");
        log_system("║   - Min Hold: " + std::to_string(MIN_HOLD_BARS) + " bars required");
        log_system("║   - Position P&L: " + std::to_string(decision.position_pnl_pct * 100) + "%");
        log_system("║   - Current Equity: $" + std::to_string(decision.current_equity));
        log_system("║");

        // Signal analysis
        log_system("║ Signal Input:");
        log_system("║   - Probability: " + std::to_string(signal.probability));
        log_system("║   - Prediction: " + signal.prediction);
        log_system("║   - Confidence: " + std::to_string(signal.probability));
        log_system("║");

        // Target state mapping
        log_system("║ PSM Threshold Mapping:");
        if (signal.probability >= 0.68) {
            log_system("║   ✓ prob >= 0.68 → BULL_3X_ONLY (SPXL)");
        } else if (signal.probability >= 0.60) {
            log_system("║   ✓ 0.60 <= prob < 0.68 → BASE_BULL_3X (SPY+SPXL)");
        } else if (signal.probability >= 0.55) {
            log_system("║   ✓ 0.55 <= prob < 0.60 → BASE_ONLY (SPY)");
        } else if (signal.probability >= 0.49) {
            log_system("║   ✓ 0.49 <= prob < 0.55 → CASH_ONLY");
        } else if (signal.probability >= 0.45) {
            log_system("║   ✓ 0.45 <= prob < 0.49 → BEAR_1X_ONLY (SH)");
        } else if (signal.probability >= 0.35) {
            log_system("║   ✓ 0.35 <= prob < 0.45 → BEAR_1X_NX (SH+SDS)");
        } else {
            log_system("║   ✓ prob < 0.35 → BEAR_NX_ONLY (SDS)");
        }
        log_system("║   → Target State: " + psm_.state_to_string(decision.target_state));
        log_system("║");

        // Decision logic
        log_system("║ Decision Logic:");
        if (decision.profit_target_hit) {
            log_system("║   🎯 PROFIT TARGET HIT (" + std::to_string(decision.position_pnl_pct * 100) + "%)");
            log_system("║   → Force exit to CASH");
        } else if (decision.stop_loss_hit) {
            log_system("║   🛑 STOP LOSS HIT (" + std::to_string(decision.position_pnl_pct * 100) + "%)");
            log_system("║   → Force exit to CASH");
        } else if (decision.target_state == current_state_) {
            log_system("║   ✓ Target matches current state");
            log_system("║   → NO CHANGE (hold position)");
        } else if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
            log_system("║   ⏳ MIN HOLD PERIOD VIOLATED");
            log_system("║      - Currently held: " + std::to_string(bars_held_) + " bars");
            log_system("║      - Required: " + std::to_string(MIN_HOLD_BARS) + " bars");
            log_system("║      - Remaining: " + std::to_string(MIN_HOLD_BARS - bars_held_) + " bars");
            log_system("║   → BLOCKED (must wait)");
        } else {
            log_system("║   ✓ State transition approved");
            log_system("║      - Target differs from current");
            log_system("║      - Min hold satisfied or in CASH");
            log_system("║   → EXECUTE TRANSITION");
        }
        log_system("║");

        // Final decision
        if (decision.should_trade) {
            log_system("║ ✅ FINAL DECISION: TRADE");
            log_system("║    Transition: " + psm_.state_to_string(current_state_) +
                      " → " + psm_.state_to_string(decision.target_state));
        } else {
            log_system("║ ⏸️  FINAL DECISION: NO TRADE");
        }
        log_system("║    Reason: " + decision.reason);
        log_system("╚═══════════════════════════════════════════════════════════════");
        log_system("");
    }

    void log_decision(const Decision& decision) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["should_trade"] = decision.should_trade;
        j["current_state"] = psm_.state_to_string(current_state_);
        j["target_state"] = psm_.state_to_string(decision.target_state);
        j["reason"] = decision.reason;
        j["current_equity"] = decision.current_equity;
        j["position_pnl_pct"] = decision.position_pnl_pct;
        j["bars_held"] = bars_held_;

        log_decisions_ << j.dump() << std::endl;
        log_decisions_.flush();
    }

    void log_portfolio_state() {
        auto account = broker_->get_account();
        if (!account) return;

        auto positions = broker_->get_positions();

        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["cash"] = account->cash;
        j["buying_power"] = account->buying_power;
        j["portfolio_value"] = account->portfolio_value;
        j["equity"] = account->equity;
        j["total_return"] = account->portfolio_value - 100000.0;
        j["total_return_pct"] = (account->portfolio_value - 100000.0) / 100000.0;

        nlohmann::json positions_json = nlohmann::json::array();
        for (const auto& pos : positions) {
            nlohmann::json p;
            p["symbol"] = pos.symbol;
            p["quantity"] = pos.qty;
            p["avg_entry_price"] = pos.avg_entry_price;
            p["current_price"] = pos.current_price;
            p["market_value"] = pos.qty * pos.current_price;
            p["unrealized_pl"] = pos.unrealized_pnl;
            p["unrealized_pl_pct"] = pos.unrealized_pnl;
            positions_json.push_back(p);
        }
        j["positions"] = positions_json;

        log_positions_ << j.dump() << std::endl;
        log_positions_.flush();
    }

    // NEW: Convert Alpaca positions to BrokerPosition format for reconciliation
    std::vector<BrokerPosition> get_broker_positions() {
        auto alpaca_positions = broker_->get_positions();
        std::vector<BrokerPosition> broker_positions;

        for (const auto& pos : alpaca_positions) {
            BrokerPosition bp;
            bp.symbol = pos.symbol;
            bp.qty = static_cast<int64_t>(pos.qty);
            bp.avg_entry_price = pos.avg_entry_price;
            bp.current_price = pos.current_price;
            bp.unrealized_pnl = pos.unrealized_pnl;
            broker_positions.push_back(bp);
        }

        return broker_positions;
    }

    /**
     * Save comprehensive warmup data: historical bars + all of today's bars
     * This ensures optimization uses ALL available data up to current moment
     */
    std::string save_comprehensive_warmup_to_csv() {
        auto et_tm = et_time_.get_current_et_tm();
        std::string today = format_et_date(et_tm);

        std::string filename = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/comprehensive_warmup_" +
                               today + ".csv";

        std::ofstream csv(filename);
        if (!csv.is_open()) {
            log_error("Failed to open file for writing: " + filename);
            return "";
        }

        // Write CSV header
        csv << "timestamp,open,high,low,close,volume\n";

        log_system("Building comprehensive warmup data...");

        // Step 1: Load historical warmup bars (20 blocks = 7800 bars + 64 feature bars)
        std::string warmup_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_warmup_latest.csv";
        std::ifstream warmup_csv(warmup_file);

        if (!warmup_csv.is_open()) {
            log_error("Failed to open historical warmup file: " + warmup_file);
            log_error("Falling back to today's bars only");
        } else {
            std::string line;
            std::getline(warmup_csv, line);  // Skip header

            int historical_count = 0;
            while (std::getline(warmup_csv, line)) {
                // Filter: only include bars BEFORE today (to avoid duplicates)
                if (line.find(today) == std::string::npos) {
                    csv << line << "\n";
                    historical_count++;
                }
            }
            warmup_csv.close();

            log_system("  ✓ Historical bars: " + std::to_string(historical_count));
        }

        // Step 2: Append all of today's bars collected so far
        for (const auto& bar : todays_bars_) {
            csv << bar.timestamp_ms << ","
                << bar.open << ","
                << bar.high << ","
                << bar.low << ","
                << bar.close << ","
                << bar.volume << "\n";
        }

        csv.close();

        log_system("  ✓ Today's bars: " + std::to_string(todays_bars_.size()));
        log_system("✓ Comprehensive warmup saved: " + filename);

        return filename;
    }

    /**
     * Load optimized parameters from midday_selected_params.json
     */
    struct OptimizedParams {
        bool success{false};
        std::string source;
        // Phase 1 parameters
        double buy_threshold{0.55};
        double sell_threshold{0.45};
        double bb_amplification_factor{0.10};
        double ewrls_lambda{0.995};
        // Phase 2 parameters
        double h1_weight{0.3};
        double h5_weight{0.5};
        double h10_weight{0.2};
        int bb_period{20};
        double bb_std_dev{2.0};
        double bb_proximity{0.30};
        double regularization{0.01};
        double expected_mrb{0.0};
    };

    OptimizedParams load_optimized_parameters() {
        OptimizedParams params;

        std::string json_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/midday_selected_params.json";
        std::ifstream file(json_file);

        if (!file.is_open()) {
            log_error("Failed to open optimization results: " + json_file);
            return params;
        }

        try {
            nlohmann::json j;
            file >> j;
            file.close();

            params.success = true;
            params.source = j.value("source", "baseline");
            // Phase 1 parameters
            params.buy_threshold = j.value("buy_threshold", 0.55);
            params.sell_threshold = j.value("sell_threshold", 0.45);
            params.bb_amplification_factor = j.value("bb_amplification_factor", 0.10);
            params.ewrls_lambda = j.value("ewrls_lambda", 0.995);
            // Phase 2 parameters
            params.h1_weight = j.value("h1_weight", 0.3);
            params.h5_weight = j.value("h5_weight", 0.5);
            params.h10_weight = j.value("h10_weight", 0.2);
            params.bb_period = j.value("bb_period", 20);
            params.bb_std_dev = j.value("bb_std_dev", 2.0);
            params.bb_proximity = j.value("bb_proximity", 0.30);
            params.regularization = j.value("regularization", 0.01);
            params.expected_mrb = j.value("expected_mrb", 0.0);

            log_system("✓ Loaded optimized parameters from: " + json_file);
            log_system("  Source: " + params.source);
            log_system("  Phase 1 Parameters:");
            log_system("    buy_threshold: " + std::to_string(params.buy_threshold));
            log_system("    sell_threshold: " + std::to_string(params.sell_threshold));
            log_system("    bb_amplification_factor: " + std::to_string(params.bb_amplification_factor));
            log_system("    ewrls_lambda: " + std::to_string(params.ewrls_lambda));
            log_system("  Phase 2 Parameters:");
            log_system("    h1_weight: " + std::to_string(params.h1_weight));
            log_system("    h5_weight: " + std::to_string(params.h5_weight));
            log_system("    h10_weight: " + std::to_string(params.h10_weight));
            log_system("    bb_period: " + std::to_string(params.bb_period));
            log_system("    bb_std_dev: " + std::to_string(params.bb_std_dev));
            log_system("    bb_proximity: " + std::to_string(params.bb_proximity));
            log_system("    regularization: " + std::to_string(params.regularization));
            log_system("  Expected MRB: " + std::to_string(params.expected_mrb) + "%");

        } catch (const std::exception& e) {
            log_error("Failed to parse optimization results: " + std::string(e.what()));
            params.success = false;
        }

        return params;
    }

    /**
     * Update strategy configuration with new parameters
     */
    void update_strategy_parameters(const OptimizedParams& params) {
        log_system("📊 Updating strategy parameters...");

        // Create new config with optimized parameters
        auto config = create_v1_config();
        // Phase 1 parameters
        config.buy_threshold = params.buy_threshold;
        config.sell_threshold = params.sell_threshold;
        config.bb_amplification_factor = params.bb_amplification_factor;
        config.ewrls_lambda = params.ewrls_lambda;
        // Phase 2 parameters
        config.horizon_weights = {params.h1_weight, params.h5_weight, params.h10_weight};
        config.bb_period = params.bb_period;
        config.bb_std_dev = params.bb_std_dev;
        config.bb_proximity_threshold = params.bb_proximity;
        config.regularization = params.regularization;

        // Update strategy
        strategy_.update_config(config);

        log_system("✓ Strategy parameters updated with phase 1 + phase 2 optimizations");
    }

    /**
     * Run mid-day optimization at 15:15 PM ET (3:15pm)
     */
    void run_midday_optimization() {
        log_system("");
        log_system("════════════════════════════════════════════════");
        log_system("🔄 MID-DAY OPTIMIZATION TRIGGERED (15:15 PM ET / 3:15pm)");
        log_system("════════════════════════════════════════════════");
        log_system("");

        // Step 1: Save comprehensive warmup data (historical + today's bars)
        log_system("Step 1: Saving comprehensive warmup data to CSV...");
        std::string warmup_data_file = save_comprehensive_warmup_to_csv();
        if (warmup_data_file.empty()) {
            log_error("Failed to save warmup data - continuing with baseline parameters");
            return;
        }

        // Step 2: Call optimization script
        log_system("Step 2: Running Optuna optimization script...");
        log_system("  (This will take ~5 minutes for 50 trials)");

        std::string cmd = "/Volumes/ExternalSSD/Dev/C++/online_trader/tools/midday_optuna_relaunch.sh \"" +
                          warmup_data_file + "\" 2>&1 | tail -30";

        int exit_code = system(cmd.c_str());

        if (exit_code != 0) {
            log_error("Optimization script failed (exit code: " + std::to_string(exit_code) + ")");
            log_error("Continuing with baseline parameters");
            return;
        }

        log_system("✓ Optimization script completed");

        // Step 3: Load optimized parameters
        log_system("Step 3: Loading optimized parameters...");
        auto params = load_optimized_parameters();

        if (!params.success) {
            log_error("Failed to load optimized parameters - continuing with baseline");
            return;
        }

        // Step 4: Update strategy configuration
        log_system("Step 4: Updating strategy configuration...");
        update_strategy_parameters(params);

        log_system("");
        log_system("════════════════════════════════════════════════");
        log_system("✅ MID-DAY OPTIMIZATION COMPLETE");
        log_system("════════════════════════════════════════════════");
        log_system("  Parameters: " + params.source);
        log_system("  Expected MRB: " + std::to_string(params.expected_mrb) + "%");
        log_system("  Resuming trading at 14:46 PM ET");
        log_system("════════════════════════════════════════════════");
        log_system("");
    }
};

int LiveTradeCommand::execute(const std::vector<std::string>& args) {
    // Parse command-line flags
    bool is_mock = has_flag(args, "--mock");
    std::string mock_data_file = get_arg(args, "--mock-data", "");
    double mock_speed = std::stod(get_arg(args, "--mock-speed", "39.0"));

    // Log directory
    std::string log_dir = is_mock ? "logs/mock_trading" : "logs/live_trading";

    // Create broker and bar feed based on mode
    std::unique_ptr<IBrokerClient> broker;
    std::unique_ptr<IBarFeed> bar_feed;

    if (is_mock) {
        // ================================================================
        // MOCK MODE - Replay historical data
        // ================================================================
        if (mock_data_file.empty()) {
            std::cerr << "ERROR: --mock-data <file> is required in mock mode\n";
            std::cerr << "Example: sentio_cli live-trade --mock --mock-data /tmp/SPY_yesterday.csv\n";
            return 1;
        }

        std::cout << "🎭 MOCK MODE ENABLED\n";
        std::cout << "  Data file: " << mock_data_file << "\n";
        std::cout << "  Speed: " << mock_speed << "x real-time\n";
        std::cout << "  Logs: " << log_dir << "/\n";
        std::cout << "\n";

        // Create mock broker
        auto mock_broker = std::make_unique<MockBroker>(
            100000.0,  // initial_capital
            0.0        // commission_per_share (zero for testing)
        );
        mock_broker->set_fill_behavior(FillBehavior::IMMEDIATE_FULL);
        broker = std::move(mock_broker);

        // Create mock bar feed
        bar_feed = std::make_unique<MockBarFeedReplay>(
            mock_data_file,
            mock_speed
        );

    } else {
        // ================================================================
        // LIVE MODE - Real trading with Alpaca + Polygon
        // ================================================================

        // Read Alpaca credentials from environment
        const char* alpaca_key_env = std::getenv("ALPACA_PAPER_API_KEY");
        const char* alpaca_secret_env = std::getenv("ALPACA_PAPER_SECRET_KEY");

        if (!alpaca_key_env || !alpaca_secret_env) {
            std::cerr << "ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set\n";
            std::cerr << "Run: source config.env\n";
            return 1;
        }

        const std::string ALPACA_KEY = alpaca_key_env;
        const std::string ALPACA_SECRET = alpaca_secret_env;

        // Polygon API key
        const char* polygon_key_env = std::getenv("POLYGON_API_KEY");
        const std::string ALPACA_MARKET_DATA_URL = "wss://stream.data.alpaca.markets/v2/iex";
        const std::string POLYGON_KEY = polygon_key_env ? polygon_key_env : "";

        std::cout << "📈 LIVE MODE ENABLED\n";
        std::cout << "  Account: " << ALPACA_KEY.substr(0, 8) << "...\n";
        std::cout << "  Data source: Alpaca WebSocket (via Python bridge)\n";
        std::cout << "  Logs: " << log_dir << "/\n";
        std::cout << "\n";

        // Create live broker adapter
        broker = std::make_unique<AlpacaClientAdapter>(ALPACA_KEY, ALPACA_SECRET, true /* paper */);

        // Create live bar feed adapter (WebSocket via FIFO)
        bar_feed = std::make_unique<PolygonClientAdapter>(ALPACA_MARKET_DATA_URL, POLYGON_KEY);
    }

    // Create and run trader (same code path for both modes!)
    LiveTrader trader(std::move(broker), std::move(bar_feed), log_dir, is_mock, mock_data_file);
    trader.run();

    return 0;
}

void LiveTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli live-trade [options]\n\n";
    std::cout << "Run OnlineTrader v1.0 in live or mock mode\n\n";
    std::cout << "Options:\n";
    std::cout << "  --mock              Enable mock trading mode (replay historical data)\n";
    std::cout << "  --mock-data <file>  CSV file to replay (required with --mock)\n";
    std::cout << "  --mock-speed <x>    Replay speed multiplier (default: 39.0)\n\n";
    std::cout << "Trading Configuration:\n";
    std::cout << "  Instruments: SPY, SPXL (3x), SH (-1x), SDS (-2x)\n";
    std::cout << "  Hours: 9:30am - 3:58pm ET (regular hours only)\n";
    std::cout << "  Strategy: OnlineEnsemble v1.0 with asymmetric thresholds\n";
    std::cout << "  Warmup: 7,864 bars (20 blocks + 64 feature bars)\n\n";
    std::cout << "Logs:\n";
    std::cout << "  Live:  logs/live_trading/\n";
    std::cout << "  Mock:  logs/mock_trading/\n";
    std::cout << "  Files: system_*.log, signals_*.jsonl, trades_*.jsonl, decisions_*.jsonl\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Live trading\n";
    std::cout << "  sentio_cli live-trade\n\n";
    std::cout << "  # Mock trading (replay yesterday)\n";
    std::cout << "  tail -391 data/equities/SPY_RTH_NH.csv > /tmp/SPY_yesterday.csv\n";
    std::cout << "  sentio_cli live-trade --mock --mock-data /tmp/SPY_yesterday.csv\n\n";
    std::cout << "  # Mock trading at different speed\n";
    std::cout << "  sentio_cli live-trade --mock --mock-data yesterday.csv --mock-speed 100.0\n";
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 6 of 6**: tools/adaptive_optuna.py

**File Information**:
- **Path**: `tools/adaptive_optuna.py`
- **Size**: 772 lines
- **Modified**: 2025-10-09 15:15:22
- **Type**: py
- **Permissions**: -rwxr-xr-x

```text
#!/usr/bin/env python3
"""
Adaptive Optuna Framework for OnlineEnsemble Strategy

Implements three adaptive strategies for parameter optimization:
- Strategy A: Per-block adaptive (retune every block)
- Strategy B: 4-hour adaptive (retune twice daily)
- Strategy C: Static baseline (tune once, deploy fixed)

Author: Claude Code
Date: 2025-10-08
"""

import os
import sys
import json
import time
import subprocess
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from datetime import datetime

import optuna
import pandas as pd
import numpy as np


class AdaptiveOptunaFramework:
    """Framework for adaptive parameter optimization experiments."""

    def __init__(self, data_file: str, build_dir: str, output_dir: str, use_cache: bool = False, n_trials: int = 50, n_jobs: int = 4):  # DEPRECATED: No speedup
        self.data_file = data_file
        self.build_dir = build_dir
        self.output_dir = output_dir
        self.sentio_cli = os.path.join(build_dir, "sentio_cli")
        self.use_cache = use_cache
        self.n_trials = n_trials
        self.n_jobs = n_jobs

        # Create output directory
        Path(output_dir).mkdir(parents=True, exist_ok=True)

        # Load data to determine block structure
        self.df = pd.read_csv(data_file)
        self.total_bars = len(self.df)
        self.bars_per_block = 391  # 391 bars = 1 complete trading day (9:30 AM - 4:00 PM, inclusive)
        self.total_blocks = self.total_bars // self.bars_per_block

        print(f"[AdaptiveOptuna] Loaded {self.total_bars} bars")
        print(f"[AdaptiveOptuna] Total blocks: {self.total_blocks}")
        print(f"[AdaptiveOptuna] Bars per block: {self.bars_per_block}")
        print(f"[AdaptiveOptuna] Optuna trials: {self.n_trials}")
        print(f"[AdaptiveOptuna] Parallel jobs: {self.n_jobs}")

        # Feature caching for speedup (4-5x faster)
        self.features_cache = {}  # Maps data_file -> features_file
        if self.use_cache:
            print(f"[FeatureCache] Feature caching ENABLED (expect 4-5x speedup)")
        else:
            print(f"[FeatureCache] Feature caching DISABLED")

    def create_block_data(self, block_start: int, block_end: int,
                          output_file: str) -> str:
        """
        Extract specific blocks from data and save to CSV.

        Args:
            block_start: Starting block index (inclusive)
            block_end: Ending block index (exclusive)
            output_file: Path to save extracted data

        Returns:
            Path to created CSV file
        """
        start_bar = block_start * self.bars_per_block
        end_bar = block_end * self.bars_per_block

        # Extract bars with header
        block_df = self.df.iloc[start_bar:end_bar]

        # Extract symbol from original data_file and add to output filename
        # This ensures analyze-trades can detect the symbol
        import re
        symbol_match = re.search(r'(SPY|QQQ)', self.data_file, re.IGNORECASE)
        if symbol_match:
            symbol = symbol_match.group(1).upper()
            # Insert symbol before .csv extension
            output_file = output_file.replace('.csv', f'_{symbol}.csv')

        block_df.to_csv(output_file, index=False)

        print(f"[BlockData] Created {output_file}: blocks {block_start}-{block_end-1} "
              f"({len(block_df)} bars)")

        return output_file

    def extract_features_cached(self, data_file: str) -> str:
        """
        Extract features from data file and cache the result.

        Returns path to cached features CSV. If already extracted, returns cached path.
        This provides 4-5x speedup by avoiding redundant feature calculations.
        """
        if not self.use_cache:
            return None  # No caching, generate-signals will extract on-the-fly

        # Check if already cached
        if data_file in self.features_cache:
            print(f"[FeatureCache] Using existing cache for {os.path.basename(data_file)}")
            return self.features_cache[data_file]

        # Generate features file path
        features_file = data_file.replace('.csv', '_features.csv')

        # Check if features file already exists
        if os.path.exists(features_file):
            print(f"[FeatureCache] Found existing features: {os.path.basename(features_file)}")
            self.features_cache[data_file] = features_file
            return features_file

        # Extract features (one-time cost)
        print(f"[FeatureCache] Extracting features from {os.path.basename(data_file)}...")
        start_time = time.time()

        cmd = [
            self.sentio_cli, "extract-features",
            "--data", data_file,
            "--output", features_file
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Feature extraction failed: {result.stderr}")
                return None

            elapsed = time.time() - start_time
            print(f"[FeatureCache] Features extracted in {elapsed:.1f}s: {os.path.basename(features_file)}")

            # Cache the result
            self.features_cache[data_file] = features_file
            return features_file

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Feature extraction timed out")
            return None

    def run_backtest(self, data_file: str, params: Dict,
                     warmup_blocks: int = 2) -> Dict:
        """
        Run backtest with given parameters.

        Args:
            data_file: Path to data CSV
            params: Strategy parameters
            warmup_blocks: Number of blocks for warmup

        Returns:
            Dictionary with performance metrics
        """
        # Create temporary files
        signals_file = os.path.join(self.output_dir, "temp_signals.jsonl")
        trades_file = os.path.join(self.output_dir, "temp_trades.jsonl")
        equity_file = os.path.join(self.output_dir, "temp_equity.csv")

        # Calculate warmup bars
        warmup_bars = warmup_blocks * self.bars_per_block

        # Workaround: create symlinks for multi-instrument files expected by execute-trades
        # execute-trades expects SPY_RTH_NH.csv, SPXL_RTH_NH.csv, SH_RTH_NH.csv, SDS_RTH_NH.csv
        # in the same directory as the data file
        import shutil
        data_dir = os.path.dirname(data_file)
        data_basename = os.path.basename(data_file)

        # Detect symbol
        if 'SPY' in data_basename:
            symbol = 'SPY'
            instruments = ['SPY', 'SPXL', 'SH', 'SDS']
        elif 'QQQ' in data_basename:
            symbol = 'QQQ'
            instruments = ['QQQ', 'TQQQ', 'PSQ', 'SQQQ']
        else:
            print(f"[ERROR] Could not detect symbol from {data_basename}")
            return {'mrb': -999.0, 'error': 'unknown_symbol'}

        # Create copies of the data file for each instrument
        for inst in instruments:
            inst_path = os.path.join(data_dir, f"{inst}_RTH_NH.csv")
            if not os.path.exists(inst_path):
                shutil.copy(data_file, inst_path)

        # Extract features (one-time, cached)
        features_file = self.extract_features_cached(data_file)

        # Step 1: Generate signals (with optional feature cache)
        cmd_generate = [
            self.sentio_cli, "generate-signals",
            "--data", data_file,
            "--output", signals_file,
            "--warmup", str(warmup_bars),
            # Phase 1 parameters
            "--buy-threshold", str(params['buy_threshold']),
            "--sell-threshold", str(params['sell_threshold']),
            "--lambda", str(params['ewrls_lambda']),
            "--bb-amp", str(params['bb_amplification_factor'])
        ]

        # Phase 2 parameters (if present)
        if 'h1_weight' in params:
            cmd_generate.extend(["--h1-weight", str(params['h1_weight'])])
        if 'h5_weight' in params:
            cmd_generate.extend(["--h5-weight", str(params['h5_weight'])])
        if 'h10_weight' in params:
            cmd_generate.extend(["--h10-weight", str(params['h10_weight'])])
        if 'bb_period' in params:
            cmd_generate.extend(["--bb-period", str(params['bb_period'])])
        if 'bb_std_dev' in params:
            cmd_generate.extend(["--bb-std-dev", str(params['bb_std_dev'])])
        if 'bb_proximity' in params:
            cmd_generate.extend(["--bb-proximity", str(params['bb_proximity'])])
        if 'regularization' in params:
            cmd_generate.extend(["--regularization", str(params['regularization'])])

        # Add --features flag if caching enabled and features extracted
        if features_file:
            cmd_generate.extend(["--features", features_file])

        try:
            result = subprocess.run(
                cmd_generate,
                capture_output=True,
                text=True,
                timeout=300  # 5-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Signal generation failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Signal generation timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

        # Step 2: Execute trades
        cmd_execute = [
            self.sentio_cli, "execute-trades",
            "--signals", signals_file,
            "--data", data_file,
            "--output", trades_file,
            "--warmup", str(warmup_bars)
        ]

        try:
            result = subprocess.run(
                cmd_execute,
                capture_output=True,
                text=True,
                timeout=60  # 1-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Trade execution failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Trade execution timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

        # Step 3: Analyze performance
        # Calculate number of blocks in the data file for MRB
        num_bars = len(pd.read_csv(data_file))
        num_blocks = num_bars // self.bars_per_block

        cmd_analyze = [
            self.sentio_cli, "analyze-trades",
            "--trades", trades_file,
            "--data", data_file,
            "--output", equity_file,
            "--blocks", str(num_blocks)  # Pass blocks for MRB calculation
        ]

        try:
            result = subprocess.run(
                cmd_analyze,
                capture_output=True,
                text=True,
                timeout=60
            )

            if result.returncode != 0:
                print(f"[ERROR] Analysis failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

            # Parse MRD (Mean Return per Day) from output
            # Look for: "Mean Return per Day (MRD): +0.0025% (20 trading days)"
            mrd = None
            mrb = None

            for line in result.stdout.split('\n'):
                if 'Mean Return per Day' in line and 'MRD' in line:
                    # Extract the percentage value
                    import re
                    match = re.search(r'([+-]?\d+\.\d+)%', line)
                    if match:
                        mrd = float(match.group(1))

                if 'Mean Return per Block' in line and 'MRB' in line:
                    import re
                    match = re.search(r'([+-]?\d+\.\d+)%', line)
                    if match:
                        mrb = float(match.group(1))

            # Primary metric is MRD (for daily reset strategies)
            if mrd is not None:
                return {
                    'mrd': mrd,
                    'mrb': mrb if mrb is not None else 0.0,
                    'trades_file': trades_file,
                    'equity_file': equity_file
                }

            # Fallback: Calculate from equity file
            if os.path.exists(equity_file):
                equity_df = pd.read_csv(equity_file)
                if len(equity_df) > 0:
                    # Calculate MRB manually
                    total_return = (equity_df['equity'].iloc[-1] - 100000) / 100000
                    num_blocks = len(equity_df) // self.bars_per_block
                    mrb = (total_return / num_blocks) * 100 if num_blocks > 0 else 0.0
                    return {'mrb': mrb, 'mrd': mrb}  # Use MRB as fallback for MRD

            return {'mrd': 0.0, 'mrb': 0.0, 'error': 'MRD not found'}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Analysis timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

    def tune_on_window(self, block_start: int, block_end: int,
                       n_trials: int = 100, phase2_center: Dict = None) -> Tuple[Dict, float, float]:
        """
        Tune parameters on specified block window.

        Args:
            block_start: Starting block (inclusive)
            block_end: Ending block (exclusive)
            n_trials: Number of Optuna trials
            phase2_center: If provided, use narrow ranges around these params (Phase 2 micro-tuning)

        Returns:
            (best_params, best_mrb, tuning_time_seconds)
        """
        phase_label = "PHASE 2 (micro-tuning)" if phase2_center else "PHASE 1 (wide search)"
        print(f"\n[Tuning] {phase_label} - Blocks {block_start}-{block_end-1} ({n_trials} trials)")
        if phase2_center:
            print(f"[Phase2] Center params: buy={phase2_center.get('buy_threshold', 0.53):.3f}, "
                  f"sell={phase2_center.get('sell_threshold', 0.48):.3f}, "
                  f"λ={phase2_center.get('ewrls_lambda', 0.992):.4f}, "
                  f"BB={phase2_center.get('bb_amplification_factor', 0.05):.3f}")

        # Create data file for this window
        train_data = os.path.join(
            self.output_dir,
            f"train_blocks_{block_start}_{block_end}.csv"
        )
        train_data = self.create_block_data(block_start, block_end, train_data)

        # Pre-extract features for all trials (one-time cost, 4-5x speedup)
        if self.use_cache:
            self.extract_features_cached(train_data)

        # Define Optuna objective
        def objective(trial):
            if phase2_center is None:
                # PHASE 1: Optimize primary parameters (EXPANDED RANGES for 0.5% MRB target)
                params = {
                    'buy_threshold': trial.suggest_float('buy_threshold', 0.50, 0.65, step=0.01),
                    'sell_threshold': trial.suggest_float('sell_threshold', 0.35, 0.50, step=0.01),
                    'ewrls_lambda': trial.suggest_float('ewrls_lambda', 0.985, 0.999, step=0.001),
                    'bb_amplification_factor': trial.suggest_float('bb_amplification_factor',
                                                                   0.00, 0.20, step=0.01)
                }

                # Ensure asymmetric thresholds (buy > sell)
                if params['buy_threshold'] <= params['sell_threshold']:
                    return -999.0

            else:
                # PHASE 2: Optimize secondary parameters (FIX Phase 1 params at best values)
                # Use best Phase 1 parameters as FIXED

                # Sample only 2 weights, compute 3rd to ensure sum = 1.0
                h1_weight = trial.suggest_float('h1_weight', 0.1, 0.6, step=0.05)
                h5_weight = trial.suggest_float('h5_weight', 0.2, 0.7, step=0.05)
                h10_weight = 1.0 - h1_weight - h5_weight

                # Reject if h10 is out of valid range [0.1, 0.5]
                if h10_weight < 0.05 or h10_weight > 0.6:
                    return -999.0

                params = {
                    # Phase 1 params FIXED at best values
                    'buy_threshold': phase2_center.get('buy_threshold', 0.53),
                    'sell_threshold': phase2_center.get('sell_threshold', 0.48),
                    'ewrls_lambda': phase2_center.get('ewrls_lambda', 0.992),
                    'bb_amplification_factor': phase2_center.get('bb_amplification_factor', 0.05),

                    # Phase 2 params OPTIMIZED (weights guaranteed to sum to 1.0) - EXPANDED RANGES
                    'h1_weight': h1_weight,
                    'h5_weight': h5_weight,
                    'h10_weight': h10_weight,
                    'bb_period': trial.suggest_int('bb_period', 5, 40, step=5),
                    'bb_std_dev': trial.suggest_float('bb_std_dev', 1.0, 3.0, step=0.25),
                    'bb_proximity': trial.suggest_float('bb_proximity', 0.10, 0.50, step=0.05),
                    'regularization': trial.suggest_float('regularization', 0.0, 0.10, step=0.005)
                }

            result = self.run_backtest(train_data, params, warmup_blocks=2)

            # Log trial (use MRD as primary metric)
            mrd = result.get('mrd', result.get('mrb', 0.0))
            mrb = result.get('mrb', 0.0)
            print(f"  Trial {trial.number}: MRD={mrd:.4f}% (MRB={mrb:.4f}%) "
                  f"buy={params['buy_threshold']:.2f} "
                  f"sell={params['sell_threshold']:.2f}")

            return mrd  # Optimize for MRD (daily returns)

        # Run Optuna optimization
        start_time = time.time()

        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )

        # Run optimization with parallel trials
        print(f"[Optuna] Running {n_trials} trials with {self.n_jobs} parallel jobs")
        study.optimize(objective, n_trials=n_trials, n_jobs=self.n_jobs, show_progress_bar=True)

        tuning_time = time.time() - start_time

        best_params = study.best_params
        best_mrd = study.best_value

        print(f"[Tuning] Complete in {tuning_time:.1f}s")
        print(f"[Tuning] Best MRD: {best_mrd:.4f}%")
        print(f"[Tuning] Best params: {best_params}")

        return best_params, best_mrd, tuning_time

    def test_on_window(self, params: Dict, block_start: int,
                       block_end: int) -> Dict:
        """
        Test parameters on specified block window.

        Args:
            params: Strategy parameters
            block_start: Starting block (inclusive)
            block_end: Ending block (exclusive)

        Returns:
            Dictionary with test results
        """
        print(f"[Testing] Blocks {block_start}-{block_end-1} with params: {params}")

        # Create test data file
        test_data = os.path.join(
            self.output_dir,
            f"test_blocks_{block_start}_{block_end}.csv"
        )
        test_data = self.create_block_data(block_start, block_end, test_data)

        # Run backtest
        result = self.run_backtest(test_data, params, warmup_blocks=2)

        mrd = result.get('mrd', result.get('mrb', 0.0))
        mrb = result.get('mrb', 0.0)
        print(f"[Testing] MRD: {mrd:.4f}% | MRB: {mrb:.4f}%")

        return {
            'block_start': block_start,
            'block_end': block_end,
            'params': params,
            'mrd': mrd,
            'mrb': mrb
        }

    def strategy_a_per_block(self, start_block: int = 10,
                             test_horizon: int = 5) -> List[Dict]:
        """
        Strategy A: Per-block adaptive.

        Retunes parameters after every block, tests on next 5 blocks.

        Args:
            start_block: First block to start tuning from
            test_horizon: Number of blocks to test (5 blocks = ~5 days)

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY A: PER-BLOCK ADAPTIVE")
        print("="*80)

        results = []

        # Need at least start_block blocks for training + test_horizon for testing
        max_test_block = self.total_blocks - test_horizon

        for block_idx in range(start_block, max_test_block):
            print(f"\n--- Block {block_idx}/{max_test_block-1} ---")

            # Tune on last 10 blocks
            train_start = max(0, block_idx - 10)
            train_end = block_idx

            params, train_mrb, tuning_time = self.tune_on_window(
                train_start, train_end, n_trials=self.n_trials
            )

            # Test on next 5 blocks
            test_start = block_idx
            test_end = block_idx + test_horizon

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = train_start
            test_result['train_end'] = train_end

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_a_partial')

        return results

    def strategy_b_4hour(self, start_block: int = 20,
                         retune_frequency: int = 2,
                         test_horizon: int = 5) -> List[Dict]:
        """
        Strategy B: 4-hour adaptive (retune every 2 blocks).

        Args:
            start_block: First block to start from
            retune_frequency: Retune every N blocks (2 = twice daily)
            test_horizon: Number of blocks to test

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY B: 4-HOUR ADAPTIVE")
        print("="*80)

        results = []
        max_test_block = self.total_blocks - test_horizon

        current_params = None

        for block_idx in range(start_block, max_test_block, retune_frequency):
            print(f"\n--- Block {block_idx}/{max_test_block-1} ---")

            # Tune on last 20 blocks
            train_start = max(0, block_idx - 20)
            train_end = block_idx

            params, train_mrb, tuning_time = self.tune_on_window(
                train_start, train_end, n_trials=self.n_trials
            )
            current_params = params

            # Test on next 5 blocks
            test_start = block_idx
            test_end = min(block_idx + test_horizon, self.total_blocks)

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = train_start
            test_result['train_end'] = train_end

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_b_partial')

        return results

    def strategy_c_static(self, train_blocks: int = 20,
                          test_horizon: int = 5) -> List[Dict]:
        """
        Strategy C: Static baseline.

        Tune once on first N blocks, then test on all remaining blocks.

        Args:
            train_blocks: Number of blocks to train on
            test_horizon: Number of blocks per test window

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY C: STATIC BASELINE")
        print("="*80)

        # Tune once on first train_blocks
        print(f"\n--- Tuning on first {train_blocks} blocks ---")
        params, train_mrb, tuning_time = self.tune_on_window(
            0, train_blocks, n_trials=self.n_trials
        )

        print(f"\n[Static] Using fixed params for all tests: {params}")

        results = []

        # Test on all remaining blocks in test_horizon windows
        for block_idx in range(train_blocks, self.total_blocks - test_horizon,
                               test_horizon):
            print(f"\n--- Testing blocks {block_idx}-{block_idx+test_horizon-1} ---")

            test_start = block_idx
            test_end = block_idx + test_horizon

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time if block_idx == train_blocks else 0.0
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = 0
            test_result['train_end'] = train_blocks

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_c_partial')

        return results

    def _save_results(self, results: List[Dict], filename: str):
        """Save results to JSON file."""
        output_file = os.path.join(self.output_dir, f"{filename}.json")
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"[Results] Saved to {output_file}")

    def run_strategy(self, strategy: str) -> List[Dict]:
        """
        Run specified strategy.

        Args:
            strategy: 'A', 'B', or 'C'

        Returns:
            List of test results
        """
        if strategy == 'A':
            return self.strategy_a_per_block()
        elif strategy == 'B':
            return self.strategy_b_4hour()
        elif strategy == 'C':
            return self.strategy_c_static()
        else:
            raise ValueError(f"Unknown strategy: {strategy}")


def main():
    parser = argparse.ArgumentParser(
        description="Adaptive Optuna Framework for OnlineEnsemble"
    )
    parser.add_argument('--strategy', choices=['A', 'B', 'C'], required=True,
                        help='Strategy to run: A (per-block), B (4-hour), C (static)')
    parser.add_argument('--data', required=True,
                        help='Path to data CSV file')
    parser.add_argument('--build-dir', default='build',
                        help='Path to build directory')
    parser.add_argument('--output', required=True,
                        help='Path to output JSON file')
    parser.add_argument('--n-trials', type=int, default=50,
                        help='Number of Optuna trials (default: 50)')
    parser.add_argument('--n-jobs', type=int, default=4,
                        help='Number of parallel jobs (default: 4 for 4x speedup)')

    args = parser.parse_args()

    # Determine project root and build directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    build_dir = project_root / args.build_dir
    output_dir = project_root / "data" / "tmp" / "ab_test_results"

    print("="*80)
    print("ADAPTIVE OPTUNA FRAMEWORK")
    print("="*80)
    print(f"Strategy: {args.strategy}")
    print(f"Data: {args.data}")
    print(f"Build: {build_dir}")
    print(f"Output: {args.output}")
    print("="*80)

    # Create framework
    framework = AdaptiveOptunaFramework(
        data_file=args.data,
        build_dir=str(build_dir),
        output_dir=str(output_dir),
        n_trials=args.n_trials,
        n_jobs=args.n_jobs
    )

    # Run strategy
    start_time = time.time()
    results = framework.run_strategy(args.strategy)
    total_time = time.time() - start_time

    # Calculate summary statistics
    mrbs = [r['mrb'] for r in results]

    # Handle empty results
    if len(mrbs) == 0 or all(m == -999.0 for m in mrbs):
        summary = {
            'strategy': args.strategy,
            'total_tests': len(results),
            'mean_mrb': 0.0,
            'std_mrb': 0.0,
            'min_mrb': 0.0,
            'max_mrb': 0.0,
            'total_time': total_time,
            'results': results,
            'error': 'All tests failed'
        }
    else:
        # Filter out failed trials
        valid_mrbs = [m for m in mrbs if m != -999.0]
        summary = {
            'strategy': args.strategy,
            'total_tests': len(results),
            'mean_mrb': np.mean(valid_mrbs) if valid_mrbs else 0.0,
            'std_mrb': np.std(valid_mrbs) if valid_mrbs else 0.0,
            'min_mrb': np.min(valid_mrbs) if valid_mrbs else 0.0,
            'max_mrb': np.max(valid_mrbs) if valid_mrbs else 0.0,
            'total_time': total_time,
            'results': results
        }

    # Save final results
    with open(args.output, 'w') as f:
        json.dump(summary, f, indent=2)

    print("\n" + "="*80)
    print("SUMMARY")
    print("="*80)
    print(f"Strategy: {args.strategy}")
    print(f"Total tests: {len(results)}")
    print(f"Mean MRB: {summary['mean_mrb']:.4f}%")
    print(f"Std MRB: {summary['std_mrb']:.4f}%")
    print(f"Min MRB: {summary['min_mrb']:.4f}%")
    print(f"Max MRB: {summary['max_mrb']:.4f}%")
    print(f"Total time: {total_time/60:.1f} minutes")
    print(f"Results saved to: {args.output}")
    print("="*80)


if __name__ == '__main__':
    main()

```

