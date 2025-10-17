#include "trading/multi_symbol_trader.h"
#include "trading/trading_mode.h"
#include "utils/data_loader.h"
#include "utils/date_filter.h"
#include "utils/results_exporter.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <algorithm>

using namespace trading;

// Configuration from command line
struct Config {
    std::string data_dir = "data";
    std::string extension = ".bin";  // .bin or .csv
    std::vector<std::string> symbols;
    double capital = 100000.0;
    bool verbose = false;

    // Mode: mock (historical data test) or live (real-time trading)
    TradingMode mode = TradingMode::MOCK;
    std::string mode_str = "mock";

    // Date for testing (mock mode)
    std::string test_date;  // YYYY-MM-DD format (single day or recent if empty)

    // Warmup period
    int warmup_days = 3;     // Default 3 days
    size_t warmup_bars = 0;  // Calculated from warmup_days
    bool auto_adjust_warmup = true;  // Auto-adjust if insufficient data

    // Dashboard generation
    bool generate_dashboard = false;
    std::string dashboard_script = "generate_dashboard.py";
    std::string results_file = "results.json";

    // Trading parameters
    TradingConfig trading;
};

void print_usage(const char* program_name) {
    std::cout << "Sentio Lite - Multi-Symbol Rotation Trading\n\n"
              << "Two Modes (share exact same trading logic):\n"
              << "  mock  - Test on historical data (default: most recent date)\n"
              << "  live  - Real-time paper trading via Alpaca/Polygon\n\n"
              << "Usage: " << program_name << " <mock|live> [options]\n\n"
              << "Common Options:\n"
              << "  --symbols LIST       Comma-separated symbols or 6|10|14 (default: 10)\n"
              << "  --warmup-days N      Warmup days before trading (default: 3)\n"
              << "  --capital AMOUNT     Initial capital (default: 100000)\n"
              << "  --max-positions N    Max concurrent positions (default: 3)\n"
              << "  --generate-dashboard Generate HTML dashboard report\n"
              << "  --verbose            Show detailed progress\n\n"
              << "Mock Mode Options:\n"
              << "  --date YYYY-MM-DD    Test specific date (default: most recent)\n"
              << "  --data-dir DIR       Data directory (default: data)\n"
              << "  --extension EXT      File extension: .bin or .csv (default: .bin)\n\n"
              << "Live Mode Options:\n"
              << "  --fifo PATH          FIFO pipe path (default: /tmp/alpaca_bars.fifo)\n"
              << "  --websocket TYPE     Websocket: alpaca or polygon (default: alpaca)\n\n"
              << "Trading Parameters:\n"
              << "  --stop-loss PCT      Stop loss percentage (default: -0.02)\n"
              << "  --profit-target PCT  Profit target percentage (default: 0.05)\n"
              << "  --lambda LAMBDA      EWRLS forgetting factor (default: 0.98)\n\n"
              << "Output Options:\n"
              << "  --results-file FILE  Results JSON file (default: results.json)\n"
              << "  --help               Show this help message\n\n"
              << "Examples:\n\n"
              << "  # Mock mode - test most recent date with 10 symbols\n"
              << "  " << program_name << " mock\n\n"
              << "  # Mock mode - test specific date\n"
              << "  " << program_name << " mock --date 2024-10-15\n\n"
              << "  # Mock mode - test with custom symbols and generate dashboard\n"
              << "  " << program_name << " mock --symbols TQQQ,SQQQ,SSO,SDS \\\n"
              << "    --date 2024-10-15 --generate-dashboard\n\n"
              << "  # Live mode - paper trade with 10 symbols\n"
              << "  " << program_name << " live\n\n"
              << "  # Live mode - paper trade with 6 symbols and 5-day warmup\n"
              << "  " << program_name << " live --symbols 6 --warmup-days 5\n\n"
              << "Default Symbol Lists:\n"
              << "  6:  TQQQ, SQQQ, UPRO, SDS, UVXY, SVXY\n"
              << "  10: TQQQ, SQQQ, SSO, SDS, TNA, TZA, FAS, FAZ, UVXY, SVXY (default)\n"
              << "  14: + UPRO, SPXS, ERX, ERY, NUGT, DUST\n\n"
              << "Key Insight:\n"
              << "  Mock and live modes share the EXACT same trading logic.\n"
              << "  Research and optimize in mock mode, then run live with confidence!\n";
}

