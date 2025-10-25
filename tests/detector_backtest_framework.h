/**
 * Detector Backtesting Framework
 *
 * Common infrastructure for testing proposed detectors on historical data
 */

#ifndef DETECTOR_BACKTEST_FRAMEWORK_H
#define DETECTOR_BACKTEST_FRAMEWORK_H

#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <iomanip>
#include "../src/types.h"
#include "../src/data_loader.h"

struct Trade {
    std::string symbol;
    int entry_bar;
    int exit_bar;
    double entry_price;
    double exit_price;
    int direction;  // 1 = long, -1 = short
    double pnl_pct;
    std::string reason;  // Entry/exit reason
};

struct BacktestMetrics {
    int total_signals = 0;
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double total_pnl_pct = 0.0;
    double avg_win_pct = 0.0;
    double avg_loss_pct = 0.0;
    double win_rate = 0.0;
    double profit_factor = 0.0;
    double sharpe_ratio = 0.0;
    double max_drawdown_pct = 0.0;
    double avg_bars_in_trade = 0.0;

    std::vector<double> daily_returns;
    std::map<std::string, int> signal_counts;

    void calculate_derived_metrics() {
        if (total_trades > 0) {
            win_rate = (double)winning_trades / total_trades * 100.0;
            avg_bars_in_trade = 0.0; // Calculated separately
        }

        double total_wins = 0.0;
        double total_losses = 0.0;

        if (winning_trades > 0) {
            avg_win_pct = total_wins / winning_trades;
        }
        if (losing_trades > 0) {
            avg_loss_pct = total_losses / losing_trades;
        }

        if (std::abs(total_losses) > 0.001) {
            profit_factor = total_wins / std::abs(total_losses);
        }

        // Calculate Sharpe from daily returns
        if (daily_returns.size() > 1) {
            double mean = 0.0;
            for (double ret : daily_returns) {
                mean += ret;
            }
            mean /= daily_returns.size();

            double variance = 0.0;
            for (double ret : daily_returns) {
                variance += (ret - mean) * (ret - mean);
            }
            variance /= (daily_returns.size() - 1);
            double std_dev = std::sqrt(variance);

            if (std_dev > 0.0001) {
                // Annualized Sharpe (252 trading days, intraday trades)
                sharpe_ratio = (mean / std_dev) * std::sqrt(252.0);
            }
        }
    }

    void print_summary(const std::string& detector_name) const {
        std::cout << "\n═══════════════════════════════════════════════════════════\n";
        std::cout << "  " << detector_name << " - Backtest Results\n";
        std::cout << "═══════════════════════════════════════════════════════════\n\n";

        std::cout << "SIGNAL STATISTICS:\n";
        std::cout << "  Total Signals:       " << total_signals << "\n";
        std::cout << "  Signals/Day:         " << std::fixed << std::setprecision(1)
                  << (total_signals / std::max(1, (int)daily_returns.size())) << "\n\n";

        std::cout << "TRADE STATISTICS:\n";
        std::cout << "  Total Trades:        " << total_trades << "\n";
        std::cout << "  Winning Trades:      " << winning_trades << "\n";
        std::cout << "  Losing Trades:       " << losing_trades << "\n";
        std::cout << "  Win Rate:            " << std::fixed << std::setprecision(1)
                  << win_rate << "%\n\n";

        std::cout << "PERFORMANCE:\n";
        std::cout << "  Total P&L:           " << std::fixed << std::setprecision(2)
                  << total_pnl_pct << "%\n";
        std::cout << "  Avg Win:             " << std::fixed << std::setprecision(2)
                  << avg_win_pct << "%\n";
        std::cout << "  Avg Loss:            " << std::fixed << std::setprecision(2)
                  << avg_loss_pct << "%\n";
        std::cout << "  Profit Factor:       " << std::fixed << std::setprecision(2)
                  << profit_factor << "\n";
        std::cout << "  Sharpe Ratio:        " << std::fixed << std::setprecision(2)
                  << sharpe_ratio << "\n";
        std::cout << "  Max Drawdown:        " << std::fixed << std::setprecision(2)
                  << max_drawdown_pct << "%\n\n";

        std::cout << "EVALUATION:\n";
        bool pass_win_rate = win_rate >= 52.0;
        bool pass_sharpe = sharpe_ratio >= 1.0;
        bool pass_drawdown = max_drawdown_pct <= 15.0;
        bool pass_profit_factor = profit_factor >= 1.3;

        std::cout << "  Win Rate ≥52%:       " << (pass_win_rate ? "✓ PASS" : "✗ FAIL") << "\n";
        std::cout << "  Sharpe ≥1.0:         " << (pass_sharpe ? "✓ PASS" : "✗ FAIL") << "\n";
        std::cout << "  MaxDD ≤15%:          " << (pass_drawdown ? "✓ PASS" : "✗ FAIL") << "\n";
        std::cout << "  Profit Factor ≥1.3:  " << (pass_profit_factor ? "✓ PASS" : "✗ FAIL") << "\n\n";

        int passes = pass_win_rate + pass_sharpe + pass_drawdown + pass_profit_factor;
        std::cout << "OVERALL: " << passes << "/4 criteria passed\n";

        if (passes >= 3) {
            std::cout << "→ RECOMMENDATION: STRONG CANDIDATE for integration\n";
        } else if (passes >= 2) {
            std::cout << "→ RECOMMENDATION: MODERATE CANDIDATE - needs optimization\n";
        } else {
            std::cout << "→ RECOMMENDATION: WEAK CANDIDATE - reconsider or reject\n";
        }

        std::cout << "═══════════════════════════════════════════════════════════\n\n";
    }
};

