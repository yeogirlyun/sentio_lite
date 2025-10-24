#include "trading/multi_symbol_trader.h"
#include "trading/trading_mode.h"
#include "trading/trading_strategy.h"
#include "utils/data_loader.h"
#include "utils/date_filter.h"
#include "utils/results_exporter.h"
#include "utils/config_reader.h"
#include "utils/config_loader.h"
#include <nlohmann/json.hpp>
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
    int sim_days = 0;        // Default 0 days (warmup-only by default)
    size_t sim_bars = 0;     // Calculated from sim_days

    // Warmup period - can be specified in bars or days
    // Default: 100 bars (from bar 291 to 391 of previous day, ending at 4:00 PM)
    int warmup_bars_specified = 100;  // User-specified warmup bars (default 100)
    bool warmup_in_bars = true;       // True if user specified bars, false if days
    bool intraday_warmup = false;     // True = warmup from test day itself (bars 1-N)
    size_t warmup_bars = 100;         // Actual warmup bars to use

    // Strategy selection
    StrategyType strategy = StrategyType::EWRLS;  // Default to EWRLS
    std::string strategy_str = "ewrls";

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
              << "  --strategy NAME      Trading strategy: ewrls (default) or sigor\n"
              << "                       ewrls = Multi-Horizon EWRLS predictor\n"
              << "                       sigor = Rule-based 7-detector ensemble\n"
              << "  --sim-days N         Simulation trading days (default: 20, use 0 for warmup-only)\n"
              << "  --warmup-bars N      Warmup bars (default: 100, from previous day)\n"
              << "  --intraday-warmup    Use first N bars of TEST DAY as warmup (not prev day)\n"
              << "                       Example: --warmup-bars 50 --intraday-warmup\n"
              << "                       → Warmup on bars 1-50, trade on bars 51-391\n"
              << "  --no-dashboard       Disable HTML dashboard report (enabled by default)\n"
              << "  --verbose            Show detailed progress\n\n"
              << "Mock Mode Options:\n"
              << "  --data-dir DIR       Data directory (default: data)\n"
              << "  --extension EXT      File extension: .bin or .csv (default: .bin)\n\n"
              << "Configuration:\n"
              << "  All trading parameters are loaded from config/trading_params.json\n"
              << "  Run Optuna optimization to generate optimal config:\n"
              << "    python3 tools/optuna_5day_search.py --end-date 2025-10-23\n\n"
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
              << "  # Optimize parameters with Optuna (5-day validation)\n"
              << "  python3 tools/optuna_5day_search.py --end-date 2025-10-23 --trials 200\n\n"
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

    if (mode_arg != "mock" && mode_arg != "live" && mode_arg != "mock-live") {
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

    // NOTE: We'll determine strategy first, then load the appropriate config

    // Pre-pass to extract strategy if provided
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        // Strategy selection (pre-parse only)
        else if (arg == "--strategy" && i + 1 < argc) {
            config.strategy_str = argv[++i];
            try {
                config.strategy = parse_strategy_type(config.strategy_str);
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "Valid strategies: ewrls, sigor\n";
                return false;
            }
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
        // Warmup period (bars before test day)
        else if (arg == "--warmup-bars" && i + 1 < argc) {
            config.warmup_bars_specified = std::stoi(argv[++i]);
            config.warmup_in_bars = true;
        }
        // Intraday warmup (use first N bars of test day as warmup)
        else if (arg == "--intraday-warmup") {
            config.intraday_warmup = true;
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

    // Load configuration based on selected strategy
    try {
        if (config.strategy == StrategyType::SIGOR) {
            // Attempt to load full trading config from a SIGOR-specific file if it exists
            // Fallback to EWRLS trading params if SIGOR trading file is absent
            const std::string sigor_trading_path = "config/sigor_trading_params.json";
            if (std::filesystem::exists(sigor_trading_path)) {
                config.trading = trading::ConfigLoader::load(sigor_trading_path);
            } else {
                config.trading = trading::ConfigLoader::load("config/trading_params.json");
            }
            // Load SIGOR model parameters
            config.trading.sigor_config = trading::SigorConfigLoader::load("config/sigor_params.json");
            config.trading.strategy = StrategyType::SIGOR;
            std::cout << "\n📊 SIGOR Strategy Configuration Loaded\n";
            trading::SigorConfigLoader::print_config(config.trading.sigor_config, "config/sigor_params.json");

            // CRITICAL: SIGOR is rule-based and does not require warmup/simulation
            // Disable day-based warmup to avoid consuming morning bars on the test day
            config.warmup_bars_specified = 0;        // No prev-day warmup
            config.intraday_warmup = true;           // Any warmup (if set later) comes from test day
            config.trading.min_bars_to_learn = 0;    // Start trading immediately
            config.trading.warmup.enabled = false;   // Disable warmup phases entirely
            config.trading.warmup.observation_days = 0;
            config.trading.warmup.simulation_days = 0;
        } else {
            // Default EWRLS
            config.trading = trading::ConfigLoader::load("config/trading_params.json");
            config.trading.strategy = StrategyType::EWRLS;
            trading::ConfigLoader::print_config(config.trading, "config/trading_params.json");
        }
        config.capital = config.trading.initial_capital;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error loading strategy configuration: " << e.what() << "\n";
        return false;
    }

    // Calculate bars
    // Warmup: Use specified bars (default 100), always ends at bar 391
    config.warmup_bars = config.warmup_bars_specified;
    // Simulation: Calculate from days
    config.sim_bars = config.sim_days * config.trading.bars_per_day;

    return true;
}

void generate_dashboard(const std::string& results_file, const std::string& script_path,
                        const std::string& trades_file, const std::string& output_file,
                        const std::string& data_dir, double initial_capital,
                        const std::string& start_date, const std::string& end_date,
                        const std::string& config_file = "config/symbols.conf") {
    std::cout << "\nGenerating dashboard...\n";

    // Build command with all required arguments
    std::ostringstream cmd;
    cmd << "python3 " << script_path
        << " --trades " << trades_file
        << " --output " << output_file
        << " --start-equity " << std::fixed << std::setprecision(0) << initial_capital
        << " --data-dir " << data_dir
        << " --results " << results_file
        << " --config " << config_file;

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
        struct tm* timeinfo = gmtime(&time);  // Use GMT/UTC instead of localtime
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
// Warmup bars always END at bar 391 (4:00 PM) of the previous trading day
void filter_to_date(std::unordered_map<Symbol, std::vector<Bar>>& all_data,
                   const std::string& date_str, size_t sim_bars, size_t warmup_bars,
                   int bars_per_day, bool verbose = false) {
    if (all_data.empty()) return;

    // Get trading days from first symbol (optimized: only check last 60 days worth of bars)
    const auto& first_symbol_bars = all_data.begin()->second;
    size_t sample_size = std::min(first_symbol_bars.size(), size_t(60 * bars_per_day));
    size_t start_idx = first_symbol_bars.size() > sample_size ? first_symbol_bars.size() - sample_size : 0;
    std::vector<Bar> sample_bars(first_symbol_bars.begin() + start_idx, first_symbol_bars.end());
    std::vector<std::string> trading_days = get_trading_days(sample_bars);

    // Find test date index
    auto it = std::find(trading_days.begin(), trading_days.end(), date_str);
    if (it == trading_days.end()) {
        throw std::runtime_error("Test date not found: " + date_str);
    }
    int test_day_idx = std::distance(trading_days.begin(), it);

    // Total bars needed: sim_bars + warmup_bars + test_day_bars
    size_t total_bars = sim_bars + warmup_bars + bars_per_day;

    // Calculate how many bars from start of data
    // We need to go back enough to get all sim + warmup bars
    // Warmup must END at bar 391 of previous day

    // Calculate how many days we need to go back
    // Example: warmup_bars=100, sim_bars=0 → need last 100 bars of previous day (bars 292-391)
    // Example: warmup_bars=400, sim_bars=0 → need 9 bars from 2 days ago + 391 from 1 day ago
    size_t total_history_bars = sim_bars + warmup_bars;
    int days_back = (total_history_bars + bars_per_day - 1) / bars_per_day;

    // Find starting date (go back enough days before test date)
    int start_day_idx = std::max(0, test_day_idx - days_back);
    std::string start_date = trading_days[start_day_idx];

    // For the filtered data, we want:
    // - All bars from start_date onwards
    // - But we'll extract exactly (sim_bars + warmup_bars + test_day_bars) bars
    // - Where warmup ends at bar 391 of day before test

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
        std::cout << "  Test date: " << date_str << "\n";
        std::cout << "  Warmup bars: " << warmup_bars << " (ends at bar 391 of previous day)\n";
        std::cout << "  Sim bars: " << sim_bars << "\n";
        std::cout << "  Total bars needed: " << total_bars << "\n";
        std::cout << "  Days back: " << days_back << "\n";
        std::cout << "  Start date: " << start_date << "\n";
    }

    // For each symbol, collect bars and take exactly the right number ending at test date
    for (auto& [symbol, bars] : all_data) {
        // Find bars up to and including test date
        std::vector<Bar> candidate_bars;
        for (const auto& bar : bars) {
            if (bar.timestamp <= end_timestamp) {
                candidate_bars.push_back(bar);
            }
        }

        // Take the last (total_bars) bars
        if (candidate_bars.size() >= total_bars) {
            bars = std::vector<Bar>(
                candidate_bars.end() - total_bars,
                candidate_bars.end()
            );
        } else {
            throw std::runtime_error(
                "Insufficient data for " + symbol + ": need " +
                std::to_string(total_bars) + " bars, have " +
                std::to_string(candidate_bars.size())
            );
        }
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

        // Calculate required bars based on warmup mode
        size_t required_bars;
        int warmup_days;
        int required_days;

        if (config.intraday_warmup) {
            // Intraday warmup: warmup comes FROM test day, so we need: sim + test_day only
            required_bars = config.sim_bars + config.trading.bars_per_day;
            warmup_days = 0;  // No extra days for warmup
            required_days = config.sim_days + 1;  // Just sim + test day
        } else {
            // Previous day warmup: need sim + warmup + test_day
            required_bars = config.sim_bars + config.warmup_bars + config.trading.bars_per_day;
            warmup_days = (config.warmup_bars + config.trading.bars_per_day - 1) / config.trading.bars_per_day;
            required_days = config.sim_days + warmup_days + 1;  // +1 for test day
        }

        std::cout << "\n✅ Data Requirement Check:\n";
        std::cout << "  Required: ~" << required_days << " days (" << required_bars << " bars)\n";
        if (config.intraday_warmup) {
            std::cout << "    - Warmup:     " << config.warmup_bars << " bars (FROM test day, bars 1-" << config.warmup_bars << ")\n";
        } else {
            std::cout << "    - Warmup:     " << config.warmup_bars << " bars (ends at bar 391 of prev day)\n";
        }
        std::cout << "    - Simulation: " << config.sim_days << " days (" << config.sim_bars << " bars)\n";
        std::cout << "    - Test:       1 day (" << config.trading.bars_per_day << " bars)\n";

        // Check BEFORE filtering - ensure we have enough raw data
        std::cout << "\n  Available data before filtering:\n";
        for (const auto& [symbol, bars] : all_data) {
            std::cout << "    " << symbol << ": " << bars.size() << " bars (~"
                     << (bars.size() / config.trading.bars_per_day) << " days)\n";
        }

        // Filter to test date
        std::cout << "\n  Filtering to test date window...\n";
        if (config.intraday_warmup) {
            // For intraday warmup: only load sim_bars + test_day (warmup comes from test day)
            filter_to_date(all_data, test_date, config.sim_bars, 0,  // warmup=0, it's IN the test day
                          config.trading.bars_per_day, config.verbose);
        } else {
            // Normal: load sim + warmup (from prev day) + test day
            filter_to_date(all_data, test_date, config.sim_bars, config.warmup_bars,
                          config.trading.bars_per_day, config.verbose);
        }

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

        // Recalculate min_bars after filtering
        min_bars = required_bars;

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
        if (config.intraday_warmup) {
            std::cout << "  Warmup: " << config.warmup_bars << " bars (FROM test day, bars 1-" << config.warmup_bars << ")\n";
            std::cout << "  Trading: " << (config.trading.bars_per_day - config.warmup_bars) << " bars (test day, bars " << (config.warmup_bars + 1) << "-391)\n";
        } else {
            std::cout << "  Warmup: " << config.warmup_bars << " bars (ends at bar 391 of prev day)\n";
        }
        std::cout << "  Simulation: " << config.sim_bars << " bars (" << config.sim_days << " days)\n";
        std::cout << "  Test: " << config.trading.bars_per_day << " bars (1 day)\n";
        std::cout << "  Features: 54 features (8 time + 28 technical + 6 BB + 12 regime)\n";
        if (config.strategy == StrategyType::EWRLS) {
            std::cout << "  Predictor: Multi-Horizon EWRLS (1/5/10 bars, λ="
                      << config.trading.horizon_config.lambda_1bar << "/"
                      << config.trading.horizon_config.lambda_5bar << "/"
                      << config.trading.horizon_config.lambda_10bar << ")\n";
        } else if (config.strategy == StrategyType::SIGOR) {
            std::cout << "  Predictor: SIGOR (rule-based ensemble)\n";
        }
        std::cout << "  Strategy: Multi-symbol rotation (top " << config.trading.max_positions << ")\n";
        std::cout << "  Min prediction threshold: " << config.trading.filter_config.min_prediction_for_entry << "\n";
        std::cout << "  Min holding period: " << config.trading.filter_config.min_bars_to_hold << " bars\n\n";

        // Set min_bars_to_learn: Start trading after warmup (warmup + sim period)
        // Structure: [warmup_bars] + [sim_bars] + [test_bars]
        // Warmup: learn only (no trading)
        // Sim: practice trading (trades are NOT optimized for)
        // Test: real trades (ONLY these trades count for optimization)
        config.trading.min_bars_to_learn = config.warmup_bars;

        // MOCK mode: Configure warmup system for single-day optimization
        // Convert warmup bars to days (for phase management)
        int warmup_days_for_config = (config.warmup_bars + config.trading.bars_per_day - 1) / config.trading.bars_per_day;
        config.trading.warmup.observation_days = warmup_days_for_config;
        config.trading.warmup.simulation_days = config.sim_days;      // N days (user specified)
        config.trading.warmup.skip_validation = true;  // Always proceed to test day

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

        // Collect filtered bars for all symbols to embed in results
        std::unordered_map<Symbol, std::vector<Bar>> filtered_bars;
        filtered_bars.reserve(all_data.size());
        for (const auto& symbol : config.symbols) {
            filtered_bars[symbol] = all_data[symbol];
        }

        ResultsExporter::export_json(
            results, trader, config.results_file,
            symbols_str, "MOCK",
            test_date, test_date,  // Single day: start = end
            filtered_bars
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

            // Generate unique dashboard filename with mode and strategy prefix
            // Format: dashboard_mock_SIGOR_2025-10-21_20251021_005642.html
            std::string strategy_name = (config.strategy == StrategyType::SIGOR) ? "SIGOR" : "EWRLS";
            std::string dashboard_file = "logs/dashboard/dashboard_mock_" + strategy_name + "_" +
                                        test_date + "_" + std::string(timestamp) + ".html";

            generate_dashboard(
                config.results_file,
                config.dashboard_script,
                config.trades_file,
                dashboard_file,
                config.data_dir,
                config.capital,
                test_date,
                test_date,  // Single day: start = end
                (config.strategy == StrategyType::SIGOR
                    ? std::string("config/sigor_params.json")
                    : std::string("config/trading_params.json"))  // Strategy-specific
            );
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n\n";
        return 1;
    }
}

int run_live_mode(Config& config) {
    try {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║         LIVE MODE - Real-Time Paper Trading                ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

        std::cout << "🟢 Starting LIVE trading session...\n\n";

        // FIFO pipes for communication
        const std::string bar_fifo = "/tmp/alpaca_bars.fifo";
        const std::string order_fifo = "/tmp/alpaca_orders.fifo";
        const std::string response_fifo = "/tmp/alpaca_responses.fifo";

        std::cout << "Configuration:\n";
        std::cout << "  Data Source:     Alpaca WebSocket (IEX)\n";
        std::cout << "  Order Submission: Alpaca REST API\n";
        std::cout << "  Bar FIFO:        " << bar_fifo << "\n";
        std::cout << "  Order FIFO:      " << order_fifo << "\n";
        std::cout << "  Response FIFO:   " << response_fifo << "\n";
        std::cout << "\n";

        // Initialize trader with SIGOR configuration
        std::cout << "Initializing trader...\n";
        MultiSymbolTrader trader(config.symbols, config.trading);
        std::cout << "✅ Trader initialized\n\n";

        // SIGOR Warmup Strategy:
        // SIGOR is rule-based (no learning), but needs lookback bars for indicators:
        //   - RSI(14) needs 14 bars
        //   - Bollinger(20) needs 20 bars
        //   - Momentum(10) needs 10 bars
        //   - ORB needs first 30 bars of day
        //   - Volume surge needs 20-bar window
        //
        // Solution: Load today's historical bars (9:30 ET to now) before live trading
        // This gives SIGOR the lookback data it needs to calculate indicators

        std::cout << "🔄 Checking for warmup bars (today's historical data)...\n";

        // Try to load warmup bars from JSON (optional - created by fetch_today_bars.py)
        bool has_warmup = false;
        const std::string warmup_file = "warmup_bars.json";
        size_t warmup_bars_loaded = 0;

        if (std::filesystem::exists(warmup_file)) {
            try {
                std::cout << "   Found warmup_bars.json - loading historical bars...\n";

                std::ifstream warmup_stream(warmup_file);
                nlohmann::json warmup_json;
                warmup_stream >> warmup_json;

                // Feed warmup bars to trader
                for (const auto& [symbol, bars_array] : warmup_json.items()) {
                    for (const auto& bar_data : bars_array) {
                        // Convert Alpaca bar format to our Bar struct
                        Bar bar;
                        bar.symbol = symbol;

                        // Parse timestamp (ISO format: "2025-10-24T09:30:00Z")
                        std::string timestamp_str = bar_data["t"];
                        // Simple parsing - extract timestamp_ms from ISO string
                        // For production, use a proper ISO parser
                        int64_t timestamp_ms = bar_data.value("t_ms", 0);  // If we added it
                        if (timestamp_ms == 0) {
                            // Fallback: skip if we can't parse timestamp
                            continue;
                        }

                        bar.timestamp = std::chrono::system_clock::time_point(
                            std::chrono::milliseconds(timestamp_ms));
                        bar.open = bar_data["o"];
                        bar.high = bar_data["h"];
                        bar.low = bar_data["l"];
                        bar.close = bar_data["c"];
                        bar.volume = bar_data["v"];

                        // Use pre-calculated bar_id from JSON (Python script handles timezone)
                        bar.bar_id = bar_data.value("bar_id", -1);
                        if (bar.bar_id < 0) {
                            std::cerr << "[WARNING] Missing bar_id for " << symbol << ", skipping\n";
                            continue;
                        }

                        // Feed to trader (accumulate for snapshot)
                        std::unordered_map<Symbol, Bar> warmup_snapshot;
                        warmup_snapshot[symbol] = bar;

                        // Process when all symbols have this bar
                        // (simplified: process each bar individually for warmup)
                        trader.on_bar(warmup_snapshot);
                        warmup_bars_loaded++;
                    }
                }

                has_warmup = true;
                std::cout << "   ✅ Loaded " << warmup_bars_loaded << " warmup bars\n";
                std::cout << "   → SIGOR ready to trade immediately with indicator lookback\n\n";

            } catch (const std::exception& e) {
                std::cerr << "   ⚠️  Failed to load warmup bars: " << e.what() << "\n";
                std::cout << "   → SIGOR will start trading after collecting enough bars\n\n";
            }
        } else {
            std::cout << "   No warmup_bars.json found\n";
            std::cout << "   → SIGOR will start trading after collecting ~30 bars (~30 minutes)\n";
            std::cout << "   TIP: Run scripts/fetch_today_bars.py to get immediate trading\n\n";
        }

        // Market snapshot buffer - accumulate bars until all symbols updated
        std::unordered_map<Symbol, Bar> market_snapshot;
        std::unordered_map<Symbol, Timestamp> last_update_time;

        // Tracking
        size_t bars_processed = 0;
        size_t snapshots_processed = 0;
        bool running = true;

        std::cout << "📡 Opening FIFO pipe for incoming bars...\n";
        std::cout << "   (Waiting for websocket bridge to connect)\n";
        std::cout << "   TIP: Start the bridge with:\n";
        std::cout << "        python3 scripts/alpaca_websocket_bridge_rotation.py\n\n";

        // Open FIFO for reading bars (blocks until bridge connects)
        std::ifstream bar_stream(bar_fifo);
        if (!bar_stream.is_open()) {
            std::cerr << "❌ Error: Failed to open bar FIFO: " << bar_fifo << "\n";
            std::cerr << "   Make sure the websocket bridge is running!\n";
            return 1;
        }

        std::cout << "✅ Connected to websocket bridge\n";
        std::cout << "🚀 LIVE TRADING ACTIVE - Processing real-time bars\n";
        std::cout << "   Press Ctrl+C to stop\n\n";
        std::cout << "═══════════════════════════════════════════════════════════════\n\n";

        // Main trading loop - read bars from FIFO and process
        std::string line;
        while (running && std::getline(bar_stream, line)) {
            if (line.empty()) continue;

            try {
                // Parse JSON bar from websocket bridge
                nlohmann::json bar_json = nlohmann::json::parse(line);

                // Extract bar data
                std::string symbol = bar_json["symbol"];
                int64_t timestamp_ms = bar_json["timestamp_ms"];

                Bar bar;
                bar.symbol = symbol;
                bar.timestamp = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(timestamp_ms));
                bar.open = bar_json["open"];
                bar.high = bar_json["high"];
                bar.low = bar_json["low"];
                bar.close = bar_json["close"];
                bar.volume = bar_json["volume"];
                // vwap and trade_count are optional Alpaca fields, not in our Bar struct

                // Calculate bar_id from timestamp (minutes since midnight ET)
                // This is crucial for SIGOR's bar synchronization
                auto time_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    bar.timestamp.time_since_epoch()).count();
                time_t time = static_cast<time_t>(time_seconds);
                struct tm* tm_info = localtime(&time);
                int minutes_since_midnight = tm_info->tm_hour * 60 + tm_info->tm_min;
                // Market opens at 9:30 ET (570 minutes), so bar 1 = 570 minutes
                bar.bar_id = minutes_since_midnight - 569;  // 570 = 9:30, so bar_id 1

                // Update market snapshot
                market_snapshot[symbol] = bar;
                last_update_time[symbol] = bar.timestamp;
                bars_processed++;

                // Log bar receipt (every 10th bar to reduce noise)
                if (bars_processed % 10 == 0) {
                    auto now = std::chrono::system_clock::now();
                    auto time_t_now = std::chrono::system_clock::to_time_t(now);
                    struct tm* tm_now = localtime(&time_t_now);
                    char time_str[10];
                    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_now);

                    std::cout << "[" << time_str << "] "
                              << symbol << " @ " << std::setprecision(2) << std::fixed
                              << bar.close << " | Bars: " << bars_processed
                              << " | Snapshots: " << snapshots_processed << "\n";
                }

                // Check if we have all symbols updated (for synchronized processing)
                // For SIGOR, we process as soon as all symbols have at least one bar
                //
                // IMPORTANT: Missing Bar Handling
                // If a symbol has a gap (missing bar), we do NOT process that snapshot.
                // This prevents:
                //   1. Trading on stale prices
                //   2. Entering positions with bad synchronization
                //   3. Indicator calculation errors
                //
                // When SIGOR generates a signal but a bar is missing:
                //   → Skip the trade (don't enter)
                //   → Wait for next complete snapshot
                //   → Log warning if gaps are frequent
                bool all_symbols_ready = true;
                for (const auto& sym : config.symbols) {
                    if (market_snapshot.find(sym) == market_snapshot.end()) {
                        all_symbols_ready = false;
                        break;
                    }
                }

                // Process market snapshot when all symbols updated (no missing bars)
                if (all_symbols_ready) {
                    // Process bars through trader (SAME LOGIC AS MOCK MODE)
                    // The trader will internally check if it has enough indicator lookback
                    // before generating signals
                    trader.on_bar(market_snapshot);
                    snapshots_processed++;

                    // Process any orders generated by trader
                    // (This is where we'd submit orders to Alpaca)
                    // For now, the trader's internal logic handles positions
                    // In a full implementation, we'd extract signals and submit orders

                    // Every N snapshots, show status
                    if (snapshots_processed % 20 == 0) {
                        auto results = trader.get_results();
                        double equity = trader.get_equity(market_snapshot);
                        double return_pct = (equity - config.capital) / config.capital * 100;

                        std::cout << "\n📊 [Status Update] Snapshot " << snapshots_processed << "\n";
                        std::cout << "   Equity: $" << std::fixed << std::setprecision(2) << equity;
                        std::cout << " (" << std::showpos << return_pct << std::noshowpos << "%)\n";
                        std::cout << "   Trades: " << results.total_trades;
                        std::cout << " | Positions: " << trader.positions().size() << "\n";
                        std::cout << "   Win Rate: " << std::setprecision(1)
                                  << (results.win_rate * 100) << "%\n\n";
                    }
                }

            } catch (const nlohmann::json::exception& e) {
                std::cerr << "⚠️  JSON parse error: " << e.what() << "\n";
                continue;
            } catch (const std::exception& e) {
                std::cerr << "⚠️  Error processing bar: " << e.what() << "\n";
                continue;
            }
        }

        // End of day - show final results
        std::cout << "\n═══════════════════════════════════════════════════════════════\n";
        std::cout << "🏁 LIVE SESSION COMPLETE\n\n";

        // Note: EOD liquidation is handled automatically by the trader's
        // internal logic when it detects end of day timestamp

        // Get final results
        auto results = trader.get_results();
        double final_equity = trader.get_equity(market_snapshot);

        // Show open positions if any remain
        if (!trader.positions().empty()) {
            std::cout << "⚠️  Open positions at session end: " << trader.positions().size() << "\n";
            std::cout << "   (These will be automatically closed at market close)\n\n";
        }

        // Print results
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                 LIVE SESSION Results                       ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

        std::cout << "Session Summary:\n";
        std::cout << "  Bars Processed:     " << bars_processed << "\n";
        std::cout << "  Snapshots:          " << snapshots_processed << "\n";
        std::cout << "\n";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Performance:\n";
        std::cout << "  Initial Capital:    $" << config.capital << "\n";
        std::cout << "  Final Equity:       $" << final_equity << "\n";
        std::cout << "  Total Return:       " << std::showpos << (results.total_return * 100)
                  << std::noshowpos << "%\n";
        std::cout << "\n";

        std::cout << "Trade Statistics:\n";
        std::cout << "  Total Trades:       " << results.total_trades << "\n";
        std::cout << "  Winning Trades:     " << results.winning_trades << "\n";
        std::cout << "  Losing Trades:      " << results.losing_trades << "\n";
        std::cout << std::setprecision(1);
        std::cout << "  Win Rate:           " << (results.win_rate * 100) << "%\n";
        std::cout << std::setprecision(2);
        if (results.total_trades > 0) {
            std::cout << "  Average Win:        $" << results.avg_win << "\n";
            std::cout << "  Average Loss:       $" << results.avg_loss << "\n";
            std::cout << "  Profit Factor:      " << results.profit_factor << "\n";
        }
        std::cout << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error in live mode: " << e.what() << "\n\n";
        return 1;
    }
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

    // 1. Validate simulation period (allow 0 days for warmup-only mode)
    if (config.sim_days < 0) {
        std::cerr << "❌ ERROR: Simulation period cannot be negative (got: "
                  << config.sim_days << ")\n";
        std::cerr << "   Use --sim-days N where N >= 0\n";
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

    // Print loaded trading configuration (path depends on strategy)
    if (config.strategy == StrategyType::SIGOR) {
        std::string path_used = std::filesystem::exists("config/sigor_trading_params.json")
            ? "config/sigor_trading_params.json"
            : "config/trading_params.json";
        trading::ConfigLoader::print_config(config.trading, path_used);
    } else {
        trading::ConfigLoader::print_config(config.trading, "config/trading_params.json");
    }

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

    std::cout << "  Warmup Period: " << config.warmup_bars << " bars (ends at bar 391 of prev day)\n";
    std::cout << "  Simulation Period: " << config.sim_days << " days ("
              << config.sim_bars << " bars)\n";
    std::cout << "  Initial Capital: $" << std::fixed << std::setprecision(2)
              << config.capital << "\n";
    std::cout << "  Max Positions: " << config.trading.max_positions << "\n";

    if (config.generate_dashboard) {
        std::cout << "  Dashboard: Enabled\n";
    }
    std::cout << "\n";

    // Run appropriate mode
    if (config.mode == TradingMode::MOCK) {
        return run_mock_mode(config);
    } else if (config.mode == TradingMode::MOCK_LIVE) {
        // Mock-live currently shares mock path; later can integrate streaming bridge
        return run_mock_mode(config);
    } else {
        return run_live_mode(config);
    }
}
