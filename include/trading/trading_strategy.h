#pragma once

#include <string>

namespace trading {

/**
 * Trading Strategy Type
 */
enum class StrategyType {
    EWRLS,   // Multi-Horizon EWRLS with rotation
    SIGOR    // Rule-based SIGOR ensemble
};

/**
 * Parse strategy type from string
 */
inline StrategyType parse_strategy_type(const std::string& str) {
    if (str == "ewrls") return StrategyType::EWRLS;
    if (str == "sigor") return StrategyType::SIGOR;
    throw std::runtime_error("Unknown strategy type: " + str);
}

/**
 * Convert strategy type to string
 */
inline std::string to_string(StrategyType strategy) {
    switch (strategy) {
        case StrategyType::EWRLS: return "EWRLS";
        case StrategyType::SIGOR: return "SIGOR";
        default: return "Unknown";
    }
}

/**
 * Get strategy display name for dashboards
 */
inline std::string get_strategy_display_name(StrategyType strategy) {
    switch (strategy) {
        case StrategyType::EWRLS:
            return "Multi-Horizon EWRLS";
        case StrategyType::SIGOR:
            return "SIGOR (Signal-OR Ensemble)";
        default:
            return "Unknown Strategy";
    }
}

/**
 * Get strategy config file path
 */
inline std::string get_strategy_config_path(StrategyType strategy) {
    switch (strategy) {
        case StrategyType::EWRLS:
            return "config/trading_params.json";
        case StrategyType::SIGOR:
            return "config/sigor_params.json";
        default:
            throw std::runtime_error("Unknown strategy type");
    }
}

} // namespace trading