class DetectorBacktester {
private:
    std::map<Symbol, std::vector<Bar>> historical_data;
    std::vector<Trade> trades;
    BacktestMetrics metrics;

    int max_bars_in_trade = 20;  // Force exit after 20 bars
    double stop_loss_pct = 2.0;   // Stop loss at 2%
    double take_profit_pct = 3.0; // Take profit at 3%

public:
    DetectorBacktester() {}

    bool load_data(const std::vector<Symbol>& symbols, const std::string& data_dir = "data") {
        DataLoader loader;

        for (const auto& symbol : symbols) {
            std::string filename = data_dir + "/" + symbol + "_RTH_NH.bin";
            auto bars = loader.load_symbol(filename);

            if (bars.empty()) {
                std::cerr << "Failed to load data for " << symbol << "\n";
                return false;
            }

            historical_data[symbol] = bars;
            std::cout << "Loaded " << bars.size() << " bars for " << symbol << "\n";
        }

        return !historical_data.empty();
    }

    template<typename DetectorType>
    void run_backtest(DetectorType& detector, const Symbol& symbol,
                     int start_bar, int end_bar) {

        if (historical_data.find(symbol) == historical_data.end()) {
            std::cerr << "No data for symbol: " << symbol << "\n";
            return;
        }

        const auto& bars = historical_data[symbol];

        Trade* active_trade = nullptr;

        for (int i = start_bar; i < std::min(end_bar, (int)bars.size()); i++) {
            // Update detector
            detector.update(bars[i],
                          i > 0 ? &bars[i-1] : nullptr,
                          std::vector<Bar>(bars.begin() + std::max(0, i - 100),
                                         bars.begin() + i));

            // Check for signal
            int signal = detector.get_signal();

            if (signal != 0) {
                metrics.total_signals++;
            }

            // Manage active trade
            if (active_trade) {
                int bars_held = i - active_trade->entry_bar;
                double current_pnl_pct = active_trade->direction *
                    ((bars[i].close - active_trade->entry_price) / active_trade->entry_price) * 100.0;

                // Exit conditions
                bool hit_stop = (current_pnl_pct <= -stop_loss_pct);
                bool hit_target = (current_pnl_pct >= take_profit_pct);
                bool max_time = (bars_held >= max_bars_in_trade);
                bool detector_exit = (signal == -active_trade->direction);

                if (hit_stop || hit_target || max_time || detector_exit) {
                    // Close trade
                    active_trade->exit_bar = i;
                    active_trade->exit_price = bars[i].close;
                    active_trade->pnl_pct = current_pnl_pct;

                    if (hit_stop) active_trade->reason = "stop_loss";
                    else if (hit_target) active_trade->reason = "take_profit";
                    else if (max_time) active_trade->reason = "time_stop";
                    else active_trade->reason = "detector_exit";

                    metrics.total_trades++;
                    metrics.total_pnl_pct += current_pnl_pct;

                    if (current_pnl_pct > 0) {
                        metrics.winning_trades++;
                    } else {
                        metrics.losing_trades++;
                    }

                    trades.push_back(*active_trade);
                    delete active_trade;
                    active_trade = nullptr;
                }
            }

            // Enter new trade if signal and no active trade
            if (!active_trade && signal != 0) {
                active_trade = new Trade();
                active_trade->symbol = symbol;
                active_trade->entry_bar = i;
                active_trade->entry_price = bars[i].close;
                active_trade->direction = signal;
                active_trade->reason = "detector_signal";
            }
        }

        // Close any remaining active trade
        if (active_trade) {
            active_trade->exit_bar = end_bar - 1;
            active_trade->exit_price = bars[end_bar - 1].close;
            active_trade->pnl_pct = active_trade->direction *
                ((active_trade->exit_price - active_trade->entry_price) / active_trade->entry_price) * 100.0;
            active_trade->reason = "end_of_test";

            metrics.total_trades++;
            metrics.total_pnl_pct += active_trade->pnl_pct;

            if (active_trade->pnl_pct > 0) {
                metrics.winning_trades++;
            } else {
                metrics.losing_trades++;
            }

            trades.push_back(*active_trade);
            delete active_trade;
        }
    }

    const BacktestMetrics& get_metrics() {
        metrics.calculate_derived_metrics();
        return metrics;
    }

    const std::vector<Trade>& get_trades() const {
        return trades;
    }

    void export_trades(const std::string& filename) const {
        std::ofstream out(filename);
        out << "Symbol,EntryBar,ExitBar,EntryPrice,ExitPrice,Direction,PnL%,Reason\n";

        for (const auto& trade : trades) {
            out << trade.symbol << ","
                << trade.entry_bar << ","
                << trade.exit_bar << ","
                << trade.entry_price << ","
                << trade.exit_price << ","
                << trade.direction << ","
                << trade.pnl_pct << ","
                << trade.reason << "\n";
        }

        out.close();
        std::cout << "Exported " << trades.size() << " trades to " << filename << "\n";
    }
};

#endif // DETECTOR_BACKTEST_FRAMEWORK_H