bool parse_symbols(const std::string& symbols_str, std::vector<std::string>& symbols) {
    // Check for default lists
    if (symbols_str == "6") {
        for (const auto& sym : symbols::DEFAULT_6) symbols.push_back(sym);
        return true;
    } else if (symbols_str == "10") {
        for (const auto& sym : symbols::DEFAULT_10) symbols.push_back(sym);
        return true;
    } else if (symbols_str == "14") {
        for (const auto& sym : symbols::DEFAULT_14) symbols.push_back(sym);
        return true;
    }

    // Parse comma-separated list
    std::istringstream iss(symbols_str);
    std::string symbol;
    while (std::getline(iss, symbol, ',')) {
        if (!symbol.empty()) {
            symbols.push_back(symbol);
        }
    }
    return !symbols.empty();
}

bool parse_args(int argc, char* argv[], Config& config) {
    if (argc < 2) {
        return false;
    }

    // First argument is mode
    std::string mode_arg = argv[1];
    if (mode_arg == "--help" || mode_arg == "-h") {
        return false;
    }

    if (mode_arg != "mock" && mode_arg != "live") {
        std::cerr << "Error: First argument must be 'mock' or 'live'\n";
        return false;
    }

    config.mode_str = mode_arg;
    config.mode = parse_trading_mode(mode_arg);

    // Default symbols to 10
    parse_symbols("10", config.symbols);

    // Parse remaining options
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        // Data options
        else if (arg == "--data-dir" && i + 1 < argc) {
            config.data_dir = argv[++i];
        }
        else if (arg == "--extension" && i + 1 < argc) {
            config.extension = argv[++i];
            if (config.extension[0] != '.') {
                config.extension = "." + config.extension;
            }
        }
        // Symbol options
        else if (arg == "--symbols" && i + 1 < argc) {
            config.symbols.clear();
            if (!parse_symbols(argv[++i], config.symbols)) {
                std::cerr << "Error: Invalid symbols specification\n";
                return false;
            }
        }
        // Date option (mock mode)
        else if (arg == "--date" && i + 1 < argc) {
            config.test_date = argv[++i];
        }
        // Warmup
        else if (arg == "--warmup-days" && i + 1 < argc) {
            config.warmup_days = std::stoi(argv[++i]);
        }
        else if (arg == "--no-auto-adjust-warmup") {
            config.auto_adjust_warmup = false;
        }
        // Trading parameters
        else if (arg == "--capital" && i + 1 < argc) {
            config.capital = std::stod(argv[++i]);
            config.trading.initial_capital = config.capital;
        }
        else if (arg == "--max-positions" && i + 1 < argc) {
            config.trading.max_positions = std::stoul(argv[++i]);
        }
        else if (arg == "--stop-loss" && i + 1 < argc) {
            config.trading.stop_loss_pct = std::stod(argv[++i]);
        }
        else if (arg == "--profit-target" && i + 1 < argc) {
            config.trading.profit_target_pct = std::stod(argv[++i]);
        }
        else if (arg == "--lambda" && i + 1 < argc) {
            config.trading.lambda = std::stod(argv[++i]);
        }
        else if (arg == "--min-threshold" && i + 1 < argc) {
            config.trading.min_prediction_threshold = std::stod(argv[++i]);
        }
        // Output options
        else if (arg == "--generate-dashboard") {
            config.generate_dashboard = true;
        }
        else if (arg == "--results-file" && i + 1 < argc) {
            config.results_file = argv[++i];
        }
        else if (arg == "--verbose") {
            config.verbose = true;
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            return false;
        }
    }

    // Calculate warmup bars
    config.warmup_bars = config.warmup_days * config.trading.bars_per_day;

    return true;
}

void generate_dashboard(const std::string& results_file, const std::string& script_path) {
    std::cout << "\nGenerating dashboard...\n";

    // Call dashboard script - it will auto-generate timestamped filename
    std::string command = "python3 " + script_path + " " + results_file;
    int ret = system(command.c_str());

    if (ret != 0) {
        std::cerr << "⚠️  Dashboard generation failed (code: " << ret << ")\n";
        std::cerr << "   Command: " << command << "\n";
    }
    // Success message is printed by the Python script
}

