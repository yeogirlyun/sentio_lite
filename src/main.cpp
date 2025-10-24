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

    // Date for testing (mock mode) - SINGLE DAY ONLY
    std::string test_date;  // YYYY-MM-DD format (single test day, required)

    // Training/simulation period (historical data for learning)
    int sim_days = 20;       // Default 20 days of historical simulation
    size_t sim_bars = 0;     // Calculated from sim_days

    // Warmup period (fixed at 1 day before test day)
    static constexpr int warmup_days = 1;  // Fixed: 1 day warmup
    size_t warmup_bars = 0;  // Calculated: 1 day = 391 bars

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
    std::cout << "Sentio Lite - Single-Day Trading Optimization\n\n"
              << "Philosophy: Re-optimize every morning for TODAY'S trading session\n\n"
              << "Data Structure:\n"
              << "  [warmup: 1 day] + [sim: N days] + [test: 1 day]\n"
              << "  1. Warmup: Learn from 1 day (no trading)\n"
              << "  2. Simulation: Practice trading on N days (default: 20)\n"
              << "  3. Test: Single day to optimize for (TODAY)\n\n"
              << "Usage: " << program_name << " mock --date YYYY-MM-DD [options]\n\n"
              << "Required Options:\n"
              << "  --date YYYY-MM-DD    Test date (single day)\n\n"
              << "Common Options:\n"
              << "  --sim-days N         Simulation trading days (default: 20)\n"
              << "  --capital AMOUNT     Initial capital (default: 100000)\n"
              << "  --max-positions N    Max concurrent positions (default: 3)\n"
              << "  --no-dashboard       Disable HTML dashboard report (enabled by default)\n"
              << "  --verbose            Show detailed progress\n\n"
              << "Mock Mode Options:\n"
              << "  --data-dir DIR       Data directory (default: data)\n"
              << "  --extension EXT      File extension: .bin or .csv (default: .bin)\n\n"
              << "Trading Parameters:\n"
              << "  --stop-loss PCT      Stop loss percentage (default: -0.02)\n"
              << "  --profit-target PCT  Profit target percentage (default: 0.05)\n"
              << "  --lambda LAMBDA      EWRLS forgetting factor (default: 0.98)\n\n"
              << "Output Options:\n"
              << "  --results-file FILE  Results JSON file (default: results.json)\n"
              << "  --help               Show this help message\n\n"
              << "Examples:\n\n"
              << "  # Test on specific date with default 20-day simulation\n"
              << "  " << program_name << " mock --date 2025-10-21\n\n"
              << "  # Test with 10 days of simulation\n"
              << "  " << program_name << " mock --date 2025-10-21 --sim-days 10\n\n"
              << "  # Test without dashboard\n"
              << "  " << program_name << " mock --date 2025-10-21 --no-dashboard\n\n"
              << "  # Optimize parameters with Optuna\n"
              << "  python3 tools/optuna_quick_optimize.py --date 2025-10-21 --sim-days 20\n\n"
              << "Symbol Configuration:\n"
              << "  Symbols are loaded from config/symbols.conf\n"
              << "  Edit config/symbols.conf to change the symbol list\n\n"
              << "Key Insight:\n"
              << "  Optimization focuses ONLY on test day performance.\n"
              << "  Simulation trades are ignored - only test day metrics matter!\n";
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
        // Date option (mock mode) - SINGLE DAY ONLY
        else if (arg == "--date" && i + 1 < argc) {
            config.test_date = argv[++i];
        }
        // Simulation period (historical training data)
        else if (arg == "--sim-days" && i + 1 < argc) {
            config.sim_days = std::stoi(argv[++i]);
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

    // Calculate bars (warmup is fixed at 1 day, sim is configurable)
    config.warmup_bars = config.warmup_days * config.trading.bars_per_day;  // 391 bars (1 day fixed)
    config.sim_bars = config.sim_days * config.trading.bars_per_day;        // e.g., 7820 bars (20 days)

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

// Filter bars to specific date (including simulation + warmup period before it)
void filter_to_date(std::unordered_map<Symbol, std::vector<Bar>>& all_data,
                   const std::string& date_str, size_t sim_bars, size_t warmup_bars,
                   int bars_per_day, bool verbose = false) {
    if (all_data.empty()) return;

    // Get trading days from first symbol
    const auto& first_symbol_bars = all_data.begin()->second;
    std::vector<std::string> trading_days = get_trading_days(first_symbol_bars);

    // Calculate total days needed (sim + warmup)
    int total_training_days = (sim_bars + warmup_bars + bars_per_day - 1) / bars_per_day;

    // Find start date by counting backwards from target date
    std::string warmup_start_date = find_warmup_start_date(trading_days, date_str, total_training_days);

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
        std::cout << "  Total training days (sim + warmup): " << total_training_days << "\n";
        std::cout << "  Training start date: " << warmup_start_date << "\n";
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

        // Determine test date (SINGLE DAY ONLY)
        std::string test_date = config.test_date;
        std::cout << "Testing date: " << test_date << "\n";

        // Find minimum number of bars across all symbols
        size_t min_bars = std::numeric_limits<size_t>::max();
        for (const auto& [symbol, bars] : all_data) {
            min_bars = std::min(min_bars, bars.size());
        }

        // ========================================
        // STRICT DATA REQUIREMENT VALIDATION
        // ========================================
        // Required: exactly (sim_days + warmup_day + test_day) days
        // Example: 20 sim + 1 warmup + 1 test = 22 days required
        // NO FALLBACKS - fail fast if insufficient data

        int required_days = config.sim_days + config.warmup_days + 1;  // +1 for test day
        size_t required_bars = required_days * config.trading.bars_per_day;

        std::cout << "\n✅ Data Requirement Check:\n";
        std::cout << "  Required: " << required_days << " days (" << required_bars << " bars)\n";
        std::cout << "    - Warmup:     1 day (" << config.warmup_bars << " bars)\n";
        std::cout << "    - Simulation: " << config.sim_days << " days (" << config.sim_bars << " bars)\n";
        std::cout << "    - Test:       1 day (" << config.trading.bars_per_day << " bars)\n";

        // Check BEFORE filtering - ensure we have enough raw data
        std::cout << "\n  Available data before filtering:\n";
        for (const auto& [symbol, bars] : all_data) {
            std::cout << "    " << symbol << ": " << bars.size() << " bars (~"
                     << (bars.size() / config.trading.bars_per_day) << " days)\n";
        }

        // Filter to test date (including simulation + warmup + test day)
        std::cout << "\n  Filtering to test date window...\n";
        filter_to_date(all_data, test_date, config.sim_bars, config.warmup_bars,
                      config.trading.bars_per_day, config.verbose);

        // CRITICAL: Check EXACT bar count after filtering
        std::cout << "\n  Data after filtering:\n";
        for (const auto& [symbol, bars] : all_data) {
            std::cout << "    " << symbol << ": " << bars.size() << " bars\n";

            // STRICT CHECK: Must have EXACTLY the required bars
            if (bars.size() != required_bars) {
                std::cerr << "\n╔════════════════════════════════════════════════════════════╗\n";
                std::cerr << "║  ❌ FATAL ERROR: INSUFFICIENT DATA                        ║\n";
                std::cerr << "╚════════════════════════════════════════════════════════════╝\n";
                std::cerr << "\nSymbol " << symbol << " has " << bars.size() << " bars after filtering.\n";
                std::cerr << "Required: EXACTLY " << required_bars << " bars (" << required_days << " days)\n";
                std::cerr << "\nBreakdown:\n";
                std::cerr << "  Warmup:     " << config.warmup_bars << " bars (1 day, fixed)\n";
                std::cerr << "  Simulation: " << config.sim_bars << " bars (" << config.sim_days << " days)\n";
                std::cerr << "  Test:       " << config.trading.bars_per_day << " bars (1 day)\n";
                std::cerr << "  ─────────────────────────────────────\n";
                std::cerr << "  TOTAL:      " << required_bars << " bars (" << required_days << " days)\n";
                std::cerr << "\nTest date requested: " << test_date << "\n";
                std::cerr << "\n⚠️  NO FALLBACK - System requires exact data availability.\n";
                std::cerr << "\nSolutions:\n";
                std::cerr << "  1. Download more historical data before " << test_date << "\n";
                std::cerr << "  2. Reduce --sim-days (e.g., --sim-days 10 for 12 days total)\n";
                std::cerr << "  3. Choose a different test date with sufficient history\n";
                std::cerr << "\n";
                return 1;
            }
        }

        std::cout << "\n✅ STRICT VALIDATION PASSED: All symbols have exactly "
                  << required_bars << " bars (" << required_days << " days)\n";

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

        std::cout << "\nRunning MOCK mode (" << min_bars << " bars total)...\n";
        std::cout << "  Simulation: " << config.sim_bars << " bars (" << config.sim_days << " days)\n";
        std::cout << "  Warmup: " << config.warmup_bars << " bars (1 day, fixed)\n";
        std::cout << "  Test: " << config.trading.bars_per_day << " bars (1 day)\n";
        std::cout << "  Features: 54 features (8 time + 28 technical + 6 BB + 12 regime)\n";
        std::cout << "  Predictor: Multi-Horizon EWRLS (1/5/10 bars, λ="
                  << config.trading.horizon_config.lambda_1bar << "/"
                  << config.trading.horizon_config.lambda_5bar << "/"
                  << config.trading.horizon_config.lambda_10bar << ")\n";
        std::cout << "  Strategy: Multi-symbol rotation (top " << config.trading.max_positions << ")\n";
        std::cout << "  Min prediction threshold: " << config.trading.filter_config.min_prediction_for_entry << "\n";
        std::cout << "  Min holding period: " << config.trading.filter_config.min_bars_to_hold << " bars\n\n";

        // Set min_bars_to_learn: Start trading after warmup (warmup + sim period)
        // Structure: [warmup_bars] + [sim_bars] + [test_bars]
        // Warmup: learn only (no trading)
        // Sim: practice trading (trades are NOT optimized for)
        // Test: real trades (ONLY these trades count for optimization)
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

        ResultsExporter::export_json(
            results, trader, config.results_file,
            symbols_str, "MOCK",
            test_date, test_date  // Single day: start = end
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
        std::cout << "  Test Date:          " << test_date << "\n";
        std::cout << "  Warmup Period:      " << (config.warmup_bars / config.trading.bars_per_day) << " days (" << config.warmup_bars << " bars)\n";
        std::cout << "  Test Period:        1 day (" << config.trading.bars_per_day << " bars)\n";
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
            // Create unique dashboard filename with timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            struct tm* tm_now = localtime(&time_t_now);

            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_now);

            // Create logs/dashboard directory
            std::filesystem::create_directories("logs/dashboard");

            // Generate unique dashboard filename
            std::string dashboard_file = "logs/dashboard/dashboard_" +
                                        test_date + "_" + std::string(timestamp) + ".html";

            generate_dashboard(
                config.results_file,
                config.dashboard_script,
                config.trades_file,
                dashboard_file,
                config.data_dir,
                config.capital,
                test_date,
                test_date  // Single day: start = end
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

    // 1. Validate simulation period (minimum 1 day)
    if (config.sim_days < 1) {
        std::cerr << "❌ ERROR: Simulation period must be at least 1 day (got: "
                  << config.sim_days << ")\n";
        std::cerr << "   Use --sim-days N where N >= 1\n";
        return 1;
    }

    // 2. For MOCK mode, require --date (SINGLE DAY ONLY)
    if (config.mode == TradingMode::MOCK) {
        if (config.test_date.empty()) {
            std::cerr << "❌ ERROR: Mock mode requires --date YYYY-MM-DD\n";
            std::cerr << "\nExample:\n";
            std::cerr << "  " << argv[0] << " mock --date 2025-10-21\n";
            std::cerr << "  " << argv[0] << " mock --date 2025-10-21 --warmup-days 2\n";
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

    std::cout << "  Simulation Period: " << config.sim_days << " days ("
              << config.sim_bars << " bars)\n";
    std::cout << "  Warmup Period: 1 day (" << config.warmup_bars << " bars) [fixed]\n";
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
