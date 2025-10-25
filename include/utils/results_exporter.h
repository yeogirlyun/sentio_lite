#pragma once
#include "trading/multi_symbol_trader.h"
#include "core/types.h"
#include <string>
#include <fstream>
#include <iomanip>
#include <unordered_map>

namespace trading {

/**
 * Export trading results to JSON format for dashboard generation
 */
class ResultsExporter {
public:
    /**
     * Export results to JSON file
     * @param results Backtest results
     * @param trader Trader instance (for trade details)
     * @param output_path Output JSON file path
     * @param metadata Additional metadata (symbol list, config, etc.)
     */
    static void export_json(
        const MultiSymbolTrader::BacktestResults& results,
        const MultiSymbolTrader& trader,
        const std::string& output_path,
        const std::string& symbols_str,
        const std::string& mode_str,
        const std::string& start_date,
        const std::string& end_date,
        const std::unordered_map<Symbol, std::vector<Bar>>& bars_by_symbol
    ) {
        std::ofstream file(output_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot create results file: " + output_path);
        }

        file << std::fixed << std::setprecision(4);

        file << "{\n";
        file << "  \"metadata\": {\n";
        file << "    \"timestamp\": \"" << get_current_timestamp() << "\",\n";
        file << "    \"mode\": \"" << mode_str << "\",\n";
        file << "    \"symbols\": \"" << symbols_str << "\",\n";
        file << "    \"start_date\": \"" << start_date << "\",\n";
        file << "    \"end_date\": \"" << end_date << "\",\n";
        file << "    \"initial_capital\": " << trader.config().initial_capital << "\n";
        file << "  },\n";

        file << "  \"performance\": {\n";
        file << "    \"final_equity\": " << results.final_equity << ",\n";
        file << "    \"total_return\": " << results.total_return << ",\n";
        file << "    \"mrd\": " << results.mrd << ",\n";
        file << "    \"total_trades\": " << results.total_trades << ",\n";
        file << "    \"winning_trades\": " << results.winning_trades << ",\n";
        file << "    \"losing_trades\": " << results.losing_trades << ",\n";
        file << "    \"win_rate\": " << results.win_rate << ",\n";
        file << "    \"avg_win\": " << results.avg_win << ",\n";
        file << "    \"avg_loss\": " << results.avg_loss << ",\n";
        file << "    \"profit_factor\": " << results.profit_factor << ",\n";
        file << "    \"max_drawdown\": " << results.max_drawdown << "\n";
        file << "  },\n";

        file << "  \"config\": {\n";
        file << "    \"strategy_name\": \"" << get_strategy_display_name(trader.config().strategy) << "\",\n";
        file << "    \"max_positions\": " << trader.config().max_positions << ",\n";
        file << "    \"lambda_2bar\": " << trader.config().horizon_config.lambda_2bar << ",\n";
        file << "    \"min_prediction_for_entry\": " << trader.config().min_prediction_for_entry << ",\n";
        file << "    \"min_prediction_increase_on_trade\": " << trader.config().min_prediction_increase_on_trade << ",\n";
        file << "    \"min_prediction_decrease_on_no_trade\": " << trader.config().min_prediction_decrease_on_no_trade << ",\n";
        file << "    \"min_bars_to_learn\": " << trader.config().min_bars_to_learn << ",\n";
        file << "    \"bars_per_day\": " << trader.config().bars_per_day << ",\n";
        file << "    \"initial_capital\": " << trader.config().initial_capital << ",\n";
        file << "    \"lookback_window\": " << trader.config().lookback_window << ",\n";
        file << "    \"win_multiplier\": " << trader.config().win_multiplier << ",\n";
        file << "    \"loss_multiplier\": " << trader.config().loss_multiplier << ",\n";
        file << "    \"rotation_strength_delta\": " << trader.config().rotation_strength_delta << ",\n";
        file << "    \"min_rank_strength\": " << trader.config().min_rank_strength << ",\n";
        // Include SIGOR model parameters if applicable
        if (trader.config().strategy == StrategyType::SIGOR) {
            const auto& s = trader.config().sigor_config;
            file << "    \"sigor\": {\n";
            file << "      \"k\": " << s.k << ",\n";
            file << "      \"w_boll\": " << s.w_boll << ",\n";
            file << "      \"w_rsi\": " << s.w_rsi << ",\n";
            file << "      \"w_mom\": " << s.w_mom << ",\n";
            file << "      \"w_vwap\": " << s.w_vwap << ",\n";
            file << "      \"w_orb\": " << s.w_orb << ",\n";
            file << "      \"w_ofi\": " << s.w_ofi << ",\n";
            file << "      \"w_vol\": " << s.w_vol << ",\n";
            file << "      \"win_boll\": " << s.win_boll << ",\n";
            file << "      \"win_rsi\": " << s.win_rsi << ",\n";
            file << "      \"win_mom\": " << s.win_mom << ",\n";
            file << "      \"win_vwap\": " << s.win_vwap << ",\n";
            file << "      \"orb_opening_bars\": " << s.orb_opening_bars << ",\n";
            file << "      \"vol_window\": " << s.vol_window << ",\n";
            file << "      \"warmup_bars\": " << s.warmup_bars << "\n";
            file << "    }\n";
        } else {
            // Remove trailing comma by closing JSON object properly
        }
        file << "  },\n";

        // ===== TRADES (embed complete trade log) =====
        file << "  \"trades\": [\n";
        const auto all_trades = trader.get_all_trades();
        for (size_t i = 0; i < all_trades.size(); ++i) {
            const auto& t = all_trades[i];
            auto entry_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.entry_time.time_since_epoch()).count();
            auto exit_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.exit_time.time_since_epoch()).count();
            file << "    {"
                 << "\"symbol\":\"" << t.symbol << "\",";
            file << "\"entry_time_ms\":" << entry_ms << ",";
            file << "\"exit_time_ms\":" << exit_ms << ",";
            file << "\"entry_bar_id\":" << t.entry_bar_id << ",";
            file << "\"exit_bar_id\":" << t.exit_bar_id << ",";
            file << "\"entry_price\":" << t.entry_price << ",";
            file << "\"exit_price\":" << t.exit_price << ",";
            file << "\"shares\":" << t.shares << ",";
            file << "\"pnl\":" << t.pnl << ",";
            file << "\"pnl_pct\":" << t.pnl_pct << "}";
            if (i + 1 < all_trades.size()) file << ",";
            file << "\n";
        }
        file << "  ],\n";

        // ===== PRICE DATA (embed per-symbol OHLCV for filtered window) =====
        file << "  \"price_data\": {\n";
        size_t sym_idx = 0;
        for (const auto& kv : bars_by_symbol) {
            const auto& symbol = kv.first;
            const auto& bars = kv.second;
            file << "    \"" << symbol << "\": [\n";
            for (size_t j = 0; j < bars.size(); ++j) {
                const auto& b = bars[j];
                auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(b.timestamp.time_since_epoch()).count();
                file << "      {";
                file << "\"timestamp_ms\":" << ts_ms << ",";
                file << "\"open\":" << b.open << ",";
                file << "\"high\":" << b.high << ",";
                file << "\"low\":" << b.low << ",";
                file << "\"close\":" << b.close << ",";
                file << "\"volume\":" << b.volume << ",";
                file << "\"bar_id\":" << b.bar_id << "}";
                if (j + 1 < bars.size()) file << ",";
                file << "\n";
            }
            file << "    ]";
            if (++sym_idx < bars_by_symbol.size()) file << ",";
            file << "\n";
        }
        file << "  }\n";

        file << "}\n";

        file.close();
    }

private:
    static std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&time_c, &tm);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace trading