std::string get_most_recent_date(const std::unordered_map<Symbol, std::vector<Bar>>& all_data) {
    Timestamp max_timestamp = std::chrono::system_clock::time_point::min();
    for (const auto& [symbol, bars] : all_data) {
        if (!bars.empty()) {
            max_timestamp = std::max(max_timestamp, bars.back().timestamp);
        }
    }

    // Convert timestamp to YYYY-MM-DD
    auto duration = max_timestamp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    time_t time = static_cast<time_t>(seconds);
    struct tm* timeinfo = localtime(&time);
    char buffer[11];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
    return std::string(buffer);
}

// Filter bars to specific date (and warmup period before it)
void filter_to_date(std::unordered_map<Symbol, std::vector<Bar>>& all_data,
                   const std::string& date_str, size_t warmup_bars, int bars_per_day, bool verbose = false) {
    // Parse date string to get end of trading day timestamp
    int year, month, day;
    sscanf(date_str.c_str(), "%d-%d-%d", &year, &month, &day);

    struct tm timeinfo = {};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = 16;  // 4 PM ET (market close)
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = -1;

    time_t end_time = mktime(&timeinfo);
    Timestamp end_timestamp = std::chrono::system_clock::from_time_t(end_time);

    // Calculate start timestamp for warmup period
    // We need warmup_bars BEFORE the test date, plus the full test date
    // Warmup bars are approximately warmup_bars / bars_per_day trading days
    int warmup_days = (warmup_bars + bars_per_day - 1) / bars_per_day;  // Round up

    // Go back warmup_days + 1 extra day to ensure we have enough bars
    // (accounting for weekends and gaps in trading)
    time_t start_time = end_time - ((warmup_days + 2) * 24 * 3600);  // Subtract calendar days
    Timestamp start_timestamp = std::chrono::system_clock::from_time_t(start_time);

    if (verbose) {
        std::cout << "\n[DEBUG] Date filtering:\n";
        std::cout << "  Target date: " << date_str << "\n";
        std::cout << "  Start time_t: " << start_time << "\n";
        std::cout << "  End time_t: " << end_time << "\n";

        // Show sample timestamps from first symbol
        if (!all_data.empty()) {
            const auto& [symbol, bars] = *all_data.begin();
            if (!bars.empty()) {
                auto first_bar_time = std::chrono::system_clock::to_time_t(bars.front().timestamp);
                auto last_bar_time = std::chrono::system_clock::to_time_t(bars.back().timestamp);
                std::cout << "  " << symbol << " first bar time_t: " << first_bar_time << "\n";
                std::cout << "  " << symbol << " last bar time_t: " << last_bar_time << "\n";
            }
        }
    }

    // Filter each symbol to this date range
    for (auto& [symbol, bars] : all_data) {
        std::vector<Bar> filtered;

        for (const auto& bar : bars) {
            if (bar.timestamp >= start_timestamp && bar.timestamp <= end_timestamp) {
                filtered.push_back(bar);
            }
        }

        bars = std::move(filtered);
    }
}

void export_trades_jsonl(const MultiSymbolTrader& trader, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open trades file: " + filename);
    }

    auto all_trades = trader.get_all_trades();

    // Sort trades by entry time
    std::sort(all_trades.begin(), all_trades.end(),
              [](const TradeRecord& a, const TradeRecord& b) {
                  return a.entry_time < b.entry_time;
              });

    // Export each trade as ENTRY and EXIT
    for (const auto& trade : all_trades) {
        // Entry trade
        auto entry_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            trade.entry_time.time_since_epoch()).count();
        auto exit_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            trade.exit_time.time_since_epoch()).count();

        double bars_held = (exit_ms - entry_ms) / 60000.0; // Assuming 1-minute bars
        double entry_value = trade.shares * trade.entry_price;
        double exit_value = trade.shares * trade.exit_price;

        // ENTRY record
        file << "{"
             << "\"symbol\":\"" << trade.symbol << "\","
             << "\"action\":\"ENTRY\","
             << "\"timestamp_ms\":" << entry_ms << ","
             << "\"bar_id\":" << trade.entry_bar_id << ","
             << "\"price\":" << trade.entry_price << ","
             << "\"shares\":" << trade.shares << ","
             << "\"value\":" << entry_value << ","
             << "\"pnl\":0,"
             << "\"pnl_pct\":0,"
             << "\"bars_held\":0,"
             << "\"reason\":\"Rotation\""
             << "}\n";

        // EXIT record
        file << "{"
             << "\"symbol\":\"" << trade.symbol << "\","
             << "\"action\":\"EXIT\","
             << "\"timestamp_ms\":" << exit_ms << ","
             << "\"bar_id\":" << trade.exit_bar_id << ","
             << "\"price\":" << trade.exit_price << ","
             << "\"shares\":" << trade.shares << ","
             << "\"value\":" << exit_value << ","
             << "\"pnl\":" << trade.pnl << ","
             << "\"pnl_pct\":" << (trade.pnl_pct * 100) << ","
             << "\"bars_held\":" << static_cast<int>(bars_held) << ","
             << "\"reason\":\"Rotation\""
             << "}\n";
    }

    file.close();
}

