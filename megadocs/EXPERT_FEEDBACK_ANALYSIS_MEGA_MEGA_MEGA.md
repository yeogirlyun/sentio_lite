# EXPERT_FEEDBACK_ANALYSIS_MEGA_MEGA - Complete Analysis

**Generated**: 2025-10-16 09:41:54
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: /Volumes/ExternalSSD/Dev/C++/online_trader/megadocs/EXPERT_FEEDBACK_ANALYSIS_MEGA_MEGA.md
**Total Files**: 40

---

## 📋 **TABLE OF CONTENTS**

1. [data/tmp/midday_selected_params.json](#file-1)
2. [include/backend/adaptive_portfolio_manager.h](#file-2)
3. [include/backend/adaptive_trading_mechanism.h](#file-3)
4. [include/backend/position_state_machine.h](#file-4)
5. [include/cli/command_interface.h](#file-5)
6. [include/cli/ensemble_workflow_command.h](#file-6)
7. [include/cli/live_trade_command.hpp](#file-7)
8. [include/common/bar_validator.h](#file-8)
9. [include/common/eod_state.h](#file-9)
10. [include/common/exceptions.h](#file-10)
11. [include/common/nyse_calendar.h](#file-11)
12. [include/common/time_utils.h](#file-12)
13. [include/common/types.h](#file-13)
14. [include/common/utils.h](#file-14)
15. [include/features/unified_feature_engine.h](#file-15)
16. [include/learning/online_predictor.h](#file-16)
17. [include/live/alpaca_client.hpp](#file-17)
18. [include/live/alpaca_client_adapter.h](#file-18)
19. [include/live/alpaca_rest_bar_feed.h](#file-19)
20. [include/live/bar_feed_interface.h](#file-20)
21. [include/live/broker_client_interface.h](#file-21)
22. [include/live/mock_bar_feed_replay.h](#file-22)
23. [include/live/mock_broker.h](#file-23)
24. [include/live/mock_config.h](#file-24)
25. [include/live/polygon_client.hpp](#file-25)
26. [include/live/polygon_client_adapter.h](#file-26)
27. [include/live/position_book.h](#file-27)
28. [include/live/state_persistence.h](#file-28)
29. [include/strategy/market_regime_detector.h](#file-29)
30. [include/strategy/online_ensemble_strategy.h](#file-30)
31. [include/strategy/regime_parameter_manager.h](#file-31)
32. [include/strategy/signal_output.h](#file-32)
33. [include/strategy/strategy_component.h](#file-33)
34. [logs/live_trading/eod_state.txt](#file-34)
35. [scripts/comprehensive_warmup.sh](#file-35)
36. [src/cli/analyze_trades_command.cpp](#file-36)
37. [src/cli/execute_trades_command.cpp](#file-37)
38. [src/cli/live_trade_command.cpp](#file-38)
39. [tools/adaptive_optuna.py](#file-39)
40. [tools/warmup_live_trading.sh](#file-40)

---

## 📄 **FILE 1 of 40**: data/tmp/midday_selected_params.json

**File Information**:
- **Path**: `data/tmp/midday_selected_params.json`
- **Size**: 23 lines
- **Modified**: 2025-10-16 04:16:12
- **Type**: json
- **Permissions**: -rw-r--r--

```text
{
  "last_updated": "2025-10-09T22:06:32Z",
  "optimization_source": "2phase_optuna_premarket",
  "optimization_date": "2025-10-09",
  "data_used": "SPY_warmup_latest.csv",
  "n_trials_phase1": 5,
  "n_trials_phase2": 5,
  "best_mrb": -0.0396,
  "parameters": {
    "buy_threshold": 0.59,
    "sell_threshold": 0.43999999999999995,
    "ewrls_lambda": 0.997,
    "bb_amplification_factor": 0.15,
    "h1_weight": 0.5,
    "h5_weight": 0.35000000000000003,
    "h10_weight": 0.2,
    "bb_period": 15,
    "bb_std_dev": 3.0,
    "bb_proximity": 0.15000000000000002,
    "regularization": 0.01
  },
  "note": "Optimized for live trading session on 2025-10-09"
}
```

## 📄 **FILE 2 of 40**: include/backend/adaptive_portfolio_manager.h

**File Information**:
- **Path**: `include/backend/adaptive_portfolio_manager.h`
- **Size**: 269 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

// =============================================================================
// Module: backend/adaptive_portfolio_manager.h
// Purpose: Comprehensive adaptive portfolio management system that eliminates
//          phantom sell orders and provides intelligent trading capabilities.
//
// Core Components:
// - Position Validator: Prevents phantom sell orders
// - Conflict Resolution Engine: Handles position conflicts automatically
// - Cash Balance Protector: Maintains positive cash balances
// - Profit Maximization Engine: Optimizes instrument selection
// - Risk Manager: Dynamic position sizing and risk controls
// =============================================================================

#include "common/types.h"
#include "strategy/signal_output.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sentio {

// Forward declarations
struct Position;
class BackendComponent;

// We need to use BackendComponent::TradeOrder but can't include the full header due to circular dependency
// So we'll define our own TradeOrder that's compatible
struct TradeOrder {
    TradeAction action = TradeAction::HOLD;
    std::string symbol;
    double quantity = 0.0;
    double price = 0.0;
    double trade_value = 0.0;
    double fees = 0.0;
    std::string execution_reason;
    double confidence = 0.0;
    
    TradeOrder() = default;
    TradeOrder(TradeAction act, const std::string& sym, double qty, double prc)
        : action(act), symbol(sym), quantity(qty), price(prc) {
        trade_value = quantity * price;
    }
};

// TradeAction is already defined in common/types.h

// ===================================================================
// POSITION VALIDATOR - Prevents phantom sell orders
// ===================================================================

class PositionValidator {
public:
    struct ValidationResult {
        bool is_valid = false;
        std::string error_message;
        double validated_quantity = 0.0;
    };

    /// Validates a sell order to prevent phantom orders
    /// This is the CRITICAL function that fixes the phantom sell order bug
    ValidationResult validate_sell_order(const std::string& symbol, 
                                       double requested_quantity,
                                       const std::map<std::string, Position>& positions) const;
    
    /// Validates a buy order for cash availability
    bool validate_buy_order(const std::string& symbol, 
                          double quantity, 
                          double price, 
                          double available_cash,
                          double fees = 0.0) const;
};

// ===================================================================
// CONFLICT RESOLUTION ENGINE - Handles position conflicts
// ===================================================================

class ConflictResolutionEngine {
public:
    struct ConflictAnalysis {
        bool has_conflicts = false;
        std::vector<std::string> conflicting_symbols;
        std::vector<TradeOrder> liquidation_orders;
        std::string resolution_strategy;
    };

    /// Analyzes potential conflicts with proposed symbol
    ConflictAnalysis analyze_conflicts(const std::string& proposed_symbol,
                                     const std::map<std::string, Position>& current_positions,
                                     double current_price) const;
    
    /// Automatically resolves conflicts by generating liquidation orders
    std::vector<TradeOrder> resolve_conflicts_automatically(const ConflictAnalysis& analysis) const;

private:
    bool would_conflict(const std::string& proposed, const std::string& existing) const;
};

// ===================================================================
// CASH BALANCE PROTECTOR - Prevents negative cash balances
// ===================================================================

class CashBalanceProtector {
private:
    double minimum_cash_reserve_;
    
public:
    explicit CashBalanceProtector(double min_reserve = 1000.0);
    
    struct CashValidationResult {
        bool is_valid = false;
        std::string error_message;
        double projected_cash = 0.0;
        double max_affordable_quantity = 0.0;
    };
    
    /// Validates transaction won't cause negative cash balance
    CashValidationResult validate_transaction(const TradeOrder& order, 
                                            double current_cash,
                                            double fee_rate = 0.001) const;
    
    /// Adjusts order size to fit cash constraints
    TradeOrder adjust_order_for_cash_constraints(const TradeOrder& original_order,
                                                double current_cash,
                                                double fee_rate = 0.001) const;
};

// ===================================================================
// PROFIT MAXIMIZATION ENGINE - Intelligent instrument selection
// ===================================================================

class ProfitMaximizationEngine {
public:
    struct InstrumentAnalysis {
        std::string symbol;
        double profit_potential = 0.0;
        double risk_score = 0.0;
        double leverage_factor = 1.0;
        double confidence_adjustment = 1.0;
        double final_score = 0.0;
    };
    
    /// Selects optimal instrument based on signal and portfolio state
    std::string select_optimal_instrument(const SignalOutput& signal, 
                                        const std::map<std::string, Position>& current_positions) const;

private:
    InstrumentAnalysis analyze_instrument(const std::string& symbol, 
                                        const SignalOutput& signal,
                                        double leverage_factor) const;
    
    std::vector<InstrumentAnalysis> filter_conflicting_instruments(
        const std::vector<InstrumentAnalysis>& candidates,
        const std::map<std::string, Position>& current_positions) const;
    
    bool would_conflict(const std::string& proposed, const std::string& existing) const;
};

// ===================================================================
// RISK MANAGER - Dynamic position sizing and risk controls
// ===================================================================

class RiskManager {
private:
    double max_position_size_;
    double max_portfolio_risk_;
    double volatility_adjustment_factor_;
    double kelly_fraction_;  // Fraction of full Kelly to use (0.25 = 25% Kelly)

public:
    RiskManager(double max_pos_size = 0.25, double max_portfolio_risk = 0.15,
                double vol_adj = 0.1, double kelly_frac = 0.25);

    struct RiskAnalysis {
        double recommended_position_size = 0.0;
        double max_safe_quantity = 0.0;
        double risk_score = 0.0;
        std::string risk_level; // "LOW", "MEDIUM", "HIGH", "EXTREME"
        std::vector<std::string> risk_warnings;
        double kelly_position_size = 0.0;  // Kelly Criterion recommended size
        double final_position_size = 0.0;  // After all constraints
    };

    /// Calculates optimal position size with Kelly Criterion and risk controls
    RiskAnalysis calculate_optimal_position_size(const std::string& symbol,
                                               const SignalOutput& signal,
                                               double available_capital,
                                               double current_price,
                                               const std::map<std::string, Position>& positions) const;

    /// Calculate Kelly Criterion position size
    /// @param win_probability Expected win probability (from signal)
    /// @param avg_win_pct Average win percentage (estimated)
    /// @param avg_loss_pct Average loss percentage (estimated)
    /// @return Optimal position size as fraction of capital (0.0-1.0)
    double calculate_kelly_size(double win_probability,
                               double avg_win_pct = 0.02,
                               double avg_loss_pct = 0.015) const;

private:
    double get_leverage_factor(const std::string& symbol) const;
    double calculate_portfolio_risk_score(const std::string& new_symbol,
                                        double new_quantity,
                                        double new_price,
                                        const std::map<std::string, Position>& positions) const;
};

// ===================================================================
// ADAPTIVE PORTFOLIO MANAGER - Main orchestrator class
// ===================================================================

class AdaptivePortfolioManager {
private:
    std::unique_ptr<PositionValidator> position_validator_;
    std::unique_ptr<ConflictResolutionEngine> conflict_resolver_;
    std::unique_ptr<CashBalanceProtector> cash_protector_;
    std::unique_ptr<ProfitMaximizationEngine> profit_optimizer_;
    std::unique_ptr<RiskManager> risk_manager_;
    
    // Portfolio state
    std::map<std::string, Position> positions_;
    double cash_balance_;
    double total_portfolio_value_;
    
    // Configuration
    struct Config {
        double buy_threshold = 0.6;
        double sell_threshold = 0.4;
        double fee_rate = 0.001;
        bool enable_auto_conflict_resolution = true;
        bool enable_risk_management = true;
        double minimum_cash_reserve = 1000.0;
    } config_;
    
public:
    explicit AdaptivePortfolioManager(double initial_cash = 100000.0);
    
    /// Main method that replaces the buggy evaluate_signal logic
    /// This method GUARANTEES no phantom sell orders
    std::vector<TradeOrder> execute_adaptive_trade(const SignalOutput& signal, const Bar& bar);
    
    /// Execute individual orders with validation
    bool execute_order(const TradeOrder& order);
    
    // Portfolio state access
    const std::map<std::string, Position>& get_positions() const { return positions_; }
    double get_cash_balance() const { return cash_balance_; }
    double get_total_portfolio_value() const;
    bool has_position(const std::string& symbol) const;
    Position get_position(const std::string& symbol) const;
    
private:
    bool validate_inputs(const SignalOutput& signal, const Bar& bar) const;
    TradeAction determine_trade_action(const SignalOutput& signal) const;
    TradeOrder create_main_order(TradeAction action, const std::string& symbol, 
                               const SignalOutput& signal, const Bar& bar) const;
    TradeOrder apply_risk_management(const TradeOrder& order, const SignalOutput& signal, const Bar& bar) const;
    TradeOrder create_hold_order(const std::string& reason) const;
    bool execute_buy_order(const TradeOrder& order);
    bool execute_sell_order(const TradeOrder& order);
    double calculate_total_portfolio_value() const;
};

} // namespace sentio

```

## 📄 **FILE 3 of 40**: include/backend/adaptive_trading_mechanism.h

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

## 📄 **FILE 4 of 40**: include/backend/position_state_machine.h

**File Information**:
- **Path**: `include/backend/position_state_machine.h`
- **Size**: 139 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include "common/types.h"
#include "strategy/signal_output.h"

namespace sentio {

struct MarketState;

class PositionStateMachine {
public:
    enum class State {
        CASH_ONLY,      
        QQQ_ONLY,       
        TQQQ_ONLY,      
        PSQ_ONLY,       
        SQQQ_ONLY,      
        QQQ_TQQQ,       
        PSQ_SQQQ,       
        INVALID         
    };

    enum class SignalType {
        STRONG_BUY,     
        WEAK_BUY,       
        WEAK_SELL,      
        STRONG_SELL,    
        NEUTRAL         
    };

    struct StateTransition {
        State current_state;
        SignalType signal_type;
        State target_state;
        std::string optimal_action;
        std::string theoretical_basis;
        double expected_return = 0.0;      
        double risk_score = 0.0;           
        double confidence = 0.0;           
        
        // NEW: Multi-bar prediction support
        int prediction_horizon = 1;           // How many bars ahead this predicts
        uint64_t position_open_bar_id = 0;    // When position was opened
        uint64_t earliest_exit_bar_id = 0;    // When position can be closed
        bool is_hold_enforced = false;        // Currently in minimum hold period
        int bars_held = 0;                    // How many bars position has been held
        int bars_remaining = 0;               // Bars until hold period ends
    };
    
    struct PositionTracking {
        uint64_t open_bar_id;
        int horizon;
        double entry_price;
        std::string symbol;
    };

    PositionStateMachine();

    StateTransition get_optimal_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions,
        double confidence_threshold = CONFIDENCE_THRESHOLD
    );

    std::pair<double, double> get_state_aware_thresholds(
        double base_buy_threshold,
        double base_sell_threshold,
        State current_state
    ) const;

    bool validate_transition(
        const StateTransition& transition,
        const PortfolioState& current_portfolio,
        double available_capital
    ) const;
    
    // NEW: Multi-bar support methods
    bool can_close_position(uint64_t current_bar_id, const std::string& symbol) const;
    void record_position_entry(const std::string& symbol, uint64_t bar_id, 
                              int horizon, double entry_price);
    void record_position_exit(const std::string& symbol);
    void clear_position_tracking();
    int get_bars_held(const std::string& symbol, uint64_t current_bar_id) const;
    int get_bars_remaining(const std::string& symbol, uint64_t current_bar_id) const;
    bool is_in_hold_period(const PortfolioState& portfolio, uint64_t current_bar_id) const;

    static std::string state_to_string(State s);
    static std::string signal_type_to_string(SignalType st);
    
    State determine_current_state(const PortfolioState& portfolio) const;

protected:
    StateTransition get_base_transition(State current, SignalType signal) const;

private:
    SignalType classify_signal(
        const SignalOutput& signal,
        double buy_threshold,
        double sell_threshold,
        double confidence_threshold = CONFIDENCE_THRESHOLD
    ) const;
    
    void initialize_transition_matrix();
    double apply_state_risk_adjustment(State state, double base_value) const;
    double calculate_kelly_position_size(
        double signal_probability,
        double expected_return,
        double risk_estimate,
        double available_capital
    ) const;
    
    void update_position_tracking(const SignalOutput& signal, 
                                 const StateTransition& transition);

    std::map<std::pair<State, SignalType>, StateTransition> transition_matrix_;
    
    // NEW: Multi-bar position tracking
    std::map<std::string, PositionTracking> position_tracking_;

    static constexpr double DEFAULT_BUY_THRESHOLD = 0.55;
    static constexpr double DEFAULT_SELL_THRESHOLD = 0.45;
    static constexpr double CONFIDENCE_THRESHOLD = 0.7;
    static constexpr double STRONG_MARGIN = 0.15;
    static constexpr double MAX_LEVERAGE_EXPOSURE = 0.8;
    static constexpr double MAX_POSITION_SIZE = 0.6;
    static constexpr double MIN_CASH_BUFFER = 0.1;
};

using PSM = PositionStateMachine;

} // namespace sentio
```

## 📄 **FILE 5 of 40**: include/cli/command_interface.h

**File Information**:
- **Path**: `include/cli/command_interface.h`
- **Size**: 102 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <string>
#include <vector>
#include <memory>

namespace sentio {
namespace cli {

/**
 * @brief Abstract base class for all CLI commands
 * 
 * This interface defines the contract for all command implementations,
 * enabling clean separation of concerns and testability.
 */
class Command {
public:
    virtual ~Command() = default;
    
    /**
     * @brief Execute the command with given arguments
     * @param args Command-line arguments (excluding program name and command)
     * @return Exit code (0 for success, non-zero for error)
     */
    virtual int execute(const std::vector<std::string>& args) = 0;
    
    /**
     * @brief Get command name for registration
     * @return Command name as used in CLI
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Get command description for help text
     * @return Brief description of what the command does
     */
    virtual std::string get_description() const = 0;
    
    /**
     * @brief Show detailed help for this command
     */
    virtual void show_help() const = 0;

protected:
    /**
     * @brief Helper to extract argument value by name
     * @param args Argument vector
     * @param name Argument name (e.g., "--dataset")
     * @param default_value Default value if not found
     * @return Argument value or default
     */
    std::string get_arg(const std::vector<std::string>& args, 
                       const std::string& name, 
                       const std::string& default_value = "") const;
    
    /**
     * @brief Check if flag is present in arguments
     * @param args Argument vector
     * @param flag Flag name (e.g., "--verbose")
     * @return True if flag is present
     */
    bool has_flag(const std::vector<std::string>& args, 
                  const std::string& flag) const;
};

/**
 * @brief Command dispatcher that manages and executes commands
 */
class CommandDispatcher {
public:
    /**
     * @brief Register a command with the dispatcher
     * @param command Unique pointer to command implementation
     */
    void register_command(std::unique_ptr<Command> command);
    
    /**
     * @brief Execute command based on arguments
     * @param argc Argument count
     * @param argv Argument vector
     * @return Exit code
     */
    int execute(int argc, char** argv);
    
    /**
     * @brief Show general help with all available commands
     */
    void show_help() const;

private:
    std::vector<std::unique_ptr<Command>> commands_;
    
    /**
     * @brief Find command by name
     * @param name Command name
     * @return Pointer to command or nullptr if not found
     */
    Command* find_command(const std::string& name) const;
};

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 6 of 40**: include/cli/ensemble_workflow_command.h

**File Information**:
- **Path**: `include/cli/ensemble_workflow_command.h`
- **Size**: 263 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "cli/command_interface.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/adaptive_portfolio_manager.h"
#include "backend/position_state_machine.h"
#include <string>
#include <vector>
#include <memory>

namespace sentio {
namespace cli {

/**
 * @brief Complete workflow command for OnlineEnsemble experiments
 *
 * Workflow:
 * 1. generate-signals: Create signal file from market data
 * 2. execute-trades: Simulate trading with portfolio manager
 * 3. analyze: Generate performance reports
 * 4. run-all: Execute complete workflow
 */
class EnsembleWorkflowCommand : public Command {
public:
    std::string get_name() const override { return "ensemble"; }
    std::string get_description() const override {
        return "OnlineEnsemble workflow: generate signals, execute trades, analyze results";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    // Sub-commands
    int generate_signals(const std::vector<std::string>& args);
    int execute_trades(const std::vector<std::string>& args);
    int analyze(const std::vector<std::string>& args);
    int run_all(const std::vector<std::string>& args);

    // Configuration structures
    struct SignalGenConfig {
        std::string data_path;
        std::string output_path;
        int warmup_bars = 100;
        int start_bar = 0;
        int end_bar = -1;  // -1 = all

        // Strategy config
        std::vector<int> horizons = {1, 5, 10};
        std::vector<double> weights = {0.3, 0.5, 0.2};
        double lambda = 0.995;
        bool verbose = false;
    };

    struct TradeExecConfig {
        std::string signal_path;
        std::string data_path;
        std::string output_path;

        double starting_capital = 100000.0;
        double buy_threshold = 0.53;
        double sell_threshold = 0.47;
        double kelly_fraction = 0.25;
        bool enable_kelly = true;
        bool verbose = false;
    };

    struct AnalysisConfig {
        std::string trades_path;
        std::string output_path;
        bool show_detailed = true;
        bool show_trades = false;
        bool export_csv = false;
        bool export_json = true;
    };

    // Parsing helpers
    SignalGenConfig parse_signal_config(const std::vector<std::string>& args);
    TradeExecConfig parse_trade_config(const std::vector<std::string>& args);
    AnalysisConfig parse_analysis_config(const std::vector<std::string>& args);
};

/**
 * @brief Signal generation command (standalone)
 */
class GenerateSignalsCommand : public Command {
public:
    std::string get_name() const override { return "generate-signals"; }
    std::string get_description() const override {
        return "Generate OnlineEnsemble signals from market data";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    struct SignalOutput {
        uint64_t bar_id;
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        double probability;
        double confidence;
        SignalType signal_type;
        int prediction_horizon;

        // Multi-horizon data
        std::map<int, double> horizon_predictions;
        double ensemble_agreement;
    };

    void save_signals_jsonl(const std::vector<SignalOutput>& signals,
                           const std::string& path);
    void save_signals_csv(const std::vector<SignalOutput>& signals,
                         const std::string& path);
};

/**
 * @brief Trade execution command (standalone)
 */
class ExecuteTradesCommand : public Command {
public:
    std::string get_name() const override { return "execute-trades"; }
    std::string get_description() const override {
        return "Execute trades from signal file and generate portfolio history";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

public:
    struct TradeRecord {
        uint64_t bar_id;
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        TradeAction action;
        double quantity;
        double price;
        double trade_value;
        double fees;
        std::string reason;

        // Portfolio state after trade
        double cash_balance;
        double portfolio_value;
        double position_quantity;
        double position_avg_price;
    };

    struct PortfolioHistory {
        std::vector<TradeRecord> trades;
        std::vector<double> equity_curve;
        std::vector<double> drawdown_curve;

        double starting_capital;
        double final_capital;
        double max_drawdown;
        int total_trades;
        int winning_trades;
    };

    void save_trades_jsonl(const PortfolioHistory& history, const std::string& path);
    void save_trades_csv(const PortfolioHistory& history, const std::string& path);
    void save_equity_curve(const PortfolioHistory& history, const std::string& path);

    // PSM helper functions
    static double get_position_value(const PortfolioState& portfolio, double current_price);
    static std::map<std::string, double> calculate_target_positions(
        PositionStateMachine::State state,
        double total_capital,
        double price);

    // Multi-instrument versions (use correct price per instrument)
    static double get_position_value_multi(
        const PortfolioState& portfolio,
        const std::map<std::string, std::vector<Bar>>& instrument_bars,
        size_t bar_index);

    // Symbol mapping for PSM (to support both QQQ and SPY)
    struct SymbolMap {
        std::string base;      // QQQ or SPY
        std::string bull_3x;   // TQQQ or SPXL
        std::string bear_1x;   // PSQ or SH
        std::string bear_nx;   // SQQQ (-3x) or SDS (-2x)
    };

    static std::map<std::string, double> calculate_target_positions_multi(
        PositionStateMachine::State state,
        double total_capital,
        const std::map<std::string, std::vector<Bar>>& instrument_bars,
        size_t bar_index,
        const SymbolMap& symbol_map);
};

/**
 * @brief Analysis and reporting command (standalone)
 */
class AnalyzeTradesCommand : public Command {
public:
    std::string get_name() const override { return "analyze-trades"; }
    std::string get_description() const override {
        return "Analyze trade history and generate performance reports";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    struct PerformanceReport {
        // Returns
        double total_return_pct;
        double annualized_return;
        double monthly_return;
        double daily_return;

        // Risk metrics
        double max_drawdown;
        double avg_drawdown;
        double volatility;
        double downside_deviation;
        double sharpe_ratio;
        double sortino_ratio;
        double calmar_ratio;

        // Trading metrics
        int total_trades;
        int winning_trades;
        int losing_trades;
        double win_rate;
        double profit_factor;
        double avg_win;
        double avg_loss;
        double avg_trade;
        double largest_win;
        double largest_loss;

        // Position metrics
        double avg_holding_period;
        double max_holding_period;
        int total_long_trades;
        int total_short_trades;

        // Kelly metrics
        double kelly_criterion;
        double avg_position_size;
        double max_position_size;

        // Time analysis
        int trading_days;
        int bars_traded;
        std::string start_date;
        std::string end_date;
    };

    PerformanceReport calculate_metrics(const std::vector<ExecuteTradesCommand::TradeRecord>& trades);
    void print_report(const PerformanceReport& report);
    void save_report_json(const PerformanceReport& report, const std::string& path);
    void generate_plots(const std::vector<double>& equity_curve, const std::string& output_dir);
};

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 7 of 40**: include/cli/live_trade_command.hpp

**File Information**:
- **Path**: `include/cli/live_trade_command.hpp`
- **Size**: 30 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: hpp
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_LIVE_TRADE_COMMAND_HPP
#define SENTIO_LIVE_TRADE_COMMAND_HPP

#include "cli/command_interface.h"
#include <vector>
#include <string>

namespace sentio {
namespace cli {

/**
 * Live Trading Command - Run OnlineTrader v1.0 with paper account
 *
 * Connects to Alpaca Paper Trading and Polygon for live trading.
 * Trades SPY/SDS/SPXL/SH during regular hours with comprehensive logging.
 */
class LiveTradeCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "live-trade"; }
    std::string get_description() const override {
        return "Run OnlineTrader v1.0 with paper account (SPY/SPXL/SH/SDS)";
    }
    void show_help() const override;
};

} // namespace cli
} // namespace sentio

#endif // SENTIO_LIVE_TRADE_COMMAND_HPP

```

## 📄 **FILE 8 of 40**: include/common/bar_validator.h

**File Information**:
- **Path**: `include/common/bar_validator.h`
- **Size**: 118 lines
- **Modified**: 2025-10-07 12:04:46
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include "common/exceptions.h"
#include <cmath>
#include <string>
#include <sstream>

namespace sentio {

/**
 * @brief Validate bar data for correctness
 *
 * Checks OHLC relationships, finite values, and reasonable ranges.
 */
class BarValidator {
public:
    /**
     * @brief Check if bar is valid
     * @param bar Bar to validate
     * @return true if valid, false otherwise
     */
    static bool is_valid(const Bar& bar) {
        // Check for finite values
        if (!std::isfinite(bar.open) || !std::isfinite(bar.high) ||
            !std::isfinite(bar.low) || !std::isfinite(bar.close)) {
            return false;
        }

        if (!std::isfinite(bar.volume) || bar.volume < 0) {
            return false;
        }

        // Check OHLC relationships
        if (!(bar.high >= bar.low)) return false;
        if (!(bar.high >= bar.open && bar.high >= bar.close)) return false;
        if (!(bar.low <= bar.open && bar.low <= bar.close)) return false;

        // Check for positive prices
        if (bar.high <= 0 || bar.low <= 0 || bar.open <= 0 || bar.close <= 0) {
            return false;
        }

        // Check for reasonable intrabar moves (>50% move is suspicious)
        if (bar.high / bar.low > 1.5) {
            return false;
        }

        return true;
    }

    /**
     * @brief Validate bar and throw if invalid
     * @param bar Bar to validate
     * @throws InvalidBarError if bar is invalid
     */
    static void validate_or_throw(const Bar& bar) {
        if (!is_valid(bar)) {
            std::stringstream ss;
            ss << "Invalid bar: "
               << "O=" << bar.open
               << " H=" << bar.high
               << " L=" << bar.low
               << " C=" << bar.close
               << " V=" << bar.volume;
            throw InvalidBarError(ss.str());
        }
    }

    /**
     * @brief Get validation error message for invalid bar
     * @param bar Bar to check
     * @return Error message (empty if valid)
     */
    static std::string get_error_message(const Bar& bar) {
        if (!std::isfinite(bar.open) || !std::isfinite(bar.high) ||
            !std::isfinite(bar.low) || !std::isfinite(bar.close)) {
            return "Non-finite OHLC values";
        }

        if (!std::isfinite(bar.volume) || bar.volume < 0) {
            return "Invalid volume";
        }

        if (!(bar.high >= bar.low)) {
            return "High < Low";
        }

        if (!(bar.high >= bar.open && bar.high >= bar.close)) {
            return "High not highest";
        }

        if (!(bar.low <= bar.open && bar.low <= bar.close)) {
            return "Low not lowest";
        }

        if (bar.high <= 0 || bar.low <= 0) {
            return "Non-positive prices";
        }

        if (bar.high / bar.low > 1.5) {
            return "Excessive intrabar move (>50%)";
        }

        return "";
    }
};

/**
 * @brief Convenience function for bar validation
 * @param bar Bar to validate
 * @return true if valid
 */
inline bool is_valid_bar(const Bar& bar) {
    return BarValidator::is_valid(bar);
}

} // namespace sentio

```

## 📄 **FILE 9 of 40**: include/common/eod_state.h

**File Information**:
- **Path**: `include/common/eod_state.h`
- **Size**: 83 lines
- **Modified**: 2025-10-16 04:24:40
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <string>
#include <optional>

namespace sentio {

/**
 * @brief EOD liquidation status
 */
enum class EodStatus {
    PENDING,      // No EOD action taken yet today
    IN_PROGRESS,  // EOD liquidation in progress
    DONE          // EOD liquidation completed
};

/**
 * @brief EOD state for a trading day
 */
struct EodState {
    EodStatus status{EodStatus::PENDING};
    std::string positions_hash;  // Hash of positions when DONE (for verification)
    int64_t last_attempt_epoch{0};  // Unix timestamp (seconds) of last liquidation attempt
};

/**
 * @brief Persistent state tracking for End-of-Day (EOD) liquidation
 *
 * Ensures idempotent EOD execution - prevents double liquidation on process restart
 * and enables detection of missed EOD events.
 *
 * State is persisted to a simple text file containing the last ET date (YYYY-MM-DD)
 * for which EOD liquidation was completed.
 */
class EodStateStore {
public:
    /**
     * @brief Construct state store with file path
     * @param state_file_path Full path to state file (e.g., "/tmp/sentio_eod_state.txt")
     */
    explicit EodStateStore(std::string state_file_path);

    /**
     * @brief Get the ET date (YYYY-MM-DD) of the last completed EOD
     * @return ET date string if available, nullopt if no EOD recorded
     */
    std::optional<std::string> last_eod_date() const;

    /**
     * @brief Mark EOD liquidation as complete for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     *
     * Atomically writes the date to the state file, overwriting previous value.
     * This ensures exactly-once semantics for EOD execution per trading day.
     */
    void mark_eod_complete(const std::string& et_date);

    /**
     * @brief Check if EOD already completed for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @return true if EOD already done for this date
     */
    bool is_eod_complete(const std::string& et_date) const;

    /**
     * @brief Load EOD state for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @return EodState (PENDING if no state exists)
     */
    EodState load(const std::string& et_date) const;

    /**
     * @brief Save EOD state for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @param state EOD state to save
     */
    void save(const std::string& et_date, const EodState& state);

private:
    std::string state_file_;
};

} // namespace sentio

```

## 📄 **FILE 10 of 40**: include/common/exceptions.h

**File Information**:
- **Path**: `include/common/exceptions.h`
- **Size**: 75 lines
- **Modified**: 2025-10-07 12:03:42
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <stdexcept>
#include <string>

namespace sentio {

// ============================================================================
// Transient Errors (retry/reconnect)
// ============================================================================

/**
 * @brief Base class for transient errors that can be retried
 */
class TransientError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Feed disconnection error (can reconnect)
 */
class FeedDisconnectError : public TransientError {
public:
    using TransientError::TransientError;
};

/**
 * @brief Broker API error (rate limit, temporary unavailable)
 */
class BrokerApiError : public TransientError {
public:
    int status_code;

    BrokerApiError(const std::string& msg, int code)
        : TransientError(msg), status_code(code) {}
};

// ============================================================================
// Fatal Errors (flatten + exit)
// ============================================================================

/**
 * @brief Base class for fatal trading errors (requires panic flatten)
 */
class FatalTradingError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Position reconciliation failed (local != broker)
 */
class PositionReconciliationError : public FatalTradingError {
public:
    using FatalTradingError::FatalTradingError;
};

/**
 * @brief Feature engine corruption or validation failure
 */
class FeatureEngineError : public FatalTradingError {
public:
    using FatalTradingError::FatalTradingError;
};

/**
 * @brief Invalid bar data that cannot be processed
 */
class InvalidBarError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace sentio

```

## 📄 **FILE 11 of 40**: include/common/nyse_calendar.h

**File Information**:
- **Path**: `include/common/nyse_calendar.h`
- **Size**: 53 lines
- **Modified**: 2025-10-07 21:41:10
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <unordered_set>
#include <string>

namespace sentio {

/**
 * @brief NYSE market holiday and half-day calendar
 *
 * Provides trading day checks for NYSE regular hours (9:30 AM - 4:00 PM ET).
 * Includes full holidays and half-days (1:00 PM close) for 2025-2027.
 *
 * Future enhancement: Load from JSON file for 10+ year coverage.
 */
class NyseCalendar {
public:
    NyseCalendar();

    /**
     * @brief Check if given ET date is a trading day
     * @param et_date_ymd ET date in YYYY-MM-DD format
     * @return true if not a full holiday (may be half-day)
     */
    bool is_trading_day(const std::string& et_date_ymd) const;

    /**
     * @brief Check if given ET date is a half-day (1:00 PM close)
     * @param et_date_ymd ET date in YYYY-MM-DD format
     * @return true if early close at 1:00 PM ET
     */
    bool is_half_day(const std::string& et_date_ymd) const;

    /**
     * @brief Get market close hour for given ET date
     * @param et_date_ymd ET date in YYYY-MM-DD format
     * @return 13 for half-days, 16 for normal days
     */
    int market_close_hour(const std::string& et_date_ymd) const;

    /**
     * @brief Get market close minute for given ET date
     * @param et_date_ymd ET date in YYYY-MM-DD format
     * @return 0 (always on the hour)
     */
    int market_close_minute(const std::string& et_date_ymd) const;

private:
    std::unordered_set<std::string> full_holidays_;
    std::unordered_set<std::string> half_days_;
};

} // namespace sentio

```

## 📄 **FILE 12 of 40**: include/common/time_utils.h

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

## 📄 **FILE 13 of 40**: include/common/types.h

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

## 📄 **FILE 14 of 40**: include/common/utils.h

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

## 📄 **FILE 15 of 40**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`
- **Size**: 266 lines
- **Modified**: 2025-10-16 09:37:11
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include "features/feature_schema.h"
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>
#include <string>

namespace sentio {
namespace features {

/**
 * @brief Configuration for Unified Feature Engine
 */
struct UnifiedFeatureEngineConfig {
    // Feature categories
    bool enable_time_features = true;
    bool enable_price_action = true;
    bool enable_moving_averages = true;
    bool enable_momentum = true;
    bool enable_volatility = true;
    bool enable_volume = true;
    bool enable_statistical = true;
    bool enable_pattern_detection = true;
    bool enable_price_lags = true;
    bool enable_return_lags = true;
    
    // Normalization
    bool normalize_features = true;
    bool use_robust_scaling = false;  // Use median/IQR instead of mean/std
    
    // Performance optimization
    bool enable_caching = true;
    bool enable_incremental_updates = true;
    int max_history_size = 1000;
    
    // Feature dimensions (matching Kochi analysis)
    int time_features = 8;
    int price_action_features = 12;
    int moving_average_features = 16;
    int momentum_features = 20;
    int volatility_features = 15;
    int volume_features = 12;
    int statistical_features = 10;
    int pattern_features = 8;
    int price_lag_features = 10;
    int return_lag_features = 15;
    
    // Total: Variable based on enabled features  
    int total_features() const {
        int total = 0;
        if (enable_time_features) total += time_features;
        if (enable_price_action) total += price_action_features;
        if (enable_moving_averages) total += moving_average_features;
        if (enable_momentum) total += momentum_features;
        if (enable_volatility) total += volatility_features;
        if (enable_volume) total += volume_features;
        if (enable_statistical) total += statistical_features;
        if (enable_pattern_detection) total += pattern_features;
        if (enable_price_lags) total += price_lag_features;
        if (enable_return_lags) total += return_lag_features;
        return total;
    }
};

/**
 * @brief Incremental calculator for O(1) moving averages
 */
class IncrementalSMA {
public:
    explicit IncrementalSMA(int period);
    double update(double value);
    double get_value() const { return sum_ / std::min(period_, static_cast<int>(values_.size())); }
    bool is_ready() const { return static_cast<int>(values_.size()) >= period_; }

private:
    int period_;
    std::deque<double> values_;
    double sum_ = 0.0;
};

/**
 * @brief Incremental calculator for O(1) EMA
 */
class IncrementalEMA {
public:
    IncrementalEMA(int period, double alpha = -1.0);
    double update(double value);
    double get_value() const { return ema_value_; }
    bool is_ready() const { return initialized_; }

private:
    double alpha_;
    double ema_value_ = 0.0;
    bool initialized_ = false;
};

/**
 * @brief Incremental calculator for O(1) RSI
 */
class IncrementalRSI {
public:
    explicit IncrementalRSI(int period);
    double update(double price);
    double get_value() const;
    bool is_ready() const { return gain_sma_.is_ready() && loss_sma_.is_ready(); }

private:
    double prev_price_ = 0.0;
    bool first_update_ = true;
    IncrementalSMA gain_sma_;
    IncrementalSMA loss_sma_;
};

/**
 * @brief Unified Feature Engine implementing Kochi's 126-feature set
 * 
 * This engine provides a comprehensive set of technical indicators optimized
 * for machine learning applications. It implements all features identified
 * in the Kochi analysis with proper normalization and O(1) incremental updates.
 * 
 * Feature Categories:
 * 1. Time Features (8): Cyclical encoding of time components
 * 2. Price Action (12): OHLC patterns, gaps, shadows
 * 3. Moving Averages (16): SMA/EMA ratios at multiple periods
 * 4. Momentum (20): RSI, MACD, Stochastic, Williams %R
 * 5. Volatility (15): ATR, Bollinger Bands, Keltner Channels
 * 6. Volume (12): VWAP, OBV, A/D Line, Volume ratios
 * 7. Statistical (10): Correlation, regression, distribution metrics
 * 8. Patterns (8): Candlestick pattern detection
 * 9. Price Lags (10): Historical price references
 * 10. Return Lags (15): Historical return references
 */
class UnifiedFeatureEngine {
public:
    using Config = UnifiedFeatureEngineConfig;
    
    explicit UnifiedFeatureEngine(const Config& config = Config{});
    ~UnifiedFeatureEngine() = default;

    /**
     * @brief Update engine with new bar data
     * @param bar New OHLCV bar
     */
    void update(const Bar& bar);

    /**
     * @brief Get current feature vector
     * @return Vector of 126 normalized features
     */
    std::vector<double> get_features() const;

    /**
     * @brief Get specific feature category
     */
    std::vector<double> get_time_features() const;
    std::vector<double> get_price_action_features() const;
    std::vector<double> get_moving_average_features() const;
    std::vector<double> get_momentum_features() const;
    std::vector<double> get_volatility_features() const;
    std::vector<double> get_volume_features() const;
    std::vector<double> get_statistical_features() const;
    std::vector<double> get_pattern_features() const;
    std::vector<double> get_price_lag_features() const;
    std::vector<double> get_return_lag_features() const;

    /**
     * @brief Get feature names for debugging/analysis
     */
    std::vector<std::string> get_feature_names() const;

    /**
     * @brief Reset engine state
     */
    void reset();

    /**
     * @brief Check if engine has enough data for all features
     */
    bool is_ready() const;

    /**
     * @brief Get number of bars processed
     */
    size_t get_bar_count() const { return bar_history_.size(); }

    /**
     * @brief Get feature schema for validation
     */
    const FeatureSchema& get_schema() const { return feature_schema_; }

private:
    Config config_;

    // Initialization methods
    void initialize_feature_schema();

    // Data storage
    std::deque<Bar> bar_history_;
    std::deque<double> returns_;
    std::deque<double> log_returns_;
    
    // Incremental calculators
    std::unordered_map<std::string, std::unique_ptr<IncrementalSMA>> sma_calculators_;
    std::unordered_map<std::string, std::unique_ptr<IncrementalEMA>> ema_calculators_;
    std::unordered_map<std::string, std::unique_ptr<IncrementalRSI>> rsi_calculators_;
    
    // Cached features
    mutable std::vector<double> cached_features_;

    // Feature schema for validation
    FeatureSchema feature_schema_;
    mutable bool cache_valid_ = false;
    
    // Normalization parameters
    struct NormalizationParams {
        double mean = 0.0;
        double std = 1.0;
        double median = 0.0;
        double iqr = 1.0;
        std::deque<double> history;
        bool initialized = false;
    };
    mutable std::unordered_map<std::string, NormalizationParams> norm_params_;
    
    // Private methods
    void initialize_calculators();
    void update_returns(const Bar& bar);
    void update_calculators(const Bar& bar);
    void invalidate_cache();
    
    // Feature calculation methods
    std::vector<double> calculate_time_features(const Bar& bar) const;
    std::vector<double> calculate_price_action_features() const;
    std::vector<double> calculate_moving_average_features() const;
    std::vector<double> calculate_momentum_features() const;
    std::vector<double> calculate_volatility_features() const;
    std::vector<double> calculate_volume_features() const;
    std::vector<double> calculate_statistical_features() const;
    std::vector<double> calculate_pattern_features() const;
    std::vector<double> calculate_price_lag_features() const;
    std::vector<double> calculate_return_lag_features() const;
    
    // Utility methods
    double normalize_feature(const std::string& name, double value) const;
    void update_normalization_params(const std::string& name, double value) const;
    double safe_divide(double numerator, double denominator, double fallback = 0.0) const;
    double calculate_atr(int period) const;
    double calculate_true_range(size_t index) const;
    
    // Pattern detection helpers
    bool is_doji(const Bar& bar) const;
    bool is_hammer(const Bar& bar) const;
    bool is_shooting_star(const Bar& bar) const;
    bool is_engulfing_bullish(size_t index) const;
    bool is_engulfing_bearish(size_t index) const;
    
    // Statistical helpers
    double calculate_correlation(const std::vector<double>& x, const std::vector<double>& y, int period) const;
    double calculate_linear_regression_slope(const std::vector<double>& values, int period) const;
};

} // namespace features
} // namespace sentio
```

## 📄 **FILE 16 of 40**: include/learning/online_predictor.h

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

## 📄 **FILE 17 of 40**: include/live/alpaca_client.hpp

**File Information**:
- **Path**: `include/live/alpaca_client.hpp`
- **Size**: 171 lines
- **Modified**: 2025-10-16 04:22:00
- **Type**: hpp
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_ALPACA_CLIENT_HPP
#define SENTIO_ALPACA_CLIENT_HPP

#include <string>
#include <map>
#include <vector>
#include <optional>

namespace sentio {

/**
 * Alpaca Paper Trading API Client
 *
 * REST API client for Alpaca Markets paper trading.
 * Supports account info, positions, and order execution.
 */
class AlpacaClient {
public:
    struct Position {
        std::string symbol;
        double quantity;           // Positive for long, negative for short
        double avg_entry_price;
        double current_price;
        double market_value;
        double unrealized_pl;
        double unrealized_pl_pct;
    };

    struct AccountInfo {
        std::string account_number;
        double buying_power;
        double cash;
        double portfolio_value;
        double equity;
        double last_equity;
        bool pattern_day_trader;
        bool trading_blocked;
        bool account_blocked;
    };

    struct Order {
        std::string symbol;
        double quantity;
        std::string side;          // "buy" or "sell"
        std::string type;          // "market", "limit", etc.
        std::string time_in_force; // "day", "gtc", "ioc", "fok"
        std::optional<double> limit_price;

        // Response fields
        std::string order_id;
        std::string status;        // "new", "filled", "canceled", etc.
        double filled_qty;
        double filled_avg_price;
    };

    /**
     * Constructor
     * @param api_key Alpaca API key (APCA-API-KEY-ID)
     * @param secret_key Alpaca secret key (APCA-API-SECRET-KEY)
     * @param paper_trading Use paper trading endpoint (default: true)
     */
    AlpacaClient(const std::string& api_key,
                 const std::string& secret_key,
                 bool paper_trading = true);

    ~AlpacaClient() = default;

    /**
     * Get account information
     * GET /v2/account
     */
    std::optional<AccountInfo> get_account();

    /**
     * Get all open positions
     * GET /v2/positions
     */
    std::vector<Position> get_positions();

    /**
     * Get position for specific symbol
     * GET /v2/positions/{symbol}
     */
    std::optional<Position> get_position(const std::string& symbol);

    /**
     * Place a market order
     * POST /v2/orders
     *
     * @param symbol Stock symbol (e.g., "QQQ", "TQQQ")
     * @param quantity Number of shares (positive for buy, negative for sell)
     * @param time_in_force "day" or "gtc" (good till canceled)
     * @return Order details if successful
     */
    std::optional<Order> place_market_order(const std::string& symbol,
                                           double quantity,
                                           const std::string& time_in_force = "gtc");

    /**
     * Close position for a symbol
     * DELETE /v2/positions/{symbol}
     */
    bool close_position(const std::string& symbol);

    /**
     * Close all positions
     * DELETE /v2/positions
     */
    bool close_all_positions();

    /**
     * Get order by ID
     * GET /v2/orders/{order_id}
     */
    std::optional<Order> get_order(const std::string& order_id);

    /**
     * Cancel order by ID
     * DELETE /v2/orders/{order_id}
     */
    bool cancel_order(const std::string& order_id);

    /**
     * Cancel all open orders
     * DELETE /v2/orders
     */
    bool cancel_all_orders();

    /**
     * Check if market is open
     * GET /v2/clock
     */
    bool is_market_open();

private:
    std::string api_key_;
    std::string secret_key_;
    std::string base_url_;

    /**
     * Make HTTP GET request
     */
    std::string http_get(const std::string& endpoint);

    /**
     * Make HTTP POST request with JSON body
     */
    std::string http_post(const std::string& endpoint, const std::string& json_body);

    /**
     * Make HTTP DELETE request
     */
    std::string http_delete(const std::string& endpoint);

    /**
     * Add authentication headers
     */
    std::map<std::string, std::string> get_headers();

    /**
     * Parse JSON response
     */
    static std::optional<AccountInfo> parse_account_json(const std::string& json);
    static std::vector<Position> parse_positions_json(const std::string& json);
    static std::optional<Position> parse_position_json(const std::string& json);
    static std::optional<Order> parse_order_json(const std::string& json);
};

} // namespace sentio

#endif // SENTIO_ALPACA_CLIENT_HPP

```

## 📄 **FILE 18 of 40**: include/live/alpaca_client_adapter.h

**File Information**:
- **Path**: `include/live/alpaca_client_adapter.h`
- **Size**: 61 lines
- **Modified**: 2025-10-09 00:56:38
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_ALPACA_CLIENT_ADAPTER_H
#define SENTIO_ALPACA_CLIENT_ADAPTER_H

#include "live/broker_client_interface.h"
#include "live/alpaca_client.hpp"
#include "live/position_book.h"
#include <memory>

namespace sentio {

/**
 * Alpaca Client Adapter
 *
 * Adapts existing AlpacaClient to IBrokerClient interface.
 * Provides minimal wrapper to enable polymorphic substitution.
 */
class AlpacaClientAdapter : public IBrokerClient {
public:
    /**
     * Constructor
     *
     * @param api_key Alpaca API key
     * @param secret_key Alpaca secret key
     * @param paper_trading Use paper trading endpoint
     */
    AlpacaClientAdapter(const std::string& api_key,
                       const std::string& secret_key,
                       bool paper_trading = true);

    ~AlpacaClientAdapter() override = default;

    // IBrokerClient interface implementation
    void set_execution_callback(ExecutionCallback cb) override;
    void set_fill_behavior(FillBehavior behavior) override;
    std::optional<AccountInfo> get_account() override;
    std::vector<BrokerPosition> get_positions() override;
    std::optional<BrokerPosition> get_position(const std::string& symbol) override;
    std::optional<Order> place_market_order(const std::string& symbol,
                                           double quantity,
                                           const std::string& time_in_force = "gtc") override;
    bool close_position(const std::string& symbol) override;
    bool close_all_positions() override;
    std::optional<Order> get_order(const std::string& order_id) override;
    bool cancel_order(const std::string& order_id) override;
    std::vector<Order> get_open_orders() override;
    bool cancel_all_orders() override;
    bool is_market_open() override;

private:
    std::unique_ptr<AlpacaClient> client_;
    ExecutionCallback execution_callback_;

    // Helper to convert AlpacaClient types to interface types
    BrokerPosition convert_position(const AlpacaClient::Position& alpaca_pos);
    AccountInfo convert_account(const AlpacaClient::AccountInfo& alpaca_acc);
    Order convert_order(const AlpacaClient::Order& alpaca_order);
};

} // namespace sentio

#endif // SENTIO_ALPACA_CLIENT_ADAPTER_H

```

## 📄 **FILE 19 of 40**: include/live/alpaca_rest_bar_feed.h

**File Information**:
- **Path**: `include/live/alpaca_rest_bar_feed.h`
- **Size**: 87 lines
- **Modified**: 2025-10-09 12:24:59
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_ALPACA_REST_BAR_FEED_H
#define SENTIO_ALPACA_REST_BAR_FEED_H

#include "live/bar_feed_interface.h"
#include "live/alpaca_client.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <map>

namespace sentio {

/**
 * Alpaca REST Bar Feed
 *
 * Polls Alpaca REST API for latest bars instead of using WebSocket.
 * Simpler and more reliable than FIFO-based WebSocket bridge.
 *
 * Usage:
 *   auto feed = std::make_unique<AlpacaRestBarFeed>(api_key, secret_key);
 *   feed->connect();
 *   feed->subscribe({"SPY"});
 *   feed->start([](const std::string& symbol, const Bar& bar) {
 *       // Process bar
 *   });
 */
class AlpacaRestBarFeed : public IBarFeed {
public:
    /**
     * Constructor
     *
     * @param api_key Alpaca API key
     * @param secret_key Alpaca secret key
     * @param paper_trading Use paper trading endpoint (default: true)
     * @param poll_interval_ms Poll interval in milliseconds (default: 60000 = 1 minute)
     */
    AlpacaRestBarFeed(const std::string& api_key,
                      const std::string& secret_key,
                      bool paper_trading = true,
                      int poll_interval_ms = 60000);

    ~AlpacaRestBarFeed() override;

    // IBarFeed interface implementation
    bool connect() override;
    bool subscribe(const std::vector<std::string>& symbols) override;
    void start(BarCallback callback) override;
    void stop() override;
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const override;
    bool is_connected() const override;
    bool is_connection_healthy() const override;
    int get_seconds_since_last_message() const override;

private:
    std::unique_ptr<AlpacaClient> client_;
    std::vector<std::string> subscribed_symbols_;
    int poll_interval_ms_;

    // Threading
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::thread poll_thread_;
    BarCallback callback_;

    // Recent bars cache (for get_recent_bars)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::vector<Bar>> recent_bars_;
    static constexpr size_t MAX_CACHED_BARS = 1000;

    // Health tracking
    std::atomic<std::chrono::steady_clock::time_point> last_message_time_;
    std::atomic<int64_t> last_bar_timestamp_ms_;

    // Polling loop
    void poll_loop();

    // Convert AlpacaClient::BarData to Bar
    Bar convert_bar(const AlpacaClient::BarData& alpaca_bar);

    // Add bar to cache
    void cache_bar(const std::string& symbol, const Bar& bar);
};

} // namespace sentio

#endif // SENTIO_ALPACA_REST_BAR_FEED_H

```

## 📄 **FILE 20 of 40**: include/live/bar_feed_interface.h

**File Information**:
- **Path**: `include/live/bar_feed_interface.h`
- **Size**: 68 lines
- **Modified**: 2025-10-08 23:38:54
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_BAR_FEED_INTERFACE_H
#define SENTIO_BAR_FEED_INTERFACE_H

#include "common/types.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace sentio {

/**
 * Bar Feed Interface
 *
 * Polymorphic interface for market data feeds.
 * Allows substitution of PolygonClient with MockBarFeedReplay
 * without modifying LiveTradeCommand logic.
 */
class IBarFeed {
public:
    virtual ~IBarFeed() = default;

    using BarCallback = std::function<void(const std::string& symbol, const Bar& bar)>;

    /**
     * Connect to data feed
     */
    virtual bool connect() = 0;

    /**
     * Subscribe to symbols
     */
    virtual bool subscribe(const std::vector<std::string>& symbols) = 0;

    /**
     * Start receiving data (runs callback for each bar)
     */
    virtual void start(BarCallback callback) = 0;

    /**
     * Stop receiving data and disconnect
     */
    virtual void stop() = 0;

    /**
     * Get recent bars for a symbol (last N bars in memory)
     */
    virtual std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const = 0;

    /**
     * Check if connected
     */
    virtual bool is_connected() const = 0;

    /**
     * Check if connection is healthy (received message recently)
     */
    virtual bool is_connection_healthy() const = 0;

    /**
     * Get seconds since last message
     */
    virtual int get_seconds_since_last_message() const = 0;
};

} // namespace sentio

#endif // SENTIO_BAR_FEED_INTERFACE_H

```

## 📄 **FILE 21 of 40**: include/live/broker_client_interface.h

**File Information**:
- **Path**: `include/live/broker_client_interface.h`
- **Size**: 143 lines
- **Modified**: 2025-10-09 00:55:40
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_BROKER_CLIENT_INTERFACE_H
#define SENTIO_BROKER_CLIENT_INTERFACE_H

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <map>

namespace sentio {

/**
 * Fill behavior for realistic order simulation
 */
enum class FillBehavior {
    IMMEDIATE_FULL,     // Unrealistic but fast (instant full fill)
    DELAYED_FULL,       // Realistic delay, full fill
    DELAYED_PARTIAL     // Most realistic with partial fills
};

// Forward declarations - actual definitions in position_book.h
struct ExecutionReport;
struct BrokerPosition;

/**
 * Account information
 */
struct AccountInfo {
    std::string account_number;
    double buying_power;
    double cash;
    double portfolio_value;
    double equity;
    double last_equity;
    bool pattern_day_trader;
    bool trading_blocked;
    bool account_blocked;
};

/**
 * Order structure
 */
struct Order {
    std::string symbol;
    double quantity;
    std::string side;          // "buy" or "sell"
    std::string type;          // "market", "limit", etc.
    std::string time_in_force; // "day", "gtc", "ioc", "fok"
    std::optional<double> limit_price;

    // Response fields
    std::string order_id;
    std::string status;        // "new", "filled", "canceled", etc.
    double filled_qty;
    double filled_avg_price;
};

/**
 * Broker Client Interface
 *
 * Polymorphic interface for broker operations.
 * Allows substitution of AlpacaClient with MockBroker without
 * modifying LiveTradeCommand logic.
 */
class IBrokerClient {
public:
    virtual ~IBrokerClient() = default;

    // Execution callback for realistic async fills
    using ExecutionCallback = std::function<void(const ExecutionReport&)>;

    /**
     * Set callback for execution reports (async fills)
     */
    virtual void set_execution_callback(ExecutionCallback cb) = 0;

    /**
     * Set fill behavior for order simulation (mock only)
     */
    virtual void set_fill_behavior(FillBehavior behavior) = 0;

    /**
     * Get account information
     */
    virtual std::optional<AccountInfo> get_account() = 0;

    /**
     * Get all open positions
     */
    virtual std::vector<BrokerPosition> get_positions() = 0;

    /**
     * Get position for specific symbol
     */
    virtual std::optional<BrokerPosition> get_position(const std::string& symbol) = 0;

    /**
     * Place a market order
     */
    virtual std::optional<Order> place_market_order(
        const std::string& symbol,
        double quantity,
        const std::string& time_in_force = "gtc") = 0;

    /**
     * Close position for a symbol
     */
    virtual bool close_position(const std::string& symbol) = 0;

    /**
     * Close all positions
     */
    virtual bool close_all_positions() = 0;

    /**
     * Get order by ID
     */
    virtual std::optional<Order> get_order(const std::string& order_id) = 0;

    /**
     * Cancel order by ID
     */
    virtual bool cancel_order(const std::string& order_id) = 0;

    /**
     * Get all open orders
     */
    virtual std::vector<Order> get_open_orders() = 0;

    /**
     * Cancel all open orders (idempotent)
     */
    virtual bool cancel_all_orders() = 0;

    /**
     * Check if market is open
     */
    virtual bool is_market_open() = 0;
};

} // namespace sentio

#endif // SENTIO_BROKER_CLIENT_INTERFACE_H

```

## 📄 **FILE 22 of 40**: include/live/mock_bar_feed_replay.h

**File Information**:
- **Path**: `include/live/mock_bar_feed_replay.h`
- **Size**: 127 lines
- **Modified**: 2025-10-08 23:56:18
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_MOCK_BAR_FEED_REPLAY_H
#define SENTIO_MOCK_BAR_FEED_REPLAY_H

#include "live/bar_feed_interface.h"
#include <deque>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace sentio {

/**
 * Mock Bar Feed with Replay Capability
 *
 * Replays historical bar data with precise time synchronization:
 * - Drift-free timing using absolute time anchors
 * - Configurable speed multiplier (1x = real-time, 39x = accelerated)
 * - Multi-symbol support
 * - Thread-safe bar delivery
 */
class MockBarFeedReplay : public IBarFeed {
public:
    /**
     * Constructor
     *
     * @param csv_file Path to CSV file with historical bars
     * @param speed_multiplier Replay speed (1.0 = real-time, 39.0 = 39x speed)
     */
    explicit MockBarFeedReplay(const std::string& csv_file, double speed_multiplier = 1.0);

    ~MockBarFeedReplay() override;

    // IBarFeed interface implementation
    bool connect() override;
    bool subscribe(const std::vector<std::string>& symbols) override;
    void start(BarCallback callback) override;
    void stop() override;
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const override;
    bool is_connected() const override;
    bool is_connection_healthy() const override;
    int get_seconds_since_last_message() const override;

    // Mock-specific methods

    /**
     * Load bars from CSV file
     * Format: timestamp,open,high,low,close,volume
     */
    bool load_csv(const std::string& csv_file);

    /**
     * Add bar programmatically (for testing)
     */
    void add_bar(const std::string& symbol, const Bar& bar);

    /**
     * Set speed multiplier (can be changed during replay)
     */
    void set_speed_multiplier(double multiplier);

    /**
     * Get current replay progress
     */
    struct ReplayProgress {
        size_t total_bars;
        size_t current_index;
        double progress_pct;
        uint64_t current_bar_timestamp_ms;
        std::string current_bar_time_str;
    };

    ReplayProgress get_progress() const;

    /**
     * Check if replay is complete
     */
    bool is_replay_complete() const;

    /**
     * Data validation
     */
    bool validate_data_integrity() const;

private:
    using Clock = std::chrono::steady_clock;

    // Bar data (symbol -> bars)
    std::map<std::string, std::vector<Bar>> bars_by_symbol_;
    std::vector<std::string> subscribed_symbols_;

    // Replay state
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    std::atomic<size_t> current_index_;
    double speed_multiplier_;

    // Time synchronization (drift-free)
    Clock::time_point replay_start_real_;
    uint64_t replay_start_market_ms_;

    // Thread management
    std::unique_ptr<std::thread> replay_thread_;
    BarCallback callback_;

    // Recent bars cache (for get_recent_bars)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::deque<Bar>> bars_history_;
    static constexpr size_t MAX_BARS_HISTORY = 1000;

    // Health monitoring
    std::atomic<Clock::time_point> last_message_time_;

    // Replay loop
    void replay_loop();

    // Helper methods
    void store_bar(const std::string& symbol, const Bar& bar);
    std::optional<Bar> get_next_bar(std::string& out_symbol);
    void wait_until_bar_time(const Bar& bar);
};

} // namespace sentio

#endif // SENTIO_MOCK_BAR_FEED_REPLAY_H

```

## 📄 **FILE 23 of 40**: include/live/mock_broker.h

**File Information**:
- **Path**: `include/live/mock_broker.h`
- **Size**: 171 lines
- **Modified**: 2025-10-09 00:55:57
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_MOCK_BROKER_H
#define SENTIO_MOCK_BROKER_H

#include "live/broker_client_interface.h"
#include "live/position_book.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <random>
#include <memory>
#include <chrono>

namespace sentio {

/**
 * Market Impact Model
 *
 * Simulates realistic slippage and price impact based on:
 * - Order size relative to average volume
 * - Temporary vs permanent impact
 * - Bid-ask spread
 */
struct MarketImpactModel {
    double temporary_impact_bps = 5.0;  // 5 bps temporary impact
    double permanent_impact_bps = 2.0;  // 2 bps permanent impact
    double bid_ask_spread_bps = 2.0;    // 2 bps spread

    /**
     * Calculate realistic fill price with market impact
     *
     * @param base_price Current market price
     * @param quantity Order quantity (positive = buy, negative = sell)
     * @param avg_volume Average daily volume
     * @return Adjusted fill price including impact
     */
    double calculate_fill_price(double base_price, double quantity, double avg_volume) const {
        double abs_qty = std::abs(quantity);
        double participation_rate = abs_qty / avg_volume;

        // Square-root impact model (standard in literature)
        double impact_bps = temporary_impact_bps * std::sqrt(participation_rate);

        // Add bid-ask spread (pay offer when buying, hit bid when selling)
        double spread_cost = bid_ask_spread_bps / 2.0;

        double total_impact_bps = impact_bps + spread_cost;

        // Apply impact (positive for buys, negative for sells)
        double impact_multiplier = 1.0 + (quantity > 0 ? 1 : -1) * total_impact_bps / 10000.0;

        return base_price * impact_multiplier;
    }
};

/**
 * Mock Broker Client
 *
 * Simulates realistic broker behavior for testing:
 * - Order fills with configurable delays
 * - Market impact and slippage
 * - Partial fills
 * - Portfolio tracking
 * - Commission simulation
 */
class MockBroker : public IBrokerClient {
public:
    /**
     * Constructor
     *
     * @param initial_cash Starting capital
     * @param commission_per_share Commission rate (default: $0)
     */
    explicit MockBroker(double initial_cash = 100000.0, double commission_per_share = 0.0);

    ~MockBroker() override = default;

    // IBrokerClient interface implementation
    void set_execution_callback(ExecutionCallback cb) override;
    void set_fill_behavior(FillBehavior behavior) override;
    std::optional<AccountInfo> get_account() override;
    std::vector<BrokerPosition> get_positions() override;
    std::optional<BrokerPosition> get_position(const std::string& symbol) override;
    std::optional<Order> place_market_order(const std::string& symbol,
                                           double quantity,
                                           const std::string& time_in_force = "gtc") override;
    bool close_position(const std::string& symbol) override;
    bool close_all_positions() override;
    std::optional<Order> get_order(const std::string& order_id) override;
    bool cancel_order(const std::string& order_id) override;
    std::vector<Order> get_open_orders() override;
    bool cancel_all_orders() override;
    bool is_market_open() override;

    // Mock-specific methods

    /**
     * Update market prices for symbols (needed for position valuation)
     */
    void update_market_price(const std::string& symbol, double price);

    /**
     * Set average volume for symbol (for market impact calculation)
     */
    void set_avg_volume(const std::string& symbol, double avg_volume);

    /**
     * Process pending orders (called by mock session)
     */
    void process_pending_orders();

    /**
     * Get total portfolio value
     */
    double get_portfolio_value() const;

    /**
     * Get performance metrics
     */
    struct PerformanceMetrics {
        double total_commission_paid = 0.0;
        double total_slippage = 0.0;
        int total_orders = 0;
        int filled_orders = 0;
        int partial_fills = 0;
    };

    PerformanceMetrics get_performance_metrics() const;

private:
    // Account state
    double cash_;
    double initial_cash_;
    std::string account_number_;

    // Positions: symbol -> quantity
    std::map<std::string, double> positions_;
    std::map<std::string, double> avg_entry_prices_;

    // Market data
    std::map<std::string, double> market_prices_;
    std::map<std::string, double> avg_volumes_;

    // Orders
    std::map<std::string, Order> orders_;
    std::vector<std::string> pending_orders_;
    int next_order_id_;

    // Configuration
    double commission_per_share_;
    FillBehavior fill_behavior_;
    MarketImpactModel impact_model_;
    ExecutionCallback execution_callback_;

    // Performance tracking
    PerformanceMetrics metrics_;

    // Random number generation for realistic fills
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;

    // Helper methods
    std::string generate_order_id();
    void execute_order(Order& order);
    void update_position(const std::string& symbol, double quantity, double price);
    double calculate_position_value(const std::string& symbol) const;
    double calculate_unrealized_pnl(const std::string& symbol) const;
};

} // namespace sentio

#endif // SENTIO_MOCK_BROKER_H

```

## 📄 **FILE 24 of 40**: include/live/mock_config.h

**File Information**:
- **Path**: `include/live/mock_config.h`
- **Size**: 102 lines
- **Modified**: 2025-10-08 23:59:11
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_MOCK_CONFIG_H
#define SENTIO_MOCK_CONFIG_H

#include "live/broker_client_interface.h"
#include "live/bar_feed_interface.h"
#include <string>
#include <memory>

namespace sentio {

/**
 * Mock Mode Enumeration
 *
 * Defines different mock trading scenarios:
 * - LIVE: Real production trading (no mocking)
 * - REPLAY_HISTORICAL: Exact replay of historical session
 * - STRESS_TEST: Add market stress scenarios (high volatility, gaps)
 * - PARAMETER_SWEEP: Rapid parameter optimization
 * - REGRESSION_TEST: Verify bug fixes and features
 */
enum class MockMode {
    LIVE,                   // Real trading (Alpaca + Polygon)
    REPLAY_HISTORICAL,      // Replay historical data
    STRESS_TEST,           // Add market stress
    PARAMETER_SWEEP,       // Fast parameter testing
    REGRESSION_TEST        // Verify bug fixes
};

/**
 * Mock Configuration
 *
 * Configuration for mock trading infrastructure
 */
struct MockConfig {
    MockMode mode = MockMode::LIVE;

    // Data source
    std::string csv_data_path;
    double speed_multiplier = 1.0;  // 1x = real-time, 39x = accelerated

    // Broker simulation
    double initial_capital = 100000.0;
    double commission_per_share = 0.0;
    FillBehavior fill_behavior = FillBehavior::IMMEDIATE_FULL;

    // Market simulation
    bool enable_market_impact = true;
    double market_impact_bps = 5.0;
    double bid_ask_spread_bps = 2.0;

    // Stress testing (STRESS_TEST mode only)
    bool enable_random_gaps = false;
    bool enable_high_volatility = false;
    double volatility_multiplier = 1.0;

    // Session control
    std::string crash_simulation_time;  // ET time to simulate crash (empty = no crash)
    bool enable_checkpoints = true;
    std::string checkpoint_file;

    // Output
    std::string session_name = "mock_session";
    std::string output_dir = "data/mock_sessions";
    bool save_state_on_exit = true;
};

/**
 * Trading Infrastructure Factory
 *
 * Creates broker and feed clients based on configuration.
 * Enables easy switching between live and mock modes.
 */
class TradingInfrastructureFactory {
public:
    /**
     * Create broker client based on config
     */
    static std::unique_ptr<IBrokerClient> create_broker(const MockConfig& config,
                                                        const std::string& alpaca_key = "",
                                                        const std::string& alpaca_secret = "");

    /**
     * Create bar feed based on config
     */
    static std::unique_ptr<IBarFeed> create_bar_feed(const MockConfig& config,
                                                     const std::string& polygon_url = "",
                                                     const std::string& polygon_key = "");

    /**
     * Parse mock mode from string
     */
    static MockMode parse_mode(const std::string& mode_str);

    /**
     * Convert mock mode to string
     */
    static std::string mode_to_string(MockMode mode);
};

} // namespace sentio

#endif // SENTIO_MOCK_CONFIG_H

```

## 📄 **FILE 25 of 40**: include/live/polygon_client.hpp

**File Information**:
- **Path**: `include/live/polygon_client.hpp`
- **Size**: 98 lines
- **Modified**: 2025-10-16 06:06:51
- **Type**: hpp
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_POLYGON_CLIENT_HPP
#define SENTIO_POLYGON_CLIENT_HPP

#include "common/types.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <deque>
#include <mutex>
#include <atomic>
#include <chrono>

namespace sentio {

/**
 * Polygon.io WebSocket Client for Real-Time Market Data
 *
 * Connects to Polygon proxy server and receives 1-minute aggregated bars
 * for SPY, SDS, SPXL, and SH in real-time.
 */
class PolygonClient {
public:
    using BarCallback = std::function<void(const std::string& symbol, const Bar& bar)>;

    /**
     * Constructor
     * @param proxy_url WebSocket URL for Polygon proxy (e.g., "ws://proxy.example.com:8080")
     * @param auth_key Authentication key for proxy
     */
    PolygonClient(const std::string& proxy_url, const std::string& auth_key);
    ~PolygonClient();

    /**
     * Connect to Polygon proxy and authenticate
     */
    bool connect();

    /**
     * Subscribe to symbols for 1-minute aggregates
     */
    bool subscribe(const std::vector<std::string>& symbols);

    /**
     * Start receiving data (runs in separate thread)
     * @param callback Function called when new bar arrives
     */
    void start(BarCallback callback);

    /**
     * Stop receiving data and disconnect
     */
    void stop();

    /**
     * Get recent bars for a symbol (last N bars in memory)
     */
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const;

    /**
     * Check if connected
     */
    bool is_connected() const;

    /**
     * Store a bar in history (public for WebSocket callback access)
     */
    void store_bar(const std::string& symbol, const Bar& bar);

    /**
     * Health check methods
     */
    void update_last_message_time();
    bool is_connection_healthy() const;
    int get_seconds_since_last_message() const;

private:
    std::string proxy_url_;
    std::string auth_key_;
    bool connected_;
    bool running_;

    // Thread-safe storage of recent bars (per symbol)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::deque<Bar>> bars_history_;
    static constexpr size_t MAX_BARS_HISTORY = 1000;

    // Health monitoring
    std::atomic<std::chrono::steady_clock::time_point> last_message_time_;
    static constexpr int HEALTH_CHECK_TIMEOUT_SECONDS = 300;

    // WebSocket implementation
    void receive_loop(BarCallback callback);
};

} // namespace sentio

#endif // SENTIO_POLYGON_CLIENT_HPP

```

## 📄 **FILE 26 of 40**: include/live/polygon_client_adapter.h

**File Information**:
- **Path**: `include/live/polygon_client_adapter.h`
- **Size**: 46 lines
- **Modified**: 2025-10-09 00:56:47
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_POLYGON_CLIENT_ADAPTER_H
#define SENTIO_POLYGON_CLIENT_ADAPTER_H

#include "live/bar_feed_interface.h"
#include "live/polygon_client.hpp"
#include "live/position_book.h"
#include "common/types.h"
#include <memory>

namespace sentio {

/**
 * Polygon Client Adapter
 *
 * Adapts existing PolygonClient to IBarFeed interface.
 * Provides minimal wrapper to enable polymorphic substitution.
 */
class PolygonClientAdapter : public IBarFeed {
public:
    /**
     * Constructor
     *
     * @param proxy_url WebSocket URL for Polygon proxy
     * @param auth_key Authentication key
     */
    PolygonClientAdapter(const std::string& proxy_url, const std::string& auth_key);

    ~PolygonClientAdapter() override;

    // IBarFeed interface implementation
    bool connect() override;
    bool subscribe(const std::vector<std::string>& symbols) override;
    void start(BarCallback callback) override;
    void stop() override;
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const override;
    bool is_connected() const override;
    bool is_connection_healthy() const override;
    int get_seconds_since_last_message() const override;

private:
    std::unique_ptr<PolygonClient> client_;
};

} // namespace sentio

#endif // SENTIO_POLYGON_CLIENT_ADAPTER_H

```

## 📄 **FILE 27 of 40**: include/live/position_book.h

**File Information**:
- **Path**: `include/live/position_book.h`
- **Size**: 129 lines
- **Modified**: 2025-10-16 04:21:10
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace sentio {

struct BrokerPosition {
    std::string symbol;
    int64_t qty{0};
    double avg_entry_price{0.0};
    double unrealized_pnl{0.0};
    double current_price{0.0};

    bool is_flat() const { return qty == 0; }
};

struct ExecutionReport {
    std::string order_id;
    std::string client_order_id;
    std::string symbol;
    std::string side;  // "buy" or "sell"
    int64_t filled_qty{0};
    double avg_fill_price{0.0};
    std::string status;  // "filled", "partial_fill", "pending", etc.
    uint64_t timestamp{0};
};

struct ReconcileResult {
    double realized_pnl{0.0};
    int64_t filled_qty{0};
    bool flat{false};
    std::string status;
};

/**
 * @brief Position book that tracks positions and reconciles with broker
 *
 * This class maintains local position state and provides reconciliation
 * against broker truth to detect position drift.
 */
class PositionBook {
public:
    PositionBook() = default;

    /**
     * @brief Update position from execution report
     * @param exec Execution report from broker
     */
    void on_execution(const ExecutionReport& exec);

    /**
     * @brief Get current position for symbol
     * @param symbol Symbol to query
     * @return BrokerPosition (returns flat position if symbol not found)
     */
    BrokerPosition get_position(const std::string& symbol) const;

    /**
     * @brief Reconcile local positions against broker truth
     * @param broker_positions Positions from broker API
     * @throws PositionReconciliationError if drift detected
     */
    void reconcile_with_broker(const std::vector<BrokerPosition>& broker_positions);

    /**
     * @brief Get all non-flat positions
     * @return Map of symbol -> position
     */
    std::map<std::string, BrokerPosition> get_all_positions() const;

    /**
     * @brief Get total realized P&L since timestamp
     * @param since_ts Unix timestamp in microseconds
     * @return Realized P&L in dollars
     */
    double get_realized_pnl_since(uint64_t since_ts) const;

    /**
     * @brief Get total realized P&L today
     * @return Realized P&L in dollars
     */
    double get_total_realized_pnl() const { return total_realized_pnl_; }

    /**
     * @brief Reset daily P&L (call at market open)
     */
    void reset_daily_pnl() { total_realized_pnl_ = 0.0; }

    /**
     * @brief Update current market prices for unrealized P&L calculation
     * @param symbol Symbol
     * @param price Current market price
     */
    void update_market_price(const std::string& symbol, double price);

    /**
     * @brief Set position directly (for startup reconciliation)
     * @param symbol Symbol
     * @param qty Quantity
     * @param avg_price Average entry price
     */
    void set_position(const std::string& symbol, int64_t qty, double avg_price);

    /**
     * @brief Check if all positions are flat
     * @return true if no open positions
     */
    bool is_flat() const;

    /**
     * @brief Get hash of current positions (for EOD verification)
     * @return String hash of all positions
     */
    std::string positions_hash() const;

private:
    std::map<std::string, BrokerPosition> positions_;
    std::vector<ExecutionReport> execution_history_;
    double total_realized_pnl_{0.0};

    void update_position_on_fill(const ExecutionReport& exec);
    double calculate_realized_pnl(const BrokerPosition& old_pos, const ExecutionReport& exec);
};

} // namespace sentio

```

## 📄 **FILE 28 of 40**: include/live/state_persistence.h

**File Information**:
- **Path**: `include/live/state_persistence.h`
- **Size**: 103 lines
- **Modified**: 2025-10-09 23:30:03
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_STATE_PERSISTENCE_H
#define SENTIO_STATE_PERSISTENCE_H

#include <string>
#include <optional>
#include <mutex>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include "backend/position_state_machine.h"

namespace sentio {

/**
 * StatePersistence - Atomic state persistence for exact position recovery
 *
 * Provides crash-safe state persistence with:
 * - Atomic writes with backup rotation
 * - SHA256 checksum validation
 * - Multi-level recovery (primary → backup → timestamped)
 * - Exact bars_held tracking across restarts
 *
 * Usage:
 *   auto persistence = std::make_unique<StatePersistence>(log_dir + "/state");
 *
 *   // Save after every N bars and after state transitions
 *   persistence->save_state(current_state);
 *
 *   // Load on startup
 *   if (auto state = persistence->load_state()) {
 *       // Restore exact state
 *   }
 */
class StatePersistence {
public:
    struct PositionDetail {
        std::string symbol;
        double quantity;
        double avg_entry_price;
        uint64_t entry_timestamp;
    };

    struct TradingState {
        // Core PSM state
        PositionStateMachine::State psm_state;
        int bars_held;
        double entry_equity;
        uint64_t last_bar_timestamp;
        std::string last_bar_time_str;

        // Position details (for validation against broker)
        std::vector<PositionDetail> positions;

        // Metadata
        std::string session_id;
        uint64_t save_timestamp;
        int save_count;
        std::string checksum;

        // Serialization
        nlohmann::json to_json() const;
        static TradingState from_json(const nlohmann::json& j);

        // Integrity
        std::string calculate_checksum() const;
        bool validate_checksum() const;
    };

    explicit StatePersistence(const std::string& state_dir);

    // Save state atomically with backup
    bool save_state(const TradingState& state);

    // Load state with validation and fallback
    std::optional<TradingState> load_state();

    // Emergency recovery from corrupted state
    std::optional<TradingState> recover_from_backup();

    // Clean old backup files (keep last N)
    void cleanup_old_backups(int keep_count = 5);

private:
    std::string state_dir_;
    std::string primary_file_;
    std::string backup_file_;
    std::string temp_file_;
    std::string lock_file_;
    mutable std::mutex mutex_;
    mutable int lock_fd_;

    bool write_atomic(const std::string& filepath, const nlohmann::json& data);
    std::optional<TradingState> load_from_file(const std::string& filepath);
    std::string generate_backup_filename() const;

    // File locking for cross-process safety
    bool acquire_file_lock(int timeout_ms = 1000);
    void release_file_lock();
};

} // namespace sentio

#endif // SENTIO_STATE_PERSISTENCE_H

```

## 📄 **FILE 29 of 40**: include/strategy/market_regime_detector.h

**File Information**:
- **Path**: `include/strategy/market_regime_detector.h`
- **Size**: 76 lines
- **Modified**: 2025-10-08 09:53:32
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "common/types.h"
#include <deque>
#include <vector>
#include <optional>
#include <string>

namespace sentio {

enum class MarketRegime {
    TRENDING_UP,
    TRENDING_DOWN,
    CHOPPY,
    HIGH_VOLATILITY,
    LOW_VOLATILITY
};

struct RegimeFeatures {
    double vol = 0.0;   // std of log-returns
    double slope = 0.0; // slope of log-price
    double r2 = 0.0;    // R² of the regression
    double chop = 50.0; // CHOP index
};

struct MarketRegimeDetectorParams {
    int vol_window = 96;     // for std(log-returns)
    int slope_window = 120;  // for slope & R²
    int chop_window = 48;    // for CHOP
    int calibr_window = 8 * 390; // ~8 trading days of 1-min bars
    double trend_slope_min = 1.2e-4; // slope threshold (log-price / bar)
    double trend_r2_min = 0.60;      // require some linearity
    double hysteresis_margin = 0.15; // score margin to switch regimes
    int cooldown_bars = 60;          // bars before allowing another switch
};

class MarketRegimeDetector {
public:
    using Params = MarketRegimeDetectorParams;

    MarketRegimeDetector();
    explicit MarketRegimeDetector(const Params& p);

    MarketRegime detect(const std::vector<Bar>& bars);

    // Legacy API compatibility
    MarketRegime detect_regime(const std::vector<Bar>& recent_bars) {
        return detect(recent_bars);
    }

    // For testing/telemetry
    RegimeFeatures last_features() const { return last_feat_; }
    std::pair<double,double> vol_thresholds() const { return {vol_lo_, vol_hi_}; }
    MarketRegime last_regime() const { return last_regime_.value_or(MarketRegime::CHOPPY); }

    // Get regime name as string
    static std::string regime_to_string(MarketRegime regime);

private:
    Params p_;
    std::deque<double> vol_cal_; // rolling volatility samples for adaptive thresholds
    double vol_lo_ = 0.0, vol_hi_ = 0.0; // adaptive thresholds (p30/p70)
    std::optional<MarketRegime> last_regime_;
    int cooldown_ = 0;
    RegimeFeatures last_feat_{};

    // feature helpers
    static double std_log_returns(const std::vector<Bar>& v, int win);
    static void slope_r2_log_price(const std::vector<Bar>& v, int win, double& slope, double& r2);
    static double chop_index(const std::vector<Bar>& v, int win);

    // thresholds
    void update_vol_thresholds(double vol_sample);
    static double percentile(std::vector<double>& tmp, double q);
};

} // namespace sentio

```

## 📄 **FILE 30 of 40**: include/strategy/online_ensemble_strategy.h

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

## 📄 **FILE 31 of 40**: include/strategy/regime_parameter_manager.h

**File Information**:
- **Path**: `include/strategy/regime_parameter_manager.h`
- **Size**: 103 lines
- **Modified**: 2025-10-08 07:42:43
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/market_regime_detector.h"
#include <map>
#include <string>

namespace sentio {

// Forward declaration to avoid circular dependency
class OnlineEnsembleStrategy;

// Parameter set for a specific market regime
struct RegimeParams {
    // Primary strategy parameters
    double buy_threshold;
    double sell_threshold;
    double ewrls_lambda;
    double bb_amplification_factor;

    // Secondary parameters
    double h1_weight;
    double h5_weight;
    double h10_weight;
    double bb_period;
    double bb_std_dev;
    double bb_proximity;
    double regularization;

    RegimeParams()
        : buy_threshold(0.53),
          sell_threshold(0.48),
          ewrls_lambda(0.992),
          bb_amplification_factor(0.05),
          h1_weight(0.20),
          h5_weight(0.50),
          h10_weight(0.30),
          bb_period(20),
          bb_std_dev(2.0),
          bb_proximity(0.30),
          regularization(0.01) {}

    RegimeParams(double buy, double sell, double lambda, double bb_amp,
                 double h1, double h5, double h10,
                 double bb_per, double bb_std, double bb_prox, double reg)
        : buy_threshold(buy),
          sell_threshold(sell),
          ewrls_lambda(lambda),
          bb_amplification_factor(bb_amp),
          h1_weight(h1),
          h5_weight(h5),
          h10_weight(h10),
          bb_period(bb_per),
          bb_std_dev(bb_std),
          bb_proximity(bb_prox),
          regularization(reg) {}

    // Validate parameters
    bool is_valid() const {
        if (buy_threshold <= sell_threshold) return false;
        if (buy_threshold < 0.5 || buy_threshold > 0.7) return false;
        if (sell_threshold < 0.3 || sell_threshold > 0.5) return false;
        if (ewrls_lambda < 0.98 || ewrls_lambda > 1.0) return false;
        if (bb_amplification_factor < 0.0 || bb_amplification_factor > 0.3) return false;

        double weight_sum = h1_weight + h5_weight + h10_weight;
        if (std::abs(weight_sum - 1.0) > 0.01) return false;

        return true;
    }
};

// Manage regime-specific parameters
class RegimeParameterManager {
public:
    RegimeParameterManager();

    // Get parameters for a specific regime
    RegimeParams get_params_for_regime(MarketRegime regime) const;

    // Set parameters for a specific regime
    void set_params_for_regime(MarketRegime regime, const RegimeParams& params);

    // Load default parameter sets (optimized for each regime)
    void load_default_params();

    // Load from config file
    bool load_from_file(const std::string& config_path);

    // Save to config file
    bool save_to_file(const std::string& config_path) const;

private:
    std::map<MarketRegime, RegimeParams> regime_params_;

    // Default parameter sets for each regime (from Optuna optimization)
    void init_trending_up_params();
    void init_trending_down_params();
    void init_choppy_params();
    void init_high_volatility_params();
    void init_low_volatility_params();
};

} // namespace sentio

```

## 📄 **FILE 32 of 40**: include/strategy/signal_output.h

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

## 📄 **FILE 33 of 40**: include/strategy/strategy_component.h

**File Information**:
- **Path**: `include/strategy/strategy_component.h`
- **Size**: 91 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

// =============================================================================
// Module: strategy/strategy_component.h
// Purpose: Base strategy abstraction and a concrete example (SigorStrategy).
//
// Core idea:
// - A strategy processes a stream of Bars, maintains internal indicators, and
//   emits SignalOutput records once warm-up is complete.
// - The base class provides the ingest/export orchestration; derived classes
//   implement indicator updates and signal generation.
// =============================================================================

#include <vector>
#include <memory>
#include <string>
#include <map>
#include "common/types.h"
#include "signal_output.h"

namespace sentio {

class StrategyComponent {
public:
    struct StrategyConfig {
        std::string name = "default";
        std::string version = "1.0";
        double buy_threshold = 0.6;
        double sell_threshold = 0.4;
        int warmup_bars = 250;
        std::map<std::string, double> params;
        
        // NEW: Multi-bar prediction metadata
        std::map<std::string, std::string> metadata;
    };

    explicit StrategyComponent(const StrategyConfig& config);
    virtual ~StrategyComponent() = default;

    // Process a dataset file of Bars and return generated signals.
    virtual std::vector<SignalOutput> process_dataset(
        const std::string& dataset_path,
        const std::string& strategy_name,
        const std::map<std::string, std::string>& strategy_params
    );

    // Process a specific range of bars from dataset (index-based, high-performance)
    virtual std::vector<SignalOutput> process_dataset_range(
        const std::string& dataset_path,
        const std::string& strategy_name,
        const std::map<std::string, std::string>& strategy_params,
        uint64_t start_index = 0,
        uint64_t count = 0  // 0 = process from start_index to end
    );

    // Export signals to file in jsonl or csv format.
    virtual bool export_signals(
        const std::vector<SignalOutput>& signals,
        const std::string& output_path,
        const std::string& format = "jsonl"
    );
    
    // Public interface to process a single bar (eliminates duplicates)
    SignalOutput process_bar(const Bar& bar, int bar_index) {
        update_indicators(bar);
        return generate_signal(bar, bar_index);
    }

protected:
    // Hooks for strategy authors to implement
    virtual SignalOutput generate_signal(const Bar& bar, int bar_index);
    virtual void update_indicators(const Bar& bar);
    virtual bool is_warmed_up() const;

protected:
    StrategyConfig config_;
    std::vector<Bar> historical_bars_;
    int bars_processed_ = 0;
    bool warmup_complete_ = false;

    // Example internal indicators
    std::vector<double> moving_average_;
    std::vector<double> volatility_;
    std::vector<double> momentum_;
};

// Note: SigorStrategy is defined in `strategy/sigor_strategy.h`.

} // namespace sentio



```

## 📄 **FILE 34 of 40**: logs/live_trading/eod_state.txt

**File Information**:
- **Path**: `logs/live_trading/eod_state.txt`
- **Size**: 4 lines
- **Modified**: 2025-10-09 22:57:41
- **Type**: txt
- **Permissions**: -rw-r--r--

```text
date=2025-10-08
status=DONE
positions_hash=
last_attempt_epoch=0

```

## 📄 **FILE 35 of 40**: scripts/comprehensive_warmup.sh

**File Information**:
- **Path**: `scripts/comprehensive_warmup.sh`
- **Size**: 372 lines
- **Modified**: 2025-10-09 10:59:22
- **Type**: sh
- **Permissions**: -rwxr-xr-x

```text
#!/bin/bash
#
# Comprehensive Warmup Script for Live Trading
#
# Collects warmup data for strategy initialization:
# - 20 trading blocks (7800 bars @ 390 bars/block) going backwards from launch time
# - Additional 64 bars for feature engine initialization
# - Today's missing bars if launched after 9:30 AM ET
# - Only includes Regular Trading Hours (RTH) quotes: 9:30 AM - 4:00 PM ET
#
# Output: data/equities/SPY_warmup_latest.csv
#

set -e  # Exit on error

# =============================================================================
# Configuration
# =============================================================================

WARMUP_BLOCKS=20           # Number of trading blocks (390 bars each)
BARS_PER_BLOCK=390         # 1-minute bars per block (9:30 AM - 4:00 PM)
FEATURE_WARMUP_BARS=64     # Additional bars for feature engine warmup
TOTAL_WARMUP_BARS=$((WARMUP_BLOCKS * BARS_PER_BLOCK + FEATURE_WARMUP_BARS))  # 7864 bars

OUTPUT_FILE="$PROJECT_ROOT/data/equities/SPY_warmup_latest.csv"
TEMP_DIR="$PROJECT_ROOT/data/tmp/warmup"

# Alpaca API credentials
PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
if [ -f "$PROJECT_ROOT/config.env" ]; then
    source "$PROJECT_ROOT/config.env"
fi

if [ -z "$ALPACA_PAPER_API_KEY" ] || [ -z "$ALPACA_PAPER_SECRET_KEY" ]; then
    echo "❌ ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set"
    exit 1
fi

# =============================================================================
# Helper Functions
# =============================================================================

function log_info() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] $1"
}

function log_error() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] ❌ ERROR: $1" >&2
}

# Calculate trading days needed (accounting for 390 bars/day)
function calculate_trading_days_needed() {
    local bars_needed=$1
    # Add buffer for weekends/holidays (1.5x)
    local days_with_buffer=$(echo "scale=0; ($bars_needed / $BARS_PER_BLOCK) * 1.5 + 5" | bc)
    echo $days_with_buffer
}

# Get date N trading days ago (going backwards, skipping weekends)
function get_date_n_trading_days_ago() {
    local n_days=$1
    local current_date=$(TZ='America/New_York' date '+%Y-%m-%d')

    # Simple approximation: multiply by 1.4 to account for weekends
    local calendar_days=$(echo "scale=0; $n_days * 1.4 + 3" | bc)
    local calendar_days_int=$(printf "%.0f" $calendar_days)

    # Calculate date
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS - use integer days
        TZ='America/New_York' date -v-${calendar_days_int}d '+%Y-%m-%d'
    else
        # Linux
        date -d "$calendar_days_int days ago" '+%Y-%m-%d'
    fi
}

# Check if market is currently open
function is_market_open() {
    local current_time=$(TZ='America/New_York' date '+%H%M')
    local current_dow=$(TZ='America/New_York' date '+%u')  # 1=Monday, 7=Sunday

    # Check if weekend
    if [ "$current_dow" -ge 6 ]; then
        return 1  # Closed
    fi

    # Check if within RTH (9:30 AM - 4:00 PM)
    if [ "$current_time" -ge 930 ] && [ "$current_time" -lt 1600 ]; then
        return 0  # Open
    else
        return 1  # Closed
    fi
}

# Fetch bars from Alpaca API
function fetch_bars() {
    local symbol=$1
    local start_date=$2
    local end_date=$3
    local output_file=$4

    log_info "Fetching $symbol bars from $start_date to $end_date..."

    # Alpaca API endpoint for historical bars
    local url="https://data.alpaca.markets/v2/stocks/${symbol}/bars"
    url="${url}?start=${start_date}T09:30:00-05:00"
    url="${url}&end=${end_date}T16:00:00-05:00"
    url="${url}&timeframe=1Min"
    url="${url}&limit=10000"
    url="${url}&adjustment=raw"
    url="${url}&feed=iex"  # IEX feed (free tier)

    # Fetch data
    curl -s -X GET "$url" \
        -H "APCA-API-KEY-ID: $ALPACA_PAPER_API_KEY" \
        -H "APCA-API-SECRET-KEY: $ALPACA_PAPER_SECRET_KEY" \
        > "$output_file"

    if [ $? -ne 0 ]; then
        log_error "Failed to fetch bars from Alpaca API"
        return 1
    fi

    # Check if response contains bars
    if ! grep -q '"bars"' "$output_file"; then
        log_error "No bars returned from Alpaca API"
        cat "$output_file"
        return 1
    fi

    return 0
}

# Convert JSON bars to CSV format
function json_to_csv() {
    local json_file=$1
    local csv_file=$2

    log_info "Converting JSON to CSV format..."

    # Use Python to parse JSON and convert to CSV
    python3 - "$json_file" "$csv_file" << 'PYTHON_SCRIPT'
import json
import sys
from datetime import datetime

json_file = sys.argv[1]
csv_file = sys.argv[2]

with open(json_file, 'r') as f:
    data = json.load(f)

bars = data.get('bars', [])
if not bars:
    print(f"❌ No bars found in JSON file", file=sys.stderr)
    sys.exit(1)

# Write CSV header
with open(csv_file, 'w') as f:
    f.write("timestamp,open,high,low,close,volume\n")

    for bar in bars:
        # Parse timestamp (ISO 8601 format)
        timestamp_str = bar['t']
        try:
            # Remove timezone and convert to timestamp
            dt = datetime.fromisoformat(timestamp_str.replace('Z', '+00:00'))
            timestamp_ms = int(dt.timestamp() * 1000)

            # Write bar
            f.write(f"{timestamp_ms},{bar['o']},{bar['h']},{bar['l']},{bar['c']},{bar['v']}\n")
        except Exception as e:
            print(f"⚠️  Failed to parse bar: {e}", file=sys.stderr)
            continue

print(f"✓ Converted {len(bars)} bars to CSV")
PYTHON_SCRIPT

    return $?
}

# Filter to only include RTH bars (9:30 AM - 4:00 PM ET)
function filter_rth_bars() {
    local input_csv=$1
    local output_csv=$2

    log_info "Filtering to RTH bars only (9:30 AM - 4:00 PM ET)..."

    python3 - "$input_csv" "$output_csv" << 'PYTHON_SCRIPT'
import sys
from datetime import datetime, timezone
import pytz

input_csv = sys.argv[1]
output_csv = sys.argv[2]

et_tz = pytz.timezone('America/New_York')
rth_bars = []

with open(input_csv, 'r') as f:
    header = f.readline()

    for line in f:
        parts = line.strip().split(',')
        if len(parts) < 6:
            continue

        timestamp_ms = int(parts[0])
        dt_utc = datetime.fromtimestamp(timestamp_ms / 1000, tz=timezone.utc)
        dt_et = dt_utc.astimezone(et_tz)

        # Check if RTH (9:30 AM - 4:00 PM ET)
        hour = dt_et.hour
        minute = dt_et.minute
        time_minutes = hour * 60 + minute

        # 9:30 AM = 570 minutes, 4:00 PM = 960 minutes
        if 570 <= time_minutes < 960:
            rth_bars.append(line)

# Write filtered bars
with open(output_csv, 'w') as f:
    f.write(header)
    for bar in rth_bars:
        f.write(bar)

print(f"✓ Filtered to {len(rth_bars)} RTH bars")
PYTHON_SCRIPT

    return $?
}

# =============================================================================
# Main Warmup Process
# =============================================================================

function main() {
    log_info "========================================================================"
    log_info "Comprehensive Warmup for Live Trading"
    log_info "========================================================================"
    log_info "Configuration:"
    log_info "  - Warmup blocks: $WARMUP_BLOCKS (going backwards from now)"
    log_info "  - Bars per block: $BARS_PER_BLOCK (RTH only)"
    log_info "  - Feature warmup: $FEATURE_WARMUP_BARS bars"
    log_info "  - Total warmup bars: $TOTAL_WARMUP_BARS"
    log_info ""

    # Create temp directory
    mkdir -p "$TEMP_DIR"

    # Determine date range
    local today=$(TZ='America/New_York' date '+%Y-%m-%d')
    local now_et=$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S')

    log_info "Current ET time: $now_et"
    log_info ""

    # Calculate start date (need enough calendar days to get required trading bars)
    local calendar_days_needed=$(calculate_trading_days_needed $TOTAL_WARMUP_BARS)
    local start_date=$(get_date_n_trading_days_ago $calendar_days_needed)

    log_info "Step 1: Fetching Historical Bars"
    log_info "---------------------------------------------"
    log_info "Start date: $start_date (estimated)"
    log_info "End date: $today"
    log_info ""

    # Fetch historical bars from Alpaca
    local json_file="$TEMP_DIR/historical.json"
    if ! fetch_bars "SPY" "$start_date" "$today" "$json_file"; then
        log_error "Failed to fetch historical bars"
        exit 1
    fi

    # Convert JSON to CSV
    local historical_csv="$TEMP_DIR/historical_all.csv"
    if ! json_to_csv "$json_file" "$historical_csv"; then
        log_error "Failed to convert JSON to CSV"
        exit 1
    fi

    # Filter to RTH bars only
    local rth_csv="$TEMP_DIR/historical_rth.csv"
    if ! filter_rth_bars "$historical_csv" "$rth_csv"; then
        log_error "Failed to filter RTH bars"
        exit 1
    fi

    # Count bars
    local historical_bar_count=$(tail -n +2 "$rth_csv" | wc -l | tr -d ' ')
    log_info "Historical bars collected (RTH only): $historical_bar_count"
    log_info ""

    # Check if we need today's bars
    local todays_bars_needed=0
    if is_market_open; then
        log_info "Step 2: Fetching Today's Missing Bars"
        log_info "---------------------------------------------"
        log_info "Market is currently open - fetching today's bars so far"

        # Calculate bars from 9:30 AM to now
        local current_time=$(TZ='America/New_York' date '+%H:%M')
        local current_minutes=$(TZ='America/New_York' date '+%H * 60 + %M' | bc)
        local market_open_minutes=$((9 * 60 + 30))  # 9:30 AM
        todays_bars_needed=$((current_minutes - market_open_minutes))

        log_info "Current time: $current_time ET"
        log_info "Bars from 9:30 AM to now: ~$todays_bars_needed bars"
        log_info ""
    else
        log_info "Step 2: Today's Bars"
        log_info "---------------------------------------------"
        log_info "Market is closed - no additional today's bars needed"
        log_info ""
    fi

    # Take last N bars from historical data
    log_info "Step 3: Creating Final Warmup File"
    log_info "---------------------------------------------"

    # Keep last TOTAL_WARMUP_BARS bars (20 blocks + 64 feature warmup)
    local final_csv="$TEMP_DIR/final_warmup.csv"
    head -1 "$rth_csv" > "$final_csv"  # Header
    tail -n +2 "$rth_csv" | tail -n $TOTAL_WARMUP_BARS >> "$final_csv"

    local final_bar_count=$(tail -n +2 "$final_csv" | wc -l | tr -d ' ')
    log_info "Final warmup bars: $final_bar_count"

    # Verify we have enough bars
    if [ $final_bar_count -lt $TOTAL_WARMUP_BARS ]; then
        log_error "Not enough bars! Got $final_bar_count, need $TOTAL_WARMUP_BARS"
        log_error "Try increasing the date range or check data availability"
        exit 1
    fi

    # Move to final location
    mv "$final_csv" "$OUTPUT_FILE"
    log_info "✓ Warmup file created: $OUTPUT_FILE"
    log_info ""

    # Show summary
    log_info "========================================================================"
    log_info "Warmup Summary"
    log_info "========================================================================"
    log_info "Output file: $OUTPUT_FILE"
    log_info "Total bars: $final_bar_count"
    log_info "  - Historical bars: $((final_bar_count - todays_bars_needed))"
    log_info "  - Today's bars: $todays_bars_needed"
    log_info ""
    log_info "Bar distribution:"
    log_info "  - Feature warmup: First $FEATURE_WARMUP_BARS bars"
    log_info "  - Strategy training: Next $((WARMUP_BLOCKS * BARS_PER_BLOCK)) bars ($WARMUP_BLOCKS blocks)"
    log_info ""

    # Show first and last bar timestamps
    local first_bar=$(tail -n +2 "$OUTPUT_FILE" | head -1)
    local last_bar=$(tail -1 "$OUTPUT_FILE")
    log_info "Date range:"
    log_info "  - First bar: $(echo $first_bar | cut -d',' -f1)"
    log_info "  - Last bar: $(echo $last_bar | cut -d',' -f1)"
    log_info ""

    log_info "✓ Warmup complete - ready for live trading!"
    log_info "========================================================================"

    # Cleanup temp files
    rm -rf "$TEMP_DIR"
}

# Run main
main "$@"

```

## 📄 **FILE 36 of 40**: src/cli/analyze_trades_command.cpp

**File Information**:
- **Path**: `src/cli/analyze_trades_command.cpp`
- **Size**: 446 lines
- **Modified**: 2025-10-09 15:15:21
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/ensemble_workflow_command.h"
#include "common/utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace sentio {
namespace cli {

// Per-instrument performance tracking
struct InstrumentMetrics {
    std::string symbol;
    int num_trades = 0;
    int buy_count = 0;
    int sell_count = 0;
    double total_buy_value = 0.0;
    double total_sell_value = 0.0;
    double realized_pnl = 0.0;
    double avg_allocation_pct = 0.0;  // Average % of portfolio allocated
    double win_rate = 0.0;
    int winning_trades = 0;
    int losing_trades = 0;
};

int AnalyzeTradesCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string trades_path = get_arg(args, "--trades", "");
    std::string output_path = get_arg(args, "--output", "analysis_report.json");
    int num_blocks = std::stoi(get_arg(args, "--blocks", "0"));  // Number of blocks for MRB calculation
    bool show_detailed = !has_flag(args, "--summary-only");
    bool show_trades = has_flag(args, "--show-trades");
    bool export_csv = has_flag(args, "--csv");
    bool export_json = !has_flag(args, "--no-json");
    bool json_stdout = has_flag(args, "--json");  // Output JSON metrics to stdout for Optuna

    if (trades_path.empty()) {
        std::cerr << "Error: --trades is required\n";
        show_help();
        return 1;
    }

    if (!json_stdout) {
        std::cout << "=== OnlineEnsemble Trade Analysis ===\n";
        std::cout << "Trade file: " << trades_path << "\n\n";
    }

    // Load trades from JSONL
    if (!json_stdout) {
        std::cout << "Loading trade history...\n";
    }
    std::vector<ExecuteTradesCommand::TradeRecord> trades;

    std::ifstream file(trades_path);
    if (!file) {
        std::cerr << "Error: Could not open trade file\n";
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        try {
            json j = json::parse(line);
            ExecuteTradesCommand::TradeRecord trade;

            trade.bar_id = j["bar_id"];
            trade.timestamp_ms = j["timestamp_ms"];
            trade.bar_index = j["bar_index"];
            trade.symbol = j["symbol"];

            std::string action_str = j["action"];
            trade.action = (action_str == "BUY") ? TradeAction::BUY : TradeAction::SELL;

            trade.quantity = j["quantity"];
            trade.price = j["price"];
            trade.trade_value = j["trade_value"];
            trade.fees = j["fees"];
            trade.reason = j["reason"];

            trade.cash_balance = j["cash_balance"];
            trade.portfolio_value = j["portfolio_value"];
            trade.position_quantity = j["position_quantity"];
            trade.position_avg_price = j["position_avg_price"];

            trades.push_back(trade);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to parse line: " << e.what() << "\n";
        }
    }

    if (!json_stdout) {
        std::cout << "Loaded " << trades.size() << " trades\n\n";
    }

    if (trades.empty()) {
        std::cerr << "Error: No trades loaded\n";
        return 1;
    }

    // Calculate per-instrument metrics
    if (!json_stdout) {
        std::cout << "Calculating per-instrument metrics...\n";
    }
    std::map<std::string, InstrumentMetrics> instrument_metrics;
    std::map<std::string, std::vector<std::pair<double, double>>> position_tracking;  // symbol -> [(buy_price, quantity)]

    double starting_capital = 100000.0;  // Assume standard starting capital
    double total_allocation_samples = 0;

    for (const auto& trade : trades) {
        auto& metrics = instrument_metrics[trade.symbol];
        metrics.symbol = trade.symbol;
        metrics.num_trades++;

        if (trade.action == TradeAction::BUY) {
            metrics.buy_count++;
            metrics.total_buy_value += trade.trade_value;

            // Track position for P/L calculation
            position_tracking[trade.symbol].push_back({trade.price, trade.quantity});

            // Track allocation
            double allocation_pct = (trade.trade_value / trade.portfolio_value) * 100.0;
            metrics.avg_allocation_pct += allocation_pct;
            total_allocation_samples++;

        } else {  // SELL
            metrics.sell_count++;
            metrics.total_sell_value += trade.trade_value;

            // Calculate realized P/L using FIFO
            auto& positions = position_tracking[trade.symbol];
            double remaining_qty = trade.quantity;
            double trade_pnl = 0.0;

            while (remaining_qty > 0 && !positions.empty()) {
                auto& pos = positions.front();
                double qty_to_close = std::min(remaining_qty, pos.second);

                // P/L = (sell_price - buy_price) * quantity
                trade_pnl += (trade.price - pos.first) * qty_to_close;

                pos.second -= qty_to_close;
                remaining_qty -= qty_to_close;

                if (pos.second <= 0) {
                    positions.erase(positions.begin());
                }
            }

            metrics.realized_pnl += trade_pnl;

            // Track win/loss
            if (trade_pnl > 0) {
                metrics.winning_trades++;
            } else if (trade_pnl < 0) {
                metrics.losing_trades++;
            }
        }
    }

    // Calculate averages and win rates
    for (auto& [symbol, metrics] : instrument_metrics) {
        if (metrics.buy_count > 0) {
            metrics.avg_allocation_pct /= metrics.buy_count;
        }
        int completed_trades = metrics.winning_trades + metrics.losing_trades;
        if (completed_trades > 0) {
            metrics.win_rate = (double)metrics.winning_trades / completed_trades * 100.0;
        }
    }

    // Calculate overall metrics
    if (!json_stdout) {
        std::cout << "Calculating overall performance metrics...\n";
    }
    PerformanceReport report = calculate_metrics(trades);

    // Print instrument analysis
    if (!json_stdout) {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║         PER-INSTRUMENT PERFORMANCE ANALYSIS                ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }

    // Sort instruments by realized P/L (descending)
    std::vector<std::pair<std::string, InstrumentMetrics>> sorted_instruments;
    for (const auto& [symbol, metrics] : instrument_metrics) {
        sorted_instruments.push_back({symbol, metrics});
    }
    std::sort(sorted_instruments.begin(), sorted_instruments.end(),
              [](const auto& a, const auto& b) { return a.second.realized_pnl > b.second.realized_pnl; });

    if (!json_stdout) {
        std::cout << std::fixed << std::setprecision(2);

        for (const auto& [symbol, m] : sorted_instruments) {
            std::string pnl_indicator = (m.realized_pnl > 0) ? "✅" : (m.realized_pnl < 0) ? "❌" : "  ";

            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            std::cout << symbol << " " << pnl_indicator << "\n";
            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            std::cout << "  Trades:           " << m.num_trades << " (" << m.buy_count << " BUY, " << m.sell_count << " SELL)\n";
            std::cout << "  Total Buy Value:  $" << std::setw(12) << m.total_buy_value << "\n";
            std::cout << "  Total Sell Value: $" << std::setw(12) << m.total_sell_value << "\n";
            std::cout << "  Realized P/L:     $" << std::setw(12) << m.realized_pnl
                      << "  (" << std::showpos << (m.realized_pnl / starting_capital * 100.0)
                      << std::noshowpos << "% of capital)\n";
            std::cout << "  Avg Allocation:   " << std::setw(12) << m.avg_allocation_pct << "%\n";
            std::cout << "  Win Rate:         " << std::setw(12) << m.win_rate << "%  ("
                      << m.winning_trades << "W / " << m.losing_trades << "L)\n";
            std::cout << "\n";
        }

        // Summary table
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║              INSTRUMENT SUMMARY TABLE                      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        std::cout << std::left << std::setw(8) << "Symbol"
                  << std::right << std::setw(10) << "Trades"
                  << std::setw(12) << "Alloc %"
                  << std::setw(15) << "P/L ($)"
                  << std::setw(12) << "P/L (%)"
                  << std::setw(12) << "Win Rate"
                  << "\n";
        std::cout << "────────────────────────────────────────────────────────────────\n";

        for (const auto& [symbol, m] : sorted_instruments) {
            double pnl_pct = (m.realized_pnl / starting_capital) * 100.0;
            std::cout << std::left << std::setw(8) << symbol
                      << std::right << std::setw(10) << m.num_trades
                      << std::setw(12) << m.avg_allocation_pct
                      << std::setw(15) << m.realized_pnl
                      << std::setw(12) << std::showpos << pnl_pct << std::noshowpos
                      << std::setw(12) << m.win_rate
                      << "\n";
        }
    }

    // Calculate total realized P/L from instruments
    double total_realized_pnl = 0.0;
    for (const auto& [symbol, m] : instrument_metrics) {
        total_realized_pnl += m.realized_pnl;
    }

    if (!json_stdout) {
        std::cout << "────────────────────────────────────────────────────────────────\n";
        std::cout << std::left << std::setw(8) << "TOTAL"
                  << std::right << std::setw(10) << trades.size()
                  << std::setw(12) << ""
                  << std::setw(15) << total_realized_pnl
                  << std::setw(12) << std::showpos << (total_realized_pnl / starting_capital * 100.0) << std::noshowpos
                  << std::setw(12) << ""
                  << "\n\n";
    }

    // Calculate MRB (Mean Return per Block) - for strategies with overnight carry
    double total_return_pct = (total_realized_pnl / starting_capital) * 100.0;
    double mrb = 0.0;
    if (num_blocks > 0) {
        mrb = total_return_pct / num_blocks;
    }

    // Calculate MRD (Mean Return per Day) - for daily reset strategies
    // This is the more accurate metric for strategies with EOD liquidation
    double mrd = 0.0;
    int num_trading_days = 0;
    std::vector<double> daily_returns;

    if (!trades.empty()) {
        // Group trades by trading day
        std::map<std::string, std::vector<ExecuteTradesCommand::TradeRecord>> trades_by_day;

        for (const auto& trade : trades) {
            // Extract date from timestamp (YYYY-MM-DD)
            std::time_t trade_time = static_cast<std::time_t>(trade.timestamp_ms / 1000);
            std::tm tm_utc{};
            #ifdef _WIN32
                gmtime_s(&tm_utc, &trade_time);
            #else
                gmtime_r(&trade_time, &tm_utc);
            #endif

            // Convert to ET (subtract 4 hours for EDT)
            int et_hour = tm_utc.tm_hour - 4;
            if (et_hour < 0) et_hour += 24;

            // Format as YYYY-MM-DD
            char date_str[32];
            std::snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
                         tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday);

            trades_by_day[date_str].push_back(trade);
        }

        // Calculate daily returns
        double prev_day_end_value = starting_capital;

        for (const auto& [date, day_trades] : trades_by_day) {
            if (day_trades.empty()) continue;

            // Get final portfolio value of the day
            double day_end_value = day_trades.back().portfolio_value;

            // Calculate daily return
            double daily_return_pct = ((day_end_value - prev_day_end_value) / prev_day_end_value) * 100.0;
            daily_returns.push_back(daily_return_pct);

            // Update for next day
            prev_day_end_value = day_end_value;
        }

        num_trading_days = static_cast<int>(daily_returns.size());

        // MRD = mean of daily returns
        if (!daily_returns.empty()) {
            double sum = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0);
            mrd = sum / daily_returns.size();
        }
    }

    // Print metrics
    if (!json_stdout) {
        if (num_blocks > 0) {
            std::cout << "Mean Return per Block (MRB): " << std::showpos << std::fixed << std::setprecision(4)
                      << mrb << std::noshowpos << "% (" << num_blocks << " blocks of 391 bars)\n";
        }

        if (num_trading_days > 0) {
            std::cout << "Mean Return per Day (MRD):   " << std::showpos << std::fixed << std::setprecision(4)
                      << mrd << std::noshowpos << "% (" << num_trading_days << " trading days)\n";

            // Show annualized projection
            double annualized_mrd = mrd * 252.0;  // 252 trading days per year
            std::cout << "  Annualized (252 days):     " << std::showpos << std::fixed << std::setprecision(2)
                      << annualized_mrd << std::noshowpos << "%\n";
        }

        std::cout << "\n";
    }

    // Calculate overall win rate and trades per block
    int total_winning = 0, total_losing = 0;
    for (const auto& [symbol, m] : instrument_metrics) {
        total_winning += m.winning_trades;
        total_losing += m.losing_trades;
    }
    double overall_win_rate = (total_winning + total_losing > 0)
        ? (double)total_winning / (total_winning + total_losing) * 100.0 : 0.0;
    double trades_per_block = (num_blocks > 0) ? (double)trades.size() / num_blocks : 0.0;

    // If --json flag, output metrics as JSON to stdout and exit
    if (json_stdout) {
        json result;
        result["mrb"] = mrb;
        result["mrd"] = mrd;  // New: Mean Return per Day (primary metric for daily strategies)
        result["total_return_pct"] = total_return_pct;
        result["win_rate"] = overall_win_rate;
        result["total_trades"] = trades.size();
        result["trades_per_block"] = trades_per_block;
        result["num_blocks"] = num_blocks;
        result["num_trading_days"] = num_trading_days;

        // Output compact JSON (single line) for Optuna parsing
        std::cout << result.dump() << std::endl;
        return 0;
    }

    // Print overall report
    print_report(report);

    // Save report
    if (export_json) {
        std::cout << "\nSaving report to " << output_path << "...\n";
        save_report_json(report, output_path);
    }

    std::cout << "\n✅ Analysis complete!\n";
    return 0;
}

AnalyzeTradesCommand::PerformanceReport
AnalyzeTradesCommand::calculate_metrics(const std::vector<ExecuteTradesCommand::TradeRecord>& trades) {
    PerformanceReport report;

    if (trades.empty()) {
        return report;
    }

    // Basic counts
    report.total_trades = static_cast<int>(trades.size());

    // Extract equity curve from trades
    std::vector<double> equity;
    for (const auto& trade : trades) {
        equity.push_back(trade.portfolio_value);
    }

    // Calculate returns (stub - would need full implementation)
    report.total_return_pct = 0.0;
    report.annualized_return = 0.0;

    return report;
}

void AnalyzeTradesCommand::print_report(const PerformanceReport& report) {
    // Stub - basic implementation
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ONLINE ENSEMBLE PERFORMANCE REPORT                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Total Trades: " << report.total_trades << "\n";
}

void AnalyzeTradesCommand::save_report_json(const PerformanceReport& report, const std::string& path) {
    // Stub
}

void AnalyzeTradesCommand::show_help() const {
    std::cout << "Usage: sentio_cli analyze-trades --trades <file> [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --trades <file>     Trade history file (JSONL format)\n";
    std::cout << "  --output <file>     Output report file (default: analysis_report.json)\n";
    std::cout << "  --blocks <N>        Number of blocks traded (for MRB calculation)\n";
    std::cout << "  --json              Output metrics as JSON to stdout (for Optuna)\n";
    std::cout << "  --summary-only      Show only summary metrics\n";
    std::cout << "  --show-trades       Show individual trade details\n";
    std::cout << "  --csv               Export to CSV format\n";
    std::cout << "  --no-json           Disable JSON export\n";
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 37 of 40**: src/cli/execute_trades_command.cpp

**File Information**:
- **Path**: `src/cli/execute_trades_command.cpp`
- **Size**: 839 lines
- **Modified**: 2025-10-10 08:08:22
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/ensemble_workflow_command.h"
#include "backend/adaptive_portfolio_manager.h"
#include "backend/position_state_machine.h"
#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include "strategy/signal_output.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace sentio {
namespace cli {

// Helper: Get price for specific instrument at bar index
inline double get_instrument_price(
    const std::map<std::string, std::vector<Bar>>& instrument_bars,
    const std::string& symbol,
    size_t bar_index) {

    if (instrument_bars.count(symbol) > 0 && bar_index < instrument_bars.at(symbol).size()) {
        return instrument_bars.at(symbol)[bar_index].close;
    }
    return 0.0;  // Should never happen if data is properly loaded
}

// Helper: Create symbol mapping for PSM states based on base symbol
ExecuteTradesCommand::SymbolMap create_symbol_map(const std::string& base_symbol,
                                                   const std::vector<std::string>& symbols) {
    ExecuteTradesCommand::SymbolMap mapping;
    if (base_symbol == "QQQ") {
        mapping.base = "QQQ";
        mapping.bull_3x = "TQQQ";
        mapping.bear_1x = "PSQ";
        mapping.bear_nx = "SQQQ";
    } else if (base_symbol == "SPY") {
        mapping.base = "SPY";
        mapping.bull_3x = "SPXL";
        mapping.bear_1x = "SH";

        // Check if using SPXS (-3x) or SDS (-2x)
        if (std::find(symbols.begin(), symbols.end(), "SPXS") != symbols.end()) {
            mapping.bear_nx = "SPXS";  // -3x symmetric
        } else {
            mapping.bear_nx = "SDS";   // -2x asymmetric
        }
    }
    return mapping;
}

int ExecuteTradesCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string signal_path = get_arg(args, "--signals", "");
    std::string data_path = get_arg(args, "--data", "");
    std::string output_path = get_arg(args, "--output", "trades.jsonl");
    double starting_capital = std::stod(get_arg(args, "--capital", "100000"));
    double buy_threshold = std::stod(get_arg(args, "--buy-threshold", "0.53"));
    double sell_threshold = std::stod(get_arg(args, "--sell-threshold", "0.47"));
    bool enable_kelly = !has_flag(args, "--no-kelly");
    bool verbose = has_flag(args, "--verbose") || has_flag(args, "-v");
    bool csv_output = has_flag(args, "--csv");

    // PSM Risk Management Parameters (CLI overrides, defaults from v1.5 SPY calibration)
    double profit_target = std::stod(get_arg(args, "--profit-target", "0.003"));
    double stop_loss = std::stod(get_arg(args, "--stop-loss", "-0.004"));
    int min_hold_bars = std::stoi(get_arg(args, "--min-hold-bars", "0"));  // Changed from 3 to 0 for faster exits
    int max_hold_bars = std::stoi(get_arg(args, "--max-hold-bars", "100"));

    if (signal_path.empty() || data_path.empty()) {
        std::cerr << "Error: --signals and --data are required\n";
        show_help();
        return 1;
    }

    std::cout << "=== OnlineEnsemble Trade Execution ===\n";
    std::cout << "Signals: " << signal_path << "\n";
    std::cout << "Data: " << data_path << "\n";
    std::cout << "Output: " << output_path << "\n";
    std::cout << "Starting Capital: $" << std::fixed << std::setprecision(2) << starting_capital << "\n";
    std::cout << "Kelly Sizing: " << (enable_kelly ? "Enabled" : "Disabled") << "\n";
    std::cout << "PSM Parameters: profit=" << (profit_target*100) << "%, stop=" << (stop_loss*100)
              << "%, hold=" << min_hold_bars << "-" << max_hold_bars << " bars\n\n";

    // Load signals
    std::cout << "Loading signals...\n";
    std::vector<SignalOutput> signals;
    std::ifstream sig_file(signal_path);
    if (!sig_file) {
        std::cerr << "Error: Could not open signal file\n";
        return 1;
    }

    std::string line;
    while (std::getline(sig_file, line)) {
        // Parse JSONL (simplified)
        SignalOutput sig = SignalOutput::from_json(line);
        signals.push_back(sig);
    }
    std::cout << "Loaded " << signals.size() << " signals\n";

    // Load market data for ALL instruments
    // Auto-detect base symbol (QQQ or SPY) from data file path
    std::cout << "Loading market data for all instruments...\n";

    // Extract directory from data path (use same directory for all instrument files)
    size_t last_slash = data_path.find_last_of("/\\");
    std::string instruments_dir = (last_slash != std::string::npos)
        ? data_path.substr(0, last_slash)
        : "data/equities";  // Fallback if no directory in path

    std::cout << "Using instruments directory: " << instruments_dir << "\n";

    // Detect base symbol from filename (QQQ_RTH_NH.csv or SPY_RTH_NH.csv)
    std::string filename = data_path.substr(last_slash + 1);
    std::string base_symbol;
    std::vector<std::string> symbols;

    if (filename.find("QQQ") != std::string::npos) {
        base_symbol = "QQQ";
        symbols = {"QQQ", "TQQQ", "PSQ", "SQQQ"};
        std::cout << "Detected QQQ trading (3x bull: TQQQ, -1x: PSQ, -3x: SQQQ)\n";
    } else if (filename.find("SPY") != std::string::npos) {
        base_symbol = "SPY";

        // Check if SPXS (-3x) exists, otherwise use SDS (-2x)
        std::string spxs_path = instruments_dir + "/SPXS_RTH_NH.csv";
        std::ifstream spxs_check(spxs_path);

        if (spxs_check.good()) {
            symbols = {"SPY", "SPXL", "SH", "SPXS"};
            std::cout << "Detected SPY trading (3x bull: SPXL, -1x: SH, -3x: SPXS) [SYMMETRIC LEVERAGE]\n";
        } else {
            symbols = {"SPY", "SPXL", "SH", "SDS"};
            std::cout << "Detected SPY trading (3x bull: SPXL, -1x: SH, -2x: SDS) [ASYMMETRIC LEVERAGE]\n";
        }
        spxs_check.close();
    } else {
        std::cerr << "Error: Could not detect base symbol from " << filename << "\n";
        std::cerr << "Expected filename to contain 'QQQ' or 'SPY'\n";
        return 1;
    }

    // Load all 4 instruments from data/equities directory
    std::map<std::string, std::vector<Bar>> instrument_bars;

    for (const auto& symbol : symbols) {
        std::string instrument_path = instruments_dir + "/" + symbol + "_RTH_NH.csv";
        auto bars = utils::read_csv_data(instrument_path);
        if (bars.empty()) {
            std::cerr << "Error: Could not load " << symbol << " data from " << instrument_path << "\n";
            return 1;
        }
        instrument_bars[symbol] = std::move(bars);
        std::cout << "  Loaded " << instrument_bars[symbol].size() << " bars for " << symbol << "\n";
    }

    // Use base symbol bars as reference for bar count
    auto& bars = instrument_bars[base_symbol];
    std::cout << "Total bars: " << bars.size() << "\n\n";

    if (signals.size() != bars.size()) {
        std::cerr << "Warning: Signal count (" << signals.size() << ") != bar count (" << bars.size() << ")\n";
    }

    // Create symbol mapping for PSM
    SymbolMap symbol_map = create_symbol_map(base_symbol, symbols);

    // Create Position State Machine for 4-instrument strategy
    PositionStateMachine psm;

    // Portfolio state tracking
    PortfolioState portfolio;
    portfolio.cash_balance = starting_capital;
    portfolio.total_equity = starting_capital;

    // Trade history
    PortfolioHistory history;
    history.starting_capital = starting_capital;
    history.equity_curve.push_back(starting_capital);

    // Track position entry for profit-taking and stop-loss
    struct PositionTracking {
        double entry_price = 0.0;
        double entry_equity = 0.0;
        int bars_held = 0;
        PositionStateMachine::State state = PositionStateMachine::State::CASH_ONLY;
    };
    PositionTracking current_position;
    current_position.entry_equity = starting_capital;

    // Risk management parameters - Now configurable via CLI
    // Defaults from v1.5 SPY calibration (5-year analysis)
    // Use: --profit-target, --stop-loss, --min-hold-bars, --max-hold-bars
    const double PROFIT_TARGET = profit_target;
    const double STOP_LOSS = stop_loss;
    const int MIN_HOLD_BARS = min_hold_bars;
    const int MAX_HOLD_BARS = max_hold_bars;

    std::cout << "Executing trades with Position State Machine...\n";
    std::cout << "Version 1.5: SPY-CALIBRATED thresholds + 3-bar min hold + 0.3%/-0.4% targets\n";
    std::cout << "  (Calibrated from 5-year SPY data: 1,018 blocks, Oct 2020-Oct 2025)\n";
    std::cout << "  QQQ v1.0: 2%/-1.5% targets | SPY v1.5: 0.3%/-0.4% targets (6.7× reduction)\n\n";

    for (size_t i = 0; i < std::min(signals.size(), bars.size()); ++i) {
        const auto& signal = signals[i];
        const auto& bar = bars[i];

        // Check for End-of-Day (EOD) closing time: 15:58 ET (2 minutes before market close)
        // Convert timestamp_ms to ET and extract hour/minute
        std::time_t bar_time = static_cast<std::time_t>(bar.timestamp_ms / 1000);
        std::tm tm_utc{};
        #ifdef _WIN32
            gmtime_s(&tm_utc, &bar_time);
        #else
            gmtime_r(&bar_time, &tm_utc);
        #endif

        // Convert UTC to ET (subtract 4 hours for EDT, 5 for EST)
        // For simplicity, use 4 hours (EDT) since most trading happens in summer
        int et_hour = tm_utc.tm_hour - 4;
        if (et_hour < 0) et_hour += 24;
        int et_minute = tm_utc.tm_min;

        // Check if time >= 15:58 ET
        bool is_eod_close = (et_hour == 15 && et_minute >= 58) || (et_hour >= 16);

        // Update position tracking
        current_position.bars_held++;
        double current_equity = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
        double position_pnl_pct = (current_equity - current_position.entry_equity) / current_position.entry_equity;

        // Check profit-taking condition
        bool should_take_profit = (position_pnl_pct >= PROFIT_TARGET &&
                                   current_position.state != PositionStateMachine::State::CASH_ONLY);

        // Check stop-loss condition
        bool should_stop_loss = (position_pnl_pct <= STOP_LOSS &&
                                current_position.state != PositionStateMachine::State::CASH_ONLY);

        // Check maximum hold period
        bool should_reevaluate = (current_position.bars_held >= MAX_HOLD_BARS);

        // Force exit to cash if profit target hit or stop loss triggered
        PositionStateMachine::State forced_target_state = PositionStateMachine::State::INVALID;
        std::string exit_reason = "";

        if (is_eod_close && current_position.state != PositionStateMachine::State::CASH_ONLY) {
            // EOD close takes priority over all other conditions
            forced_target_state = PositionStateMachine::State::CASH_ONLY;
            exit_reason = "EOD_CLOSE (15:58 ET)";
        } else if (should_take_profit) {
            forced_target_state = PositionStateMachine::State::CASH_ONLY;
            exit_reason = "PROFIT_TARGET (" + std::to_string(position_pnl_pct * 100) + "%)";
        } else if (should_stop_loss) {
            forced_target_state = PositionStateMachine::State::CASH_ONLY;
            exit_reason = "STOP_LOSS (" + std::to_string(position_pnl_pct * 100) + "%)";
        } else if (should_reevaluate) {
            exit_reason = "MAX_HOLD_PERIOD";
            // Don't force cash, but allow PSM to reevaluate
        }

        // USE OPTIMIZED THRESHOLDS - Simple binary model
        // This allows Optuna to directly control trading behavior
        PositionStateMachine::State target_state;

        // Block new position entries after 15:58 ET (EOD close time)
        if (is_eod_close) {
            // Force CASH_ONLY - do not enter any new positions
            target_state = PositionStateMachine::State::CASH_ONLY;
        } else if (signal.probability >= 0.65) {
            // ULTRA BULLISH (>= 0.65) - Use 3x leverage bull ETF
            target_state = PositionStateMachine::State::TQQQ_ONLY;
        } else if (signal.probability >= buy_threshold) {
            // LONG signal (>= buy_threshold) - Use base instrument (1x)
            target_state = PositionStateMachine::State::QQQ_ONLY;
        } else if (signal.probability <= 0.35) {
            // ULTRA BEARISH (<= 0.35) - Use 3x leverage bear ETF
            target_state = PositionStateMachine::State::SQQQ_ONLY;
        } else if (signal.probability <= sell_threshold) {
            // SHORT signal (<= sell_threshold) - Use inverse ETF (-1x)
            target_state = PositionStateMachine::State::PSQ_ONLY;
        } else {
            // NEUTRAL (between thresholds) - stay in cash
            target_state = PositionStateMachine::State::CASH_ONLY;
        }

        // Prepare transition structure
        PositionStateMachine::StateTransition transition;
        transition.current_state = current_position.state;
        transition.target_state = target_state;

        // Override with forced exit if needed
        if (forced_target_state != PositionStateMachine::State::INVALID) {
            transition.target_state = forced_target_state;
            transition.optimal_action = exit_reason;
        }

        // Apply minimum hold period (prevent flip-flop)
        if (current_position.bars_held < MIN_HOLD_BARS &&
            transition.current_state != PositionStateMachine::State::CASH_ONLY &&
            forced_target_state == PositionStateMachine::State::INVALID) {
            // Keep current state
            transition.target_state = transition.current_state;
        }

        // Debug: Log state transitions
        if (verbose && i % 500 == 0) {
            std::cout << "  [" << i << "] Signal: " << signal.probability
                     << " | Current: " << psm.state_to_string(transition.current_state)
                     << " | Target: " << psm.state_to_string(transition.target_state)
                     << " | PnL: " << (position_pnl_pct * 100) << "%"
                     << " | Cash: $" << std::fixed << std::setprecision(2) << portfolio.cash_balance << "\n";
        }

        // Execute state transition
        if (transition.target_state != transition.current_state) {
            if (verbose && i % 100 == 0) {
                std::cerr << "DEBUG [" << i << "]: State transition detected\n"
                          << "  Current=" << static_cast<int>(transition.current_state)
                          << " (" << psm.state_to_string(transition.current_state) << ")\n"
                          << "  Target=" << static_cast<int>(transition.target_state)
                          << " (" << psm.state_to_string(transition.target_state) << ")\n"
                          << "  Cash=$" << portfolio.cash_balance << "\n";
            }

            // Calculate positions for target state (using multi-instrument prices)
            double total_capital = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
            std::map<std::string, double> target_positions =
                calculate_target_positions_multi(transition.target_state, total_capital, instrument_bars, i, symbol_map);

            // PHASE 1: Execute all SELL orders first to free up cash
            // First, sell any positions NOT in target state
            // Create a copy of position symbols to avoid iterator invalidation
            std::vector<std::string> current_symbols;
            for (const auto& [symbol, position] : portfolio.positions) {
                current_symbols.push_back(symbol);
            }

            for (const std::string& symbol : current_symbols) {
                if (portfolio.positions.count(symbol) == 0) continue;  // Already sold

                if (target_positions.count(symbol) == 0 || target_positions[symbol] == 0) {
                    // This position should be fully liquidated
                    double sell_quantity = portfolio.positions[symbol].quantity;

                    if (sell_quantity > 0) {
                        // Use correct instrument price
                        double instrument_price = get_instrument_price(instrument_bars, symbol, i);
                        portfolio.cash_balance += sell_quantity * instrument_price;

                        // Erase position FIRST
                        portfolio.positions.erase(symbol);

                        // Now record trade with correct portfolio value
                        TradeRecord trade;
                        trade.bar_id = bar.bar_id;
                        trade.timestamp_ms = bar.timestamp_ms;
                        trade.bar_index = i;
                        trade.symbol = symbol;
                        trade.action = TradeAction::SELL;
                        trade.quantity = sell_quantity;
                        trade.price = instrument_price;
                        trade.trade_value = sell_quantity * instrument_price;
                        trade.fees = 0.0;
                        trade.cash_balance = portfolio.cash_balance;
                        trade.portfolio_value = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
                        trade.position_quantity = 0.0;
                        trade.position_avg_price = 0.0;
                        // Use forced exit reason if set (EOD_CLOSE, PROFIT_TARGET, STOP_LOSS)
                        if (!transition.optimal_action.empty()) {
                            trade.reason = transition.optimal_action;
                        } else {
                            trade.reason = "PSM: " + psm.state_to_string(transition.current_state) +
                                         " -> " + psm.state_to_string(transition.target_state) +
                                         " (p=" + std::to_string(signal.probability).substr(0, 6) + ")";
                        }

                        history.trades.push_back(trade);

                        if (verbose) {
                            std::cout << "  [" << i << "] " << symbol << " SELL "
                                     << sell_quantity << " @ $" << instrument_price
                                     << " | Portfolio: $" << trade.portfolio_value << "\n";
                        }
                    }
                }
            }

            // Then, reduce positions that are in both current and target but need downsizing
            for (const auto& [symbol, target_shares] : target_positions) {
                double current_shares = portfolio.positions.count(symbol) ?
                                       portfolio.positions[symbol].quantity : 0.0;
                double delta_shares = target_shares - current_shares;

                // Only process SELL orders in this phase
                if (delta_shares < -0.01) {  // Selling (delta is negative)
                    double quantity = std::abs(delta_shares);
                    double sell_quantity = std::min(quantity, portfolio.positions[symbol].quantity);

                    if (sell_quantity > 0) {
                        double instrument_price = get_instrument_price(instrument_bars, symbol, i);
                        portfolio.cash_balance += sell_quantity * instrument_price;
                        portfolio.positions[symbol].quantity -= sell_quantity;

                        if (portfolio.positions[symbol].quantity < 0.01) {
                            portfolio.positions.erase(symbol);
                        }

                        // Record trade
                        TradeRecord trade;
                        trade.bar_id = bar.bar_id;
                        trade.timestamp_ms = bar.timestamp_ms;
                        trade.bar_index = i;
                        trade.symbol = symbol;
                        trade.action = TradeAction::SELL;
                        trade.quantity = sell_quantity;
                        trade.price = instrument_price;
                        trade.trade_value = sell_quantity * instrument_price;
                        trade.fees = 0.0;
                        trade.cash_balance = portfolio.cash_balance;
                        trade.portfolio_value = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
                        trade.position_quantity = portfolio.positions.count(symbol) ? portfolio.positions[symbol].quantity : 0.0;
                        trade.position_avg_price = portfolio.positions.count(symbol) ? portfolio.positions[symbol].avg_price : 0.0;
                        // Use forced exit reason if set (EOD_CLOSE, PROFIT_TARGET, STOP_LOSS)
                        if (!transition.optimal_action.empty()) {
                            trade.reason = transition.optimal_action;
                        } else {
                            trade.reason = "PSM: " + psm.state_to_string(transition.current_state) +
                                         " -> " + psm.state_to_string(transition.target_state) +
                                         " (p=" + std::to_string(signal.probability).substr(0, 6) + ")";
                        }

                        history.trades.push_back(trade);

                        if (verbose) {
                            std::cout << "  [" << i << "] " << symbol << " SELL "
                                     << sell_quantity << " @ $" << instrument_price
                                     << " | Portfolio: $" << trade.portfolio_value << "\n";
                        }
                    }
                }
            }

            // PHASE 2: Execute all BUY orders with freed-up cash
            for (const auto& [symbol, target_shares] : target_positions) {
                double current_shares = portfolio.positions.count(symbol) ?
                                       portfolio.positions[symbol].quantity : 0.0;
                double delta_shares = target_shares - current_shares;

                // Only process BUY orders in this phase
                if (delta_shares > 0.01) {  // Buying (delta is positive)
                    double quantity = std::abs(delta_shares);
                    double instrument_price = get_instrument_price(instrument_bars, symbol, i);
                    double trade_value = quantity * instrument_price;

                    // Execute BUY trade
                    if (trade_value <= portfolio.cash_balance) {
                        portfolio.cash_balance -= trade_value;
                        portfolio.positions[symbol].quantity += quantity;
                        portfolio.positions[symbol].avg_price = instrument_price;
                        portfolio.positions[symbol].symbol = symbol;

                        // Record trade
                        TradeRecord trade;
                        trade.bar_id = bar.bar_id;
                        trade.timestamp_ms = bar.timestamp_ms;
                        trade.bar_index = i;
                        trade.symbol = symbol;
                        trade.action = TradeAction::BUY;
                        trade.quantity = quantity;
                        trade.price = instrument_price;
                        trade.trade_value = trade_value;
                        trade.fees = 0.0;
                        trade.cash_balance = portfolio.cash_balance;
                        trade.portfolio_value = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
                        trade.position_quantity = portfolio.positions[symbol].quantity;
                        trade.position_avg_price = portfolio.positions[symbol].avg_price;
                        // Use forced exit reason if set (EOD_CLOSE, PROFIT_TARGET, STOP_LOSS)
                        if (!transition.optimal_action.empty()) {
                            trade.reason = transition.optimal_action;
                        } else {
                            trade.reason = "PSM: " + psm.state_to_string(transition.current_state) +
                                         " -> " + psm.state_to_string(transition.target_state) +
                                         " (p=" + std::to_string(signal.probability).substr(0, 6) + ")";
                        }

                        history.trades.push_back(trade);

                        if (verbose) {
                            std::cout << "  [" << i << "] " << symbol << " BUY "
                                     << quantity << " @ $" << instrument_price
                                     << " | Portfolio: $" << trade.portfolio_value << "\n";
                        }
                    } else {
                        // Cash balance insufficient - log the blocked trade
                        if (verbose) {
                            std::cerr << "  [" << i << "] " << symbol << " BUY BLOCKED"
                                      << " | Required: $" << std::fixed << std::setprecision(2) << trade_value
                                      << " | Available: $" << portfolio.cash_balance << "\n";
                        }
                    }
                }
            }

            // Reset position tracking on state change
            current_position.entry_price = bars[i].close;  // Use QQQ price as reference
            current_position.entry_equity = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);
            current_position.bars_held = 0;
            current_position.state = transition.target_state;
        }

        // Update portfolio total equity
        portfolio.total_equity = portfolio.cash_balance + get_position_value_multi(portfolio, instrument_bars, i);

        // Record equity curve
        history.equity_curve.push_back(portfolio.total_equity);

        // Calculate drawdown
        double peak = *std::max_element(history.equity_curve.begin(), history.equity_curve.end());
        double drawdown = (peak - portfolio.total_equity) / peak;
        history.drawdown_curve.push_back(drawdown);
        history.max_drawdown = std::max(history.max_drawdown, drawdown);
    }

    history.final_capital = portfolio.total_equity;
    history.total_trades = static_cast<int>(history.trades.size());

    // Calculate win rate
    for (const auto& trade : history.trades) {
        if (trade.action == TradeAction::SELL) {
            double pnl = (trade.price - trade.position_avg_price) * trade.quantity;
            if (pnl > 0) history.winning_trades++;
        }
    }

    std::cout << "\nTrade execution complete!\n";
    std::cout << "Total trades: " << history.total_trades << "\n";
    std::cout << "Final capital: $" << std::fixed << std::setprecision(2) << history.final_capital << "\n";
    std::cout << "Total return: " << ((history.final_capital / history.starting_capital - 1.0) * 100) << "%\n";
    std::cout << "Max drawdown: " << (history.max_drawdown * 100) << "%\n\n";

    // Save trade history
    std::cout << "Saving trade history to " << output_path << "...\n";
    if (csv_output) {
        save_trades_csv(history, output_path);
    } else {
        save_trades_jsonl(history, output_path);
    }

    // Save equity curve
    std::string equity_path = output_path.substr(0, output_path.find_last_of('.')) + "_equity.csv";
    save_equity_curve(history, equity_path);

    std::cout << "✅ Trade execution complete!\n";
    return 0;
}

// Helper function: Calculate total value of all positions
double ExecuteTradesCommand::get_position_value(const PortfolioState& portfolio, double current_price) {
    // Legacy function - DO NOT USE for multi-instrument portfolios
    // Use get_position_value_multi() instead
    double total = 0.0;
    for (const auto& [symbol, position] : portfolio.positions) {
        total += position.quantity * current_price;
    }
    return total;
}

// Multi-instrument position value calculation
double ExecuteTradesCommand::get_position_value_multi(
    const PortfolioState& portfolio,
    const std::map<std::string, std::vector<Bar>>& instrument_bars,
    size_t bar_index) {

    double total = 0.0;
    for (const auto& [symbol, position] : portfolio.positions) {
        if (instrument_bars.count(symbol) > 0 && bar_index < instrument_bars.at(symbol).size()) {
            double current_price = instrument_bars.at(symbol)[bar_index].close;
            total += position.quantity * current_price;
        }
    }
    return total;
}

// Helper function: Calculate target positions for each PSM state (LEGACY - single price)
std::map<std::string, double> ExecuteTradesCommand::calculate_target_positions(
    PositionStateMachine::State state,
    double total_capital,
    double price) {

    std::map<std::string, double> positions;

    switch (state) {
        case PositionStateMachine::State::CASH_ONLY:
            // No positions - all cash
            break;

        case PositionStateMachine::State::QQQ_ONLY:
            // 100% in QQQ (moderate long)
            positions["QQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::TQQQ_ONLY:
            // 100% in TQQQ (strong long, 3x leverage)
            positions["TQQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::PSQ_ONLY:
            // 100% in PSQ (moderate short, -1x)
            positions["PSQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::SQQQ_ONLY:
            // 100% in SQQQ (strong short, -3x)
            positions["SQQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::QQQ_TQQQ:
            // Split: 50% QQQ + 50% TQQQ (blended long)
            positions["QQQ"] = (total_capital * 0.5) / price;
            positions["TQQQ"] = (total_capital * 0.5) / price;
            break;

        case PositionStateMachine::State::PSQ_SQQQ:
            // Split: 50% PSQ + 50% SQQQ (blended short)
            positions["PSQ"] = (total_capital * 0.5) / price;
            positions["SQQQ"] = (total_capital * 0.5) / price;
            break;

        default:
            // INVALID or unknown state - go to cash
            break;
    }

    return positions;
}

// Multi-instrument position calculation - uses correct price for each instrument
std::map<std::string, double> ExecuteTradesCommand::calculate_target_positions_multi(
    PositionStateMachine::State state,
    double total_capital,
    const std::map<std::string, std::vector<Bar>>& instrument_bars,
    size_t bar_index,
    const SymbolMap& symbol_map) {

    std::map<std::string, double> positions;

    switch (state) {
        case PositionStateMachine::State::CASH_ONLY:
            // No positions - all cash
            break;

        case PositionStateMachine::State::QQQ_ONLY:
            // 100% in base symbol (moderate long, 1x)
            if (instrument_bars.count(symbol_map.base) && bar_index < instrument_bars.at(symbol_map.base).size()) {
                positions[symbol_map.base] = total_capital / instrument_bars.at(symbol_map.base)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::TQQQ_ONLY:
            // 100% in leveraged bull (strong long, 3x leverage)
            if (instrument_bars.count(symbol_map.bull_3x) && bar_index < instrument_bars.at(symbol_map.bull_3x).size()) {
                positions[symbol_map.bull_3x] = total_capital / instrument_bars.at(symbol_map.bull_3x)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::PSQ_ONLY:
            // 100% in moderate bear (moderate short, -1x)
            if (instrument_bars.count(symbol_map.bear_1x) && bar_index < instrument_bars.at(symbol_map.bear_1x).size()) {
                positions[symbol_map.bear_1x] = total_capital / instrument_bars.at(symbol_map.bear_1x)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::SQQQ_ONLY:
            // 100% in leveraged bear (strong short, -2x or -3x)
            if (instrument_bars.count(symbol_map.bear_nx) && bar_index < instrument_bars.at(symbol_map.bear_nx).size()) {
                positions[symbol_map.bear_nx] = total_capital / instrument_bars.at(symbol_map.bear_nx)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::QQQ_TQQQ:
            // Split: 50% base + 50% leveraged bull (blended long)
            if (instrument_bars.count(symbol_map.base) && bar_index < instrument_bars.at(symbol_map.base).size()) {
                positions[symbol_map.base] = (total_capital * 0.5) / instrument_bars.at(symbol_map.base)[bar_index].close;
            }
            if (instrument_bars.count(symbol_map.bull_3x) && bar_index < instrument_bars.at(symbol_map.bull_3x).size()) {
                positions[symbol_map.bull_3x] = (total_capital * 0.5) / instrument_bars.at(symbol_map.bull_3x)[bar_index].close;
            }
            break;

        case PositionStateMachine::State::PSQ_SQQQ:
            // Split: 50% moderate bear + 50% leveraged bear (blended short)
            if (instrument_bars.count(symbol_map.bear_1x) && bar_index < instrument_bars.at(symbol_map.bear_1x).size()) {
                positions[symbol_map.bear_1x] = (total_capital * 0.5) / instrument_bars.at(symbol_map.bear_1x)[bar_index].close;
            }
            if (instrument_bars.count(symbol_map.bear_nx) && bar_index < instrument_bars.at(symbol_map.bear_nx).size()) {
                positions[symbol_map.bear_nx] = (total_capital * 0.5) / instrument_bars.at(symbol_map.bear_nx)[bar_index].close;
            }
            break;

        default:
            // INVALID or unknown state - go to cash
            break;
    }

    return positions;
}

void ExecuteTradesCommand::save_trades_jsonl(const PortfolioHistory& history,
                                            const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    for (const auto& trade : history.trades) {
        out << "{"
            << "\"bar_id\":" << trade.bar_id << ","
            << "\"timestamp_ms\":" << trade.timestamp_ms << ","
            << "\"bar_index\":" << trade.bar_index << ","
            << "\"symbol\":\"" << trade.symbol << "\","
            << "\"action\":\"" << (trade.action == TradeAction::BUY ? "BUY" : "SELL") << "\","
            << "\"quantity\":" << std::fixed << std::setprecision(4) << trade.quantity << ","
            << "\"price\":" << std::setprecision(2) << trade.price << ","
            << "\"trade_value\":" << trade.trade_value << ","
            << "\"fees\":" << trade.fees << ","
            << "\"cash_balance\":" << trade.cash_balance << ","
            << "\"portfolio_value\":" << trade.portfolio_value << ","
            << "\"position_quantity\":" << trade.position_quantity << ","
            << "\"position_avg_price\":" << trade.position_avg_price << ","
            << "\"reason\":\"" << trade.reason << "\""
            << "}\n";
    }
}

void ExecuteTradesCommand::save_trades_csv(const PortfolioHistory& history,
                                          const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    // Header
    out << "bar_id,timestamp_ms,bar_index,symbol,action,quantity,price,trade_value,fees,"
        << "cash_balance,portfolio_value,position_quantity,position_avg_price,reason\n";

    // Data
    for (const auto& trade : history.trades) {
        out << trade.bar_id << ","
            << trade.timestamp_ms << ","
            << trade.bar_index << ","
            << trade.symbol << ","
            << (trade.action == TradeAction::BUY ? "BUY" : "SELL") << ","
            << std::fixed << std::setprecision(4) << trade.quantity << ","
            << std::setprecision(2) << trade.price << ","
            << trade.trade_value << ","
            << trade.fees << ","
            << trade.cash_balance << ","
            << trade.portfolio_value << ","
            << trade.position_quantity << ","
            << trade.position_avg_price << ","
            << "\"" << trade.reason << "\"\n";
    }
}

void ExecuteTradesCommand::save_equity_curve(const PortfolioHistory& history,
                                            const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open equity curve file: " + path);
    }

    // Header
    out << "bar_index,equity,drawdown\n";

    // Data
    for (size_t i = 0; i < history.equity_curve.size(); ++i) {
        double drawdown = (i < history.drawdown_curve.size()) ? history.drawdown_curve[i] : 0.0;
        out << i << ","
            << std::fixed << std::setprecision(2) << history.equity_curve[i] << ","
            << std::setprecision(4) << drawdown << "\n";
    }
}

void ExecuteTradesCommand::show_help() const {
    std::cout << R"(
Execute OnlineEnsemble Trades
==============================

Execute trades from signal file and generate portfolio history.

USAGE:
    sentio_cli execute-trades --signals <path> --data <path> [OPTIONS]

REQUIRED:
    --signals <path>           Path to signal file (JSONL or CSV)
    --data <path>              Path to market data file

OPTIONS:
    --output <path>            Output trade file (default: trades.jsonl)
    --capital <amount>         Starting capital (default: 100000)
    --buy-threshold <val>      Buy signal threshold (default: 0.53)
    --sell-threshold <val>     Sell signal threshold (default: 0.47)
    --no-kelly                 Disable Kelly criterion sizing
    --csv                      Output in CSV format
    --verbose, -v              Show each trade

PSM RISK MANAGEMENT (Optuna-optimizable):
    --profit-target <val>      Profit target % (default: 0.003 = 0.3%)
    --stop-loss <val>          Stop loss % (default: -0.004 = -0.4%)
    --min-hold-bars <n>        Min holding period (default: 3 bars)
    --max-hold-bars <n>        Max holding period (default: 100 bars)

EXAMPLES:
    # Execute trades with default settings
    sentio_cli execute-trades --signals signals.jsonl --data data/SPY.csv

    # Custom capital and thresholds
    sentio_cli execute-trades --signals signals.jsonl --data data/QQQ.bin \
        --capital 50000 --buy-threshold 0.55 --sell-threshold 0.45

    # Verbose mode with CSV output
    sentio_cli execute-trades --signals signals.jsonl --data data/futures.bin \
        --verbose --csv --output trades.csv

    # Custom PSM parameters (for Optuna optimization)
    sentio_cli execute-trades --signals signals.jsonl --data data/SPY.csv \
        --profit-target 0.005 --stop-loss -0.006 --min-hold-bars 5

OUTPUT FILES:
    - trades.jsonl (or .csv)   Trade-by-trade history
    - trades_equity.csv        Equity curve and drawdowns

)" << std::endl;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 38 of 40**: src/cli/live_trade_command.cpp

**File Information**:
- **Path**: `src/cli/live_trade_command.cpp`
- **Size**: 2345 lines
- **Modified**: 2025-10-16 06:18:20
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/live_trade_command.hpp"
#include "live/alpaca_client.hpp"
#include "live/polygon_client.hpp"
#include "live/position_book.h"
#include "live/broker_client_interface.h"
#include "live/bar_feed_interface.h"
#include "live/mock_broker.h"
#include "live/mock_bar_feed_replay.h"
#include "live/alpaca_client_adapter.h"
#include "live/polygon_client_adapter.h"
// #include "live/alpaca_rest_bar_feed.h"  // DISABLED: incomplete implementation
#include "live/mock_config.h"
#include "live/state_persistence.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/position_state_machine.h"
#include "common/time_utils.h"
#include "common/bar_validator.h"
#include "common/exceptions.h"
#include "common/eod_state.h"
#include "common/nyse_calendar.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <optional>
#include <memory>
#include <csignal>
#include <atomic>

namespace sentio {
namespace cli {

// Global pointer for signal handler (necessary for C-style signal handlers)
static std::atomic<bool> g_shutdown_requested{false};

/**
 * Create OnlineEnsemble v1.0 configuration with asymmetric thresholds
 * Target: 0.6086% MRB (10.5% monthly, 125% annual)
 *
 * Now loads optimized parameters from midday_selected_params.json if available
 */
static OnlineEnsembleStrategy::OnlineEnsembleConfig create_v1_config(bool is_mock = false) {
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;

    // Default v1.0 parameters
    config.buy_threshold = 0.55;
    config.sell_threshold = 0.45;
    config.neutral_zone = 0.10;
    config.ewrls_lambda = 0.995;
    config.warmup_samples = is_mock ? 780 : 7800;  // Mock: 2 blocks, Live: 20 blocks
    config.enable_bb_amplification = true;
    config.bb_amplification_factor = 0.10;
    config.bb_period = 20;
    config.bb_std_dev = 2.0;
    config.bb_proximity_threshold = 0.30;
    config.regularization = 0.01;
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;
    config.enable_regime_detection = false;
    config.regime_check_interval = 60;

    // Try to load optimized parameters from JSON file
    std::string json_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/midday_selected_params.json";
    std::ifstream file(json_file);

    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            file.close();

            // Load phase 1 parameters
            config.buy_threshold = j.value("buy_threshold", config.buy_threshold);
            config.sell_threshold = j.value("sell_threshold", config.sell_threshold);
            config.bb_amplification_factor = j.value("bb_amplification_factor", config.bb_amplification_factor);
            config.ewrls_lambda = j.value("ewrls_lambda", config.ewrls_lambda);

            // Load phase 2 parameters
            double h1 = j.value("h1_weight", 0.3);
            double h5 = j.value("h5_weight", 0.5);
            double h10 = j.value("h10_weight", 0.2);
            config.horizon_weights = {h1, h5, h10};
            config.bb_period = j.value("bb_period", config.bb_period);
            config.bb_std_dev = j.value("bb_std_dev", config.bb_std_dev);
            config.bb_proximity_threshold = j.value("bb_proximity", config.bb_proximity_threshold);
            config.regularization = j.value("regularization", config.regularization);

            std::cout << "✅ Loaded optimized parameters from: " << json_file << std::endl;
            std::cout << "   Source: " << j.value("source", "unknown") << std::endl;
            std::cout << "   MRB target: " << j.value("expected_mrb", 0.0) << "%" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Failed to load optimized parameters: " << e.what() << std::endl;
            std::cerr << "   Using default configuration" << std::endl;
        }
    }

    return config;
}

/**
 * Load leveraged ETF prices from CSV files for mock mode
 * Returns: map[timestamp_sec][symbol] -> close_price
 */
static std::unordered_map<uint64_t, std::unordered_map<std::string, double>>
load_leveraged_prices(const std::string& base_path) {
    std::unordered_map<uint64_t, std::unordered_map<std::string, double>> prices;

    std::vector<std::string> symbols = {"SH", "SDS", "SPXL"};

    for (const auto& symbol : symbols) {
        std::string filepath = base_path + "/" + symbol + "_yesterday.csv";
        std::ifstream file(filepath);

        if (!file.is_open()) {
            std::cerr << "⚠️  Warning: Could not load " << filepath << std::endl;
            continue;
        }

        std::string line;
        int line_count = 0;
        while (std::getline(file, line)) {
            // Skip empty lines or header-like lines
            if (line.empty() ||
                line.find("timestamp") != std::string::npos ||
                line.find("ts_utc") != std::string::npos ||
                line.find("ts_nyt_epoch") != std::string::npos) {
                continue;
            }

            std::istringstream iss(line);
            std::string date_str, ts_str, o, h, l, close_str, v;

            if (std::getline(iss, date_str, ',') &&
                std::getline(iss, ts_str, ',') &&
                std::getline(iss, o, ',') &&
                std::getline(iss, h, ',') &&
                std::getline(iss, l, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, v)) {

                uint64_t timestamp_sec = std::stoull(ts_str);
                double close_price = std::stod(close_str);

                prices[timestamp_sec][symbol] = close_price;
                line_count++;
            }
        }

        if (line_count > 0) {
            std::cout << "✅ Loaded " << line_count << " bars for " << symbol << std::endl;
        }
    }

    return prices;
}

/**
 * Micro-Adaptation State Tracker
 *
 * Tracks and applies small parameter adjustments during live trading
 * based on recent performance and market conditions.
 *
 * Key Features:
 * - Hourly threshold drift (±0.002 max per hour, ±1% max total drift)
 * - Volatility-based lambda adaptation (every 30 bars)
 * - Performance-based adjustments (win rate tracking)
 * - Saves baseline from morning optimization
 */
struct MicroAdaptationState {
    // Baseline parameters from morning optimization
    double baseline_buy_threshold{0.511};
    double baseline_sell_threshold{0.47};
    double baseline_lambda{0.984};

    // Current adapted values
    double current_buy_threshold{0.511};
    double current_sell_threshold{0.47};
    double current_lambda{0.984};

    // Adaptation tracking
    int bars_since_last_threshold_adapt{0};
    int bars_since_last_lambda_adapt{0};
    int adaptation_hour{0};  // Track which hour we're in

    // Performance tracking for adaptation
    int recent_wins{0};
    int recent_losses{0};
    int trades_this_period{0};
    std::vector<double> recent_returns;  // Last 30 bars for volatility calc

    // Adaptation limits
    static constexpr double MAX_HOURLY_DRIFT = 0.002;  // ±0.2% per hour
    static constexpr double MAX_TOTAL_DRIFT = 0.01;     // ±1% total drift from baseline
    static constexpr int THRESHOLD_ADAPT_INTERVAL = 60; // 60 bars = 1 hour
    static constexpr int LAMBDA_ADAPT_INTERVAL = 30;     // 30 bars = 30 minutes

    bool enabled{false};  // Micro-adaptations on/off

    /**
     * Load baseline parameters from morning optimization
     */
    bool load_baseline(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "⚠️  Could not load baseline params from: " << filepath << std::endl;
            return false;
        }

        try {
            nlohmann::json j;
            file >> j;
            file.close();

            baseline_buy_threshold = j.value("buy_threshold", 0.511);
            baseline_sell_threshold = j.value("sell_threshold", 0.47);
            baseline_lambda = j.value("ewrls_lambda", 0.984);

            // Initialize current values to baseline
            current_buy_threshold = baseline_buy_threshold;
            current_sell_threshold = baseline_sell_threshold;
            current_lambda = baseline_lambda;

            std::cout << "✅ Loaded baseline parameters for micro-adaptation:" << std::endl;
            std::cout << "   buy_threshold: " << baseline_buy_threshold << std::endl;
            std::cout << "   sell_threshold: " << baseline_sell_threshold << std::endl;
            std::cout << "   lambda: " << baseline_lambda << std::endl;

            return true;
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Failed to parse baseline params: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * Perform hourly threshold micro-adaptation
     * Adjusts thresholds by ±0.002 based on recent win rate
     */
    void adapt_thresholds_hourly() {
        if (!enabled) return;

        bars_since_last_threshold_adapt = 0;
        adaptation_hour++;

        // Calculate win rate from recent trades
        int total_trades = recent_wins + recent_losses;
        if (total_trades < 5) {
            // Not enough data - no adaptation
            return;
        }

        double win_rate = static_cast<double>(recent_wins) / total_trades;

        // Adaptation logic:
        // - High win rate (>60%) → tighten thresholds (more aggressive)
        // - Low win rate (<40%) → widen thresholds (more conservative)
        // - Neutral (40-60%) → small random walk

        double drift_amount = 0.0;
        if (win_rate > 0.60) {
            // Winning strategy - tighten thresholds
            drift_amount = -MAX_HOURLY_DRIFT;  // Move closer together
        } else if (win_rate < 0.40) {
            // Losing strategy - widen thresholds
            drift_amount = +MAX_HOURLY_DRIFT;  // Move further apart
        } else {
            // Neutral - small random exploration (±0.001)
            drift_amount = ((std::rand() % 2) * 2 - 1) * (MAX_HOURLY_DRIFT * 0.5);
        }

        // Apply drift with max total drift limit
        double new_buy = current_buy_threshold + drift_amount;
        double new_sell = current_sell_threshold - drift_amount;

        // Enforce max total drift from baseline
        if (std::abs(new_buy - baseline_buy_threshold) <= MAX_TOTAL_DRIFT) {
            current_buy_threshold = new_buy;
        }
        if (std::abs(new_sell - baseline_sell_threshold) <= MAX_TOTAL_DRIFT) {
            current_sell_threshold = new_sell;
        }

        // Ensure thresholds don't cross
        if (current_buy_threshold <= current_sell_threshold) {
            current_buy_threshold = baseline_buy_threshold;
            current_sell_threshold = baseline_sell_threshold;
        }

        // Reset performance counters for next period
        recent_wins = 0;
        recent_losses = 0;
        trades_this_period = 0;
    }

    /**
     * Perform volatility-based lambda adaptation
     * Higher volatility → lower lambda (faster adaptation)
     */
    void adapt_lambda_on_volatility() {
        if (!enabled) return;
        if (recent_returns.size() < 20) return;  // Need minimum data

        bars_since_last_lambda_adapt = 0;

        // Calculate recent return volatility
        double mean = 0.0;
        for (double ret : recent_returns) {
            mean += ret;
        }
        mean /= recent_returns.size();

        double variance = 0.0;
        for (double ret : recent_returns) {
            variance += (ret - mean) * (ret - mean);
        }
        double volatility = std::sqrt(variance / recent_returns.size());

        // Adaptation logic:
        // - High volatility (>0.015) → lower lambda (0.980-0.985) for fast adaptation
        // - Low volatility (<0.005) → higher lambda (0.990-0.995) for stability
        // - Normal volatility → baseline lambda

        if (volatility > 0.015) {
            // High volatility - fast adaptation
            current_lambda = std::max(baseline_lambda - 0.005, 0.980);
        } else if (volatility < 0.005) {
            // Low volatility - stable learning
            current_lambda = std::min(baseline_lambda + 0.005, 0.995);
        } else {
            // Normal volatility - use baseline
            current_lambda = baseline_lambda;
        }
    }

    /**
     * Update adaptation state after each bar
     */
    void update_on_bar(double bar_return) {
        bars_since_last_threshold_adapt++;
        bars_since_last_lambda_adapt++;

        // Track recent returns for volatility calculation
        recent_returns.push_back(bar_return);
        if (recent_returns.size() > 30) {
            recent_returns.erase(recent_returns.begin());
        }

        // Trigger hourly threshold adaptation
        if (bars_since_last_threshold_adapt >= THRESHOLD_ADAPT_INTERVAL) {
            adapt_thresholds_hourly();
        }

        // Trigger volatility-based lambda adaptation
        if (bars_since_last_lambda_adapt >= LAMBDA_ADAPT_INTERVAL) {
            adapt_lambda_on_volatility();
        }
    }

    /**
     * Record trade outcome for performance tracking
     */
    void record_trade(bool won) {
        if (won) {
            recent_wins++;
        } else {
            recent_losses++;
        }
        trades_this_period++;
    }
};

/**
 * Live Trading Runner for OnlineEnsemble Strategy v1.0
 *
 * - Trades SPY/SDS/SPXL/SH during regular hours (9:30am - 4:00pm ET)
 * - Uses OnlineEnsemble EWRLS with asymmetric thresholds
 * - Comprehensive logging of all decisions and trades
 */
class LiveTrader {
public:
    LiveTrader(std::unique_ptr<IBrokerClient> broker,
               std::unique_ptr<IBarFeed> bar_feed,
               const std::string& log_dir,
               bool is_mock_mode = false,
               const std::string& data_file = "")
        : broker_(std::move(broker))
        , bar_feed_(std::move(bar_feed))
        , log_dir_(log_dir)
        , is_mock_mode_(is_mock_mode)
        , data_file_(data_file)
        , strategy_(create_v1_config(is_mock_mode))
        , psm_()
        , current_state_(PositionStateMachine::State::CASH_ONLY)
        , bars_held_(0)
        , entry_equity_(100000.0)
        , previous_portfolio_value_(100000.0)  // Initialize to starting equity
        , et_time_()  // Initialize ET time manager
        , eod_state_(log_dir + "/eod_state.txt")  // Persistent EOD tracking
        , nyse_calendar_()  // NYSE holiday calendar
        , state_persistence_(std::make_unique<StatePersistence>(log_dir + "/state"))  // State persistence
    {
        // Initialize log files
        init_logs();

        // SPY trading configuration (maps to sentio PSM states)
        symbol_map_ = {
            {"SPY", "SPY"},      // Base 1x
            {"SPXL", "SPXL"},    // Bull 3x
            {"SH", "SH"},        // Bear -1x
            {"SDS", "SDS"}       // Bear -2x
        };
    }

    ~LiveTrader() {
        // Generate dashboard on exit
        generate_dashboard();
    }

    void run() {
        if (is_mock_mode_) {
            log_system("=== OnlineTrader v1.0 Mock Trading Started ===");
            log_system("Mode: MOCK REPLAY (39x speed)");
        } else {
            log_system("=== OnlineTrader v1.0 Live Paper Trading Started ===");
            log_system("Mode: LIVE TRADING");
        }
        log_system("Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)");
        log_system("Trading Hours: 9:30am - 4:00pm ET (Regular Hours Only)");
        log_system("Strategy: OnlineEnsemble EWRLS with Asymmetric Thresholds");
        log_system("");

        // Connect to broker (Alpaca or Mock)
        log_system(is_mock_mode_ ? "Initializing Mock Broker..." : "Connecting to Alpaca Paper Trading...");
        auto account = broker_->get_account();
        if (!account) {
            log_error("Failed to get account");
            return;
        }
        log_system("✓ Account ready - ID: " + account->account_number);
        log_system("  Starting Capital: $" + std::to_string(account->portfolio_value));
        entry_equity_ = account->portfolio_value;

        // Connect to bar feed (Polygon or Mock)
        log_system(is_mock_mode_ ? "Loading mock bar feed..." : "Connecting to Polygon proxy...");
        if (!bar_feed_->connect()) {
            log_error("Failed to connect to bar feed");
            return;
        }
        log_system(is_mock_mode_ ? "✓ Mock bars loaded" : "✓ Connected to Polygon");

        // In mock mode, load leveraged ETF prices
        if (is_mock_mode_) {
            log_system("Loading leveraged ETF prices for mock mode...");
            leveraged_prices_ = load_leveraged_prices("/tmp");
            if (!leveraged_prices_.empty()) {
                log_system("✓ Leveraged ETF prices loaded (SH, SDS, SPXL)");
            } else {
                log_system("⚠️  Warning: No leveraged ETF prices loaded - using fallback prices");
            }
            log_system("");
        }

        // Subscribe to symbols (SPY instruments)
        std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};
        if (!bar_feed_->subscribe(symbols)) {
            log_error("Failed to subscribe to symbols");
            return;
        }
        log_system("✓ Subscribed to SPY, SPXL, SH, SDS");
        log_system("");

        // Reconcile existing positions on startup (seamless continuation)
        reconcile_startup_positions();

        // Check for missed EOD and startup catch-up liquidation
        check_startup_eod_catch_up();

        // Initialize strategy with warmup
        log_system("Initializing OnlineEnsemble strategy...");
        warmup_strategy();
        log_system("✓ Strategy initialized and ready");
        log_system("");

        // Initialize micro-adaptation system (load baseline from morning optimization)
        std::string baseline_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/morning_baseline_params.json";
        if (std::ifstream(baseline_file).good()) {
            if (micro_adaptation_.load_baseline(baseline_file)) {
                micro_adaptation_.enabled = true;
                log_system("✅ Micro-adaptation system enabled");
                log_system("   Baseline buy: " + std::to_string(micro_adaptation_.baseline_buy_threshold));
                log_system("   Baseline sell: " + std::to_string(micro_adaptation_.baseline_sell_threshold));
                log_system("   Hourly drift: ±" + std::to_string(MicroAdaptationState::MAX_HOURLY_DRIFT * 100) + "%");
                log_system("   Max total drift: ±" + std::to_string(MicroAdaptationState::MAX_TOTAL_DRIFT * 100) + "%");
            } else {
                log_system("⚠️  Micro-adaptation baseline load failed - using static params");
            }
        } else {
            log_system("ℹ️  No baseline params found - micro-adaptation disabled");
            log_system("   (Run morning optimization to enable micro-adaptations)");
        }
        log_system("");

        // Start main trading loop
        bar_feed_->start([this](const std::string& symbol, const Bar& bar) {
            if (symbol == "SPY") {  // Only process on SPY bars (trigger for multi-instrument PSM)
                on_new_bar(bar);
            }
        });

        log_system("=== Live trading active - Press Ctrl+C to stop ===");
        log_system("");

        // Install signal handlers for graceful shutdown
        std::signal(SIGINT, [](int) { g_shutdown_requested = true; });
        std::signal(SIGTERM, [](int) { g_shutdown_requested = true; });

        // Keep running until shutdown requested
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Auto-shutdown at market close (4:00 PM ET) after EOD liquidation completes
            std::string today_et = et_time_.get_current_et_date();
            if (et_time_.is_market_close_time() && eod_state_.is_eod_complete(today_et)) {
                log_system("⏰ Market closed and EOD complete - initiating automatic shutdown");
                g_shutdown_requested = true;
            }
        }

        log_system("=== Shutdown requested - cleaning up ===");
    }

private:
    std::unique_ptr<IBrokerClient> broker_;
    std::unique_ptr<IBarFeed> bar_feed_;
    std::string log_dir_;
    bool is_mock_mode_;
    std::string data_file_;  // Path to market data CSV file for dashboard generation
    OnlineEnsembleStrategy strategy_;
    PositionStateMachine psm_;
    std::map<std::string, std::string> symbol_map_;

    // NEW: Production safety infrastructure
    PositionBook position_book_;
    ETTimeManager et_time_;  // Centralized ET time management
    EodStateStore eod_state_;  // Idempotent EOD tracking
    NyseCalendar nyse_calendar_;  // Holiday and half-day calendar
    std::unique_ptr<StatePersistence> state_persistence_;  // Atomic state persistence
    std::optional<Bar> previous_bar_;  // For bar-to-bar learning
    uint64_t bar_count_{0};

    // Mid-day optimization (15:15 PM ET / 3:15pm)
    std::vector<Bar> todays_bars_;  // Collect ALL bars from 9:30 onwards
    bool midday_optimization_done_{false};  // Flag to track if optimization ran today
    std::string midday_optimization_date_;  // Date of last optimization (YYYY-MM-DD)

    // State tracking
    PositionStateMachine::State current_state_;
    int bars_held_;
    double entry_equity_;
    double previous_portfolio_value_;  // Track portfolio value before trade for P&L calculation

    // Mock mode: Leveraged ETF prices loaded from CSV
    std::unordered_map<uint64_t, std::unordered_map<std::string, double>> leveraged_prices_;

    // Micro-adaptation system (NEW v2.6)
    MicroAdaptationState micro_adaptation_;

    // Log file streams
    std::ofstream log_system_;
    std::ofstream log_signals_;
    std::ofstream log_trades_;
    std::ofstream log_positions_;
    std::ofstream log_decisions_;
    std::string session_timestamp_;  // Store timestamp for dashboard generation

    // Risk management (v1.0 parameters)
    const double PROFIT_TARGET = 0.02;   // 2%
    const double STOP_LOSS = -0.015;     // -1.5%
    const int MIN_HOLD_BARS = 3;
    const int MAX_HOLD_BARS = 100;

    void init_logs() {
        // Create log directory if needed
        system(("mkdir -p " + log_dir_).c_str());

        session_timestamp_ = get_timestamp();

        log_system_.open(log_dir_ + "/system_" + session_timestamp_ + ".log");
        log_signals_.open(log_dir_ + "/signals_" + session_timestamp_ + ".jsonl");
        log_trades_.open(log_dir_ + "/trades_" + session_timestamp_ + ".jsonl");
        log_positions_.open(log_dir_ + "/positions_" + session_timestamp_ + ".jsonl");
        log_decisions_.open(log_dir_ + "/decisions_" + session_timestamp_ + ".jsonl");
    }

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    std::string get_timestamp_readable() const {
        return et_time_.get_current_et_string();
    }

    bool is_regular_hours() const {
        return et_time_.is_regular_hours();
    }

    bool is_end_of_day_liquidation_time() const {
        return et_time_.is_eod_liquidation_window();
    }

    void log_system(const std::string& message) {
        auto timestamp = get_timestamp_readable();
        std::cout << "[" << timestamp << "] " << message << std::endl;
        log_system_ << "[" << timestamp << "] " << message << std::endl;
        log_system_.flush();
    }

    void log_error(const std::string& message) {
        log_system("ERROR: " + message);
    }

    void generate_dashboard() {
        // Close log files to ensure all data is flushed
        log_system_.close();
        log_signals_.close();
        log_trades_.close();
        log_positions_.close();
        log_decisions_.close();

        std::cout << "\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "📊 Generating Trading Dashboard...\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

        // Construct file paths
        std::string trades_file = log_dir_ + "/trades_" + session_timestamp_ + ".jsonl";
        std::string signals_file = log_dir_ + "/signals_" + session_timestamp_ + ".jsonl";
        std::string dashboard_dir = "data/dashboards";
        std::string dashboard_file = dashboard_dir + "/session_" + session_timestamp_ + ".html";

        // Create dashboard directory
        system(("mkdir -p " + dashboard_dir).c_str());

        // Build Python command
        std::string python_cmd = "python3 tools/professional_trading_dashboard.py "
                                "--tradebook " + trades_file + " "
                                "--signals " + signals_file + " "
                                "--output " + dashboard_file + " "
                                "--start-equity 100000 ";

        // Add data file if available (for candlestick charts and trade markers)
        if (!data_file_.empty()) {
            python_cmd += "--data " + data_file_ + " ";
        }

        python_cmd += "> /dev/null 2>&1";

        std::cout << "  Tradebook: " << trades_file << "\n";
        std::cout << "  Signals: " << signals_file << "\n";
        if (!data_file_.empty()) {
            std::cout << "  Data: " + data_file_ + "\n";
        }
        std::cout << "  Output: " << dashboard_file << "\n";
        std::cout << "\n";

        // Execute Python dashboard generator
        int result = system(python_cmd.c_str());

        if (result == 0) {
            std::cout << "✅ Dashboard generated successfully!\n";
            std::cout << "   📂 Open: " << dashboard_file << "\n";
            std::cout << "\n";

            // Send email notification (works in both live and mock modes)
            std::cout << "📧 Sending email notification...\n";

            std::string email_cmd = "python3 tools/send_dashboard_email.py "
                                   "--dashboard " + dashboard_file + " "
                                   "--trades " + trades_file + " "
                                   "--recipient yeogirl@gmail.com "
                                   "> /dev/null 2>&1";

            int email_result = system(email_cmd.c_str());

            if (email_result == 0) {
                std::cout << "✅ Email sent to yeogirl@gmail.com\n";
            } else {
                std::cout << "⚠️  Email sending failed (check GMAIL_APP_PASSWORD)\n";
            }
        } else {
            std::cout << "⚠️  Dashboard generation failed (exit code: " << result << ")\n";
            std::cout << "   You can manually generate it with:\n";
            std::cout << "   python3 tools/professional_trading_dashboard.py \\\n";
            std::cout << "     --tradebook " << trades_file << " \\\n";
            std::cout << "     --signals " << signals_file << " \\\n";
            std::cout << "     --output " << dashboard_file << "\n";
        }

        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "\n";
    }

    void reconcile_startup_positions() {
        log_system("=== Startup Position Reconciliation ===");

        // Get current broker state
        auto account = broker_->get_account();
        if (!account) {
            log_error("Failed to get account info for startup reconciliation");
            return;
        }

        auto broker_positions = get_broker_positions();

        log_system("  Cash: $" + std::to_string(account->cash));
        log_system("  Portfolio Value: $" + std::to_string(account->portfolio_value));

        // ===================================================================
        // STEP 1: Try to load persisted state from previous session
        // ===================================================================
        if (auto persisted = state_persistence_->load_state()) {
            log_system("[STATE_PERSIST] ✓ Found persisted state from previous session");
            log_system("  Session ID: " + persisted->session_id);
            log_system("  Last save: " + persisted->last_bar_time_str);
            log_system("  PSM State: " + psm_.state_to_string(persisted->psm_state));
            log_system("  Bars held: " + std::to_string(persisted->bars_held));

            // Validate positions match broker
            bool positions_match = validate_positions_match(persisted->positions, broker_positions);

            if (positions_match) {
                log_system("[STATE_PERSIST] ✓ Positions match broker - restoring exact state");

                // Restore exact state
                current_state_ = persisted->psm_state;
                bars_held_ = persisted->bars_held;
                entry_equity_ = persisted->entry_equity;

                // Calculate bars elapsed since last save
                if (previous_bar_.has_value()) {
                    uint64_t bars_elapsed = calculate_bars_since(
                        persisted->last_bar_timestamp,
                        previous_bar_->timestamp_ms
                    );
                    bars_held_ += bars_elapsed;
                    log_system("  Adjusted bars held: " + std::to_string(bars_held_) +
                              " (+" + std::to_string(bars_elapsed) + " bars since save)");
                }

                // Initialize position book
                for (const auto& pos : broker_positions) {
                    position_book_.set_position(pos.symbol, pos.qty, pos.avg_entry_price);
                }

                log_system("✓ State fully recovered from persistence");
                log_system("");
                return;
            } else {
                log_system("[STATE_PERSIST] ⚠️  Position mismatch - falling back to broker reconciliation");
            }
        } else {
            log_system("[STATE_PERSIST] No persisted state found - using broker reconciliation");
        }

        // ===================================================================
        // STEP 2: Fall back to broker-based reconciliation
        // ===================================================================
        if (broker_positions.empty()) {
            log_system("  Current Positions: NONE (starting flat)");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;
            log_system("  Initial State: CASH_ONLY");
            log_system("  Bars Held: 0 (no positions)");
        } else {
            log_system("  Current Positions:");
            for (const auto& pos : broker_positions) {
                log_system("    " + pos.symbol + ": " +
                          std::to_string(pos.qty) + " shares @ $" +
                          std::to_string(pos.avg_entry_price) +
                          " (P&L: $" + std::to_string(pos.unrealized_pnl) + ")");

                // Initialize position book with existing positions
                position_book_.set_position(pos.symbol, pos.qty, pos.avg_entry_price);
            }

            // Infer current PSM state from positions
            current_state_ = infer_state_from_positions(broker_positions);

            // CRITICAL FIX: Set bars_held to MIN_HOLD_BARS to allow immediate exits
            // since we don't know how long the positions have been held
            bars_held_ = MIN_HOLD_BARS;

            log_system("  Inferred PSM State: " + psm_.state_to_string(current_state_));
            log_system("  Bars Held: " + std::to_string(bars_held_) +
                      " (set to MIN_HOLD to allow immediate exits on startup)");
            log_system("  NOTE: Positions were reconciled from broker - assuming min hold satisfied");
        }

        log_system("✓ Startup reconciliation complete - resuming trading seamlessly");
        log_system("");
    }

    void check_startup_eod_catch_up() {
        log_system("=== Startup EOD Catch-Up Check ===");

        auto et_tm = et_time_.get_current_et_tm();
        std::string today_et = format_et_date(et_tm);
        std::string prev_trading_day = get_previous_trading_day(et_tm);

        log_system("  Current ET Time: " + et_time_.get_current_et_string());
        log_system("  Today (ET): " + today_et);
        log_system("  Previous Trading Day: " + prev_trading_day);

        // Check 1: Did we miss previous trading day's EOD?
        if (!eod_state_.is_eod_complete(prev_trading_day)) {
            log_system("  ⚠️  WARNING: Previous trading day's EOD not completed");

            auto broker_positions = get_broker_positions();
            if (!broker_positions.empty()) {
                log_system("  ⚠️  Open positions detected - executing catch-up liquidation");
                liquidate_all_positions();
                eod_state_.mark_eod_complete(prev_trading_day);
                log_system("  ✓ Catch-up liquidation complete for " + prev_trading_day);
            } else {
                log_system("  ✓ No open positions - marking previous EOD as complete");
                eod_state_.mark_eod_complete(prev_trading_day);
            }
        } else {
            log_system("  ✓ Previous trading day EOD already complete");
        }

        // Check 2: Started outside trading hours with positions?
        if (et_time_.should_liquidate_on_startup(has_open_positions())) {
            log_system("  ⚠️  Started outside trading hours with open positions");
            log_system("  ⚠️  Executing immediate liquidation");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(today_et);
            log_system("  ✓ Startup liquidation complete");
        }

        log_system("✓ Startup EOD check complete");
        log_system("");
    }

    std::string format_et_date(const std::tm& tm) const {
        char buffer[11];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        return std::string(buffer);
    }

    std::string get_previous_trading_day(const std::tm& current_tm) const {
        // Walk back day-by-day until we find a trading day
        std::tm tm = current_tm;
        for (int i = 1; i <= 10; ++i) {
            // Subtract i days (approximate - good enough for recent history)
            std::time_t t = std::mktime(&tm) - (i * 86400);
            std::tm* prev_tm = std::localtime(&t);
            std::string prev_date = format_et_date(*prev_tm);

            // Check if weekday and not holiday
            if (prev_tm->tm_wday >= 1 && prev_tm->tm_wday <= 5) {
                if (nyse_calendar_.is_trading_day(prev_date)) {
                    return prev_date;
                }
            }
        }
        // Fallback: return today if can't find previous trading day
        return format_et_date(current_tm);
    }

    bool has_open_positions() {
        auto broker_positions = get_broker_positions();
        return !broker_positions.empty();
    }

    PositionStateMachine::State infer_state_from_positions(
        const std::vector<BrokerPosition>& positions) {

        // Map SPY instruments to equivalent QQQ PSM states
        // SPY/SPXL/SH/SDS → QQQ/TQQQ/PSQ/SQQQ
        bool has_base = false;   // SPY
        bool has_bull3x = false; // SPXL
        bool has_bear1x = false; // SH
        bool has_bear_nx = false; // SDS

        for (const auto& pos : positions) {
            if (pos.qty > 0) {
                if (pos.symbol == "SPXL") has_bull3x = true;
                if (pos.symbol == "SPY") has_base = true;
                if (pos.symbol == "SH") has_bear1x = true;
                if (pos.symbol == "SDS") has_bear_nx = true;
            }
        }

        // Check for dual-instrument states first
        if (has_base && has_bull3x) return PositionStateMachine::State::QQQ_TQQQ;    // BASE_BULL_3X
        if (has_bear1x && has_bear_nx) return PositionStateMachine::State::PSQ_SQQQ; // BEAR_1X_NX

        // Single instrument states
        if (has_bull3x) return PositionStateMachine::State::TQQQ_ONLY;  // BULL_3X_ONLY
        if (has_base) return PositionStateMachine::State::QQQ_ONLY;     // BASE_ONLY
        if (has_bear1x) return PositionStateMachine::State::PSQ_ONLY;   // BEAR_1X_ONLY
        if (has_bear_nx) return PositionStateMachine::State::SQQQ_ONLY; // BEAR_NX_ONLY

        return PositionStateMachine::State::CASH_ONLY;
    }

    // =====================================================================
    // State Persistence Helper Methods
    // =====================================================================

    /**
     * Calculate number of 1-minute bars elapsed between two timestamps
     */
    uint64_t calculate_bars_since(uint64_t from_ts_ms, uint64_t to_ts_ms) const {
        if (to_ts_ms <= from_ts_ms) return 0;
        uint64_t elapsed_ms = to_ts_ms - from_ts_ms;
        uint64_t elapsed_minutes = elapsed_ms / (60 * 1000);
        return elapsed_minutes;
    }

    /**
     * Validate that persisted positions match broker positions
     */
    bool validate_positions_match(
        const std::vector<StatePersistence::PositionDetail>& persisted,
        const std::vector<BrokerPosition>& broker) {

        // Quick check: same number of positions
        if (persisted.size() != broker.size()) {
            log_system("  Position count mismatch: persisted=" +
                      std::to_string(persisted.size()) +
                      " broker=" + std::to_string(broker.size()));
            return false;
        }

        // Build maps for easier comparison
        std::map<std::string, double> persisted_map;
        for (const auto& p : persisted) {
            persisted_map[p.symbol] = p.quantity;
        }

        std::map<std::string, double> broker_map;
        for (const auto& p : broker) {
            broker_map[p.symbol] = p.qty;
        }

        // Check each symbol
        for (const auto& [symbol, qty] : persisted_map) {
            if (broker_map.find(symbol) == broker_map.end()) {
                log_system("  Symbol mismatch: " + symbol + " in persisted but not in broker");
                return false;
            }
            if (std::abs(broker_map[symbol] - qty) > 0.01) {  // Allow tiny floating point difference
                log_system("  Quantity mismatch for " + symbol + ": persisted=" +
                          std::to_string(qty) + " broker=" + std::to_string(broker_map[symbol]));
                return false;
            }
        }

        return true;
    }

    /**
     * Persist current trading state to disk
     */
    void persist_current_state() {
        try {
            StatePersistence::TradingState state;
            state.psm_state = current_state_;
            state.bars_held = bars_held_;
            state.entry_equity = entry_equity_;

            if (previous_bar_.has_value()) {
                state.last_bar_timestamp = previous_bar_->timestamp_ms;
                state.last_bar_time_str = format_bar_time(*previous_bar_);
            }

            // Add current positions
            auto broker_positions = get_broker_positions();
            for (const auto& pos : broker_positions) {
                StatePersistence::PositionDetail detail;
                detail.symbol = pos.symbol;
                detail.quantity = pos.qty;
                detail.avg_entry_price = pos.avg_entry_price;
                detail.entry_timestamp = previous_bar_ ? previous_bar_->timestamp_ms : 0;
                state.positions.push_back(detail);
            }

            state.session_id = session_timestamp_;

            if (!state_persistence_->save_state(state)) {
                log_system("⚠️  State persistence failed (non-fatal - continuing)");
            }

        } catch (const std::exception& e) {
            log_system("⚠️  State persistence error: " + std::string(e.what()));
        }
    }

    void warmup_strategy() {
        // Load warmup data created by comprehensive_warmup.sh script
        // This file contains: 7864 warmup bars (20 blocks @ 390 bars/block + 64 feature bars) + all of today's bars up to now
        std::string warmup_file = "data/equities/SPY_warmup_latest.csv";

        // Try relative path first, then from parent directory
        std::ifstream file(warmup_file);
        if (!file.is_open()) {
            warmup_file = "../data/equities/SPY_warmup_latest.csv";
            file.open(warmup_file);
        }

        if (!file.is_open()) {
            log_system("WARNING: Could not open warmup file: " + warmup_file);
            log_system("         Run tools/warmup_live_trading.sh first!");
            log_system("         Strategy will learn from first few live bars");
            return;
        }

        // Read all bars from warmup file
        std::vector<Bar> all_bars;
        std::string line;
        std::getline(file, line); // Skip header

        while (std::getline(file, line)) {
            // Skip empty lines or header-like lines
            if (line.empty() ||
                line.find("timestamp") != std::string::npos ||
                line.find("ts_utc") != std::string::npos ||
                line.find("ts_nyt_epoch") != std::string::npos) {
                continue;
            }

            std::istringstream iss(line);
            std::string ts_utc_str, ts_epoch_str, open_str, high_str, low_str, close_str, volume_str;

            // CSV format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            if (std::getline(iss, ts_utc_str, ',') &&
                std::getline(iss, ts_epoch_str, ',') &&
                std::getline(iss, open_str, ',') &&
                std::getline(iss, high_str, ',') &&
                std::getline(iss, low_str, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, volume_str)) {

                Bar bar;
                bar.timestamp_ms = std::stoll(ts_epoch_str) * 1000ULL;  // Convert seconds to milliseconds
                bar.open = std::stod(open_str);
                bar.high = std::stod(high_str);
                bar.low = std::stod(low_str);
                bar.close = std::stod(close_str);
                bar.volume = std::stoll(volume_str);
                all_bars.push_back(bar);
            }
        }
        file.close();

        if (all_bars.empty()) {
            log_system("WARNING: No bars loaded from warmup file");
            return;
        }

        log_system("Loaded " + std::to_string(all_bars.size()) + " bars from warmup file");
        log_system("");

        // Feed ALL bars (3900 warmup + today's bars)
        // This ensures we're caught up to the current time
        log_system("=== Starting Warmup Process ===");
        log_system("  Target: 3900 bars (10 blocks @ 390 bars/block)");
        log_system("  Available: " + std::to_string(all_bars.size()) + " bars");
        log_system("");

        int predictor_training_count = 0;
        int feature_engine_ready_bar = 0;
        int strategy_ready_bar = 0;

        for (size_t i = 0; i < all_bars.size(); ++i) {
            strategy_.on_bar(all_bars[i]);

            // Report feature engine ready
            if (i == 64 && feature_engine_ready_bar == 0) {
                feature_engine_ready_bar = i;
                log_system("✓ Feature Engine Warmup Complete (64 bars)");
                log_system("  - All rolling windows initialized");
                log_system("  - Technical indicators ready");
                log_system("  - Starting predictor training...");
                log_system("");
            }

            // Train predictor on bar-to-bar returns (wait for strategy to be fully ready)
            if (strategy_.is_ready() && i + 1 < all_bars.size()) {
                auto features = strategy_.extract_features(all_bars[i]);
                if (!features.empty()) {
                    double current_close = all_bars[i].close;
                    double next_close = all_bars[i + 1].close;
                    double realized_return = (next_close - current_close) / current_close;

                    strategy_.train_predictor(features, realized_return);
                    predictor_training_count++;
                }
            }

            // Report strategy ready
            if (strategy_.is_ready() && strategy_ready_bar == 0) {
                strategy_ready_bar = i;
                log_system("✓ Strategy Warmup Complete (" + std::to_string(i) + " bars)");
                log_system("  - EWRLS predictor fully trained");
                log_system("  - Multi-horizon predictions ready");
                log_system("  - Strategy ready for live trading");
                log_system("");
            }

            // Progress indicator every 1000 bars
            if ((i + 1) % 1000 == 0) {
                log_system("  Progress: " + std::to_string(i + 1) + "/" + std::to_string(all_bars.size()) +
                          " bars (" + std::to_string(predictor_training_count) + " training samples)");
            }

            // Update bar_count_ and previous_bar_ for seamless transition to live
            bar_count_++;
            previous_bar_ = all_bars[i];
        }

        log_system("");
        log_system("=== Warmup Summary ===");
        log_system("✓ Total bars processed: " + std::to_string(all_bars.size()));
        log_system("✓ Feature engine ready: Bar " + std::to_string(feature_engine_ready_bar));
        log_system("✓ Strategy ready: Bar " + std::to_string(strategy_ready_bar));
        log_system("✓ Predictor trained: " + std::to_string(predictor_training_count) + " samples");
        log_system("✓ Last warmup bar: " + format_bar_time(all_bars.back()));
        log_system("✓ Strategy is_ready() = " + std::string(strategy_.is_ready() ? "YES" : "NO"));
        log_system("");
    }

    std::string format_bar_time(const Bar& bar) const {
        time_t time_t_val = static_cast<time_t>(bar.timestamp_ms / 1000);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void on_new_bar(const Bar& bar) {
        bar_count_++;

        // In mock mode, sync time manager to bar timestamp and update market prices
        if (is_mock_mode_) {
            et_time_.set_mock_time(bar.timestamp_ms);

            // Update MockBroker with current market prices
            auto* mock_broker = dynamic_cast<MockBroker*>(broker_.get());
            if (mock_broker) {
                // Update SPY price from bar
                mock_broker->update_market_price("SPY", bar.close);

                // Update leveraged ETF prices from loaded CSV data
                uint64_t bar_ts_sec = bar.timestamp_ms / 1000;

                // CRITICAL: Crash fast if no price data found (no silent fallbacks!)
                if (!leveraged_prices_.count(bar_ts_sec)) {
                    throw std::runtime_error(
                        "CRITICAL: No leveraged ETF price data for timestamp " +
                        std::to_string(bar_ts_sec) + " (bar time: " +
                        get_timestamp_readable() + ")");
                }

                const auto& prices_at_ts = leveraged_prices_[bar_ts_sec];

                // Validate all required symbols have prices
                std::vector<std::string> required_symbols = {"SPXL", "SH", "SDS"};
                for (const auto& symbol : required_symbols) {
                    if (!prices_at_ts.count(symbol)) {
                        throw std::runtime_error(
                            "CRITICAL: Missing price for " + symbol +
                            " at timestamp " + std::to_string(bar_ts_sec));
                    }
                    mock_broker->update_market_price(symbol, prices_at_ts.at(symbol));
                }
            }
        }

        auto timestamp = get_timestamp_readable();

        // Log bar received
        log_system("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        log_system("📊 BAR #" + std::to_string(bar_count_) + " Received from Polygon");
        log_system("  Time: " + timestamp);
        log_system("  OHLC: O=$" + std::to_string(bar.open) + " H=$" + std::to_string(bar.high) +
                  " L=$" + std::to_string(bar.low) + " C=$" + std::to_string(bar.close));
        log_system("  Volume: " + std::to_string(bar.volume));

        // =====================================================================
        // STEP 1: Bar Validation (NEW - P4)
        // =====================================================================
        if (!is_valid_bar(bar)) {
            log_error("❌ Invalid bar dropped: " + BarValidator::get_error_message(bar));
            return;
        }
        log_system("✓ Bar validation passed");

        // =====================================================================
        // STEP 2: Feed to Strategy (ALWAYS - for continuous learning)
        // =====================================================================
        log_system("⚙️  Feeding bar to strategy (updating indicators)...");
        strategy_.on_bar(bar);

        // =====================================================================
        // STEP 3: Continuous Bar-to-Bar Learning (NEW - P1-1 fix)
        // =====================================================================
        if (previous_bar_.has_value()) {
            auto features = strategy_.extract_features(*previous_bar_);
            if (!features.empty()) {
                double return_1bar = (bar.close - previous_bar_->close) /
                                    previous_bar_->close;
                strategy_.train_predictor(features, return_1bar);
                log_system("✓ Predictor updated (learning from previous bar return: " +
                          std::to_string(return_1bar * 100) + "%)");

                // Update micro-adaptation state with bar return
                if (micro_adaptation_.enabled) {
                    micro_adaptation_.update_on_bar(return_1bar);

                    // Log threshold adaptations when they occur
                    if (micro_adaptation_.bars_since_last_threshold_adapt == 1) {
                        log_system("🔧 MICRO-ADAPTATION: Hourly threshold update (hour " +
                                  std::to_string(micro_adaptation_.adaptation_hour) + ")");
                        log_system("   Adapted buy threshold: " +
                                  std::to_string(micro_adaptation_.current_buy_threshold) +
                                  " (drift: " +
                                  std::to_string((micro_adaptation_.current_buy_threshold -
                                                micro_adaptation_.baseline_buy_threshold) * 100) + "%)");
                        log_system("   Adapted sell threshold: " +
                                  std::to_string(micro_adaptation_.current_sell_threshold) +
                                  " (drift: " +
                                  std::to_string((micro_adaptation_.current_sell_threshold -
                                                micro_adaptation_.baseline_sell_threshold) * 100) + "%)");
                    }

                    // Log lambda adaptations when they occur
                    if (micro_adaptation_.bars_since_last_lambda_adapt == 1) {
                        log_system("🔧 MICRO-ADAPTATION: Lambda update");
                        log_system("   Adapted lambda: " +
                                  std::to_string(micro_adaptation_.current_lambda) +
                                  " (baseline: " +
                                  std::to_string(micro_adaptation_.baseline_lambda) + ")");
                    }
                }
            }
        }
        previous_bar_ = bar;

        // =====================================================================
        // STEP 3.5: Increment bars_held counter (CRITICAL for min hold period)
        // =====================================================================
        if (current_state_ != PositionStateMachine::State::CASH_ONLY) {
            bars_held_++;
            log_system("📊 Position holding duration: " + std::to_string(bars_held_) + " bars");
        }

        // =====================================================================
        // STEP 4: Periodic Position Reconciliation (NEW - P0-3)
        // Skip in mock mode - no external broker to drift from
        // =====================================================================
        if (!is_mock_mode_ && bar_count_ % 60 == 0) {  // Every 60 bars (60 minutes)
            try {
                auto broker_positions = get_broker_positions();
                position_book_.reconcile_with_broker(broker_positions);
            } catch (const PositionReconciliationError& e) {
                log_error("[" + timestamp + "] RECONCILIATION FAILED: " +
                         std::string(e.what()));
                log_error("[" + timestamp + "] Initiating emergency flatten");
                liquidate_all_positions();
                throw;  // Exit for supervisor restart
            }
        }

        // =====================================================================
        // STEP 4.5: Persist State (Every 10 bars for low overhead)
        // =====================================================================
        if (bar_count_ % 10 == 0) {
            persist_current_state();
        }

        // =====================================================================
        // STEP 5: Check End-of-Day Liquidation (IDEMPOTENT)
        // =====================================================================
        std::string today_et = timestamp.substr(0, 10);  // Extract YYYY-MM-DD from timestamp

        // Check if today is a trading day
        if (!nyse_calendar_.is_trading_day(today_et)) {
            log_system("⏸️  Holiday/Weekend - no trading (learning continues)");
            return;
        }

        // Idempotent EOD check: only liquidate once per trading day
        if (is_end_of_day_liquidation_time() && !eod_state_.is_eod_complete(today_et)) {
            log_system("🔔 END OF DAY - Liquidation window active");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(today_et);
            log_system("✓ EOD liquidation complete for " + today_et);
            return;
        }

        // =====================================================================
        // STEP 5.5: Mid-Day Optimization at 16:05 PM ET (NEW)
        // =====================================================================
        // Reset optimization flag for new trading day
        if (midday_optimization_date_ != today_et) {
            midday_optimization_done_ = false;
            midday_optimization_date_ = today_et;
            todays_bars_.clear();  // Clear today's bars for new day
        }

        // Collect ALL bars during regular hours (9:30-16:00) for optimization
        if (is_regular_hours()) {
            todays_bars_.push_back(bar);

            // Check if it's 15:15 PM ET and optimization hasn't been done yet
            if (et_time_.is_midday_optimization_time() && !midday_optimization_done_) {
                log_system("🔔 MID-DAY OPTIMIZATION TIME (15:15 PM ET / 3:15pm)");

                // Liquidate all positions before optimization
                log_system("Liquidating all positions before optimization...");
                liquidate_all_positions();
                log_system("✓ Positions liquidated - going 100% cash");

                // Run optimization
                run_midday_optimization();

                // Mark as done
                midday_optimization_done_ = true;

                // Skip trading for this bar (optimization takes time)
                return;
            }
        }

        // =====================================================================
        // STEP 6: Trading Hours Gate (NEW - only trade during RTH, before EOD)
        // =====================================================================
        if (!is_regular_hours()) {
            log_system("⏰ After-hours - learning only, no trading");
            return;  // Learning continues, but no trading
        }

        // CRITICAL: Block trading after EOD liquidation (3:58 PM - 4:00 PM)
        if (et_time_.is_eod_liquidation_window()) {
            log_system("🔴 EOD window active - learning only, no new trades");
            return;  // Learning continues, but no new positions
        }

        log_system("🕐 Regular Trading Hours - processing for signals and trades");

        // =====================================================================
        // STEP 7: Generate Signal and Trade (RTH only)
        // =====================================================================
        log_system("🧠 Generating signal from strategy...");
        auto signal = generate_signal(bar);

        // Log signal with detailed info
        log_system("📈 SIGNAL GENERATED:");
        log_system("  Prediction: " + signal.prediction);
        log_system("  Probability: " + std::to_string(signal.probability));
        log_system("  Confidence: " + std::to_string(signal.probability));
        log_system("  Strategy Ready: " + std::string(strategy_.is_ready() ? "YES" : "NO"));

        log_signal(bar, signal);

        // Make trading decision
        log_system("🎯 Evaluating trading decision...");
        auto decision = make_decision(signal, bar);

        // Enhanced decision logging with detailed explanation
        log_enhanced_decision(signal, decision);
        log_decision(decision);

        // Execute if needed
        if (decision.should_trade) {
            execute_transition(decision);
        } else {
            log_system("⏸️  NO TRADE: " + decision.reason);
        }

        // Log current portfolio state
        log_portfolio_state();
    }

    struct Signal {
        double probability;
        double confidence;
        std::string prediction;  // "LONG", "SHORT", "NEUTRAL"
        double prob_1bar;
        double prob_5bar;
        double prob_10bar;
    };

    Signal generate_signal(const Bar& bar) {
        // Call OnlineEnsemble strategy to generate real signal
        auto strategy_signal = strategy_.generate_signal(bar);

        // DEBUG: Check why we're getting 0.5
        if (strategy_signal.probability == 0.5) {
            std::string reason = "unknown";
            if (strategy_signal.metadata.count("skip_reason")) {
                reason = strategy_signal.metadata.at("skip_reason");
            }
            std::cout << "  [DBG: p=0.5 reason=" << reason << "]" << std::endl;
        }

        Signal signal;
        signal.probability = strategy_signal.probability;
        signal.probability = strategy_signal.probability;  // Use confidence from strategy

        // Map signal type to prediction string
        if (strategy_signal.signal_type == SignalType::LONG) {
            signal.prediction = "LONG";
        } else if (strategy_signal.signal_type == SignalType::SHORT) {
            signal.prediction = "SHORT";
        } else {
            signal.prediction = "NEUTRAL";
        }

        // Use same probability for all horizons (OnlineEnsemble provides single probability)
        signal.prob_1bar = strategy_signal.probability;
        signal.prob_5bar = strategy_signal.probability;
        signal.prob_10bar = strategy_signal.probability;

        return signal;
    }

    struct Decision {
        bool should_trade;
        PositionStateMachine::State target_state;
        std::string reason;
        double current_equity;
        double position_pnl_pct;
        bool profit_target_hit;
        bool stop_loss_hit;
        bool min_hold_violated;
    };

    Decision make_decision(const Signal& signal, const Bar& bar) {
        Decision decision;
        decision.should_trade = false;

        // Get current portfolio state
        auto account = broker_->get_account();
        if (!account) {
            decision.reason = "Failed to get account info";
            return decision;
        }

        decision.current_equity = account->portfolio_value;
        decision.position_pnl_pct = (decision.current_equity - entry_equity_) / entry_equity_;

        // Check profit target / stop loss
        decision.profit_target_hit = (decision.position_pnl_pct >= PROFIT_TARGET &&
                                      current_state_ != PositionStateMachine::State::CASH_ONLY);
        decision.stop_loss_hit = (decision.position_pnl_pct <= STOP_LOSS &&
                                  current_state_ != PositionStateMachine::State::CASH_ONLY);

        // Check minimum hold period
        decision.min_hold_violated = (bars_held_ < MIN_HOLD_BARS);

        // Force exit to cash if profit/stop hit
        if (decision.profit_target_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "PROFIT_TARGET (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        if (decision.stop_loss_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "STOP_LOSS (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        // Map signal probability to PSM state (v1.0 asymmetric thresholds)
        // Use micro-adapted thresholds if enabled, otherwise use defaults
        double buy_threshold = micro_adaptation_.enabled ? micro_adaptation_.current_buy_threshold : 0.55;
        double sell_threshold = micro_adaptation_.enabled ? micro_adaptation_.current_sell_threshold : 0.45;

        // Calculate derived thresholds (maintain relative spacing)
        double ultra_bull_threshold = buy_threshold + 0.13;  // 0.55 + 0.13 = 0.68
        double mixed_bull_threshold = buy_threshold + 0.05;  // 0.55 + 0.05 = 0.60
        double ultra_bear_threshold = sell_threshold - 0.10; // 0.45 - 0.10 = 0.35
        double mixed_bear_threshold = sell_threshold - 0.13; // 0.45 - 0.13 = 0.32
        double cash_high = 0.49;  // Fixed narrow band around 0.5

        PositionStateMachine::State target_state;

        if (signal.probability >= ultra_bull_threshold) {
            target_state = PositionStateMachine::State::TQQQ_ONLY;  // Maps to SPXL
        } else if (signal.probability >= mixed_bull_threshold) {
            target_state = PositionStateMachine::State::QQQ_TQQQ;   // Mixed
        } else if (signal.probability >= buy_threshold) {
            target_state = PositionStateMachine::State::QQQ_ONLY;   // Maps to SPY
        } else if (signal.probability >= cash_high) {
            target_state = PositionStateMachine::State::CASH_ONLY;
        } else if (signal.probability >= sell_threshold) {
            target_state = PositionStateMachine::State::PSQ_ONLY;   // Maps to SH
        } else if (signal.probability >= ultra_bear_threshold) {
            target_state = PositionStateMachine::State::PSQ_SQQQ;   // Mixed
        } else if (signal.probability < mixed_bear_threshold) {
            target_state = PositionStateMachine::State::SQQQ_ONLY;  // Maps to SDS
        } else {
            target_state = PositionStateMachine::State::CASH_ONLY;
        }

        decision.target_state = target_state;

        // Check if state transition needed
        if (target_state != current_state_) {
            // Check minimum hold period
            if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
                decision.should_trade = false;
                decision.reason = "MIN_HOLD_PERIOD (held " + std::to_string(bars_held_) + " bars)";
            } else {
                decision.should_trade = true;
                decision.reason = "STATE_TRANSITION (prob=" + std::to_string(signal.probability) + ")";
            }
        } else {
            decision.should_trade = false;
            decision.reason = "NO_CHANGE";
        }

        return decision;
    }

    void liquidate_all_positions() {
        log_system("Closing all positions for end of day...");

        if (broker_->close_all_positions()) {
            log_system("✓ All positions closed");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;

            auto account = broker_->get_account();
            if (account) {
                log_system("Final portfolio value: $" + std::to_string(account->portfolio_value));
                entry_equity_ = account->portfolio_value;
            }
        } else {
            log_error("Failed to close all positions");
        }

        log_portfolio_state();
    }

    void execute_transition(const Decision& decision) {
        log_system("");
        log_system("🚀 *** EXECUTING TRADE ***");
        log_system("  Current State: " + psm_.state_to_string(current_state_));
        log_system("  Target State: " + psm_.state_to_string(decision.target_state));
        log_system("  Reason: " + decision.reason);
        log_system("");

        // Step 1: Close all current positions
        log_system("📤 Step 1: Closing current positions...");

        // Get current positions before closing (for logging)
        auto positions_to_close = broker_->get_positions();

        if (!broker_->close_all_positions()) {
            log_error("❌ Failed to close positions - aborting transition");
            return;
        }

        // Get account info before closing for accurate P&L calculation
        auto account_before = broker_->get_account();
        double portfolio_before = account_before ? account_before->portfolio_value : previous_portfolio_value_;

        // Log the close orders
        if (!positions_to_close.empty()) {
            for (const auto& pos : positions_to_close) {
                if (std::abs(pos.qty) >= 0.001) {
                    // Create a synthetic Order object for logging
                    Order close_order;
                    close_order.symbol = pos.symbol;
                    close_order.quantity = -pos.qty;  // Negative to close
                    close_order.side = (pos.qty > 0) ? "sell" : "buy";
                    close_order.type = "market";
                    close_order.time_in_force = "gtc";
                    close_order.order_id = "CLOSE-" + pos.symbol;
                    close_order.status = "filled";
                    close_order.filled_qty = std::abs(pos.qty);
                    close_order.filled_avg_price = pos.current_price;

                    // Calculate realized P&L for this close
                    double trade_pnl = (pos.qty > 0) ?
                        pos.qty * (pos.current_price - pos.avg_entry_price) :  // Long close
                        pos.qty * (pos.avg_entry_price - pos.current_price);   // Short close

                    // Get updated account info
                    auto account_after = broker_->get_account();
                    double cash = account_after ? account_after->cash : 0.0;
                    double portfolio = account_after ? account_after->portfolio_value : portfolio_before;

                    log_trade(close_order, bar_count_, cash, portfolio, trade_pnl, "Close position");
                    log_system("  🔴 CLOSE " + std::to_string(std::abs(pos.qty)) + " " + pos.symbol +
                              " (P&L: $" + std::to_string(trade_pnl) + ")");

                    previous_portfolio_value_ = portfolio;
                }
            }
        }

        log_system("✓ All positions closed");

        // Wait a moment for orders to settle (only in live mode)
        // In mock mode, skip sleep to avoid deadlock with replay thread
        if (!is_mock_mode_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        // Step 2: Get current account info
        log_system("💰 Step 2: Fetching account balance from Alpaca...");
        auto account = broker_->get_account();
        if (!account) {
            log_error("❌ Failed to get account info - aborting transition");
            return;
        }

        double available_capital = account->cash;
        double portfolio_value = account->portfolio_value;
        log_system("✓ Account Status:");
        log_system("  Cash: $" + std::to_string(available_capital));
        log_system("  Portfolio Value: $" + std::to_string(portfolio_value));
        log_system("  Buying Power: $" + std::to_string(account->buying_power));

        // Step 3: Calculate target positions based on PSM state
        auto target_positions = calculate_target_allocations(decision.target_state, available_capital);

        // CRITICAL: If target is not CASH_ONLY but we got empty positions, something is wrong
        bool position_entry_failed = false;
        if (target_positions.empty() && decision.target_state != PositionStateMachine::State::CASH_ONLY) {
            log_error("❌ CRITICAL: Target state is " + psm_.state_to_string(decision.target_state) +
                     " but failed to calculate positions (likely price fetch failure)");
            log_error("   Staying in CASH_ONLY for safety");
            position_entry_failed = true;
        }

        // Step 4: Execute buy orders for target positions
        if (!target_positions.empty()) {
            log_system("");
            log_system("📥 Step 3: Opening new positions...");
            for (const auto& [symbol, quantity] : target_positions) {
                if (quantity > 0) {
                    log_system("  🔵 Sending BUY order to Alpaca:");
                    log_system("     Symbol: " + symbol);
                    log_system("     Quantity: " + std::to_string(quantity) + " shares");

                    auto order = broker_->place_market_order(symbol, quantity, "gtc");
                    if (order) {
                        log_system("  ✓ Order Confirmed:");
                        log_system("     Order ID: " + order->order_id);
                        log_system("     Status: " + order->status);

                        // Get updated account info for accurate logging
                        auto account_after = broker_->get_account();
                        double cash = account_after ? account_after->cash : 0.0;
                        double portfolio = account_after ? account_after->portfolio_value : previous_portfolio_value_;
                        double trade_pnl = portfolio - previous_portfolio_value_;  // Portfolio change from this trade

                        // Build reason string from decision
                        std::string reason = "Enter " + psm_.state_to_string(decision.target_state);
                        if (decision.profit_target_hit) reason += " (profit target)";
                        else if (decision.stop_loss_hit) reason += " (stop loss)";

                        log_trade(*order, bar_count_, cash, portfolio, trade_pnl, reason);
                        previous_portfolio_value_ = portfolio;
                    } else {
                        log_error("  ❌ Failed to place order for " + symbol);
                    }

                    // Small delay between orders
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
        } else {
            log_system("💵 Target state is CASH_ONLY - no positions to open");
        }

        // Update state - CRITICAL FIX: Only update to target state if we successfully entered positions
        // or if target was CASH_ONLY
        if (position_entry_failed) {
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            log_system("⚠️  State forced to CASH_ONLY due to position entry failure");
        } else {
            current_state_ = decision.target_state;
        }
        bars_held_ = 0;
        entry_equity_ = decision.current_equity;

        // Final account status
        log_system("");
        log_system("✓ Transition Complete!");
        log_system("  New State: " + psm_.state_to_string(current_state_));
        log_system("  Entry Equity: $" + std::to_string(entry_equity_));
        log_system("");

        // Persist state immediately after transition
        persist_current_state();
    }

    // Calculate position allocations based on PSM state
    std::map<std::string, double> calculate_target_allocations(
        PositionStateMachine::State state, double capital) {

        std::map<std::string, double> allocations;

        // Map PSM states to SPY instrument allocations
        switch (state) {
            case PositionStateMachine::State::TQQQ_ONLY:
                // 3x bull → SPXL only
                allocations["SPXL"] = capital;
                break;

            case PositionStateMachine::State::QQQ_TQQQ:
                // Blended long → SPY (50%) + SPXL (50%)
                allocations["SPY"] = capital * 0.5;
                allocations["SPXL"] = capital * 0.5;
                break;

            case PositionStateMachine::State::QQQ_ONLY:
                // 1x base → SPY only
                allocations["SPY"] = capital;
                break;

            case PositionStateMachine::State::CASH_ONLY:
                // No positions
                break;

            case PositionStateMachine::State::PSQ_ONLY:
                // -1x bear → SH only
                allocations["SH"] = capital;
                break;

            case PositionStateMachine::State::PSQ_SQQQ:
                // Blended short → SH (50%) + SDS (50%)
                allocations["SH"] = capital * 0.5;
                allocations["SDS"] = capital * 0.5;
                break;

            case PositionStateMachine::State::SQQQ_ONLY:
                // -2x bear → SDS only
                allocations["SDS"] = capital;
                break;

            default:
                break;
        }

        // Convert dollar allocations to share quantities
        std::map<std::string, double> quantities;
        for (const auto& [symbol, dollar_amount] : allocations) {
            double price = 0.0;

            // In mock mode, use leveraged_prices_ for SH, SDS, SPXL
            if (is_mock_mode_ && (symbol == "SH" || symbol == "SDS" || symbol == "SPXL")) {
                // Get current bar timestamp
                auto spy_bars = bar_feed_->get_recent_bars("SPY", 1);
                if (spy_bars.empty()) {
                    throw std::runtime_error("CRITICAL: No SPY bars available for timestamp lookup");
                }

                uint64_t bar_ts_sec = spy_bars[0].timestamp_ms / 1000;

                // Crash fast if no price data (no silent failures!)
                if (!leveraged_prices_.count(bar_ts_sec)) {
                    throw std::runtime_error(
                        "CRITICAL: No leveraged ETF price data for timestamp " +
                        std::to_string(bar_ts_sec) + " when calculating " + symbol + " position");
                }

                if (!leveraged_prices_[bar_ts_sec].count(symbol)) {
                    throw std::runtime_error(
                        "CRITICAL: No price for " + symbol + " at timestamp " +
                        std::to_string(bar_ts_sec));
                }

                price = leveraged_prices_[bar_ts_sec].at(symbol);
            } else {
                // Get price from bar feed (SPY or live mode)
                auto bars = bar_feed_->get_recent_bars(symbol, 1);
                if (bars.empty() || bars[0].close <= 0) {
                    throw std::runtime_error(
                        "CRITICAL: No valid price for " + symbol + " from bar feed");
                }
                price = bars[0].close;
            }

            // Calculate shares
            if (price <= 0) {
                throw std::runtime_error(
                    "CRITICAL: Invalid price " + std::to_string(price) + " for " + symbol);
            }

            double shares = std::floor(dollar_amount / price);
            if (shares > 0) {
                quantities[symbol] = shares;
            }
        }

        return quantities;
    }

    void log_trade(const Order& order, uint64_t bar_index = 0, double cash_balance = 0.0,
                   double portfolio_value = 0.0, double trade_pnl = 0.0, const std::string& reason = "") {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["bar_index"] = bar_index;
        j["order_id"] = order.order_id;
        j["symbol"] = order.symbol;
        j["side"] = order.side;
        j["quantity"] = order.quantity;
        j["type"] = order.type;
        j["time_in_force"] = order.time_in_force;
        j["status"] = order.status;
        j["filled_qty"] = order.filled_qty;
        j["filled_avg_price"] = order.filled_avg_price;
        j["cash_balance"] = cash_balance;
        j["portfolio_value"] = portfolio_value;
        j["trade_pnl"] = trade_pnl;
        if (!reason.empty()) {
            j["reason"] = reason;
        }

        log_trades_ << j.dump() << std::endl;
        log_trades_.flush();
    }

    void log_signal(const Bar& bar, const Signal& signal) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["bar_timestamp_ms"] = bar.timestamp_ms;
        j["probability"] = signal.probability;
        j["confidence"] = signal.probability;
        j["prediction"] = signal.prediction;
        j["prob_1bar"] = signal.prob_1bar;
        j["prob_5bar"] = signal.prob_5bar;
        j["prob_10bar"] = signal.prob_10bar;

        log_signals_ << j.dump() << std::endl;
        log_signals_.flush();
    }

    void log_enhanced_decision(const Signal& signal, const Decision& decision) {
        log_system("");
        log_system("╔═══════════════════════════════════════════════════════════════");
        log_system("║ 📋 DECISION ANALYSIS");
        log_system("╠═══════════════════════════════════════════════════════════════");

        // Current state
        log_system("║ Current State: " + psm_.state_to_string(current_state_));
        log_system("║   - Bars Held: " + std::to_string(bars_held_) + " bars");
        log_system("║   - Min Hold: " + std::to_string(MIN_HOLD_BARS) + " bars required");
        log_system("║   - Position P&L: " + std::to_string(decision.position_pnl_pct * 100) + "%");
        log_system("║   - Current Equity: $" + std::to_string(decision.current_equity));
        log_system("║");

        // Signal analysis
        log_system("║ Signal Input:");
        log_system("║   - Probability: " + std::to_string(signal.probability));
        log_system("║   - Prediction: " + signal.prediction);
        log_system("║   - Confidence: " + std::to_string(signal.probability));
        log_system("║");

        // Target state mapping
        log_system("║ PSM Threshold Mapping:");
        if (signal.probability >= 0.68) {
            log_system("║   ✓ prob >= 0.68 → BULL_3X_ONLY (SPXL)");
        } else if (signal.probability >= 0.60) {
            log_system("║   ✓ 0.60 <= prob < 0.68 → BASE_BULL_3X (SPY+SPXL)");
        } else if (signal.probability >= 0.55) {
            log_system("║   ✓ 0.55 <= prob < 0.60 → BASE_ONLY (SPY)");
        } else if (signal.probability >= 0.49) {
            log_system("║   ✓ 0.49 <= prob < 0.55 → CASH_ONLY");
        } else if (signal.probability >= 0.45) {
            log_system("║   ✓ 0.45 <= prob < 0.49 → BEAR_1X_ONLY (SH)");
        } else if (signal.probability >= 0.35) {
            log_system("║   ✓ 0.35 <= prob < 0.45 → BEAR_1X_NX (SH+SDS)");
        } else {
            log_system("║   ✓ prob < 0.35 → BEAR_NX_ONLY (SDS)");
        }
        log_system("║   → Target State: " + psm_.state_to_string(decision.target_state));
        log_system("║");

        // Decision logic
        log_system("║ Decision Logic:");
        if (decision.profit_target_hit) {
            log_system("║   🎯 PROFIT TARGET HIT (" + std::to_string(decision.position_pnl_pct * 100) + "%)");
            log_system("║   → Force exit to CASH");
        } else if (decision.stop_loss_hit) {
            log_system("║   🛑 STOP LOSS HIT (" + std::to_string(decision.position_pnl_pct * 100) + "%)");
            log_system("║   → Force exit to CASH");
        } else if (decision.target_state == current_state_) {
            log_system("║   ✓ Target matches current state");
            log_system("║   → NO CHANGE (hold position)");
        } else if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
            log_system("║   ⏳ MIN HOLD PERIOD VIOLATED");
            log_system("║      - Currently held: " + std::to_string(bars_held_) + " bars");
            log_system("║      - Required: " + std::to_string(MIN_HOLD_BARS) + " bars");
            log_system("║      - Remaining: " + std::to_string(MIN_HOLD_BARS - bars_held_) + " bars");
            log_system("║   → BLOCKED (must wait)");
        } else {
            log_system("║   ✓ State transition approved");
            log_system("║      - Target differs from current");
            log_system("║      - Min hold satisfied or in CASH");
            log_system("║   → EXECUTE TRANSITION");
        }
        log_system("║");

        // Final decision
        if (decision.should_trade) {
            log_system("║ ✅ FINAL DECISION: TRADE");
            log_system("║    Transition: " + psm_.state_to_string(current_state_) +
                      " → " + psm_.state_to_string(decision.target_state));
        } else {
            log_system("║ ⏸️  FINAL DECISION: NO TRADE");
        }
        log_system("║    Reason: " + decision.reason);
        log_system("╚═══════════════════════════════════════════════════════════════");
        log_system("");
    }

    void log_decision(const Decision& decision) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["should_trade"] = decision.should_trade;
        j["current_state"] = psm_.state_to_string(current_state_);
        j["target_state"] = psm_.state_to_string(decision.target_state);
        j["reason"] = decision.reason;
        j["current_equity"] = decision.current_equity;
        j["position_pnl_pct"] = decision.position_pnl_pct;
        j["bars_held"] = bars_held_;

        log_decisions_ << j.dump() << std::endl;
        log_decisions_.flush();
    }

    void log_portfolio_state() {
        auto account = broker_->get_account();
        if (!account) return;

        auto positions = broker_->get_positions();

        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["cash"] = account->cash;
        j["buying_power"] = account->buying_power;
        j["portfolio_value"] = account->portfolio_value;
        j["equity"] = account->equity;
        j["total_return"] = account->portfolio_value - 100000.0;
        j["total_return_pct"] = (account->portfolio_value - 100000.0) / 100000.0;

        nlohmann::json positions_json = nlohmann::json::array();
        for (const auto& pos : positions) {
            nlohmann::json p;
            p["symbol"] = pos.symbol;
            p["quantity"] = pos.qty;
            p["avg_entry_price"] = pos.avg_entry_price;
            p["current_price"] = pos.current_price;
            p["market_value"] = pos.qty * pos.current_price;
            p["unrealized_pl"] = pos.unrealized_pnl;
            p["unrealized_pl_pct"] = pos.unrealized_pnl;
            positions_json.push_back(p);
        }
        j["positions"] = positions_json;

        log_positions_ << j.dump() << std::endl;
        log_positions_.flush();
    }

    // NEW: Convert Alpaca positions to BrokerPosition format for reconciliation
    std::vector<BrokerPosition> get_broker_positions() {
        auto alpaca_positions = broker_->get_positions();
        std::vector<BrokerPosition> broker_positions;

        for (const auto& pos : alpaca_positions) {
            BrokerPosition bp;
            bp.symbol = pos.symbol;
            bp.qty = static_cast<int64_t>(pos.qty);
            bp.avg_entry_price = pos.avg_entry_price;
            bp.current_price = pos.current_price;
            bp.unrealized_pnl = pos.unrealized_pnl;
            broker_positions.push_back(bp);
        }

        return broker_positions;
    }

    /**
     * Save comprehensive warmup data: historical bars + all of today's bars
     * This ensures optimization uses ALL available data up to current moment
     */
    std::string save_comprehensive_warmup_to_csv() {
        auto et_tm = et_time_.get_current_et_tm();
        std::string today = format_et_date(et_tm);

        std::string filename = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/comprehensive_warmup_" +
                               today + ".csv";

        std::ofstream csv(filename);
        if (!csv.is_open()) {
            log_error("Failed to open file for writing: " + filename);
            return "";
        }

        // Write CSV header
        csv << "timestamp,open,high,low,close,volume\n";

        log_system("Building comprehensive warmup data...");

        // Step 1: Load historical warmup bars (20 blocks = 7800 bars + 64 feature bars)
        std::string warmup_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_warmup_latest.csv";
        std::ifstream warmup_csv(warmup_file);

        if (!warmup_csv.is_open()) {
            log_error("Failed to open historical warmup file: " + warmup_file);
            log_error("Falling back to today's bars only");
        } else {
            std::string line;
            std::getline(warmup_csv, line);  // Skip header

            int historical_count = 0;
            while (std::getline(warmup_csv, line)) {
                // Filter: only include bars BEFORE today (to avoid duplicates)
                if (line.find(today) == std::string::npos) {
                    csv << line << "\n";
                    historical_count++;
                }
            }
            warmup_csv.close();

            log_system("  ✓ Historical bars: " + std::to_string(historical_count));
        }

        // Step 2: Append all of today's bars collected so far
        for (const auto& bar : todays_bars_) {
            csv << bar.timestamp_ms << ","
                << bar.open << ","
                << bar.high << ","
                << bar.low << ","
                << bar.close << ","
                << bar.volume << "\n";
        }

        csv.close();

        log_system("  ✓ Today's bars: " + std::to_string(todays_bars_.size()));
        log_system("✓ Comprehensive warmup saved: " + filename);

        return filename;
    }

    /**
     * Load optimized parameters from midday_selected_params.json
     */
    struct OptimizedParams {
        bool success{false};
        std::string source;
        // Phase 1 parameters
        double buy_threshold{0.55};
        double sell_threshold{0.45};
        double bb_amplification_factor{0.10};
        double ewrls_lambda{0.995};
        // Phase 2 parameters
        double h1_weight{0.3};
        double h5_weight{0.5};
        double h10_weight{0.2};
        int bb_period{20};
        double bb_std_dev{2.0};
        double bb_proximity{0.30};
        double regularization{0.01};
        double expected_mrb{0.0};
    };

    OptimizedParams load_optimized_parameters() {
        OptimizedParams params;

        std::string json_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/midday_selected_params.json";
        std::ifstream file(json_file);

        if (!file.is_open()) {
            log_error("Failed to open optimization results: " + json_file);
            return params;
        }

        try {
            nlohmann::json j;
            file >> j;
            file.close();

            params.success = true;
            params.source = j.value("source", "baseline");
            // Phase 1 parameters
            params.buy_threshold = j.value("buy_threshold", 0.55);
            params.sell_threshold = j.value("sell_threshold", 0.45);
            params.bb_amplification_factor = j.value("bb_amplification_factor", 0.10);
            params.ewrls_lambda = j.value("ewrls_lambda", 0.995);
            // Phase 2 parameters
            params.h1_weight = j.value("h1_weight", 0.3);
            params.h5_weight = j.value("h5_weight", 0.5);
            params.h10_weight = j.value("h10_weight", 0.2);
            params.bb_period = j.value("bb_period", 20);
            params.bb_std_dev = j.value("bb_std_dev", 2.0);
            params.bb_proximity = j.value("bb_proximity", 0.30);
            params.regularization = j.value("regularization", 0.01);
            params.expected_mrb = j.value("expected_mrb", 0.0);

            log_system("✓ Loaded optimized parameters from: " + json_file);
            log_system("  Source: " + params.source);
            log_system("  Phase 1 Parameters:");
            log_system("    buy_threshold: " + std::to_string(params.buy_threshold));
            log_system("    sell_threshold: " + std::to_string(params.sell_threshold));
            log_system("    bb_amplification_factor: " + std::to_string(params.bb_amplification_factor));
            log_system("    ewrls_lambda: " + std::to_string(params.ewrls_lambda));
            log_system("  Phase 2 Parameters:");
            log_system("    h1_weight: " + std::to_string(params.h1_weight));
            log_system("    h5_weight: " + std::to_string(params.h5_weight));
            log_system("    h10_weight: " + std::to_string(params.h10_weight));
            log_system("    bb_period: " + std::to_string(params.bb_period));
            log_system("    bb_std_dev: " + std::to_string(params.bb_std_dev));
            log_system("    bb_proximity: " + std::to_string(params.bb_proximity));
            log_system("    regularization: " + std::to_string(params.regularization));
            log_system("  Expected MRB: " + std::to_string(params.expected_mrb) + "%");

        } catch (const std::exception& e) {
            log_error("Failed to parse optimization results: " + std::string(e.what()));
            params.success = false;
        }

        return params;
    }

    /**
     * Update strategy configuration with new parameters
     */
    void update_strategy_parameters(const OptimizedParams& params) {
        log_system("📊 Updating strategy parameters...");

        // Create new config with optimized parameters
        auto config = create_v1_config();
        // Phase 1 parameters
        config.buy_threshold = params.buy_threshold;
        config.sell_threshold = params.sell_threshold;
        config.bb_amplification_factor = params.bb_amplification_factor;
        config.ewrls_lambda = params.ewrls_lambda;
        // Phase 2 parameters
        config.horizon_weights = {params.h1_weight, params.h5_weight, params.h10_weight};
        config.bb_period = params.bb_period;
        config.bb_std_dev = params.bb_std_dev;
        config.bb_proximity_threshold = params.bb_proximity;
        config.regularization = params.regularization;

        // Update strategy
        strategy_.update_config(config);

        log_system("✓ Strategy parameters updated with phase 1 + phase 2 optimizations");
    }

    /**
     * Run mid-day optimization at 15:15 PM ET (3:15pm)
     */
    void run_midday_optimization() {
        log_system("");
        log_system("════════════════════════════════════════════════");
        log_system("🔄 MID-DAY OPTIMIZATION TRIGGERED (15:15 PM ET / 3:15pm)");
        log_system("════════════════════════════════════════════════");
        log_system("");

        // Step 1: Save comprehensive warmup data (historical + today's bars)
        log_system("Step 1: Saving comprehensive warmup data to CSV...");
        std::string warmup_data_file = save_comprehensive_warmup_to_csv();
        if (warmup_data_file.empty()) {
            log_error("Failed to save warmup data - continuing with baseline parameters");
            return;
        }

        // Step 2: Call optimization script
        log_system("Step 2: Running Optuna optimization script...");
        log_system("  (This will take ~5 minutes for 50 trials)");

        std::string cmd = "/Volumes/ExternalSSD/Dev/C++/online_trader/tools/midday_optuna_relaunch.sh \"" +
                          warmup_data_file + "\" 2>&1 | tail -30";

        int exit_code = system(cmd.c_str());

        if (exit_code != 0) {
            log_error("Optimization script failed (exit code: " + std::to_string(exit_code) + ")");
            log_error("Continuing with baseline parameters");
            return;
        }

        log_system("✓ Optimization script completed");

        // Step 3: Load optimized parameters
        log_system("Step 3: Loading optimized parameters...");
        auto params = load_optimized_parameters();

        if (!params.success) {
            log_error("Failed to load optimized parameters - continuing with baseline");
            return;
        }

        // Step 4: Update strategy configuration
        log_system("Step 4: Updating strategy configuration...");
        update_strategy_parameters(params);

        log_system("");
        log_system("════════════════════════════════════════════════");
        log_system("✅ MID-DAY OPTIMIZATION COMPLETE");
        log_system("════════════════════════════════════════════════");
        log_system("  Parameters: " + params.source);
        log_system("  Expected MRB: " + std::to_string(params.expected_mrb) + "%");
        log_system("  Resuming trading at 14:46 PM ET");
        log_system("════════════════════════════════════════════════");
        log_system("");
    }
};

int LiveTradeCommand::execute(const std::vector<std::string>& args) {
    // Parse command-line flags
    bool is_mock = has_flag(args, "--mock");
    std::string mock_data_file = get_arg(args, "--mock-data", "");
    double mock_speed = std::stod(get_arg(args, "--mock-speed", "39.0"));

    // Log directory
    std::string log_dir = is_mock ? "logs/mock_trading" : "logs/live_trading";

    // Create broker and bar feed based on mode
    std::unique_ptr<IBrokerClient> broker;
    std::unique_ptr<IBarFeed> bar_feed;

    if (is_mock) {
        // ================================================================
        // MOCK MODE - Replay historical data
        // ================================================================
        if (mock_data_file.empty()) {
            std::cerr << "ERROR: --mock-data <file> is required in mock mode\n";
            std::cerr << "Example: sentio_cli live-trade --mock --mock-data /tmp/SPY_yesterday.csv\n";
            return 1;
        }

        std::cout << "🎭 MOCK MODE ENABLED\n";
        std::cout << "  Data file: " << mock_data_file << "\n";
        std::cout << "  Speed: " << mock_speed << "x real-time\n";
        std::cout << "  Logs: " << log_dir << "/\n";
        std::cout << "\n";

        // Create mock broker
        auto mock_broker = std::make_unique<MockBroker>(
            100000.0,  // initial_capital
            0.0        // commission_per_share (zero for testing)
        );
        mock_broker->set_fill_behavior(FillBehavior::IMMEDIATE_FULL);
        broker = std::move(mock_broker);

        // Create mock bar feed
        bar_feed = std::make_unique<MockBarFeedReplay>(
            mock_data_file,
            mock_speed
        );

    } else {
        // ================================================================
        // LIVE MODE - Real trading with Alpaca + Polygon
        // ================================================================

        // Read Alpaca credentials from environment
        const char* alpaca_key_env = std::getenv("ALPACA_PAPER_API_KEY");
        const char* alpaca_secret_env = std::getenv("ALPACA_PAPER_SECRET_KEY");

        if (!alpaca_key_env || !alpaca_secret_env) {
            std::cerr << "ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set\n";
            std::cerr << "Run: source config.env\n";
            return 1;
        }

        const std::string ALPACA_KEY = alpaca_key_env;
        const std::string ALPACA_SECRET = alpaca_secret_env;

        // Polygon API key
        const char* polygon_key_env = std::getenv("POLYGON_API_KEY");
        const std::string ALPACA_MARKET_DATA_URL = "wss://stream.data.alpaca.markets/v2/iex";
        const std::string POLYGON_KEY = polygon_key_env ? polygon_key_env : "";

        std::cout << "📈 LIVE MODE ENABLED\n";
        std::cout << "  Account: " << ALPACA_KEY.substr(0, 8) << "...\n";
        std::cout << "  Data source: Alpaca WebSocket (via Python bridge)\n";
        std::cout << "  Logs: " << log_dir << "/\n";
        std::cout << "\n";

        // Create live broker adapter
        broker = std::make_unique<AlpacaClientAdapter>(ALPACA_KEY, ALPACA_SECRET, true /* paper */);

        // Create live bar feed adapter (WebSocket via FIFO)
        bar_feed = std::make_unique<PolygonClientAdapter>(ALPACA_MARKET_DATA_URL, POLYGON_KEY);
    }

    // Create and run trader (same code path for both modes!)
    LiveTrader trader(std::move(broker), std::move(bar_feed), log_dir, is_mock, mock_data_file);
    trader.run();

    return 0;
}

void LiveTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli live-trade [options]\n\n";
    std::cout << "Run OnlineTrader v1.0 in live or mock mode\n\n";
    std::cout << "Options:\n";
    std::cout << "  --mock              Enable mock trading mode (replay historical data)\n";
    std::cout << "  --mock-data <file>  CSV file to replay (required with --mock)\n";
    std::cout << "  --mock-speed <x>    Replay speed multiplier (default: 39.0)\n\n";
    std::cout << "Trading Configuration:\n";
    std::cout << "  Instruments: SPY, SPXL (3x), SH (-1x), SDS (-2x)\n";
    std::cout << "  Hours: 9:30am - 3:58pm ET (regular hours only)\n";
    std::cout << "  Strategy: OnlineEnsemble v1.0 with asymmetric thresholds\n";
    std::cout << "  Warmup: 7,864 bars (20 blocks + 64 feature bars)\n\n";
    std::cout << "Logs:\n";
    std::cout << "  Live:  logs/live_trading/\n";
    std::cout << "  Mock:  logs/mock_trading/\n";
    std::cout << "  Files: system_*.log, signals_*.jsonl, trades_*.jsonl, decisions_*.jsonl\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Live trading\n";
    std::cout << "  sentio_cli live-trade\n\n";
    std::cout << "  # Mock trading (replay yesterday)\n";
    std::cout << "  tail -391 data/equities/SPY_RTH_NH.csv > /tmp/SPY_yesterday.csv\n";
    std::cout << "  sentio_cli live-trade --mock --mock-data /tmp/SPY_yesterday.csv\n\n";
    std::cout << "  # Mock trading at different speed\n";
    std::cout << "  sentio_cli live-trade --mock --mock-data yesterday.csv --mock-speed 100.0\n";
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 39 of 40**: tools/adaptive_optuna.py

**File Information**:
- **Path**: `tools/adaptive_optuna.py`
- **Size**: 772 lines
- **Modified**: 2025-10-09 15:15:22
- **Type**: py
- **Permissions**: -rwxr-xr-x

```text
#!/usr/bin/env python3
"""
Adaptive Optuna Framework for OnlineEnsemble Strategy

Implements three adaptive strategies for parameter optimization:
- Strategy A: Per-block adaptive (retune every block)
- Strategy B: 4-hour adaptive (retune twice daily)
- Strategy C: Static baseline (tune once, deploy fixed)

Author: Claude Code
Date: 2025-10-08
"""

import os
import sys
import json
import time
import subprocess
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from datetime import datetime

import optuna
import pandas as pd
import numpy as np


class AdaptiveOptunaFramework:
    """Framework for adaptive parameter optimization experiments."""

    def __init__(self, data_file: str, build_dir: str, output_dir: str, use_cache: bool = False, n_trials: int = 50, n_jobs: int = 4):  # DEPRECATED: No speedup
        self.data_file = data_file
        self.build_dir = build_dir
        self.output_dir = output_dir
        self.sentio_cli = os.path.join(build_dir, "sentio_cli")
        self.use_cache = use_cache
        self.n_trials = n_trials
        self.n_jobs = n_jobs

        # Create output directory
        Path(output_dir).mkdir(parents=True, exist_ok=True)

        # Load data to determine block structure
        self.df = pd.read_csv(data_file)
        self.total_bars = len(self.df)
        self.bars_per_block = 391  # 391 bars = 1 complete trading day (9:30 AM - 4:00 PM, inclusive)
        self.total_blocks = self.total_bars // self.bars_per_block

        print(f"[AdaptiveOptuna] Loaded {self.total_bars} bars")
        print(f"[AdaptiveOptuna] Total blocks: {self.total_blocks}")
        print(f"[AdaptiveOptuna] Bars per block: {self.bars_per_block}")
        print(f"[AdaptiveOptuna] Optuna trials: {self.n_trials}")
        print(f"[AdaptiveOptuna] Parallel jobs: {self.n_jobs}")

        # Feature caching for speedup (4-5x faster)
        self.features_cache = {}  # Maps data_file -> features_file
        if self.use_cache:
            print(f"[FeatureCache] Feature caching ENABLED (expect 4-5x speedup)")
        else:
            print(f"[FeatureCache] Feature caching DISABLED")

    def create_block_data(self, block_start: int, block_end: int,
                          output_file: str) -> str:
        """
        Extract specific blocks from data and save to CSV.

        Args:
            block_start: Starting block index (inclusive)
            block_end: Ending block index (exclusive)
            output_file: Path to save extracted data

        Returns:
            Path to created CSV file
        """
        start_bar = block_start * self.bars_per_block
        end_bar = block_end * self.bars_per_block

        # Extract bars with header
        block_df = self.df.iloc[start_bar:end_bar]

        # Extract symbol from original data_file and add to output filename
        # This ensures analyze-trades can detect the symbol
        import re
        symbol_match = re.search(r'(SPY|QQQ)', self.data_file, re.IGNORECASE)
        if symbol_match:
            symbol = symbol_match.group(1).upper()
            # Insert symbol before .csv extension
            output_file = output_file.replace('.csv', f'_{symbol}.csv')

        block_df.to_csv(output_file, index=False)

        print(f"[BlockData] Created {output_file}: blocks {block_start}-{block_end-1} "
              f"({len(block_df)} bars)")

        return output_file

    def extract_features_cached(self, data_file: str) -> str:
        """
        Extract features from data file and cache the result.

        Returns path to cached features CSV. If already extracted, returns cached path.
        This provides 4-5x speedup by avoiding redundant feature calculations.
        """
        if not self.use_cache:
            return None  # No caching, generate-signals will extract on-the-fly

        # Check if already cached
        if data_file in self.features_cache:
            print(f"[FeatureCache] Using existing cache for {os.path.basename(data_file)}")
            return self.features_cache[data_file]

        # Generate features file path
        features_file = data_file.replace('.csv', '_features.csv')

        # Check if features file already exists
        if os.path.exists(features_file):
            print(f"[FeatureCache] Found existing features: {os.path.basename(features_file)}")
            self.features_cache[data_file] = features_file
            return features_file

        # Extract features (one-time cost)
        print(f"[FeatureCache] Extracting features from {os.path.basename(data_file)}...")
        start_time = time.time()

        cmd = [
            self.sentio_cli, "extract-features",
            "--data", data_file,
            "--output", features_file
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Feature extraction failed: {result.stderr}")
                return None

            elapsed = time.time() - start_time
            print(f"[FeatureCache] Features extracted in {elapsed:.1f}s: {os.path.basename(features_file)}")

            # Cache the result
            self.features_cache[data_file] = features_file
            return features_file

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Feature extraction timed out")
            return None

    def run_backtest(self, data_file: str, params: Dict,
                     warmup_blocks: int = 2) -> Dict:
        """
        Run backtest with given parameters.

        Args:
            data_file: Path to data CSV
            params: Strategy parameters
            warmup_blocks: Number of blocks for warmup

        Returns:
            Dictionary with performance metrics
        """
        # Create temporary files
        signals_file = os.path.join(self.output_dir, "temp_signals.jsonl")
        trades_file = os.path.join(self.output_dir, "temp_trades.jsonl")
        equity_file = os.path.join(self.output_dir, "temp_equity.csv")

        # Calculate warmup bars
        warmup_bars = warmup_blocks * self.bars_per_block

        # Workaround: create symlinks for multi-instrument files expected by execute-trades
        # execute-trades expects SPY_RTH_NH.csv, SPXL_RTH_NH.csv, SH_RTH_NH.csv, SDS_RTH_NH.csv
        # in the same directory as the data file
        import shutil
        data_dir = os.path.dirname(data_file)
        data_basename = os.path.basename(data_file)

        # Detect symbol
        if 'SPY' in data_basename:
            symbol = 'SPY'
            instruments = ['SPY', 'SPXL', 'SH', 'SDS']
        elif 'QQQ' in data_basename:
            symbol = 'QQQ'
            instruments = ['QQQ', 'TQQQ', 'PSQ', 'SQQQ']
        else:
            print(f"[ERROR] Could not detect symbol from {data_basename}")
            return {'mrb': -999.0, 'error': 'unknown_symbol'}

        # Create copies of the data file for each instrument
        for inst in instruments:
            inst_path = os.path.join(data_dir, f"{inst}_RTH_NH.csv")
            if not os.path.exists(inst_path):
                shutil.copy(data_file, inst_path)

        # Extract features (one-time, cached)
        features_file = self.extract_features_cached(data_file)

        # Step 1: Generate signals (with optional feature cache)
        cmd_generate = [
            self.sentio_cli, "generate-signals",
            "--data", data_file,
            "--output", signals_file,
            "--warmup", str(warmup_bars),
            # Phase 1 parameters
            "--buy-threshold", str(params['buy_threshold']),
            "--sell-threshold", str(params['sell_threshold']),
            "--lambda", str(params['ewrls_lambda']),
            "--bb-amp", str(params['bb_amplification_factor'])
        ]

        # Phase 2 parameters (if present)
        if 'h1_weight' in params:
            cmd_generate.extend(["--h1-weight", str(params['h1_weight'])])
        if 'h5_weight' in params:
            cmd_generate.extend(["--h5-weight", str(params['h5_weight'])])
        if 'h10_weight' in params:
            cmd_generate.extend(["--h10-weight", str(params['h10_weight'])])
        if 'bb_period' in params:
            cmd_generate.extend(["--bb-period", str(params['bb_period'])])
        if 'bb_std_dev' in params:
            cmd_generate.extend(["--bb-std-dev", str(params['bb_std_dev'])])
        if 'bb_proximity' in params:
            cmd_generate.extend(["--bb-proximity", str(params['bb_proximity'])])
        if 'regularization' in params:
            cmd_generate.extend(["--regularization", str(params['regularization'])])

        # Add --features flag if caching enabled and features extracted
        if features_file:
            cmd_generate.extend(["--features", features_file])

        try:
            result = subprocess.run(
                cmd_generate,
                capture_output=True,
                text=True,
                timeout=300  # 5-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Signal generation failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Signal generation timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

        # Step 2: Execute trades
        cmd_execute = [
            self.sentio_cli, "execute-trades",
            "--signals", signals_file,
            "--data", data_file,
            "--output", trades_file,
            "--warmup", str(warmup_bars)
        ]

        try:
            result = subprocess.run(
                cmd_execute,
                capture_output=True,
                text=True,
                timeout=60  # 1-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Trade execution failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Trade execution timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

        # Step 3: Analyze performance
        # Calculate number of blocks in the data file for MRB
        num_bars = len(pd.read_csv(data_file))
        num_blocks = num_bars // self.bars_per_block

        cmd_analyze = [
            self.sentio_cli, "analyze-trades",
            "--trades", trades_file,
            "--data", data_file,
            "--output", equity_file,
            "--blocks", str(num_blocks)  # Pass blocks for MRB calculation
        ]

        try:
            result = subprocess.run(
                cmd_analyze,
                capture_output=True,
                text=True,
                timeout=60
            )

            if result.returncode != 0:
                print(f"[ERROR] Analysis failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

            # Parse MRD (Mean Return per Day) from output
            # Look for: "Mean Return per Day (MRD): +0.0025% (20 trading days)"
            mrd = None
            mrb = None

            for line in result.stdout.split('\n'):
                if 'Mean Return per Day' in line and 'MRD' in line:
                    # Extract the percentage value
                    import re
                    match = re.search(r'([+-]?\d+\.\d+)%', line)
                    if match:
                        mrd = float(match.group(1))

                if 'Mean Return per Block' in line and 'MRB' in line:
                    import re
                    match = re.search(r'([+-]?\d+\.\d+)%', line)
                    if match:
                        mrb = float(match.group(1))

            # Primary metric is MRD (for daily reset strategies)
            if mrd is not None:
                return {
                    'mrd': mrd,
                    'mrb': mrb if mrb is not None else 0.0,
                    'trades_file': trades_file,
                    'equity_file': equity_file
                }

            # Fallback: Calculate from equity file
            if os.path.exists(equity_file):
                equity_df = pd.read_csv(equity_file)
                if len(equity_df) > 0:
                    # Calculate MRB manually
                    total_return = (equity_df['equity'].iloc[-1] - 100000) / 100000
                    num_blocks = len(equity_df) // self.bars_per_block
                    mrb = (total_return / num_blocks) * 100 if num_blocks > 0 else 0.0
                    return {'mrb': mrb, 'mrd': mrb}  # Use MRB as fallback for MRD

            return {'mrd': 0.0, 'mrb': 0.0, 'error': 'MRD not found'}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Analysis timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

    def tune_on_window(self, block_start: int, block_end: int,
                       n_trials: int = 100, phase2_center: Dict = None) -> Tuple[Dict, float, float]:
        """
        Tune parameters on specified block window.

        Args:
            block_start: Starting block (inclusive)
            block_end: Ending block (exclusive)
            n_trials: Number of Optuna trials
            phase2_center: If provided, use narrow ranges around these params (Phase 2 micro-tuning)

        Returns:
            (best_params, best_mrb, tuning_time_seconds)
        """
        phase_label = "PHASE 2 (micro-tuning)" if phase2_center else "PHASE 1 (wide search)"
        print(f"\n[Tuning] {phase_label} - Blocks {block_start}-{block_end-1} ({n_trials} trials)")
        if phase2_center:
            print(f"[Phase2] Center params: buy={phase2_center.get('buy_threshold', 0.53):.3f}, "
                  f"sell={phase2_center.get('sell_threshold', 0.48):.3f}, "
                  f"λ={phase2_center.get('ewrls_lambda', 0.992):.4f}, "
                  f"BB={phase2_center.get('bb_amplification_factor', 0.05):.3f}")

        # Create data file for this window
        train_data = os.path.join(
            self.output_dir,
            f"train_blocks_{block_start}_{block_end}.csv"
        )
        train_data = self.create_block_data(block_start, block_end, train_data)

        # Pre-extract features for all trials (one-time cost, 4-5x speedup)
        if self.use_cache:
            self.extract_features_cached(train_data)

        # Define Optuna objective
        def objective(trial):
            if phase2_center is None:
                # PHASE 1: Optimize primary parameters (EXPANDED RANGES for 0.5% MRB target)
                params = {
                    'buy_threshold': trial.suggest_float('buy_threshold', 0.50, 0.65, step=0.01),
                    'sell_threshold': trial.suggest_float('sell_threshold', 0.35, 0.50, step=0.01),
                    'ewrls_lambda': trial.suggest_float('ewrls_lambda', 0.985, 0.999, step=0.001),
                    'bb_amplification_factor': trial.suggest_float('bb_amplification_factor',
                                                                   0.00, 0.20, step=0.01)
                }

                # Ensure asymmetric thresholds (buy > sell)
                if params['buy_threshold'] <= params['sell_threshold']:
                    return -999.0

            else:
                # PHASE 2: Optimize secondary parameters (FIX Phase 1 params at best values)
                # Use best Phase 1 parameters as FIXED

                # Sample only 2 weights, compute 3rd to ensure sum = 1.0
                h1_weight = trial.suggest_float('h1_weight', 0.1, 0.6, step=0.05)
                h5_weight = trial.suggest_float('h5_weight', 0.2, 0.7, step=0.05)
                h10_weight = 1.0 - h1_weight - h5_weight

                # Reject if h10 is out of valid range [0.1, 0.5]
                if h10_weight < 0.05 or h10_weight > 0.6:
                    return -999.0

                params = {
                    # Phase 1 params FIXED at best values
                    'buy_threshold': phase2_center.get('buy_threshold', 0.53),
                    'sell_threshold': phase2_center.get('sell_threshold', 0.48),
                    'ewrls_lambda': phase2_center.get('ewrls_lambda', 0.992),
                    'bb_amplification_factor': phase2_center.get('bb_amplification_factor', 0.05),

                    # Phase 2 params OPTIMIZED (weights guaranteed to sum to 1.0) - EXPANDED RANGES
                    'h1_weight': h1_weight,
                    'h5_weight': h5_weight,
                    'h10_weight': h10_weight,
                    'bb_period': trial.suggest_int('bb_period', 5, 40, step=5),
                    'bb_std_dev': trial.suggest_float('bb_std_dev', 1.0, 3.0, step=0.25),
                    'bb_proximity': trial.suggest_float('bb_proximity', 0.10, 0.50, step=0.05),
                    'regularization': trial.suggest_float('regularization', 0.0, 0.10, step=0.005)
                }

            result = self.run_backtest(train_data, params, warmup_blocks=2)

            # Log trial (use MRD as primary metric)
            mrd = result.get('mrd', result.get('mrb', 0.0))
            mrb = result.get('mrb', 0.0)
            print(f"  Trial {trial.number}: MRD={mrd:.4f}% (MRB={mrb:.4f}%) "
                  f"buy={params['buy_threshold']:.2f} "
                  f"sell={params['sell_threshold']:.2f}")

            return mrd  # Optimize for MRD (daily returns)

        # Run Optuna optimization
        start_time = time.time()

        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )

        # Run optimization with parallel trials
        print(f"[Optuna] Running {n_trials} trials with {self.n_jobs} parallel jobs")
        study.optimize(objective, n_trials=n_trials, n_jobs=self.n_jobs, show_progress_bar=True)

        tuning_time = time.time() - start_time

        best_params = study.best_params
        best_mrd = study.best_value

        print(f"[Tuning] Complete in {tuning_time:.1f}s")
        print(f"[Tuning] Best MRD: {best_mrd:.4f}%")
        print(f"[Tuning] Best params: {best_params}")

        return best_params, best_mrd, tuning_time

    def test_on_window(self, params: Dict, block_start: int,
                       block_end: int) -> Dict:
        """
        Test parameters on specified block window.

        Args:
            params: Strategy parameters
            block_start: Starting block (inclusive)
            block_end: Ending block (exclusive)

        Returns:
            Dictionary with test results
        """
        print(f"[Testing] Blocks {block_start}-{block_end-1} with params: {params}")

        # Create test data file
        test_data = os.path.join(
            self.output_dir,
            f"test_blocks_{block_start}_{block_end}.csv"
        )
        test_data = self.create_block_data(block_start, block_end, test_data)

        # Run backtest
        result = self.run_backtest(test_data, params, warmup_blocks=2)

        mrd = result.get('mrd', result.get('mrb', 0.0))
        mrb = result.get('mrb', 0.0)
        print(f"[Testing] MRD: {mrd:.4f}% | MRB: {mrb:.4f}%")

        return {
            'block_start': block_start,
            'block_end': block_end,
            'params': params,
            'mrd': mrd,
            'mrb': mrb
        }

    def strategy_a_per_block(self, start_block: int = 10,
                             test_horizon: int = 5) -> List[Dict]:
        """
        Strategy A: Per-block adaptive.

        Retunes parameters after every block, tests on next 5 blocks.

        Args:
            start_block: First block to start tuning from
            test_horizon: Number of blocks to test (5 blocks = ~5 days)

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY A: PER-BLOCK ADAPTIVE")
        print("="*80)

        results = []

        # Need at least start_block blocks for training + test_horizon for testing
        max_test_block = self.total_blocks - test_horizon

        for block_idx in range(start_block, max_test_block):
            print(f"\n--- Block {block_idx}/{max_test_block-1} ---")

            # Tune on last 10 blocks
            train_start = max(0, block_idx - 10)
            train_end = block_idx

            params, train_mrb, tuning_time = self.tune_on_window(
                train_start, train_end, n_trials=self.n_trials
            )

            # Test on next 5 blocks
            test_start = block_idx
            test_end = block_idx + test_horizon

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = train_start
            test_result['train_end'] = train_end

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_a_partial')

        return results

    def strategy_b_4hour(self, start_block: int = 20,
                         retune_frequency: int = 2,
                         test_horizon: int = 5) -> List[Dict]:
        """
        Strategy B: 4-hour adaptive (retune every 2 blocks).

        Args:
            start_block: First block to start from
            retune_frequency: Retune every N blocks (2 = twice daily)
            test_horizon: Number of blocks to test

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY B: 4-HOUR ADAPTIVE")
        print("="*80)

        results = []
        max_test_block = self.total_blocks - test_horizon

        current_params = None

        for block_idx in range(start_block, max_test_block, retune_frequency):
            print(f"\n--- Block {block_idx}/{max_test_block-1} ---")

            # Tune on last 20 blocks
            train_start = max(0, block_idx - 20)
            train_end = block_idx

            params, train_mrb, tuning_time = self.tune_on_window(
                train_start, train_end, n_trials=self.n_trials
            )
            current_params = params

            # Test on next 5 blocks
            test_start = block_idx
            test_end = min(block_idx + test_horizon, self.total_blocks)

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = train_start
            test_result['train_end'] = train_end

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_b_partial')

        return results

    def strategy_c_static(self, train_blocks: int = 20,
                          test_horizon: int = 5) -> List[Dict]:
        """
        Strategy C: Static baseline.

        Tune once on first N blocks, then test on all remaining blocks.

        Args:
            train_blocks: Number of blocks to train on
            test_horizon: Number of blocks per test window

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY C: STATIC BASELINE")
        print("="*80)

        # Tune once on first train_blocks
        print(f"\n--- Tuning on first {train_blocks} blocks ---")
        params, train_mrb, tuning_time = self.tune_on_window(
            0, train_blocks, n_trials=self.n_trials
        )

        print(f"\n[Static] Using fixed params for all tests: {params}")

        results = []

        # Test on all remaining blocks in test_horizon windows
        for block_idx in range(train_blocks, self.total_blocks - test_horizon,
                               test_horizon):
            print(f"\n--- Testing blocks {block_idx}-{block_idx+test_horizon-1} ---")

            test_start = block_idx
            test_end = block_idx + test_horizon

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time if block_idx == train_blocks else 0.0
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = 0
            test_result['train_end'] = train_blocks

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_c_partial')

        return results

    def _save_results(self, results: List[Dict], filename: str):
        """Save results to JSON file."""
        output_file = os.path.join(self.output_dir, f"{filename}.json")
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"[Results] Saved to {output_file}")

    def run_strategy(self, strategy: str) -> List[Dict]:
        """
        Run specified strategy.

        Args:
            strategy: 'A', 'B', or 'C'

        Returns:
            List of test results
        """
        if strategy == 'A':
            return self.strategy_a_per_block()
        elif strategy == 'B':
            return self.strategy_b_4hour()
        elif strategy == 'C':
            return self.strategy_c_static()
        else:
            raise ValueError(f"Unknown strategy: {strategy}")


def main():
    parser = argparse.ArgumentParser(
        description="Adaptive Optuna Framework for OnlineEnsemble"
    )
    parser.add_argument('--strategy', choices=['A', 'B', 'C'], required=True,
                        help='Strategy to run: A (per-block), B (4-hour), C (static)')
    parser.add_argument('--data', required=True,
                        help='Path to data CSV file')
    parser.add_argument('--build-dir', default='build',
                        help='Path to build directory')
    parser.add_argument('--output', required=True,
                        help='Path to output JSON file')
    parser.add_argument('--n-trials', type=int, default=50,
                        help='Number of Optuna trials (default: 50)')
    parser.add_argument('--n-jobs', type=int, default=4,
                        help='Number of parallel jobs (default: 4 for 4x speedup)')

    args = parser.parse_args()

    # Determine project root and build directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    build_dir = project_root / args.build_dir
    output_dir = project_root / "data" / "tmp" / "ab_test_results"

    print("="*80)
    print("ADAPTIVE OPTUNA FRAMEWORK")
    print("="*80)
    print(f"Strategy: {args.strategy}")
    print(f"Data: {args.data}")
    print(f"Build: {build_dir}")
    print(f"Output: {args.output}")
    print("="*80)

    # Create framework
    framework = AdaptiveOptunaFramework(
        data_file=args.data,
        build_dir=str(build_dir),
        output_dir=str(output_dir),
        n_trials=args.n_trials,
        n_jobs=args.n_jobs
    )

    # Run strategy
    start_time = time.time()
    results = framework.run_strategy(args.strategy)
    total_time = time.time() - start_time

    # Calculate summary statistics
    mrbs = [r['mrb'] for r in results]

    # Handle empty results
    if len(mrbs) == 0 or all(m == -999.0 for m in mrbs):
        summary = {
            'strategy': args.strategy,
            'total_tests': len(results),
            'mean_mrb': 0.0,
            'std_mrb': 0.0,
            'min_mrb': 0.0,
            'max_mrb': 0.0,
            'total_time': total_time,
            'results': results,
            'error': 'All tests failed'
        }
    else:
        # Filter out failed trials
        valid_mrbs = [m for m in mrbs if m != -999.0]
        summary = {
            'strategy': args.strategy,
            'total_tests': len(results),
            'mean_mrb': np.mean(valid_mrbs) if valid_mrbs else 0.0,
            'std_mrb': np.std(valid_mrbs) if valid_mrbs else 0.0,
            'min_mrb': np.min(valid_mrbs) if valid_mrbs else 0.0,
            'max_mrb': np.max(valid_mrbs) if valid_mrbs else 0.0,
            'total_time': total_time,
            'results': results
        }

    # Save final results
    with open(args.output, 'w') as f:
        json.dump(summary, f, indent=2)

    print("\n" + "="*80)
    print("SUMMARY")
    print("="*80)
    print(f"Strategy: {args.strategy}")
    print(f"Total tests: {len(results)}")
    print(f"Mean MRB: {summary['mean_mrb']:.4f}%")
    print(f"Std MRB: {summary['std_mrb']:.4f}%")
    print(f"Min MRB: {summary['min_mrb']:.4f}%")
    print(f"Max MRB: {summary['max_mrb']:.4f}%")
    print(f"Total time: {total_time/60:.1f} minutes")
    print(f"Results saved to: {args.output}")
    print("="*80)


if __name__ == '__main__':
    main()

```

## 📄 **FILE 40 of 40**: tools/warmup_live_trading.sh

**File Information**:
- **Path**: `tools/warmup_live_trading.sh`
- **Size**: 251 lines
- **Modified**: 2025-10-16 04:16:12
- **Type**: sh
- **Permissions**: -rwxr-xr-x

```text
#!/bin/bash
# =============================================================================
# Live Trading Warmup Script
# =============================================================================
# Downloads recent historical data and prepares the strategy for live trading.
# This script ensures the strategy can start at ANY time during market hours
# with full warmup (960 bars + today's bars).
#
# Usage:
#   ./warmup_live_trading.sh [--symbols SPY,SPXL,SH,SDS]
#
# What it does:
#   1. Calculates date range (2 trading days ago → now)
#   2. Downloads 1-min bars for all trading symbols
#   3. Combines warmup (960 bars) + today's bars
#   4. Starts live trading with pre-warmed strategy
#
# Author: Generated by Claude Code
# Date: 2025-10-08
# =============================================================================

set -e  # Exit on error

# Configuration
PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
DATA_DIR="$PROJECT_ROOT/data/equities"
WARMUP_DIR="$PROJECT_ROOT/data/tmp/warmup"
TOOLS_DIR="$PROJECT_ROOT/tools"
BUILD_DIR="$PROJECT_ROOT/build"

# Default symbols (OnlineTrader v2.0 instruments)
SYMBOLS="SPY"
PRIMARY_SYMBOL="SPY"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --symbols)
            SYMBOLS="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [--symbols SPY,SPXL,SH,SDS]"
            echo ""
            echo "Downloads recent market data and warms up the strategy for live trading."
            echo ""
            echo "Options:"
            echo "  --symbols    Comma-separated list of symbols (default: SPY)"
            echo "  --help       Show this help message"
            echo ""
            echo "Example:"
            echo "  $0 --symbols SPY,SPXL,SH,SDS"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Create warmup directory
mkdir -p "$WARMUP_DIR"

# Get current date and time in ET
echo "=== Live Trading Warmup ==="
echo ""
echo "Determining current market time..."

# Get today's date
TODAY=$(TZ='America/New_York' date '+%Y-%m-%d')
CURRENT_TIME=$(TZ='America/New_York' date '+%H:%M:%S')
CURRENT_HOUR=$(TZ='America/New_York' date '+%H')
CURRENT_MIN=$(TZ='America/New_York' date '+%M')

echo "  Current time (ET): $TODAY $CURRENT_TIME"

# Calculate dates for warmup (need 2-3 trading days for 960 bars)
# We'll download last 5 calendar days to ensure we get 2+ trading days
START_DATE=$(TZ='America/New_York' date -v-5d '+%Y-%m-%d')
END_DATE=$TODAY

echo "  Warmup period: $START_DATE to $END_DATE"
echo ""

# Check if market is open
MARKET_OPEN=0
if [[ $CURRENT_HOUR -ge 9 && $CURRENT_HOUR -lt 16 ]]; then
    if [[ $CURRENT_HOUR -eq 9 && $CURRENT_MIN -lt 30 ]]; then
        echo "⚠️  Before market open (9:30 AM ET) - will wait for open"
    elif [[ $CURRENT_HOUR -eq 15 && $CURRENT_MIN -ge 58 ]]; then
        echo "⚠️  After market close (4:00 PM ET) - using today's full data"
    else
        echo "✓ Market is currently open - downloading real-time warmup data"
        MARKET_OPEN=1
    fi
fi
echo ""

# Load Polygon API key from config
if [[ -f "$PROJECT_ROOT/config.env" ]]; then
    source "$PROJECT_ROOT/config.env"
    echo "✓ Loaded API credentials from config.env"
else
    echo "❌ config.env not found - please create it with POLYGON_API_KEY"
    exit 1
fi

if [[ -z "$POLYGON_API_KEY" ]]; then
    echo "❌ POLYGON_API_KEY not set in config.env"
    exit 1
fi
echo ""

# Download data for each symbol
echo "=== Downloading Market Data ==="
echo ""

IFS=',' read -ra SYMBOL_ARRAY <<< "$SYMBOLS"

for SYMBOL in "${SYMBOL_ARRAY[@]}"; do
    echo "Downloading $SYMBOL..."

    OUTPUT_FILE="$WARMUP_DIR/${SYMBOL}_warmup_${TODAY}.csv"

    python3 "$TOOLS_DIR/data_downloader.py" \
        $SYMBOL \
        --start "$START_DATE" \
        --end "$END_DATE" \
        --outdir "$WARMUP_DIR" \
        --timespan minute \
        --multiplier 1 \
        2>&1 | grep -E "(Downloading|Downloaded|bars|ERROR)" || true

    # Rename output to expected format (data_downloader creates *_RTH_NH.csv)
    if [[ -f "$WARMUP_DIR/${SYMBOL}_RTH_NH.csv" ]]; then
        mv "$WARMUP_DIR/${SYMBOL}_RTH_NH.csv" "$OUTPUT_FILE"
        BAR_COUNT=$(tail -n +2 "$OUTPUT_FILE" | wc -l | tr -d ' ')
        echo "  ✓ $SYMBOL: $BAR_COUNT bars saved to $OUTPUT_FILE"
    elif [[ -f "$WARMUP_DIR/${SYMBOL}.csv" ]]; then
        mv "$WARMUP_DIR/${SYMBOL}.csv" "$OUTPUT_FILE"
        BAR_COUNT=$(tail -n +2 "$OUTPUT_FILE" | wc -l | tr -d ' ')
        echo "  ✓ $SYMBOL: $BAR_COUNT bars saved to $OUTPUT_FILE"
    else
        echo "  ❌ Failed to download $SYMBOL"
    fi
    echo ""
done

# Analyze primary symbol data
PRIMARY_FILE="$WARMUP_DIR/${PRIMARY_SYMBOL}_warmup_${TODAY}.csv"

if [[ ! -f "$PRIMARY_FILE" ]]; then
    echo "❌ Primary symbol ($PRIMARY_SYMBOL) data not found"
    exit 1
fi

# Count total bars
TOTAL_BARS=$(tail -n +2 "$PRIMARY_FILE" | wc -l | tr -d ' ')
echo "=== Warmup Data Summary ==="
echo "  Primary symbol: $PRIMARY_SYMBOL"
echo "  Total bars: $TOTAL_BARS"
echo ""

# Determine how many bars are from today
TODAY_BARS=$(tail -n +2 "$PRIMARY_FILE" | grep "^$TODAY" | wc -l | tr -d ' ')
HISTORICAL_BARS=$((TOTAL_BARS - TODAY_BARS))

echo "  Historical bars: $HISTORICAL_BARS"
echo "  Today's bars: $TODAY_BARS"
echo ""

# Check if we have enough for warmup (need at least 960 bars)
MIN_WARMUP=960

if [[ $TOTAL_BARS -lt $MIN_WARMUP ]]; then
    echo "⚠️  WARNING: Only $TOTAL_BARS bars available, need at least $MIN_WARMUP"
    echo "    Strategy may not be fully warmed up"
    echo "    Consider extending START_DATE"
    echo ""
fi

# Calculate warmup strategy
if [[ $TOTAL_BARS -ge $MIN_WARMUP ]]; then
    # Use last 960 bars for warmup, rest for today
    WARMUP_BARS=$MIN_WARMUP
    if [[ $TODAY_BARS -gt 0 ]]; then
        echo "✓ Warmup strategy:"
        echo "    1. Feed $WARMUP_BARS historical bars for warmup"
        echo "    2. Feed $TODAY_BARS bars from today"
        echo "    3. Start live trading at current bar"
    else
        echo "✓ Warmup strategy:"
        echo "    1. Feed $WARMUP_BARS bars for warmup"
        echo "    2. Wait for market open to start live trading"
    fi
else
    WARMUP_BARS=$HISTORICAL_BARS
    echo "⚠️  Limited warmup:"
    echo "    1. Feed all $WARMUP_BARS available bars"
    echo "    2. Start live trading (may be cold start)"
fi
echo ""

# Save warmup info for live trading
WARMUP_INFO="$WARMUP_DIR/warmup_info.txt"
cat > "$WARMUP_INFO" <<EOF
PRIMARY_SYMBOL=$PRIMARY_SYMBOL
WARMUP_FILE=$PRIMARY_FILE
TOTAL_BARS=$TOTAL_BARS
WARMUP_BARS=$WARMUP_BARS
TODAY_BARS=$TODAY_BARS
GENERATED_AT=$(date '+%Y-%m-%d %H:%M:%S %Z')
EOF

echo "✓ Warmup info saved to $WARMUP_INFO"
echo ""

# Create a copy in the expected location
cp "$PRIMARY_FILE" "$DATA_DIR/SPY_warmup_latest.csv"
echo "✓ Copied to $DATA_DIR/SPY_warmup_latest.csv for easy access"
echo ""

# Summary
echo "=== Ready for Live Trading ==="
echo ""
echo "Warmup data prepared successfully!"
echo ""
echo "Next steps:"
echo "  1. The live trading system will use: $DATA_DIR/SPY_warmup_latest.csv"
echo "  2. It will warm up on first $WARMUP_BARS bars"
echo "  3. Then process today's $TODAY_BARS bars"
echo "  4. Finally, start live trading at current time"
echo ""
echo "To start live trading:"
echo "  cd $BUILD_DIR"
echo "  ./sentio_cli live-trade"
echo ""

# Optionally, display the last few bars to verify data
echo "=== Last 5 Bars (verification) ==="
tail -6 "$PRIMARY_FILE" | tail -5 | while IFS=, read timestamp open high low close volume; do
    if [[ "$timestamp" != "timestamp" ]]; then
        # Format nicely
        echo "  $(echo $timestamp | cut -d'T' -f1) $(echo $timestamp | cut -d'T' -f2 | cut -d'-' -f1): O=$open H=$high L=$low C=$close V=$volume"
    fi
done
echo ""

echo "✓ Warmup complete - ready to start live trading!"

```

