#include "trading/multi_symbol_trader.h"
#include "trading/trading_mode.h"
#include "utils/data_loader.h"
#include "utils/date_filter.h"
#include "utils/results_exporter.h"
#include "utils/config_reader.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <set>

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
    std::string start_date; // YYYY-MM-DD format (for multi-day testing)
    std::string end_date;   // YYYY-MM-DD format (for multi-day testing)

    // Warmup period
    int warmup_days = 2;     // Default 2 days (minimum 1)
    size_t warmup_bars = 0;  // Calculated from warmup_days
    bool auto_adjust_warmup = true;  // Auto-adjust if insufficient data

    // Dashboard generation (enabled by default)
    bool generate_dashboard = true;
    std::string dashboard_script = "scripts/rotation_trading_dashboard_html.py";
    std::string results_file = "results.json";
    std::string trades_file = "trades.jsonl";
    std::string dashboard_output = "trading_dashboard.html";  // Will be updated with timestamp

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
              << "  --warmup-days N      Warmup days before trading (default: 2, minimum: 1)\n"
              << "  --enable-warmup      Enable warmup system (observation + simulation phases)\n"
              << "  --warmup-obs-days N  Observation phase days (default: 2, learning only)\n"
              << "  --warmup-sim-days N  Simulation phase days (default: 5, paper trading)\n"
              << "  --warmup-mode MODE   Warmup mode: production or testing (default: production)\n"
              << "                       ⚠️  PRODUCTION = strict criteria (SAFE for live)\n"
              << "                       ⚠️  TESTING = relaxed criteria (DEVELOPMENT ONLY)\n"
              << "  --capital AMOUNT     Initial capital (default: 100000)\n"
              << "  --max-positions N    Max concurrent positions (default: 3)\n"
              << "  --no-dashboard       Disable HTML dashboard report (enabled by default)\n"
              << "  --verbose            Show detailed progress\n\n"
              << "Mock Mode Options (REQUIRED: either --date OR both --start-date and --end-date):\n"
              << "  --date YYYY-MM-DD    Test specific single date\n"
              << "  --start-date DATE    Start date for multi-day testing (requires --end-date)\n"
              << "  --end-date DATE      End date for multi-day testing (requires --start-date)\n"
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
              << "  # Mock mode - test most recent date\n"
              << "  " << program_name << " mock\n\n"
              << "  # Mock mode - test specific date\n"
              << "  " << program_name << " mock --date 2024-10-15\n\n"
              << "  # Mock mode - test without dashboard\n"
              << "  " << program_name << " mock --date 2024-10-15 --no-dashboard\n\n"
              << "  # Live mode - paper trading\n"
              << "  " << program_name << " live\n\n"
              << "  # Live mode - with custom warmup period\n"
              << "  " << program_name << " live --warmup-days 5\n\n"
              << "Symbol Configuration:\n"
              << "  Symbols are loaded from config/symbols.conf\n"
              << "  Edit config/symbols.conf to change the symbol list\n\n"
              << "Key Insight:\n"
              << "  Mock and live modes share the EXACT same trading logic.\n"
              << "  Research and optimize in mock mode, then run live with confidence!\n";
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

    // Load symbols from config file
    try {
        config.symbols = utils::ConfigReader::load_symbols("config/symbols.conf");
    } catch (const std::exception& e) {
        std::cerr << "Error loading symbols from config: " << e.what() << "\n";
        std::cerr << "Please ensure config/symbols.conf exists and contains valid symbols.\n";
        return false;
    }

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
        // Date option (mock mode)
        else if (arg == "--date" && i + 1 < argc) {
            config.test_date = argv[++i];
        }
        else if (arg == "--start-date" && i + 1 < argc) {
            config.start_date = argv[++i];
        }
        else if (arg == "--end-date" && i + 1 < argc) {
            config.end_date = argv[++i];
        }
        // Warmup
        else if (arg == "--warmup-days" && i + 1 < argc) {
            config.warmup_days = std::stoi(argv[++i]);
        }
        else if (arg == "--no-auto-adjust-warmup") {
            config.auto_adjust_warmup = false;
        }
        else if (arg == "--enable-warmup") {
            config.trading.warmup.enabled = true;
        }
        else if (arg == "--warmup-obs-days" && i + 1 < argc) {
            config.trading.warmup.observation_days = std::stoi(argv[++i]);
        }
        else if (arg == "--warmup-sim-days" && i + 1 < argc) {
            config.trading.warmup.simulation_days = std::stoi(argv[++i]);
        }
        else if (arg == "--warmup-mode" && i + 1 < argc) {
            std::string mode_str = argv[++i];
            std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(), ::tolower);
            if (mode_str == "production") {
                config.trading.warmup.set_mode(TradingConfig::WarmupMode::PRODUCTION);
            } else if (mode_str == "testing") {
                config.trading.warmup.set_mode(TradingConfig::WarmupMode::TESTING);
                std::cerr << "⚠️  WARNING: Warmup mode set to TESTING (relaxed criteria)\n";
                std::cerr << "⚠️  NOT SAFE FOR LIVE TRADING! Use 'production' mode for real money.\n";
            } else {
                std::cerr << "Invalid warmup mode: " << mode_str << " (use 'production' or 'testing')\n";
                return false;
            }
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
            // Set all lambda values to the same (can be customized further if needed)
            double lambda = std::stod(argv[++i]);
            config.trading.horizon_config.lambda_1bar = lambda;
            config.trading.horizon_config.lambda_5bar = lambda;
            config.trading.horizon_config.lambda_10bar = lambda;
        }
        else if (arg == "--min-threshold" && i + 1 < argc) {
            config.trading.filter_config.min_prediction_for_entry = std::stod(argv[++i]);
        }
        // Output options
        else if (arg == "--no-dashboard") {
            config.generate_dashboard = false;
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

void generate_dashboard(const std::string& results_file, const std::string& script_path,
                        const std::string& trades_file, const std::string& output_file,
                        const std::string& data_dir, double initial_capital,
                        const std::string& start_date, const std::string& end_date) {
    std::cout << "\nGenerating dashboard...\n";

    // Build command with all required arguments
    std::ostringstream cmd;
    cmd << "python3 " << script_path
        << " --trades " << trades_file
        << " --output " << output_file
        << " --start-equity " << std::fixed << std::setprecision(0) << initial_capital
        << " --data-dir " << data_dir
        << " --results " << results_file;

    if (!start_date.empty()) {
        cmd << " --start-date " << start_date;
    }
    if (!end_date.empty()) {
        cmd << " --end-date " << end_date;
    }

    std::string command = cmd.str();
    int ret = system(command.c_str());

    if (ret != 0) {
        std::cerr << "⚠️  Dashboard generation failed (code: " << ret << ")\n";
        std::cerr << "   Command: " << command << "\n";
    } else {
        std::cout << "✅ Dashboard generated: " << output_file << "\n";

        // Auto-open dashboard in default browser
        std::cout << "🌐 Opening dashboard in browser...\n";
        std::string open_cmd = "open \"" + output_file + "\"";
        int open_ret = system(open_cmd.c_str());

        if (open_ret != 0) {
            std::cerr << "⚠️  Failed to open dashboard automatically\n";
            std::cerr << "   You can manually open: " << output_file << "\n";
        }
    }
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

// Extract unique trading days from bar data (already filtered for RTH and holidays)
std::vector<std::string> get_trading_days(const std::vector<Bar>& bars) {
    std::set<std::string> unique_days;

    for (const auto& bar : bars) {
        auto duration = bar.timestamp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        time_t time = static_cast<time_t>(seconds);
        struct tm* timeinfo = localtime(&time);
        char buffer[11];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
        unique_days.insert(buffer);
    }

    return std::vector<std::string>(unique_days.begin(), unique_days.end());
}

// Find warmup start date by counting backwards N trading days
std::string find_warmup_start_date(const std::vector<std::string>& trading_days,
                                   const std::string& target_date,
                                   int warmup_days) {
    // Find target date in trading days
    auto it = std::find(trading_days.begin(), trading_days.end(), target_date);
    if (it == trading_days.end()) {
        throw std::runtime_error("Target date not found in trading days: " + target_date);
    }

    // Count backwards warmup_days trading days
    int idx = std::distance(trading_days.begin(), it);
    int warmup_start_idx = std::max(0, idx - warmup_days);

    return trading_days[warmup_start_idx];
}

// Filter bars to date range (and warmup period before start)
void filter_to_date_range(std::unordered_map<Symbol, std::vector<Bar>>& all_data,
                         const std::string& start_date_str, const std::string& end_date_str,
                         size_t warmup_bars, int bars_per_day, bool verbose = false) {
    if (all_data.empty()) return;

    // Get trading days from first symbol
    const auto& first_symbol_bars = all_data.begin()->second;
    std::vector<std::string> trading_days = get_trading_days(first_symbol_bars);

    // Calculate warmup days needed
    int warmup_days = (warmup_bars + bars_per_day - 1) / bars_per_day;

    // Find warmup start date by counting backwards from start_date
    std::string warmup_start_date = find_warmup_start_date(trading_days, start_date_str, warmup_days);

    // Parse warmup start date
    int ws_year, ws_month, ws_day;
    sscanf(warmup_start_date.c_str(), "%d-%d-%d", &ws_year, &ws_month, &ws_day);

    struct tm warmup_start_timeinfo = {};
    warmup_start_timeinfo.tm_year = ws_year - 1900;
    warmup_start_timeinfo.tm_mon = ws_month - 1;
    warmup_start_timeinfo.tm_mday = ws_day;
    warmup_start_timeinfo.tm_hour = 9;   // 9:30 AM ET (market open)
    warmup_start_timeinfo.tm_min = 30;
    warmup_start_timeinfo.tm_sec = 0;
    warmup_start_timeinfo.tm_isdst = -1;

    time_t warmup_start_time = mktime(&warmup_start_timeinfo);
    Timestamp warmup_start_timestamp = std::chrono::system_clock::from_time_t(warmup_start_time);

    // Parse end date
    int end_year, end_month, end_day;
    sscanf(end_date_str.c_str(), "%d-%d-%d", &end_year, &end_month, &end_day);

    struct tm end_timeinfo = {};
    end_timeinfo.tm_year = end_year - 1900;
    end_timeinfo.tm_mon = end_month - 1;
    end_timeinfo.tm_mday = end_day;
    end_timeinfo.tm_hour = 16;  // 4 PM ET (market close)
    end_timeinfo.tm_min = 0;
    end_timeinfo.tm_sec = 0;
    end_timeinfo.tm_isdst = -1;

    time_t end_time = mktime(&end_timeinfo);
    Timestamp end_timestamp = std::chrono::system_clock::from_time_t(end_time);

    if (verbose) {
        std::cout << "\n[DEBUG] Date range filtering:\n";
        std::cout << "  Start date: " << start_date_str << "\n";
        std::cout << "  End date: " << end_date_str << "\n";
        std::cout << "  Warmup days needed: " << warmup_days << "\n";
        std::cout << "  Warmup start date: " << warmup_start_date << "\n";
    }

    // Filter each symbol to this date range (including warmup)
    for (auto& [symbol, bars] : all_data) {
        std::vector<Bar> filtered;

        for (const auto& bar : bars) {
            if (bar.timestamp >= warmup_start_timestamp && bar.timestamp <= end_timestamp) {
                filtered.push_back(bar);
            }
        }

        bars = std::move(filtered);
    }
}

// Filter bars to specific date (and warmup period before it)
void filter_to_date(std::unordered_map<Symbol, std::vector<Bar>>& all_data,
                   const std::string& date_str, size_t warmup_bars, int bars_per_day, bool verbose = false) {
    if (all_data.empty()) return;

    // Get trading days from first symbol
    const auto& first_symbol_bars = all_data.begin()->second;
    std::vector<std::string> trading_days = get_trading_days(first_symbol_bars);

    // Calculate warmup days needed
    int warmup_days = (warmup_bars + bars_per_day - 1) / bars_per_day;

    // Find warmup start date by counting backwards from target date
    std::string warmup_start_date = find_warmup_start_date(trading_days, date_str, warmup_days);

    // Parse warmup start date
    int ws_year, ws_month, ws_day;
    sscanf(warmup_start_date.c_str(), "%d-%d-%d", &ws_year, &ws_month, &ws_day);

    struct tm warmup_start_timeinfo = {};
    warmup_start_timeinfo.tm_year = ws_year - 1900;
    warmup_start_timeinfo.tm_mon = ws_month - 1;
    warmup_start_timeinfo.tm_mday = ws_day;
    warmup_start_timeinfo.tm_hour = 9;   // 9:30 AM ET (market open)
    warmup_start_timeinfo.tm_min = 30;
    warmup_start_timeinfo.tm_sec = 0;
    warmup_start_timeinfo.tm_isdst = -1;

    time_t warmup_start_time = mktime(&warmup_start_timeinfo);
    Timestamp warmup_start_timestamp = std::chrono::system_clock::from_time_t(warmup_start_time);

    // Parse target date (end of day)
    int year, month, day;
    sscanf(date_str.c_str(), "%d-%d-%d", &year, &month, &day);

    struct tm end_timeinfo = {};
    end_timeinfo.tm_year = year - 1900;
    end_timeinfo.tm_mon = month - 1;
    end_timeinfo.tm_mday = day;
    end_timeinfo.tm_hour = 16;  // 4 PM ET (market close)
    end_timeinfo.tm_min = 0;
    end_timeinfo.tm_sec = 0;
    end_timeinfo.tm_isdst = -1;

    time_t end_time = mktime(&end_timeinfo);
    Timestamp end_timestamp = std::chrono::system_clock::from_time_t(end_time);

    if (verbose) {
        std::cout << "\n[DEBUG] Date filtering:\n";
        std::cout << "  Target date: " << date_str << "\n";
        std::cout << "  Warmup days needed: " << warmup_days << "\n";
        std::cout << "  Warmup start date: " << warmup_start_date << "\n";
    }

    // Filter each symbol to this date range
    for (auto& [symbol, bars] : all_data) {
        std::vector<Bar> filtered;

        for (const auto& bar : bars) {
            if (bar.timestamp >= warmup_start_timestamp && bar.timestamp <= end_timestamp) {
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

        // Determine test date(s)
        std::string test_date = config.test_date;
        bool is_multi_day = !config.start_date.empty() && !config.end_date.empty();

        if (is_multi_day) {
            std::cout << "Testing date range: " << config.start_date << " to " << config.end_date << "\n";
            test_date = config.end_date;  // Use end date for filtering
        } else if (test_date.empty()) {
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

        // Filter to test date(s) using actual timestamps (not bar count!)
        if (is_multi_day) {
            std::cout << "\nFiltering to date range (including warmup period)...\n";
            filter_to_date_range(all_data, config.start_date, config.end_date,
                               config.warmup_bars, config.trading.bars_per_day, config.verbose);
        } else {
            std::cout << "\nFiltering to test date (including warmup period)...\n";
            filter_to_date(all_data, test_date, config.warmup_bars, config.trading.bars_per_day, config.verbose);
        }

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

        // ========================================
        // DATA AVAILABILITY VALIDATION
        // ========================================

        // Calculate minimum required bars (warmup + at least 1 trading day)
        size_t min_required_bars = config.warmup_bars + config.trading.bars_per_day;

        if (min_bars == 0) {
            std::cerr << "\n❌ ERROR: No data available for the specified date(s)!\n";
            std::cerr << "\nDebug Information:\n";
            std::cerr << "  Requested: " << (is_multi_day ?
                (config.start_date + " to " + config.end_date) : test_date) << "\n";
            std::cerr << "  Data directory: " << config.data_dir << "\n";
            std::cerr << "  File extension: " << config.extension << "\n";
            std::cerr << "\nPossible causes:\n";
            std::cerr << "  1. Date out of range (check available dates in data files)\n";
            std::cerr << "  2. Incorrect date format (use YYYY-MM-DD)\n";
            std::cerr << "  3. Data files are empty or corrupted\n";
            return 1;
        }

        if (min_bars < min_required_bars) {
            std::cerr << "\n❌ ERROR: Insufficient data for the specified date(s)!\n";
            std::cerr << "\nData Availability:\n";
            std::cerr << "  Available bars: " << min_bars << " (~"
                     << (min_bars / config.trading.bars_per_day) << " days)\n";
            std::cerr << "  Required bars:  " << min_required_bars << " (~"
                     << (min_required_bars / config.trading.bars_per_day) << " days)\n";
            std::cerr << "    - Warmup:     " << config.warmup_bars << " bars ("
                     << config.warmup_days << " days)\n";
            std::cerr << "    - Trading:    " << config.trading.bars_per_day
                     << " bars (1 day minimum)\n";
            std::cerr << "\nDebug Information:\n";
            std::cerr << "  Requested: " << (is_multi_day ?
                (config.start_date + " to " + config.end_date) : test_date) << "\n";

            // Show per-symbol breakdown
            std::cerr << "\n  Bars per symbol after filtering:\n";
            for (const auto& [symbol, bars] : all_data) {
                std::cerr << "    " << symbol << ": " << bars.size() << " bars\n";
            }

            std::cerr << "\nSuggestions:\n";
            std::cerr << "  1. Reduce warmup period: --warmup-days 1\n";
            std::cerr << "  2. Choose a different date range with more data\n";
            std::cerr << "  3. Check that data files contain the requested dates\n";
            return 1;
        }

        // Calculate trading days available
        size_t trading_bars = min_bars - config.warmup_bars;
        size_t trading_days = trading_bars / config.trading.bars_per_day;

        std::cout << "\n✅ Data validation passed:\n";
        std::cout << "  Total bars:    " << min_bars << " (~"
                  << (min_bars / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Warmup:        " << config.warmup_bars << " bars ("
                  << config.warmup_days << " days)\n";
        std::cout << "  Trading:       " << trading_bars << " bars (~"
                  << trading_days << " days)\n";

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
        std::cout << "  Features: 33 features (8 time + 25 technical)\n";
        std::cout << "  Predictor: Multi-Horizon EWRLS (1/5/10 bars, λ="
                  << config.trading.horizon_config.lambda_1bar << "/"
                  << config.trading.horizon_config.lambda_5bar << "/"
                  << config.trading.horizon_config.lambda_10bar << ")\n";
        std::cout << "  Strategy: Multi-symbol rotation (top " << config.trading.max_positions << ")\n";
        std::cout << "  Min prediction threshold: " << config.trading.filter_config.min_prediction_for_entry << "\n";
        std::cout << "  Min holding period: " << config.trading.filter_config.min_bars_to_hold << " bars\n\n";

        // Adjust min_bars_to_learn based on warmup
        // CRITICAL FIX: Add 1 to skip overnight gap between last warmup bar and first test day bar
        // This ensures we trade the FULL test day (all 391 bars) instead of including the last bar of warmup day
        // Example: warmup_bars=1173 means bars 0-1172 (3 days), test day starts at bar 1173+1=1174
        config.trading.min_bars_to_learn = config.warmup_bars + 1;

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
            std::cout << "  - Min prediction threshold: " << config.trading.filter_config.min_prediction_for_entry << "\n";
            std::cout << "\n  Possible causes:\n";
            std::cout << "  1. Prediction threshold too high (try --min-threshold 0.0001)\n";
            std::cout << "  2. Insufficient trading period after warmup\n";
            std::cout << "  3. All predictions below threshold\n\n";
        }

        // Export results JSON (always, for optimization and analysis)
        std::string symbols_str;
        for (size_t i = 0; i < config.symbols.size(); ++i) {
            symbols_str += config.symbols[i];
            if (i < config.symbols.size() - 1) symbols_str += ",";
        }

        std::string start_for_export = is_multi_day ? config.start_date : test_date;
        std::string end_for_export = is_multi_day ? config.end_date : test_date;

        ResultsExporter::export_json(
            results, trader, config.results_file,
            symbols_str, "MOCK",
            start_for_export, end_for_export
        );

        if (!config.generate_dashboard) {
            // Only show export confirmation when dashboard is disabled
            std::cout << "\n✅ Results exported to: " << config.results_file << "\n";
        }

        // Export trades for dashboard (only if dashboard enabled)
        if (config.generate_dashboard) {
            export_trades_jsonl(trader, "trades.jsonl");
            std::cout << "\n✅ Results exported to: " << config.results_file << "\n";
            std::cout << "✅ Trades exported to: trades.jsonl\n";
        }

        // Print results
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                 MOCK MODE Results                          ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

        std::cout << "Test Summary:\n";
        if (is_multi_day) {
            std::cout << "  Test Period:        " << config.start_date << " to " << config.end_date << "\n";
        } else {
            std::cout << "  Test Date:          " << test_date << "\n";
        }
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
            std::string start_date, end_date;
            if (is_multi_day) {
                start_date = config.start_date;
                end_date = config.end_date;
            } else {
                start_date = config.test_date.empty() ? "" : config.test_date;
                end_date = config.test_date.empty() ? "" : config.test_date;
            }

            // Create unique dashboard filename with timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            struct tm* tm_now = localtime(&time_t_now);

            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_now);

            // Create logs/dashboard directory
            std::filesystem::create_directories("logs/dashboard");

            // Generate unique dashboard filename
            std::string dashboard_file;
            if (is_multi_day) {
                dashboard_file = "logs/dashboard/dashboard_" +
                                config.start_date + "_to_" + config.end_date +
                                "_" + std::string(timestamp) + ".html";
            } else {
                std::string date_for_filename = config.test_date.empty() ? "latest" : config.test_date;
                dashboard_file = "logs/dashboard/dashboard_" +
                                date_for_filename + "_" + std::string(timestamp) + ".html";
            }

            generate_dashboard(
                config.results_file,
                config.dashboard_script,
                config.trades_file,
                dashboard_file,
                config.data_dir,
                config.capital,
                start_date,
                end_date
            );
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

    // ========================================
    // SANITY CHECKS
    // ========================================

    // 1. Validate warmup period (minimum 1 day)
    if (config.warmup_days < 1) {
        std::cerr << "❌ ERROR: Warmup period must be at least 1 day (got: "
                  << config.warmup_days << ")\n";
        std::cerr << "   Use --warmup-days N where N >= 1\n";
        return 1;
    }

    // 2. For MOCK mode, require date specification
    if (config.mode == TradingMode::MOCK) {
        bool has_single_date = !config.test_date.empty();
        bool has_date_range = !config.start_date.empty() && !config.end_date.empty();

        if (!has_single_date && !has_date_range) {
            std::cerr << "❌ ERROR: Mock mode requires date specification:\n";
            std::cerr << "   Either: --date YYYY-MM-DD (for single day test)\n";
            std::cerr << "   Or:     --start-date YYYY-MM-DD --end-date YYYY-MM-DD (for multi-day test)\n";
            std::cerr << "\nExample:\n";
            std::cerr << "  " << argv[0] << " mock --date 2024-10-15\n";
            std::cerr << "  " << argv[0] << " mock --start-date 2024-10-07 --end-date 2024-10-11\n";
            return 1;
        }

        // Validate that both start and end are provided together
        if (!config.start_date.empty() && config.end_date.empty()) {
            std::cerr << "❌ ERROR: --start-date requires --end-date\n";
            return 1;
        }
        if (config.start_date.empty() && !config.end_date.empty()) {
            std::cerr << "❌ ERROR: --end-date requires --start-date\n";
            return 1;
        }

        // Validate that only one method is used
        if (has_single_date && has_date_range) {
            std::cerr << "❌ ERROR: Cannot use both --date and --start-date/--end-date\n";
            std::cerr << "   Choose one: single date OR date range\n";
            return 1;
        }
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
