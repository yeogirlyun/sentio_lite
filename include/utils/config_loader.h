#pragma once

#include "trading/multi_symbol_trader.h"
#include "strategy/sigor_strategy.h"
#include "strategy/williams_rsi_strategy.h"
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace trading {

namespace config_loader_internal {
    // Shared inline helpers to avoid duplicate definitions across loaders
    inline size_t skip_ws(const std::string& s, size_t pos) {
        while (pos < s.length() && std::isspace(static_cast<unsigned char>(s[pos]))) pos++;
        return pos;
    }

    inline size_t find_key_pos(const std::string& content, const std::string& key) {
        std::string search = "\"" + key + "\":";
        size_t pos = content.find(search);
        if (pos == std::string::npos) {
            throw std::runtime_error("Config parsing error: '" + key + "' not found");
        }
        return skip_ws(content, pos + search.length());
    }

    inline int parse_int_value(const std::string& content, const std::string& key) {
        size_t pos = find_key_pos(content, key);
        size_t end = pos;
        while (end < content.length() && (std::isdigit(static_cast<unsigned char>(content[end])) || content[end] == '-')) end++;
        if (end == pos) throw std::runtime_error("Config parsing error: invalid value for '" + key + "'");
        return std::stoi(content.substr(pos, end - pos));
    }

    inline double parse_double_value(const std::string& content, const std::string& key) {
        size_t pos = find_key_pos(content, key);
        size_t end = pos;
        while (end < content.length() && (std::isdigit(static_cast<unsigned char>(content[end])) || content[end] == '.' || content[end] == '-' || content[end] == 'e' || content[end] == 'E')) end++;
        if (end == pos) throw std::runtime_error("Config parsing error: invalid value for '" + key + "'");
        return std::stod(content.substr(pos, end - pos));
    }
}

/**
 * Load trading configuration from JSON file
 *
 * Parses config/trading_params.json and populates TradingConfig
 */
class ConfigLoader {
public:
    /**
     * Load configuration from JSON file
     *
     * @param config_path Path to trading_params.json
     * @return TradingConfig populated with values from file
     * @throws runtime_error if file not found or parsing fails
     */
    static TradingConfig load(const std::string& config_path = "config/trading_params.json") {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + config_path +
                "\n\nRun Optuna optimization first:\n  python3 tools/optuna_5day_search.py --end-date 2025-10-23");
        }

        // Read entire file
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        // Parse JSON manually (simple parser for our specific format)
        TradingConfig config;

        // Extract parameters section
        size_t params_start = content.find("\"parameters\"");
        if (params_start == std::string::npos) {
            throw std::runtime_error("Invalid config file: 'parameters' section not found");
        }

        // Parse each parameter
        using namespace config_loader_internal;
        config.max_positions = parse_int_value(content, "max_positions");
        config.min_bars_to_learn = parse_int_value(content, "min_bars_to_learn");
        // Removed: EWRLS lookback_window
        config.bars_per_day = parse_int_value(content, "bars_per_day");
        config.win_multiplier = parse_double_value(content, "win_multiplier");
        config.loss_multiplier = parse_double_value(content, "loss_multiplier");
        config.initial_capital = parse_double_value(content, "initial_capital");
        config.rotation_strength_delta = parse_double_value(content, "rotation_strength_delta");
        config.min_rank_strength = parse_double_value(content, "min_rank_strength");

        // Removed: EWRLS horizon_config

        // Removed: adaptive prediction threshold (EWRLS-era)

        // Min hold period (prevents churning)
        if (content.find("\"min_bars_to_hold\":") != std::string::npos) {
            config.filter_config.min_bars_to_hold = parse_int_value(content, "min_bars_to_hold");
        }

        // Profit Target & Stop Loss (from online_trader v2.0)
        if (content.find("\"profit_target_pct\":") != std::string::npos) {
            config.profit_target_pct = parse_double_value(content, "profit_target_pct");
        }
        if (content.find("\"stop_loss_pct\":") != std::string::npos) {
            config.stop_loss_pct = parse_double_value(content, "stop_loss_pct");
        }

        // Position Sizing Configuration (Kelly Criterion with volatility adjustment)
        if (content.find("\"position_sizing\":") != std::string::npos) {
            // Find position_sizing section
            size_t ps_start = content.find("\"position_sizing\":");
            if (ps_start != std::string::npos) {
                std::string ps_section = content.substr(ps_start);

                // Parse position sizing parameters if they exist
                if (ps_section.find("\"expected_win_pct\":") != std::string::npos) {
                    config.position_sizing.expected_win_pct = parse_double_value(ps_section, "expected_win_pct");
                }
                if (ps_section.find("\"expected_loss_pct\":") != std::string::npos) {
                    config.position_sizing.expected_loss_pct = parse_double_value(ps_section, "expected_loss_pct");
                }
                if (ps_section.find("\"fractional_kelly\":") != std::string::npos) {
                    config.position_sizing.fractional_kelly = parse_double_value(ps_section, "fractional_kelly");
                }
                if (ps_section.find("\"min_position_pct\":") != std::string::npos) {
                    config.position_sizing.min_position_pct = parse_double_value(ps_section, "min_position_pct");
                }
                if (ps_section.find("\"max_position_pct\":") != std::string::npos) {
                    config.position_sizing.max_position_pct = parse_double_value(ps_section, "max_position_pct");
                }
                if (ps_section.find("\"volatility_lookback\":") != std::string::npos) {
                    config.position_sizing.volatility_lookback = parse_int_value(ps_section, "volatility_lookback");
                }
                if (ps_section.find("\"max_volatility_reduce\":") != std::string::npos) {
                    config.position_sizing.max_volatility_reduce = parse_double_value(ps_section, "max_volatility_reduce");
                }
                if (ps_section.find("\"enable_volatility_adjustment\": true") != std::string::npos) {
                    config.position_sizing.enable_volatility_adjustment = true;
                } else if (ps_section.find("\"enable_volatility_adjustment\": false") != std::string::npos) {
                    config.position_sizing.enable_volatility_adjustment = false;
                }
            }
        }

        // Validate: fail fast on unsupported/deprecated parameters
        validate_no_deprecated_params(content);

        return config;
    }

    /**
     * Display loaded configuration
     */
    static void print_config(const TradingConfig& config, const std::string& config_path) {
        std::cout << "\n📋 Configuration loaded from: " << config_path << "\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "Position Management:\n";
        std::cout << "  Max Positions:       " << config.max_positions << "\n";
        std::cout << "\n";

        // Removed: EWRLS parameters section
        std::cout << "\n";

        std::cout << "Entry Rules:\n";
        std::cout << "  Min Bars to Learn:        " << config.min_bars_to_learn << " bars\n";
        std::cout << "\n";

        std::cout << "Rotation Strategy:\n";
        std::cout << "  Rotation Delta:      " << (config.rotation_strength_delta * 100) << "%\n";
        std::cout << "  Min Rank Strength:   " << (config.min_rank_strength * 100) << "%\n";
        std::cout << "\n";

        std::cout << "Other:\n";
        std::cout << "  Initial Capital:     $" << config.initial_capital << "\n";
        // Removed: Lookback Window (EWRLS)
        std::cout << "  Win Multiplier:      " << config.win_multiplier << "\n";
        std::cout << "  Loss Multiplier:     " << config.loss_multiplier << "\n";
        std::cout << "  Bars per Day:        " << config.bars_per_day << "\n";
        std::cout << "═══════════════════════════════════════════════════════\n\n";
    }

