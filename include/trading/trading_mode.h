#pragma once
#include <string>

namespace trading {

/**
 * Trading Mode - Mock vs Live
 */
enum class TradingMode {
    MOCK,       // Backtest on historical data
    MOCK_LIVE,  // Replay historical data as if live (bridge-compatible)
    LIVE        // Live paper trading (future: real trading)
};

/**
 * Convert string to TradingMode
 */
inline TradingMode parse_trading_mode(const std::string& mode_str) {
    if (mode_str == "live" || mode_str == "LIVE") {
        return TradingMode::LIVE;
    }
    if (mode_str == "mock-live" || mode_str == "MOCK-LIVE" || mode_str == "mock_live") {
        return TradingMode::MOCK_LIVE;
    }
    return TradingMode::MOCK;
}

/**
 * Convert TradingMode to string
 */
inline std::string to_string(TradingMode mode) {
    switch (mode) {
        case TradingMode::MOCK: return "MOCK";
        case TradingMode::MOCK_LIVE: return "MOCK_LIVE";
        case TradingMode::LIVE: return "LIVE";
        default: return "UNKNOWN";
    }
}

/**
 * Default symbol lists for multi-symbol rotation trading
 */
namespace symbols {

/**
 * 6 core symbols (most liquid, reliable)
 */
inline const char* DEFAULT_6[] = {
    "TQQQ",  // 3x QQQ bull
    "SQQQ",  // 3x QQQ bear
    "UPRO",  // 3x SPY bull
    "SDS",   // 2x SPY bear
    "UVXY",  // VIX call
    "SVXY"   // VIX put
};

/**
 * 10 symbols for extended rotation (recommended)
 */
inline const char* DEFAULT_10[] = {
    // QQQ leveraged (3x)
    "TQQQ",  // 3x QQQ bull
    "SQQQ",  // 3x QQQ bear

    // SPY leveraged (2x - more stable than 3x)
    "SSO",   // 2x SPY bull
    "SDS",   // 2x SPY bear

    // Russell 2000 (3x)
    "TNA",   // 3x IWM bull
    "TZA",   // 3x IWM bear

    // Financial sector (3x)
    "FAS",   // 3x Finance bull
    "FAZ",   // 3x Finance bear

    // Volatility
    "UVXY",  // VIX call
    "SVXY"   // VIX put
};

/**
 * 14 symbols (maximum diversity)
 */
inline const char* DEFAULT_14[] = {
    "TQQQ", "SQQQ",  // QQQ 3x
    "SSO", "SDS",     // SPY 2x
    "TNA", "TZA",     // Russell 3x
    "FAS", "FAZ",     // Finance 3x
    "ERX", "ERY",     // Energy 3x
    "UVXY", "SVXY",   // Volatility
    "NUGT", "DUST"    // Gold miners 3x
};

} // namespace symbols
} // namespace trading
