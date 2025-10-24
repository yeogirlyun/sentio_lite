#include "strategy/sigor_strategy.h"
#include "utils/data_loader.h"
#include <iostream>
#include <iomanip>
#include <map>
#include <vector>
#include <ctime>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <algorithm>

using namespace trading;

struct TradeStats {
    double total_return = 0.0;
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double total_profit = 0.0;
    double total_loss = 0.0;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <date> [--symbol TQQQ]\n";
        std::cerr << "Example: " << argv[0] << " 2025-10-17\n";
        return 1;
    }

    std::string date = argv[1];
    std::string test_symbol = "TQQQ";

    // Parse optional --symbol flag
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--symbol" && i + 1 < argc) {
            test_symbol = argv[i + 1];
            ++i;
        }
    }

    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  SIGOR STRATEGY TEST\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  Date: " << date << "\n";
    std::cout << "  Symbol: " << test_symbol << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";

    // Symbol list
    std::vector<std::string> symbols = {
        "TQQQ", "SQQQ", "TNA", "TZA", "UVXY", "SVIX",
        "SOXS", "SOXL", "SPXL", "SPXS", "FAS", "FAZ"
    };

    // Load data for all symbols
    std::map<std::string, std::vector<Bar>> symbol_data;

    std::cout << "📊 Loading data for " << date << "...\n";

    for (const auto& sym : symbols) {
        std::string file_path = "data/" + sym + "_RTH_NH.bin";
        try {
            auto all_bars = DataLoader::load(file_path);

            // Filter bars for the requested date
            std::vector<Bar> date_bars;
            for (const auto& bar : all_bars) {
                auto time_t = std::chrono::system_clock::to_time_t(bar.timestamp);
                struct tm tm = *std::localtime(&time_t);

                char date_str[11];
                std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);

                if (std::string(date_str) == date) {
                    date_bars.push_back(bar);
                }
            }

            if (!date_bars.empty()) {
                symbol_data[sym] = date_bars;
                std::cout << "  " << sym << ": " << date_bars.size() << " bars\n";
            }
        } catch (const std::exception& e) {
            // File not found or error - skip this symbol
        }
    }

    if (symbol_data.empty()) {
        std::cerr << "\n❌ No data loaded for date " << date << "\n";
        return 1;
    }

    // Create Sigor strategy for test symbol
    SigorConfig config;

    // Load config from environment variable if set (for Optuna optimization)
    const char* config_path = std::getenv("SIGOR_CONFIG");
    if (config_path != nullptr) {
        std::ifstream config_file(config_path);
        if (config_file.is_open()) {
            // Simple JSON parsing for our config
            std::string line;
            while (std::getline(config_file, line)) {
                // Parse key-value pairs (simple approach)
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 1);

                    // Remove quotes, spaces, commas
                    key.erase(remove(key.begin(), key.end(), ' '), key.end());
                    key.erase(remove(key.begin(), key.end(), '\"'), key.end());
                    value.erase(remove(value.begin(), value.end(), ' '), value.end());
                    value.erase(remove(value.begin(), value.end(), ','), value.end());

                    // Update config
                    if (key == "k") config.k = std::stod(value);
                    else if (key == "w_boll") config.w_boll = std::stod(value);
                    else if (key == "w_rsi") config.w_rsi = std::stod(value);
                    else if (key == "w_mom") config.w_mom = std::stod(value);
                    else if (key == "w_vwap") config.w_vwap = std::stod(value);
                    else if (key == "w_orb") config.w_orb = std::stod(value);
                    else if (key == "w_ofi") config.w_ofi = std::stod(value);
                    else if (key == "w_vol") config.w_vol = std::stod(value);
                    else if (key == "win_boll") config.win_boll = std::stoi(value);
                    else if (key == "win_rsi") config.win_rsi = std::stoi(value);
                    else if (key == "win_mom") config.win_mom = std::stoi(value);
                    else if (key == "win_vwap") config.win_vwap = std::stoi(value);
                    else if (key == "orb_opening_bars") config.orb_opening_bars = std::stoi(value);
                    else if (key == "vol_window") config.vol_window = std::stoi(value);
                    else if (key == "warmup_bars") config.warmup_bars = std::stoi(value);
                }
            }
            config_file.close();
            std::cout << "✅ Loaded config from " << config_path << "\n";
        }
    }

    SigorStrategy sigor(config);

    std::cout << "\n🔧 Sigor Configuration:\n";
    std::cout << "  Sharpness (k): " << config.k << "\n";
    std::cout << "  Detector weights: "
              << "Boll=" << config.w_boll << ", "
              << "RSI=" << config.w_rsi << ", "
              << "Mom=" << config.w_mom << ", "
              << "VWAP=" << config.w_vwap << "\n";
    std::cout << "  Window sizes: "
              << "Boll=" << config.win_boll << ", "
              << "RSI=" << config.win_rsi << ", "
              << "Mom=" << config.win_mom << "\n";
    std::cout << "  Warmup: " << config.warmup_bars << " bars\n\n";

    // Check if test symbol exists
    if (symbol_data.find(test_symbol) == symbol_data.end()) {
        std::cerr << "❌ Symbol " << test_symbol << " not found in loaded data\n";
        return 1;
    }

    const auto& bars = symbol_data[test_symbol];

    // Run Sigor strategy
    std::cout << "🚀 Running Sigor strategy on " << test_symbol << "...\n\n";

    TradeStats stats;
    double position = 0.0;  // 0 = no position, 1 = long, -1 = short
    double entry_price = 0.0;
    int bars_in_position = 0;

    int long_signals = 0;
    int short_signals = 0;
    int neutral_signals = 0;

    for (size_t i = 0; i < bars.size(); ++i) {
        const Bar& bar = bars[i];
        SigorSignal signal = sigor.generate_signal(bar, test_symbol);

        // Count signal types
        if (signal.is_long) long_signals++;
        else if (signal.is_short) short_signals++;
        else neutral_signals++;

        // Only trade after warmup
        if (!sigor.is_warmed_up()) continue;

        // Print every 50th signal
        if (i % 50 == 0 && i > 0) {
            std::cout << "Bar " << std::setw(3) << i
                      << " | Prob=" << std::fixed << std::setprecision(3) << signal.probability
                      << " | Conf=" << signal.confidence
                      << " | " << (signal.is_long ? "LONG " : signal.is_short ? "SHORT" : "NEUT ")
                      << " | Price=" << std::setprecision(2) << bar.close
                      << "\n";
        }

        // Simple trading logic: enter on strong signals, exit on reversal or neutral
        if (position == 0.0) {
            // Enter long on strong bullish signal
            if (signal.is_long && signal.confidence > 0.6) {
                position = 1.0;
                entry_price = bar.close;
                bars_in_position = 0;
                stats.total_trades++;
            }
            // Enter short on strong bearish signal
            else if (signal.is_short && signal.confidence > 0.6) {
                position = -1.0;
                entry_price = bar.close;
                bars_in_position = 0;
                stats.total_trades++;
            }
        } else {
            bars_in_position++;

            // Exit conditions
            bool should_exit = false;

            // Exit long if signal reverses or goes neutral
            if (position > 0 && (signal.is_short || (signal.is_neutral && bars_in_position > 5))) {
                should_exit = true;
            }
            // Exit short if signal reverses or goes neutral
            else if (position < 0 && (signal.is_long || (signal.is_neutral && bars_in_position > 5))) {
                should_exit = true;
            }

            if (should_exit) {
                double pnl = position * (bar.close - entry_price);
                double pnl_pct = pnl / entry_price;
                stats.total_return += pnl_pct;

                if (pnl > 0) {
                    stats.winning_trades++;
                    stats.total_profit += pnl;
                } else {
                    stats.losing_trades++;
                    stats.total_loss += std::abs(pnl);
                }

                position = 0.0;
            }
        }
    }

    // Close any open position at end of day
    if (position != 0.0) {
        const Bar& last_bar = bars.back();
        double pnl = position * (last_bar.close - entry_price);
        double pnl_pct = pnl / entry_price;
        stats.total_return += pnl_pct;

        if (pnl > 0) {
            stats.winning_trades++;
            stats.total_profit += pnl;
        } else {
            stats.losing_trades++;
            stats.total_loss += std::abs(pnl);
        }
    }

    // Print results
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  RESULTS (" << test_symbol << " on " << date << ")\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";

    std::cout << "📊 Signal Distribution:\n";
    std::cout << "  Long signals:    " << long_signals << " ("
              << (100.0 * long_signals / bars.size()) << "%)\n";
    std::cout << "  Short signals:   " << short_signals << " ("
              << (100.0 * short_signals / bars.size()) << "%)\n";
    std::cout << "  Neutral signals: " << neutral_signals << " ("
              << (100.0 * neutral_signals / bars.size()) << "%)\n\n";

    std::cout << "💰 Trading Performance:\n";
    std::cout << "  Total trades:    " << stats.total_trades << "\n";
    std::cout << "  Winning trades:  " << stats.winning_trades << "\n";
    std::cout << "  Losing trades:   " << stats.losing_trades << "\n";

    if (stats.total_trades > 0) {
        double win_rate = 100.0 * stats.winning_trades / stats.total_trades;
        std::cout << "  Win rate:        " << std::fixed << std::setprecision(1) << win_rate << "%\n";
    }

    std::cout << "\n  Total return:    " << std::fixed << std::setprecision(4)
              << (stats.total_return * 100.0) << "%\n";
    std::cout << "  MRD:             " << (stats.total_return * 100.0) << "%\n";

    if (stats.total_loss > 0) {
        double profit_factor = stats.total_profit / stats.total_loss;
        std::cout << "  Profit factor:   " << std::setprecision(2) << profit_factor << "\n";
    }

    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";

    return 0;
}