private:

    /**
     * Validate config file doesn't contain deprecated/unsupported parameters
     * Fails fast with clear error message
     */
    static void validate_no_deprecated_params(const std::string& content) {
        // List of deprecated/removed parameters
        const std::vector<std::string> deprecated_params = {
            "emergency_stop_loss_pct"
            // stop_loss_pct is now SUPPORTED (from online_trader v2.0)
            // profit_target_pct is now SUPPORTED (from online_trader v2.0)
            // min_bars_to_hold is now SUPPORTED - prevents churning
        };

        std::vector<std::string> found_deprecated;

        for (const auto& param : deprecated_params) {
            std::string search = "\"" + param + "\":";
            if (content.find(search) != std::string::npos) {
                found_deprecated.push_back(param);
            }
        }

        if (!found_deprecated.empty()) {
            std::string error_msg = "\n\n❌ ERROR: Config file contains deprecated/unsupported parameters:\n\n";
            for (const auto& param : found_deprecated) {
                error_msg += "  - " + param + " (NO LONGER SUPPORTED)\n";
            }
            error_msg += "\nThese parameters have been removed from the system:\n";
            error_msg += "  - emergency_stop_loss_pct: Use stop_loss_pct instead\n\n";
            error_msg += "Please remove these parameters from config/trading_params.json\n\n";

            throw std::runtime_error(error_msg);
        }
    }
};

/**
 * Load SIGOR strategy configuration from JSON file
 */
class SigorConfigLoader {
public:
    /**
     * Load SIGOR configuration from JSON file
     *
     * @param config_path Path to sigor_params.json
     * @return SigorConfig populated with values from file
     * @throws runtime_error if file not found or parsing fails
     */
    static SigorConfig load(const std::string& config_path = "config/sigor_params.json") {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open SIGOR config file: " + config_path);
        }

        // Read entire file
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        // Parse JSON manually (simple parser for our specific format)
        SigorConfig config;

        // Extract parameters section
        size_t params_start = content.find("\"parameters\"");
        if (params_start == std::string::npos) {
            throw std::runtime_error("Invalid SIGOR config file: 'parameters' section not found");
        }

