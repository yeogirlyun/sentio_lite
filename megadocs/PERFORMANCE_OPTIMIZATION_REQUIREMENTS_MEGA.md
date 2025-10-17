# PERFORMANCE_OPTIMIZATION_REQUIREMENTS - Complete Analysis

**Generated**: 2025-10-16 22:22:00
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: /Volumes/ExternalSSD/Dev/C++/online_trader/PERFORMANCE_OPTIMIZATION_REQUIREMENTS.md
**Total Files**: 30

---

## 📋 **TABLE OF CONTENTS**

1. [CMakeLists.txt](#file-1)
2. [config/rotation_strategy.json](#file-2)
3. [include/backend/adaptive_trading_mechanism.h](#file-3)
4. [include/backend/rotation_trading_backend.h](#file-4)
5. [include/cli/rotation_trade_command.h](#file-5)
6. [include/common/data_validator.h](#file-6)
7. [include/common/time_utils.h](#file-7)
8. [include/common/types.h](#file-8)
9. [include/common/utils.h](#file-9)
10. [include/data/mock_multi_symbol_feed.h](#file-10)
11. [include/data/multi_symbol_data_manager.h](#file-11)
12. [include/features/feature_schema.h](#file-12)
13. [include/features/unified_feature_engine.h](#file-13)
14. [include/learning/online_predictor.h](#file-14)
15. [include/strategy/multi_symbol_oes_manager.h](#file-15)
16. [include/strategy/online_ensemble_strategy.h](#file-16)
17. [include/strategy/signal_output.h](#file-17)
18. [src/backend/adaptive_trading_mechanism.cpp](#file-18)
19. [src/backend/rotation_trading_backend.cpp](#file-19)
20. [src/cli/command_registry.cpp](#file-20)
21. [src/cli/rotation_trade_command.cpp](#file-21)
22. [src/common/time_utils.cpp](#file-22)
23. [src/common/utils.cpp](#file-23)
24. [src/data/mock_multi_symbol_feed.cpp](#file-24)
25. [src/data/multi_symbol_data_manager.cpp](#file-25)
26. [src/features/unified_feature_engine.cpp](#file-26)
27. [src/learning/online_predictor.cpp](#file-27)
28. [src/strategy/multi_symbol_oes_manager.cpp](#file-28)
29. [src/strategy/online_ensemble_strategy.cpp](#file-29)
30. [src/strategy/signal_output.cpp](#file-30)

---

## 📄 **FILE 1 of 30**: CMakeLists.txt

**File Information**:
- **Path**: `CMakeLists.txt`
- **Size**: 376 lines
- **Modified**: 2025-10-16 06:53:23
- **Type**: txt
- **Permissions**: -rw-r--r--

```text
cmake_minimum_required(VERSION 3.16)
project(online_trader VERSION 2.2.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Performance optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Enabling performance optimizations for Release build")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -funroll-loops -DNDEBUG")
    add_compile_definitions(NDEBUG)
    
    # Enable OpenMP for parallel processing if available
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found - enabling parallel processing")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fopenmp")
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

# Find Eigen3 for online learning (REQUIRED for this project)
find_package(Eigen3 3.3 REQUIRED)
message(STATUS "Eigen3 found - Online learning support enabled")
message(STATUS "Eigen3 version: ${EIGEN3_VERSION}")
message(STATUS "Eigen3 include: ${EIGEN3_INCLUDE_DIR}")

# Find nlohmann/json for JSON parsing
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann/json found - enabling robust JSON parsing")
    add_compile_definitions(NLOHMANN_JSON_AVAILABLE)
else()
    message(STATUS "nlohmann/json not found - using header-only fallback")
endif()

# =============================================================================
# Common Library
# =============================================================================
add_library(online_common
    src/common/types.cpp
    src/common/utils.cpp
    src/common/json_utils.cpp
    src/common/trade_event.cpp
    src/common/binary_data.cpp
    src/common/time_utils.cpp
    src/common/eod_state.cpp
    src/common/eod_guardian.cpp
    src/common/nyse_calendar.cpp
    src/core/data_io.cpp
    src/core/data_manager.cpp
    # Multi-symbol data management
    src/data/multi_symbol_data_manager.cpp
    src/data/mock_multi_symbol_feed.cpp
    src/data/alpaca_multi_symbol_feed.cpp
    # Rotation trading utilities
    src/common/data_validator.cpp
    src/strategy/signal_aggregator.cpp
    src/backend/trading_monitor.cpp
)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_common PRIVATE nlohmann_json::nlohmann_json)
endif()

# =============================================================================
# Strategy Library (Base Framework for Online Learning)
# =============================================================================
set(STRATEGY_SOURCES
    src/strategy/istrategy.cpp
    src/strategy/ml_strategy_base.cpp
    src/strategy/online_strategy_base.cpp
    src/strategy/strategy_component.cpp
    src/strategy/signal_output.cpp
    src/strategy/trading_state.cpp
    src/strategy/online_ensemble_strategy.cpp
    src/strategy/market_regime_detector.cpp
    src/strategy/regime_parameter_manager.cpp
    # Multi-symbol strategy management
    src/strategy/multi_symbol_oes_manager.cpp
)

# Add unified feature engine for online learning
list(APPEND STRATEGY_SOURCES src/features/unified_feature_engine.cpp)

add_library(online_strategy ${STRATEGY_SOURCES})
target_link_libraries(online_strategy PRIVATE online_common)
target_link_libraries(online_strategy PUBLIC Eigen3::Eigen)
target_include_directories(online_strategy PUBLIC
    ${EIGEN3_INCLUDE_DIR}
)

# Link OpenSSL for SHA1 hashing in feature engine V2
find_package(OpenSSL REQUIRED)
target_link_libraries(online_strategy PRIVATE OpenSSL::Crypto)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_strategy PRIVATE nlohmann_json::nlohmann_json)
endif()

# Link OpenMP if available for performance optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND OpenMP_CXX_FOUND)
    target_link_libraries(online_strategy PRIVATE OpenMP::OpenMP_CXX)
endif()

# =============================================================================
# Backend Library (Ensemble PSM for Online Learning)
# =============================================================================
add_library(online_backend
    src/backend/backend_component.cpp
    src/backend/portfolio_manager.cpp
    src/backend/audit_component.cpp
    src/backend/leverage_manager.cpp
    src/backend/adaptive_portfolio_manager.cpp
    src/backend/adaptive_trading_mechanism.cpp
    src/backend/position_state_machine.cpp
    # Enhanced Dynamic PSM components
    src/backend/dynamic_hysteresis_manager.cpp
    src/backend/dynamic_allocation_manager.cpp
    src/backend/enhanced_position_state_machine.cpp
    src/backend/enhanced_backend_component.cpp
    # Ensemble PSM for online learning (KEY COMPONENT)
    src/backend/ensemble_position_state_machine.cpp
    # Rotation Trading Backend (multi-symbol)
    src/backend/rotation_trading_backend.cpp
    src/strategy/rotation_position_manager.cpp
)
target_link_libraries(online_backend PRIVATE online_common Eigen3::Eigen)
target_include_directories(online_backend PUBLIC ${EIGEN3_INCLUDE_DIR})

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_backend PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(online_backend PRIVATE /opt/homebrew/include)
endif()

# =============================================================================
# Online Learning Library (Core Focus of This Project)
# =============================================================================
add_library(online_learning
    src/learning/online_predictor.cpp
)
target_link_libraries(online_learning PUBLIC 
    online_common 
    online_strategy
    Eigen3::Eigen
)
target_include_directories(online_learning PUBLIC
    ${EIGEN3_INCLUDE_DIR}
)
message(STATUS "Created online_learning library with Eigen3 support")

# =============================================================================
# Testing Framework
# =============================================================================
add_library(online_testing_framework STATIC
    # Core Testing Framework
    src/testing/test_framework.cpp
    src/testing/test_result.cpp
    src/testing/enhanced_test_framework.cpp

    # Validation
    src/validation/strategy_validator.cpp
    src/validation/validation_result.cpp
    src/validation/walk_forward_validator.cpp
    src/validation/bar_id_validator.cpp

    # Analysis
    src/analysis/performance_metrics.cpp
    src/analysis/performance_analyzer.cpp
    src/analysis/temp_file_manager.cpp
    src/analysis/statistical_tests.cpp
    src/analysis/enhanced_performance_analyzer.cpp
)

target_include_directories(online_testing_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(online_testing_framework
    PUBLIC
        online_strategy      # Strategy implementation library
        online_backend       # Backend components
    PRIVATE
        online_common        # Common utilities (only needed internally)
)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_testing_framework PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(online_testing_framework PRIVATE /opt/homebrew/include)
endif()

# =============================================================================
# Live Trading Library (Alpaca + Polygon WebSocket Integration)
# =============================================================================
find_package(CURL REQUIRED)

# Live trading library (Alpaca REST API + Python WebSocket bridge via FIFO)
# Note: Uses Python bridge for real-time data (tools/alpaca_websocket_bridge.py)
add_library(online_live
    src/live/alpaca_client.cpp
    src/live/polygon_websocket_fifo.cpp
    # src/live/alpaca_rest_bar_feed.cpp  # DISABLED: incomplete implementation
    src/live/position_book.cpp
    src/live/state_persistence.cpp
    # Mock trading infrastructure
    src/live/mock_broker.cpp
    src/live/mock_bar_feed_replay.cpp
    src/live/mock_session_state.cpp
    src/live/alpaca_client_adapter.cpp
    src/live/polygon_client_adapter.cpp
    src/live/mock_config.cpp
)
target_link_libraries(online_live PRIVATE
    online_common
    CURL::libcurl
    OpenSSL::Crypto
)
target_include_directories(online_live PRIVATE /opt/homebrew/include)
if(nlohmann_json_FOUND)
    target_link_libraries(online_live PRIVATE nlohmann_json::nlohmann_json)
endif()
message(STATUS "Created online_live library for live trading (Alpaca REST + Python WebSocket bridge + Mock Infrastructure)")

# =============================================================================
# CLI Executable (sentio_cli for online learning)
# =============================================================================
add_executable(sentio_cli
    src/cli/sentio_cli_main.cpp
    src/cli/command_interface.cpp
    src/cli/command_registry.cpp
    src/cli/parameter_validator.cpp
    # Online learning commands (commented out - missing XGBFeatureSet implementations)
    # src/cli/online_command.cpp
    # src/cli/online_sanity_check_command.cpp
    # src/cli/online_trade_command.cpp
    # OnlineEnsemble workflow commands
    src/cli/generate_signals_command.cpp
    src/cli/execute_trades_command.cpp
    src/cli/analyze_trades_command.cpp
    # Feature extraction (for Optuna caching)
    src/cli/extract_features_command.cpp
    # Workflow commands
    src/cli/backtest_command.cpp
    # Live trading command
    src/cli/live_trade_command.cpp
    # Rotation trading command (batch mock mode)
    src/cli/rotation_trade_command.cpp
)

# Link all required libraries
# Note: online_strategy, online_backend, and online_common are transitively included
# via online_learning and online_testing_framework, so we don't list them explicitly
target_link_libraries(sentio_cli PRIVATE
    online_learning          # brings in online_strategy + online_common
    online_testing_framework # brings in online_strategy + online_backend + online_common
    online_live             # brings in online_common
)

# Add nlohmann/json include for CLI
if(nlohmann_json_FOUND)
    target_link_libraries(sentio_cli PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(sentio_cli PRIVATE /opt/homebrew/include)
endif()

message(STATUS "Created sentio_cli executable with online learning support")

# Create standalone test executable for online learning
add_executable(test_online_trade tools/test_online_trade.cpp)
target_link_libraries(test_online_trade PRIVATE
    online_learning
    online_strategy
    online_backend
    online_common
)
message(STATUS "Created test_online_trade executable")

# Create test executable for regime detector validation
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_regime_detector.cpp")
    add_executable(test_regime_detector tests/test_regime_detector.cpp)
    target_link_libraries(test_regime_detector PRIVATE
        online_strategy
        online_common
    )
    message(STATUS "Created test_regime_detector executable")
endif()

# =============================================================================
# Utility Tools
# =============================================================================
# CSV to Binary Converter Tool
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/csv_to_binary_converter.cpp")
    add_executable(csv_to_binary_converter tools/csv_to_binary_converter.cpp)
    target_link_libraries(csv_to_binary_converter PRIVATE online_common)
    message(STATUS "Created csv_to_binary_converter tool")
endif()

# Dataset Analysis Tool
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/analyze_dataset.cpp")
    add_executable(analyze_dataset tools/analyze_dataset.cpp)
    target_link_libraries(analyze_dataset PRIVATE online_common)
    message(STATUS "Created analyze_dataset tool")
endif()

# =============================================================================
# Unit Tests (optional)
# =============================================================================
if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        enable_testing()
        
        # Framework tests
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_framework_test.cpp")
            add_executable(test_framework_tests
                tests/test_framework_test.cpp
            )
            target_link_libraries(test_framework_tests
                PRIVATE
                    online_testing_framework
                    GTest::gtest_main
            )
            add_test(NAME TestFrameworkTests COMMAND test_framework_tests)
        endif()
        
        # Dynamic PSM Tests
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_dynamic_hysteresis.cpp")
            add_executable(test_dynamic_hysteresis
                tests/test_dynamic_hysteresis.cpp
            )
            target_link_libraries(test_dynamic_hysteresis
                PRIVATE
                    online_backend
                    online_strategy
                    online_common
                    GTest::gtest_main
            )
            add_test(NAME DynamicHysteresisTests COMMAND test_dynamic_hysteresis)
        endif()
        
        message(STATUS "Testing framework enabled with GTest")
    else()
        message(STATUS "GTest not found - skipping testing targets")
    endif()
endif()

# =============================================================================
# Installation
# =============================================================================
install(TARGETS online_testing_framework online_learning online_strategy online_backend online_common
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

message(STATUS "========================================")
message(STATUS "Online Trader Configuration Summary:")
message(STATUS "  - Eigen3: ${EIGEN3_VERSION}")
message(STATUS "  - Online Learning: ENABLED")
message(STATUS "  - Ensemble PSM: ENABLED")
message(STATUS "  - Strategy Framework: ENABLED")
message(STATUS "  - Testing Framework: ENABLED")
message(STATUS "  - Mock/Live Trading: ENABLED")
message(STATUS "========================================")

```

## 📄 **FILE 2 of 30**: config/rotation_strategy.json

**File Information**:
- **Path**: `config/rotation_strategy.json`
- **Size**: 82 lines
- **Modified**: 2025-10-16 21:42:33
- **Type**: json
- **Permissions**: -rw-r--r--

```text
{
  "name": "Multi-Symbol Rotation Strategy v2.0",
  "description": "6-symbol leveraged ETF rotation with OnlineEnsemble learning + VIX exposure",
  "version": "2.0.1",

  "symbols": {
    "active": [
      "ERX", "ERY", "FAS", "FAZ", "SDS", "SSO", "SQQQ", "SVIX", "TNA", "TQQQ", "TZA", "UVXY",
      "AAPL", "MSFT", "AMZN", "TSLA", "NVDA", "META", "BRK.B", "GOOGL"
    ],
    "leverage_boosts": {
      "ERX": 1.5,
      "ERY": 1.5,
      "FAS": 1.5,
      "FAZ": 1.5,
      "SDS": 1.4,
      "SSO": 1.3,
      "SQQQ": 1.5,
      "SVIX": 1.3,
      "TNA": 1.5,
      "TQQQ": 1.5,
      "TZA": 1.5,
      "UVXY": 1.6,
      "AAPL": 1.0,
      "MSFT": 1.0,
      "AMZN": 1.0,
      "TSLA": 1.2,
      "NVDA": 1.1,
      "META": 1.0,
      "BRK.B": 0.8,
      "GOOGL": 1.0
    }
  },

  "oes_config": {
    "ewrls_lambda": 0.98,
    "initial_variance": 10.0,
    "regularization": 0.1,
    "warmup_samples": 500,

    "prediction_horizons": [1, 5, 10],
    "horizon_weights": [0.3, 0.5, 0.2],

    "buy_threshold": 0.53,
    "sell_threshold": 0.47,
    "neutral_zone": 0.06,

    "enable_bb_amplification": true,
    "bb_period": 20,
    "bb_std_dev": 2.0,
    "bb_proximity_threshold": 0.30,
    "bb_amplification_factor": 0.10
  },

  "signal_aggregator_config": {
    "min_probability": 0.51,
    "min_confidence": 0.01,
    "min_strength": 0.005,

    "filter_stale_signals": true,
    "max_staleness_seconds": 120.0
  },

  "rotation_manager_config": {
    "max_positions": 3,
    "min_strength_to_enter": 0.50,
    "rotation_strength_delta": 0.10,

    "profit_target_pct": 0.03,
    "stop_loss_pct": 0.015,

    "eod_liquidation": true,
    "eod_exit_time_minutes": 388
  },

  "notes": {
    "eod_liquidation": "All positions closed at 3:58 PM ET - eliminates overnight decay risk",
    "leverage_boost": "Prioritizes leveraged ETFs due to higher profit potential with EOD exit",
    "rotation_logic": "Capital flows to strongest signals - simpler than PSM",
    "independence": "Each symbol learns independently - no cross-contamination"
  }
}

```

## 📄 **FILE 3 of 30**: include/backend/adaptive_trading_mechanism.h

**File Information**:
- **Path**: `include/backend/adaptive_trading_mechanism.h`
- **Size**: 460 lines
- **Modified**: 2025-10-16 06:14:33
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <cmath>
#include <random>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "common/types.h"
#include "strategy/signal_output.h"
#include "strategy/market_regime_detector.h"

// Forward declarations to avoid circular dependencies
namespace sentio {
    class BackendComponent;
}

namespace sentio {

// ===================================================================
// THRESHOLD PAIR STRUCTURE
// ===================================================================

/**
 * @brief Represents a pair of buy and sell thresholds for trading decisions
 * 
 * The ThresholdPair encapsulates the core decision boundaries for the adaptive
 * trading system. Buy threshold determines when signals trigger buy orders,
 * sell threshold determines sell orders, with a neutral zone between them.
 */
struct ThresholdPair {
    double buy_threshold = 0.6;   // Probability threshold for buy orders
    double sell_threshold = 0.4;  // Probability threshold for sell orders
    
    ThresholdPair() = default;
    ThresholdPair(double buy, double sell) : buy_threshold(buy), sell_threshold(sell) {}
    
    /**
     * @brief Validates that thresholds are within acceptable bounds
     * @return true if thresholds are valid, false otherwise
     */
    bool is_valid() const {
        return buy_threshold > sell_threshold + 0.05 && // Min 5% gap
               buy_threshold >= 0.51 && buy_threshold <= 0.90 &&
               sell_threshold >= 0.10 && sell_threshold <= 0.49;
    }
    
    /**
     * @brief Gets the size of the neutral zone between thresholds
     * @return Size of neutral zone (buy_threshold - sell_threshold)
     */
    double get_neutral_zone_size() const {
        return buy_threshold - sell_threshold;
    }
};

// ===================================================================
// MARKET STATE AND REGIME DETECTION
// ===================================================================

/**
 * @brief Comprehensive market state information for adaptive decision making
 * Note: MarketRegime and MarketRegimeDetector are now defined in strategy/market_regime_detector.h
 */
struct MarketState {
    double current_price = 0.0;
    double volatility = 0.0;          // 20-day volatility measure
    double trend_strength = 0.0;      // -1 (strong bear) to +1 (strong bull)
    double volume_ratio = 1.0;        // Current volume / average volume
    MarketRegime regime = MarketRegime::CHOPPY;  // Use default from market_regime_detector.h

    // Signal distribution statistics
    double avg_signal_strength = 0.5;
    double signal_volatility = 0.1;

    // Portfolio state
    int open_positions = 0;
    double cash_utilization = 0.0;    // 0.0 to 1.0
};

// ===================================================================
// PERFORMANCE TRACKING AND EVALUATION
// ===================================================================

/**
 * @brief Represents the outcome of a completed trade for learning feedback
 */
struct TradeOutcome {
    // Store essential trade information instead of full TradeOrder to avoid circular dependency
    std::string symbol;
    TradeAction action = TradeAction::HOLD;
    double quantity = 0.0;
    double price = 0.0;
    double trade_value = 0.0;
    double fees = 0.0;
    double actual_pnl = 0.0;
    double pnl_percentage = 0.0;
    bool was_profitable = false;
    int bars_to_profit = 0;
    double max_adverse_move = 0.0;
    double sharpe_contribution = 0.0;
    std::chrono::system_clock::time_point outcome_timestamp;
};

/**
 * @brief Comprehensive performance metrics for adaptive learning evaluation
 */
struct PerformanceMetrics {
    double win_rate = 0.0;              // Percentage of profitable trades
    double profit_factor = 1.0;         // Gross profit / Gross loss
    double sharpe_ratio = 0.0;          // Risk-adjusted return
    double max_drawdown = 0.0;          // Maximum peak-to-trough decline
    double trade_frequency = 0.0;       // Trades per day
    double capital_efficiency = 0.0;    // Return on deployed capital
    double opportunity_cost = 0.0;      // Estimated missed profits
    std::vector<double> returns;        // Historical returns
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double gross_profit = 0.0;
    double gross_loss = 0.0;
};

/**
 * @brief Evaluates trading performance and generates learning signals
 * 
 * The PerformanceEvaluator tracks trade outcomes, calculates comprehensive
 * performance metrics, and generates reward signals for the learning algorithms.
 * It maintains rolling windows of performance data for adaptive optimization.
 */
class PerformanceEvaluator {
private:
    std::vector<TradeOutcome> trade_history_;
    std::vector<double> portfolio_values_;
    const size_t MAX_HISTORY = 1000;
    const size_t PERFORMANCE_WINDOW = 100;
    
public:
    /**
     * @brief Adds a completed trade outcome for performance tracking
     * @param outcome TradeOutcome with P&L and timing information
     */
    void add_trade_outcome(const TradeOutcome& outcome);
    
    /**
     * @brief Adds portfolio value snapshot for drawdown calculation
     * @param value Current total portfolio value
     */
    void add_portfolio_value(double value);
    
    /**
     * @brief Calculates comprehensive performance metrics from recent trades
     * @return PerformanceMetrics with win rate, Sharpe ratio, drawdown, etc.
     */
    PerformanceMetrics calculate_performance_metrics();
    
    /**
     * @brief Calculates reward signal for learning algorithms
     * @param metrics Current performance metrics
     * @return Reward value for reinforcement learning
     */
    double calculate_reward_signal(const PerformanceMetrics& metrics);
    
private:
    double calculate_sharpe_ratio(const std::vector<double>& returns);
    double calculate_max_drawdown();
    double calculate_capital_efficiency();
};

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER
// ===================================================================

/**
 * @brief State-action pair for Q-learning lookup table
 */
struct StateActionPair {
    int state_hash;
    int action_index;
    
    bool operator<(const StateActionPair& other) const {
        return std::tie(state_hash, action_index) < std::tie(other.state_hash, other.action_index);
    }
};

/**
 * @brief Available actions for threshold adjustment in Q-learning
 */
enum class ThresholdAction {
    INCREASE_BUY_SMALL,      // +0.01
    INCREASE_BUY_MEDIUM,     // +0.03
    DECREASE_BUY_SMALL,      // -0.01
    DECREASE_BUY_MEDIUM,     // -0.03
    INCREASE_SELL_SMALL,     // +0.01
    INCREASE_SELL_MEDIUM,    // +0.03
    DECREASE_SELL_SMALL,     // -0.01
    DECREASE_SELL_MEDIUM,    // -0.03
    MAINTAIN_THRESHOLDS,     // No change
    COUNT
};

/**
 * @brief Q-Learning based threshold optimizer for adaptive trading
 * 
 * Implements reinforcement learning to find optimal buy/sell thresholds.
 * Uses epsilon-greedy exploration and Q-value updates to learn from
 * trading outcomes and maximize long-term performance.
 */
class QLearningThresholdOptimizer {
private:
    std::map<StateActionPair, double> q_table_;
    std::map<int, int> state_visit_count_;
    
    // Hyperparameters
    double learning_rate_ = 0.1;
    double discount_factor_ = 0.95;
    double exploration_rate_ = 0.1;
    double exploration_decay_ = 0.995;
    double min_exploration_ = 0.01;
    
    // State discretization
    const int THRESHOLD_BINS = 20;
    const int PERFORMANCE_BINS = 10;
    
    std::mt19937 rng_;
    
public:
    QLearningThresholdOptimizer();
    
    /**
     * @brief Selects next action using epsilon-greedy policy
     * @param state Current market state
     * @param current_thresholds Current threshold values
     * @param performance Recent performance metrics
     * @return Selected threshold action
     */
    ThresholdAction select_action(const MarketState& state, 
                                 const ThresholdPair& current_thresholds,
                                 const PerformanceMetrics& performance);
    
    /**
     * @brief Updates Q-value based on observed reward
     * @param prev_state Previous market state
     * @param prev_thresholds Previous thresholds
     * @param prev_performance Previous performance
     * @param action Action taken
     * @param reward Observed reward
     * @param new_state New market state
     * @param new_thresholds New thresholds
     * @param new_performance New performance
     */
    void update_q_value(const MarketState& prev_state,
                       const ThresholdPair& prev_thresholds,
                       const PerformanceMetrics& prev_performance,
                       ThresholdAction action,
                       double reward,
                       const MarketState& new_state,
                       const ThresholdPair& new_thresholds,
                       const PerformanceMetrics& new_performance);
    
    /**
     * @brief Applies selected action to current thresholds
     * @param current_thresholds Current threshold values
     * @param action Action to apply
     * @return New threshold values after action
     */
    ThresholdPair apply_action(const ThresholdPair& current_thresholds, ThresholdAction action);
    
    /**
     * @brief Gets current learning progress (1.0 - exploration_rate)
     * @return Learning progress from 0.0 to 1.0
     */
    double get_learning_progress() const;
    
private:
    int discretize_state(const MarketState& state, 
                        const ThresholdPair& thresholds,
                        const PerformanceMetrics& performance);
    double get_q_value(const StateActionPair& sa_pair);
    double get_max_q_value(int state_hash);
    ThresholdAction get_best_action(int state_hash);
};

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER
// ===================================================================

/**
 * @brief Represents a bandit arm (threshold combination) with statistics
 */
struct BanditArm {
    ThresholdPair thresholds;
    double estimated_reward = 0.0;
    int pull_count = 0;
    double confidence_bound = 0.0;
    
    BanditArm(const ThresholdPair& t) : thresholds(t) {}
};

/**
 * @brief Multi-Armed Bandit optimizer for threshold selection
 * 
 * Implements UCB1 algorithm to balance exploration and exploitation
 * across different threshold combinations. Maintains confidence bounds
 * for each arm and selects based on upper confidence bounds.
 */
class MultiArmedBanditOptimizer {
private:
    std::vector<BanditArm> arms_;
    int total_pulls_ = 0;
    std::mt19937 rng_;
    
public:
    MultiArmedBanditOptimizer();
    
    /**
     * @brief Selects threshold pair using UCB1 algorithm
     * @return Selected threshold pair
     */
    ThresholdPair select_thresholds();
    
    /**
     * @brief Updates reward for selected threshold pair
     * @param thresholds Threshold pair that was used
     * @param reward Observed reward
     */
    void update_reward(const ThresholdPair& thresholds, double reward);
    
private:
    void initialize_arms();
    void update_confidence_bounds();
};

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER - Main Orchestrator
// ===================================================================

/**
 * @brief Learning algorithm selection for adaptive threshold optimization
 */
enum class LearningAlgorithm {
    Q_LEARNING,           // Reinforcement learning approach
    MULTI_ARMED_BANDIT,   // UCB1 bandit algorithm
    ENSEMBLE              // Combination of multiple algorithms
};

/**
 * @brief Configuration parameters for adaptive threshold system
 */
struct AdaptiveConfig {
    LearningAlgorithm algorithm = LearningAlgorithm::Q_LEARNING;
    double learning_rate = 0.1;
    double exploration_rate = 0.1;
    int performance_window = 50;
    int feedback_delay = 5;
    double max_drawdown_limit = 0.05;
    bool enable_regime_adaptation = true;
    bool conservative_mode = false;
};

/**
 * @brief Main orchestrator for adaptive threshold management
 * 
 * The AdaptiveThresholdManager coordinates all components of the adaptive
 * trading system. It manages learning algorithms, performance evaluation,
 * market regime detection, and provides the main interface for getting
 * optimal thresholds and processing trade outcomes.
 */
class AdaptiveThresholdManager {
private:
    // Current state
    ThresholdPair current_thresholds_;
    MarketState current_market_state_;
    PerformanceMetrics current_performance_;
    
    // Learning components
    std::unique_ptr<QLearningThresholdOptimizer> q_learner_;
    std::unique_ptr<MultiArmedBanditOptimizer> bandit_optimizer_;
    std::unique_ptr<MarketRegimeDetector> regime_detector_;
    std::unique_ptr<PerformanceEvaluator> performance_evaluator_;
    
    // Configuration
    AdaptiveConfig config_;
    
    // State tracking
    std::queue<std::pair<TradeOutcome, std::chrono::system_clock::time_point>> pending_trades_;
    std::vector<Bar> recent_bars_;
    bool learning_enabled_ = true;
    bool circuit_breaker_active_ = false;
    
    // Regime-specific thresholds
    std::map<MarketRegime, ThresholdPair> regime_thresholds_;
    
public:
    /**
     * @brief Constructs adaptive threshold manager with configuration
     * @param config Configuration parameters for the adaptive system
     */
    AdaptiveThresholdManager(const AdaptiveConfig& config = AdaptiveConfig());
    
    /**
     * @brief Gets current optimal thresholds for given market conditions
     * @param signal Current signal output
     * @param bar Current market data bar
     * @return Optimal threshold pair for current conditions
     */
    ThresholdPair get_current_thresholds(const SignalOutput& signal, const Bar& bar);
    
    /**
     * @brief Processes trade outcome for learning feedback
     * @param symbol Trade symbol
     * @param action Trade action (BUY/SELL)
     * @param quantity Trade quantity
     * @param price Trade price
     * @param trade_value Trade value
     * @param fees Trade fees
     * @param actual_pnl Actual profit/loss from trade
     * @param pnl_percentage P&L as percentage of trade value
     * @param was_profitable Whether trade was profitable
     */
    void process_trade_outcome(const std::string& symbol, TradeAction action, 
                              double quantity, double price, double trade_value, double fees,
                              double actual_pnl, double pnl_percentage, bool was_profitable);
    
    /**
     * @brief Updates portfolio value for performance tracking
     * @param value Current total portfolio value
     */
    void update_portfolio_value(double value);
    
    // Control methods
    void enable_learning(bool enabled) { learning_enabled_ = enabled; }
    void reset_circuit_breaker() { circuit_breaker_active_ = false; }
    bool is_circuit_breaker_active() const { return circuit_breaker_active_; }
    
    // Analytics methods
    PerformanceMetrics get_current_performance() const { return current_performance_; }
    MarketState get_current_market_state() const { return current_market_state_; }
    double get_learning_progress() const;
    
    /**
     * @brief Generates comprehensive performance report
     * @return Formatted string with performance metrics and insights
     */
    std::string generate_performance_report() const;
    
private:
    void initialize_regime_thresholds();
    void update_performance_and_learn();
    ThresholdPair get_regime_adapted_thresholds();
    ThresholdPair get_conservative_thresholds();
    void check_circuit_breaker();
};

} // namespace sentio

```

## 📄 **FILE 4 of 30**: include/backend/rotation_trading_backend.h

**File Information**:
- **Path**: `include/backend/rotation_trading_backend.h`
- **Size**: 377 lines
- **Modified**: 2025-10-16 09:45:21
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/multi_symbol_oes_manager.h"
#include "strategy/signal_aggregator.h"
#include "strategy/rotation_position_manager.h"
#include "data/multi_symbol_data_manager.h"
#include "live/alpaca_client.hpp"
#include "common/types.h"
#include "common/data_validator.h"
#include "backend/trading_monitor.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <fstream>

namespace sentio {

/**
 * @brief Complete trading backend for multi-symbol rotation strategy
 *
 * This backend integrates all Phase 1 and Phase 2 components into a
 * unified trading system:
 *
 * Phase 1 (Data):
 * - MultiSymbolDataManager (async data handling)
 * - IBarFeed (live or mock data source)
 *
 * Phase 2 (Strategy):
 * - MultiSymbolOESManager (6 independent learners)
 * - SignalAggregator (rank by strength)
 * - RotationPositionManager (hold top N, rotate)
 *
 * Integration:
 * - Broker interface (Alpaca for live, mock for testing)
 * - Performance tracking (MRD, Sharpe, win rate)
 * - Trade logging (signals, decisions, executions)
 * - EOD management (liquidate at 3:58 PM ET)
 *
 * Usage:
 *   RotationTradingBackend backend(config);
 *   backend.warmup(historical_data);
 *   backend.start_trading();
 *
 *   // Each bar:
 *   backend.on_bar();
 *
 *   backend.stop_trading();
 */
class RotationTradingBackend {
public:
    struct Config {
        // Symbols to trade
        std::vector<std::string> symbols = {
            "TQQQ", "SQQQ", "UPRO", "SDS", "UVXY", "SVIX"
        };

        // Component configurations
        OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;
        SignalAggregator::Config aggregator_config;
        RotationPositionManager::Config rotation_config;
        data::MultiSymbolDataManager::Config data_config;

        // Trading parameters
        double starting_capital = 100000.0;
        bool enable_trading = true;        // If false, only log signals
        bool log_all_signals = true;       // Log all signals (not just trades)
        bool log_all_decisions = true;     // Log all position decisions

        // Output paths
        std::string signal_log_path = "logs/live_trading/signals.jsonl";
        std::string decision_log_path = "logs/live_trading/decisions.jsonl";
        std::string trade_log_path = "logs/live_trading/trades.jsonl";
        std::string position_log_path = "logs/live_trading/positions.jsonl";

        // Performance tracking
        bool enable_performance_tracking = true;
        int performance_window = 200;      // Bars for rolling metrics

        // Broker (for live trading)
        std::string alpaca_api_key = "";
        std::string alpaca_secret_key = "";
        bool paper_trading = true;
    };

    /**
     * @brief Trading session statistics
     */
    struct SessionStats {
        int bars_processed = 0;
        int signals_generated = 0;
        int trades_executed = 0;
        int positions_opened = 0;
        int positions_closed = 0;
        int rotations = 0;

        double total_pnl = 0.0;
        double total_pnl_pct = 0.0;
        double current_equity = 0.0;
        double max_equity = 0.0;
        double min_equity = 0.0;
        double max_drawdown = 0.0;

        double win_rate = 0.0;
        double avg_win_pct = 0.0;
        double avg_loss_pct = 0.0;
        double sharpe_ratio = 0.0;
        double mrd = 0.0;  // Mean Return per Day

        std::chrono::system_clock::time_point session_start;
        std::chrono::system_clock::time_point session_end;
    };

    /**
     * @brief Construct backend
     *
     * @param config Configuration
     * @param data_mgr Data manager (optional, will create if not provided)
     * @param broker Broker client (optional, for live trading)
     */
    explicit RotationTradingBackend(
        const Config& config,
        std::shared_ptr<data::MultiSymbolDataManager> data_mgr = nullptr,
        std::shared_ptr<AlpacaClient> broker = nullptr
    );

    ~RotationTradingBackend();

    // === Trading Session Management ===

    /**
     * @brief Warmup strategy with historical data
     *
     * @param symbol_bars Map of symbol → historical bars
     * @return true if warmup successful
     */
    bool warmup(const std::map<std::string, std::vector<Bar>>& symbol_bars);

    /**
     * @brief Start trading session
     *
     * Opens log files, initializes session stats.
     *
     * @return true if started successfully
     */
    bool start_trading();

    /**
     * @brief Stop trading session
     *
     * Closes all positions, finalizes logs, prints summary.
     */
    void stop_trading();

    /**
     * @brief Process new bar (main trading loop)
     *
     * This is the core method called each minute:
     * 1. Update data manager
     * 2. Generate signals (6 symbols)
     * 3. Rank signals by strength
     * 4. Make position decisions
     * 5. Execute trades
     * 6. Update learning
     * 7. Log everything
     *
     * @return true if processing successful
     */
    bool on_bar();

    /**
     * @brief Check if EOD time reached
     *
     * @param current_time_minutes Minutes since market open (9:30 AM ET)
     * @return true if at or past EOD exit time
     */
    bool is_eod(int current_time_minutes) const;

    /**
     * @brief Liquidate all positions (EOD or emergency)
     *
     * @param reason Reason for liquidation
     * @return true if liquidation successful
     */
    bool liquidate_all_positions(const std::string& reason = "EOD");

    // === State Access ===

    /**
     * @brief Check if backend is ready to trade
     *
     * @return true if all components warmed up
     */
    bool is_ready() const;

    /**
     * @brief Get current session statistics
     *
     * @return Session stats
     */
    const SessionStats& get_session_stats() const { return session_stats_; }

    /**
     * @brief Get current equity
     *
     * @return Current equity (cash + unrealized P&L)
     */
    double get_current_equity() const;

    /**
     * @brief Get current positions
     *
     * @return Map of symbol → position
     */
    const std::map<std::string, RotationPositionManager::Position>& get_positions() const {
        return rotation_manager_->get_positions();
    }

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration
     */
    void update_config(const Config& new_config);

private:
    /**
     * @brief Generate all signals
     *
     * @return Map of symbol → signal
     */
    std::map<std::string, SignalOutput> generate_signals();

    /**
     * @brief Rank signals by strength
     *
     * @param signals Map of symbol → signal
     * @return Ranked signals (strongest first)
     */
    std::vector<SignalAggregator::RankedSignal> rank_signals(
        const std::map<std::string, SignalOutput>& signals
    );

    /**
     * @brief Make position decisions
     *
     * @param ranked_signals Ranked signals
     * @return Position decisions
     */
    std::vector<RotationPositionManager::PositionDecision> make_decisions(
        const std::vector<SignalAggregator::RankedSignal>& ranked_signals
    );

    /**
     * @brief Execute position decision
     *
     * @param decision Position decision
     * @return true if execution successful
     */
    bool execute_decision(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Get execution price for symbol
     *
     * @param symbol Symbol ticker
     * @param side BUY or SELL
     * @return Execution price
     */
    double get_execution_price(const std::string& symbol, const std::string& side);

    /**
     * @brief Calculate position size
     *
     * @param decision Position decision
     * @return Number of shares
     */
    int calculate_position_size(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Update learning with realized P&L
     */
    void update_learning();

    /**
     * @brief Log signal
     *
     * @param symbol Symbol
     * @param signal Signal output
     */
    void log_signal(const std::string& symbol, const SignalOutput& signal);

    /**
     * @brief Log position decision
     *
     * @param decision Position decision
     */
    void log_decision(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Log trade execution
     *
     * @param decision Position decision
     * @param execution_price Price
     * @param shares Shares traded
     * @param realized_pnl Realized P&L for EXIT trades (optional)
     * @param realized_pnl_pct Realized P&L % for EXIT trades (optional)
     */
    void log_trade(
        const RotationPositionManager::PositionDecision& decision,
        double execution_price,
        int shares,
        double realized_pnl = std::numeric_limits<double>::quiet_NaN(),
        double realized_pnl_pct = std::numeric_limits<double>::quiet_NaN()
    );

    /**
     * @brief Log current positions
     */
    void log_positions();

    /**
     * @brief Update session statistics
     */
    void update_session_stats();

    /**
     * @brief Calculate current time in minutes since market open
     *
     * @return Minutes since 9:30 AM ET
     */
    int get_current_time_minutes() const;

    Config config_;

    // Core components
    std::shared_ptr<data::MultiSymbolDataManager> data_manager_;
    std::unique_ptr<MultiSymbolOESManager> oes_manager_;
    std::unique_ptr<SignalAggregator> signal_aggregator_;
    std::unique_ptr<RotationPositionManager> rotation_manager_;

    // Broker (for live trading)
    std::shared_ptr<AlpacaClient> broker_;

    // Data quality and monitoring
    DataValidator data_validator_;
    TradingMonitor trading_monitor_;

    // State
    bool trading_active_{false};
    bool is_warmup_{true};  // True during warmup, false during actual trading
    bool circuit_breaker_triggered_{false};  // CRITICAL FIX: Circuit breaker to stop trading after large losses
    double current_cash_;
    double allocated_capital_{0.0};  // Track capital in open positions for validation
    std::map<std::string, double> position_entry_costs_;  // CRITICAL FIX: Track entry cost per position for accurate exit accounting
    std::map<std::string, int> position_shares_;  // CRITICAL FIX: Track shares per position
    std::map<std::string, double> realized_pnls_;  // For learning updates

    // Per-symbol trade history for adaptive volatility adjustment
    struct TradeHistory {
        double pnl_pct;       // P&L percentage
        int64_t timestamp;    // When the trade closed
    };
    std::map<std::string, std::deque<TradeHistory>> symbol_trade_history_;  // Last 2 trades per symbol

    // Logging
    std::ofstream signal_log_;
    std::ofstream decision_log_;
    std::ofstream trade_log_;
    std::ofstream position_log_;

    // Statistics
    SessionStats session_stats_;
    std::vector<double> equity_curve_;
    std::vector<double> returns_;
};

} // namespace sentio

```

## 📄 **FILE 5 of 30**: include/cli/rotation_trade_command.h

**File Information**:
- **Path**: `include/cli/rotation_trade_command.h`
- **Size**: 199 lines
- **Modified**: 2025-10-16 20:30:05
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "cli/command_interface.h"
#include "backend/rotation_trading_backend.h"
#include "data/multi_symbol_data_manager.h"
#include "data/mock_multi_symbol_feed.h"
#include "live/alpaca_client.hpp"
#include "common/types.h"
#include <string>
#include <memory>
#include <atomic>

namespace sentio {
namespace cli {

/**
 * @brief CLI command for multi-symbol rotation trading
 *
 * Supports two modes:
 * 1. Live trading: Real-time trading with Alpaca paper/live account
 * 2. Mock trading: Backtest replay from CSV files
 *
 * Usage:
 *   ./sentio_cli rotation-trade --mode live
 *   ./sentio_cli rotation-trade --mode mock --data-dir data/equities
 */
class RotationTradeCommand : public Command {
public:
    struct Options {
        // Mode selection
        bool is_mock_mode = false;
        std::string data_dir = "data/equities";

        // Symbols to trade (loaded from config file - no hardcoding)
        std::vector<std::string> symbols;  // Will be loaded from rotation_strategy.json

        // Capital
        double starting_capital = 100000.0;

        // Warmup configuration
        int warmup_blocks = 20;  // For live mode
        int warmup_days = 4;      // Days of historical data before test_date for warmup
        std::string warmup_dir = "data/equities";

        // Date filtering (for single-day tests)
        std::string test_date;    // YYYY-MM-DD format (empty = run all data)

        // Test period (for multi-day batch testing)
        std::string start_date;   // YYYY-MM-DD format (empty = single day mode)
        std::string end_date;     // YYYY-MM-DD format (empty = single day mode)
        bool generate_dashboards = false;  // Generate HTML dashboards for each day

        // Output paths
        std::string log_dir = "logs/rotation_trading";
        std::string dashboard_output_dir;  // For batch test dashboards (default: log_dir + "/dashboards")

        // Alpaca credentials (for live mode)
        std::string alpaca_api_key;
        std::string alpaca_secret_key;
        bool paper_trading = true;

        // Configuration file
        std::string config_file = "config/rotation_strategy.json";
    };

    RotationTradeCommand();
    explicit RotationTradeCommand(const Options& options);
    ~RotationTradeCommand() override;

    // Command interface
    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "rotation-trade"; }
    std::string get_description() const override {
        return "Multi-symbol rotation trading (live or mock mode)";
    }
    void show_help() const override;

    /**
     * @brief Execute with pre-configured options
     *
     * @return 0 on success, non-zero on error
     */
    int execute_with_options();

private:
    /**
     * @brief Load configuration from JSON file
     *
     * @return Backend configuration
     */
    RotationTradingBackend::Config load_config();

    /**
     * @brief Load warmup data for all symbols
     *
     * For live mode: Loads recent historical data (warmup_blocks)
     * For mock mode: Loads from CSV files
     *
     * @return Map of symbol → bars
     */
    std::map<std::string, std::vector<Bar>> load_warmup_data();

    /**
     * @brief Execute mock trading (backtest)
     *
     * @return 0 on success, non-zero on error
     */
    int execute_mock_trading();

    /**
     * @brief Execute batch mock trading across multiple days
     *
     * Runs mock trading for each trading day in the specified range,
     * generates dashboards, and creates a summary report.
     *
     * @return 0 on success, non-zero on error
     */
    int execute_batch_mock_trading();

    /**
     * @brief Extract trading days from data within date range
     *
     * @param start_date Start date (YYYY-MM-DD)
     * @param end_date End date (YYYY-MM-DD)
     * @return Vector of trading dates
     */
    std::vector<std::string> extract_trading_days(
        const std::string& start_date,
        const std::string& end_date
    );

    /**
     * @brief Generate dashboard for a specific day's results
     *
     * @param date Trading date (YYYY-MM-DD)
     * @param output_dir Output directory for the day
     * @return 0 on success, non-zero on error
     */
    int generate_daily_dashboard(
        const std::string& date,
        const std::string& output_dir
    );

    /**
     * @brief Generate summary dashboard across all test days
     *
     * @param daily_results Results from each day
     * @param output_dir Output directory for summary
     * @return 0 on success, non-zero on error
     */
    int generate_summary_dashboard(
        const std::vector<std::map<std::string, std::string>>& daily_results,
        const std::string& output_dir
    );

    /**
     * @brief Execute live trading
     *
     * @return 0 on success, non-zero on error
     */
    int execute_live_trading();

    /**
     * @brief Setup signal handlers for graceful shutdown
     */
    void setup_signal_handlers();

    /**
     * @brief Check if EOD time reached
     *
     * @return true if at or past 3:58 PM ET
     */
    bool is_eod() const;

    /**
     * @brief Get minutes since market open (9:30 AM ET)
     *
     * @return Minutes since open
     */
    int get_minutes_since_open() const;

    /**
     * @brief Print session summary
     */
    void print_summary(const RotationTradingBackend::SessionStats& stats);

    /**
     * @brief Log system message
     */
    void log_system(const std::string& msg);

    Options options_;
    std::unique_ptr<RotationTradingBackend> backend_;
    std::shared_ptr<data::MultiSymbolDataManager> data_manager_;
    std::shared_ptr<AlpacaClient> broker_;
};

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 6 of 30**: include/common/data_validator.h

**File Information**:
- **Path**: `include/common/data_validator.h`
- **Size**: 57 lines
- **Modified**: 2025-10-16 06:27:54
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include <map>
#include <chrono>
#include <string>

namespace sentio {

/**
 * @brief Data quality validation for market data bars
 *
 * Validates incoming bar data to prevent:
 * - Price anomalies (excessive moves)
 * - Stale data
 * - Invalid OHLC relationships
 * - Zero/negative prices
 */
class DataValidator {
public:
    struct ValidationConfig {
        double max_price_move_pct = 0.10;     // 10% max move per minute
        double max_spread_pct = 0.05;         // 5% max high-low spread
        int max_staleness_seconds = 60;       // 60 seconds max staleness
        double min_volume = 100;              // Minimum volume threshold
        bool strict_mode = true;              // Fail on any violation
    };

    DataValidator() : config_(ValidationConfig{}) {}
    explicit DataValidator(ValidationConfig config)
        : config_(config) {}

    // Validate single bar
    bool validate_bar(const std::string& symbol, const Bar& bar);

    // Validate snapshot (multi-symbol)
    bool validate_snapshot(const std::map<std::string, Bar>& snapshot);

    // Get validation report
    std::string get_last_error() const { return last_error_; }

    // Reset validator state
    void reset();

private:
    ValidationConfig config_;
    std::map<std::string, Bar> prev_bars_;
    std::string last_error_;

    bool check_price_anomaly(const std::string& symbol, const Bar& bar);
    bool check_spread(const Bar& bar);
    bool check_staleness(const Bar& bar);
    bool check_volume(const Bar& bar);
    bool check_ohlc_consistency(const Bar& bar);
};

} // namespace sentio

```

## 📄 **FILE 7 of 30**: include/common/time_utils.h

**File Information**:
- **Path**: `include/common/time_utils.h`
- **Size**: 241 lines
- **Modified**: 2025-10-16 04:16:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <cstdio>

namespace sentio {

/**
 * @brief Trading session configuration with timezone support
 *
 * Handles market hours, weekends, and timezone conversions.
 * Uses system timezone API for DST-aware calculations.
 */
struct TradingSession {
    std::string timezone_name;  // IANA timezone (e.g., "America/New_York")
    int market_open_hour{9};
    int market_open_minute{30};
    int market_close_hour{16};
    int market_close_minute{0};

    TradingSession(const std::string& tz_name = "America/New_York")
        : timezone_name(tz_name) {}

    /**
     * @brief Check if given time is during regular trading hours
     * @param tp System clock time point
     * @return true if within market hours (9:30 AM - 4:00 PM ET)
     */
    bool is_regular_hours(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Check if given time is a weekday
     * @param tp System clock time point
     * @return true if Monday-Friday
     */
    bool is_weekday(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Check if given time is a trading day (weekday, not holiday)
     * @param tp System clock time point
     * @return true if trading day
     * @note Holiday calendar not yet implemented - returns weekday check only
     */
    bool is_trading_day(const std::chrono::system_clock::time_point& tp) const {
        // TODO: Add holiday calendar check
        return is_weekday(tp);
    }

    /**
     * @brief Get local time string in timezone
     * @param tp System clock time point
     * @return Formatted time string "YYYY-MM-DD HH:MM:SS TZ"
     */
    std::string to_local_string(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Convert system time to local time in configured timezone
     * @param tp System clock time point
     * @return Local time struct
     */
    std::tm to_local_time(const std::chrono::system_clock::time_point& tp) const;
};

/**
 * @brief Get current time (always uses system UTC, convert to ET via TradingSession)
 * @return System clock time point
 */
inline std::chrono::system_clock::time_point now() {
    return std::chrono::system_clock::now();
}

/**
 * @brief Format timestamp to ISO 8601 string
 * @param tp System clock time point
 * @return ISO formatted string "YYYY-MM-DDTHH:MM:SSZ"
 */
std::string to_iso_string(const std::chrono::system_clock::time_point& tp);

/**
 * @brief Centralized ET Time Manager - ALL time operations should use this
 *
 * This class ensures consistent ET timezone handling across the entire system.
 * No direct time conversions should be done elsewhere.
 */
class ETTimeManager {
public:
    ETTimeManager() : session_("America/New_York"), use_mock_time_(false) {}

    /**
     * @brief Enable mock time mode (for replay/testing)
     * @param timestamp_ms Simulated time in milliseconds
     */
    void set_mock_time(uint64_t timestamp_ms) {
        use_mock_time_ = true;
        mock_time_ = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms)
        );
    }

    /**
     * @brief Disable mock time mode (return to wall-clock time)
     */
    void disable_mock_time() {
        use_mock_time_ = false;
    }

    /**
     * @brief Get current ET time as formatted string
     * @return "YYYY-MM-DD HH:MM:SS ET"
     */
    std::string get_current_et_string() const {
        return session_.to_local_string(get_time());
    }

    /**
     * @brief Get current ET time components
     * @return struct tm in ET timezone
     */
    std::tm get_current_et_tm() const {
        return session_.to_local_time(get_time());
    }

    /**
     * @brief Get current ET date as string (YYYY-MM-DD format)
     * @return Date string in format "2025-10-09"
     */
    std::string get_current_et_date() const {
        auto et_tm = get_current_et_tm();
        char buffer[11];  // "YYYY-MM-DD\0"
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                     et_tm.tm_year + 1900,
                     et_tm.tm_mon + 1,
                     et_tm.tm_mday);
        return std::string(buffer);
    }

    /**
     * @brief Check if current time is during regular trading hours (9:30 AM - 4:00 PM ET)
     */
    bool is_regular_hours() const {
        return session_.is_regular_hours(get_time()) && session_.is_trading_day(get_time());
    }

    /**
     * @brief Check if current time is in EOD liquidation window (3:58 PM - 4:00 PM ET)
     * Uses a 2-minute window to liquidate positions before market close
     */
    bool is_eod_liquidation_window() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // EOD window: 3:58 PM - 4:00 PM ET
        if (hour == 15 && minute >= 58) return true;  // 3:58-3:59 PM
        if (hour == 16 && minute == 0) return true;   // 4:00 PM exactly

        return false;
    }

    /**
     * @brief Check if current time is mid-day optimization window (15:15 PM ET exactly)
     * Used for adaptive parameter tuning based on comprehensive data (historical + today's bars)
     */
    bool is_midday_optimization_time() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // Mid-day optimization: 15:15 PM ET (3:15pm) - during trading hours
        return (hour == 15 && minute == 15);
    }

    /**
     * @brief Check if we should liquidate positions on startup (started outside trading hours with open positions)
     */
    bool should_liquidate_on_startup(bool has_positions) const {
        if (!has_positions) return false;

        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;

        // If started after market close (after 4 PM) or before market open (before 9:30 AM),
        // and we have positions, we should liquidate
        bool after_hours = (hour >= 16) || (hour < 9) || (hour == 9 && et_tm.tm_min < 30);

        return after_hours;
    }

    /**
     * @brief Check if market has closed (>= 4:00 PM ET)
     * Used to trigger automatic shutdown after EOD liquidation
     */
    bool is_market_close_time() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // Market closes at 4:00 PM ET - shutdown at 4:00 PM or later
        return (hour >= 16);
    }

    /**
     * @brief Get minutes since midnight ET
     */
    int get_et_minutes_since_midnight() const {
        auto et_tm = get_current_et_tm();
        return et_tm.tm_hour * 60 + et_tm.tm_min;
    }

    /**
     * @brief Access to underlying TradingSession
     */
    const TradingSession& session() const { return session_; }

private:
    /**
     * @brief Get current time (mock or wall-clock)
     */
    std::chrono::system_clock::time_point get_time() const {
        return use_mock_time_ ? mock_time_ : now();
    }

    TradingSession session_;
    bool use_mock_time_;
    std::chrono::system_clock::time_point mock_time_;
};

/**
 * @brief Get Unix timestamp in microseconds
 * @param tp System clock time point
 * @return Microseconds since epoch
 */
inline uint64_t to_unix_micros(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        tp.time_since_epoch()
    ).count();
}

} // namespace sentio

```

## 📄 **FILE 8 of 30**: include/common/types.h

**File Information**:
- **Path**: `include/common/types.h`
- **Size**: 113 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

// =============================================================================
// Module: common/types.h
// Purpose: Defines core value types used across the Sentio trading platform.
//
// Overview:
// - Contains lightweight, Plain-Old-Data (POD) structures that represent
//   market bars, positions, and the overall portfolio state.
// - These types are intentionally free of behavior (no I/O, no business logic)
//   to keep the Domain layer pure and deterministic.
// - Serialization helpers (to/from JSON) are declared here and implemented in
//   the corresponding .cpp, allowing adapters to convert data at the edges.
//
// Design Notes:
// - Keep this header stable; many modules include it. Prefer additive changes.
// - Avoid heavy includes; use forward declarations elsewhere when possible.
// =============================================================================

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

namespace sentio {

// -----------------------------------------------------------------------------
// System Constants
// -----------------------------------------------------------------------------

/// Standard block size for backtesting and signal processing
/// One block represents approximately 8 hours of trading (480 minutes)
/// This constant ensures consistency across strattest, trade, and audit commands
static constexpr size_t STANDARD_BLOCK_SIZE = 480;

// -----------------------------------------------------------------------------
// Struct: Bar
// A single OHLCV market bar for a given symbol and timestamp.
// Core idea: immutable snapshot of market state at time t.
// -----------------------------------------------------------------------------
struct Bar {
    // Immutable, globally unique identifier for this bar
    // Generated from timestamp_ms and symbol at load time
    uint64_t bar_id = 0;
    int64_t timestamp_ms;   // Milliseconds since Unix epoch
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::string symbol;
    // Derived fields for traceability/debugging (filled by loader)
    uint32_t sequence_num = 0;   // Position in original dataset
    uint16_t block_num = 0;      // STANDARD_BLOCK_SIZE partition index
    std::string date_str;        // e.g. "2025-09-09" for human-readable logs
};

// -----------------------------------------------------------------------------
// Struct: Position
// A held position for a given symbol, tracking quantity and P&L components.
// Core idea: minimal position accounting without execution-side effects.
// -----------------------------------------------------------------------------
struct Position {
    std::string symbol;
    double quantity = 0.0;
    double avg_price = 0.0;
    double current_price = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
};

// -----------------------------------------------------------------------------
// Struct: PortfolioState
// A snapshot of portfolio metrics and positions at a point in time.
// Core idea: serializable state to audit and persist run-time behavior.
// -----------------------------------------------------------------------------
struct PortfolioState {
    double cash_balance = 0.0;
    double total_equity = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    std::map<std::string, Position> positions; // keyed by symbol
    int64_t timestamp_ms = 0;

    // Serialize this state to JSON (implemented in src/common/types.cpp)
    std::string to_json() const;
    // Parse a JSON string into a PortfolioState (implemented in .cpp)
    static PortfolioState from_json(const std::string& json_str);
};

// -----------------------------------------------------------------------------
// Enum: TradeAction
// The intended trade action derived from strategy/backend decision.
// -----------------------------------------------------------------------------
enum class TradeAction {
    BUY,
    SELL,
    HOLD
};

// -----------------------------------------------------------------------------
// Enum: CostModel
// Commission/fee model abstraction to support multiple broker-like schemes.
// -----------------------------------------------------------------------------
enum class CostModel {
    ZERO,
    FIXED,
    PERCENTAGE,
    ALPACA
};

} // namespace sentio

```

## 📄 **FILE 9 of 30**: include/common/utils.h

**File Information**:
- **Path**: `include/common/utils.h`
- **Size**: 205 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

// =============================================================================
// Module: common/utils.h
// Purpose: Comprehensive utility library for the Sentio Trading System
//
// Core Architecture & Recent Enhancements:
// This module provides essential utilities that support the entire trading
// system infrastructure. It has been significantly enhanced with robust
// error handling, CLI utilities, and improved JSON parsing capabilities.
//
// Key Design Principles:
// - Centralized reusable functionality to eliminate code duplication
// - Fail-fast error handling with detailed logging and validation
// - UTC timezone consistency across all time-related operations
// - Robust JSON parsing that handles complex data structures correctly
// - File organization utilities that maintain proper data structure
//
// Recent Major Improvements:
// - Added CLI argument parsing utilities (get_arg) to eliminate duplicates
// - Enhanced JSON parsing to prevent field corruption from quoted commas
// - Implemented comprehensive logging system with file rotation
// - Added robust error handling with crash-on-error philosophy
// - Improved time utilities with consistent UTC timezone handling
//
// Module Categories:
// 1. File I/O: CSV/JSONL reading/writing with format detection
// 2. Time Utilities: UTC-consistent timestamp conversion and formatting
// 3. JSON Utilities: Robust parsing that handles complex quoted strings
// 4. Hash Utilities: SHA-256 and run ID generation for data integrity
// 5. Math Utilities: Financial metrics (Sharpe ratio, drawdown analysis)
// 6. Logging Utilities: Structured logging with file rotation and levels
// 7. CLI Utilities: Command-line argument parsing with flexible formats
// =============================================================================

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <map>
#include <cstdint>
#include "types.h"

namespace sentio {
namespace utils {
// ------------------------------ Bar ID utilities ------------------------------
/// Generate a stable 64-bit bar identifier from timestamp and symbol
/// Layout: [16 bits symbol hash][48 bits timestamp_ms]
uint64_t generate_bar_id(int64_t timestamp_ms, const std::string& symbol);

/// Extract timestamp (lower 48 bits) from bar id
int64_t extract_timestamp(uint64_t bar_id);

/// Extract 16-bit symbol hash (upper bits) from bar id
uint16_t extract_symbol_hash(uint64_t bar_id);


// ----------------------------- File I/O utilities ----------------------------
/// Advanced CSV data reader with automatic format detection and symbol extraction
/// 
/// This function intelligently handles multiple CSV formats:
/// 1. QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume (symbol from filename)
/// 2. Standard format: symbol,timestamp_ms,open,high,low,close,volume
/// 
/// Key Features:
/// - Automatic format detection by analyzing header row
/// - Symbol extraction from filename for QQQ format files
/// - Timestamp conversion from seconds to milliseconds for QQQ format
/// - Robust error handling with graceful fallbacks
/// 
/// @param path Path to CSV file (supports both relative and absolute paths)
/// @return Vector of Bar structures with OHLCV data and metadata
std::vector<Bar> read_csv_data(const std::string& path);

/// High-performance binary data reader with index-based range queries
/// 
/// This function provides fast access to market data stored in binary format:
/// - Direct index-based access without loading entire dataset
/// - Support for range queries (start_index, count)
/// - Automatic fallback to CSV if binary file doesn't exist
/// - Consistent indexing across entire trading pipeline
/// 
/// @param data_path Path to binary file (or CSV as fallback)
/// @param start_index Starting index for data range (0-based)
/// @param count Number of bars to read (0 = read all from start_index)
/// @return Vector of Bar structures for the specified range
/// @throws Logs errors and returns empty vector on failure
std::vector<Bar> read_market_data_range(const std::string& data_path, 
                                       uint64_t start_index = 0, 
                                       uint64_t count = 0);

/// Get total number of bars in a market data file
/// 
/// @param data_path Path to binary or CSV file
/// @return Total number of bars, or 0 on error
uint64_t get_market_data_count(const std::string& data_path);

/// Get the most recent N bars from a market data file
/// 
/// @param data_path Path to binary or CSV file  
/// @param count Number of recent bars to retrieve
/// @return Vector of the most recent bars
std::vector<Bar> read_recent_market_data(const std::string& data_path, uint64_t count);

/// Write data in JSON Lines format for efficient streaming and processing
/// 
/// JSON Lines (JSONL) format stores one JSON object per line, making it ideal
/// for large datasets that need to be processed incrementally. This format
/// is used throughout the Sentio system for signals and trade data.
/// 
/// @param path Output file path
/// @param lines Vector of JSON strings (one per line)
/// @return true if write successful, false otherwise
bool write_jsonl(const std::string& path, const std::vector<std::string>& lines);

/// Write structured data to CSV format with proper escaping
/// 
/// @param path Output CSV file path
/// @param data 2D string matrix where first row typically contains headers
/// @return true if write successful, false otherwise
bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data);

// ------------------------------ Time utilities -------------------------------
// Parse ISO-like timestamp (YYYY-MM-DD HH:MM:SS) into milliseconds since epoch
int64_t timestamp_to_ms(const std::string& timestamp_str);

// Convert milliseconds since epoch to formatted timestamp string
std::string ms_to_timestamp(int64_t ms);


// ------------------------------ JSON utilities -------------------------------
/// Convert string map to JSON format for lightweight serialization
/// 
/// This function creates simple JSON objects from string key-value pairs.
/// It's designed for lightweight serialization of metadata and configuration.
/// 
/// @param data Map of string keys to string values
/// @return JSON string representation
std::string to_json(const std::map<std::string, std::string>& data);

/// Robust JSON parser for flat string maps with enhanced quote handling
/// 
/// This parser has been significantly enhanced to correctly handle complex
/// JSON structures that contain commas and colons within quoted strings.
/// It prevents the field corruption issues that were present in earlier versions.
/// 
/// Key Features:
/// - Proper handling of commas within quoted values
/// - Correct parsing of colons within quoted strings
/// - Robust quote escaping and state tracking
/// - Graceful error handling with empty map fallback
/// 
/// @param json_str JSON string to parse (must be flat object format)
/// @return Map of parsed key-value pairs, empty map on parse errors
std::map<std::string, std::string> from_json(const std::string& json_str);

// -------------------------------- Hash utilities -----------------------------

// Generate an 8-digit numeric run id (zero-padded). Unique enough per run.
std::string generate_run_id(const std::string& prefix);

// -------------------------------- Math utilities -----------------------------
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate = 0.0);
double calculate_max_drawdown(const std::vector<double>& equity_curve);

// -------------------------------- Logging utilities -------------------------- 
// Minimal file logger. Writes to logs/debug.log and logs/errors.log.
// Messages should be pre-sanitized (no secrets/PII).
void log_debug(const std::string& message);
void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);

// Leverage conflict detection utility (consolidates duplicate code)
bool would_instruments_conflict(const std::string& proposed, const std::string& existing);

// -------------------------------- CLI utilities ------------------------------- 
/// Flexible command-line argument parser supporting multiple formats
/// 
/// This utility function was extracted from duplicate implementations across
/// multiple CLI files to eliminate code duplication and ensure consistency.
/// It provides flexible parsing that accommodates different user preferences.
/// 
/// Supported Formats:
/// - Space-separated: --name value
/// - Equals-separated: --name=value
/// - Mixed usage within the same command line
/// 
/// Key Features:
/// - Robust argument validation (prevents parsing flags as values)
/// - Consistent behavior across all CLI tools
/// - Graceful fallback to default values
/// - No external dependencies or complex parsing libraries
/// 
/// @param argc Number of command line arguments
/// @param argv Array of command line argument strings
/// @param name The argument name to search for (including -- prefix)
/// @param def Default value returned if argument not found
/// @return The argument value if found, otherwise the default value
std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def = "");

} // namespace utils
} // namespace sentio



```

## 📄 **FILE 10 of 30**: include/data/mock_multi_symbol_feed.h

**File Information**:
- **Path**: `include/data/mock_multi_symbol_feed.h`
- **Size**: 210 lines
- **Modified**: 2025-10-15 10:36:54
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "data/bar_feed_interface.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <deque>
#include <thread>
#include <atomic>

namespace sentio {
namespace data {

/**
 * @brief Mock data feed for testing - replays historical CSV data
 *
 * Reads CSV files for multiple symbols and replays them synchronously.
 * Useful for backtesting and integration testing.
 *
 * CSV Format (per symbol):
 *   timestamp,open,high,low,close,volume
 *   1696464600000,432.15,432.89,431.98,432.56,12345678
 *
 * Usage:
 *   MockMultiSymbolFeed feed(data_manager, {
 *       {"TQQQ", "data/equities/TQQQ_RTH_NH.csv"},
 *       {"SQQQ", "data/equities/SQQQ_RTH_NH.csv"}
 *   });
 *   feed.connect();  // Load CSV files
 *   feed.start();    // Start replay in background thread
 */
class MockMultiSymbolFeed : public IBarFeed {
public:
    struct Config {
        std::map<std::string, std::string> symbol_files;  // Symbol → CSV path
        double replay_speed = 39.0;                       // Speed multiplier (0=instant)
        bool loop = false;                                // Loop replay?
        bool sync_timestamps = true;                      // Synchronize timestamps?
        std::string filter_date;                          // Filter to specific date (YYYY-MM-DD, empty = all)
    };

    /**
     * @brief Construct mock feed
     *
     * @param data_manager Data manager to feed
     * @param config Configuration
     */
    MockMultiSymbolFeed(std::shared_ptr<MultiSymbolDataManager> data_manager,
                       const Config& config);

    ~MockMultiSymbolFeed() override;

    // === IBarFeed Interface ===

    /**
     * @brief Connect to data source (loads CSV files)
     * @return true if all CSV files loaded successfully
     */
    bool connect() override;

    /**
     * @brief Disconnect from data source
     */
    void disconnect() override;

    /**
     * @brief Check if connected (data loaded)
     */
    bool is_connected() const override;

    /**
     * @brief Start feeding data (begins replay in background thread)
     * @return true if started successfully
     */
    bool start() override;

    /**
     * @brief Stop feeding data
     */
    void stop() override;

    /**
     * @brief Check if feed is active
     */
    bool is_active() const override;

    /**
     * @brief Get feed type identifier
     */
    std::string get_type() const override;

    /**
     * @brief Get symbols being fed
     */
    std::vector<std::string> get_symbols() const override;

    /**
     * @brief Set callback for bar updates
     */
    void set_bar_callback(BarCallback callback) override;

    /**
     * @brief Set callback for errors
     */
    void set_error_callback(ErrorCallback callback) override;

    /**
     * @brief Set callback for connection state changes
     */
    void set_connection_callback(ConnectionCallback callback) override;

    /**
     * @brief Get feed statistics
     */
    FeedStats get_stats() const override;

    // === Additional Mock-Specific Methods ===

    /**
     * @brief Get total bars loaded per symbol
     */
    std::map<std::string, int> get_bar_counts() const;

    /**
     * @brief Get replay progress
     */
    struct Progress {
        int bars_replayed;
        int total_bars;
        double progress_pct;
        std::string current_symbol;
        uint64_t current_timestamp;
    };

    Progress get_progress() const;

private:
    /**
     * @brief Load CSV file for a symbol
     *
     * @param symbol Symbol ticker
     * @param filepath Path to CSV file
     * @return Number of bars loaded
     */
    int load_csv(const std::string& symbol, const std::string& filepath);

    /**
     * @brief Parse CSV line into Bar
     *
     * @param line CSV line
     * @param bar Output bar
     * @return true if parsed successfully
     */
    bool parse_csv_line(const std::string& line, Bar& bar);

    /**
     * @brief Sleep for replay speed
     *
     * @param bars Number of bars to sleep for (1 = 1 minute real-time)
     */
    void sleep_for_replay(int bars = 1);

    /**
     * @brief Replay loop (runs in background thread)
     */
    void replay_loop();

    /**
     * @brief Replay single bar for all symbols
     * @return true if bars available, false if EOF
     */
    bool replay_next_bar();

    std::shared_ptr<MultiSymbolDataManager> data_manager_;
    Config config_;

    // Data storage
    struct SymbolData {
        std::deque<Bar> bars;
        size_t current_index;

        SymbolData() : current_index(0) {}
    };

    std::map<std::string, SymbolData> symbol_data_;

    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> active_{false};
    std::atomic<bool> should_stop_{false};

    // Background thread
    std::thread replay_thread_;

    // Callbacks
    BarCallback bar_callback_;
    ErrorCallback error_callback_;
    ConnectionCallback connection_callback_;

    // Replay state
    std::atomic<int> bars_replayed_{0};
    int total_bars_{0};
    std::atomic<int> errors_{0};
};

} // namespace data
} // namespace sentio

```

## 📄 **FILE 11 of 30**: include/data/multi_symbol_data_manager.h

**File Information**:
- **Path**: `include/data/multi_symbol_data_manager.h`
- **Size**: 259 lines
- **Modified**: 2025-10-14 22:52:31
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include <map>
#include <vector>
#include <deque>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

namespace sentio {
namespace data {

/**
 * @brief Snapshot of a symbol's latest data with staleness tracking
 *
 * Each symbol maintains its own update timeline. Staleness weighting
 * reduces influence of old data in signal ranking.
 */
struct SymbolSnapshot {
    Bar latest_bar;                     // Most recent bar
    uint64_t last_update_ms;            // Timestamp of last update
    double staleness_seconds;           // Age of data (seconds)
    double staleness_weight;            // Exponential decay weight: exp(-age/60)
    int forward_fill_count;             // How many times forward-filled
    bool is_valid;                      // Data is usable

    SymbolSnapshot()
        : last_update_ms(0)
        , staleness_seconds(0.0)
        , staleness_weight(1.0)
        , forward_fill_count(0)
        , is_valid(false) {}

    // Calculate staleness based on current time
    void update_staleness(uint64_t current_time_ms) {
        staleness_seconds = (current_time_ms - last_update_ms) / 1000.0;

        // Exponential decay: fresh = 1.0, 60s = 0.37, 120s = 0.14
        staleness_weight = std::exp(-staleness_seconds / 60.0);

        is_valid = (staleness_seconds < 300.0);  // Invalid after 5 minutes
    }
};

/**
 * @brief Synchronized snapshot of all symbols at a logical timestamp
 *
 * Not all symbols may be present (async data arrival). Missing symbols
 * are forward-filled from last known data.
 */
struct MultiSymbolSnapshot {
    uint64_t logical_timestamp_ms;                     // Logical time (aligned)
    std::map<std::string, SymbolSnapshot> snapshots;   // Symbol → data
    std::vector<std::string> missing_symbols;          // Symbols not updated
    int total_forward_fills;                           // Total symbols forward-filled
    bool is_complete;                                  // All symbols present
    double avg_staleness_seconds;                      // Average age across symbols

    MultiSymbolSnapshot()
        : logical_timestamp_ms(0)
        , total_forward_fills(0)
        , is_complete(false)
        , avg_staleness_seconds(0.0) {}

    // Check if snapshot is usable
    bool is_usable() const {
        // Must have at least 50% of symbols with fresh data
        int valid_count = 0;
        for (const auto& [symbol, snap] : snapshots) {
            if (snap.is_valid && snap.staleness_seconds < 120.0) {
                valid_count++;
            }
        }
        return valid_count >= (snapshots.size() / 2);
    }
};

/**
 * @brief Asynchronous multi-symbol data manager
 *
 * Core design principles:
 * 1. NON-BLOCKING: Never wait for all symbols, use latest available
 * 2. STALENESS WEIGHTING: Reduce influence of old data
 * 3. FORWARD FILL: Use last known price for missing data (max 5 fills)
 * 4. THREAD-SAFE: WebSocket updates from background thread
 *
 * Usage:
 *   auto snapshot = data_mgr->get_latest_snapshot();
 *   for (const auto& [symbol, data] : snapshot.snapshots) {
 *       double adjusted_strength = base_strength * data.staleness_weight;
 *   }
 */
class MultiSymbolDataManager {
public:
    struct Config {
        std::vector<std::string> symbols;          // Active symbols to track
        int max_forward_fills = 5;                 // Max consecutive forward fills
        double max_staleness_seconds = 300.0;      // Max age before invalid (5 min)
        int history_size = 500;                    // Bars to keep per symbol
        bool log_data_quality = true;              // Log missing/stale data
        bool backtest_mode = false;                // Disable timestamp validation for backtesting
    };

    explicit MultiSymbolDataManager(const Config& config);
    virtual ~MultiSymbolDataManager() = default;

    // === Main Interface ===

    /**
     * @brief Get latest snapshot for all symbols (non-blocking)
     *
     * Returns immediately with whatever data is available. Applies
     * staleness weighting and forward-fill for missing symbols.
     *
     * @return MultiSymbolSnapshot with latest data
     */
    MultiSymbolSnapshot get_latest_snapshot();

    /**
     * @brief Update a symbol's data (called from WebSocket/feed thread)
     *
     * Thread-safe update of symbol data. Validates and stores bar.
     *
     * @param symbol Symbol ticker
     * @param bar New bar data
     * @return true if update successful, false if validation failed
     */
    bool update_symbol(const std::string& symbol, const Bar& bar);

    /**
     * @brief Bulk update multiple symbols (for mock replay)
     *
     * @param bars Map of symbol → bar
     * @return Number of successful updates
     */
    int update_all(const std::map<std::string, Bar>& bars);

    // === History Access ===

    /**
     * @brief Get recent bars for a symbol (for volatility calculation)
     *
     * @param symbol Symbol ticker
     * @param count Number of bars to retrieve
     * @return Vector of recent bars (newest first)
     */
    std::vector<Bar> get_recent_bars(const std::string& symbol, int count) const;

    /**
     * @brief Get all history for a symbol
     *
     * @param symbol Symbol ticker
     * @return Deque of all bars (oldest first)
     */
    std::deque<Bar> get_all_bars(const std::string& symbol) const;

    // === Statistics & Monitoring ===

    /**
     * @brief Get data quality stats
     */
    struct DataQualityStats {
        std::map<std::string, int> update_counts;          // Symbol → updates
        std::map<std::string, double> avg_staleness;       // Symbol → avg age
        std::map<std::string, int> forward_fill_counts;    // Symbol → fills
        int total_updates;
        int total_forward_fills;
        int total_rejections;
        double overall_avg_staleness;
    };

    DataQualityStats get_quality_stats() const;

    /**
     * @brief Reset statistics
     */
    void reset_stats();

    /**
     * @brief Get configured symbols
     */
    const std::vector<std::string>& get_symbols() const { return config_.symbols; }

protected:
    /**
     * @brief Validate incoming bar data
     *
     * @param symbol Symbol ticker
     * @param bar Bar to validate
     * @return true if valid, false otherwise
     */
    bool validate_bar(const std::string& symbol, const Bar& bar);

    /**
     * @brief Forward-fill missing symbol from last known bar
     *
     * @param symbol Symbol ticker
     * @param logical_time Timestamp to use for forward-filled bar
     * @return SymbolSnapshot with forward-filled data
     */
    SymbolSnapshot forward_fill_symbol(const std::string& symbol,
                                       uint64_t logical_time);

private:
    struct SymbolState {
        std::deque<Bar> history;           // Bar history
        Bar latest_bar;                    // Most recent bar
        uint64_t last_update_ms;           // Last update timestamp
        int update_count;                  // Total updates received
        int forward_fill_count;            // Consecutive forward fills
        int rejection_count;               // Rejected updates
        double cumulative_staleness;       // For avg calculation

        SymbolState()
            : last_update_ms(0)
            , update_count(0)
            , forward_fill_count(0)
            , rejection_count(0)
            , cumulative_staleness(0.0) {}
    };

    Config config_;
    std::map<std::string, SymbolState> symbol_states_;
    mutable std::mutex data_mutex_;

    // Statistics
    std::atomic<int> total_updates_{0};
    std::atomic<int> total_forward_fills_{0};
    std::atomic<int> total_rejections_{0};

    // Current time (for testing - can be injected)
    std::function<uint64_t()> time_provider_;

    uint64_t get_current_time_ms() const {
        if (time_provider_) {
            return time_provider_();
        }
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

public:
    // For testing: inject time provider
    void set_time_provider(std::function<uint64_t()> provider) {
        time_provider_ = provider;
    }

private:
    // Helper functions
    std::string join_symbols() const;
    std::string join_vector(const std::vector<std::string>& vec) const;
};

} // namespace data
} // namespace sentio

```

## 📄 **FILE 12 of 30**: include/features/feature_schema.h

**File Information**:
- **Path**: `include/features/feature_schema.h`
- **Size**: 123 lines
- **Modified**: 2025-10-07 12:04:31
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace sentio {

/**
 * @brief Feature schema for reproducibility and validation
 *
 * Tracks feature names, version, and hash for model compatibility checking.
 */
struct FeatureSchema {
    std::vector<std::string> feature_names;
    int version{1};
    std::string hash;  // Hex digest of names + params

    /**
     * @brief Compute hash from feature names and version
     * @return Hex string hash (16 chars)
     */
    std::string compute_hash() const {
        std::stringstream ss;
        for (const auto& name : feature_names) {
            ss << name << "|";
        }
        ss << "v" << version;

        // Use std::hash as placeholder (use proper SHA256 in production)
        std::string s = ss.str();
        std::hash<std::string> hasher;
        size_t h = hasher(s);

        std::stringstream hex;
        hex << std::hex << std::setw(16) << std::setfill('0') << h;
        return hex.str();
    }

    /**
     * @brief Finalize schema by computing hash
     */
    void finalize() {
        hash = compute_hash();
    }

    /**
     * @brief Check if schema matches another
     * @param other Other schema to compare
     * @return true if compatible (same hash)
     */
    bool is_compatible(const FeatureSchema& other) const {
        return hash == other.hash && version == other.version;
    }
};

/**
 * @brief Feature snapshot with timestamp and schema
 */
struct FeatureSnapshot {
    uint64_t timestamp{0};
    uint64_t bar_id{0};
    std::vector<double> features;
    FeatureSchema schema;

    /**
     * @brief Check if snapshot is valid (size matches schema)
     * @return true if valid
     */
    bool is_valid() const {
        return features.size() == schema.feature_names.size();
    }
};

/**
 * @brief Replace NaN/Inf values with 0.0
 * @param features Feature vector to clean
 */
inline void nan_guard(std::vector<double>& features) {
    for (auto& f : features) {
        if (!std::isfinite(f)) {
            f = 0.0;
        }
    }
}

/**
 * @brief Clamp extreme feature values
 * @param features Feature vector to clamp
 * @param min_val Minimum allowed value
 * @param max_val Maximum allowed value
 */
inline void clamp_features(std::vector<double>& features,
                          double min_val = -1e6,
                          double max_val = 1e6) {
    for (auto& f : features) {
        f = std::clamp(f, min_val, max_val);
    }
}

/**
 * @brief Sanitize features: NaN guard + clamp
 * @param features Feature vector to sanitize
 */
inline void sanitize_features(std::vector<double>& features) {
    nan_guard(features);
    clamp_features(features);
}

/**
 * @brief Check if feature vector contains any invalid values
 * @param features Feature vector to check
 * @return true if all values are finite
 */
inline bool is_feature_vector_valid(const std::vector<double>& features) {
    return std::all_of(features.begin(), features.end(),
                      [](double f) { return std::isfinite(f); });
}

} // namespace sentio

```

## 📄 **FILE 13 of 30**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`
- **Size**: 253 lines
- **Modified**: 2025-10-16 13:09:26
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include "features/indicators.h"
#include "features/scaler.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <optional>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace sentio {
namespace features {

// =============================================================================
// Configuration for Production-Grade Unified Feature Engine
// =============================================================================

struct EngineConfig {
    // Feature toggles
    bool time = true;         // Time-of-day features (8 features)
    bool patterns = true;     // Candlestick patterns (5 features)
    bool momentum = true;
    bool volatility = true;
    bool volume = true;
    bool statistics = true;

    // Indicator periods
    int rsi14 = 14;
    int rsi21 = 21;
    int atr14 = 14;
    int bb20 = 20;
    int bb_k = 2;
    int stoch14 = 14;
    int will14 = 14;
    int macd_fast = 12;
    int macd_slow = 26;
    int macd_sig = 9;
    int roc5 = 5;
    int roc10 = 10;
    int roc20 = 20;
    int cci20 = 20;
    int don20 = 20;
    int keltner_ema = 20;
    int keltner_atr = 10;
    double keltner_mult = 2.0;

    // Moving averages
    int sma10 = 10;
    int sma20 = 20;
    int sma50 = 50;
    int ema10 = 10;
    int ema20 = 20;
    int ema50 = 50;

    // Normalization
    bool normalize = true;
    bool robust = false;
};

// =============================================================================
// Feature Schema with Hash for Model Compatibility
// =============================================================================

struct Schema {
    std::vector<std::string> names;
    std::string sha1_hash;  // Hash of (names + config) for version control
};

// =============================================================================
// Production-Grade Unified Feature Engine
//
// Key Features:
// - Stable, deterministic feature ordering (std::map, not unordered_map)
// - O(1) incremental updates using Welford's algorithm and ring buffers
// - Schema hash for model compatibility checks
// - Complete public API: update(), features_view(), names(), schema()
// - Serialization/restoration for online learning
// - Zero duplicate calculations (shared statistics cache)
// =============================================================================

class UnifiedFeatureEngine {
public:
    explicit UnifiedFeatureEngine(EngineConfig cfg = {});

    // ==========================================================================
    // Core API
    // ==========================================================================

    /**
     * Idempotent update with new bar. Returns true if state advanced.
     */
    bool update(const Bar& b);

    /**
     * Get contiguous feature vector in stable order (ready for model input).
     * Values may contain NaN until warmup complete for each feature.
     */
    const std::vector<double>& features_view() const { return feats_; }

    /**
     * Get canonical feature names in fixed, deterministic order.
     */
    const std::vector<std::string>& names() const { return schema_.names; }

    /**
     * Get schema with hash for model compatibility checks.
     */
    const Schema& schema() const { return schema_; }

    /**
     * Count of bars remaining before all features are non-NaN.
     */
    int warmup_remaining() const;

    /**
     * Get list of indicator names that are not yet ready (for debugging).
     */
    std::vector<std::string> get_unready_indicators() const;

    /**
     * Reset engine to initial state.
     */
    void reset();

    /**
     * Serialize engine state for persistence (online learning resume).
     */
    std::string serialize() const;

    /**
     * Restore engine state from serialized blob.
     */
    void restore(const std::string& blob);

    /**
     * Check if engine has processed at least one bar.
     */
    bool is_seeded() const { return seeded_; }

    /**
     * Get number of bars processed.
     */
    size_t bar_count() const { return bar_count_; }

    /**
     * Get normalization scaler (for external persistence).
     */
    const Scaler& get_scaler() const { return scaler_; }

    /**
     * Set scaler from external source (for trained models).
     */
    void set_scaler(const Scaler& s) { scaler_ = s; }

    /**
     * Get realized volatility (standard deviation of returns).
     * @param lookback Number of bars to calculate over (default 20)
     * @return Realized volatility, or 0.0 if insufficient data
     */
    double get_realized_volatility(int lookback = 20) const;

    /**
     * Get annualized volatility (realized vol * sqrt(252 * 390 minutes/day)).
     * @return Annualized volatility percentage
     */
    double get_annualized_volatility() const;

private:
    void build_schema_();
    void recompute_vector_();
    std::string compute_schema_hash_(const std::string& concatenated_names);

    EngineConfig cfg_;
    Schema schema_;

    // ==========================================================================
    // Indicators (all O(1) incremental)
    // ==========================================================================

    ind::RSI rsi14_;
    ind::RSI rsi21_;
    ind::ATR atr14_;
    ind::Boll bb20_;
    ind::Stoch stoch14_;
    ind::WilliamsR will14_;
    ind::MACD macd_;
    ind::ROC roc5_, roc10_, roc20_;
    ind::CCI cci20_;
    ind::Donchian don20_;
    ind::Keltner keltner_;
    ind::OBV obv_;
    ind::VWAP vwap_;

    // Moving averages
    roll::EMA ema10_, ema20_, ema50_;
    roll::Ring<double> sma10_ring_, sma20_ring_, sma50_ring_;

    // ==========================================================================
    // State
    // ==========================================================================

    bool seeded_ = false;
    size_t bar_count_ = 0;
    uint64_t prevTimestamp_ = 0;  // For time features
    double prevClose_ = std::numeric_limits<double>::quiet_NaN();
    double prevOpen_ = std::numeric_limits<double>::quiet_NaN();
    double prevHigh_ = std::numeric_limits<double>::quiet_NaN();
    double prevLow_ = std::numeric_limits<double>::quiet_NaN();
    double prevVolume_ = std::numeric_limits<double>::quiet_NaN();

    // For computing 1-bar return (current close vs previous close)
    double prevPrevClose_ = std::numeric_limits<double>::quiet_NaN();

    // For computing volume change ratio
    double prevPrevVolume_ = std::numeric_limits<double>::quiet_NaN();

    // Rolling returns buffer for volatility calculation (stores last 50 returns)
    std::deque<double> recent_returns_;
    static constexpr size_t MAX_RETURNS_HISTORY = 50;

    // Feature vector (stable order, contiguous for model input)
    std::vector<double> feats_;

    // Normalization
    Scaler scaler_;
    std::vector<std::vector<double>> normalization_buffer_;  // For fit()
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Compute SHA1 hash of string (for schema versioning).
 */
std::string sha1_hex(const std::string& s);

/**
 * Safe return calculation (handles NaN and division by zero).
 */
inline double safe_return(double current, double previous) {
    if (std::isnan(previous) || previous == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (current / previous) - 1.0;
}

} // namespace features
} // namespace sentio

```

## 📄 **FILE 14 of 30**: include/learning/online_predictor.h

**File Information**:
- **Path**: `include/learning/online_predictor.h`
- **Size**: 133 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <Eigen/Dense>
#include <vector>
#include <string>
#include <fstream>
#include <deque>
#include <memory>
#include <cmath>

namespace sentio {
namespace learning {

/**
 * Online learning predictor that eliminates train/inference parity issues
 * Uses Exponentially Weighted Recursive Least Squares (EWRLS)
 */
class OnlinePredictor {
public:
    struct Config {
        double lambda;
        double initial_variance;
        double regularization;
        int warmup_samples;
        bool adaptive_learning;
        double min_lambda;
        double max_lambda;
        
        Config()
            : lambda(0.995),
              initial_variance(100.0),
              regularization(0.01),
              warmup_samples(100),
              adaptive_learning(true),
              min_lambda(0.990),
              max_lambda(0.999) {}
    };
    
    struct PredictionResult {
        double predicted_return;
        double confidence;
        double volatility_estimate;
        bool is_ready;
        
        PredictionResult()
            : predicted_return(0.0),
              confidence(0.0),
              volatility_estimate(0.0),
              is_ready(false) {}
    };
    
    explicit OnlinePredictor(size_t num_features, const Config& config = Config());
    
    // Main interface - predict and optionally update
    PredictionResult predict(const std::vector<double>& features);
    void update(const std::vector<double>& features, double actual_return);
    
    // Combined predict-then-update for efficiency
    PredictionResult predict_and_update(const std::vector<double>& features, 
                                        double actual_return);
    
    // Adaptive learning rate based on recent volatility
    void adapt_learning_rate(double market_volatility);
    
    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);
    
    // Diagnostics
    double get_recent_rmse() const;
    double get_directional_accuracy() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }
    
private:
    Config config_;
    size_t num_features_;
    int samples_seen_;
    
    // EWRLS parameters
    Eigen::VectorXd theta_;      // Model parameters
    Eigen::MatrixXd P_;          // Covariance matrix
    double current_lambda_;      // Adaptive forgetting factor
    
    // Performance tracking
    std::deque<double> recent_errors_;
    std::deque<bool> recent_directions_;
    static constexpr size_t HISTORY_SIZE = 100;
    
    // Volatility estimation for adaptive learning
    std::deque<double> recent_returns_;
    double estimate_volatility() const;
    
    // Numerical stability
    void ensure_positive_definite();
    static constexpr double EPSILON = 1e-8;
};

/**
 * Ensemble of online predictors for different time horizons
 */
class MultiHorizonPredictor {
public:
    struct HorizonConfig {
        int horizon_bars;
        double weight;
        OnlinePredictor::Config predictor_config;
        
        HorizonConfig()
            : horizon_bars(1),
              weight(1.0),
              predictor_config() {}
    };
    
    explicit MultiHorizonPredictor(size_t num_features);
    
    // Add predictors for different horizons
    void add_horizon(int bars, double weight = 1.0);
    
    // Ensemble prediction
    OnlinePredictor::PredictionResult predict(const std::vector<double>& features);
    
    // Update all predictors
    void update(int bars_ago, const std::vector<double>& features, double actual_return);
    
private:
    size_t num_features_;
    std::vector<std::unique_ptr<OnlinePredictor>> predictors_;
    std::vector<HorizonConfig> configs_;
};

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 15 of 30**: include/strategy/multi_symbol_oes_manager.h

**File Information**:
- **Path**: `include/strategy/multi_symbol_oes_manager.h`
- **Size**: 215 lines
- **Modified**: 2025-10-14 21:14:34
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/online_ensemble_strategy.h"
#include "strategy/signal_output.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <memory>
#include <map>
#include <vector>
#include <string>

namespace sentio {

/**
 * @brief Manages 6 independent OnlineEnsemble strategy instances
 *
 * Each symbol gets its own:
 * - OnlineEnsembleStrategy instance
 * - EWRLS predictor
 * - Feature engine
 * - Learning state
 *
 * This ensures no cross-contamination between symbols and allows
 * pure independent signal generation.
 *
 * Usage:
 *   MultiSymbolOESManager oes_mgr(config, data_mgr);
 *   auto signals = oes_mgr.generate_all_signals();
 *   oes_mgr.update_all(realized_pnls);
 */
class MultiSymbolOESManager {
public:
    struct Config {
        std::vector<std::string> symbols;              // Active symbols
        OnlineEnsembleStrategy::OnlineEnsembleConfig base_config;  // Base config for all OES
        bool independent_learning = true;              // Each symbol learns independently
        bool share_features = false;                   // Share feature engine (NOT RECOMMENDED)

        // Symbol-specific overrides (optional)
        std::map<std::string, OnlineEnsembleStrategy::OnlineEnsembleConfig> symbol_configs;
    };

    /**
     * @brief Construct manager with data source
     *
     * @param config Configuration
     * @param data_mgr Data manager for multi-symbol data
     */
    explicit MultiSymbolOESManager(
        const Config& config,
        std::shared_ptr<data::MultiSymbolDataManager> data_mgr
    );

    ~MultiSymbolOESManager() = default;

    // === Signal Generation ===

    /**
     * @brief Generate signals for all symbols
     *
     * Returns map of symbol → signal. Only symbols with valid data
     * will have signals.
     *
     * @return Map of symbol → SignalOutput
     */
    std::map<std::string, SignalOutput> generate_all_signals();

    /**
     * @brief Generate signal for specific symbol
     *
     * @param symbol Symbol ticker
     * @return SignalOutput for symbol (or empty if data invalid)
     */
    SignalOutput generate_signal(const std::string& symbol);

    // === Learning Updates ===

    /**
     * @brief Update all OES instances with realized P&L
     *
     * @param realized_pnls Map of symbol → realized P&L
     */
    void update_all(const std::map<std::string, double>& realized_pnls);

    /**
     * @brief Update specific symbol with realized P&L
     *
     * @param symbol Symbol ticker
     * @param realized_pnl Realized P&L from last trade
     */
    void update(const std::string& symbol, double realized_pnl);

    /**
     * @brief Process new bar for all symbols
     *
     * Called each bar to update feature engines and check learning state.
     */
    void on_bar();

    // === Warmup ===

    /**
     * @brief Warmup all OES instances from historical data
     *
     * @param symbol_bars Map of symbol → historical bars
     * @return true if warmup successful
     */
    bool warmup_all(const std::map<std::string, std::vector<Bar>>& symbol_bars);

    /**
     * @brief Warmup specific symbol
     *
     * @param symbol Symbol ticker
     * @param bars Historical bars
     * @return true if warmup successful
     */
    bool warmup(const std::string& symbol, const std::vector<Bar>& bars);

    // === Configuration ===

    /**
     * @brief Update configuration for all symbols
     *
     * @param new_config New base configuration
     */
    void update_config(const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config);

    /**
     * @brief Update configuration for specific symbol
     *
     * @param symbol Symbol ticker
     * @param new_config New configuration
     */
    void update_config(const std::string& symbol,
                      const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config);

    // === Diagnostics ===

    /**
     * @brief Get performance metrics for all symbols
     *
     * @return Map of symbol → performance metrics
     */
    std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics>
    get_all_performance_metrics() const;

    /**
     * @brief Get performance metrics for specific symbol
     *
     * @param symbol Symbol ticker
     * @return Performance metrics
     */
    OnlineEnsembleStrategy::PerformanceMetrics
    get_performance_metrics(const std::string& symbol) const;

    /**
     * @brief Check if all OES instances are ready
     *
     * @return true if all have sufficient warmup samples
     */
    bool all_ready() const;

    /**
     * @brief Get ready status for each symbol
     *
     * @return Map of symbol → ready status
     */
    std::map<std::string, bool> get_ready_status() const;

    /**
     * @brief Get learning state for all symbols
     *
     * @return Map of symbol → learning state
     */
    std::map<std::string, OnlineEnsembleStrategy::LearningState>
    get_all_learning_states() const;

    /**
     * @brief Get OES instance for symbol (for direct access)
     *
     * @param symbol Symbol ticker
     * @return Pointer to OES instance (nullptr if not found)
     */
    OnlineEnsembleStrategy* get_oes_instance(const std::string& symbol);

    /**
     * @brief Get const OES instance for symbol
     *
     * @param symbol Symbol ticker
     * @return Const pointer to OES instance (nullptr if not found)
     */
    const OnlineEnsembleStrategy* get_oes_instance(const std::string& symbol) const;

private:
    /**
     * @brief Get latest bar for symbol from data manager
     *
     * @param symbol Symbol ticker
     * @param bar Output bar
     * @return true if valid bar available
     */
    bool get_latest_bar(const std::string& symbol, Bar& bar);

    Config config_;
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr_;

    // Map of symbol → OES instance
    std::map<std::string, std::unique_ptr<OnlineEnsembleStrategy>> oes_instances_;

    // Statistics
    int total_signals_generated_{0};
    int total_updates_{0};
};

} // namespace sentio

```

## 📄 **FILE 16 of 30**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`
- **Size**: 245 lines
- **Modified**: 2025-10-16 04:25:52
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "strategy/market_regime_detector.h"
#include "strategy/regime_parameter_manager.h"
#include "learning/online_predictor.h"
#include "features/unified_feature_engine.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <map>

namespace sentio {

/**
 * @brief Full OnlineEnsemble Strategy using EWRLS multi-horizon predictor
 *
 * This strategy achieves online learning with ensemble methods:
 * - Real-time EWRLS model adaptation based on realized P&L
 * - Multi-horizon predictions (1, 5, 10 bars) with weighted ensemble
 * - Continuous performance tracking and adaptive calibration
 * - Target: 10% monthly return @ 60%+ signal accuracy
 *
 * Key Features:
 * - Incremental learning without retraining
 * - Adaptive learning rate based on market volatility
 * - Self-calibrating buy/sell thresholds
 * - Kelly Criterion position sizing integration
 * - Real-time performance metrics
 */
class OnlineEnsembleStrategy : public StrategyComponent {
public:
    struct OnlineEnsembleConfig : public StrategyConfig {
        // EWRLS parameters
        double ewrls_lambda = 0.995;          // Forgetting factor (0.99-0.999)
        double initial_variance = 100.0;       // Initial parameter uncertainty
        double regularization = 0.01;          // L2 regularization
        int warmup_samples = 100;              // Minimum samples before trading

        // Multi-horizon ensemble parameters
        std::vector<int> prediction_horizons = {1, 5, 10};  // Prediction horizons (bars)
        std::vector<double> horizon_weights = {0.3, 0.5, 0.2};  // Ensemble weights

        // Adaptive learning parameters
        bool enable_adaptive_learning = true;
        double min_lambda = 0.990;             // Fast adaptation limit
        double max_lambda = 0.999;             // Slow adaptation limit

        // Signal generation thresholds
        double buy_threshold = 0.53;           // Initial buy threshold
        double sell_threshold = 0.47;          // Initial sell threshold
        double neutral_zone = 0.06;            // Width of neutral zone

        // Bollinger Bands amplification (from WilliamsRSIBB strategy)
        bool enable_bb_amplification = true;   // Enable BB-based signal amplification
        int bb_period = 20;                    // BB period (matches feature engine)
        double bb_std_dev = 2.0;               // BB standard deviations
        double bb_proximity_threshold = 0.30;  // Within 30% of band for amplification
        double bb_amplification_factor = 0.10; // Boost probability by this much

        // Adaptive calibration
        bool enable_threshold_calibration = true;
        int calibration_window = 200;          // Bars for threshold calibration
        double target_win_rate = 0.60;        // Target 60% accuracy
        double threshold_step = 0.005;         // Calibration step size

        // Risk management
        bool enable_kelly_sizing = true;
        double kelly_fraction = 0.25;          // 25% of full Kelly
        double max_position_size = 0.50;       // Max 50% capital per position

        // Performance tracking
        int performance_window = 200;          // Window for metrics
        double target_monthly_return = 0.10;   // Target 10% monthly return

        // Regime detection parameters
        bool enable_regime_detection = false;  // Enable regime-aware parameter switching
        int regime_check_interval = 100;       // Check regime every N bars
        int regime_lookback_period = 100;      // Bars to analyze for regime detection

        OnlineEnsembleConfig() {
            name = "OnlineEnsemble";
            version = "2.0";
        }
    };

    struct PerformanceMetrics {
        double win_rate = 0.0;
        double avg_return = 0.0;
        double monthly_return_estimate = 0.0;
        double sharpe_estimate = 0.0;
        double directional_accuracy = 0.0;
        double recent_rmse = 0.0;
        int total_trades = 0;
        bool targets_met = false;
    };

    explicit OnlineEnsembleStrategy(const OnlineEnsembleConfig& config);
    virtual ~OnlineEnsembleStrategy() = default;

    // Main interface
    SignalOutput generate_signal(const Bar& bar);
    void update(const Bar& bar, double realized_pnl);
    void on_bar(const Bar& bar);

    // Predictor training (for warmup)
    void train_predictor(const std::vector<double>& features, double realized_return);
    std::vector<double> extract_features(const Bar& current_bar);

    // Feature caching support (for Optuna optimization speedup)
    void set_external_features(const std::vector<double>* features) {
        external_features_ = features;
        skip_feature_engine_update_ = (features != nullptr);
    }

    // Runtime configuration update (for mid-day optimization)
    void update_config(const OnlineEnsembleConfig& new_config) {
        config_ = new_config;
        // CRITICAL: Update member variables used by determine_signal()
        current_buy_threshold_ = new_config.buy_threshold;
        current_sell_threshold_ = new_config.sell_threshold;
    }

    // Get current thresholds (for PSM decision logic)
    double get_current_buy_threshold() const { return current_buy_threshold_; }
    double get_current_sell_threshold() const { return current_sell_threshold_; }

    // Learning state management
    struct LearningState {
        int64_t last_trained_bar_id = -1;      // Global bar ID of last training
        int last_trained_bar_index = -1;       // Index of last trained bar
        int64_t last_trained_timestamp_ms = 0; // Timestamp of last training
        bool is_warmed_up = false;              // Feature engine ready
        bool is_learning_current = true;        // Learning is up-to-date
        int bars_behind = 0;                    // How many bars behind
    };

    LearningState get_learning_state() const { return learning_state_; }
    bool ensure_learning_current(const Bar& bar);  // Catch up if needed
    bool is_learning_current() const { return learning_state_.is_learning_current; }

    // Performance and diagnostics
    PerformanceMetrics get_performance_metrics() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const {
        // Check both predictor warmup AND feature engine warmup
        return samples_seen_ >= config_.warmup_samples &&
               feature_engine_->warmup_remaining() == 0;
    }

    // Feature engine access (for volatility calculation)
    const features::UnifiedFeatureEngine* get_feature_engine() const {
        return feature_engine_.get();
    }

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);

private:
    OnlineEnsembleConfig config_;

    // Multi-horizon EWRLS predictor
    std::unique_ptr<learning::MultiHorizonPredictor> ensemble_predictor_;

    // Feature engineering (production-grade with O(1) updates, 45 features)
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_;

    // Bar history for feature generation
    std::deque<Bar> bar_history_;
    static constexpr size_t MAX_HISTORY = 500;

    // Horizon tracking for delayed updates
    struct HorizonPrediction {
        int entry_bar_index;
        int target_bar_index;
        int horizon;
        std::shared_ptr<const std::vector<double>> features;  // Shared, immutable
        double entry_price;
        bool is_long;
    };

    struct PendingUpdate {
        std::array<HorizonPrediction, 3> horizons;  // Fixed size for 3 horizons
        uint8_t count = 0;  // Track actual count (1-3)
    };

    std::map<int, PendingUpdate> pending_updates_;

    // Performance tracking
    struct TradeResult {
        bool won;
        double return_pct;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    int samples_seen_;

    // Adaptive thresholds
    double current_buy_threshold_;
    double current_sell_threshold_;
    int calibration_count_;

    // Learning state tracking
    LearningState learning_state_;
    std::deque<Bar> missed_bars_;  // Queue of bars that need training

    // External feature support for caching
    const std::vector<double>* external_features_ = nullptr;
    bool skip_feature_engine_update_ = false;

    // Regime detection (optional)
    std::unique_ptr<MarketRegimeDetector> regime_detector_;
    std::unique_ptr<RegimeParameterManager> regime_param_manager_;
    MarketRegime current_regime_;
    int bars_since_regime_check_;

    // Private methods
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);
    void check_and_update_regime();  // Regime detection method

    // BB amplification
    struct BollingerBands {
        double upper;
        double middle;
        double lower;
        double bandwidth;
        double position_pct;  // 0=lower band, 1=upper band
    };
    BollingerBands calculate_bollinger_bands() const;
    double apply_bb_amplification(double base_probability, const BollingerBands& bb) const;

    // Constants
    static constexpr int MIN_FEATURES_BARS = 100;  // Minimum bars for features
    static constexpr size_t TRADE_HISTORY_SIZE = 500;
};

} // namespace sentio

```

## 📄 **FILE 17 of 30**: include/strategy/signal_output.h

**File Information**:
- **Path**: `include/strategy/signal_output.h`
- **Size**: 40 lines
- **Modified**: 2025-10-16 06:50:41
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace sentio {

enum class SignalType {
    NEUTRAL,
    LONG,
    SHORT
};

struct SignalOutput {
    // Core fields
    uint64_t bar_id = 0;
    int64_t timestamp_ms = 0;
    int bar_index = 0;
    std::string symbol;
    double probability = 0.0;
    double confidence = 0.0;        // Confidence in the prediction (0-1)
    SignalType signal_type = SignalType::NEUTRAL;
    std::string strategy_name;
    std::string strategy_version;
    
    // NEW: Multi-bar prediction fields
    int prediction_horizon = 1;        // How many bars ahead this predicts (default=1 for backward compat)
    uint64_t target_bar_id = 0;       // The bar this prediction targets
    bool requires_hold = false;        // Signal requires minimum hold period
    int signal_generation_interval = 1; // How often signals are generated
    
    std::map<std::string, std::string> metadata;

    std::string to_json() const;
    std::string to_csv() const;
    static SignalOutput from_json(const std::string& json_str);
};

} // namespace sentio
```

## 📄 **FILE 18 of 30**: src/backend/adaptive_trading_mechanism.cpp

**File Information**:
- **Path**: `src/backend/adaptive_trading_mechanism.cpp`
- **Size**: 597 lines
- **Modified**: 2025-10-16 06:14:43
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include <numeric>
#include <filesystem>

namespace sentio {


// ===================================================================
// PERFORMANCE EVALUATOR IMPLEMENTATION
// ===================================================================

void PerformanceEvaluator::add_trade_outcome(const TradeOutcome& outcome) {
    trade_history_.push_back(outcome);
    
    // Maintain rolling window
    if (trade_history_.size() > MAX_HISTORY) {
        trade_history_.erase(trade_history_.begin());
    }
    
    utils::log_debug("Trade outcome added: PnL=" + std::to_string(outcome.actual_pnl) + 
                    ", Profitable=" + (outcome.was_profitable ? "YES" : "NO"));
}

void PerformanceEvaluator::add_portfolio_value(double value) {
    portfolio_values_.push_back(value);
    
    if (portfolio_values_.size() > MAX_HISTORY) {
        portfolio_values_.erase(portfolio_values_.begin());
    }
}

PerformanceMetrics PerformanceEvaluator::calculate_performance_metrics() {
    PerformanceMetrics metrics;
    
    if (trade_history_.empty()) {
        return metrics;
    }
    
    // Get recent trades for analysis
    size_t start_idx = trade_history_.size() > PERFORMANCE_WINDOW ? 
                      trade_history_.size() - PERFORMANCE_WINDOW : 0;
    
    std::vector<TradeOutcome> recent_trades(
        trade_history_.begin() + start_idx, trade_history_.end());
    
    // Calculate basic metrics
    metrics.total_trades = static_cast<int>(recent_trades.size());
    metrics.winning_trades = 0;
    metrics.losing_trades = 0;
    metrics.gross_profit = 0.0;
    metrics.gross_loss = 0.0;
    
    for (const auto& trade : recent_trades) {
        if (trade.was_profitable) {
            metrics.winning_trades++;
            metrics.gross_profit += trade.actual_pnl;
        } else {
            metrics.losing_trades++;
            metrics.gross_loss += std::abs(trade.actual_pnl);
        }
        
        metrics.returns.push_back(trade.pnl_percentage);
    }
    
    // Calculate derived metrics
    metrics.win_rate = metrics.total_trades > 0 ? 
                      static_cast<double>(metrics.winning_trades) / metrics.total_trades : 0.0;
    
    metrics.profit_factor = metrics.gross_loss > 0 ? 
                           metrics.gross_profit / metrics.gross_loss : 1.0;
    
    metrics.sharpe_ratio = calculate_sharpe_ratio(metrics.returns);
    metrics.max_drawdown = calculate_max_drawdown();
    metrics.capital_efficiency = calculate_capital_efficiency();
    
    return metrics;
}

double PerformanceEvaluator::calculate_reward_signal(const PerformanceMetrics& metrics) {
    // Multi-objective reward function
    double profit_component = metrics.gross_profit - metrics.gross_loss;
    double risk_component = metrics.sharpe_ratio * 0.5;
    double drawdown_penalty = metrics.max_drawdown * -2.0;
    double overtrading_penalty = std::max(0.0, metrics.trade_frequency - 10.0) * -0.1;
    
    double total_reward = profit_component + risk_component + drawdown_penalty + overtrading_penalty;
    
    utils::log_debug("Reward calculation: Profit=" + std::to_string(profit_component) + 
                    ", Risk=" + std::to_string(risk_component) + 
                    ", Drawdown=" + std::to_string(drawdown_penalty) + 
                    ", Total=" + std::to_string(total_reward));
    
    return total_reward;
}

double PerformanceEvaluator::calculate_sharpe_ratio(const std::vector<double>& returns) {
    if (returns.size() < 2) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    
    double std_dev = std::sqrt(variance);
    return std_dev > 0 ? mean_return / std_dev : 0.0;
}

double PerformanceEvaluator::calculate_max_drawdown() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double peak = portfolio_values_[0];
    double max_dd = 0.0;
    
    for (double value : portfolio_values_) {
        if (value > peak) {
            peak = value;
        }
        
        double drawdown = (peak - value) / peak;
        max_dd = std::max(max_dd, drawdown);
    }
    
    return max_dd;
}

double PerformanceEvaluator::calculate_capital_efficiency() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double initial_value = portfolio_values_.front();
    double final_value = portfolio_values_.back();
    
    return initial_value > 0 ? (final_value - initial_value) / initial_value : 0.0;
}

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER IMPLEMENTATION
// ===================================================================

QLearningThresholdOptimizer::QLearningThresholdOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    utils::log_info("Q-Learning Threshold Optimizer initialized with learning_rate=" + 
                   std::to_string(learning_rate_) + ", exploration_rate=" + std::to_string(exploration_rate_));
}

ThresholdAction QLearningThresholdOptimizer::select_action(const MarketState& state, 
                                                          const ThresholdPair& current_thresholds,
                                                          const PerformanceMetrics& performance) {
    int state_hash = discretize_state(state, current_thresholds, performance);
    
    // Epsilon-greedy action selection
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    if (dis(rng_) < exploration_rate_) {
        // Explore: random action
        std::uniform_int_distribution<int> action_dis(0, static_cast<int>(ThresholdAction::COUNT) - 1);
        ThresholdAction action = static_cast<ThresholdAction>(action_dis(rng_));
        utils::log_debug("Q-Learning: EXPLORE action=" + std::to_string(static_cast<int>(action)));
        return action;
    } else {
        // Exploit: best known action
        ThresholdAction action = get_best_action(state_hash);
        utils::log_debug("Q-Learning: EXPLOIT action=" + std::to_string(static_cast<int>(action)));
        return action;
    }
}

void QLearningThresholdOptimizer::update_q_value(const MarketState& prev_state,
                                                 const ThresholdPair& prev_thresholds,
                                                 const PerformanceMetrics& prev_performance,
                                                 ThresholdAction action,
                                                 double reward,
                                                 const MarketState& new_state,
                                                 const ThresholdPair& new_thresholds,
                                                 const PerformanceMetrics& new_performance) {
    
    int prev_state_hash = discretize_state(prev_state, prev_thresholds, prev_performance);
    int new_state_hash = discretize_state(new_state, new_thresholds, new_performance);
    
    StateActionPair sa_pair{prev_state_hash, static_cast<int>(action)};
    
    // Get current Q-value
    double current_q = get_q_value(sa_pair);
    
    // Get maximum Q-value for next state
    double max_next_q = get_max_q_value(new_state_hash);
    
    // Q-learning update
    double target = reward + discount_factor_ * max_next_q;
    double new_q = current_q + learning_rate_ * (target - current_q);
    
    q_table_[sa_pair] = new_q;
    state_visit_count_[prev_state_hash]++;
    
    // Decay exploration rate
    exploration_rate_ = std::max(min_exploration_, exploration_rate_ * exploration_decay_);
    
    utils::log_debug("Q-Learning update: State=" + std::to_string(prev_state_hash) + 
                    ", Action=" + std::to_string(static_cast<int>(action)) + 
                    ", Reward=" + std::to_string(reward) + 
                    ", Q_old=" + std::to_string(current_q) + 
                    ", Q_new=" + std::to_string(new_q));
}

ThresholdPair QLearningThresholdOptimizer::apply_action(const ThresholdPair& current_thresholds, ThresholdAction action) {
    ThresholdPair new_thresholds = current_thresholds;
    
    switch (action) {
        case ThresholdAction::INCREASE_BUY_SMALL:
            new_thresholds.buy_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_BUY_SMALL:
            new_thresholds.buy_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold -= 0.03;
            break;
        case ThresholdAction::INCREASE_SELL_SMALL:
            new_thresholds.sell_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_SELL_SMALL:
            new_thresholds.sell_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold -= 0.03;
            break;
        case ThresholdAction::MAINTAIN_THRESHOLDS:
        default:
            // No change
            break;
    }
    
    // Ensure thresholds remain valid
    new_thresholds.buy_threshold = std::clamp(new_thresholds.buy_threshold, 0.51, 0.90);
    new_thresholds.sell_threshold = std::clamp(new_thresholds.sell_threshold, 0.10, 0.49);
    
    // Ensure minimum gap
    if (new_thresholds.buy_threshold - new_thresholds.sell_threshold < 0.05) {
        new_thresholds.buy_threshold = new_thresholds.sell_threshold + 0.05;
        new_thresholds.buy_threshold = std::min(new_thresholds.buy_threshold, 0.90);
    }
    
    return new_thresholds;
}

double QLearningThresholdOptimizer::get_learning_progress() const {
    return 1.0 - exploration_rate_;
}

int QLearningThresholdOptimizer::discretize_state(const MarketState& state, 
                                                 const ThresholdPair& thresholds,
                                                 const PerformanceMetrics& performance) {
    // Create a hash of the discretized state
    int buy_bin = static_cast<int>((thresholds.buy_threshold - 0.5) / 0.4 * THRESHOLD_BINS);
    int sell_bin = static_cast<int>((thresholds.sell_threshold - 0.1) / 0.4 * THRESHOLD_BINS);
    int vol_bin = static_cast<int>(std::min(state.volatility / 0.5, 1.0) * 5);
    int trend_bin = static_cast<int>((state.trend_strength + 1.0) / 2.0 * 5);
    int perf_bin = static_cast<int>(std::clamp(performance.win_rate, 0.0, 1.0) * PERFORMANCE_BINS);
    
    // Combine bins into a single hash
    return buy_bin * 10000 + sell_bin * 1000 + vol_bin * 100 + trend_bin * 10 + perf_bin;
}

double QLearningThresholdOptimizer::get_q_value(const StateActionPair& sa_pair) {
    auto it = q_table_.find(sa_pair);
    return (it != q_table_.end()) ? it->second : 0.0; // Optimistic initialization
}

double QLearningThresholdOptimizer::get_max_q_value(int state_hash) {
    double max_q = 0.0;
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        max_q = std::max(max_q, get_q_value(sa_pair));
    }
    
    return max_q;
}

ThresholdAction QLearningThresholdOptimizer::get_best_action(int state_hash) {
    ThresholdAction best_action = ThresholdAction::MAINTAIN_THRESHOLDS;
    double best_q = get_q_value({state_hash, static_cast<int>(best_action)});
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        double q_val = get_q_value(sa_pair);
        
        if (q_val > best_q) {
            best_q = q_val;
            best_action = static_cast<ThresholdAction>(action);
        }
    }
    
    return best_action;
}

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER IMPLEMENTATION
// ===================================================================

MultiArmedBanditOptimizer::MultiArmedBanditOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    initialize_arms();
    utils::log_info("Multi-Armed Bandit Optimizer initialized with " + std::to_string(arms_.size()) + " arms");
}

ThresholdPair MultiArmedBanditOptimizer::select_thresholds() {
    if (arms_.empty()) {
        return ThresholdPair(); // Default thresholds
    }
    
    // UCB1 algorithm
    update_confidence_bounds();
    
    auto best_arm = std::max_element(arms_.begin(), arms_.end(),
        [](const BanditArm& a, const BanditArm& b) {
            return (a.estimated_reward + a.confidence_bound) < 
                   (b.estimated_reward + b.confidence_bound);
        });
    
    utils::log_debug("Bandit selected: Buy=" + std::to_string(best_arm->thresholds.buy_threshold) + 
                    ", Sell=" + std::to_string(best_arm->thresholds.sell_threshold) + 
                    ", UCB=" + std::to_string(best_arm->estimated_reward + best_arm->confidence_bound));
    
    return best_arm->thresholds;
}

void MultiArmedBanditOptimizer::update_reward(const ThresholdPair& thresholds, double reward) {
    // Find the arm that was pulled
    auto arm_it = std::find_if(arms_.begin(), arms_.end(),
        [&thresholds](const BanditArm& arm) {
            return std::abs(arm.thresholds.buy_threshold - thresholds.buy_threshold) < 0.005 &&
                   std::abs(arm.thresholds.sell_threshold - thresholds.sell_threshold) < 0.005;
        });
    
    if (arm_it != arms_.end()) {
        // Update arm's estimated reward using incremental mean
        arm_it->pull_count++;
        total_pulls_++;
        
        double old_estimate = arm_it->estimated_reward;
        arm_it->estimated_reward = old_estimate + (reward - old_estimate) / arm_it->pull_count;
        
        utils::log_debug("Bandit reward update: Buy=" + std::to_string(thresholds.buy_threshold) + 
                        ", Sell=" + std::to_string(thresholds.sell_threshold) + 
                        ", Reward=" + std::to_string(reward) + 
                        ", New_Est=" + std::to_string(arm_it->estimated_reward));
    }
}

void MultiArmedBanditOptimizer::initialize_arms() {
    // Create a grid of threshold combinations
    for (double buy = 0.55; buy <= 0.85; buy += 0.05) {
        for (double sell = 0.15; sell <= 0.45; sell += 0.05) {
            if (buy > sell + 0.05) { // Ensure minimum gap
                arms_.emplace_back(ThresholdPair(buy, sell));
            }
        }
    }
}

void MultiArmedBanditOptimizer::update_confidence_bounds() {
    for (auto& arm : arms_) {
        if (arm.pull_count == 0) {
            arm.confidence_bound = std::numeric_limits<double>::max();
        } else {
            arm.confidence_bound = std::sqrt(2.0 * std::log(total_pulls_) / arm.pull_count);
        }
    }
}

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER IMPLEMENTATION
// ===================================================================

AdaptiveThresholdManager::AdaptiveThresholdManager(const AdaptiveConfig& config) 
    : config_(config), current_thresholds_(0.55, 0.45) {
    
    // Initialize components
    q_learner_ = std::make_unique<QLearningThresholdOptimizer>();
    bandit_optimizer_ = std::make_unique<MultiArmedBanditOptimizer>();
    regime_detector_ = std::make_unique<MarketRegimeDetector>();
    performance_evaluator_ = std::make_unique<PerformanceEvaluator>();
    
    // Initialize regime-specific thresholds
    initialize_regime_thresholds();
    
    utils::log_info("AdaptiveThresholdManager initialized: Algorithm=" + 
                   std::to_string(static_cast<int>(config_.algorithm)) + 
                   ", LearningRate=" + std::to_string(config_.learning_rate) + 
                   ", ConservativeMode=" + (config_.conservative_mode ? "YES" : "NO"));
}

ThresholdPair AdaptiveThresholdManager::get_current_thresholds(const SignalOutput& signal, const Bar& bar) {
    // Update market state
    // current_market_state_ = regime_detector_->analyze_market_state(bar, recent_bars_, signal);
    recent_bars_.push_back(bar);
    if (recent_bars_.size() > 100) {
        recent_bars_.erase(recent_bars_.begin());
    }
    
    // Check circuit breaker
    if (circuit_breaker_active_) {
        utils::log_warning("Circuit breaker active - using conservative thresholds");
        return get_conservative_thresholds();
    }
    
    // Update performance and potentially adjust thresholds
    update_performance_and_learn();
    
    // Get regime-adapted thresholds if enabled
    if (config_.enable_regime_adaptation) {
        return get_regime_adapted_thresholds();
    }
    
    return current_thresholds_;
}

void AdaptiveThresholdManager::process_trade_outcome(const std::string& symbol, TradeAction action, 
                                                    double quantity, double price, double trade_value, double fees,
                                                    double actual_pnl, double pnl_percentage, bool was_profitable) {
    TradeOutcome outcome;
    outcome.symbol = symbol;
    outcome.action = action;
    outcome.quantity = quantity;
    outcome.price = price;
    outcome.trade_value = trade_value;
    outcome.fees = fees;
    outcome.actual_pnl = actual_pnl;
    outcome.pnl_percentage = pnl_percentage;
    outcome.was_profitable = was_profitable;
    outcome.outcome_timestamp = std::chrono::system_clock::now();
    
    performance_evaluator_->add_trade_outcome(outcome);
    
    // Update learning algorithms with reward feedback
    if (learning_enabled_) {
        current_performance_ = performance_evaluator_->calculate_performance_metrics();
        double reward = performance_evaluator_->calculate_reward_signal(current_performance_);
        
        // Update based on algorithm type
        switch (config_.algorithm) {
            case LearningAlgorithm::Q_LEARNING:
                // Q-learning update will happen in next call to update_performance_and_learn()
                break;
                
            case LearningAlgorithm::MULTI_ARMED_BANDIT:
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
                
            case LearningAlgorithm::ENSEMBLE:
                // Update both algorithms
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
        }
    }
    
    // Check for circuit breaker conditions
    check_circuit_breaker();
}

void AdaptiveThresholdManager::update_portfolio_value(double value) {
    performance_evaluator_->add_portfolio_value(value);
}

double AdaptiveThresholdManager::get_learning_progress() const {
    return q_learner_->get_learning_progress();
}

std::string AdaptiveThresholdManager::generate_performance_report() const {
    std::ostringstream report;
    
    report << "=== ADAPTIVE TRADING PERFORMANCE REPORT ===\n";
    report << "Current Thresholds: Buy=" << std::fixed << std::setprecision(3) << current_thresholds_.buy_threshold 
           << ", Sell=" << current_thresholds_.sell_threshold << "\n";
    report << "Market Regime: " << static_cast<int>(current_market_state_.regime) << "\n";
    report << "Total Trades: " << current_performance_.total_trades << "\n";
    report << "Win Rate: " << std::fixed << std::setprecision(1) << (current_performance_.win_rate * 100) << "%\n";
    report << "Profit Factor: " << std::fixed << std::setprecision(2) << current_performance_.profit_factor << "\n";
    report << "Sharpe Ratio: " << std::fixed << std::setprecision(2) << current_performance_.sharpe_ratio << "\n";
    report << "Max Drawdown: " << std::fixed << std::setprecision(1) << (current_performance_.max_drawdown * 100) << "%\n";
    report << "Learning Progress: " << std::fixed << std::setprecision(1) << (get_learning_progress() * 100) << "%\n";
    report << "Circuit Breaker: " << (circuit_breaker_active_ ? "ACTIVE" : "INACTIVE") << "\n";
    
    return report.str();
}

void AdaptiveThresholdManager::initialize_regime_thresholds() {
    // Conservative thresholds for volatile markets
    // regime_thresholds_[MarketRegime::BULL_HIGH_VOL] = ThresholdPair(0.65, 0.35);
    // regime_thresholds_[MarketRegime::BEAR_HIGH_VOL] = ThresholdPair(0.70, 0.30);
    // regime_thresholds_[MarketRegime::SIDEWAYS_HIGH_VOL] = ThresholdPair(0.68, 0.32);
    
    // More aggressive thresholds for stable markets
    // regime_thresholds_[MarketRegime::BULL_LOW_VOL] = ThresholdPair(0.58, 0.42);
    // regime_thresholds_[MarketRegime::BEAR_LOW_VOL] = ThresholdPair(0.62, 0.38);
    // regime_thresholds_[MarketRegime::SIDEWAYS_LOW_VOL] = ThresholdPair(0.60, 0.40);
}

void AdaptiveThresholdManager::update_performance_and_learn() {
    if (!learning_enabled_ || circuit_breaker_active_) {
        return;
    }
    
    // Update performance metrics
    PerformanceMetrics new_performance = performance_evaluator_->calculate_performance_metrics();
    
    // Only learn if we have enough data
    if (new_performance.total_trades < config_.performance_window / 2) {
        return;
    }
    
    // Q-Learning update
    if (config_.algorithm == LearningAlgorithm::Q_LEARNING || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        double reward = performance_evaluator_->calculate_reward_signal(new_performance);
        
        // Select and apply action
        ThresholdAction action = q_learner_->select_action(
            current_market_state_, current_thresholds_, current_performance_);
        
        ThresholdPair new_thresholds = q_learner_->apply_action(current_thresholds_, action);
        
        // Update Q-values if we have previous state
        if (current_performance_.total_trades > 0) {
            q_learner_->update_q_value(
                current_market_state_, current_thresholds_, current_performance_,
                action, reward,
                current_market_state_, new_thresholds, new_performance);
        }
        
        current_thresholds_ = new_thresholds;
    }
    
    // Multi-Armed Bandit update
    if (config_.algorithm == LearningAlgorithm::MULTI_ARMED_BANDIT || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        current_thresholds_ = bandit_optimizer_->select_thresholds();
    }
    
    current_performance_ = new_performance;
}

ThresholdPair AdaptiveThresholdManager::get_regime_adapted_thresholds() {
    auto regime_it = regime_thresholds_.find(current_market_state_.regime);
    if (regime_it != regime_thresholds_.end()) {
        // Blend learned thresholds with regime-specific ones
        ThresholdPair regime_thresholds = regime_it->second;
        double blend_factor = config_.conservative_mode ? 0.7 : 0.3;
        
        return ThresholdPair(
            current_thresholds_.buy_threshold * (1.0 - blend_factor) + 
            regime_thresholds.buy_threshold * blend_factor,
            current_thresholds_.sell_threshold * (1.0 - blend_factor) + 
            regime_thresholds.sell_threshold * blend_factor
        );
    }
    
    return current_thresholds_;
}

ThresholdPair AdaptiveThresholdManager::get_conservative_thresholds() {
    // Return very conservative thresholds during circuit breaker
    return ThresholdPair(0.75, 0.25);
}

void AdaptiveThresholdManager::check_circuit_breaker() {
    // Only activate circuit breaker if we have sufficient trading history
    if (current_performance_.total_trades < 10) {
        return; // Not enough data to make circuit breaker decisions
    }
    
    if (current_performance_.max_drawdown > config_.max_drawdown_limit ||
        current_performance_.win_rate < 0.3 ||
        (current_performance_.total_trades > 20 && current_performance_.profit_factor < 0.8)) {
        
        circuit_breaker_active_ = true;
        learning_enabled_ = false;
        
        utils::log_error("CIRCUIT BREAKER ACTIVATED: Drawdown=" + std::to_string(current_performance_.max_drawdown) + 
                        ", WinRate=" + std::to_string(current_performance_.win_rate) + 
                        ", ProfitFactor=" + std::to_string(current_performance_.profit_factor));
    }
}

} // namespace sentio

```

## 📄 **FILE 19 of 30**: src/backend/rotation_trading_backend.cpp

**File Information**:
- **Path**: `src/backend/rotation_trading_backend.cpp`
- **Size**: 1191 lines
- **Modified**: 2025-10-16 22:13:11
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "backend/rotation_trading_backend.h"
#include "common/utils.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iomanip>
#include <iostream>

using json = nlohmann::json;

namespace sentio {

RotationTradingBackend::RotationTradingBackend(
    const Config& config,
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr,
    std::shared_ptr<AlpacaClient> broker
)
    : config_(config)
    , data_manager_(data_mgr)
    , broker_(broker)
    , current_cash_(config.starting_capital) {

    utils::log_info("========================================");
    utils::log_info("RotationTradingBackend Initializing");
    utils::log_info("========================================");

    // Create data manager if not provided
    if (!data_manager_) {
        data::MultiSymbolDataManager::Config dm_config = config_.data_config;
        dm_config.symbols = config_.symbols;
        data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);
        utils::log_info("Created MultiSymbolDataManager");
    }

    // Create OES manager
    MultiSymbolOESManager::Config oes_config;
    oes_config.symbols = config_.symbols;
    oes_config.base_config = config_.oes_config;
    oes_manager_ = std::make_unique<MultiSymbolOESManager>(oes_config, data_manager_);
    utils::log_info("Created MultiSymbolOESManager");

    // Create signal aggregator
    signal_aggregator_ = std::make_unique<SignalAggregator>(config_.aggregator_config);
    utils::log_info("Created SignalAggregator");

    // Create rotation manager
    rotation_manager_ = std::make_unique<RotationPositionManager>(config_.rotation_config);
    utils::log_info("Created RotationPositionManager");

    utils::log_info("Symbols: " + std::to_string(config_.symbols.size()));
    utils::log_info("Starting capital: $" + std::to_string(config_.starting_capital));
    utils::log_info("Max positions: " + std::to_string(config_.rotation_config.max_positions));
    utils::log_info("Backend initialization complete");
}

RotationTradingBackend::~RotationTradingBackend() {
    if (trading_active_) {
        stop_trading();
    }
}

// === Trading Session Management ===

bool RotationTradingBackend::warmup(
    const std::map<std::string, std::vector<Bar>>& symbol_bars
) {
    utils::log_info("========================================");
    utils::log_info("Warmup Phase");
    utils::log_info("========================================");
    std::cout << "Starting warmup with " << symbol_bars.size() << " symbols..." << std::endl;

    // Log warmup data sizes (to log file only)
    for (const auto& [symbol, bars] : symbol_bars) {
        utils::log_info("  " + symbol + ": " + std::to_string(bars.size()) + " warmup bars");
    }

    bool success = oes_manager_->warmup_all(symbol_bars);

    // Check individual readiness (to log file only)
    auto ready_status = oes_manager_->get_ready_status();
    for (const auto& [symbol, is_ready] : ready_status) {
        utils::log_info("  " + symbol + ": " + (is_ready ? "READY" : "NOT READY"));
    }

    if (success) {
        utils::log_info("✓ Warmup complete - all OES instances ready");
        std::cout << "✓ Warmup complete - all strategies ready" << std::endl;
    } else {
        utils::log_error("Warmup failed - some OES instances not ready");
        std::cout << "❌ Warmup failed - some strategies not ready" << std::endl;
    }

    return success;
}

bool RotationTradingBackend::start_trading() {
    utils::log_info("========================================");
    utils::log_info("Starting Trading Session");
    utils::log_info("========================================");

    // Check if ready
    if (!is_ready()) {
        utils::log_error("Cannot start trading - backend not ready");
        std::cout << "❌ Cannot start trading - backend not ready" << std::endl;

        // Debug: Check which OES instances are not ready
        auto ready_status = oes_manager_->get_ready_status();
        for (const auto& [symbol, is_ready] : ready_status) {
            if (!is_ready) {
                utils::log_error("  " + symbol + " is NOT READY");
                std::cout << "  " << symbol << " is NOT READY" << std::endl;
            }
        }

        return false;
    }

    // Open log files
    if (config_.log_all_signals) {
        signal_log_.open(config_.signal_log_path, std::ios::out | std::ios::trunc);
        if (!signal_log_.is_open()) {
            utils::log_error("Failed to open signal log: " + config_.signal_log_path);
            std::cout << "❌ Failed to open signal log: " << config_.signal_log_path << std::endl;
            return false;
        }
    }

    if (config_.log_all_decisions) {
        decision_log_.open(config_.decision_log_path, std::ios::out | std::ios::trunc);
        if (!decision_log_.is_open()) {
            utils::log_error("Failed to open decision log: " + config_.decision_log_path);
            std::cout << "❌ Failed to open decision log: " << config_.decision_log_path << std::endl;
            return false;
        }
    }

    trade_log_.open(config_.trade_log_path, std::ios::out | std::ios::trunc);
    if (!trade_log_.is_open()) {
        utils::log_error("Failed to open trade log: " + config_.trade_log_path);
        std::cout << "❌ Failed to open trade log: " << config_.trade_log_path << std::endl;
        return false;
    }

    position_log_.open(config_.position_log_path, std::ios::out | std::ios::trunc);
    if (!position_log_.is_open()) {
        utils::log_error("Failed to open position log: " + config_.position_log_path);
        std::cout << "❌ Failed to open position log: " << config_.position_log_path << std::endl;
        return false;
    }

    // Initialize session stats
    session_stats_ = SessionStats();
    session_stats_.session_start = std::chrono::system_clock::now();
    session_stats_.current_equity = config_.starting_capital;
    session_stats_.max_equity = config_.starting_capital;
    session_stats_.min_equity = config_.starting_capital;

    trading_active_ = true;
    is_warmup_ = false;  // End warmup mode, start actual trading

    utils::log_info("✓ Trading session started");
    utils::log_info("✓ Warmup mode disabled - trades will now execute");
    utils::log_info("  Signal log: " + config_.signal_log_path);
    utils::log_info("  Decision log: " + config_.decision_log_path);
    utils::log_info("  Trade log: " + config_.trade_log_path);
    utils::log_info("  Position log: " + config_.position_log_path);

    return true;
}

void RotationTradingBackend::stop_trading() {
    if (!trading_active_) {
        return;
    }

    utils::log_info("========================================");
    utils::log_info("Stopping Trading Session");
    utils::log_info("========================================");

    // DIAGNOSTIC: Pre-liquidation state
    utils::log_info("========================================");
    utils::log_info("Pre-Liquidation State");
    utils::log_info("========================================");
    utils::log_info("Cash: $" + std::to_string(current_cash_));
    utils::log_info("Allocated Capital: $" + std::to_string(allocated_capital_));

    auto positions = rotation_manager_->get_positions();
    double unrealized_total = 0.0;

    for (const auto& [symbol, pos] : positions) {
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_[symbol];
            double entry_cost = position_entry_costs_[symbol];
            double current_value = shares * pos.current_price;
            double unrealized = current_value - entry_cost;
            unrealized_total += unrealized;

            utils::log_info("Position " + symbol + ": " +
                          std::to_string(shares) + " shares, " +
                          "entry_cost=$" + std::to_string(entry_cost) +
                          ", current_value=$" + std::to_string(current_value) +
                          ", unrealized=$" + std::to_string(unrealized) +
                          " (" + std::to_string(unrealized / entry_cost * 100.0) + "%)");
        }
    }

    utils::log_info("Total Unrealized P&L: $" + std::to_string(unrealized_total));
    double pre_liquidation_equity = current_cash_ + allocated_capital_ + unrealized_total;
    utils::log_info("Pre-liquidation Equity: $" + std::to_string(pre_liquidation_equity) +
                   " (" + std::to_string(pre_liquidation_equity / config_.starting_capital * 100.0) + "%)");

    // Liquidate all positions
    if (rotation_manager_->get_position_count() > 0) {
        utils::log_info("========================================");
        utils::log_info("Liquidating " + std::to_string(positions.size()) + " positions...");
        liquidate_all_positions("Session End");
    }

    // Update session stats after liquidation
    update_session_stats();

    // DIAGNOSTIC: Post-liquidation state
    utils::log_info("========================================");
    utils::log_info("Post-Liquidation State");
    utils::log_info("========================================");
    utils::log_info("Final Cash: $" + std::to_string(current_cash_));
    utils::log_info("Final Allocated: $" + std::to_string(allocated_capital_) +
                   " (should be ~$0)");
    utils::log_info("Final Equity (from stats): $" + std::to_string(session_stats_.current_equity));
    utils::log_info("Total P&L: $" + std::to_string(session_stats_.total_pnl) +
                   " (" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%)");

    // Close log files
    if (signal_log_.is_open()) signal_log_.close();
    if (decision_log_.is_open()) decision_log_.close();
    if (trade_log_.is_open()) trade_log_.close();
    if (position_log_.is_open()) position_log_.close();

    // Finalize session stats
    session_stats_.session_end = std::chrono::system_clock::now();

    trading_active_ = false;

    // Print summary
    utils::log_info("========================================");
    utils::log_info("Session Summary");
    utils::log_info("========================================");
    utils::log_info("Bars processed: " + std::to_string(session_stats_.bars_processed));
    utils::log_info("Signals generated: " + std::to_string(session_stats_.signals_generated));
    utils::log_info("Trades executed: " + std::to_string(session_stats_.trades_executed));
    utils::log_info("Positions opened: " + std::to_string(session_stats_.positions_opened));
    utils::log_info("Positions closed: " + std::to_string(session_stats_.positions_closed));
    utils::log_info("Rotations: " + std::to_string(session_stats_.rotations));
    utils::log_info("");
    utils::log_info("Total P&L: $" + std::to_string(session_stats_.total_pnl) +
                   " (" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%)");
    utils::log_info("Final equity: $" + std::to_string(session_stats_.current_equity));
    utils::log_info("Max drawdown: " + std::to_string(session_stats_.max_drawdown * 100.0) + "%");
    utils::log_info("Win rate: " + std::to_string(session_stats_.win_rate * 100.0) + "%");
    utils::log_info("Sharpe ratio: " + std::to_string(session_stats_.sharpe_ratio));
    utils::log_info("MRD: " + std::to_string(session_stats_.mrd * 100.0) + "%");
    utils::log_info("========================================");
}

bool RotationTradingBackend::on_bar() {
    if (!trading_active_) {
        utils::log_error("Cannot process bar - trading not active");
        return false;
    }

    session_stats_.bars_processed++;

    // Step 1: Update OES on_bar (updates feature engines)
    oes_manager_->on_bar();

    // Step 1.5: Data quality validation
    // Get current snapshot and validate bars
    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, Bar> current_bars;
    for (const auto& [symbol, snap] : snapshot.snapshots) {
        current_bars[symbol] = snap.latest_bar;
    }
    if (!data_validator_.validate_snapshot(current_bars)) {
        std::string error = data_validator_.get_last_error();
        utils::log_error("[DataValidator] Bar validation failed: " + error);
        // In strict mode, we could skip this bar, but for now just warn
    }

    // Step 2: Generate signals
    auto signals = generate_signals();
    session_stats_.signals_generated += signals.size();

    // Log signals
    if (config_.log_all_signals) {
        for (const auto& [symbol, signal] : signals) {
            log_signal(symbol, signal);
        }
    }

    // Step 3: Rank signals
    auto ranked_signals = rank_signals(signals);

    // CRITICAL FIX: Circuit breaker - check for large losses or minimum capital
    // IMPORTANT: Calculate total unrealized P&L using current position values
    double unrealized_pnl = 0.0;
    auto positions = rotation_manager_->get_positions();
    for (const auto& [symbol, position] : positions) {
        if (position_entry_costs_.count(symbol) > 0 && position_shares_.count(symbol) > 0) {
            double entry_cost = position_entry_costs_.at(symbol);
            int shares = position_shares_.at(symbol);
            double current_value = shares * position.current_price;
            double pnl = current_value - entry_cost;
            unrealized_pnl += pnl;
        }
    }
    double current_equity = current_cash_ + allocated_capital_ + unrealized_pnl;
    double equity_pct = current_equity / config_.starting_capital;
    const double MIN_TRADING_CAPITAL = 10000.0;  // $10k minimum to continue trading

    // Update trading monitor with equity
    trading_monitor_.update_equity(current_equity, config_.starting_capital);

    // DEBUG: Commented out to reduce output noise
    // std::cerr << "[EQUITY] cash=$" << current_cash_
    //           << ", allocated=$" << allocated_capital_
    //           << ", unrealized=$" << unrealized_pnl
    //           << ", equity=$" << current_equity
    //           << " (" << (equity_pct * 100.0) << "%)" << std::endl;

    if (!circuit_breaker_triggered_) {
        if (equity_pct < 0.60) {  // 40% loss threshold
            circuit_breaker_triggered_ = true;
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
            utils::log_error("║ CIRCUIT BREAKER TRIGGERED - LARGE LOSS DETECTED          ║");
            utils::log_error("║ Current equity: $" + std::to_string(current_equity) +
                            " (" + std::to_string(equity_pct * 100.0) + "% of start)      ║");
            utils::log_error("║ Stopping all new entries - will only exit positions      ║");
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
        } else if (current_equity < MIN_TRADING_CAPITAL) {  // Minimum capital threshold
            circuit_breaker_triggered_ = true;
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
            utils::log_error("║ CIRCUIT BREAKER TRIGGERED - MINIMUM CAPITAL BREACH       ║");
            utils::log_error("║ Current equity: $" + std::to_string(current_equity) +
                            " (below $" + std::to_string(MIN_TRADING_CAPITAL) + " minimum)      ║");
            utils::log_error("║ Stopping all new entries - will only exit positions      ║");
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
        }
    }

    // Step 4: Check for EOD
    int current_time_minutes = get_current_time_minutes();

    if (is_eod(current_time_minutes)) {
        utils::log_info("EOD reached - liquidating all positions");
        liquidate_all_positions("EOD");
        return true;
    }

    // Step 5: Make position decisions
    auto decisions = make_decisions(ranked_signals);

    // DIAGNOSTIC: Log received decisions
    utils::log_info("╔════════════════════════════════════════════════════════════╗");
    utils::log_info("║ BACKEND RECEIVED " + std::to_string(decisions.size()) + " DECISIONS FROM make_decisions()     ║");
    utils::log_info("╚════════════════════════════════════════════════════════════╝");

    // Log decisions
    if (config_.log_all_decisions) {
        for (const auto& decision : decisions) {
            log_decision(decision);
        }
    }

    // Step 6: Execute decisions
    utils::log_info("╔════════════════════════════════════════════════════════════╗");
    utils::log_info("║ EXECUTING DECISIONS (skipping HOLDs)                      ║");
    utils::log_info("╚════════════════════════════════════════════════════════════╝");
    int executed_count = 0;
    for (const auto& decision : decisions) {
        if (decision.decision != RotationPositionManager::Decision::HOLD) {
            utils::log_info(">>> EXECUTING decision #" + std::to_string(executed_count + 1) +
                          ": " + decision.symbol);
            execute_decision(decision);
            executed_count++;
        }
    }
    utils::log_info(">>> EXECUTED " + std::to_string(executed_count) + " decisions (skipped " +
                   std::to_string(decisions.size() - executed_count) + " HOLDs)");

    // Step 7: Update learning
    update_learning();

    // Step 8: Log positions
    log_positions();

    // Step 9: Update statistics
    update_session_stats();

    return true;
}

bool RotationTradingBackend::is_eod(int current_time_minutes) const {
    return current_time_minutes >= config_.rotation_config.eod_exit_time_minutes;
}

bool RotationTradingBackend::liquidate_all_positions(const std::string& reason) {
    auto positions = rotation_manager_->get_positions();

    utils::log_info("[EOD] Liquidating " + std::to_string(positions.size()) +
                   " positions. Reason: " + reason);
    utils::log_info("[EOD] Cash before: $" + std::to_string(current_cash_) +
                   ", Allocated: $" + std::to_string(allocated_capital_));

    for (const auto& [symbol, position] : positions) {
        // Get tracking info for logging
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_.at(symbol);
            double entry_cost = position_entry_costs_.at(symbol);
            double exit_value = shares * position.current_price;
            double realized_pnl = exit_value - entry_cost;

            utils::log_info("[EOD] Liquidating " + symbol + ": " +
                          std::to_string(shares) + " shares @ $" +
                          std::to_string(position.current_price) +
                          ", proceeds=$" + std::to_string(exit_value) +
                          ", P&L=$" + std::to_string(realized_pnl) +
                          " (" + std::to_string(realized_pnl / entry_cost * 100.0) + "%)");
        }

        // Create EOD exit decision
        RotationPositionManager::PositionDecision decision;
        decision.symbol = symbol;
        decision.decision = RotationPositionManager::Decision::EOD_EXIT;
        decision.position = position;
        decision.reason = reason;

        // Execute (this handles all accounting via execute_decision)
        execute_decision(decision);
    }

    utils::log_info("[EOD] Liquidation complete. Final cash: $" +
                   std::to_string(current_cash_) +
                   ", Final allocated: $" + std::to_string(allocated_capital_));

    // Verify accounting - allocated should be 0 or near-0 after liquidation
    if (std::abs(allocated_capital_) > 0.01) {
        utils::log_error("[EOD] WARNING: Allocated capital should be ~0 but is $" +
                        std::to_string(allocated_capital_) +
                        " after liquidation!");
    }

    return true;
}

// === State Access ===

bool RotationTradingBackend::is_ready() const {
    return oes_manager_->all_ready();
}

double RotationTradingBackend::get_current_equity() const {
    // CRITICAL FIX: Calculate proper unrealized P&L using tracked positions
    double unrealized_pnl = 0.0;
    auto positions = rotation_manager_->get_positions();

    for (const auto& [symbol, position] : positions) {
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_.at(symbol);
            double entry_cost = position_entry_costs_.at(symbol);
            double current_value = shares * position.current_price;
            unrealized_pnl += (current_value - entry_cost);
        }
    }

    // CRITICAL FIX: Include allocated_capital_ which represents entry costs of positions
    return current_cash_ + allocated_capital_ + unrealized_pnl;
}

void RotationTradingBackend::update_config(const Config& new_config) {
    config_ = new_config;

    // Update component configs
    oes_manager_->update_config(new_config.oes_config);
    signal_aggregator_->update_config(new_config.aggregator_config);
    rotation_manager_->update_config(new_config.rotation_config);
}

// === Private Methods ===

std::map<std::string, SignalOutput> RotationTradingBackend::generate_signals() {
    return oes_manager_->generate_all_signals();
}

std::vector<SignalAggregator::RankedSignal> RotationTradingBackend::rank_signals(
    const std::map<std::string, SignalOutput>& signals
) {
    // Get staleness weights from data manager
    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> staleness_weights;

    for (const auto& [symbol, snap] : snapshot.snapshots) {
        staleness_weights[symbol] = snap.staleness_weight;
    }

    return signal_aggregator_->rank_signals(signals, staleness_weights);
}

std::vector<RotationPositionManager::PositionDecision>
RotationTradingBackend::make_decisions(
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals
) {
    // Get current prices
    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> current_prices;

    // FIX 1: Diagnostic logging to identify data synchronization issues
    static int call_count = 0;
    if (call_count++ % 100 == 0) {  // Log every 100 calls to avoid spam
        utils::log_info("[DEBUG] make_decisions() call #" + std::to_string(call_count) +
                       ": Snapshot has " + std::to_string(snapshot.snapshots.size()) + " symbols");
    }

    for (const auto& [symbol, snap] : snapshot.snapshots) {
        current_prices[symbol] = snap.latest_bar.close;

        if (call_count % 100 == 0) {
            utils::log_info("[DEBUG]   " + symbol + " price: " +
                           std::to_string(snap.latest_bar.close) +
                           " (bar_id: " + std::to_string(snap.latest_bar.bar_id) + ")");
        }
    }

    if (current_prices.empty()) {
        utils::log_error("[CRITICAL] No current prices available for position decisions!");
        utils::log_error("  Snapshot size: " + std::to_string(snapshot.snapshots.size()));
        utils::log_error("  Data manager appears to have no data");
    }

    int current_time_minutes = get_current_time_minutes();

    return rotation_manager_->make_decisions(
        ranked_signals,
        current_prices,
        current_time_minutes
    );
}

bool RotationTradingBackend::execute_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!config_.enable_trading) {
        // Dry run mode - just log
        utils::log_info("[DRY RUN] " + decision.symbol + ": " +
                       std::to_string(static_cast<int>(decision.decision)));
        return true;
    }

    // WARMUP FIX: Skip trade execution during warmup phase
    if (is_warmup_) {
        utils::log_info("[WARMUP] Skipping trade execution for " + decision.symbol +
                       " (warmup mode active)");
        return true;  // Return success but don't execute
    }

    // CRITICAL FIX: Circuit breaker - block new entries if triggered
    bool is_entry = (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
                     decision.decision == RotationPositionManager::Decision::ENTER_SHORT);

    if (circuit_breaker_triggered_ && is_entry) {
        utils::log_warning("[CIRCUIT BREAKER] Blocking new entry for " + decision.symbol +
                          " - circuit breaker active due to large losses");
        return false;  // Block entry
    }

    // Get execution price
    std::string side = (decision.decision == RotationPositionManager::Decision::ENTER_LONG) ?
                       "BUY" : "SELL";
    double execution_price = get_execution_price(decision.symbol, side);

    // Calculate position size
    int shares = 0;
    double position_cost = 0.0;

    if (is_entry) {

        shares = calculate_position_size(decision);

        if (shares == 0) {
            utils::log_warning("Position size is 0 for " + decision.symbol + " - skipping");
            return false;
        }

        // CRITICAL FIX: Validate we have sufficient cash BEFORE proceeding
        position_cost = shares * execution_price;

        if (position_cost > current_cash_) {
            utils::log_error("INSUFFICIENT FUNDS: Need $" + std::to_string(position_cost) +
                           " but only have $" + std::to_string(current_cash_) +
                           " for " + decision.symbol);
            return false;
        }

        // PRE-DEDUCT cash to prevent over-allocation race condition
        current_cash_ -= position_cost;
        utils::log_info("Pre-deducted $" + std::to_string(position_cost) +
                       " for " + decision.symbol +
                       " (remaining cash: $" + std::to_string(current_cash_) + ")");

    }

    // Execute with rotation manager
    bool success = rotation_manager_->execute_decision(decision, execution_price);

    // Variables for tracking realized P&L (for EXIT trades)
    double realized_pnl = std::numeric_limits<double>::quiet_NaN();
    double realized_pnl_pct = std::numeric_limits<double>::quiet_NaN();

    if (success) {
        if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
            decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
            // Cash already deducted above, track allocated capital
            allocated_capital_ += position_cost;

            // CRITICAL FIX: Track entry cost and shares for this position
            position_entry_costs_[decision.symbol] = position_cost;
            position_shares_[decision.symbol] = shares;

            session_stats_.positions_opened++;
            session_stats_.trades_executed++;

            utils::log_info("Entry: allocated $" + std::to_string(position_cost) +
                          " for " + decision.symbol + " (" + std::to_string(shares) + " shares)");

            // Validate total capital
            double total_capital = current_cash_ + allocated_capital_;
            if (std::abs(total_capital - config_.starting_capital) > 1.0) {
                utils::log_warning("Capital tracking error: cash=$" +
                                 std::to_string(current_cash_) +
                                 ", allocated=$" + std::to_string(allocated_capital_) +
                                 ", total=$" + std::to_string(total_capital) +
                                 ", expected=$" + std::to_string(config_.starting_capital));
            }
        } else {
            // Exit - return cash and release allocated capital
            // CRITICAL FIX: Use tracked entry cost and shares
            if (position_entry_costs_.count(decision.symbol) == 0) {
                utils::log_error("CRITICAL: No entry cost tracked for " + decision.symbol);
                return false;
            }

            double entry_cost = position_entry_costs_[decision.symbol];
            int exit_shares = position_shares_[decision.symbol];
            double exit_value = exit_shares * execution_price;

            current_cash_ += exit_value;
            allocated_capital_ -= entry_cost;  // Remove the original allocation

            // Remove from tracking maps
            position_entry_costs_.erase(decision.symbol);
            position_shares_.erase(decision.symbol);

            session_stats_.positions_closed++;
            session_stats_.trades_executed++;

            // Calculate realized P&L for this exit
            realized_pnl = exit_value - entry_cost;
            realized_pnl_pct = realized_pnl / entry_cost;

            // Update trading monitor with trade result
            bool is_win = (realized_pnl > 0.0);
            trading_monitor_.update_trade_result(is_win, realized_pnl);

            utils::log_info("Exit: " + decision.symbol +
                          " - entry_cost=$" + std::to_string(entry_cost) +
                          ", exit_value=$" + std::to_string(exit_value) +
                          ", realized_pnl=$" + std::to_string(realized_pnl) +
                          " (" + std::to_string(realized_pnl_pct * 100.0) + "%)");

            // Track realized P&L for learning
            realized_pnls_[decision.symbol] = realized_pnl;

            // Track trade history for adaptive volatility adjustment (last 2 trades)
            TradeHistory trade_record;
            trade_record.pnl_pct = realized_pnl_pct;
            trade_record.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            auto& history = symbol_trade_history_[decision.symbol];
            history.push_back(trade_record);

            // Keep only last 2 trades
            if (history.size() > 2) {
                history.pop_front();
            }

            // Validate total capital
            double total_capital = current_cash_ + allocated_capital_;
            if (std::abs(total_capital - config_.starting_capital) > 1.0) {
                utils::log_warning("Capital tracking error after exit: cash=$" +
                                 std::to_string(current_cash_) +
                                 ", allocated=$" + std::to_string(allocated_capital_) +
                                 ", total=$" + std::to_string(total_capital) +
                                 ", expected=$" + std::to_string(config_.starting_capital));
            }

            // Update shares for logging
            shares = exit_shares;
        }

        // Track rotations
        if (decision.decision == RotationPositionManager::Decision::ROTATE_OUT) {
            session_stats_.rotations++;
        }

        // Log trade (with actual realized P&L for exits)
        log_trade(decision, execution_price, shares, realized_pnl, realized_pnl_pct);
    } else {
        // ROLLBACK on failure for entry positions
        if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
            decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
            current_cash_ += position_cost;  // Restore cash
            utils::log_error("Failed to execute " + decision.symbol +
                           " - rolled back $" + std::to_string(position_cost) +
                           " (cash now: $" + std::to_string(current_cash_) + ")");
        }
    }

    return success;
}

double RotationTradingBackend::get_execution_price(
    const std::string& symbol,
    const std::string& side
) {
    auto snapshot = data_manager_->get_latest_snapshot();

    if (snapshot.snapshots.count(symbol) == 0) {
        utils::log_error("CRITICAL: No data for " + symbol + " - cannot get price");
        // Return last known price if available, or throw
        if (rotation_manager_->has_position(symbol)) {
            auto& positions = rotation_manager_->get_positions();
            return positions.at(symbol).current_price;
        }
        throw std::runtime_error("No price available for " + symbol);
    }

    double price = snapshot.snapshots.at(symbol).latest_bar.close;
    if (price <= 0.0) {
        throw std::runtime_error("Invalid price for " + symbol + ": " + std::to_string(price));
    }

    return price;
}

int RotationTradingBackend::calculate_position_size(
    const RotationPositionManager::PositionDecision& decision
) {
    // CRITICAL FIX: Use current equity (not starting capital) to prevent over-allocation
    // This adapts position sizing to account for current P&L
    double current_equity = current_cash_ + allocated_capital_;
    int max_positions = config_.rotation_config.max_positions;
    double base_allocation = (current_equity * 0.95) / max_positions;

    // ADAPTIVE Volatility-adjusted position sizing
    // Get realized volatility from the feature engine for this symbol
    auto* oes_instance = oes_manager_->get_oes_instance(decision.symbol);
    double volatility = 0.0;
    if (oes_instance && oes_instance->get_feature_engine()) {
        volatility = oes_instance->get_feature_engine()->get_realized_volatility(20);
    }

    // Check past 2 trades performance to determine if volatility is helping or hurting
    double volatility_weight = 1.0;
    std::string adjustment_reason = "no_history";

    if (symbol_trade_history_.count(decision.symbol) > 0) {
        const auto& history = symbol_trade_history_.at(decision.symbol);

        if (history.size() >= 2) {
            // Have 2 trades - check if both winning, both losing, or mixed
            bool trade1_win = (history[0].pnl_pct > 0.0);
            bool trade2_win = (history[1].pnl_pct > 0.0);

            if (trade1_win && trade2_win) {
                // Both trades won - volatility is helping us!
                // INCREASE position aggressively when winning
                double avg_pnl = (history[0].pnl_pct + history[1].pnl_pct) / 2.0;
                if (avg_pnl > 0.03) {  // Average > 3% gain - strong winners
                    volatility_weight = 1.5;  // AGGRESSIVE increase
                    adjustment_reason = "both_wins_strong";
                } else if (avg_pnl > 0.01) {  // Average > 1% gain
                    volatility_weight = 1.3;  // Moderate increase
                    adjustment_reason = "both_wins_moderate";
                } else {
                    volatility_weight = 1.15;  // Slight increase even for small wins
                    adjustment_reason = "both_wins";
                }
            } else if (!trade1_win && !trade2_win) {
                // Both trades lost - volatility is hurting us!
                // Apply VERY aggressive inverse volatility reduction
                if (volatility > 0.0) {
                    const double baseline_vol = 0.01;  // VERY low baseline for extreme reduction
                    volatility_weight = baseline_vol / (volatility + baseline_vol);
                    volatility_weight = std::max(0.3, std::min(0.9, volatility_weight));  // Clamp [0.3, 0.9]
                    adjustment_reason = "both_losses";
                } else {
                    volatility_weight = 0.7;  // Reduce even with no volatility data
                    adjustment_reason = "both_losses_no_vol";
                }
            } else {
                // Mixed results (1 win, 1 loss) - stay neutral or slight reduction
                volatility_weight = 0.95;  // Very slight reduction
                adjustment_reason = "mixed";
            }
        } else if (history.size() == 1) {
            // Only 1 trade - use it as a signal and react quickly
            bool trade_win = (history[0].pnl_pct > 0.0);
            if (trade_win) {
                // React faster to wins - increase position after just 1 win
                if (history[0].pnl_pct > 0.03) {
                    volatility_weight = 1.4;  // Strong win -> aggressive increase
                    adjustment_reason = "one_win_strong";
                } else if (history[0].pnl_pct > 0.015) {
                    volatility_weight = 1.25;  // Good win -> moderate increase
                    adjustment_reason = "one_win_good";
                } else {
                    volatility_weight = 1.15;  // Small win -> slight increase
                    adjustment_reason = "one_win";
                }
            } else {
                // React to losses with reduction
                if (volatility > 0.0) {
                    const double baseline_vol = 0.015;
                    volatility_weight = baseline_vol / (volatility + baseline_vol);
                    volatility_weight = std::max(0.6, std::min(1.0, volatility_weight));  // Clamp [0.6, 1.0]
                    adjustment_reason = "one_loss";
                } else {
                    volatility_weight = 0.85;  // Reduce even without volatility data
                    adjustment_reason = "one_loss_no_vol";
                }
            }
        }
    } else if (volatility > 0.0) {
        // No trade history - use standard inverse volatility
        const double baseline_vol = 0.02;
        volatility_weight = baseline_vol / (volatility + baseline_vol);
        volatility_weight = std::max(0.7, std::min(1.3, volatility_weight));  // Conservative clamp
        adjustment_reason = "no_history";
    }

    // Apply volatility weight to allocation
    double fixed_allocation = base_allocation * volatility_weight;

    // Log volatility adjustment with reasoning (helps understand position sizing decisions)
    std::cerr << "[ADAPTIVE VOL] " << decision.symbol
              << ": vol=" << (volatility * 100.0) << "%"
              << ", weight=" << volatility_weight
              << ", reason=" << adjustment_reason
              << ", base=$" << base_allocation
              << " → adj=$" << fixed_allocation << std::endl;

    // But still check against available cash
    double available_cash = current_cash_;
    double allocation = std::min(fixed_allocation, available_cash * 0.95);

    if (allocation <= 100.0) {
        utils::log_warning("Insufficient cash for position: $" +
                          std::to_string(available_cash) +
                          " (fixed_alloc=$" + std::to_string(fixed_allocation) + ")");
        return 0;  // Don't trade with less than $100
    }

    // Get execution price
    double price = get_execution_price(decision.symbol, "BUY");
    if (price <= 0) {
        utils::log_error("Invalid price for position sizing: " +
                        std::to_string(price));
        return 0;
    }

    int shares = static_cast<int>(allocation / price);

    // Final validation - ensure position doesn't exceed available cash
    double position_value = shares * price;
    if (position_value > available_cash) {
        shares = static_cast<int>(available_cash / price);
    }

    // Validate we got non-zero shares
    if (shares == 0) {
        utils::log_warning("[POSITION SIZE] Calculated 0 shares for " + decision.symbol +
                          " (fixed_alloc=$" + std::to_string(fixed_allocation) +
                          ", available=$" + std::to_string(available_cash) +
                          ", allocation=$" + std::to_string(allocation) +
                          ", price=$" + std::to_string(price) + ")");

        // Force minimum 1 share if we have enough capital
        if (allocation >= price) {
            utils::log_info("[POSITION SIZE] Forcing minimum 1 share");
            shares = 1;
        } else {
            utils::log_error("[POSITION SIZE] Insufficient capital even for 1 share - skipping");
            return 0;
        }
    }

    utils::log_info("Position sizing for " + decision.symbol +
                   ": fixed_alloc=$" + std::to_string(fixed_allocation) +
                   ", available=$" + std::to_string(available_cash) +
                   ", allocation=$" + std::to_string(allocation) +
                   ", price=$" + std::to_string(price) +
                   ", shares=" + std::to_string(shares) +
                   ", value=$" + std::to_string(shares * price));

    return shares;
}

void RotationTradingBackend::update_learning() {
    // FIX #1: Continuous Learning Feedback
    // Predictor now receives bar-to-bar returns EVERY bar, not just on exits
    // This is critical for learning - predictor needs frequent feedback

    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> bar_returns;

    // Calculate bar-to-bar return for each symbol
    for (const auto& [symbol, snap] : snapshot.snapshots) {
        auto history = data_manager_->get_recent_bars(symbol, 2);
        if (history.size() >= 2) {
            // Return = (current_close - previous_close) / previous_close
            double bar_return = (history[0].close - history[1].close) / history[1].close;
            bar_returns[symbol] = bar_return;
        }
    }

    // Update all predictors with bar-to-bar returns (weight = 1.0)
    if (!bar_returns.empty()) {
        oes_manager_->update_all(bar_returns);
    }

    // ALSO update with realized P&L when positions exit (weight = 10.0)
    // Realized P&L is more important than bar-to-bar noise
    if (!realized_pnls_.empty()) {
        // Scale realized P&L by 10x to give it more weight in learning
        std::map<std::string, double> weighted_pnls;
        for (const auto& [symbol, pnl] : realized_pnls_) {
            // Convert P&L to return percentage
            double return_pct = pnl / config_.starting_capital;
            weighted_pnls[symbol] = return_pct * 10.0;  // 10x weight
        }
        oes_manager_->update_all(weighted_pnls);
        realized_pnls_.clear();
    }
}

void RotationTradingBackend::log_signal(
    const std::string& symbol,
    const SignalOutput& signal
) {
    if (!signal_log_.is_open()) {
        return;
    }

    json j;
    j["timestamp_ms"] = signal.timestamp_ms;
    j["bar_id"] = signal.bar_id;
    j["symbol"] = symbol;
    j["signal"] = static_cast<int>(signal.signal_type);
    j["probability"] = signal.probability;
    j["confidence"] = signal.confidence;

    signal_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!decision_log_.is_open()) {
        return;
    }

    json j;
    j["symbol"] = decision.symbol;
    j["decision"] = static_cast<int>(decision.decision);
    j["reason"] = decision.reason;

    if (decision.decision != RotationPositionManager::Decision::HOLD) {
        j["rank"] = decision.signal.rank;
        j["strength"] = decision.signal.strength;
    }

    decision_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_trade(
    const RotationPositionManager::PositionDecision& decision,
    double execution_price,
    int shares,
    double realized_pnl,
    double realized_pnl_pct
) {
    if (!trade_log_.is_open()) {
        return;
    }

    json j;
    j["timestamp_ms"] = data_manager_->get_latest_snapshot().logical_timestamp_ms;
    j["symbol"] = decision.symbol;
    j["decision"] = static_cast<int>(decision.decision);
    j["exec_price"] = execution_price;
    j["shares"] = shares;
    j["action"] = (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
                   decision.decision == RotationPositionManager::Decision::ENTER_SHORT) ?
                  "ENTRY" : "EXIT";
    j["direction"] = (decision.signal.signal.signal_type == SignalType::LONG) ?
                     "LONG" : "SHORT";
    j["price"] = execution_price;
    j["value"] = execution_price * shares;
    j["reason"] = decision.reason;  // Add reason for entry/exit

    // Add P&L for exits
    if (decision.decision != RotationPositionManager::Decision::ENTER_LONG &&
        decision.decision != RotationPositionManager::Decision::ENTER_SHORT) {
        // CRITICAL FIX: Use actual realized P&L passed from execute_decision (exit_value - entry_cost)
        if (!std::isnan(realized_pnl) && !std::isnan(realized_pnl_pct)) {
            j["pnl"] = realized_pnl;
            j["pnl_pct"] = realized_pnl_pct;
        } else {
            // Fallback to position P&L (should not happen for EXIT trades)
            j["pnl"] = decision.position.pnl * shares;
            j["pnl_pct"] = decision.position.pnl_pct;
        }
        j["bars_held"] = decision.position.bars_held;
    } else {
        // For ENTRY trades, add signal metadata
        j["signal_probability"] = decision.signal.signal.probability;
        j["signal_confidence"] = decision.signal.signal.confidence;
        j["signal_rank"] = decision.signal.rank;
    }

    trade_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_positions() {
    if (!position_log_.is_open()) {
        return;
    }

    json j;
    j["bar"] = session_stats_.bars_processed;
    j["positions"] = json::array();

    for (const auto& [symbol, position] : rotation_manager_->get_positions()) {
        json pos_j;
        pos_j["symbol"] = symbol;
        pos_j["direction"] = (position.direction == SignalType::LONG) ? "LONG" : "SHORT";
        pos_j["entry_price"] = position.entry_price;
        pos_j["current_price"] = position.current_price;
        pos_j["pnl"] = position.pnl;
        pos_j["pnl_pct"] = position.pnl_pct;
        pos_j["bars_held"] = position.bars_held;
        pos_j["current_rank"] = position.current_rank;
        pos_j["current_strength"] = position.current_strength;

        j["positions"].push_back(pos_j);
    }

    j["total_unrealized_pnl"] = rotation_manager_->get_total_unrealized_pnl();
    j["current_equity"] = get_current_equity();

    position_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::update_session_stats() {
    // Calculate current equity using CORRECT formula (cash + allocated + unrealized)
    session_stats_.current_equity = get_current_equity();

    // Track equity curve
    equity_curve_.push_back(session_stats_.current_equity);

    // Update max/min equity
    if (session_stats_.current_equity > session_stats_.max_equity) {
        session_stats_.max_equity = session_stats_.current_equity;
    }
    if (session_stats_.current_equity < session_stats_.min_equity) {
        session_stats_.min_equity = session_stats_.current_equity;
    }

    // Calculate drawdown
    double drawdown = (session_stats_.max_equity - session_stats_.current_equity) /
                     session_stats_.max_equity;
    if (drawdown > session_stats_.max_drawdown) {
        session_stats_.max_drawdown = drawdown;
    }

    // Calculate total P&L from FULL equity (not just cash!)
    session_stats_.total_pnl = session_stats_.current_equity - config_.starting_capital;
    session_stats_.total_pnl_pct = session_stats_.total_pnl / config_.starting_capital;

    // Diagnostic logging every 100 bars
    if (session_stats_.bars_processed % 100 == 0) {
        // Calculate unrealized P&L for logging
        double unrealized_pnl = 0.0;
        auto positions = rotation_manager_->get_positions();
        for (const auto& [symbol, position] : positions) {
            if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
                int shares = position_shares_.at(symbol);
                double entry_cost = position_entry_costs_.at(symbol);
                double current_value = shares * position.current_price;
                unrealized_pnl += (current_value - entry_cost);
            }
        }

        utils::log_info("[STATS] Bar " + std::to_string(session_stats_.bars_processed) +
                       ": Cash=$" + std::to_string(current_cash_) +
                       ", Allocated=$" + std::to_string(allocated_capital_) +
                       ", Unrealized=$" + std::to_string(unrealized_pnl) +
                       ", Equity=$" + std::to_string(session_stats_.current_equity) +
                       ", P&L=" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%");
    }

    // Calculate returns for Sharpe
    if (equity_curve_.size() > 1) {
        double ret = (equity_curve_.back() - equity_curve_[equity_curve_.size() - 2]) /
                     equity_curve_[equity_curve_.size() - 2];
        returns_.push_back(ret);
    }

    // Calculate Sharpe ratio (if enough data)
    if (returns_.size() >= 20) {
        double mean_return = 0.0;
        for (double r : returns_) {
            mean_return += r;
        }
        mean_return /= returns_.size();

        double variance = 0.0;
        for (double r : returns_) {
            variance += (r - mean_return) * (r - mean_return);
        }
        variance /= returns_.size();

        double std_dev = std::sqrt(variance);
        if (std_dev > 0.0) {
            // Annualize: 390 bars per day, ~252 trading days
            session_stats_.sharpe_ratio = (mean_return / std_dev) * std::sqrt(390.0 * 252.0);
        }
    }

    // Calculate MRD (Mean Return per Day)
    // Assume 390 bars per day
    if (session_stats_.bars_processed >= 390) {
        int trading_days = session_stats_.bars_processed / 390;
        session_stats_.mrd = session_stats_.total_pnl_pct / trading_days;
    }
}

int RotationTradingBackend::get_current_time_minutes() const {
    // Calculate minutes since market open (9:30 AM ET)
    // Works for both mock and live modes

    auto snapshot = data_manager_->get_latest_snapshot();
    if (snapshot.snapshots.empty()) {
        return 0;
    }

    // Get first symbol's timestamp
    auto first_snap = snapshot.snapshots.begin()->second;
    int64_t timestamp_ms = first_snap.latest_bar.timestamp_ms;

    // Convert to time-of-day (assuming ET timezone)
    int64_t timestamp_sec = timestamp_ms / 1000;
    std::time_t t = timestamp_sec;
    std::tm* tm_info = std::localtime(&t);

    if (!tm_info) {
        utils::log_error("Failed to convert timestamp to local time");
        return 0;
    }

    // Calculate minutes since market open (9:30 AM)
    int hour = tm_info->tm_hour;
    int minute = tm_info->tm_min;
    int minutes_since_midnight = hour * 60 + minute;
    constexpr int market_open_minutes = 9 * 60 + 30;  // 9:30 AM = 570 minutes
    int minutes_since_open = minutes_since_midnight - market_open_minutes;

    return minutes_since_open;
}

} // namespace sentio

```

## 📄 **FILE 20 of 30**: src/cli/command_registry.cpp

**File Information**:
- **Path**: `src/cli/command_registry.cpp`
- **Size**: 662 lines
- **Modified**: 2025-10-16 06:27:21
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/command_registry.h"
// #include "cli/canonical_commands.h"  // Not implemented yet
// #include "cli/strattest_command.h"    // Not implemented yet
// #include "cli/audit_command.h"        // Not implemented yet
// #include "cli/trade_command.h"        // Not implemented yet
// #include "cli/full_test_command.h"    // Not implemented yet
// #include "cli/sanity_check_command.h" // Not implemented yet
// #include "cli/walk_forward_command.h" // Not implemented yet
// #include "cli/validate_bar_id_command.h" // Not implemented yet
// #include "cli/train_xgb60sa_command.h" // Not implemented yet
// #include "cli/train_xgb8_command.h"   // Not implemented yet
// #include "cli/train_xgb25_command.h"  // Not implemented yet
// #include "cli/online_command.h"  // Commented out - missing implementations
// #include "cli/online_sanity_check_command.h"  // Commented out - missing implementations
// #include "cli/online_trade_command.h"  // Commented out - missing implementations
#include "cli/ensemble_workflow_command.h"
#include "cli/live_trade_command.hpp"
#include "cli/rotation_trade_command.h"
#ifdef XGBOOST_AVAILABLE
#include "cli/train_command.h"
#endif
#ifdef TORCH_AVAILABLE
// PPO training command removed from this project scope
#endif
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace sentio::cli {

// ================================================================================================
// COMMAND REGISTRY IMPLEMENTATION
// ================================================================================================

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry registry;
    return registry;
}

void CommandRegistry::register_command(const std::string& name, 
                                      std::shared_ptr<Command> command,
                                      const CommandInfo& info) {
    CommandInfo cmd_info = info;
    cmd_info.command = command;
    if (cmd_info.description.empty()) {
        cmd_info.description = command->get_description();
    }
    
    commands_[name] = cmd_info;
    
    // Register aliases
    for (const auto& alias : cmd_info.aliases) {
        AliasInfo alias_info;
        alias_info.target_command = name;
        aliases_[alias] = alias_info;
    }
}

void CommandRegistry::register_alias(const std::string& alias, 
                                    const std::string& target_command,
                                    const AliasInfo& info) {
    AliasInfo alias_info = info;
    alias_info.target_command = target_command;
    aliases_[alias] = alias_info;
}

void CommandRegistry::deprecate_command(const std::string& name, 
                                       const std::string& replacement,
                                       const std::string& message) {
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        it->second.deprecated = true;
        it->second.replacement_command = replacement;
        it->second.deprecation_message = message.empty() ? 
            "This command is deprecated. Use '" + replacement + "' instead." : message;
    }
}

std::shared_ptr<Command> CommandRegistry::get_command(const std::string& name) {
    // Check direct command first
    auto cmd_it = commands_.find(name);
    if (cmd_it != commands_.end()) {
        if (cmd_it->second.deprecated) {
            show_deprecation_warning(name, cmd_it->second);
        }
        return cmd_it->second.command;
    }
    
    // Check aliases
    auto alias_it = aliases_.find(name);
    if (alias_it != aliases_.end()) {
        if (alias_it->second.deprecated) {
            show_alias_warning(name, alias_it->second);
        }
        
        auto target_it = commands_.find(alias_it->second.target_command);
        if (target_it != commands_.end()) {
            return target_it->second.command;
        }
    }
    
    return nullptr;
}

bool CommandRegistry::has_command(const std::string& name) const {
    return commands_.find(name) != commands_.end() || 
           aliases_.find(name) != aliases_.end();
}

std::vector<std::string> CommandRegistry::get_available_commands() const {
    std::vector<std::string> commands;
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            commands.push_back(name);
        }
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

std::vector<std::string> CommandRegistry::get_commands_by_category(const std::string& category) const {
    std::vector<std::string> commands;
    for (const auto& [name, info] : commands_) {
        if (info.category == category && !info.deprecated) {
            commands.push_back(name);
        }
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

const CommandRegistry::CommandInfo* CommandRegistry::get_command_info(const std::string& name) const {
    auto it = commands_.find(name);
    return (it != commands_.end()) ? &it->second : nullptr;
}

void CommandRegistry::show_help() const {
    std::cout << "Sentio CLI - Advanced Trading System Command Line Interface\n\n";
    std::cout << "Usage: sentio_cli <command> [options]\n\n";
    
    // Group commands by category
    std::map<std::string, std::vector<std::string>> categories;
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            categories[info.category].push_back(name);
        }
    }
    
    // Show each category
    for (const auto& [category, commands] : categories) {
        std::cout << category << " Commands:\n";
        for (const auto& cmd : commands) {
            const auto& info = commands_.at(cmd);
            std::cout << "  " << std::left << std::setw(15) << cmd 
                     << info.description << "\n";
        }
        std::cout << "\n";
    }
    
    std::cout << "Global Options:\n";
    std::cout << "  --help, -h         Show this help message\n";
    std::cout << "  --version, -v      Show version information\n\n";
    
    std::cout << "Use 'sentio_cli <command> --help' for detailed command help.\n";
    std::cout << "Use 'sentio_cli --migration' to see deprecated command alternatives.\n\n";
    
    EnhancedCommandDispatcher::show_usage_examples();
}

void CommandRegistry::show_category_help(const std::string& category) const {
    auto commands = get_commands_by_category(category);
    if (commands.empty()) {
        std::cout << "No commands found in category: " << category << "\n";
        return;
    }
    
    std::cout << category << " Commands:\n\n";
    for (const auto& cmd : commands) {
        const auto& info = commands_.at(cmd);
        std::cout << "  " << cmd << " - " << info.description << "\n";
        
        if (!info.aliases.empty()) {
            std::cout << "    Aliases: " << format_command_list(info.aliases) << "\n";
        }
        
        if (!info.tags.empty()) {
            std::cout << "    Tags: " << format_command_list(info.tags) << "\n";
        }
        std::cout << "\n";
    }
}

void CommandRegistry::show_migration_guide() const {
    std::cout << "Migration Guide - Deprecated Commands\n";
    std::cout << "=====================================\n\n";
    
    bool has_deprecated = false;
    
    for (const auto& [name, info] : commands_) {
        if (info.deprecated) {
            has_deprecated = true;
            std::cout << "❌ " << name << " (deprecated)\n";
            std::cout << "   " << info.deprecation_message << "\n";
            if (!info.replacement_command.empty()) {
                std::cout << "   ✅ Use instead: " << info.replacement_command << "\n";
            }
            std::cout << "\n";
        }
    }
    
    for (const auto& [alias, info] : aliases_) {
        if (info.deprecated) {
            has_deprecated = true;
            std::cout << "⚠️  " << alias << " (deprecated alias)\n";
            std::cout << "   " << info.deprecation_message << "\n";
            std::cout << "   ✅ Use instead: " << info.target_command << "\n";
            if (!info.migration_guide.empty()) {
                std::cout << "   📖 Migration: " << info.migration_guide << "\n";
            }
            std::cout << "\n";
        }
    }
    
    if (!has_deprecated) {
        std::cout << "✅ No deprecated commands or aliases found.\n";
        std::cout << "All commands are up-to-date!\n";
    }
}

int CommandRegistry::execute_command(const std::string& name, const std::vector<std::string>& args) {
    auto command = get_command(name);
    if (!command) {
        std::cerr << "❌ Unknown command: " << name << "\n\n";
        
        auto suggestions = suggest_commands(name);
        if (!suggestions.empty()) {
            std::cerr << "💡 Did you mean:\n";
            for (const auto& suggestion : suggestions) {
                std::cerr << "  " << suggestion << "\n";
            }
            std::cerr << "\n";
        }
        
        std::cerr << "Use 'sentio_cli --help' to see available commands.\n";
        return 1;
    }
    
    try {
        return command->execute(args);
    } catch (const std::exception& e) {
        std::cerr << "❌ Command execution failed: " << e.what() << "\n";
        return 1;
    }
}

std::vector<std::string> CommandRegistry::suggest_commands(const std::string& input) const {
    std::vector<std::pair<std::string, int>> candidates;
    
    // Check all commands and aliases
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            int distance = levenshtein_distance(input, name);
            if (distance <= 2 && distance < static_cast<int>(name.length())) {
                candidates.emplace_back(name, distance);
            }
        }
    }
    
    for (const auto& [alias, info] : aliases_) {
        if (!info.deprecated) {
            int distance = levenshtein_distance(input, alias);
            if (distance <= 2 && distance < static_cast<int>(alias.length())) {
                candidates.emplace_back(alias, distance);
            }
        }
    }
    
    // Sort by distance and return top suggestions
    std::sort(candidates.begin(), candidates.end(), 
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    std::vector<std::string> suggestions;
    for (size_t i = 0; i < std::min(size_t(3), candidates.size()); ++i) {
        suggestions.push_back(candidates[i].first);
    }
    
    return suggestions;
}

void CommandRegistry::initialize_default_commands() {
    // Canonical commands and legacy commands commented out - not implemented yet
    // TODO: Implement these commands when needed

    /* COMMENTED OUT - NOT IMPLEMENTED YET
    // Register canonical commands (new interface)
    CommandInfo generate_info;
    generate_info.category = "Signal Generation";
    generate_info.version = "2.0";
    generate_info.description = "Generate trading signals (canonical interface)";
    generate_info.tags = {"signals", "generation", "canonical"};
    register_command("generate", std::make_shared<GenerateCommand>(), generate_info);

    CommandInfo analyze_info;
    analyze_info.category = "Performance Analysis";
    analyze_info.version = "2.0";
    analyze_info.description = "Analyze trading performance (canonical interface)";
    analyze_info.tags = {"analysis", "performance", "canonical"};
    register_command("analyze", std::make_shared<AnalyzeCommand>(), analyze_info);

    CommandInfo execute_info;
    execute_info.category = "Trade Execution";
    execute_info.version = "2.0";
    execute_info.description = "Execute trades from signals (canonical interface)";
    execute_info.tags = {"trading", "execution", "canonical"};
    register_command("execute", std::make_shared<TradeCanonicalCommand>(), execute_info);

    CommandInfo pipeline_info;
    pipeline_info.category = "Workflows";
    pipeline_info.version = "2.0";
    pipeline_info.description = "Run multi-step trading workflows";
    pipeline_info.tags = {"workflow", "automation", "canonical"};
    register_command("pipeline", std::make_shared<PipelineCommand>(), pipeline_info);

    // Register legacy commands (backward compatibility)
    CommandInfo strattest_info;
    strattest_info.category = "Legacy";
    strattest_info.version = "1.0";
    strattest_info.description = "Generate trading signals (legacy interface)";
    strattest_info.deprecated = false;  // Keep for now
    strattest_info.tags = {"signals", "legacy"};
    register_command("strattest", std::make_shared<StrattestCommand>(), strattest_info);

    CommandInfo audit_info;
    audit_info.category = "Legacy";
    audit_info.version = "1.0";
    audit_info.description = "Analyze performance with reports (legacy interface)";
    audit_info.deprecated = false;  // Keep for now
    audit_info.tags = {"analysis", "legacy"};
    register_command("audit", std::make_shared<AuditCommand>(), audit_info);
    END OF COMMENTED OUT SECTION */

    // All legacy and canonical commands commented out above - not implemented yet

    // Register OnlineEnsemble workflow commands
    CommandInfo generate_signals_info;
    generate_signals_info.category = "OnlineEnsemble Workflow";
    generate_signals_info.version = "1.0";
    generate_signals_info.description = "Generate trading signals using OnlineEnsemble strategy";
    generate_signals_info.tags = {"ensemble", "signals", "online-learning"};
    register_command("generate-signals", std::make_shared<GenerateSignalsCommand>(), generate_signals_info);

    CommandInfo execute_trades_info;
    execute_trades_info.category = "OnlineEnsemble Workflow";
    execute_trades_info.version = "1.0";
    execute_trades_info.description = "Execute trades from signals with Kelly sizing";
    execute_trades_info.tags = {"ensemble", "trading", "kelly", "portfolio"};
    register_command("execute-trades", std::make_shared<ExecuteTradesCommand>(), execute_trades_info);

    CommandInfo analyze_trades_info;
    analyze_trades_info.category = "OnlineEnsemble Workflow";
    analyze_trades_info.version = "1.0";
    analyze_trades_info.description = "Analyze trade performance and generate reports";
    analyze_trades_info.tags = {"ensemble", "analysis", "metrics", "reporting"};
    register_command("analyze-trades", std::make_shared<AnalyzeTradesCommand>(), analyze_trades_info);

    // Register live trading command
    CommandInfo live_trade_info;
    live_trade_info.category = "Live Trading";
    live_trade_info.version = "1.0";
    live_trade_info.description = "Run OnlineTrader v1.0 with paper account (SPY/SPXL/SH/SDS)";
    live_trade_info.tags = {"live", "paper-trading", "alpaca", "polygon"};
    register_command("live-trade", std::make_shared<LiveTradeCommand>(), live_trade_info);

    // Register rotation/mock trading command
    CommandInfo mock_trade_info;
    mock_trade_info.category = "Live Trading";
    mock_trade_info.version = "1.0";
    mock_trade_info.description = "Run multi-symbol rotation trading (mock/backtest mode)";
    mock_trade_info.tags = {"mock", "rotation", "multi-symbol", "backtest"};
    register_command("mock", std::make_shared<RotationTradeCommand>(), mock_trade_info);

    // Register training commands if available
// XGBoost training now handled by Python scripts (tools/train_xgboost_binary.py)
// C++ train command disabled

#ifdef TORCH_AVAILABLE
    // PPO training command intentionally removed
#endif
}

void CommandRegistry::setup_canonical_aliases() {
    // Canonical command aliases commented out - canonical commands not implemented yet
    /* COMMENTED OUT - CANONICAL COMMANDS NOT IMPLEMENTED
    // Setup helpful aliases for canonical commands
    AliasInfo gen_alias;
    gen_alias.target_command = "generate";
    gen_alias.migration_guide = "Use 'generate' instead of 'strattest' for consistent interface";
    register_alias("gen", "generate", gen_alias);

    AliasInfo report_alias;
    report_alias.target_command = "analyze";
    report_alias.migration_guide = "Use 'analyze report' instead of 'audit report'";
    register_alias("report", "analyze", report_alias);

    AliasInfo run_alias;
    run_alias.target_command = "execute";
    register_alias("run", "execute", run_alias);

    // Deprecate old patterns
    AliasInfo strattest_alias;
    strattest_alias.target_command = "generate";
    strattest_alias.deprecated = true;
    strattest_alias.deprecation_message = "The 'strattest' command interface is being replaced";
    strattest_alias.migration_guide = "Use 'generate --strategy <name> --data <path>' for the new canonical interface";
    // Don't register as alias yet - keep original command for compatibility
    */
}

// ================================================================================================
// PRIVATE HELPER METHODS
// ================================================================================================

void CommandRegistry::show_deprecation_warning(const std::string& command_name, const CommandInfo& info) {
    std::cerr << "⚠️  WARNING: Command '" << command_name << "' is deprecated.\n";
    std::cerr << "   " << info.deprecation_message << "\n";
    if (!info.replacement_command.empty()) {
        std::cerr << "   Use '" << info.replacement_command << "' instead.\n";
    }
    std::cerr << "\n";
}

void CommandRegistry::show_alias_warning(const std::string& alias, const AliasInfo& info) {
    std::cerr << "⚠️  WARNING: Alias '" << alias << "' is deprecated.\n";
    std::cerr << "   " << info.deprecation_message << "\n";
    std::cerr << "   Use '" << info.target_command << "' instead.\n";
    if (!info.migration_guide.empty()) {
        std::cerr << "   Migration: " << info.migration_guide << "\n";
    }
    std::cerr << "\n";
}

std::string CommandRegistry::format_command_list(const std::vector<std::string>& commands) const {
    std::ostringstream ss;
    for (size_t i = 0; i < commands.size(); ++i) {
        ss << commands[i];
        if (i < commands.size() - 1) ss << ", ";
    }
    return ss.str();
}

int CommandRegistry::levenshtein_distance(const std::string& s1, const std::string& s2) const {
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();
    
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));
    
    for (size_t i = 0; i <= len1; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j) dp[0][j] = static_cast<int>(j);
    
    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }
    
    return dp[len1][len2];
}

// ================================================================================================
// ENHANCED COMMAND DISPATCHER IMPLEMENTATION
// ================================================================================================

int EnhancedCommandDispatcher::execute(int argc, char** argv) {
    if (argc < 2) {
        show_help();
        return 1;
    }
    
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    
    // Handle global flags
    if (handle_global_flags(args)) {
        return 0;
    }
    
    std::string command_name = argv[1];
    
    // Handle special cases
    if (command_name == "--help" || command_name == "-h") {
        show_help();
        return 0;
    }
    
    if (command_name == "--version" || command_name == "-v") {
        show_version();
        return 0;
    }
    
    if (command_name == "--migration") {
        CommandRegistry::instance().show_migration_guide();
        return 0;
    }
    
    // Execute command through registry
    auto& registry = CommandRegistry::instance();
    return registry.execute_command(command_name, args);
}

void EnhancedCommandDispatcher::show_help() {
    CommandRegistry::instance().show_help();
}

void EnhancedCommandDispatcher::show_version() {
    std::cout << "Sentio CLI " << get_version_string() << "\n";
    std::cout << "Advanced Trading System Command Line Interface\n";
    std::cout << "Copyright (c) 2024 Sentio Trading Systems\n\n";
    
    std::cout << "Features:\n";
    std::cout << "  • Multi-strategy signal generation (SGO, AWR, XGBoost, CatBoost)\n";
    std::cout << "  • Advanced portfolio management with leverage\n";
    std::cout << "  • Comprehensive performance analysis\n";
    std::cout << "  • Automated trading workflows\n";
    std::cout << "  • Machine learning model training (Python-side for XGB/CTB)\n\n";
    
    std::cout << "Build Information:\n";
#ifdef TORCH_AVAILABLE
    std::cout << "  • PyTorch/LibTorch: Enabled\n";
#else
    std::cout << "  • PyTorch/LibTorch: Disabled\n";
#endif
#ifdef XGBOOST_AVAILABLE
    std::cout << "  • XGBoost: Enabled\n";
#else
    std::cout << "  • XGBoost: Disabled\n";
#endif
    std::cout << "  • Compiler: " << __VERSION__ << "\n";
    std::cout << "  • Build Date: " << __DATE__ << " " << __TIME__ << "\n";
}

bool EnhancedCommandDispatcher::handle_global_flags(const std::vector<std::string>& args) {
    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
            show_help();
            return true;
        }
        if (arg == "--version" || arg == "-v") {
            show_version();
            return true;
        }
        if (arg == "--migration") {
            CommandRegistry::instance().show_migration_guide();
            return true;
        }
    }
    return false;
}

void EnhancedCommandDispatcher::show_command_not_found_help(const std::string& command_name) {
    std::cerr << "Command '" << command_name << "' not found.\n\n";
    
    auto& registry = CommandRegistry::instance();
    auto suggestions = registry.suggest_commands(command_name);
    
    if (!suggestions.empty()) {
        std::cerr << "Did you mean:\n";
        for (const auto& suggestion : suggestions) {
            std::cerr << "  " << suggestion << "\n";
        }
        std::cerr << "\n";
    }
    
    std::cerr << "Use 'sentio_cli --help' to see all available commands.\n";
}

void EnhancedCommandDispatcher::show_usage_examples() {
    std::cout << "Common Usage Examples:\n";
    std::cout << "======================\n\n";
    
    std::cout << "Signal Generation:\n";
    std::cout << "  sentio_cli generate --strategy sgo --data data/equities/QQQ_RTH_NH.csv\n\n";
    
    std::cout << "Performance Analysis:\n";
    std::cout << "  sentio_cli analyze summary --signals data/signals/sgo-timestamp.jsonl\n\n";
    
    std::cout << "Automated Workflows:\n";
    std::cout << "  sentio_cli pipeline backtest --strategy sgo --blocks 20\n";
    std::cout << "  sentio_cli pipeline compare --strategies \"sgo,xgb,ctb\" --blocks 20\n\n";
    
    std::cout << "Legacy Commands (still supported):\n";
    std::cout << "  sentio_cli strattest --strategy sgo --blocks 20\n";
    std::cout << "  sentio_cli audit report --signals data/signals/sgo-timestamp.jsonl\n\n";
}

std::string EnhancedCommandDispatcher::get_version_string() {
    return "2.0.0-beta";  // Update as needed
}

// ================================================================================================
// COMMAND FACTORY IMPLEMENTATION
// ================================================================================================

std::map<std::string, CommandFactory::CommandCreator> CommandFactory::factories_;

void CommandFactory::register_factory(const std::string& name, CommandCreator creator) {
    factories_[name] = creator;
}

std::shared_ptr<Command> CommandFactory::create_command(const std::string& name) {
    auto it = factories_.find(name);
    if (it != factories_.end()) {
        return it->second();
    }
    return nullptr;
}

void CommandFactory::register_builtin_commands() {
    // Canonical commands and legacy commands not implemented - commented out
    /* COMMENTED OUT - NOT IMPLEMENTED
    // Register factory functions for lazy loading
    register_factory("generate", []() { return std::make_shared<GenerateCommand>(); });
    register_factory("analyze", []() { return std::make_shared<AnalyzeCommand>(); });
    register_factory("execute", []() { return std::make_shared<TradeCanonicalCommand>(); });
    register_factory("pipeline", []() { return std::make_shared<PipelineCommand>(); });

    register_factory("strattest", []() { return std::make_shared<StrattestCommand>(); });
    register_factory("audit", []() { return std::make_shared<AuditCommand>(); });
    register_factory("trade", []() { return std::make_shared<TradeCommand>(); });
    register_factory("full-test", []() { return std::make_shared<FullTestCommand>(); });
    */

    // Online learning strategies - commented out (missing implementations)
    // register_factory("online", []() { return std::make_shared<OnlineCommand>(); });
    // register_factory("online-sanity", []() { return std::make_shared<OnlineSanityCheckCommand>(); });
    // register_factory("online-trade", []() { return std::make_shared<OnlineTradeCommand>(); });

    // OnlineEnsemble workflow commands
    register_factory("generate-signals", []() { return std::make_shared<GenerateSignalsCommand>(); });
    register_factory("execute-trades", []() { return std::make_shared<ExecuteTradesCommand>(); });
    register_factory("analyze-trades", []() { return std::make_shared<AnalyzeTradesCommand>(); });

    // Live trading command
    register_factory("live-trade", []() { return std::make_shared<LiveTradeCommand>(); });

    // Rotation/mock trading command
    register_factory("mock", []() { return std::make_shared<RotationTradeCommand>(); });

// XGBoost training now handled by Python scripts

#ifdef TORCH_AVAILABLE
    register_factory("train_ppo", []() { return std::make_shared<TrainPpoCommand>(); });
#endif
}

} // namespace sentio::cli

```

## 📄 **FILE 21 of 30**: src/cli/rotation_trade_command.cpp

**File Information**:
- **Path**: `src/cli/rotation_trade_command.cpp`
- **Size**: 981 lines
- **Modified**: 2025-10-16 21:01:48
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/rotation_trade_command.h"
#include "common/utils.h"
#include "common/time_utils.h"
#include "live/polygon_client.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <csignal>
#include <thread>
#include <chrono>
#include <filesystem>
#include <set>
#include <cstdlib>

namespace sentio {
namespace cli {

// Static member for signal handling
static std::atomic<bool> g_shutdown_requested{false};

// Signal handler
static void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << " - initiating graceful shutdown...\n";
    g_shutdown_requested = true;
}

RotationTradeCommand::RotationTradeCommand()
    : options_() {
}

RotationTradeCommand::RotationTradeCommand(const Options& options)
    : options_(options) {
}

RotationTradeCommand::~RotationTradeCommand() {
}

int RotationTradeCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == "--mode" && i + 1 < args.size()) {
            std::string mode = args[++i];
            options_.is_mock_mode = (mode == "mock" || mode == "backtest");
        } else if (arg == "--data-dir" && i + 1 < args.size()) {
            options_.data_dir = args[++i];
        } else if (arg == "--warmup-dir" && i + 1 < args.size()) {
            options_.warmup_dir = args[++i];
        } else if (arg == "--warmup-days" && i + 1 < args.size()) {
            options_.warmup_days = std::stoi(args[++i]);
        } else if (arg == "--date" && i + 1 < args.size()) {
            options_.test_date = args[++i];
        } else if (arg == "--start-date" && i + 1 < args.size()) {
            options_.start_date = args[++i];
        } else if (arg == "--end-date" && i + 1 < args.size()) {
            options_.end_date = args[++i];
        } else if (arg == "--generate-dashboards") {
            options_.generate_dashboards = true;
        } else if (arg == "--dashboard-dir" && i + 1 < args.size()) {
            options_.dashboard_output_dir = args[++i];
        } else if (arg == "--log-dir" && i + 1 < args.size()) {
            options_.log_dir = args[++i];
        } else if (arg == "--config" && i + 1 < args.size()) {
            options_.config_file = args[++i];
        } else if (arg == "--capital" && i + 1 < args.size()) {
            options_.starting_capital = std::stod(args[++i]);
        } else if (arg == "--help" || arg == "-h") {
            show_help();
            return 0;
        }
    }

    // Get Alpaca credentials from environment if in live mode
    if (!options_.is_mock_mode) {
        const char* api_key = std::getenv("ALPACA_PAPER_API_KEY");
        const char* secret_key = std::getenv("ALPACA_PAPER_SECRET_KEY");

        if (api_key) options_.alpaca_api_key = api_key;
        if (secret_key) options_.alpaca_secret_key = secret_key;
    }

    return execute_with_options();
}

void RotationTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli rotation-trade [OPTIONS]\n\n";
    std::cout << "Multi-symbol rotation trading system\n\n";
    std::cout << "Options:\n";
    std::cout << "  --mode <mode>         Trading mode: 'live' or 'mock' (default: live)\n";
    std::cout << "  --date <YYYY-MM-DD>   Test specific date only (mock mode)\n";
    std::cout << "                        Warmup data loaded from prior days\n";
    std::cout << "  --start-date <date>   Start date for batch testing (YYYY-MM-DD)\n";
    std::cout << "  --end-date <date>     End date for batch testing (YYYY-MM-DD)\n";
    std::cout << "                        When specified, runs mock tests for each trading day\n";
    std::cout << "  --generate-dashboards Generate HTML dashboards for batch tests\n";
    std::cout << "  --dashboard-dir <dir> Dashboard output directory (default: <log-dir>/dashboards)\n";
    std::cout << "  --warmup-days <N>     Days of historical data for warmup (default: 4)\n";
    std::cout << "  --data-dir <dir>      Data directory for CSV files (default: data/equities)\n";
    std::cout << "  --warmup-dir <dir>    Warmup data directory (default: data/equities)\n";
    std::cout << "  --log-dir <dir>       Log output directory (default: logs/rotation_trading)\n";
    std::cout << "  --config <file>       Configuration file (default: config/rotation_strategy.json)\n";
    std::cout << "  --capital <amount>    Starting capital (default: 100000.0)\n";
    std::cout << "  --help, -h            Show this help message\n\n";
    std::cout << "Environment Variables (for live mode):\n";
    std::cout << "  ALPACA_PAPER_API_KEY      Alpaca API key\n";
    std::cout << "  ALPACA_PAPER_SECRET_KEY   Alpaca secret key\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Mock trading (backtest)\n";
    std::cout << "  sentio_cli rotation-trade --mode mock --data-dir data/equities\n\n";
    std::cout << "  # Live paper trading\n";
    std::cout << "  export ALPACA_PAPER_API_KEY=your_key\n";
    std::cout << "  export ALPACA_PAPER_SECRET_KEY=your_secret\n";
    std::cout << "  sentio_cli rotation-trade --mode live\n";
}

int RotationTradeCommand::execute_with_options() {
    log_system("========================================");
    log_system("Multi-Symbol Rotation Trading System");
    log_system("========================================");
    log_system("");

    log_system("Mode: " + std::string(options_.is_mock_mode ? "MOCK (Backtest)" : "LIVE (Paper)"));
    log_system("Symbols: " + std::to_string(options_.symbols.size()) + " instruments");
    for (const auto& symbol : options_.symbols) {
        log_system("  - " + symbol);
    }
    log_system("");

    // Setup signal handlers
    setup_signal_handlers();

    // Execute based on mode
    if (options_.is_mock_mode) {
        // Check if batch mode (date range specified)
        if (!options_.start_date.empty() && !options_.end_date.empty()) {
            return execute_batch_mock_trading();
        } else {
            return execute_mock_trading();
        }
    } else {
        return execute_live_trading();
    }
}

RotationTradingBackend::Config RotationTradeCommand::load_config() {
    RotationTradingBackend::Config config;

    // Load from JSON if available
    std::ifstream file(options_.config_file);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            file.close();

            // Load symbols from config (if not already set via command line)
            if (options_.symbols.empty() && j.contains("symbols") && j["symbols"].contains("active")) {
                options_.symbols = j["symbols"]["active"].get<std::vector<std::string>>();
                log_system("Loaded " + std::to_string(options_.symbols.size()) + " symbols from config:");
                std::string symbol_list = "  ";
                for (size_t i = 0; i < options_.symbols.size(); ++i) {
                    symbol_list += options_.symbols[i];
                    if (i < options_.symbols.size() - 1) symbol_list += ", ";
                }
                log_system(symbol_list);
                log_system("");
            }

            // Load OES config
            if (j.contains("oes_config")) {
                auto oes = j["oes_config"];
                config.oes_config.buy_threshold = oes.value("buy_threshold", 0.53);
                config.oes_config.sell_threshold = oes.value("sell_threshold", 0.47);
                config.oes_config.neutral_zone = oes.value("neutral_zone", 0.06);
                config.oes_config.ewrls_lambda = oes.value("ewrls_lambda", 0.995);
                config.oes_config.warmup_samples = oes.value("warmup_samples", 100);
                config.oes_config.enable_bb_amplification = oes.value("enable_bb_amplification", true);
                config.oes_config.bb_amplification_factor = oes.value("bb_amplification_factor", 0.10);
                config.oes_config.bb_period = oes.value("bb_period", 20);
                config.oes_config.bb_std_dev = oes.value("bb_std_dev", 2.0);
                config.oes_config.bb_proximity_threshold = oes.value("bb_proximity_threshold", 0.30);
                config.oes_config.regularization = oes.value("regularization", 0.01);
                config.oes_config.initial_variance = oes.value("initial_variance", 100.0);

                if (oes.contains("horizon_weights")) {
                    config.oes_config.horizon_weights = oes["horizon_weights"].get<std::vector<double>>();
                }
            }

            // Load signal aggregator config
            if (j.contains("signal_aggregator_config")) {
                auto agg = j["signal_aggregator_config"];
                config.aggregator_config.min_probability = agg.value("min_probability", 0.51);
                config.aggregator_config.min_confidence = agg.value("min_confidence", 0.55);
                config.aggregator_config.min_strength = agg.value("min_strength", 0.40);
            }

            // Load leverage boosts
            if (j.contains("symbols") && j["symbols"].contains("leverage_boosts")) {
                auto boosts = j["symbols"]["leverage_boosts"];
                for (const auto& symbol : options_.symbols) {
                    if (boosts.contains(symbol)) {
                        config.aggregator_config.leverage_boosts[symbol] = boosts[symbol];
                    }
                }
            }

            // Load rotation config
            if (j.contains("rotation_manager_config")) {
                auto rot = j["rotation_manager_config"];
                config.rotation_config.max_positions = rot.value("max_positions", 3);
                config.rotation_config.min_strength_to_enter = rot.value("min_strength_to_enter", 0.50);
                config.rotation_config.rotation_strength_delta = rot.value("rotation_strength_delta", 0.10);
                config.rotation_config.profit_target_pct = rot.value("profit_target_pct", 0.03);
                config.rotation_config.stop_loss_pct = rot.value("stop_loss_pct", 0.015);
                config.rotation_config.eod_liquidation = rot.value("eod_liquidation", true);
                config.rotation_config.eod_exit_time_minutes = rot.value("eod_exit_time_minutes", 388);
            }

            log_system("✓ Loaded configuration from: " + options_.config_file);
        } catch (const std::exception& e) {
            log_system("⚠️  Failed to load config: " + std::string(e.what()));
            log_system("   Using default configuration");
        }
    } else {
        log_system("⚠️  Config file not found: " + options_.config_file);
        log_system("   Using default configuration");
    }

    // =========================================================================
    // CRITICAL VALIDATION: Fail-fast on invalid configuration
    // =========================================================================

    // Validate buy/sell thresholds
    if (config.oes_config.buy_threshold <= 0.0 || config.oes_config.buy_threshold > 1.0) {
        throw std::runtime_error("FATAL: buy_threshold must be in (0, 1], got " +
                                std::to_string(config.oes_config.buy_threshold));
    }
    if (config.oes_config.sell_threshold <= 0.0 || config.oes_config.sell_threshold > 1.0) {
        throw std::runtime_error("FATAL: sell_threshold must be in (0, 1], got " +
                                std::to_string(config.oes_config.sell_threshold));
    }
    if (config.oes_config.buy_threshold <= config.oes_config.sell_threshold) {
        throw std::runtime_error("FATAL: buy_threshold (" +
                                std::to_string(config.oes_config.buy_threshold) +
                                ") must be > sell_threshold (" +
                                std::to_string(config.oes_config.sell_threshold) + ")");
    }

    // Validate warmup samples
    if (config.oes_config.warmup_samples < 10) {
        throw std::runtime_error("FATAL: warmup_samples must be >= 10, got " +
                                std::to_string(config.oes_config.warmup_samples));
    }

    // Validate symbols list (after loading from config)
    if (options_.symbols.empty()) {
        throw std::runtime_error("FATAL: No symbols configured. Check rotation_strategy.json");
    }

    // Validate rotation config
    if (config.rotation_config.max_positions < 1) {
        throw std::runtime_error("FATAL: max_positions must be >= 1, got " +
                                std::to_string(config.rotation_config.max_positions));
    }
    if (config.rotation_config.min_strength_to_enter <= 0.0 ||
        config.rotation_config.min_strength_to_enter > 1.0) {
        throw std::runtime_error("FATAL: min_strength_to_enter must be in (0, 1], got " +
                                std::to_string(config.rotation_config.min_strength_to_enter));
    }

    // Validate starting capital
    if (options_.starting_capital <= 0.0) {
        throw std::runtime_error("FATAL: starting_capital must be > 0, got " +
                                std::to_string(options_.starting_capital));
    }

    log_system("✓ Configuration validation passed");
    log_system("");

    // Set symbols and capital
    config.symbols = options_.symbols;
    config.starting_capital = options_.starting_capital;

    // Set output paths
    config.signal_log_path = options_.log_dir + "/signals.jsonl";
    config.decision_log_path = options_.log_dir + "/decisions.jsonl";
    config.trade_log_path = options_.log_dir + "/trades.jsonl";
    config.position_log_path = options_.log_dir + "/positions.jsonl";

    // Set broker credentials
    config.alpaca_api_key = options_.alpaca_api_key;
    config.alpaca_secret_key = options_.alpaca_secret_key;
    config.paper_trading = options_.paper_trading;

    return config;
}

std::map<std::string, std::vector<Bar>> RotationTradeCommand::load_warmup_data() {
    std::map<std::string, std::vector<Bar>> warmup_data;

    log_system("Loading warmup data...");

    for (const auto& symbol : options_.symbols) {
        std::string filename = options_.warmup_dir + "/" + symbol + "_RTH_NH.csv";

        std::ifstream file(filename);
        if (!file.is_open()) {
            log_system("⚠️  Could not open warmup file for " + symbol + ": " + filename);
            continue;
        }

        std::vector<Bar> bars;
        std::string line;

        // Skip header
        std::getline(file, line);

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::stringstream ss(line);
            std::vector<std::string> tokens;
            std::string token;

            while (std::getline(ss, token, ',')) {
                tokens.push_back(token);
            }

            try {
                Bar bar;

                // Support both 6-column and 7-column formats
                if (tokens.size() == 7) {
                    // Format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
                    bar.timestamp_ms = std::stoull(tokens[1]) * 1000;  // Convert epoch seconds to ms
                    bar.open = std::stod(tokens[2]);
                    bar.high = std::stod(tokens[3]);
                    bar.low = std::stod(tokens[4]);
                    bar.close = std::stod(tokens[5]);
                    bar.volume = std::stoll(tokens[6]);
                } else if (tokens.size() >= 6) {
                    // Format: timestamp,open,high,low,close,volume
                    bar.timestamp_ms = std::stoull(tokens[0]);
                    bar.open = std::stod(tokens[1]);
                    bar.high = std::stod(tokens[2]);
                    bar.low = std::stod(tokens[3]);
                    bar.close = std::stod(tokens[4]);
                    bar.volume = std::stoll(tokens[5]);
                } else {
                    continue;  // Skip malformed lines
                }

                bars.push_back(bar);
            } catch (const std::exception& e) {
                // Skip malformed lines
                continue;
            }
        }

        if (options_.is_mock_mode && !options_.test_date.empty()) {
            // Date-specific test: Load warmup_days before test_date
            // Parse test_date (YYYY-MM-DD)
            int test_year, test_month, test_day;
            if (std::sscanf(options_.test_date.c_str(), "%d-%d-%d", &test_year, &test_month, &test_day) == 3) {
                // Calculate warmup start date (test_date - warmup_days)
                std::tm test_tm = {};
                test_tm.tm_year = test_year - 1900;
                test_tm.tm_mon = test_month - 1;
                test_tm.tm_mday = test_day - options_.warmup_days;  // Go back warmup_days
                std::mktime(&test_tm);  // Normalize the date

                std::tm end_tm = {};
                end_tm.tm_year = test_year - 1900;
                end_tm.tm_mon = test_month - 1;
                end_tm.tm_mday = test_day - 1;  // Day before test_date
                end_tm.tm_hour = 23;
                end_tm.tm_min = 59;
                std::mktime(&end_tm);

                // Filter bars between warmup_start and test_date-1
                std::vector<Bar> warmup_bars;
                for (const auto& bar : bars) {
                    std::time_t bar_time = bar.timestamp_ms / 1000;
                    std::tm* bar_tm = std::localtime(&bar_time);

                    if (bar_tm &&
                        std::mktime(bar_tm) >= std::mktime(&test_tm) &&
                        std::mktime(bar_tm) <= std::mktime(&end_tm)) {
                        warmup_bars.push_back(bar);
                    }
                }

                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) +
                          " bars (" + std::to_string(options_.warmup_days) + " days warmup before " +
                          options_.test_date + ")");
            } else {
                log_system("⚠️  Invalid date format: " + options_.test_date);
                warmup_data[symbol] = bars;
            }
        } else if (options_.is_mock_mode) {
            // For mock mode (no specific date), use last 1560 bars (4 blocks) for warmup
            // This ensures 50+ bars for indicator warmup (max_period=50) plus 100+ for predictor training
            if (bars.size() > 1560) {
                std::vector<Bar> warmup_bars(bars.end() - 1560, bars.end());
                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) + " bars (4 blocks)");
            } else {
                warmup_data[symbol] = bars;
                log_system("  " + symbol + ": " + std::to_string(bars.size()) + " bars (all available)");
            }
        } else {
            // For live mode, use last 7800 bars (20 blocks) for warmup
            if (bars.size() > 7800) {
                std::vector<Bar> warmup_bars(bars.end() - 7800, bars.end());
                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) + " bars (20 blocks)");
            } else {
                warmup_data[symbol] = bars;
                log_system("  " + symbol + ": " + std::to_string(bars.size()) + " bars (all available)");
            }
        }
    }

    // **VALIDATION**: Ensure all expected symbols were loaded
    if (warmup_data.size() != options_.symbols.size()) {
        log_system("");
        log_system("❌ SYMBOL VALIDATION FAILED!");
        log_system("Expected " + std::to_string(options_.symbols.size()) + " symbols, but loaded " + std::to_string(warmup_data.size()));
        log_system("");
        log_system("Expected symbols:");
        for (const auto& sym : options_.symbols) {
            bool loaded = warmup_data.find(sym) != warmup_data.end();
            log_system("  " + sym + ": " + (loaded ? "✓ LOADED" : "❌ FAILED"));
        }
        log_system("");
        throw std::runtime_error("Symbol validation failed: Not all symbols loaded successfully");
    }

    log_system("✓ Symbol validation passed: All " + std::to_string(warmup_data.size()) + " symbols loaded successfully");
    log_system("");
    return warmup_data;
}

int RotationTradeCommand::execute_mock_trading() {
    log_system("========================================");
    log_system("Mock Trading Mode (Backtest)");
    log_system("========================================");
    log_system("");

    // Load configuration
    auto config = load_config();

    // Create data manager
    data::MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = options_.symbols;
    dm_config.backtest_mode = true;  // Disable timestamp validation for mock trading
    data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);

    // Create backend (no broker for mock mode)
    backend_ = std::make_unique<RotationTradingBackend>(config, data_manager_, nullptr);

    // Load and warmup
    auto warmup_data = load_warmup_data();
    if (!backend_->warmup(warmup_data)) {
        log_system("❌ Warmup failed!");
        return 1;
    }
    log_system("✓ Warmup complete");
    log_system("");

    // Create mock feed
    data::MockMultiSymbolFeed::Config feed_config;
    for (const auto& symbol : options_.symbols) {
        feed_config.symbol_files[symbol] = options_.data_dir + "/" + symbol + "_RTH_NH.csv";
    }
    feed_config.replay_speed = 0.0;  // Instant replay for testing
    feed_config.filter_date = options_.test_date;  // Apply date filter if specified

    if (!options_.test_date.empty()) {
        log_system("Starting mock trading session...");
        log_system("Test date: " + options_.test_date + " (single-day mode)");
        log_system("");
    } else {
        log_system("Starting mock trading session...");
        log_system("");
    }

    auto feed = std::make_shared<data::MockMultiSymbolFeed>(data_manager_, feed_config);

    // Set callback to trigger backend on each bar
    feed->set_bar_callback([this](const std::string& symbol, const Bar& bar) {
        if (g_shutdown_requested) {
            return;
        }
        backend_->on_bar();
    });

    // Create log directory if it doesn't exist
    std::string mkdir_cmd = "mkdir -p " + options_.log_dir;
    int result = system(mkdir_cmd.c_str());
    if (result != 0) {
        log_system("⚠️  Failed to create log directory: " + options_.log_dir);
    }

    // Start trading
    log_system("Starting mock trading session...");
    log_system("");

    if (!backend_->start_trading()) {
        log_system("❌ Failed to start trading!");
        return 1;
    }

    // Connect and start feed
    if (!feed->connect()) {
        log_system("❌ Failed to connect feed!");
        return 1;
    }

    if (!feed->start()) {
        log_system("❌ Failed to start feed!");
        return 1;
    }

    // Wait for replay to complete
    log_system("Waiting for backtest to complete...");
    while (feed->is_active() && !g_shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop trading
    backend_->stop_trading();
    log_system("");
    log_system("Mock trading session complete");
    log_system("");

    // Print summary
    print_summary(backend_->get_session_stats());

    return 0;
}

int RotationTradeCommand::execute_live_trading() {
    log_system("========================================");
    log_system("Live Trading Mode (Paper)");
    log_system("========================================");
    log_system("");

    // Check credentials
    if (options_.alpaca_api_key.empty() || options_.alpaca_secret_key.empty()) {
        log_system("❌ Missing Alpaca credentials!");
        log_system("   Set ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY");
        return 1;
    }

    // Load configuration
    auto config = load_config();

    // Create data manager
    data::MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = options_.symbols;
    data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);

    // Create Alpaca client
    broker_ = std::make_shared<AlpacaClient>(
        options_.alpaca_api_key,
        options_.alpaca_secret_key,
        options_.paper_trading
    );

    // Test connection
    log_system("Testing Alpaca connection...");
    auto account = broker_->get_account();
    if (!account || account->cash < 0) {
        log_system("❌ Failed to connect to Alpaca!");
        return 1;
    }
    log_system("✓ Connected to Alpaca");
    log_system("  Cash: $" + std::to_string(account->cash));
    log_system("");

    // Create backend
    backend_ = std::make_unique<RotationTradingBackend>(config, data_manager_, broker_);

    // Load and warmup
    auto warmup_data = load_warmup_data();
    if (!backend_->warmup(warmup_data)) {
        log_system("❌ Warmup failed!");
        return 1;
    }
    log_system("✓ Warmup complete");
    log_system("");

    // Create FIFO reader for live bars (from Python WebSocket bridge)
    log_system("Connecting to Python WebSocket bridge (FIFO)...");
    auto polygon_client = std::make_shared<PolygonClient>("", "");  // Unused params for FIFO mode

    if (!polygon_client->connect()) {
        log_system("❌ Failed to connect to FIFO!");
        return 1;
    }

    // Subscribe to all symbols
    polygon_client->subscribe(options_.symbols);

    // Start trading
    log_system("Starting live rotation trading session...");
    log_system("");

    if (!backend_->start_trading()) {
        log_system("❌ Failed to start trading!");
        return 1;
    }

    // Set up bar callback to trigger backend on each bar
    std::atomic<int> bar_count{0};
    auto bar_callback = [this, &bar_count](const std::string& symbol, const Bar& bar) {
        if (g_shutdown_requested) {
            return;
        }

        // Update data manager with new bar
        data_manager_->update_symbol(symbol, bar);

        // Process bar (generate signals, make trades)
        backend_->on_bar();

        bar_count++;

        // Log progress every 10 bars
        if (bar_count % 10 == 0) {
            log_system("Bars processed: " + std::to_string(bar_count.load()));
        }
    };

    // Start FIFO reader
    polygon_client->start(bar_callback);

    log_system("✓ Live trading active - listening for bars from Python bridge");
    log_system("  FIFO path: /tmp/alpaca_bars.fifo");
    log_system("  Press Ctrl+C to stop");
    log_system("");

    // Wait for shutdown signal
    while (!g_shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check if EOD
        if (backend_->is_eod(get_minutes_since_open())) {
            log_system("");
            log_system("EOD time reached - liquidating positions...");
            backend_->liquidate_all_positions("EOD");
            break;
        }
    }

    // Stop trading
    log_system("");
    log_system("Stopping live trading...");
    polygon_client->stop();
    backend_->stop_trading();

    // Print summary
    auto stats = backend_->get_session_stats();
    print_summary(stats);

    return 0;
}

void RotationTradeCommand::setup_signal_handlers() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

bool RotationTradeCommand::is_eod() const {
    int minutes = get_minutes_since_open();
    return minutes >= 358;  // 3:58 PM ET
}

int RotationTradeCommand::get_minutes_since_open() const {
    // Get current ET time
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    // Convert to ET (this is simplified - production should use proper timezone handling)
    std::tm* now_tm = std::localtime(&now_time);

    // Calculate minutes since 9:30 AM
    int minutes = (now_tm->tm_hour - 9) * 60 + (now_tm->tm_min - 30);

    return minutes;
}

void RotationTradeCommand::print_summary(const RotationTradingBackend::SessionStats& stats) {
    log_system("========================================");
    log_system("Session Summary");
    log_system("========================================");

    log_system("Bars processed: " + std::to_string(stats.bars_processed));
    log_system("Signals generated: " + std::to_string(stats.signals_generated));
    log_system("Trades executed: " + std::to_string(stats.trades_executed));
    log_system("Positions opened: " + std::to_string(stats.positions_opened));
    log_system("Positions closed: " + std::to_string(stats.positions_closed));
    log_system("Rotations: " + std::to_string(stats.rotations));
    log_system("");

    log_system("Total P&L: $" + std::to_string(stats.total_pnl) +
               " (" + std::to_string(stats.total_pnl_pct * 100.0) + "%)");
    log_system("Final equity: $" + std::to_string(stats.current_equity));
    log_system("Max drawdown: " + std::to_string(stats.max_drawdown * 100.0) + "%");
    log_system("");

    log_system("Win rate: " + std::to_string(stats.win_rate * 100.0) + "%");
    log_system("Avg win: " + std::to_string(stats.avg_win_pct * 100.0) + "%");
    log_system("Avg loss: " + std::to_string(stats.avg_loss_pct * 100.0) + "%");
    log_system("Sharpe ratio: " + std::to_string(stats.sharpe_ratio));
    log_system("MRD: " + std::to_string(stats.mrd * 100.0) + "%");

    log_system("========================================");

    // Highlight MRD performance
    if (stats.mrd >= 0.005) {
        log_system("");
        log_system("🎯 TARGET ACHIEVED! MRD >= 0.5%");
    } else if (stats.mrd >= 0.0) {
        log_system("");
        log_system("✓ Positive MRD: " + std::to_string(stats.mrd * 100.0) + "%");
    } else {
        log_system("");
        log_system("⚠️  Negative MRD: " + std::to_string(stats.mrd * 100.0) + "%");
    }
}

void RotationTradeCommand::log_system(const std::string& msg) {
    std::cout << msg << std::endl;
}

int RotationTradeCommand::execute_batch_mock_trading() {
    log_system("========================================");
    log_system("Batch Mock Trading Mode");
    log_system("========================================");
    log_system("Start Date: " + options_.start_date);
    log_system("End Date: " + options_.end_date);
    log_system("");

    // Set dashboard output directory if not specified
    if (options_.dashboard_output_dir.empty()) {
        options_.dashboard_output_dir = options_.log_dir + "/dashboards";
    }

    // Extract trading days from data
    auto trading_days = extract_trading_days(options_.start_date, options_.end_date);

    if (trading_days.empty()) {
        log_system("❌ No trading days found in date range");
        return 1;
    }

    log_system("Found " + std::to_string(trading_days.size()) + " trading days");
    for (const auto& day : trading_days) {
        log_system("  - " + day);
    }
    log_system("");

    // Results tracking
    std::vector<std::map<std::string, std::string>> daily_results;
    int success_count = 0;

    // Run mock trading for each day
    for (size_t i = 0; i < trading_days.size(); ++i) {
        const auto& date = trading_days[i];

        log_system("");
        log_system("========================================");
        log_system("[" + std::to_string(i+1) + "/" + std::to_string(trading_days.size()) + "] " + date);
        log_system("========================================");
        log_system("");

        // Set test date for this iteration
        options_.test_date = date;

        // Create day-specific output directory
        std::string day_output = options_.log_dir + "/" + date;
        std::filesystem::create_directories(day_output);

        // Temporarily redirect log_dir for this day
        std::string original_log_dir = options_.log_dir;
        options_.log_dir = day_output;

        // Execute single day mock trading
        int result = execute_mock_trading();

        // Restore log_dir
        options_.log_dir = original_log_dir;

        if (result == 0) {
            success_count++;

            // Generate dashboard if requested
            if (options_.generate_dashboards) {
                generate_daily_dashboard(date, day_output);
            }

            // Store results for summary
            std::map<std::string, std::string> day_result;
            day_result["date"] = date;
            day_result["output_dir"] = day_output;
            daily_results.push_back(day_result);
        }
    }

    log_system("");
    log_system("========================================");
    log_system("BATCH TEST COMPLETE");
    log_system("========================================");
    log_system("Successful days: " + std::to_string(success_count) + "/" + std::to_string(trading_days.size()));
    log_system("");

    // Generate summary dashboard
    if (!daily_results.empty() && options_.generate_dashboards) {
        generate_summary_dashboard(daily_results, options_.dashboard_output_dir);
    }

    return (success_count > 0) ? 0 : 1;
}

std::vector<std::string> RotationTradeCommand::extract_trading_days(
    const std::string& start_date,
    const std::string& end_date
) {
    std::vector<std::string> trading_days;
    std::set<std::string> unique_dates;

    // Use SQQQ as reference (one of the rotation trading symbols)
    std::string reference_file = options_.data_dir + "/SQQQ_RTH_NH.csv";
    std::ifstream file(reference_file);

    if (!file.is_open()) {
        log_system("❌ Could not open " + reference_file);
        return trading_days;
    }

    std::string line;
    std::getline(file, line);  // Skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Extract date from timestamp (format: YYYY-MM-DDTHH:MM:SS)
        size_t t_pos = line.find('T');
        if (t_pos != std::string::npos) {
            std::string date_str = line.substr(0, t_pos);

            // Check if within range
            if (date_str >= start_date && date_str <= end_date) {
                unique_dates.insert(date_str);
            }
        }
    }

    file.close();

    // Convert set to vector
    trading_days.assign(unique_dates.begin(), unique_dates.end());
    std::sort(trading_days.begin(), trading_days.end());

    return trading_days;
}

int RotationTradeCommand::generate_daily_dashboard(
    const std::string& date,
    const std::string& output_dir
) {
    log_system("📈 Generating dashboard for " + date + "...");

    // Build command
    std::string cmd = "python3 scripts/rotation_trading_dashboard.py";
    cmd += " --trades " + output_dir + "/trades.jsonl";
    cmd += " --output " + output_dir + "/dashboard.html";

    // Add optional files if they exist
    std::string signals_file = output_dir + "/signals.jsonl";
    std::string positions_file = output_dir + "/positions.jsonl";
    std::string decisions_file = output_dir + "/decisions.jsonl";

    if (std::filesystem::exists(signals_file)) {
        cmd += " --signals " + signals_file;
    }
    if (std::filesystem::exists(positions_file)) {
        cmd += " --positions " + positions_file;
    }
    if (std::filesystem::exists(decisions_file)) {
        cmd += " --decisions " + decisions_file;
    }

    // Execute
    int result = std::system(cmd.c_str());

    if (result == 0) {
        log_system("✓ Dashboard generated: " + output_dir + "/dashboard.html");
    } else {
        log_system("❌ Dashboard generation failed");
    }

    return result;
}

int RotationTradeCommand::generate_summary_dashboard(
    const std::vector<std::map<std::string, std::string>>& daily_results,
    const std::string& output_dir
) {
    log_system("");
    log_system("========================================");
    log_system("Generating Summary Dashboard");
    log_system("========================================");

    std::filesystem::create_directories(output_dir);

    // Create summary markdown
    std::string summary_file = output_dir + "/SUMMARY.md";
    std::ofstream out(summary_file);

    out << "# Rotation Trading Batch Test Summary\n\n";
    out << "## Test Period\n";
    out << "- **Start Date**: " << options_.start_date << "\n";
    out << "- **End Date**: " << options_.end_date << "\n";
    out << "- **Trading Days**: " << daily_results.size() << "\n\n";

    out << "## Daily Results\n\n";
    out << "| Date | Dashboard | Trades | Signals | Decisions |\n";
    out << "|------|-----------|--------|---------|----------|\n";

    for (const auto& result : daily_results) {
        std::string date = result.at("date");
        std::string dir = result.at("output_dir");

        out << "| " << date << " ";
        out << "| [View](" << dir << "/dashboard.html) ";
        out << "| [trades.jsonl](" << dir << "/trades.jsonl) ";
        out << "| [signals.jsonl](" << dir << "/signals.jsonl) ";
        out << "| [decisions.jsonl](" << dir << "/decisions.jsonl) ";
        out << "|\n";
    }

    out << "\n---\n\n";
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    out << "Generated on: " << std::ctime(&time);

    out.close();

    log_system("✅ Summary saved: " + summary_file);

    // Generate aggregate HTML dashboard
    log_system("");
    log_system("📊 Generating aggregate HTML dashboard...");

    std::string aggregate_html = output_dir + "/aggregate_summary.html";
    std::string cmd = "python3 scripts/rotation_trading_aggregate_dashboard.py";
    cmd += " --batch-dir " + options_.log_dir;
    cmd += " --output " + aggregate_html;
    cmd += " --start-date " + options_.start_date;
    cmd += " --end-date " + options_.end_date;

    int result = std::system(cmd.c_str());

    if (result == 0) {
        log_system("✅ Aggregate dashboard generated: " + aggregate_html);
    } else {
        log_system("❌ Aggregate dashboard generation failed");
    }

    return result;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 22 of 30**: src/common/time_utils.cpp

**File Information**:
- **Path**: `src/common/time_utils.cpp`
- **Size**: 126 lines
- **Modified**: 2025-10-07 21:46:56
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "common/time_utils.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>

namespace sentio {

std::tm TradingSession::to_local_time(const std::chrono::system_clock::time_point& tp) const {
    // C++20 thread-safe timezone conversion using zoned_time
    // This replaces the unsafe setenv("TZ") approach

    #if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
        // Use C++20 timezone database
        try {
            const auto* tz = std::chrono::locate_zone(timezone_name);
            std::chrono::zoned_time zt{tz, tp};

            // Convert zoned_time to std::tm
            auto local_time = zt.get_local_time();
            auto local_dp = std::chrono::floor<std::chrono::days>(local_time);
            auto ymd = std::chrono::year_month_day{local_dp};
            auto tod = std::chrono::hh_mm_ss{local_time - local_dp};

            std::tm result{};
            result.tm_year = static_cast<int>(ymd.year()) - 1900;
            result.tm_mon = static_cast<unsigned>(ymd.month()) - 1;
            result.tm_mday = static_cast<unsigned>(ymd.day());
            result.tm_hour = tod.hours().count();
            result.tm_min = tod.minutes().count();
            result.tm_sec = tod.seconds().count();

            // Calculate day of week
            auto dp_sys = std::chrono::sys_days{ymd};
            auto weekday = std::chrono::weekday{dp_sys};
            result.tm_wday = weekday.c_encoding();

            // DST info
            auto info = zt.get_info();
            result.tm_isdst = (info.save != std::chrono::minutes{0}) ? 1 : 0;

            return result;

        } catch (const std::exception& e) {
            // Fallback: if timezone not found, use UTC
            auto tt = std::chrono::system_clock::to_time_t(tp);
            std::tm result;
            gmtime_r(&tt, &result);
            return result;
        }
    #else
        // Fallback for C++17: use old setenv approach (NOT thread-safe)
        // This should not happen since we require C++20
        #warning "C++20 chrono timezone database not available - using unsafe setenv fallback"

        auto tt = std::chrono::system_clock::to_time_t(tp);

        const char* old_tz = getenv("TZ");
        setenv("TZ", timezone_name.c_str(), 1);
        tzset();

        std::tm local_tm;
        localtime_r(&tt, &local_tm);

        if (old_tz) {
            setenv("TZ", old_tz, 1);
        } else {
            unsetenv("TZ");
        }
        tzset();

        return local_tm;
    #endif
}

bool TradingSession::is_regular_hours(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    int hour = local_tm.tm_hour;
    int minute = local_tm.tm_min;

    // Calculate minutes since midnight
    int open_mins = market_open_hour * 60 + market_open_minute;
    int close_mins = market_close_hour * 60 + market_close_minute;
    int current_mins = hour * 60 + minute;

    return current_mins >= open_mins && current_mins < close_mins;
}

bool TradingSession::is_weekday(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    // tm_wday: 0 = Sunday, 1 = Monday, ..., 6 = Saturday
    int wday = local_tm.tm_wday;

    return wday >= 1 && wday <= 5;  // Monday - Friday
}

std::string TradingSession::to_local_string(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    std::stringstream ss;
    ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    ss << " " << timezone_name;

    return ss.str();
}

std::string to_iso_string(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm utc_tm;
    gmtime_r(&tt, &utc_tm);

    std::stringstream ss;
    ss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S");

    // Add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()
    ).count() % 1000;
    ss << "." << std::setfill('0') << std::setw(3) << ms << "Z";

    return ss.str();
}

} // namespace sentio

```

## 📄 **FILE 23 of 30**: src/common/utils.cpp

**File Information**:
- **Path**: `src/common/utils.cpp`
- **Size**: 581 lines
- **Modified**: 2025-10-16 21:00:02
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "common/utils.h"
#include "common/binary_data.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

// =============================================================================
// Module: common/utils.cpp
// Purpose: Implementation of utility functions for file I/O, time handling,
//          JSON parsing, hashing, and mathematical calculations.
//
// This module provides the concrete implementations for all utility functions
// declared in utils.h. Each section handles a specific domain of functionality
// to keep the codebase modular and maintainable.
// =============================================================================

// ============================================================================
// Helper Functions to Fix ODR Violations
// ============================================================================

/**
 * @brief Convert CSV path to binary path (fixes ODR violation)
 * 
 * This helper function eliminates code duplication that was causing ODR violations
 * by consolidating identical path conversion logic used in multiple places.
 */
static std::string convert_csv_to_binary_path(const std::string& data_path) {
    std::filesystem::path p(data_path);
    if (!p.has_extension()) {
        p += ".bin";
    } else {
        p.replace_extension(".bin");
    }
    // Ensure parent directory exists
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    return p.string();
}

namespace sentio {
namespace utils {
// ------------------------------ Bar ID utilities ------------------------------
uint64_t generate_bar_id(int64_t timestamp_ms, const std::string& symbol) {
    uint64_t timestamp_part = static_cast<uint64_t>(timestamp_ms) & 0xFFFFFFFFFFFFULL; // lower 48 bits
    uint32_t symbol_hash = static_cast<uint32_t>(std::hash<std::string>{}(symbol));
    uint64_t symbol_part = (static_cast<uint64_t>(symbol_hash) & 0xFFFFULL) << 48; // upper 16 bits
    return timestamp_part | symbol_part;
}

int64_t extract_timestamp(uint64_t bar_id) {
    return static_cast<int64_t>(bar_id & 0xFFFFFFFFFFFFULL);
}

uint16_t extract_symbol_hash(uint64_t bar_id) {
    return static_cast<uint16_t>((bar_id >> 48) & 0xFFFFULL);
}


// --------------------------------- Helpers ----------------------------------
namespace {
    /// Helper function to remove leading and trailing whitespace from strings
    /// Used internally by CSV parsing and JSON processing functions
    static inline std::string trim(const std::string& s) {
        const char* ws = " \t\n\r\f\v";
        const auto start = s.find_first_not_of(ws);
        if (start == std::string::npos) return "";
        const auto end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }
}

// ----------------------------- File I/O utilities ----------------------------

/// Reads OHLCV market data from CSV files with automatic format detection
/// 
/// This function handles two CSV formats:
/// 1. QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume (symbol extracted from filename)
/// 2. Standard format: symbol,timestamp_ms,open,high,low,close,volume
/// 
/// The function automatically detects the format by examining the header row
/// and processes the data accordingly, ensuring compatibility with different
/// data sources while maintaining a consistent Bar output format.
std::vector<Bar> read_csv_data(const std::string& path) {
    std::vector<Bar> bars;
    std::ifstream file(path);
    
    // Early return if file cannot be opened
    if (!file.is_open()) {
        return bars;
    }

    std::string line;
    
    // Read and analyze header to determine CSV format
    std::getline(file, line);
    bool is_qqq_format = (line.find("ts_utc") != std::string::npos);
    bool is_standard_format = (line.find("symbol") != std::string::npos && line.find("timestamp_ms") != std::string::npos);
    bool is_datetime_format = (line.find("timestamp") != std::string::npos && line.find("timestamp_ms") == std::string::npos);
    
    // For QQQ format, extract symbol from filename since it's not in the CSV
    std::string default_symbol = "UNKNOWN";
    if (is_qqq_format) {
        size_t last_slash = path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
        
        // Pattern matching for common ETF symbols
        if (filename.find("QQQ") != std::string::npos) default_symbol = "QQQ";
        else if (filename.find("SQQQ") != std::string::npos) default_symbol = "SQQQ";
        else if (filename.find("TQQQ") != std::string::npos) default_symbol = "TQQQ";
    }

    // Process each data row according to the detected format
    size_t sequence_index = 0;
    size_t parse_errors = 0;

    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        std::stringstream ss(line);
        std::string item;
        Bar b{};

        try {
            // Parse timestamp and symbol based on detected format
            if (is_qqq_format) {
                // QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
                b.symbol = default_symbol;

                // Parse ts_utc column (ISO timestamp string) but discard value
                std::getline(ss, item, ',');

                // Use ts_nyt_epoch as timestamp (Unix seconds -> convert to milliseconds)
                std::getline(ss, item, ',');
                b.timestamp_ms = std::stoll(trim(item)) * 1000;

            } else if (is_standard_format) {
                // Standard format: symbol,timestamp_ms,open,high,low,close,volume
                std::getline(ss, item, ',');
                b.symbol = trim(item);

                std::getline(ss, item, ',');
                b.timestamp_ms = std::stoll(trim(item));

            } else if (is_datetime_format) {
                // Datetime format: timestamp,symbol,open,high,low,close,volume
                // where timestamp is "YYYY-MM-DD HH:MM:SS"
                std::getline(ss, item, ',');
                b.timestamp_ms = timestamp_to_ms(trim(item));

                std::getline(ss, item, ',');
                b.symbol = trim(item);

            } else {
                // Unknown format: treat first column as symbol, second as timestamp_ms
                std::getline(ss, item, ',');
                b.symbol = trim(item);
                std::getline(ss, item, ',');
                b.timestamp_ms = std::stoll(trim(item));
            }

            // Parse OHLCV data (same format across all CSV types)
            std::getline(ss, item, ',');
            b.open = std::stod(trim(item));

            std::getline(ss, item, ',');
            b.high = std::stod(trim(item));

            std::getline(ss, item, ',');
            b.low = std::stod(trim(item));

            std::getline(ss, item, ',');
            b.close = std::stod(trim(item));

            std::getline(ss, item, ',');
            b.volume = std::stod(trim(item));

            // Populate immutable id and derived fields
            b.bar_id = generate_bar_id(b.timestamp_ms, b.symbol);
            b.sequence_num = static_cast<uint32_t>(sequence_index);
            b.block_num = static_cast<uint16_t>(sequence_index / STANDARD_BLOCK_SIZE);
            std::string ts = ms_to_timestamp(b.timestamp_ms);
            if (ts.size() >= 10) b.date_str = ts.substr(0, 10);
            bars.push_back(b);
            ++sequence_index;

        } catch (const std::invalid_argument& e) {
            // Invalid numeric conversion (e.g., non-numeric string)
            parse_errors++;
            continue;
        } catch (const std::out_of_range& e) {
            // Number too large to fit in data type
            parse_errors++;
            continue;
        } catch (const std::exception& e) {
            // Any other parsing error
            parse_errors++;
            continue;
        }
    }

    // Log warning if we encountered malformed data
    if (parse_errors > 0) {
        log_warning("CSV parsing: skipped " + std::to_string(parse_errors) +
                   " malformed line(s) from " + path);
    }

    return bars;
}

bool write_jsonl(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& l : lines) {
        out << l << '\n';
    }
    return true;
}

bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            out << row[i];
            if (i + 1 < row.size()) out << ',';
        }
        out << '\n';
    }
    return true;
}

// --------------------------- Binary Data utilities ---------------------------

std::vector<Bar> read_market_data_range(const std::string& data_path, 
                                       uint64_t start_index, 
                                       uint64_t count) {
    // Try binary format first (much faster)
    // 🔧 ODR FIX: Use helper function to eliminate code duplication
    std::string binary_path = convert_csv_to_binary_path(data_path);
    
    if (std::filesystem::exists(binary_path)) {
        sentio::binary_data::BinaryDataReader reader(binary_path);
        if (reader.open()) {
            if (count == 0) {
                // Read from start_index to end
                count = reader.get_bar_count() - start_index;
            }
            
            auto bars = reader.read_range(start_index, count);
            if (!bars.empty()) {
                // Populate ids and derived fields for the selected range
                for (size_t i = 0; i < bars.size(); ++i) {
                    Bar& b = bars[i];
                    b.bar_id = generate_bar_id(b.timestamp_ms, b.symbol);
                    uint64_t seq = start_index + i;
                    b.sequence_num = static_cast<uint32_t>(seq);
                    b.block_num = static_cast<uint16_t>(seq / STANDARD_BLOCK_SIZE);
                    std::string ts = ms_to_timestamp(b.timestamp_ms);
                    if (ts.size() >= 10) b.date_str = ts.substr(0, 10);
                }
                log_debug("Loaded " + std::to_string(bars.size()) + " bars from binary file: " + 
                         binary_path + " (range: " + std::to_string(start_index) + "-" + 
                         std::to_string(start_index + count - 1) + ")");
                return bars;
            }
        }
    }
    
    // Read from CSV when binary is not available
    log_info("Binary file not found, reading CSV: " + data_path);
    auto all_bars = read_csv_data(data_path);
    
    if (all_bars.empty()) {
        return all_bars;
    }
    
    // Apply range selection
    if (start_index >= all_bars.size()) {
        log_error("Start index " + std::to_string(start_index) + 
                 " exceeds data size " + std::to_string(all_bars.size()));
        return {};
    }
    
    uint64_t end_index = start_index + (count == 0 ? all_bars.size() - start_index : count);
    end_index = std::min(end_index, static_cast<uint64_t>(all_bars.size()));
    
    std::vector<Bar> result(all_bars.begin() + start_index, all_bars.begin() + end_index);
    // Ensure derived fields are consistent with absolute indexing
    for (size_t i = 0; i < result.size(); ++i) {
        Bar& b = result[i];
        // bar_id should already be set by read_csv_data; recompute defensively if missing
        if (b.bar_id == 0) b.bar_id = generate_bar_id(b.timestamp_ms, b.symbol);
        uint64_t seq = start_index + i;
        b.sequence_num = static_cast<uint32_t>(seq);
        b.block_num = static_cast<uint16_t>(seq / STANDARD_BLOCK_SIZE);
        if (b.date_str.empty()) {
            std::string ts = ms_to_timestamp(b.timestamp_ms);
            if (ts.size() >= 10) b.date_str = ts.substr(0, 10);
        }
    }
    log_debug("Loaded " + std::to_string(result.size()) + " bars from CSV file: " + 
             data_path + " (range: " + std::to_string(start_index) + "-" + 
             std::to_string(end_index - 1) + ")");
    
    return result;
}

uint64_t get_market_data_count(const std::string& data_path) {
    // Try binary format first
    // 🔧 ODR FIX: Use helper function to eliminate code duplication
    std::string binary_path = convert_csv_to_binary_path(data_path);
    
    if (std::filesystem::exists(binary_path)) {
        sentio::binary_data::BinaryDataReader reader(binary_path);
        if (reader.open()) {
            return reader.get_bar_count();
        }
    }
    
    // Read from CSV when binary is not available
    auto bars = read_csv_data(data_path);
    return bars.size();
}

std::vector<Bar> read_recent_market_data(const std::string& data_path, uint64_t count) {
    uint64_t total_count = get_market_data_count(data_path);
    if (total_count == 0 || count == 0) {
        return {};
    }
    
    uint64_t start_index = (count >= total_count) ? 0 : (total_count - count);
    return read_market_data_range(data_path, start_index, count);
}

// ------------------------------ Time utilities -------------------------------
int64_t timestamp_to_ms(const std::string& timestamp_str) {
    // Strict parser for "YYYY-MM-DD HH:MM:SS" (UTC) -> epoch ms
    std::tm tm{};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        throw std::runtime_error("timestamp_to_ms parse failed for: " + timestamp_str);
    }
    auto time_c = timegm(&tm); // UTC
    if (time_c == -1) {
        throw std::runtime_error("timestamp_to_ms timegm failed for: " + timestamp_str);
    }
    return static_cast<int64_t>(time_c) * 1000;
}

std::string ms_to_timestamp(int64_t ms) {
    std::time_t t = static_cast<std::time_t>(ms / 1000);
    std::tm* gmt = gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmt);
    return std::string(buf);
}


// ------------------------------ JSON utilities -------------------------------
std::string to_json(const std::map<std::string, std::string>& data) {
    std::ostringstream os;
    os << '{';
    bool first = true;
    for (const auto& [k, v] : data) {
        if (!first) os << ',';
        first = false;
        os << '"' << k << '"' << ':' << '"' << v << '"';
    }
    os << '}';
    return os.str();
}

std::map<std::string, std::string> from_json(const std::string& json_str) {
    // Robust parser for a flat string map {"k":"v",...} that respects quotes and escapes
    std::map<std::string, std::string> out;
    if (json_str.size() < 2 || json_str.front() != '{' || json_str.back() != '}') return out;
    const std::string s = json_str.substr(1, json_str.size() - 2);

    // Split into top-level pairs by commas not inside quotes
    std::vector<std::string> pairs;
    std::string current;
    bool in_quotes = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') {
            // toggle quotes unless escaped
            bool escaped = (i > 0 && s[i-1] == '\\');
            if (!escaped) in_quotes = !in_quotes;
            current.push_back(c);
        } else if (c == ',' && !in_quotes) {
            pairs.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) pairs.push_back(current);

    auto trim_ws = [](const std::string& str){
        size_t a = 0, b = str.size();
        while (a < b && std::isspace(static_cast<unsigned char>(str[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(str[b-1]))) --b;
        return str.substr(a, b - a);
    };

    for (auto& p : pairs) {
        std::string pair = trim_ws(p);
        // find colon not inside quotes
        size_t colon_pos = std::string::npos;
        in_quotes = false;
        for (size_t i = 0; i < pair.size(); ++i) {
            char c = pair[i];
            if (c == '"') {
                bool escaped = (i > 0 && pair[i-1] == '\\');
                if (!escaped) in_quotes = !in_quotes;
            } else if (c == ':' && !in_quotes) {
                colon_pos = i; break;
            }
        }
        if (colon_pos == std::string::npos) continue;
        std::string key = trim_ws(pair.substr(0, colon_pos));
        std::string val = trim_ws(pair.substr(colon_pos + 1));
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') key = key.substr(1, key.size() - 2);
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
        out[key] = val;
    }
    return out;
}

// -------------------------------- Hash utilities -----------------------------

std::string generate_run_id(const std::string& prefix) {
    // Collision-resistant run id: <prefix>-<YYYYMMDDHHMMSS>-<pid>-<rand16hex>
    std::ostringstream os;
    // Timestamp UTC
    std::time_t now = std::time(nullptr);
    std::tm* gmt = gmtime(&now);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d%H%M%S", gmt);
    // Random 64-bit
    uint64_t r = static_cast<uint64_t>(now) ^ 0x9e3779b97f4a7c15ULL;
    r ^= (r << 13);
    r ^= (r >> 7);
    r ^= (r << 17);
    os << (prefix.empty() ? "run" : prefix) << "-" << ts << "-" << std::hex << std::setw(4) << (static_cast<unsigned>(now) & 0xFFFF) << "-";
    os << std::hex << std::setw(16) << std::setfill('0') << (r | 0x1ULL);
    return os.str();
}

// -------------------------------- Math utilities -----------------------------
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate) {
    if (returns.empty()) return 0.0;
    double mean = 0.0;
    for (double r : returns) mean += r;
    mean /= static_cast<double>(returns.size());
    double variance = 0.0;
    for (double r : returns) variance += (r - mean) * (r - mean);
    variance /= static_cast<double>(returns.size());
    double stddev = std::sqrt(variance);
    if (stddev == 0.0) return 0.0;
    return (mean - risk_free_rate) / stddev;
}

double calculate_max_drawdown(const std::vector<double>& equity_curve) {
    if (equity_curve.size() < 2) return 0.0;
    double peak = equity_curve.front();
    double max_dd = 0.0;
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        double e = equity_curve[i];
        if (e > peak) peak = e;
        if (peak > 0.0) {
            double dd = (peak - e) / peak;
            if (dd > max_dd) max_dd = dd;
        }
    }
    return max_dd;
}

// -------------------------------- Logging utilities --------------------------
namespace {
    static inline std::string log_dir() {
        return std::string("logs");
    }
    static inline void ensure_log_dir() {
        std::error_code ec;
        std::filesystem::create_directories(log_dir(), ec);
    }
    static inline std::string iso_now() {
        std::time_t now = std::time(nullptr);
        std::tm* gmt = gmtime(&now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmt);
        return std::string(buf);
    }
}

void log_debug(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/debug.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " DEBUG common:utils:0 - " << message << '\n';
}

void log_info(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/app.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " INFO common:utils:0 - " << message << '\n';
}

void log_warning(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/app.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " WARNING common:utils:0 - " << message << '\n';
}

void log_error(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/errors.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " ERROR common:utils:0 - " << message << '\n';
}

bool would_instruments_conflict(const std::string& proposed, const std::string& existing) {
    // Consolidated conflict detection logic (removes duplicate code)
    static const std::map<std::string, std::vector<std::string>> conflicts = {
        {"TQQQ", {"SQQQ", "PSQ"}},
        {"SQQQ", {"TQQQ", "QQQ"}},
        {"PSQ",  {"TQQQ", "QQQ"}},
        {"QQQ",  {"SQQQ", "PSQ"}}
    };
    
    auto it = conflicts.find(proposed);
    if (it != conflicts.end()) {
        return std::find(it->second.begin(), it->second.end(), existing) != it->second.end();
    }
    
    return false;
}

// -------------------------------- CLI utilities -------------------------------

/// Parse command line arguments supporting both "--name value" and "--name=value" formats
/// 
/// This function provides flexible command-line argument parsing that supports:
/// - Space-separated format: --name value
/// - Equals-separated format: --name=value
/// 
/// @param argc Number of command line arguments
/// @param argv Array of command line argument strings
/// @param name The argument name to search for (including --)
/// @param def Default value to return if argument not found
/// @return The argument value if found, otherwise the default value
std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == name) {
            // Handle "--name value" format
            if (i + 1 < argc) {
                std::string next = argv[i + 1];
                if (!next.empty() && next[0] != '-') return next;
            }
        } else if (arg.rfind(name + "=", 0) == 0) {
            // Handle "--name=value" format
            return arg.substr(name.size() + 1);
        }
    }
    return def;
}

} // namespace utils
} // namespace sentio

```

## 📄 **FILE 24 of 30**: src/data/mock_multi_symbol_feed.cpp

**File Information**:
- **Path**: `src/data/mock_multi_symbol_feed.cpp`
- **Size**: 485 lines
- **Modified**: 2025-10-16 08:28:07
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "data/mock_multi_symbol_feed.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <set>

namespace sentio {
namespace data {

MockMultiSymbolFeed::MockMultiSymbolFeed(
    std::shared_ptr<MultiSymbolDataManager> data_manager,
    const Config& config
)
    : data_manager_(data_manager)
    , config_(config)
    , total_bars_(0) {

    utils::log_info("MockMultiSymbolFeed initialized with " +
                   std::to_string(config_.symbol_files.size()) + " symbols, " +
                   "speed=" + std::to_string(config_.replay_speed) + "x");
}

MockMultiSymbolFeed::~MockMultiSymbolFeed() {
    stop();
    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }
}

// === IBarFeed Interface Implementation ===

bool MockMultiSymbolFeed::connect() {
    if (connected_) {
        utils::log_warning("MockMultiSymbolFeed already connected");
        return true;
    }

    utils::log_info("Loading CSV data for " +
                   std::to_string(config_.symbol_files.size()) + " symbols...");
//     std::cout << "[Feed] Loading CSV data for " << config_.symbol_files.size() << " symbols..." << std::endl;

    int total_loaded = 0;

    for (const auto& [symbol, filepath] : config_.symbol_files) {
//         std::cout << "[Feed] Loading " << symbol << " from " << filepath << std::endl;
        int loaded = load_csv(symbol, filepath);
        if (loaded == 0) {
            utils::log_error("Failed to load data for " + symbol + " from " + filepath);
//             std::cout << "[Feed] ❌ Failed to load " << symbol << " (0 bars loaded)" << std::endl;
            if (error_callback_) {
                error_callback_("Failed to load " + symbol + ": " + filepath);
            }
            return false;
        }

        total_loaded += loaded;
        utils::log_info("  " + symbol + ": " + std::to_string(loaded) + " bars");
//         std::cout << "[Feed] ✓ Loaded " << symbol << ": " << loaded << " bars" << std::endl;
    }

    total_bars_ = total_loaded;
    connected_ = true;

    utils::log_info("Total bars loaded: " + std::to_string(total_loaded));

    if (connection_callback_) {
        connection_callback_(true);
    }

    return true;
}

void MockMultiSymbolFeed::disconnect() {
    if (!connected_) {
        return;
    }

    stop();

    symbol_data_.clear();
    bars_replayed_ = 0;
    total_bars_ = 0;
    connected_ = false;

    utils::log_info("MockMultiSymbolFeed disconnected");

    if (connection_callback_) {
        connection_callback_(false);
    }
}

bool MockMultiSymbolFeed::is_connected() const {
    return connected_;
}

bool MockMultiSymbolFeed::start() {
    if (!connected_) {
        utils::log_error("Cannot start - not connected. Call connect() first.");
        return false;
    }

    if (active_) {
        utils::log_warning("MockMultiSymbolFeed already active");
        return true;
    }

    if (symbol_data_.empty()) {
        utils::log_error("No data loaded - call connect() first");
        return false;
    }

    utils::log_info("Starting replay (" +
                   std::to_string(config_.replay_speed) + "x speed)...");

    bars_replayed_ = 0;
    should_stop_ = false;
    active_ = true;

    // Start replay thread
    replay_thread_ = std::thread(&MockMultiSymbolFeed::replay_loop, this);

    return true;
}

void MockMultiSymbolFeed::stop() {
    if (!active_) {
        return;
    }

    utils::log_info("Stopping replay...");
    should_stop_ = true;
    active_ = false;

    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }

    utils::log_info("Replay stopped: " + std::to_string(bars_replayed_.load()) + " bars");
}

bool MockMultiSymbolFeed::is_active() const {
    return active_;
}

std::string MockMultiSymbolFeed::get_type() const {
    return "MockMultiSymbolFeed";
}

std::vector<std::string> MockMultiSymbolFeed::get_symbols() const {
    std::vector<std::string> symbols;
    for (const auto& [symbol, _] : config_.symbol_files) {
        symbols.push_back(symbol);
    }
    return symbols;
}

void MockMultiSymbolFeed::set_bar_callback(BarCallback callback) {
    bar_callback_ = callback;
}

void MockMultiSymbolFeed::set_error_callback(ErrorCallback callback) {
    error_callback_ = callback;
}

void MockMultiSymbolFeed::set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = callback;
}

IBarFeed::FeedStats MockMultiSymbolFeed::get_stats() const {
    FeedStats stats;
    stats.total_bars_received = bars_replayed_.load();
    stats.errors = errors_.load();
    stats.reconnects = 0;  // Mock doesn't reconnect
    stats.avg_latency_ms = 0.0;  // Mock has no latency

    // Per-symbol counts
    int i = 0;
    for (const auto& [symbol, data] : symbol_data_) {
        if (i < 10) {
            stats.bars_per_symbol[i] = static_cast<int>(data.current_index);
            i++;
        }
    }

    return stats;
}

// === Additional Mock-Specific Methods ===

void MockMultiSymbolFeed::replay_loop() {
    utils::log_info("Replay loop started");

    // Reset current_index for all symbols
    for (auto& [symbol, data] : symbol_data_) {
        data.current_index = 0;
    }

    // Replay until all symbols exhausted or stop requested
    while (!should_stop_ && replay_next_bar()) {
        // Sleep for replay speed
        if (config_.replay_speed > 0) {
            sleep_for_replay();
        }
    }

    active_ = false;
    utils::log_info("Replay loop complete: " + std::to_string(bars_replayed_.load()) + " bars");
}

bool MockMultiSymbolFeed::replay_next_bar() {
    // Check if any symbol has bars remaining
    bool has_data = false;
    for (const auto& [symbol, data] : symbol_data_) {
        if (data.current_index < data.bars.size()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        return false;  // All symbols exhausted
    }

    // If syncing timestamps, find common timestamp
    uint64_t target_timestamp = 0;

    if (config_.sync_timestamps) {
        // Find minimum timestamp across all symbols
        target_timestamp = UINT64_MAX;

        for (const auto& [symbol, data] : symbol_data_) {
            if (data.current_index < data.bars.size()) {
                uint64_t ts = data.bars[data.current_index].timestamp_ms;
                target_timestamp = std::min(target_timestamp, ts);
            }
        }
    }

    // Update all symbols at this timestamp
    std::string last_symbol;
    Bar last_bar;
    bool any_updated = false;

    // FIX 3: Track which symbols are updated for validation
    std::set<std::string> updated_symbols;
    int expected_symbols = symbol_data_.size();

    for (auto& [symbol, data] : symbol_data_) {
        if (data.current_index >= data.bars.size()) {
            continue;  // Symbol exhausted
        }

        const auto& bar = data.bars[data.current_index];

        // If syncing, only update if timestamp matches
        if (config_.sync_timestamps && bar.timestamp_ms != target_timestamp) {
            continue;
        }

        // Feed bar to data manager (direct update)
        if (data_manager_) {
            data_manager_->update_symbol(symbol, bar);
            updated_symbols.insert(symbol);
        }

        data.current_index++;
        bars_replayed_++;

        last_symbol = symbol;
        last_bar = bar;
        any_updated = true;
    }

    // FIX 3: Validate all expected symbols were updated
    static int validation_counter = 0;
    if (validation_counter++ % 100 == 0) {  // Check every 100 bars
        if (updated_symbols.size() != static_cast<size_t>(expected_symbols)) {
            utils::log_warning("[FEED VALIDATION] Only updated " +
                             std::to_string(updated_symbols.size()) +
                             " of " + std::to_string(expected_symbols) + " symbols at bar " +
                             std::to_string(bars_replayed_.load()));

            // Log which symbols are missing
            for (const auto& [symbol, data] : symbol_data_) {
                if (updated_symbols.count(symbol) == 0) {
                    utils::log_warning("  Missing update for: " + symbol +
                                     " (index: " + std::to_string(data.current_index) +
                                     "/" + std::to_string(data.bars.size()) + ")");
                }
            }
        }
    }

    // Call callback ONCE after all symbols updated (not for each symbol!)
    if (bar_callback_ && any_updated) {
        bar_callback_(last_symbol, last_bar);
    }

    return true;
}

std::map<std::string, int> MockMultiSymbolFeed::get_bar_counts() const {
    std::map<std::string, int> counts;
    for (const auto& [symbol, data] : symbol_data_) {
        counts[symbol] = static_cast<int>(data.bars.size());
    }
    return counts;
}

MockMultiSymbolFeed::Progress MockMultiSymbolFeed::get_progress() const {
    Progress prog;
    prog.bars_replayed = bars_replayed_;
    prog.total_bars = total_bars_;
    prog.progress_pct = (total_bars_ > 0) ?
        (static_cast<double>(bars_replayed_) / total_bars_ * 100.0) : 0.0;

    // Find current symbol/timestamp
    for (const auto& [symbol, data] : symbol_data_) {
        if (data.current_index < data.bars.size()) {
            prog.current_symbol = symbol;
            prog.current_timestamp = data.bars[data.current_index].timestamp_ms;
            break;
        }
    }

    return prog;
}

// === Private methods ===

int MockMultiSymbolFeed::load_csv(const std::string& symbol, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        utils::log_error("Cannot open file: " + filepath);
        return 0;
    }

    SymbolData data;
    std::string line;
    int line_num = 0;
    int loaded = 0;
    int parse_failures = 0;

    // Skip header
    if (std::getline(file, line)) {
        line_num++;
        utils::log_info("  Header: " + line.substr(0, std::min(100, (int)line.size())));
    }

    // Read data
    while (std::getline(file, line)) {
        line_num++;

        if (line.empty() || line[0] == '#') {
            continue;  // Skip empty/comment lines
        }

        Bar bar;
        if (parse_csv_line(line, bar)) {
            data.bars.push_back(bar);
            loaded++;

            // Log first bar as sample
            if (loaded == 1) {
                utils::log_info("  First bar: timestamp=" + std::to_string(bar.timestamp_ms) +
                              " close=" + std::to_string(bar.close));
            }
        } else {
            parse_failures++;
            if (parse_failures <= 3) {
                utils::log_warning("Failed to parse line " + std::to_string(line_num) +
                                  " in " + filepath + ": " + line.substr(0, 80));
            }
        }
    }

    file.close();

    if (parse_failures > 3) {
        utils::log_warning("  Total parse failures: " + std::to_string(parse_failures));
    }

    // Apply date filter if specified
    if (!config_.filter_date.empty() && loaded > 0) {
        // Parse filter_date (YYYY-MM-DD)
        int year, month, day;
        if (std::sscanf(config_.filter_date.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
            // Filter bars to only include this specific date
            std::deque<Bar> filtered_bars;
            for (const auto& bar : data.bars) {
                std::time_t bar_time = bar.timestamp_ms / 1000;
                std::tm* bar_tm = std::localtime(&bar_time);

                if (bar_tm &&
                    bar_tm->tm_year + 1900 == year &&
                    bar_tm->tm_mon + 1 == month &&
                    bar_tm->tm_mday == day) {
                    filtered_bars.push_back(bar);
                }
            }

            data.bars = std::move(filtered_bars);
            int filtered_count = static_cast<int>(data.bars.size());
            utils::log_info("  Date-filtered to " + std::to_string(filtered_count) +
                          " bars for " + symbol + " on " + config_.filter_date);
            loaded = filtered_count;
        } else {
            utils::log_warning("  Invalid filter_date format: " + config_.filter_date);
        }
    }

    if (loaded > 0) {
        symbol_data_[symbol] = std::move(data);
        utils::log_info("  Successfully loaded " + std::to_string(loaded) + " bars for " + symbol);
    } else {
        utils::log_error("  No bars loaded for " + symbol);
    }

    return loaded;
}

bool MockMultiSymbolFeed::parse_csv_line(const std::string& line, Bar& bar) {
    std::istringstream ss(line);
    std::string token;

    try {
        // Format: timestamp,open,high,low,close,volume
        // OR: ts_utc,ts_nyt_epoch,open,high,low,close,volume (7 columns)
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        // Support both 6-column and 7-column formats
        if (tokens.size() == 7) {
            // Format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            bar.timestamp_ms = std::stoull(tokens[1]) * 1000;  // Convert seconds to ms
            bar.open = std::stod(tokens[2]);
            bar.high = std::stod(tokens[3]);
            bar.low = std::stod(tokens[4]);
            bar.close = std::stod(tokens[5]);
            bar.volume = std::stoll(tokens[6]);
        } else if (tokens.size() >= 6) {
            // Format: timestamp,open,high,low,close,volume
            bar.timestamp_ms = std::stoull(tokens[0]);
            bar.open = std::stod(tokens[1]);
            bar.high = std::stod(tokens[2]);
            bar.low = std::stod(tokens[3]);
            bar.close = std::stod(tokens[4]);
            bar.volume = std::stoll(tokens[5]);
        } else {
            return false;
        }

        // Set bar_id (not in CSV, use timestamp)
        bar.bar_id = bar.timestamp_ms / 60000;  // Minutes since epoch

        return true;

    } catch (const std::exception& e) {
        return false;
    }
}

void MockMultiSymbolFeed::sleep_for_replay(int bars) {
    if (config_.replay_speed <= 0.0) {
        return;  // Instant replay
    }

    // 1 minute real-time = 60000 ms
    // At 39x speed: 60000 / 39 = 1538 ms per bar
    double ms_per_bar = 60000.0 / config_.replay_speed;
    int sleep_ms = static_cast<int>(ms_per_bar * bars);

    if (sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
}

} // namespace data
} // namespace sentio

```

## 📄 **FILE 25 of 30**: src/data/multi_symbol_data_manager.cpp

**File Information**:
- **Path**: `src/data/multi_symbol_data_manager.cpp`
- **Size**: 375 lines
- **Modified**: 2025-10-16 08:28:07
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "data/multi_symbol_data_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iostream>

namespace sentio {
namespace data {

MultiSymbolDataManager::MultiSymbolDataManager(const Config& config)
    : config_(config)
    , time_provider_(nullptr) {

    // Initialize state for each symbol
    for (const auto& symbol : config_.symbols) {
        symbol_states_[symbol] = SymbolState();
    }

    utils::log_info("MultiSymbolDataManager initialized with " +
                   std::to_string(config_.symbols.size()) + " symbols: " +
                   join_symbols());
}

MultiSymbolSnapshot MultiSymbolDataManager::get_latest_snapshot() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    MultiSymbolSnapshot snapshot;

    // In backtest mode, use the latest bar timestamp instead of wall-clock time
    // This prevents historical data from being marked as "stale"
    if (config_.backtest_mode) {
        // Find the latest timestamp across all symbols
        uint64_t max_ts = 0;
        for (const auto& [symbol, state] : symbol_states_) {
            if (state.update_count > 0 && state.last_update_ms > max_ts) {
                max_ts = state.last_update_ms;
            }
        }
        snapshot.logical_timestamp_ms = (max_ts > 0) ? max_ts : get_current_time_ms();
    } else {
        snapshot.logical_timestamp_ms = get_current_time_ms();
    }

    double total_staleness = 0.0;
    int stale_count = 0;

    // Build snapshot for each symbol
    for (const auto& symbol : config_.symbols) {
        auto it = symbol_states_.find(symbol);
        if (it == symbol_states_.end()) {
            continue;  // Symbol not tracked
        }

        const auto& state = it->second;

        if (state.update_count == 0) {
            // Never received data - skip this symbol
            snapshot.missing_symbols.push_back(symbol);
            continue;
        }

        SymbolSnapshot sym_snap;
        sym_snap.latest_bar = state.latest_bar;
        sym_snap.last_update_ms = state.last_update_ms;
        sym_snap.forward_fill_count = state.forward_fill_count;

        // Calculate staleness
        sym_snap.update_staleness(snapshot.logical_timestamp_ms);

        // Check if we need to forward-fill
        if (sym_snap.staleness_seconds > 60.0 &&
            state.forward_fill_count < config_.max_forward_fills) {

            // Forward-fill (use last known bar, update timestamp)
            sym_snap = forward_fill_symbol(symbol, snapshot.logical_timestamp_ms);
            snapshot.total_forward_fills++;
            total_forward_fills_++;

            if (config_.log_data_quality) {
                utils::log_warning("Forward-filling " + symbol +
                                 " (stale: " + std::to_string(sym_snap.staleness_seconds) +
                                 "s, fill #" + std::to_string(sym_snap.forward_fill_count) + ")");
            }
        }

        snapshot.snapshots[symbol] = sym_snap;

        total_staleness += sym_snap.staleness_seconds;
        stale_count++;

        // Track missing if too stale
        if (!sym_snap.is_valid) {
            snapshot.missing_symbols.push_back(symbol);
        }
    }

    // Calculate aggregate stats
    snapshot.avg_staleness_seconds = (stale_count > 0) ?
        (total_staleness / stale_count) : 0.0;

    snapshot.is_complete = snapshot.missing_symbols.empty();

    // Log quality issues
    if (config_.log_data_quality && !snapshot.is_complete) {
        utils::log_warning("Snapshot incomplete: " +
                          std::to_string(snapshot.missing_symbols.size()) +
                          "/" + std::to_string(config_.symbols.size()) +
                          " missing: " + join_vector(snapshot.missing_symbols));
    }

    return snapshot;
}

bool MultiSymbolDataManager::update_symbol(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    static int update_count = 0;
    if (update_count < 3) {
//         std::cout << "[DataMgr] update_symbol called for " << symbol << " (bar timestamp: " << bar.timestamp_ms << ")" << std::endl;
        update_count++;
    }

    // Check if symbol is tracked
    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end()) {
        utils::log_warning("Ignoring update for untracked symbol: " + symbol);
//         std::cout << "[DataMgr] ❌ Ignoring update for untracked symbol: " << symbol << std::endl;
        return false;
    }

    // Validate bar
    if (!validate_bar(symbol, bar)) {
        it->second.rejection_count++;
        total_rejections_++;
        return false;
    }

    auto& state = it->second;

    // Add to history
    state.history.push_back(bar);
    if (state.history.size() > static_cast<size_t>(config_.history_size)) {
        state.history.pop_front();
    }

    // Update latest
    state.latest_bar = bar;
    state.last_update_ms = bar.timestamp_ms;
    state.update_count++;
    state.forward_fill_count = 0;  // Reset forward fill counter

    total_updates_++;

    return true;
}

int MultiSymbolDataManager::update_all(const std::map<std::string, Bar>& bars) {
    int success_count = 0;
    for (const auto& [symbol, bar] : bars) {
        if (update_symbol(symbol, bar)) {
            success_count++;
        }
    }
    return success_count;
}

std::vector<Bar> MultiSymbolDataManager::get_recent_bars(
    const std::string& symbol,
    int count
) const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end() || it->second.history.empty()) {
        return {};
    }

    const auto& history = it->second.history;
    int available = static_cast<int>(history.size());
    int to_return = std::min(count, available);

    std::vector<Bar> result;
    result.reserve(to_return);

    // Return newest bars first
    auto start_it = history.end() - to_return;
    for (auto it = start_it; it != history.end(); ++it) {
        result.push_back(*it);
    }

    std::reverse(result.begin(), result.end());  // Newest first

    return result;
}

std::deque<Bar> MultiSymbolDataManager::get_all_bars(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end()) {
        return {};
    }

    return it->second.history;
}

MultiSymbolDataManager::DataQualityStats
MultiSymbolDataManager::get_quality_stats() const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    DataQualityStats stats;
    stats.total_updates = total_updates_.load();
    stats.total_forward_fills = total_forward_fills_.load();
    stats.total_rejections = total_rejections_.load();

    double total_avg_staleness = 0.0;
    int count = 0;

    for (const auto& [symbol, state] : symbol_states_) {
        stats.update_counts[symbol] = state.update_count;
        stats.forward_fill_counts[symbol] = state.forward_fill_count;

        if (state.update_count > 0) {
            double avg = state.cumulative_staleness / state.update_count;
            stats.avg_staleness[symbol] = avg;
            total_avg_staleness += avg;
            count++;
        }
    }

    stats.overall_avg_staleness = (count > 0) ?
        (total_avg_staleness / count) : 0.0;

    return stats;
}

void MultiSymbolDataManager::reset_stats() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    total_updates_ = 0;
    total_forward_fills_ = 0;
    total_rejections_ = 0;

    for (auto& [symbol, state] : symbol_states_) {
        state.update_count = 0;
        state.forward_fill_count = 0;
        state.rejection_count = 0;
        state.cumulative_staleness = 0.0;
    }
}

bool MultiSymbolDataManager::validate_bar(const std::string& symbol, const Bar& bar) {
    // Check 1: Timestamp is reasonable (not in future, not too old)
    // SKIP timestamp validation in backtest mode (historical data is expected to be old)
    if (!config_.backtest_mode) {
        uint64_t now = get_current_time_ms();
        if (bar.timestamp_ms > now + 60000) {  // Future by > 1 minute
            utils::log_error("Rejected " + symbol + " bar: timestamp in future (" +
                            std::to_string(bar.timestamp_ms) + " vs " +
                            std::to_string(now) + ")");
            return false;
        }

        if (bar.timestamp_ms < now - 86400000) {  // Older than 24 hours
            utils::log_warning("Rejected " + symbol + " bar: timestamp too old (" +
                              std::to_string((now - bar.timestamp_ms) / 1000) + "s)");
            return false;
        }
    }

    // Check 2: Price sanity (0.01 < price < 10000)
    if (bar.close <= 0.01 || bar.close > 10000.0) {
        utils::log_error("Rejected " + symbol + " bar: invalid price (" +
                        std::to_string(bar.close) + ")");
        return false;
    }

    // Check 3: OHLC consistency
    if (bar.low > bar.close || bar.high < bar.close ||
        bar.low > bar.open || bar.high < bar.open) {
        utils::log_warning("Rejected " + symbol + " bar: OHLC inconsistent (O=" +
                          std::to_string(bar.open) + " H=" +
                          std::to_string(bar.high) + " L=" +
                          std::to_string(bar.low) + " C=" +
                          std::to_string(bar.close) + ")");
        return false;
    }

    // Check 4: Volume non-negative
    if (bar.volume < 0) {
        utils::log_warning("Rejected " + symbol + " bar: negative volume (" +
                          std::to_string(bar.volume) + ")");
        return false;
    }

    // Check 5: Duplicate detection (same timestamp as last bar)
    auto it = symbol_states_.find(symbol);
    if (it != symbol_states_.end() && it->second.update_count > 0) {
        if (bar.timestamp_ms == it->second.last_update_ms) {
            // Duplicate - not necessarily an error, just skip
            return false;
        }

        // Check timestamp ordering (must be after last update)
        if (bar.timestamp_ms < it->second.last_update_ms) {
            utils::log_warning("Rejected " + symbol + " bar: out-of-order timestamp (" +
                              std::to_string(bar.timestamp_ms) + " < " +
                              std::to_string(it->second.last_update_ms) + ")");
            return false;
        }
    }

    return true;
}

SymbolSnapshot MultiSymbolDataManager::forward_fill_symbol(
    const std::string& symbol,
    uint64_t logical_time
) {
    SymbolSnapshot snap;

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end() || it->second.update_count == 0) {
        snap.is_valid = false;
        return snap;
    }

    auto& state = it->second;

    // Use last known bar, update timestamp
    snap.latest_bar = state.latest_bar;
    snap.latest_bar.timestamp_ms = logical_time;  // Forward-filled timestamp

    snap.last_update_ms = state.last_update_ms;  // Original update time
    snap.forward_fill_count = state.forward_fill_count + 1;

    // Update state forward fill counter
    state.forward_fill_count++;

    // Calculate staleness based on original update time
    snap.update_staleness(logical_time);

    // Mark invalid if too many forward fills
    if (snap.forward_fill_count >= config_.max_forward_fills) {
        snap.is_valid = false;
        utils::log_error("Symbol " + symbol + " exceeded max forward fills (" +
                        std::to_string(config_.max_forward_fills) + ")");
    }

    return snap;
}

// === Helper functions ===

std::string MultiSymbolDataManager::join_symbols() const {
    std::ostringstream oss;
    for (size_t i = 0; i < config_.symbols.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << config_.symbols[i];
    }
    return oss.str();
}

std::string MultiSymbolDataManager::join_vector(const std::vector<std::string>& vec) const {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << vec[i];
    }
    return oss.str();
}

} // namespace data
} // namespace sentio

```

## 📄 **FILE 26 of 30**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`
- **Size**: 611 lines
- **Modified**: 2025-10-16 22:13:31
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "features/unified_feature_engine.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

// OpenSSL for SHA1 hashing (already in dependencies)
#include <openssl/sha.h>

namespace sentio {
namespace features {

// =============================================================================
// SHA1 Hash Utility
// =============================================================================

std::string sha1_hex(const std::string& s) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(s.data()), s.size(), hash);

    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (unsigned char c : hash) {
        os << std::setw(2) << static_cast<int>(c);
    }
    return os.str();
}

// =============================================================================
// UnifiedFeatureEngineV2 Implementation
// =============================================================================

UnifiedFeatureEngine::UnifiedFeatureEngine(EngineConfig cfg)
    : cfg_(cfg),
      rsi14_(cfg.rsi14),
      rsi21_(cfg.rsi21),
      atr14_(cfg.atr14),
      bb20_(cfg.bb20, cfg.bb_k),
      stoch14_(cfg.stoch14),
      will14_(cfg.will14),
      macd_(),  // Uses default periods 12/26/9
      roc5_(cfg.roc5),
      roc10_(cfg.roc10),
      roc20_(cfg.roc20),
      cci20_(cfg.cci20),
      don20_(cfg.don20),
      keltner_(cfg.keltner_ema, cfg.keltner_atr, cfg.keltner_mult),
      obv_(),
      vwap_(),
      ema10_(cfg.ema10),
      ema20_(cfg.ema20),
      ema50_(cfg.ema50),
      sma10_ring_(cfg.sma10),
      sma20_ring_(cfg.sma20),
      sma50_ring_(cfg.sma50),
      scaler_(cfg.robust ? Scaler::Type::ROBUST : Scaler::Type::STANDARD)
{
    build_schema_();
    feats_.assign(schema_.names.size(), std::numeric_limits<double>::quiet_NaN());
}

void UnifiedFeatureEngine::build_schema_() {
    std::vector<std::string> n;

    // ==========================================================================
    // Time features (cyclical encoding for intraday patterns)
    // ==========================================================================
    if (cfg_.time) {
        n.push_back("time.hour_sin");
        n.push_back("time.hour_cos");
        n.push_back("time.minute_sin");
        n.push_back("time.minute_cos");
        n.push_back("time.dow_sin");
        n.push_back("time.dow_cos");
        n.push_back("time.dom_sin");
        n.push_back("time.dom_cos");
    }

    // ==========================================================================
    // Core price/volume features (NORMALIZED - always included)
    // ==========================================================================
    n.push_back("price.range_ratio");      // (high - low) / close
    n.push_back("price.body_ratio");       // (close - open) / close
    n.push_back("price.upper_wick_ratio"); // (high - close) / close
    n.push_back("price.lower_wick_ratio"); // (close - low) / close
    n.push_back("price.return_1");         // 1-bar return
    n.push_back("volume.change_ratio");    // volume change vs previous

    // ==========================================================================
    // Moving Averages (DEVIATION RATIOS - always included for baseline)
    // ==========================================================================
    n.push_back("sma10_dev");       // (close - sma10) / sma10
    n.push_back("sma20_dev");       // (close - sma20) / sma20
    n.push_back("sma50_dev");       // (close - sma50) / sma50
    n.push_back("ema10_dev");       // (close - ema10) / ema10
    n.push_back("ema20_dev");       // (close - ema20) / ema20
    n.push_back("ema50_dev");       // (close - ema50) / ema50
    n.push_back("price_vs_sma20");  // (close - sma20) / sma20 (duplicate for compatibility)
    n.push_back("price_vs_ema20");  // (close - ema20) / ema20 (duplicate for compatibility)

    // ==========================================================================
    // Volatility Features (NORMALIZED)
    // ==========================================================================
    if (cfg_.volatility) {
        n.push_back("atr14_pct");              // ATR / close
        n.push_back("bb20.mean_dev");          // (close - bb_mean) / close
        n.push_back("bb20.sd_pct");            // bb_sd / close
        n.push_back("bb20.upper_dev");         // (close - bb_upper) / close
        n.push_back("bb20.lower_dev");         // (close - bb_lower) / close
        n.push_back("bb20.percent_b");         // Already ratio
        n.push_back("bb20.bandwidth");         // Already ratio
        n.push_back("keltner.middle_dev");     // (close - keltner_mid) / close
        n.push_back("keltner.upper_dev");      // (close - keltner_up) / close
        n.push_back("keltner.lower_dev");      // (close - keltner_dn) / close
    }

    // ==========================================================================
    // Momentum Features
    // ==========================================================================
    if (cfg_.momentum) {
        n.push_back("rsi14");
        n.push_back("rsi21");
        n.push_back("stoch14.k");
        n.push_back("stoch14.d");
        n.push_back("stoch14.slow");
        n.push_back("will14");
        n.push_back("macd.line");
        n.push_back("macd.signal");
        n.push_back("macd.hist");
        n.push_back("roc5");
        n.push_back("roc10");
        n.push_back("roc20");
        n.push_back("cci20");
    }

    // ==========================================================================
    // Volume Features (NORMALIZED)
    // ==========================================================================
    if (cfg_.volume) {
        n.push_back("obv_scaled");       // OBV / (close * 1M)
        n.push_back("vwap_dist");        // (close - vwap) / vwap
    }

    // ==========================================================================
    // Donchian Channels (NORMALIZED as deviations)
    // ==========================================================================
    n.push_back("don20.up_dev");         // (close - don_up) / close
    n.push_back("don20.mid_dev");        // (close - don_mid) / close
    n.push_back("don20.dn_dev");         // (close - don_dn) / close
    n.push_back("don20.position");       // Already ratio  // (close - dn) / (up - dn)

    // ==========================================================================
    // Candlestick Pattern Features (from v1.0)
    // ==========================================================================
    if (cfg_.patterns) {
        n.push_back("pattern.doji");           // Body < 10% of range
        n.push_back("pattern.hammer");         // Lower shadow > 2x body
        n.push_back("pattern.shooting_star");  // Upper shadow > 2x body
        n.push_back("pattern.engulfing_bull"); // Bullish engulfing
        n.push_back("pattern.engulfing_bear"); // Bearish engulfing
    }

    // ==========================================================================
    // Finalize schema and compute hash
    // ==========================================================================
    schema_.names = std::move(n);

    // Concatenate names and critical config for hash
    std::ostringstream cat;
    for (const auto& name : schema_.names) {
        cat << name << "\n";
    }
    cat << "cfg:"
        << cfg_.rsi14 << ","
        << cfg_.bb20 << ","
        << cfg_.bb_k << ","
        << cfg_.macd_fast << ","
        << cfg_.macd_slow << ","
        << cfg_.macd_sig;

    schema_.sha1_hash = sha1_hex(cat.str());
}

bool UnifiedFeatureEngine::update(const Bar& b) {
    // ==========================================================================
    // Update all indicators (O(1) incremental)
    // ==========================================================================

    // Volatility
    atr14_.update(b.high, b.low, b.close);
    bb20_.update(b.close);
    keltner_.update(b.high, b.low, b.close);

    // Momentum
    rsi14_.update(b.close);
    rsi21_.update(b.close);
    stoch14_.update(b.high, b.low, b.close);
    will14_.update(b.high, b.low, b.close);
    macd_.update(b.close);
    roc5_.update(b.close);
    roc10_.update(b.close);
    roc20_.update(b.close);
    cci20_.update(b.high, b.low, b.close);

    // Channels
    don20_.update(b.high, b.low);

    // Volume
    obv_.update(b.close, b.volume);
    vwap_.update(b.close, b.volume);

    // Moving averages
    ema10_.update(b.close);
    ema20_.update(b.close);
    ema50_.update(b.close);
    sma10_ring_.push(b.close);
    sma20_ring_.push(b.close);
    sma50_ring_.push(b.close);

    // Store previous close and volume BEFORE updating (for 1-bar return calculation)
    prevPrevClose_ = prevClose_;
    prevPrevVolume_ = prevVolume_;

    // Calculate and store 1-bar return for volatility calculation
    if (!std::isnan(prevClose_) && prevClose_ > 0.0) {
        double bar_return = (b.close - prevClose_) / prevClose_;
        recent_returns_.push_back(bar_return);

        // Keep only last MAX_RETURNS_HISTORY returns
        if (recent_returns_.size() > MAX_RETURNS_HISTORY) {
            recent_returns_.pop_front();
        }
    }

    // Store current bar values for derived features
    prevTimestamp_ = b.timestamp_ms;
    prevClose_ = b.close;
    prevOpen_ = b.open;
    prevHigh_ = b.high;
    prevLow_ = b.low;
    prevVolume_ = b.volume;

    // Recompute feature vector
    recompute_vector_();

    seeded_ = true;
    ++bar_count_;
    return true;
}

void UnifiedFeatureEngine::recompute_vector_() {
    size_t k = 0;

    // ==========================================================================
    // Time features (cyclical encoding from v1.0)
    // ==========================================================================
    if (cfg_.time && prevTimestamp_ > 0) {
        time_t timestamp = prevTimestamp_ / 1000;
        struct tm* time_info = gmtime(&timestamp);

        if (time_info) {
            double hour = time_info->tm_hour;
            double minute = time_info->tm_min;
            double day_of_week = time_info->tm_wday;     // 0-6 (Sunday=0)
            double day_of_month = time_info->tm_mday;    // 1-31

            // Cyclical encoding (sine/cosine to preserve continuity)
            feats_[k++] = std::sin(2.0 * M_PI * hour / 24.0);           // hour_sin
            feats_[k++] = std::cos(2.0 * M_PI * hour / 24.0);           // hour_cos
            feats_[k++] = std::sin(2.0 * M_PI * minute / 60.0);         // minute_sin
            feats_[k++] = std::cos(2.0 * M_PI * minute / 60.0);         // minute_cos
            feats_[k++] = std::sin(2.0 * M_PI * day_of_week / 7.0);     // dow_sin
            feats_[k++] = std::cos(2.0 * M_PI * day_of_week / 7.0);     // dow_cos
            feats_[k++] = std::sin(2.0 * M_PI * day_of_month / 31.0);   // dom_sin
            feats_[k++] = std::cos(2.0 * M_PI * day_of_month / 31.0);   // dom_cos
        } else {
            // If time parsing fails, fill with NaN
            for (int i = 0; i < 8; ++i) {
                feats_[k++] = std::numeric_limits<double>::quiet_NaN();
            }
        }
    }

    // ==========================================================================
    // Core price/volume (NORMALIZED RATIOS - not raw prices!)
    // ==========================================================================
    // Range/Close ratio (typical: 0.01-0.05 for 1-5% range)
    double range = prevHigh_ - prevLow_;
    feats_[k++] = (prevClose_ != 0) ? range / prevClose_ : 0.0;

    // Body/Close ratio (typical: -0.02 to +0.02 for 2% moves)
    feats_[k++] = (prevClose_ != 0) ? (prevClose_ - prevOpen_) / prevClose_ : 0.0;

    // High/Close ratio (upper wick strength)
    feats_[k++] = (prevClose_ != 0) ? (prevHigh_ - prevClose_) / prevClose_ : 0.0;

    // Low/Close ratio (lower wick strength)
    feats_[k++] = (prevClose_ != 0) ? (prevClose_ - prevLow_) / prevClose_ : 0.0;

    // 1-bar return (already normalized - KEEP)
    feats_[k++] = safe_return(prevClose_, prevPrevClose_);

    // Volume change ratio (typical: -0.5 to +2.0)
    feats_[k++] = (!std::isnan(prevPrevVolume_) && prevPrevVolume_ > 0)
                  ? (prevVolume_ / prevPrevVolume_) - 1.0
                  : 0.0;

    // ==========================================================================
    // Moving Averages (DEVIATION RATIOS - not raw MA values!)
    // ==========================================================================
    double sma10 = sma10_ring_.full() ? sma10_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double sma20 = sma20_ring_.full() ? sma20_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double sma50 = sma50_ring_.full() ? sma50_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double ema10 = ema10_.get_value();
    double ema20 = ema20_.get_value();
    double ema50 = ema50_.get_value();

    // Price deviation from MAs (typical: -0.05 to +0.05 for 5% deviation)
    feats_[k++] = (!std::isnan(sma10) && sma10 != 0) ? (prevClose_ - sma10) / sma10 : 0.0;
    feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : 0.0;
    feats_[k++] = (!std::isnan(sma50) && sma50 != 0) ? (prevClose_ - sma50) / sma50 : 0.0;
    feats_[k++] = (!std::isnan(ema10) && ema10 != 0) ? (prevClose_ - ema10) / ema10 : 0.0;
    feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : 0.0;
    feats_[k++] = (!std::isnan(ema50) && ema50 != 0) ? (prevClose_ - ema50) / ema50 : 0.0;

    // Additional MA cross ratios (already using deviations - KEEP)
    feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : std::numeric_limits<double>::quiet_NaN();

    // ==========================================================================
    // Volatility (NORMALIZED)
    // ==========================================================================
    if (cfg_.volatility) {
        // ATR as percentage of price (typical: 0.01-0.05)
        feats_[k++] = (prevClose_ != 0 && !std::isnan(atr14_.value)) ? atr14_.value / prevClose_ : std::numeric_limits<double>::quiet_NaN();

        // Debug BB NaN issue - check Welford stats when BB produces NaN
        if (bar_count_ > 100 && std::isnan(bb20_.sd)) {
            static int late_nan_count = 0;
            if (late_nan_count < 10) {
                std::cerr << "[FeatureEngine CRITICAL] BB.sd is NaN!"
                          << " bar_count=" << bar_count_
                          << ", bb20_.win.size=" << bb20_.win.size()
                          << ", bb20_.win.capacity=" << bb20_.win.capacity()
                          << ", bb20_.win.full=" << bb20_.win.full()
                          << ", bb20_.win.welford_n=" << bb20_.win.welford_n()
                          << ", bb20_.win.welford_m2=" << bb20_.win.welford_m2()
                          << ", bb20_.win.variance=" << bb20_.win.variance()
                          << ", bb20_.is_ready=" << bb20_.is_ready()
                          << ", bb20_.mean=" << bb20_.mean
                          << ", bb20_.sd=" << bb20_.sd
                          << ", prevClose=" << prevClose_ << std::endl;
                late_nan_count++;
            }
        }

        size_t bb_start_idx = k;
        // Normalize BB bands as deviations from current price
        feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.mean)) ? (prevClose_ - bb20_.mean) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.sd)) ? bb20_.sd / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.upper)) ? (prevClose_ - bb20_.upper) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.lower)) ? (prevClose_ - bb20_.lower) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = bb20_.percent_b;  // Already a ratio (0-1)
        feats_[k++] = bb20_.bandwidth;  // Already a ratio

        // DEBUG: Commented out to reduce output noise
        // if (bar_count_ > 100) {
        //     static int bb_assign_debug = 0;
        //     if (bb_assign_debug < 3) {
        //         std::cerr << "[FeatureEngine] BB features assigned at indices " << bb_start_idx << "-" << (k-1)
        //                   << ", bb20_.mean=" << bb20_.mean
        //                   << ", bb20_.sd=" << bb20_.sd
        //                   << ", feats_[" << bb_start_idx << "]=" << feats_[bb_start_idx]
        //                   << ", feats_[" << (bb_start_idx+1) << "]=" << feats_[bb_start_idx+1]
        //                   << std::endl;
        //         bb_assign_debug++;
        //     }
        // }

        // Normalize Keltner channels as deviations from current price
        feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.middle)) ? (prevClose_ - keltner_.middle) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.upper)) ? (prevClose_ - keltner_.upper) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.lower)) ? (prevClose_ - keltner_.lower) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
    }

    // ==========================================================================
    // Momentum
    // ==========================================================================
    if (cfg_.momentum) {
        feats_[k++] = rsi14_.value;
        feats_[k++] = rsi21_.value;
        feats_[k++] = stoch14_.k;
        feats_[k++] = stoch14_.d;
        feats_[k++] = stoch14_.slow;
        feats_[k++] = will14_.r;
        feats_[k++] = macd_.macd;
        feats_[k++] = macd_.signal;
        feats_[k++] = macd_.hist;
        feats_[k++] = roc5_.value;
        feats_[k++] = roc10_.value;
        feats_[k++] = roc20_.value;
        feats_[k++] = cci20_.value;
    }

    // ==========================================================================
    // Volume
    // ==========================================================================
    if (cfg_.volume) {
        // OBV scaled by (price * 1M) to normalize magnitude (typical: -0.01 to +0.01)
        feats_[k++] = (prevClose_ != 0 && !std::isnan(obv_.value))
                      ? obv_.value / (prevClose_ * 1000000.0)
                      : std::numeric_limits<double>::quiet_NaN();

        // VWAP distance as ratio (typical: -0.02 to +0.02 for 2% deviation)
        double vwap_dist = (!std::isnan(vwap_.value) && vwap_.value != 0)
                           ? (prevClose_ - vwap_.value) / vwap_.value
                           : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = vwap_dist;
    }

    // ==========================================================================
    // Donchian
    // ==========================================================================
    // Donchian bands as price deviations (typical: -0.05 to +0.05 for 5% from extremes)
    feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.up))
                  ? (prevClose_ - don20_.up) / prevClose_
                  : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.mid))
                  ? (prevClose_ - don20_.mid) / prevClose_
                  : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.dn))
                  ? (prevClose_ - don20_.dn) / prevClose_
                  : std::numeric_limits<double>::quiet_NaN();

    // Donchian position: (close - dn) / (up - dn) - already normalized ratio (0 to 1)
    double don_range = don20_.up - don20_.dn;
    double don_pos = (don_range != 0 && !std::isnan(don20_.up) && !std::isnan(don20_.dn))
                     ? (prevClose_ - don20_.dn) / don_range
                     : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = don_pos;

    // ==========================================================================
    // Candlestick Pattern Features (from v1.0)
    // ==========================================================================
    if (cfg_.patterns) {
        double range = prevHigh_ - prevLow_;
        double body = std::abs(prevClose_ - prevOpen_);
        double upper_shadow = prevHigh_ - std::max(prevOpen_, prevClose_);
        double lower_shadow = std::min(prevOpen_, prevClose_) - prevLow_;

        // Doji: body < 10% of range
        bool is_doji = (range > 0) && (body / range < 0.1);
        feats_[k++] = is_doji ? 1.0 : 0.0;

        // Hammer: lower shadow > 2x body, upper shadow < body
        bool is_hammer = (lower_shadow > 2.0 * body) && (upper_shadow < body);
        feats_[k++] = is_hammer ? 1.0 : 0.0;

        // Shooting star: upper shadow > 2x body, lower shadow < body
        bool is_shooting_star = (upper_shadow > 2.0 * body) && (lower_shadow < body);
        feats_[k++] = is_shooting_star ? 1.0 : 0.0;

        // Engulfing patterns require previous bar - use prevPrevClose_
        bool engulfing_bull = false;
        bool engulfing_bear = false;
        if (!std::isnan(prevPrevClose_)) {
            bool prev_bearish = prevPrevClose_ < prevOpen_;  // Prev bar was bearish
            bool curr_bullish = prevClose_ > prevOpen_;       // Current bar is bullish
            bool engulfs = (prevOpen_ < prevPrevClose_) && (prevClose_ > prevOpen_);
            engulfing_bull = prev_bearish && curr_bullish && engulfs;

            bool prev_bullish = prevPrevClose_ > prevOpen_;
            bool curr_bearish = prevClose_ < prevOpen_;
            engulfs = (prevOpen_ > prevPrevClose_) && (prevClose_ < prevOpen_);
            engulfing_bear = prev_bullish && curr_bearish && engulfs;
        }
        feats_[k++] = engulfing_bull ? 1.0 : 0.0;
        feats_[k++] = engulfing_bear ? 1.0 : 0.0;
    }
}

int UnifiedFeatureEngine::warmup_remaining() const {
    // Conservative: max lookback across all indicators
    int max_period = std::max({
        cfg_.rsi14, cfg_.rsi21, cfg_.atr14, cfg_.bb20,
        cfg_.stoch14, cfg_.will14, cfg_.macd_slow, cfg_.don20,
        cfg_.sma50, cfg_.ema50
    });

    // Need at least max_period + 1 bars for all indicators to be valid
    int required_bars = max_period + 1;
    return std::max(0, required_bars - static_cast<int>(bar_count_));
}

std::vector<std::string> UnifiedFeatureEngine::get_unready_indicators() const {
    std::vector<std::string> unready;

    // Check each indicator's readiness
    if (!bb20_.is_ready()) unready.push_back("BB20");
    if (!rsi14_.is_ready()) unready.push_back("RSI14");
    if (!rsi21_.is_ready()) unready.push_back("RSI21");
    if (!atr14_.is_ready()) unready.push_back("ATR14");
    if (!stoch14_.is_ready()) unready.push_back("Stoch14");
    if (!will14_.is_ready()) unready.push_back("Will14");
    if (!don20_.is_ready()) unready.push_back("Don20");

    // Check moving averages
    if (bar_count_ < static_cast<size_t>(cfg_.sma10)) unready.push_back("SMA10");
    if (bar_count_ < static_cast<size_t>(cfg_.sma20)) unready.push_back("SMA20");
    if (bar_count_ < static_cast<size_t>(cfg_.sma50)) unready.push_back("SMA50");
    if (bar_count_ < static_cast<size_t>(cfg_.ema10)) unready.push_back("EMA10");
    if (bar_count_ < static_cast<size_t>(cfg_.ema20)) unready.push_back("EMA20");
    if (bar_count_ < static_cast<size_t>(cfg_.ema50)) unready.push_back("EMA50");

    return unready;
}

void UnifiedFeatureEngine::reset() {
    *this = UnifiedFeatureEngine(cfg_);
}

std::string UnifiedFeatureEngine::serialize() const {
    std::ostringstream os;
    os << std::setprecision(17);

    os << "prevTimestamp " << prevTimestamp_ << "\n";
    os << "prevClose " << prevClose_ << "\n";
    os << "prevPrevClose " << prevPrevClose_ << "\n";
    os << "prevOpen " << prevOpen_ << "\n";
    os << "prevHigh " << prevHigh_ << "\n";
    os << "prevLow " << prevLow_ << "\n";
    os << "prevVolume " << prevVolume_ << "\n";
    os << "bar_count " << bar_count_ << "\n";
    os << "obv " << obv_.value << "\n";
    os << "vwap " << vwap_.sumPV << " " << vwap_.sumV << "\n";

    // Add EMA/indicator states if exact resume needed
    // (Omitted for brevity; can be extended)

    return os.str();
}

void UnifiedFeatureEngine::restore(const std::string& blob) {
    reset();

    std::istringstream is(blob);
    std::string key;

    while (is >> key) {
        if (key == "prevTimestamp") is >> prevTimestamp_;
        else if (key == "prevClose") is >> prevClose_;
        else if (key == "prevPrevClose") is >> prevPrevClose_;
        else if (key == "prevOpen") is >> prevOpen_;
        else if (key == "prevHigh") is >> prevHigh_;
        else if (key == "prevLow") is >> prevLow_;
        else if (key == "prevVolume") is >> prevVolume_;
        else if (key == "bar_count") is >> bar_count_;
        else if (key == "obv") is >> obv_.value;
        else if (key == "vwap") is >> vwap_.sumPV >> vwap_.sumV;
    }
}

double UnifiedFeatureEngine::get_realized_volatility(int lookback) const {
    if (recent_returns_.empty() || static_cast<int>(recent_returns_.size()) < lookback) {
        return 0.0;  // Insufficient data
    }

    // Calculate standard deviation of returns over lookback period
    double sum = 0.0;
    int count = 0;

    // Get the last 'lookback' returns
    auto it = recent_returns_.rbegin();
    while (count < lookback && it != recent_returns_.rend()) {
        sum += *it;
        ++count;
        ++it;
    }

    double mean = sum / count;

    // Calculate variance
    double sum_sq_diff = 0.0;
    it = recent_returns_.rbegin();
    count = 0;
    while (count < lookback && it != recent_returns_.rend()) {
        double diff = *it - mean;
        sum_sq_diff += diff * diff;
        ++count;
        ++it;
    }

    double variance = sum_sq_diff / (count - 1);  // Sample variance
    return std::sqrt(variance);  // Standard deviation
}

double UnifiedFeatureEngine::get_annualized_volatility() const {
    double realized_vol = get_realized_volatility(20);  // 20-bar lookback

    // Annualize: volatility * sqrt(minutes per year)
    // Assuming 1-minute bars, 390 minutes/day, 252 trading days/year
    // Total minutes per year = 390 * 252 = 98,280
    double annualization_factor = std::sqrt(390.0 * 252.0);

    return realized_vol * annualization_factor;
}

} // namespace features
} // namespace sentio

```

## 📄 **FILE 27 of 30**: src/learning/online_predictor.cpp

**File Information**:
- **Path**: `src/learning/online_predictor.cpp`
- **Size**: 340 lines
- **Modified**: 2025-10-16 04:16:12
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "learning/online_predictor.h"
#include "common/utils.h"
#include <numeric>
#include <algorithm>

namespace sentio {
namespace learning {

OnlinePredictor::OnlinePredictor(size_t num_features, const Config& config)
    : config_(config), num_features_(num_features), samples_seen_(0),
      current_lambda_(config.lambda) {
    
    // Initialize parameters to zero
    theta_ = Eigen::VectorXd::Zero(num_features);
    
    // Initialize covariance with high uncertainty
    P_ = Eigen::MatrixXd::Identity(num_features, num_features) * config.initial_variance;
    
    utils::log_info("OnlinePredictor initialized with " + std::to_string(num_features) + 
                   " features, lambda=" + std::to_string(config.lambda));
}

OnlinePredictor::PredictionResult OnlinePredictor::predict(const std::vector<double>& features) {
    PredictionResult result;
    result.is_ready = is_ready();
    
    if (!result.is_ready) {
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }
    
    // Convert to Eigen vector
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(features.data(), features.size());
    
    // Linear prediction
    result.predicted_return = theta_.dot(x);
    
    // Confidence from prediction variance
    double prediction_variance = x.transpose() * P_ * x;
    result.confidence = 1.0 / (1.0 + std::sqrt(prediction_variance));
    
    // Current volatility estimate
    result.volatility_estimate = estimate_volatility();
    
    return result;
}

void OnlinePredictor::update(const std::vector<double>& features, double actual_return) {
    samples_seen_++;

    // Store return for volatility estimation
    recent_returns_.push_back(actual_return);
    if (recent_returns_.size() > HISTORY_SIZE) {
        recent_returns_.pop_front();
    }

    // Use Eigen::Map to avoid copy (zero-copy view of std::vector)
    Eigen::Map<const Eigen::VectorXd> x(features.data(), features.size());

    // Current prediction
    double predicted = theta_.dot(x);
    double error = actual_return - predicted;
    
    // Store error for diagnostics
    recent_errors_.push_back(error);
    if (recent_errors_.size() > HISTORY_SIZE) {
        recent_errors_.pop_front();
    }
    
    // Store direction accuracy
    bool correct_direction = (predicted > 0 && actual_return > 0) || 
                           (predicted < 0 && actual_return < 0);
    recent_directions_.push_back(correct_direction);
    if (recent_directions_.size() > HISTORY_SIZE) {
        recent_directions_.pop_front();
    }
    
    // EWRLS update with regularization
    double lambda_reg = current_lambda_ + config_.regularization;
    
    // Kalman gain
    Eigen::VectorXd Px = P_ * x;
    double denominator = lambda_reg + x.dot(Px);
    
    if (std::abs(denominator) < EPSILON) {
        utils::log_warning("Near-zero denominator in EWRLS update, skipping");
        return;
    }
    
    Eigen::VectorXd k = Px / denominator;

    // Update parameters
    theta_.noalias() += k * error;

    // Update covariance (optimized: reuse Px, avoid k * x.transpose() * P_)
    // P = (P - k * x' * P) / lambda = (P - k * Px') / lambda
    P_.noalias() -= k * Px.transpose();
    P_ /= current_lambda_;
    
    // Ensure numerical stability
    ensure_positive_definite();
    
    // Adapt learning rate if enabled
    if (config_.adaptive_learning && samples_seen_ % 10 == 0) {
        adapt_learning_rate(estimate_volatility());
    }
}

OnlinePredictor::PredictionResult OnlinePredictor::predict_and_update(
    const std::vector<double>& features, double actual_return) {
    
    auto result = predict(features);
    update(features, actual_return);
    return result;
}

void OnlinePredictor::adapt_learning_rate(double market_volatility) {
    // Higher volatility -> faster adaptation (lower lambda)
    // Lower volatility -> slower adaptation (higher lambda)
    
    double baseline_vol = 0.001;  // 0.1% baseline volatility
    double vol_ratio = market_volatility / baseline_vol;
    
    // Map volatility ratio to lambda
    // High vol (ratio=2) -> lambda=0.990
    // Low vol (ratio=0.5) -> lambda=0.999
    double target_lambda = config_.lambda - 0.005 * std::log(vol_ratio);
    target_lambda = std::clamp(target_lambda, config_.min_lambda, config_.max_lambda);
    
    // Smooth transition
    current_lambda_ = 0.9 * current_lambda_ + 0.1 * target_lambda;
    
    utils::log_debug("Adapted lambda: " + std::to_string(current_lambda_) + 
                    " (volatility=" + std::to_string(market_volatility) + ")");
}

bool OnlinePredictor::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Save config
        file.write(reinterpret_cast<const char*>(&config_), sizeof(Config));
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_lambda_), sizeof(double));
        
        // Save theta
        file.write(reinterpret_cast<const char*>(theta_.data()), 
                  sizeof(double) * theta_.size());
        
        // Save P (covariance)
        file.write(reinterpret_cast<const char*>(P_.data()), 
                  sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Saved predictor state to: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlinePredictor::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Load config
        file.read(reinterpret_cast<char*>(&config_), sizeof(Config));
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_lambda_), sizeof(double));
        
        // Load theta
        theta_.resize(num_features_);
        file.read(reinterpret_cast<char*>(theta_.data()), 
                 sizeof(double) * theta_.size());
        
        // Load P
        P_.resize(num_features_, num_features_);
        file.read(reinterpret_cast<char*>(P_.data()), 
                 sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Loaded predictor state from: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

double OnlinePredictor::get_recent_rmse() const {
    if (recent_errors_.empty()) return 0.0;
    
    double sum_sq = 0.0;
    for (double error : recent_errors_) {
        sum_sq += error * error;
    }
    return std::sqrt(sum_sq / recent_errors_.size());
}

double OnlinePredictor::get_directional_accuracy() const {
    if (recent_directions_.empty()) return 0.5;
    
    int correct = std::count(recent_directions_.begin(), recent_directions_.end(), true);
    return static_cast<double>(correct) / recent_directions_.size();
}

std::vector<double> OnlinePredictor::get_feature_importance() const {
    // Feature importance based on parameter magnitude * covariance
    std::vector<double> importance(num_features_);
    
    for (size_t i = 0; i < num_features_; ++i) {
        // Combine parameter magnitude with certainty (inverse variance)
        double param_importance = std::abs(theta_[i]);
        double certainty = 1.0 / (1.0 + std::sqrt(P_(i, i)));
        importance[i] = param_importance * certainty;
    }
    
    // Normalize
    double max_imp = *std::max_element(importance.begin(), importance.end());
    if (max_imp > 0) {
        for (double& imp : importance) {
            imp /= max_imp;
        }
    }
    
    return importance;
}

double OnlinePredictor::estimate_volatility() const {
    if (recent_returns_.size() < 20) return 0.001;  // Default 0.1%
    
    double mean = std::accumulate(recent_returns_.begin(), recent_returns_.end(), 0.0) 
                 / recent_returns_.size();
    
    double sum_sq = 0.0;
    for (double ret : recent_returns_) {
        sum_sq += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sum_sq / recent_returns_.size());
}

void OnlinePredictor::ensure_positive_definite() {
    // Eigenvalue decomposition
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(P_);
    Eigen::VectorXd eigenvalues = solver.eigenvalues();
    
    // Ensure all eigenvalues are positive
    bool needs_correction = false;
    for (int i = 0; i < eigenvalues.size(); ++i) {
        if (eigenvalues[i] < EPSILON) {
            eigenvalues[i] = EPSILON;
            needs_correction = true;
        }
    }
    
    if (needs_correction) {
        // Reconstruct with corrected eigenvalues
        P_ = solver.eigenvectors() * eigenvalues.asDiagonal() * solver.eigenvectors().transpose();
        utils::log_debug("Corrected covariance matrix for positive definiteness");
    }
}

// MultiHorizonPredictor Implementation

MultiHorizonPredictor::MultiHorizonPredictor(size_t num_features) 
    : num_features_(num_features) {
}

void MultiHorizonPredictor::add_horizon(int bars, double weight) {
    HorizonConfig config;
    config.horizon_bars = bars;
    config.weight = weight;

    // Adjust learning rate based on horizon
    config.predictor_config.lambda = 0.995 + 0.001 * std::log(bars);
    config.predictor_config.lambda = std::clamp(config.predictor_config.lambda, 0.990, 0.999);

    // Reduce warmup for multi-horizon learning
    // Updates arrive delayed by horizon length, so effective warmup is longer
    config.predictor_config.warmup_samples = 20;

    predictors_.emplace_back(std::make_unique<OnlinePredictor>(num_features_, config.predictor_config));
    configs_.push_back(config);

    utils::log_info("Added predictor for " + std::to_string(bars) + "-bar horizon");
}

OnlinePredictor::PredictionResult MultiHorizonPredictor::predict(const std::vector<double>& features) {
    OnlinePredictor::PredictionResult ensemble_result;
    ensemble_result.predicted_return = 0.0;
    ensemble_result.confidence = 0.0;
    ensemble_result.volatility_estimate = 0.0;
    
    double total_weight = 0.0;
    int ready_count = 0;
    
    for (size_t i = 0; i < predictors_.size(); ++i) {
        auto result = predictors_[i]->predict(features);
        
        if (result.is_ready) {
            double weight = configs_[i].weight * result.confidence;
            ensemble_result.predicted_return += result.predicted_return * weight;
            ensemble_result.confidence += result.confidence * configs_[i].weight;
            ensemble_result.volatility_estimate += result.volatility_estimate * configs_[i].weight;
            total_weight += weight;
            ready_count++;
        }
    }
    
    if (total_weight > 0) {
        ensemble_result.predicted_return /= total_weight;
        ensemble_result.confidence /= configs_.size();
        ensemble_result.volatility_estimate /= configs_.size();
        ensemble_result.is_ready = true;
    }
    
    return ensemble_result;
}

void MultiHorizonPredictor::update(int bars_ago, const std::vector<double>& features, 
                                   double actual_return) {
    // Update the appropriate predictor
    for (size_t i = 0; i < predictors_.size(); ++i) {
        if (configs_[i].horizon_bars == bars_ago) {
            predictors_[i]->update(features, actual_return);
            break;
        }
    }
}

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 28 of 30**: src/strategy/multi_symbol_oes_manager.cpp

**File Information**:
- **Path**: `src/strategy/multi_symbol_oes_manager.cpp`
- **Size**: 407 lines
- **Modified**: 2025-10-16 07:14:11
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "strategy/multi_symbol_oes_manager.h"
#include "common/utils.h"
#include <iostream>

namespace sentio {

MultiSymbolOESManager::MultiSymbolOESManager(
    const Config& config,
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr
)
    : config_(config)
    , data_mgr_(data_mgr) {

    utils::log_info("MultiSymbolOESManager initializing for " +
                   std::to_string(config_.symbols.size()) + " symbols");

    // Create OES instance for each symbol
    for (const auto& symbol : config_.symbols) {
        // Use symbol-specific config if available, otherwise use base config
        OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;
        if (config_.symbol_configs.count(symbol) > 0) {
            oes_config = config_.symbol_configs.at(symbol);
            utils::log_info("  " + symbol + ": Using custom config");
        } else {
            oes_config = config_.base_config;
            utils::log_info("  " + symbol + ": Using base config");
        }

        // Create OES instance
        auto oes = std::make_unique<OnlineEnsembleStrategy>(oes_config);
        oes_instances_[symbol] = std::move(oes);
    }

    utils::log_info("MultiSymbolOESManager initialized: " +
                   std::to_string(oes_instances_.size()) + " instances created");
}

// === Signal Generation ===

std::map<std::string, SignalOutput> MultiSymbolOESManager::generate_all_signals() {
    std::map<std::string, SignalOutput> signals;

    auto snapshot = data_mgr_->get_latest_snapshot();

    // DEBUG: Comment out for performance
    // static int debug_count = 0;
    // if (debug_count < 5) {
    //     utils::log_info("DEBUG generate_all_signals: snapshot has " +
    //                    std::to_string(snapshot.snapshots.size()) + " symbols");
    //     std::cout << "[OES] generate_all_signals: snapshot has " << snapshot.snapshots.size() << " symbols: ";
    //     for (const auto& [symbol, _] : snapshot.snapshots) {
    //         std::cout << symbol << " ";
    //     }
    //     std::cout << std::endl;
    //     debug_count++;
    // }

    for (const auto& symbol : config_.symbols) {
        // Check if symbol has valid data
        if (snapshot.snapshots.count(symbol) == 0) {
            static std::map<std::string, int> warning_counts;
            if (warning_counts[symbol] < 3) {
                utils::log_warning("No data for " + symbol + " - skipping signal");
                // std::cout << "[OES]   " << symbol << ": No data in snapshot - skipping" << std::endl;
                warning_counts[symbol]++;
            }
            continue;
        }

        const auto& sym_snap = snapshot.snapshots.at(symbol);
        if (!sym_snap.is_valid) {
            static std::map<std::string, int> stale_counts;
            if (stale_counts[symbol] < 3) {
                utils::log_warning("Stale data for " + symbol + " (" +
                                 std::to_string(sym_snap.staleness_seconds) + "s) - skipping signal");
                // std::cout << "[OES]   " << symbol << ": Stale data (" << sym_snap.staleness_seconds << "s) - skipping" << std::endl;
                stale_counts[symbol]++;
            }
            continue;
        }

        // Get OES instance
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            utils::log_error("No OES instance for " + symbol);
            // std::cout << "[OES]   " << symbol << ": No OES instance - skipping" << std::endl;
            continue;
        }

        // Check if OES is ready
        if (!it->second->is_ready()) {
            static std::map<std::string, int> not_ready_counts;
            if (not_ready_counts[symbol] < 3) {
                // std::cout << "[OES]   " << symbol << ": OES not ready - skipping" << std::endl;
                not_ready_counts[symbol]++;
            }
            continue;
        }

        // Generate signal
        SignalOutput signal = it->second->generate_signal(sym_snap.latest_bar);

        // DEBUG: Comment out for performance
        // static int nan_signal_count = 0;
        // if (nan_signal_count < 5 && signal.probability == 0.5) {
        //     std::cout << "[OES]   " << symbol << ": NEUTRAL signal (prob=0.5) - might be due to NaN features" << std::endl;
        //     nan_signal_count++;
        // }

        // Apply staleness weighting to probability
        // Reduce confidence in signal if data is old
        signal.probability *= sym_snap.staleness_weight;

        signals[symbol] = signal;
        total_signals_generated_++;

        // DEBUG: Comment out for performance
        // static int signal_debug_count = 0;
        // if (signal_debug_count < 3) {
        //     std::cout << "[OES]   " << symbol << ": Generated signal (type=" << static_cast<int>(signal.signal_type)
        //               << ", prob=" << signal.probability << ")" << std::endl;
        //     signal_debug_count++;
        // }
    }

    // DEBUG: Comment out for performance
    // std::cout << "[OES] Returning " << signals.size() << " signals" << std::endl;
    return signals;
}

SignalOutput MultiSymbolOESManager::generate_signal(const std::string& symbol) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return SignalOutput();  // Return empty signal
    }

    Bar bar;
    if (!get_latest_bar(symbol, bar)) {
        utils::log_warning("No valid bar for " + symbol);
        return SignalOutput();
    }

    SignalOutput signal = it->second->generate_signal(bar);
    total_signals_generated_++;

    return signal;
}

// === Learning Updates ===

void MultiSymbolOESManager::update_all(const std::map<std::string, double>& realized_pnls) {
    auto snapshot = data_mgr_->get_latest_snapshot();

    for (const auto& [symbol, realized_pnl] : realized_pnls) {
        // Get OES instance
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            utils::log_warning("No OES instance for " + symbol + " - cannot update");
            continue;
        }

        // Get latest bar
        if (snapshot.snapshots.count(symbol) == 0) {
            utils::log_warning("No data for " + symbol + " - cannot update");
            continue;
        }

        const auto& bar = snapshot.snapshots.at(symbol).latest_bar;

        // Update OES
        it->second->update(bar, realized_pnl);
        total_updates_++;
    }
}

void MultiSymbolOESManager::update(const std::string& symbol, double realized_pnl) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return;
    }

    Bar bar;
    if (!get_latest_bar(symbol, bar)) {
        utils::log_warning("No valid bar for " + symbol);
        return;
    }

    it->second->update(bar, realized_pnl);
    total_updates_++;
}

void MultiSymbolOESManager::on_bar() {
    auto snapshot = data_mgr_->get_latest_snapshot();

    for (const auto& symbol : config_.symbols) {
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            continue;
        }

        // Get latest bar
        if (snapshot.snapshots.count(symbol) == 0) {
            continue;
        }

        const auto& bar = snapshot.snapshots.at(symbol).latest_bar;

        // Call on_bar for each OES
        it->second->on_bar(bar);
    }
}

// === Warmup ===

bool MultiSymbolOESManager::warmup_all(
    const std::map<std::string, std::vector<Bar>>& symbol_bars
) {
    utils::log_info("Warming up all OES instances...");

    bool all_success = true;
    for (const auto& [symbol, bars] : symbol_bars) {
        if (!warmup(symbol, bars)) {
            utils::log_error("Warmup failed for " + symbol);
            all_success = false;
        }
    }

    if (all_success) {
        utils::log_info("All OES instances warmed up successfully");
    } else {
        utils::log_warning("Some OES instances failed warmup");
    }

    return all_success;
}

bool MultiSymbolOESManager::warmup(const std::string& symbol, const std::vector<Bar>& bars) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return false;
    }

    utils::log_info("Warming up " + symbol + " with " + std::to_string(bars.size()) + " bars...");
    // DEBUG: Comment out for performance
    // std::cout << "[OESManager::warmup] Starting warmup for " << symbol
    //           << " with " << bars.size() << " bars" << std::endl;

    // Feed bars one by one
    for (size_t i = 0; i < bars.size(); ++i) {
        it->second->on_bar(bars[i]);

        // DEBUG: Comment out for performance
        // if (i < 3) {
        //     std::cout << "[OESManager::warmup]   Bar " << i << " processed" << std::endl;
        // }
    }

    // DEBUG: Comment out for performance
    // std::cout << "[OESManager::warmup] Completed " << bars.size() << " warmup bars for " << symbol << std::endl;

    // Check if ready
    bool ready = it->second->is_ready();
    if (ready) {
        utils::log_info("  " + symbol + ": Warmup complete - ready for trading");
        // std::cout << "[OESManager::warmup]   " << symbol << ": READY" << std::endl;
    } else {
        utils::log_warning("  " + symbol + ": Warmup incomplete - needs more data");
        // std::cout << "[OESManager::warmup]   " << symbol << ": NOT READY" << std::endl;
    }

    return ready;
}

// === Configuration ===

void MultiSymbolOESManager::update_config(
    const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config
) {
    utils::log_info("Updating config for all OES instances");

    config_.base_config = new_config;

    for (auto& [symbol, oes] : oes_instances_) {
        // Only update if not using custom config
        if (config_.symbol_configs.count(symbol) == 0) {
            oes->update_config(new_config);
        }
    }
}

void MultiSymbolOESManager::update_config(
    const std::string& symbol,
    const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config
) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return;
    }

    utils::log_info("Updating config for " + symbol);
    it->second->update_config(new_config);

    // Save as custom config
    config_.symbol_configs[symbol] = new_config;
}

// === Diagnostics ===

std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics>
MultiSymbolOESManager::get_all_performance_metrics() const {
    std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics> metrics;

    for (const auto& [symbol, oes] : oes_instances_) {
        metrics[symbol] = oes->get_performance_metrics();
    }

    return metrics;
}

OnlineEnsembleStrategy::PerformanceMetrics
MultiSymbolOESManager::get_performance_metrics(const std::string& symbol) const {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return OnlineEnsembleStrategy::PerformanceMetrics();
    }

    return it->second->get_performance_metrics();
}

bool MultiSymbolOESManager::all_ready() const {
    for (const auto& [symbol, oes] : oes_instances_) {
        if (!oes->is_ready()) {
            // Log which symbol isn't ready and why (debug only, limit output)
            static std::map<std::string, int> log_count;
            if (log_count[symbol] < 3) {
                std::cout << "[MultiSymbolOES] " << symbol << " not ready" << std::endl;
                log_count[symbol]++;
            }
            return false;
        }
    }
    return !oes_instances_.empty();
}

std::map<std::string, bool> MultiSymbolOESManager::get_ready_status() const {
    std::map<std::string, bool> status;

    for (const auto& [symbol, oes] : oes_instances_) {
        status[symbol] = oes->is_ready();
    }

    return status;
}

std::map<std::string, OnlineEnsembleStrategy::LearningState>
MultiSymbolOESManager::get_all_learning_states() const {
    std::map<std::string, OnlineEnsembleStrategy::LearningState> states;

    for (const auto& [symbol, oes] : oes_instances_) {
        states[symbol] = oes->get_learning_state();
    }

    return states;
}

OnlineEnsembleStrategy* MultiSymbolOESManager::get_oes_instance(const std::string& symbol) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const OnlineEnsembleStrategy* MultiSymbolOESManager::get_oes_instance(
    const std::string& symbol
) const {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        return nullptr;
    }
    return it->second.get();
}

// === Private Methods ===

bool MultiSymbolOESManager::get_latest_bar(const std::string& symbol, Bar& bar) {
    auto snapshot = data_mgr_->get_latest_snapshot();

    if (snapshot.snapshots.count(symbol) == 0) {
        return false;
    }

    const auto& sym_snap = snapshot.snapshots.at(symbol);
    if (!sym_snap.is_valid) {
        return false;
    }

    bar = sym_snap.latest_bar;
    return true;
}

} // namespace sentio

```

## 📄 **FILE 29 of 30**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`
- **Size**: 580 lines
- **Modified**: 2025-10-16 22:13:12
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace sentio {

OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : StrategyComponent(config),
      config_(config),
      samples_seen_(0),
      current_buy_threshold_(config.buy_threshold),
      current_sell_threshold_(config.sell_threshold),
      calibration_count_(0) {

    // Initialize feature engine first to get actual feature count
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>();

    // Initialize multi-horizon EWRLS predictor with correct feature count
    size_t num_features = feature_engine_->features_view().size();
    if (num_features == 0) {
        // Feature engine not ready yet, use schema names count
        num_features = feature_engine_->names().size();
    }
    ensemble_predictor_ = std::make_unique<learning::MultiHorizonPredictor>(num_features);

    // Add predictors for each horizon with reduced warmup
    // EWRLS predictor warmup should be much smaller than strategy warmup
    // because updates are delayed by horizon length
    learning::OnlinePredictor::Config predictor_config;
    predictor_config.warmup_samples = 50;  // Lower warmup for EWRLS
    predictor_config.lambda = config_.ewrls_lambda;
    predictor_config.initial_variance = config_.initial_variance;
    predictor_config.regularization = config_.regularization;
    predictor_config.adaptive_learning = config_.enable_adaptive_learning;
    predictor_config.min_lambda = config_.min_lambda;
    predictor_config.max_lambda = config_.max_lambda;

    for (size_t i = 0; i < config_.prediction_horizons.size(); ++i) {
        int horizon = config_.prediction_horizons[i];
        double weight = config_.horizon_weights[i];
        // Need to pass config to add_horizon - but API doesn't support it
        // Will need to modify MultiHorizonPredictor
        ensemble_predictor_->add_horizon(horizon, weight);
    }

    utils::log_info("OnlineEnsembleStrategy initialized with " +
                   std::to_string(config_.prediction_horizons.size()) + " horizons, " +
                   std::to_string(num_features) + " features");
}

void OnlineEnsembleStrategy::train_predictor(const std::vector<double>& features, double realized_return) {
    // Train all horizons with the same realized return (for warmup)
    for (size_t i = 0; i < config_.prediction_horizons.size(); ++i) {
        int horizon = config_.prediction_horizons[i];
        ensemble_predictor_->update(horizon, features, realized_return);
    }
}

SignalOutput OnlineEnsembleStrategy::generate_signal(const Bar& bar) {
    SignalOutput output;
    output.bar_id = bar.bar_id;
    output.timestamp_ms = bar.timestamp_ms;
    output.bar_index = samples_seen_;
    output.symbol = "UNKNOWN";  // Set by caller if needed
    output.strategy_name = config_.name;
    output.strategy_version = config_.version;

    // Wait for warmup
    if (!is_ready()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        return output;
    }

    // Extract features
    std::vector<double> features = extract_features(bar);
    if (features.empty()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        return output;
    }

    // Get ensemble prediction
    auto prediction = ensemble_predictor_->predict(features);

    // Convert predicted return to probability
    // Predicted return is in decimal (e.g., 0.01 = 1% return)
    // Map to probability: positive return -> prob > 0.5, negative -> prob < 0.5
    double base_prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    base_prob = std::clamp(base_prob, 0.05, 0.95);  // Keep within reasonable bounds

    // Apply Bollinger Bands amplification if enabled
    double prob = base_prob;
    if (config_.enable_bb_amplification) {
        BollingerBands bb = calculate_bollinger_bands();
        prob = apply_bb_amplification(base_prob, bb);

        // Store BB metadata
        output.metadata["bb_upper"] = std::to_string(bb.upper);
        output.metadata["bb_middle"] = std::to_string(bb.middle);
        output.metadata["bb_lower"] = std::to_string(bb.lower);
        output.metadata["bb_position"] = std::to_string(bb.position_pct);
        output.metadata["base_probability"] = std::to_string(base_prob);
    }

    output.probability = prob;
    output.confidence = prediction.confidence;
    output.signal_type = determine_signal(prob);

    // Track for multi-horizon updates (always, not just for non-neutral signals)
    // This allows the model to learn from all market data, not just when we trade
    bool is_long = (prob > 0.5);  // Use probability, not signal type
    for (int horizon : config_.prediction_horizons) {
        track_prediction(samples_seen_, horizon, features, bar.close, is_long);
    }

    // Add metadata
    output.metadata["confidence"] = std::to_string(prediction.confidence);
    output.metadata["volatility"] = std::to_string(prediction.volatility_estimate);
    output.metadata["buy_threshold"] = std::to_string(current_buy_threshold_);
    output.metadata["sell_threshold"] = std::to_string(current_sell_threshold_);

    return output;
}

void OnlineEnsembleStrategy::update(const Bar& bar, double realized_pnl) {
    // Update performance metrics
    if (std::abs(realized_pnl) > 1e-6) {  // Non-zero P&L
        double return_pct = realized_pnl / 100000.0;  // Assuming $100k base
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }

    // Process pending horizon updates
    process_pending_updates(bar);
}

void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine
    feature_engine_->update(bar);

    samples_seen_++;

    // CRITICAL FIX: Train predictor during warmup
    // This ensures the predictor learns from historical data before making predictions
    if (!is_ready() && bar_history_.size() >= 2 && feature_engine_->warmup_remaining() == 0) {
        // Get previous bar (we're training on previous bar to predict current bar)
        const Bar& prev_bar = bar_history_[bar_history_.size() - 2];

        // Calculate realized return from previous bar to current bar
        double realized_return = (bar.close - prev_bar.close) / prev_bar.close;

        // Extract features at previous bar
        // Note: features are already calculated for current state after update
        std::vector<double> features = feature_engine_->features_view();

        if (!features.empty()) {
            // Train predictor with features and realized return
            train_predictor(features, realized_return);

            // DEBUG: Commented out to reduce output noise
            // if (samples_seen_ % 100 == 0) {
            //     utils::log_debug("Warmup training: bar " + std::to_string(samples_seen_) +
            //                    "/" + std::to_string(config_.warmup_samples) +
            //                    ", return=" + std::to_string(realized_return * 100.0) + "%");
            // }
        }
    }

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // Process any pending updates for this bar
    process_pending_updates(bar);
}

std::vector<double> OnlineEnsembleStrategy::extract_features(const Bar& current_bar) {
    if (bar_history_.size() < MIN_FEATURES_BARS) {
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (feature_engine_->warmup_remaining() > 0) {
        return {};
    }

    return feature_engine_->features_view();
}

void OnlineEnsembleStrategy::track_prediction(int bar_index, int horizon,
                                              const std::vector<double>& features,
                                              double entry_price, bool is_long) {
    // Create shared_ptr only once per bar (reuse for all horizons)
    static std::shared_ptr<const std::vector<double>> shared_features;
    static int last_bar_index = -1;

    if (bar_index != last_bar_index) {
        // New bar - create new shared features
        shared_features = std::make_shared<const std::vector<double>>(features);
        last_bar_index = bar_index;
    }

    HorizonPrediction pred;
    pred.entry_bar_index = bar_index;
    pred.target_bar_index = bar_index + horizon;
    pred.horizon = horizon;
    pred.features = shared_features;  // Share, don't copy
    pred.entry_price = entry_price;
    pred.is_long = is_long;

    // Use fixed array instead of vector
    auto& update = pending_updates_[pred.target_bar_index];
    if (update.count < 3) {
        update.horizons[update.count++] = std::move(pred);  // Move, don't copy
    }
}

void OnlineEnsembleStrategy::process_pending_updates(const Bar& current_bar) {
    auto it = pending_updates_.find(samples_seen_);
    if (it != pending_updates_.end()) {
        const auto& update = it->second;

        // Process only the valid predictions (0 to count-1)
        for (uint8_t i = 0; i < update.count; ++i) {
            const auto& pred = update.horizons[i];

            double actual_return = (current_bar.close - pred.entry_price) / pred.entry_price;
            if (!pred.is_long) {
                actual_return = -actual_return;
            }

            // Dereference shared_ptr only when needed
            ensemble_predictor_->update(pred.horizon, *pred.features, actual_return);
        }

        if (samples_seen_ % 100 == 0) {
            utils::log_debug("Processed " + std::to_string(static_cast<int>(update.count)) +
                           " updates at bar " + std::to_string(samples_seen_) +
                           ", pending_count=" + std::to_string(pending_updates_.size()));
        }

        pending_updates_.erase(it);
    }
}

SignalType OnlineEnsembleStrategy::determine_signal(double probability) const {
    if (probability > current_buy_threshold_) {
        return SignalType::LONG;
    } else if (probability < current_sell_threshold_) {
        return SignalType::SHORT;
    } else {
        return SignalType::NEUTRAL;
    }
}

void OnlineEnsembleStrategy::update_performance_metrics(bool won, double return_pct) {
    TradeResult result;
    result.won = won;
    result.return_pct = return_pct;
    result.timestamp = 0;  // Could add actual timestamp

    recent_trades_.push_back(result);
    if (recent_trades_.size() > TRADE_HISTORY_SIZE) {
        recent_trades_.pop_front();
    }
}

void OnlineEnsembleStrategy::calibrate_thresholds() {
    if (recent_trades_.size() < 50) {
        return;  // Not enough data
    }

    // Calculate current win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    double win_rate = static_cast<double>(wins) / recent_trades_.size();

    // Adjust thresholds to hit target win rate
    if (win_rate < config_.target_win_rate) {
        // Win rate too low -> make thresholds more selective (move apart)
        current_buy_threshold_ += config_.threshold_step;
        current_sell_threshold_ -= config_.threshold_step;
    } else if (win_rate > config_.target_win_rate + 0.05) {
        // Win rate too high -> trade more (move together)
        current_buy_threshold_ -= config_.threshold_step;
        current_sell_threshold_ += config_.threshold_step;
    }

    // Keep within reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.70);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.30, 0.49);

    // Ensure minimum separation
    double min_separation = 0.04;
    if (current_buy_threshold_ - current_sell_threshold_ < min_separation) {
        double center = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = center + min_separation / 2.0;
        current_sell_threshold_ = center - min_separation / 2.0;
    }

    calibration_count_++;
    utils::log_info("Calibrated thresholds #" + std::to_string(calibration_count_) +
                   ": buy=" + std::to_string(current_buy_threshold_) +
                   ", sell=" + std::to_string(current_sell_threshold_) +
                   " (win_rate=" + std::to_string(win_rate) + ")");
}

OnlineEnsembleStrategy::PerformanceMetrics
OnlineEnsembleStrategy::get_performance_metrics() const {
    PerformanceMetrics metrics;

    if (recent_trades_.empty()) {
        return metrics;
    }

    // Win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    metrics.win_rate = static_cast<double>(wins) / recent_trades_.size();
    metrics.total_trades = static_cast<int>(recent_trades_.size());

    // Average return
    double sum_returns = std::accumulate(recent_trades_.begin(), recent_trades_.end(), 0.0,
                                        [](double sum, const TradeResult& r) {
                                            return sum + r.return_pct;
                                        });
    metrics.avg_return = sum_returns / recent_trades_.size();

    // Monthly return estimate (assuming 252 trading days, ~21 per month)
    // If we have N trades over M bars, estimate monthly trades
    if (samples_seen_ > 0) {
        double trades_per_bar = static_cast<double>(recent_trades_.size()) / std::min(samples_seen_, 500);
        double bars_per_month = 21.0 * 390.0;  // 21 days * 390 minutes (6.5 hours)
        double monthly_trades = trades_per_bar * bars_per_month;
        metrics.monthly_return_estimate = metrics.avg_return * monthly_trades;
    }

    // Sharpe estimate
    if (recent_trades_.size() > 10) {
        double mean = metrics.avg_return;
        double sum_sq = 0.0;
        for (const auto& trade : recent_trades_) {
            double diff = trade.return_pct - mean;
            sum_sq += diff * diff;
        }
        double std_dev = std::sqrt(sum_sq / recent_trades_.size());
        if (std_dev > 1e-8) {
            metrics.sharpe_estimate = mean / std_dev * std::sqrt(252.0);  // Annualized
        }
    }

    // Check if targets met
    metrics.targets_met = (metrics.win_rate >= config_.target_win_rate) &&
                         (metrics.monthly_return_estimate >= config_.target_monthly_return);

    return metrics;
}

std::vector<double> OnlineEnsembleStrategy::get_feature_importance() const {
    // Get feature importance from first predictor (they should be similar)
    // Would need to expose this through MultiHorizonPredictor
    // For now return empty
    return {};
}

bool OnlineEnsembleStrategy::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Save basic state
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_buy_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&current_sell_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&calibration_count_), sizeof(int));

        // Save trade history size
        size_t trade_count = recent_trades_.size();
        file.write(reinterpret_cast<const char*>(&trade_count), sizeof(size_t));

        // Save trades
        for (const auto& trade : recent_trades_) {
            file.write(reinterpret_cast<const char*>(&trade.won), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&trade.return_pct), sizeof(double));
            file.write(reinterpret_cast<const char*>(&trade.timestamp), sizeof(int64_t));
        }

        file.close();
        utils::log_info("Saved OnlineEnsembleStrategy state to: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlineEnsembleStrategy::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Load basic state
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_buy_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&current_sell_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&calibration_count_), sizeof(int));

        // Load trade history
        size_t trade_count;
        file.read(reinterpret_cast<char*>(&trade_count), sizeof(size_t));

        recent_trades_.clear();
        for (size_t i = 0; i < trade_count; ++i) {
            TradeResult trade;
            file.read(reinterpret_cast<char*>(&trade.won), sizeof(bool));
            file.read(reinterpret_cast<char*>(&trade.return_pct), sizeof(double));
            file.read(reinterpret_cast<char*>(&trade.timestamp), sizeof(int64_t));
            recent_trades_.push_back(trade);
        }

        file.close();
        utils::log_info("Loaded OnlineEnsembleStrategy state from: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

// Bollinger Bands calculation
OnlineEnsembleStrategy::BollingerBands OnlineEnsembleStrategy::calculate_bollinger_bands() const {
    BollingerBands bb;
    bb.upper = 0.0;
    bb.middle = 0.0;
    bb.lower = 0.0;
    bb.bandwidth = 0.0;
    bb.position_pct = 0.5;

    if (bar_history_.size() < static_cast<size_t>(config_.bb_period)) {
        return bb;
    }

    // Calculate SMA (middle band)
    size_t start = bar_history_.size() - config_.bb_period;
    double sum = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        sum += bar_history_[i].close;
    }
    bb.middle = sum / config_.bb_period;

    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        double diff = bar_history_[i].close - bb.middle;
        variance += diff * diff;
    }
    double std_dev = std::sqrt(variance / config_.bb_period);

    // Calculate bands
    bb.upper = bb.middle + (config_.bb_std_dev * std_dev);
    bb.lower = bb.middle - (config_.bb_std_dev * std_dev);
    bb.bandwidth = bb.upper - bb.lower;

    // Calculate position within bands (0=lower, 1=upper)
    double current_price = bar_history_.back().close;
    if (bb.bandwidth > 1e-8) {
        bb.position_pct = (current_price - bb.lower) / bb.bandwidth;
        bb.position_pct = std::clamp(bb.position_pct, 0.0, 1.0);
    }

    return bb;
}

// Apply BB amplification to base probability
double OnlineEnsembleStrategy::apply_bb_amplification(double base_probability, const BollingerBands& bb) const {
    double amplified_prob = base_probability;

    // Only amplify if BB bands are valid
    if (bb.bandwidth < 1e-8) {
        return amplified_prob;
    }

    // LONG signals: amplify when near lower band (position < threshold)
    if (base_probability > 0.5) {
        if (bb.position_pct <= config_.bb_proximity_threshold) {
            // Near lower band - amplify LONG signal
            double proximity_factor = 1.0 - (bb.position_pct / config_.bb_proximity_threshold);
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob += amplification;

            // Extra boost for extreme oversold (position < 10%)
            if (bb.position_pct < 0.10) {
                amplified_prob += 0.05;
            }
        }
    }
    // SHORT signals: amplify when near upper band (position > 1 - threshold)
    else if (base_probability < 0.5) {
        if (bb.position_pct >= (1.0 - config_.bb_proximity_threshold)) {
            // Near upper band - amplify SHORT signal
            double proximity_factor = (bb.position_pct - (1.0 - config_.bb_proximity_threshold)) / config_.bb_proximity_threshold;
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob -= amplification;

            // Extra boost for extreme overbought (position > 90%)
            if (bb.position_pct > 0.90) {
                amplified_prob -= 0.05;
            }
        }
    }

    // Clamp to valid probability range
    amplified_prob = std::clamp(amplified_prob, 0.05, 0.95);

    return amplified_prob;
}

// NOTE: These methods are commented out as they're not needed for rotation trading
// and reference config fields that don't exist
/*
double OnlineEnsembleStrategy::calculate_atr(int period) const {
    if (bar_history_.size() < static_cast<size_t>(period + 1)) {
        return 0.0;
    }

    // Calculate True Range for each bar and average
    double sum_tr = 0.0;
    for (size_t i = bar_history_.size() - period; i < bar_history_.size(); ++i) {
        const auto& curr = bar_history_[i];
        const auto& prev = bar_history_[i - 1];

        // True Range = max(high-low, |high-prev_close|, |low-prev_close|)
        double hl = curr.high - curr.low;
        double hc = std::abs(curr.high - prev.close);
        double lc = std::abs(curr.low - prev.close);

        double tr = std::max({hl, hc, lc});
        sum_tr += tr;
    }

    return sum_tr / period;
}

bool OnlineEnsembleStrategy::has_sufficient_volatility() const {
    if (bar_history_.empty()) {
        return false;
    }

    // Calculate ATR
    double atr = calculate_atr(config_.atr_period);

    // Get current price
    double current_price = bar_history_.back().close;

    // Calculate ATR as percentage of price
    double atr_ratio = (current_price > 0) ? (atr / current_price) : 0.0;

    // Check if ATR ratio meets minimum threshold
    return atr_ratio >= config_.min_atr_ratio;
}
*/

} // namespace sentio

```

## 📄 **FILE 30 of 30**: src/strategy/signal_output.cpp

**File Information**:
- **Path**: `src/strategy/signal_output.cpp`
- **Size**: 361 lines
- **Modified**: 2025-10-16 06:52:13
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "strategy/signal_output.h"
#include "common/utils.h"

#include <sstream>
#include <iostream>

#ifdef NLOHMANN_JSON_AVAILABLE
#include <nlohmann/json.hpp>
using nlohmann::json;
#endif

namespace sentio {

std::string SignalOutput::to_json() const {
#ifdef NLOHMANN_JSON_AVAILABLE
    nlohmann::json j;
    j["version"] = "2.0";  // Version field for migration
    if (bar_id != 0) j["bar_id"] = bar_id;  // Numeric
    j["timestamp_ms"] = timestamp_ms;  // Numeric
    j["bar_index"] = bar_index;  // Numeric
    j["symbol"] = symbol;
    j["probability"] = probability;  // Numeric - CRITICAL FIX
    j["confidence"] = confidence;    // Numeric

    // Add signal_type
    if (signal_type == SignalType::LONG) {
        j["signal_type"] = "LONG";
    } else if (signal_type == SignalType::SHORT) {
        j["signal_type"] = "SHORT";
    } else if (signal_type == SignalType::NEUTRAL) {
        j["signal_type"] = "NEUTRAL";
    }
    
    j["strategy_name"] = strategy_name;
    j["strategy_version"] = strategy_version;
    // Flatten commonly used metadata keys for portability
    auto it = metadata.find("market_data_path");
    if (it != metadata.end()) {
        j["market_data_path"] = it->second;
    }
    
    // Include calibration method for debugging
    auto cal_it = metadata.find("calibration_method");
    if (cal_it != metadata.end()) {
        j["calibration_method"] = cal_it->second;
    }
    
    // Include raw and calibrated probabilities for debugging
    auto raw_it = metadata.find("raw_probability");
    if (raw_it != metadata.end()) {
        j["raw_probability"] = raw_it->second;
    }
    
    auto cal_prob_it = metadata.find("calibrated_probability");
    if (cal_prob_it != metadata.end()) {
        j["calibrated_probability"] = cal_prob_it->second;
    }
    
    // Include optimization metadata
    auto opt_config_it = metadata.find("optimized_config");
    if (opt_config_it != metadata.end()) {
        j["optimized_config"] = opt_config_it->second;
    }
    
    auto eff_conf_it = metadata.find("effective_confidence_threshold");
    if (eff_conf_it != metadata.end()) {
        j["effective_confidence_threshold"] = eff_conf_it->second;
    }
    
    auto bear_thresh_it = metadata.find("bear_threshold");
    if (bear_thresh_it != metadata.end()) {
        j["bear_threshold"] = bear_thresh_it->second;
    }
    
    auto bull_thresh_it = metadata.find("bull_threshold");
    if (bull_thresh_it != metadata.end()) {
        j["bull_threshold"] = bull_thresh_it->second;
    }
    
    // Include minimum_hold_bars for PSM hold period control
    auto hold_bars_it = metadata.find("minimum_hold_bars");
    if (hold_bars_it != metadata.end()) {
        j["minimum_hold_bars"] = hold_bars_it->second;
    }
    
    return j.dump();
#else
    // Fallback to string-based JSON (legacy format v1.0)
    std::map<std::string, std::string> m;
    m["version"] = "1.0";
    if (bar_id != 0) m["bar_id"] = std::to_string(bar_id);
    m["timestamp_ms"] = std::to_string(timestamp_ms);
    m["bar_index"] = std::to_string(bar_index);
    m["symbol"] = symbol;
    m["probability"] = std::to_string(probability);
    m["confidence"] = std::to_string(confidence);

    // Add signal_type
    if (signal_type == SignalType::LONG) {
        m["signal_type"] = "LONG";
    } else if (signal_type == SignalType::SHORT) {
        m["signal_type"] = "SHORT";
    } else if (signal_type == SignalType::NEUTRAL) {
        m["signal_type"] = "NEUTRAL";
    }
    
    m["strategy_name"] = strategy_name;
    m["strategy_version"] = strategy_version;
    // Flatten commonly used metadata keys for portability
    auto it = metadata.find("market_data_path");
    if (it != metadata.end()) {
        m["market_data_path"] = it->second;
    }
    
    // Include calibration method for debugging
    auto cal_it = metadata.find("calibration_method");
    if (cal_it != metadata.end()) {
        m["calibration_method"] = cal_it->second;
    }
    
    // Include raw and calibrated probabilities for debugging
    auto raw_it = metadata.find("raw_probability");
    if (raw_it != metadata.end()) {
        m["raw_probability"] = raw_it->second;
    }
    
    auto cal_prob_it = metadata.find("calibrated_probability");
    if (cal_prob_it != metadata.end()) {
        m["calibrated_probability"] = cal_prob_it->second;
    }
    
    // Include optimization metadata
    auto opt_config_it = metadata.find("optimized_config");
    if (opt_config_it != metadata.end()) {
        m["optimized_config"] = opt_config_it->second;
    }
    
    auto eff_conf_it = metadata.find("effective_confidence_threshold");
    if (eff_conf_it != metadata.end()) {
        m["effective_confidence_threshold"] = eff_conf_it->second;
    }
    
    auto bear_thresh_it = metadata.find("bear_threshold");
    if (bear_thresh_it != metadata.end()) {
        m["bear_threshold"] = bear_thresh_it->second;
    }
    
    auto bull_thresh_it = metadata.find("bull_threshold");
    if (bull_thresh_it != metadata.end()) {
        m["bull_threshold"] = bull_thresh_it->second;
    }
    
    // Include minimum_hold_bars for PSM hold period control
    auto hold_bars_it = metadata.find("minimum_hold_bars");
    if (hold_bars_it != metadata.end()) {
        m["minimum_hold_bars"] = hold_bars_it->second;
    }
    
    return utils::to_json(m);
#endif
}

std::string SignalOutput::to_csv() const {
    std::ostringstream os;
    os << timestamp_ms << ','
       << bar_index << ','
       << symbol << ','
       << probability << ',';
    
    // Add signal_type
    if (signal_type == SignalType::LONG) {
        os << "LONG,";
    } else if (signal_type == SignalType::SHORT) {
        os << "SHORT,";
    } else {
        os << "NEUTRAL,";
    }
    
    os << strategy_name << ','
       << strategy_version;
    return os.str();
}

SignalOutput SignalOutput::from_json(const std::string& json_str) {
    SignalOutput s;
#ifdef NLOHMANN_JSON_AVAILABLE
    try {
        auto j = nlohmann::json::parse(json_str);
        
        // Handle both numeric (v2.0) and string (v1.0) formats
        if (j.contains("bar_id")) {
            if (j["bar_id"].is_number()) {
                s.bar_id = j["bar_id"].get<uint64_t>();
            } else if (j["bar_id"].is_string()) {
                s.bar_id = static_cast<uint64_t>(std::stoull(j["bar_id"].get<std::string>()));
            }
        }
        
        if (j.contains("timestamp_ms")) {
            if (j["timestamp_ms"].is_number()) {
                s.timestamp_ms = j["timestamp_ms"].get<int64_t>();
            } else if (j["timestamp_ms"].is_string()) {
                s.timestamp_ms = std::stoll(j["timestamp_ms"].get<std::string>());
            }
        }
        
        if (j.contains("bar_index")) {
            if (j["bar_index"].is_number()) {
                s.bar_index = j["bar_index"].get<int>();
            } else if (j["bar_index"].is_string()) {
                s.bar_index = std::stoi(j["bar_index"].get<std::string>());
            }
        }
        
        if (j.contains("symbol")) s.symbol = j["symbol"].get<std::string>();
        
        // Parse signal_type
        if (j.contains("signal_type")) {
            std::string type_str = j["signal_type"].get<std::string>();
            std::cerr << "DEBUG: Parsing signal_type='" << type_str << "'" << std::endl;
            if (type_str == "LONG") {
                s.signal_type = SignalType::LONG;
                std::cerr << "DEBUG: Set to LONG" << std::endl;
            } else if (type_str == "SHORT") {
                s.signal_type = SignalType::SHORT;
                std::cerr << "DEBUG: Set to SHORT" << std::endl;
            } else {
                s.signal_type = SignalType::NEUTRAL;
                std::cerr << "DEBUG: Set to NEUTRAL (default)" << std::endl;
            }
        } else {
            std::cerr << "DEBUG: signal_type field NOT FOUND in JSON" << std::endl;
        }
        
        if (j.contains("probability")) {
            if (j["probability"].is_number()) {
                s.probability = j["probability"].get<double>();
            } else if (j["probability"].is_string()) {
                std::string prob_str = j["probability"].get<std::string>();
                if (!prob_str.empty()) {
                    try {
                        s.probability = std::stod(prob_str);
                    } catch (const std::exception& e) {
                        std::cerr << "ERROR: Failed to parse probability '" << prob_str << "': " << e.what() << "\n";
                        std::cerr << "JSON: " << json_str << "\n";
                        throw;
                    }
                }
            }
        }

        if (j.contains("confidence")) {
            if (j["confidence"].is_number()) {
                s.confidence = j["confidence"].get<double>();
            } else if (j["confidence"].is_string()) {
                std::string conf_str = j["confidence"].get<std::string>();
                if (!conf_str.empty()) {
                    try {
                        s.confidence = std::stod(conf_str);
                    } catch (const std::exception& e) {
                        // Confidence is optional, don't throw
                        s.confidence = 0.0;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        // Fallback to string-based parsing
        std::cerr << "WARNING: nlohmann::json parsing failed, falling back to string parsing: " << e.what() << "\n";
        auto m = utils::from_json(json_str);
        if (m.count("bar_id")) s.bar_id = static_cast<uint64_t>(std::stoull(m["bar_id"]));
        if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
        if (m.count("bar_index")) s.bar_index = std::stoi(m["bar_index"]);
        if (m.count("symbol")) s.symbol = m["symbol"];
        if (m.count("signal_type")) {
            std::string type_str = m["signal_type"];
            if (type_str == "LONG") {
                s.signal_type = SignalType::LONG;
            } else if (type_str == "SHORT") {
                s.signal_type = SignalType::SHORT;
            } else {
                s.signal_type = SignalType::NEUTRAL;
            }
        }
        if (m.count("probability") && !m["probability"].empty()) {
            try {
                s.probability = std::stod(m["probability"]);
            } catch (const std::exception& e2) {
                std::cerr << "ERROR: Failed to parse probability from string map '" << m["probability"] << "': " << e2.what() << "\n";
                std::cerr << "Original JSON: " << json_str << "\n";
                throw;
            }
        }
        if (m.count("confidence") && !m["confidence"].empty()) {
            try {
                s.confidence = std::stod(m["confidence"]);
            } catch (const std::exception& e2) {
                // Confidence is optional
                s.confidence = 0.0;
            }
        }
    }
#else
    auto m = utils::from_json(json_str);
    if (m.count("bar_id")) s.bar_id = static_cast<uint64_t>(std::stoull(m["bar_id"]));
    if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
    if (m.count("bar_index")) s.bar_index = std::stoi(m["bar_index"]);
    if (m.count("symbol")) s.symbol = m["symbol"];
    if (m.count("signal_type")) {
        std::string type_str = m["signal_type"];
        if (type_str == "LONG") {
            s.signal_type = SignalType::LONG;
        } else if (type_str == "SHORT") {
            s.signal_type = SignalType::SHORT;
        } else {
            s.signal_type = SignalType::NEUTRAL;
        }
    }
    if (m.count("probability") && !m["probability"].empty()) {
        s.probability = std::stod(m["probability"]);
    }
    if (m.count("confidence") && !m["confidence"].empty()) {
        try {
            s.confidence = std::stod(m["confidence"]);
        } catch (...) {
            // Confidence is optional
            s.confidence = 0.0;
        }
    }
#endif
    
    // Parse additional metadata (strategy_name, strategy_version, etc.)
    // Note: signal_type is already parsed above in the main parsing section
#ifdef NLOHMANN_JSON_AVAILABLE
    try {
        auto j = nlohmann::json::parse(json_str);
        if (j.contains("strategy_name")) s.strategy_name = j["strategy_name"].get<std::string>();
        if (j.contains("strategy_version")) s.strategy_version = j["strategy_version"].get<std::string>();
        if (j.contains("market_data_path")) s.metadata["market_data_path"] = j["market_data_path"].get<std::string>();
        if (j.contains("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = j["minimum_hold_bars"].get<std::string>();
    } catch (...) {
        // Fallback to string map
        auto m = utils::from_json(json_str);
        if (m.count("strategy_name")) s.strategy_name = m["strategy_name"];
        if (m.count("strategy_version")) s.strategy_version = m["strategy_version"];
        if (m.count("market_data_path")) s.metadata["market_data_path"] = m["market_data_path"];
        if (m.count("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = m["minimum_hold_bars"];
    }
#else
    auto m = utils::from_json(json_str);
    if (m.count("strategy_name")) s.strategy_name = m["strategy_name"];
    if (m.count("strategy_version")) s.strategy_version = m["strategy_version"];
    if (m.count("market_data_path")) s.metadata["market_data_path"] = m["market_data_path"];
    if (m.count("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = m["minimum_hold_bars"];
#endif
    return s;
}

} // namespace sentio



```