int run_mock_mode(Config& config) {
    try {
        // Load market data
        std::cout << "Loading market data from " << config.data_dir << "...\n";
        auto start_load = std::chrono::high_resolution_clock::now();

        auto all_data = DataLoader::load_from_directory(
            config.data_dir,
            config.symbols,
            config.extension
        );

        auto end_load = std::chrono::high_resolution_clock::now();
        auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_load - start_load).count();

        std::cout << "Data loaded in " << load_duration << "ms\n";

        // Determine test date
        std::string test_date = config.test_date;
        if (test_date.empty()) {
            test_date = get_most_recent_date(all_data);
            std::cout << "Testing most recent date: " << test_date << "\n";
        } else {
            std::cout << "Testing specific date: " << test_date << "\n";
        }

        // Find minimum number of bars across all symbols
        size_t min_bars = std::numeric_limits<size_t>::max();
        for (const auto& [symbol, bars] : all_data) {
            min_bars = std::min(min_bars, bars.size());
        }

        std::cout << "\nData Statistics:\n";
        std::cout << "  Total bars available: " << min_bars << " (~"
                  << (min_bars / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Requested warmup: " << config.warmup_bars << " bars (~"
                  << (config.warmup_bars / config.trading.bars_per_day) << " days)\n";

        // ====== CRITICAL FIX: Smart warmup adjustment ======
        size_t original_warmup = config.warmup_bars;

        if (config.auto_adjust_warmup && min_bars < config.warmup_bars + 100) {
            // For single-day testing, use minimal warmup
            if (min_bars <= config.trading.bars_per_day) {
                // Single day: use 50 bars warmup (enough for features)
                config.warmup_bars = 50;
                std::cout << "\n⚠️  Single-day test detected. Adjusting warmup to "
                         << config.warmup_bars << " bars\n";
            } else {
                // Multiple days: use proportional warmup
                config.warmup_bars = std::min(
                    static_cast<size_t>(min_bars * 0.3),  // Use 30% for warmup
                    static_cast<size_t>(config.trading.bars_per_day * 2)  // Max 2 days
                );
                std::cout << "\n⚠️  Insufficient data for requested warmup.\n";
                std::cout << "   Auto-adjusting warmup to " << config.warmup_bars
                         << " bars (~" << (config.warmup_bars / config.trading.bars_per_day)
                         << " days)\n";
            }
        }

        // Ensure minimum bars for feature extraction (50 bars required)
        if (config.warmup_bars < 50) {
            config.warmup_bars = 50;
        }

        if (min_bars < config.warmup_bars) {
            std::cerr << "\n❌ ERROR: Not enough data!\n";
            std::cerr << "   Available: " << min_bars << " bars\n";
            std::cerr << "   Required: " << config.warmup_bars << " bars (minimum for features)\n";
            return 1;
        }

        // Filter to test date using actual timestamps (not bar count!)
        std::cout << "\nFiltering to test date (including warmup period)...\n";
        filter_to_date(all_data, test_date, config.warmup_bars, config.trading.bars_per_day, config.verbose);

        // Show filtered bar counts
        for (const auto& [symbol, bars] : all_data) {
            std::cout << "  " << symbol << ": " << bars.size() << " bars\n";
        }

        // CRITICAL: Recalculate min_bars after filtering!
        min_bars = std::numeric_limits<size_t>::max();
        for (const auto& [symbol, bars] : all_data) {
            if (bars.size() < min_bars) {
                min_bars = bars.size();
            }
        }

        // DEBUG: Verify filtered data integrity
        if (config.verbose) {
            std::cout << "\n[DEBUG] Checking filtered data integrity:\n";
            for (const auto& symbol : config.symbols) {
                const auto& bars = all_data[symbol];
                if (bars.size() >= 3) {
                    std::cout << "  " << symbol << " first bar: close=$"
                             << bars[0].close << ", last bar: close=$"
                             << bars[bars.size()-1].close << "\n";
                }
            }
        }

        std::cout << "\nRunning MOCK mode (" << min_bars << " bars)...\n";
        std::cout << "  Warmup: " << config.warmup_bars << " bars (~"
                  << (config.warmup_bars / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Trading: " << (min_bars - config.warmup_bars) << " bars (~"
                  << ((min_bars - config.warmup_bars) / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Features: 25 technical indicators\n";
        std::cout << "  Predictor: EWRLS (Online Learning, λ=" << config.trading.lambda << ")\n";
        std::cout << "  Strategy: Multi-symbol rotation (top " << config.trading.max_positions << ")\n";
        std::cout << "  Min prediction threshold: " << config.trading.min_prediction_threshold << "\n\n";

        // Adjust min_bars_to_learn based on warmup
        config.trading.min_bars_to_learn = config.warmup_bars;

        // Initialize trader
        MultiSymbolTrader trader(config.symbols, config.trading);

        // Process bars (same logic as live mode would use)
        auto start_trading = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < min_bars; ++i) {
            // Create market snapshot for this bar
            std::unordered_map<Symbol, Bar> market_snapshot;
            for (const auto& symbol : config.symbols) {
                market_snapshot[symbol] = all_data[symbol][i];
            }

            // Process bar (SAME CODE AS LIVE MODE)
            trader.on_bar(market_snapshot);

            // Enhanced progress updates
            if (i == config.warmup_bars - 1) {
                std::cout << "  ✅ Warmup complete (" << config.warmup_bars
                         << " bars), starting trading...\n";
            }

            if (i >= config.warmup_bars && (i - config.warmup_bars + 1) % 50 == 0) {
                auto current_results = trader.get_results();
                double equity = trader.get_equity(market_snapshot);
                double return_pct = (equity - config.capital) / config.capital * 100;

                std::cout << "  [Bar " << i << "/" << min_bars << "] "
                         << "Equity: $" << std::fixed << std::setprecision(2) << equity
                         << " (" << std::showpos << return_pct << std::noshowpos << "%), "
                         << "Trades: " << current_results.total_trades
                         << ", Positions: " << trader.positions().size()
                         << std::endl;
            }
        }

        auto end_trading = std::chrono::high_resolution_clock::now();
        auto trading_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_trading - start_trading).count();

        // Get results
        auto results = trader.get_results();

        // Debug: Check if predictions are being made
        if (results.total_trades == 0) {
            std::cout << "\n⚠️  NO TRADES EXECUTED - Debugging Info:\n";
            std::cout << "  - Warmup bars: " << config.warmup_bars << "\n";
            std::cout << "  - Total bars processed: " << min_bars << "\n";
            std::cout << "  - Trading bars: " << (min_bars - config.warmup_bars) << "\n";
            std::cout << "  - Min prediction threshold: " << config.trading.min_prediction_threshold << "\n";
            std::cout << "\n  Possible causes:\n";
            std::cout << "  1. Prediction threshold too high (try --min-threshold 0.0001)\n";
            std::cout << "  2. Insufficient trading period after warmup\n";
            std::cout << "  3. All predictions below threshold\n\n";
        }

        // Export results and trades for dashboard
        if (config.generate_dashboard) {
            std::string symbols_str;
            for (size_t i = 0; i < config.symbols.size(); ++i) {
                symbols_str += config.symbols[i];
                if (i < config.symbols.size() - 1) symbols_str += ",";
            }

            ResultsExporter::export_json(
                results, trader, config.results_file,
                symbols_str, "MOCK",
                test_date, test_date
            );
            std::cout << "\n✅ Results exported to: " << config.results_file << "\n";

            // Export trades for detailed dashboard
            export_trades_jsonl(trader, "trades.jsonl");
            std::cout << "✅ Trades exported to: trades.jsonl\n";
        }

        // Print results
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                 MOCK MODE Results                          ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

        std::cout << "Test Summary:\n";
        std::cout << "  Test Date:          " << test_date << "\n";
        std::cout << "  Warmup:             " << (config.warmup_bars / config.trading.bars_per_day) << " days\n";
        std::cout << "  Trading Period:     " << ((min_bars - config.warmup_bars) / config.trading.bars_per_day) << " days\n";
        std::cout << "\n";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Performance:\n";
        std::cout << "  Initial Capital:    $" << config.capital << "\n";
        std::cout << "  Final Equity:       $" << results.final_equity << "\n";
        std::cout << "  Total Return:       " << std::showpos << (results.total_return * 100)
                  << std::noshowpos << "%\n";
        std::cout << "  MRD (Daily):        " << std::showpos << (results.mrd * 100)
                  << std::noshowpos << "% per day\n";
        std::cout << "\n";

        std::cout << "Trade Statistics:\n";
        std::cout << "  Total Trades:       " << results.total_trades << "\n";
        std::cout << "  Winning Trades:     " << results.winning_trades << "\n";
        std::cout << "  Losing Trades:      " << results.losing_trades << "\n";
        std::cout << std::setprecision(1);
        std::cout << "  Win Rate:           " << (results.win_rate * 100) << "%\n";
        std::cout << std::setprecision(2);
        std::cout << "  Average Win:        $" << results.avg_win << "\n";
        std::cout << "  Average Loss:       $" << results.avg_loss << "\n";
        std::cout << "  Profit Factor:      " << results.profit_factor << "\n";
        std::cout << "\n";

        std::cout << "Execution:\n";
        std::cout << "  Bars Processed:     " << min_bars << " ("
                  << config.warmup_bars << " warmup + "
                  << (min_bars - config.warmup_bars) << " trading)\n";
        std::cout << "  Data Load Time:     " << load_duration << "ms\n";
        std::cout << "  Execution Time:     " << trading_duration << "ms\n";
        std::cout << "  Total Time:         " << (load_duration + trading_duration) << "ms\n";
        std::cout << "\n";

        // Performance assessment
        std::cout << "Assessment: ";
        if (results.total_return > 0.02 && results.win_rate > 0.55) {
            std::cout << "🟢 Excellent (ready for live)\n";
        } else if (results.total_return > 0.01 && results.win_rate > 0.50) {
            std::cout << "🟡 Good (consider more testing)\n";
        } else if (results.total_return > 0.0) {
            std::cout << "🟠 Moderate (needs optimization)\n";
        } else {
            std::cout << "🔴 Poor (not ready for live)\n";
        }

        std::cout << "\n";

        // Generate dashboard if requested
        if (config.generate_dashboard) {
            generate_dashboard(config.results_file, config.dashboard_script);
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n\n";
        return 1;
    }
}

int run_live_mode(Config& config) {
    (void)config;  // Suppress unused warning - will be used when live mode is implemented

    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              LIVE MODE (Paper Trading)                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    std::cout << "⚠️  LIVE MODE NOT YET IMPLEMENTED\n\n";
    std::cout << "To implement live mode:\n";
    std::cout << "  1. Start websocket bridge (Alpaca or Polygon)\n";
    std::cout << "  2. Read bars from FIFO pipe\n";
    std::cout << "  3. Process bars using SAME trading logic as mock mode\n";
    std::cout << "  4. Submit orders via broker API\n\n";
    std::cout << "The beauty: Mock and live share EXACT same trading code!\n";
    std::cout << "Research in mock mode = confidence in live mode\n\n";

    return 1;
}

int main(int argc, char* argv[]) {
    Config config;

    if (!parse_args(argc, argv, config)) {
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         Sentio Lite - Rotation Trading System             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Print configuration
    std::cout << "Configuration:\n";
    std::cout << "  Mode: " << to_string(config.mode);
    if (config.mode == TradingMode::LIVE) {
        std::cout << " (⚠️  NOT YET IMPLEMENTED)";
    }
    std::cout << "\n";

    std::cout << "  Symbols (" << config.symbols.size() << "): ";
    for (size_t i = 0; i < config.symbols.size(); ++i) {
        std::cout << config.symbols[i];
        if (i < config.symbols.size() - 1) std::cout << ", ";
    }
    std::cout << "\n";

    std::cout << "  Warmup Period: " << config.warmup_days << " days ("
              << config.warmup_bars << " bars)\n";
    std::cout << "  Initial Capital: $" << std::fixed << std::setprecision(2)
              << config.capital << "\n";
    std::cout << "  Max Positions: " << config.trading.max_positions << "\n";
    std::cout << "  Stop Loss: " << (config.trading.stop_loss_pct * 100) << "%\n";
    std::cout << "  Profit Target: " << (config.trading.profit_target_pct * 100) << "%\n";

    if (config.generate_dashboard) {
        std::cout << "  Dashboard: Enabled\n";
    }
    std::cout << "\n";

    // Run appropriate mode
    if (config.mode == TradingMode::MOCK) {
        return run_mock_mode(config);
    } else {
        return run_live_mode(config);
    }
}
