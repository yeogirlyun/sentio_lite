#pragma once
#include "trading/multi_symbol_trader.h"
#include "core/types.h"
#include <string>
#include <fstream>
#include <iomanip>

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
        const std::string& end_date
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
        file << "    \"max_positions\": " << trader.config().max_positions << ",\n";
        file << "    \"stop_loss_pct\": " << trader.config().stop_loss_pct << ",\n";
        file << "    \"profit_target_pct\": " << trader.config().profit_target_pct << ",\n";
        file << "    \"lambda\": " << trader.config().lambda << ",\n";
        file << "    \"min_bars_to_learn\": " << trader.config().min_bars_to_learn << ",\n";
        file << "    \"bars_per_day\": " << trader.config().bars_per_day << "\n";
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