        // Parse each parameter
        using namespace config_loader_internal;
        config.k = parse_double_value(content, "k");
        config.w_boll = parse_double_value(content, "w_boll");
        config.w_rsi = parse_double_value(content, "w_rsi");
        config.w_mom = parse_double_value(content, "w_mom");
        config.w_vwap = parse_double_value(content, "w_vwap");
        config.w_orb = parse_double_value(content, "w_orb");
        config.w_ofi = parse_double_value(content, "w_ofi");
        config.w_vol = parse_double_value(content, "w_vol");
        config.win_boll = parse_int_value(content, "win_boll");
        config.win_rsi = parse_int_value(content, "win_rsi");
        config.win_mom = parse_int_value(content, "win_mom");
        config.win_vwap = parse_int_value(content, "win_vwap");
        config.orb_opening_bars = parse_int_value(content, "orb_opening_bars");
        config.vol_window = parse_int_value(content, "vol_window");
        // Optional warmup_bars for SIGOR (fallback to default if missing)
        if (content.find("\"warmup_bars\":") != std::string::npos) {
            config.warmup_bars = parse_int_value(content, "warmup_bars");
        }

        return config;
    }

    /**
     * Display loaded SIGOR configuration
     */
    static void print_config(const SigorConfig& config, const std::string& config_path) {
        std::cout << "\n📋 SIGOR Configuration loaded from: " << config_path << "\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "Fusion Parameter:\n";
        std::cout << "  k (sharpness):       " << config.k << "\n";
        std::cout << "\n";

        std::cout << "Detector Weights:\n";
        std::cout << "  Bollinger Bands:     " << config.w_boll << "\n";
        std::cout << "  RSI:                 " << config.w_rsi << "\n";
        std::cout << "  Momentum:            " << config.w_mom << "\n";
        std::cout << "  VWAP:                " << config.w_vwap << "\n";
        std::cout << "  ORB:                 " << config.w_orb << "\n";
        std::cout << "  OFI:                 " << config.w_ofi << "\n";
        std::cout << "  Volume:              " << config.w_vol << "\n";
        std::cout << "\n";

        std::cout << "Window Parameters:\n";
        std::cout << "  Bollinger Window:    " << config.win_boll << "\n";
        std::cout << "  RSI Window:          " << config.win_rsi << "\n";
        std::cout << "  Momentum Window:     " << config.win_mom << "\n";
        std::cout << "  VWAP Window:         " << config.win_vwap << "\n";
        std::cout << "  ORB Opening Bars:    " << config.orb_opening_bars << "\n";
        std::cout << "  Volume Window:       " << config.vol_window << "\n";
        std::cout << "  Warmup Bars:         " << config.warmup_bars << "\n";
        std::cout << "═══════════════════════════════════════════════════════\n\n";
    }

private:
};

/**
 * Load AWR strategy configuration from JSON file
 */
class AwrConfigLoader {
public:
    static WilliamsRsiConfig load(const std::string& config_path = "config/awr_params.json") {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            // Fallback to defaults if file not found
            return WilliamsRsiConfig();
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        WilliamsRsiConfig cfg;
        using namespace config_loader_internal;
        if (content.find("\"williams_period\":") != std::string::npos)
            cfg.williams_period = parse_int_value(content, "williams_period");
        if (content.find("\"rsi_period\":") != std::string::npos)
            cfg.rsi_period = parse_int_value(content, "rsi_period");
        if (content.find("\"bb_period\":") != std::string::npos)
            cfg.bb_period = parse_int_value(content, "bb_period");
        if (content.find("\"bb_stddev\":") != std::string::npos)
            cfg.bb_stddev = parse_double_value(content, "bb_stddev");
        if (content.find("\"approach_threshold\":") != std::string::npos)
            cfg.approach_threshold = parse_int_value(content, "approach_threshold");
        if (content.find("\"fresh_bars\":") != std::string::npos)
            cfg.fresh_bars = parse_int_value(content, "fresh_bars");
        if (content.find("\"lower_band_zone\":") != std::string::npos)
            cfg.lower_band_zone = parse_double_value(content, "lower_band_zone");
        if (content.find("\"upper_band_zone\":") != std::string::npos)
            cfg.upper_band_zone = parse_double_value(content, "upper_band_zone");
        if (content.find("\"crossing_strength\":") != std::string::npos)
            cfg.crossing_strength = parse_double_value(content, "crossing_strength");
        if (content.find("\"approaching_strength\":") != std::string::npos)
            cfg.approaching_strength = parse_double_value(content, "approaching_strength");
        if (content.find("\"fresh_strength\":") != std::string::npos)
            cfg.fresh_strength = parse_double_value(content, "fresh_strength");

        return cfg;
    }
};

} // namespace trading
