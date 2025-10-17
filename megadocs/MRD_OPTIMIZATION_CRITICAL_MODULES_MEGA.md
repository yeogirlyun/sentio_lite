# MRD_OPTIMIZATION_CRITICAL_MODULES - Complete Analysis

**Generated**: 2025-10-17 00:12:27
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: /Volumes/ExternalSSD/Dev/C++/online_trader/MRD_OPTIMIZATION_CRITICAL_MODULES.md
**Total Files**: 14

---

## 📋 **TABLE OF CONTENTS**

1. [config/rotation_strategy.json](#file-1)
2. [include/backend/adaptive_trading_mechanism.h](#file-2)
3. [include/features/unified_feature_engine.h](#file-3)
4. [include/strategy/online_ensemble_strategy.h](#file-4)
5. [src/backend/adaptive_trading_mechanism.cpp](#file-5)
6. [src/common/data_validator.cpp](#file-6)
7. [src/data/multi_symbol_data_manager.cpp](#file-7)
8. [src/features/unified_feature_engine.cpp](#file-8)
9. [src/learning/online_predictor.cpp](#file-9)
10. [src/strategy/market_regime_detector.cpp](#file-10)
11. [src/strategy/multi_symbol_oes_manager.cpp](#file-11)
12. [src/strategy/online_ensemble_strategy.cpp](#file-12)
13. [src/strategy/regime_parameter_manager.cpp](#file-13)
14. [src/strategy/signal_output.cpp](#file-14)

---

## 📄 **FILE 1 of 14**: config/rotation_strategy.json

**File Information**:
- **Path**: `config/rotation_strategy.json`
- **Size**: 82 lines
- **Modified**: 2025-10-16 23:33:31
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
    "min_probability": 0.48,
    "min_confidence": 0.01,
    "min_strength": 0.005,

    "filter_stale_signals": true,
    "max_staleness_seconds": 120.0
  },

  "rotation_manager_config": {
    "max_positions": 3,
    "min_strength_to_enter": 0.05,
    "rotation_strength_delta": 0.05,

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

## 📄 **FILE 2 of 14**: include/backend/adaptive_trading_mechanism.h

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

## 📄 **FILE 3 of 14**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`
- **Size**: 270 lines
- **Modified**: 2025-10-16 23:05:14
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include "features/indicators.h"
#include "features/scaler.h"
#include <string>
#include <vector>
#include <array>
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
    // Maximum features with all toggles enabled (buffer for future expansion)
    static constexpr size_t MAX_FEATURES = 64;

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
     * Returns pointer and size for efficient access without heap allocation.
     */
    const double* features_data() const { return feats_.data(); }
    size_t features_size() const { return feature_count_; }

    /**
     * Get feature vector as std::vector for backward compatibility.
     * Note: This creates a copy - prefer features_data()/features_size() for performance.
     */
    std::vector<double> features_vector() const {
        return std::vector<double>(feats_.data(), feats_.data() + feature_count_);
    }

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
    void validate_features_();  // Phase 0: NaN/Inf validation
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
    // Pre-allocated array eliminates per-bar heap allocations (8-12% speedup)
    std::array<double, MAX_FEATURES> feats_;
    size_t feature_count_;  // Actual number of features (based on config)

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

## 📄 **FILE 4 of 14**: include/strategy/online_ensemble_strategy.h

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

## 📄 **FILE 5 of 14**: src/backend/adaptive_trading_mechanism.cpp

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

## 📄 **FILE 6 of 14**: src/common/data_validator.cpp

**File Information**:
- **Path**: `src/common/data_validator.cpp`
- **Size**: 149 lines
- **Modified**: 2025-10-16 00:11:09
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "common/data_validator.h"
#include <cmath>
#include <sstream>

namespace sentio {

bool DataValidator::validate_bar(const std::string& symbol, const Bar& bar) {
    last_error_.clear();

    // Check for NaN or invalid prices
    if (std::isnan(bar.open) || std::isnan(bar.high) ||
        std::isnan(bar.low) || std::isnan(bar.close)) {
        last_error_ = "[DataValidator] NaN price detected for " + symbol;
        return false;
    }

    // Check for zero or negative prices
    if (bar.open <= 0 || bar.high <= 0 || bar.low <= 0 || bar.close <= 0) {
        last_error_ = "[DataValidator] Zero/negative price detected for " + symbol;
        return false;
    }

    // Check OHLC consistency
    if (!check_ohlc_consistency(bar)) {
        return config_.strict_mode ? false : true;
    }

    // Check price anomaly (compared to previous bar)
    if (!check_price_anomaly(symbol, bar)) {
        return config_.strict_mode ? false : true;
    }

    // Check spread (high-low range)
    if (!check_spread(bar)) {
        return config_.strict_mode ? false : true;
    }

    // Check volume
    if (!check_volume(bar)) {
        return config_.strict_mode ? false : true;
    }

    // Store for next comparison
    prev_bars_[symbol] = bar;

    return true;
}

bool DataValidator::check_ohlc_consistency(const Bar& bar) {
    // High must be >= all other prices
    if (bar.high < bar.low || bar.high < bar.open || bar.high < bar.close) {
        last_error_ = "[DataValidator] High price inconsistency (high < low/open/close)";
        return false;
    }

    // Low must be <= all other prices
    if (bar.low > bar.open || bar.low > bar.close) {
        last_error_ = "[DataValidator] Low price inconsistency (low > open/close)";
        return false;
    }

    return true;
}

bool DataValidator::check_price_anomaly(const std::string& symbol, const Bar& bar) {
    auto it = prev_bars_.find(symbol);
    if (it == prev_bars_.end()) {
        return true;  // First bar, no comparison
    }

    const Bar& prev = it->second;
    double price_move = std::abs(bar.close - prev.close) / prev.close;

    if (price_move > config_.max_price_move_pct) {
        std::ostringstream oss;
        oss << "[DataValidator] Price anomaly for " << symbol
            << ": " << (price_move * 100.0) << "% move in 1 minute"
            << " (prev=" << prev.close << ", curr=" << bar.close << ")";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_spread(const Bar& bar) {
    if (bar.close <= 0) return false;

    double spread = (bar.high - bar.low) / bar.close;

    if (spread > config_.max_spread_pct) {
        std::ostringstream oss;
        oss << "[DataValidator] Excessive spread: " << (spread * 100.0) << "%"
            << " (high=" << bar.high << ", low=" << bar.low << ")";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_staleness(const Bar& bar) {
    auto now = std::chrono::system_clock::now();
    auto bar_time = std::chrono::system_clock::from_time_t(bar.timestamp_ms / 1000);
    auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - bar_time).count();

    if (age_seconds > config_.max_staleness_seconds) {
        std::ostringstream oss;
        oss << "[DataValidator] Stale data: " << age_seconds << " seconds old";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_volume(const Bar& bar) {
    if (bar.volume < config_.min_volume) {
        std::ostringstream oss;
        oss << "[DataValidator] Low volume: " << bar.volume
            << " (min=" << config_.min_volume << ")";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::validate_snapshot(const std::map<std::string, Bar>& snapshot) {
    bool all_valid = true;

    for (const auto& [symbol, bar] : snapshot) {
        if (!validate_bar(symbol, bar)) {
            all_valid = false;
            if (config_.strict_mode) {
                return false;  // Fail fast in strict mode
            }
        }
    }

    return all_valid;
}

void DataValidator::reset() {
    prev_bars_.clear();
    last_error_.clear();
}

} // namespace sentio

```

## 📄 **FILE 7 of 14**: src/data/multi_symbol_data_manager.cpp

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

## 📄 **FILE 8 of 14**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`
- **Size**: 701 lines
- **Modified**: 2025-10-16 23:05:47
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
      scaler_(cfg.robust ? Scaler::Type::ROBUST : Scaler::Type::STANDARD),
      feature_count_(0)
{
    build_schema_();

    // Validate feature count fits in pre-allocated array
    feature_count_ = schema_.names.size();
    if (feature_count_ > MAX_FEATURES) {
        throw std::runtime_error(
            "Feature count " + std::to_string(feature_count_) +
            " exceeds MAX_FEATURES " + std::to_string(MAX_FEATURES)
        );
    }

    // Initialize feature array with NaN
    std::fill(feats_.begin(), feats_.begin() + feature_count_, std::numeric_limits<double>::quiet_NaN());
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

    // Phase 0: Validate features for NaN/Inf (after warmup)
    validate_features_();

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

void UnifiedFeatureEngine::validate_features_() {
    // Phase 0: NaN/Inf validation
    // Only validate after warmup is complete to avoid false alarms during indicator initialization
    if (warmup_remaining() > 0) {
        return;  // Still in warmup - NaN is expected
    }

    // Count invalid features
    std::vector<size_t> nan_indices;
    std::vector<size_t> inf_indices;

    for (size_t i = 0; i < feature_count_; ++i) {
        if (std::isnan(feats_[i])) {
            nan_indices.push_back(i);
        } else if (std::isinf(feats_[i])) {
            inf_indices.push_back(i);
        }
    }

    // Log warnings if invalid features found (limit to first 10 occurrences to avoid spam)
    static int nan_warning_count = 0;
    static int inf_warning_count = 0;
    const int MAX_WARNINGS = 10;

    if (!nan_indices.empty() && nan_warning_count < MAX_WARNINGS) {
        std::cerr << "[FeatureEngine WARNING] NaN features detected after warmup!"
                  << " bar_count=" << bar_count_
                  << ", count=" << nan_indices.size() << "/" << feature_count_
                  << ", indices=[";

        // Show first 5 invalid feature names
        size_t show_count = std::min(size_t(5), nan_indices.size());
        for (size_t i = 0; i < show_count; ++i) {
            size_t idx = nan_indices[i];
            if (idx < schema_.names.size()) {
                std::cerr << (i > 0 ? ", " : "") << schema_.names[idx];
            }
        }
        if (nan_indices.size() > show_count) {
            std::cerr << ", ..." << (nan_indices.size() - show_count) << " more";
        }
        std::cerr << "]" << std::endl;

        nan_warning_count++;
        if (nan_warning_count == MAX_WARNINGS) {
            std::cerr << "[FeatureEngine] (suppressing further NaN warnings)" << std::endl;
        }
    }

    if (!inf_indices.empty() && inf_warning_count < MAX_WARNINGS) {
        std::cerr << "[FeatureEngine WARNING] Inf features detected after warmup!"
                  << " bar_count=" << bar_count_
                  << ", count=" << inf_indices.size() << "/" << feature_count_
                  << ", indices=[";

        // Show first 5 invalid feature names
        size_t show_count = std::min(size_t(5), inf_indices.size());
        for (size_t i = 0; i < show_count; ++i) {
            size_t idx = inf_indices[i];
            if (idx < schema_.names.size()) {
                std::cerr << (i > 0 ? ", " : "") << schema_.names[idx];
            }
        }
        if (inf_indices.size() > show_count) {
            std::cerr << ", ..." << (inf_indices.size() - show_count) << " more";
        }
        std::cerr << "]" << std::endl;

        inf_warning_count++;
        if (inf_warning_count == MAX_WARNINGS) {
            std::cerr << "[FeatureEngine] (suppressing further Inf warnings)" << std::endl;
        }
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

## 📄 **FILE 9 of 14**: src/learning/online_predictor.cpp

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

## 📄 **FILE 10 of 14**: src/strategy/market_regime_detector.cpp

**File Information**:
- **Path**: `src/strategy/market_regime_detector.cpp`
- **Size**: 171 lines
- **Modified**: 2025-10-08 09:53:39
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "strategy/market_regime_detector.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace sentio {

static inline double safe_log(double x) { return std::log(std::max(1e-12, x)); }

MarketRegimeDetector::MarketRegimeDetector() : p_(MarketRegimeDetectorParams{}) {}

MarketRegimeDetector::MarketRegimeDetector(const Params& p) : p_(p) {}

double MarketRegimeDetector::std_log_returns(const std::vector<Bar>& v, int win) {
    if ((int)v.size() < win + 1) return 0.0;
    const int n0 = (int)v.size() - win;
    std::vector<double> r; r.reserve(win);
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        r.push_back(safe_log(v[i].close / v[i-1].close));
    }
    double mean = std::accumulate(r.begin(), r.end(), 0.0) / r.size();
    double acc = 0.0;
    for (double x : r) { double d = x - mean; acc += d * d; }
    return std::sqrt(acc / std::max<size_t>(1, r.size() - 1));
}

void MarketRegimeDetector::slope_r2_log_price(const std::vector<Bar>& v, int win, double& slope, double& r2) {
    slope = 0.0; r2 = 0.0;
    if ((int)v.size() < win) return;
    const int n0 = (int)v.size() - win;
    std::vector<double> y; y.reserve(win);
    for (int i = n0; i < (int)v.size(); ++i) y.push_back(safe_log(v[i].close));

    // x = 0..win-1
    const double n = (double)win;
    const double sx = (n - 1.0) * n / 2.0;
    const double sxx = (n - 1.0) * n * (2.0 * n - 1.0) / 6.0;
    double sy = 0.0, sxy = 0.0;
    for (int i = 0; i < win; ++i) { sy += y[i]; sxy += i * y[i]; }

    const double denom = n * sxx - sx * sx;
    if (std::abs(denom) < 1e-12) return;
    slope = (n * sxy - sx * sy) / denom;
    const double intercept = (sy - slope * sx) / n;

    // R²
    double ss_tot = 0.0, ss_res = 0.0;
    const double y_bar = sy / n;
    for (int i = 0; i < win; ++i) {
        const double y_hat = intercept + slope * i;
        ss_res += (y[i] - y_hat) * (y[i] - y_hat);
        ss_tot += (y[i] - y_bar) * (y[i] - y_bar);
    }
    r2 = (ss_tot > 0.0) ? (1.0 - ss_res / ss_tot) : 0.0;
}

double MarketRegimeDetector::chop_index(const std::vector<Bar>& v, int win) {
    if ((int)v.size() < win + 1) return 50.0;
    const int n0 = (int)v.size() - win;
    double atr_sum = 0.0;
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        const auto& c = v[i];
        const auto& p = v[i-1];
        const double tr = std::max({ c.high - c.low,
                                     std::abs(c.high - p.close),
                                     std::abs(c.low  - p.close) });
        atr_sum += tr;
    }
    double hi = v[n0].high, lo = v[n0].low;
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        hi = std::max(hi, v[i].high);
        lo = std::min(lo, v[i].low);
    }
    const double range = std::max(1e-12, hi - lo);
    const double x = std::log10(std::max(1e-12, atr_sum / range));
    const double denom = std::log10((double)win);
    return 100.0 * x / std::max(1e-12, denom); // typical CHOP 0..100
}

double MarketRegimeDetector::percentile(std::vector<double>& tmp, double q) {
    if (tmp.empty()) return 0.0;
    q = std::clamp(q, 0.0, 1.0);
    size_t k = (size_t)std::floor(q * (tmp.size() - 1));
    std::nth_element(tmp.begin(), tmp.begin() + k, tmp.end());
    return tmp[k];
}

void MarketRegimeDetector::update_vol_thresholds(double vol_sample) {
    // Keep rolling window
    vol_cal_.push_back(vol_sample);
    while ((int)vol_cal_.size() > p_.calibr_window) vol_cal_.pop_front();
    if ((int)vol_cal_.size() < std::min(500, p_.calibr_window/2)) return; // not enough

    std::vector<double> tmp(vol_cal_.begin(), vol_cal_.end());
    vol_lo_ = percentile(tmp, 0.30);
    vol_hi_ = percentile(tmp, 0.70);

    // Safety guard: keep some separation
    if (vol_hi_ - vol_lo_ < 5e-5) {
        vol_lo_ = std::max(0.0, vol_lo_ - 1e-4);
        vol_hi_ = vol_hi_ + 1e-4;
    }
}

MarketRegime MarketRegimeDetector::detect(const std::vector<Bar>& bars) {
    // 1) Extract features
    last_feat_.vol = std_log_returns(bars, p_.vol_window);
    slope_r2_log_price(bars, p_.slope_window, last_feat_.slope, last_feat_.r2);
    last_feat_.chop = chop_index(bars, p_.chop_window);

    // 2) Adapt volatility thresholds
    update_vol_thresholds(last_feat_.vol);

    // 3) Compute scores
    auto score_high_vol = (vol_hi_ > 0.0) ? (last_feat_.vol - vol_hi_) / std::max(1e-12, vol_hi_) : -1.0;
    auto score_low_vol  = (vol_lo_ > 0.0) ? (vol_lo_ - last_feat_.vol) / std::max(1e-12, vol_lo_) : -1.0;

    const bool trending = std::abs(last_feat_.slope) >= p_.trend_slope_min && last_feat_.r2 >= p_.trend_r2_min;
    const double trend_mag = (std::abs(last_feat_.slope) / std::max(1e-12, p_.trend_slope_min)) * last_feat_.r2;

    // 4) Precedence: HIGH_VOL -> LOW_VOL -> TREND -> CHOP
    struct Candidate { MarketRegime r; double score; };
    std::vector<Candidate> cands;
    cands.push_back({ MarketRegime::HIGH_VOLATILITY, score_high_vol });
    cands.push_back({ MarketRegime::LOW_VOLATILITY,  score_low_vol  });
    if (trending) {
        cands.push_back({ last_feat_.slope > 0.0 ? MarketRegime::TRENDING_UP : MarketRegime::TRENDING_DOWN, trend_mag });
    } else {
        // Use inverse CHOP as weak score for choppy (higher chop -> more choppy, but we want positive score)
        const double chop_score = std::max(0.0, (last_feat_.chop - 50.0) / 50.0);
        cands.push_back({ MarketRegime::CHOPPY, chop_score });
    }

    // Pick best candidate
    auto best = std::max_element(cands.begin(), cands.end(), [](const auto& a, const auto& b){ return a.score < b.score; });
    MarketRegime proposed = best->r;
    double proposed_score = best->score;

    // 5) Hysteresis + cooldown
    if (cooldown_ > 0) --cooldown_;
    if (last_regime_.has_value()) {
        if (proposed != *last_regime_) {
            if (proposed_score < p_.hysteresis_margin && cooldown_ > 0) {
                return *last_regime_; // hold
            }
            // switch only if the new regime is clearly supported
            if (proposed_score < p_.hysteresis_margin) {
                return *last_regime_;
            }
        }
    }

    if (!last_regime_.has_value() || proposed != *last_regime_) {
        last_regime_ = proposed;
        cooldown_ = p_.cooldown_bars;
    }
    return *last_regime_;
}

std::string MarketRegimeDetector::regime_to_string(MarketRegime regime) {
    switch (regime) {
        case MarketRegime::TRENDING_UP: return "TRENDING_UP";
        case MarketRegime::TRENDING_DOWN: return "TRENDING_DOWN";
        case MarketRegime::CHOPPY: return "CHOPPY";
        case MarketRegime::HIGH_VOLATILITY: return "HIGH_VOLATILITY";
        case MarketRegime::LOW_VOLATILITY: return "LOW_VOLATILITY";
        default: return "UNKNOWN";
    }
}

} // namespace sentio

```

## 📄 **FILE 11 of 14**: src/strategy/multi_symbol_oes_manager.cpp

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

## 📄 **FILE 12 of 14**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`
- **Size**: 580 lines
- **Modified**: 2025-10-16 22:55:00
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
    size_t num_features = feature_engine_->features_size();
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
        std::vector<double> features = feature_engine_->features_vector();

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

    return feature_engine_->features_vector();
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

## 📄 **FILE 13 of 14**: src/strategy/regime_parameter_manager.cpp

**File Information**:
- **Path**: `src/strategy/regime_parameter_manager.cpp`
- **Size**: 153 lines
- **Modified**: 2025-10-08 07:43:00
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "strategy/regime_parameter_manager.h"
#include "strategy/online_ensemble_strategy.h"
#include <fstream>
#include <iostream>

namespace sentio {

RegimeParameterManager::RegimeParameterManager() {
    load_default_params();
}

RegimeParams RegimeParameterManager::get_params_for_regime(MarketRegime regime) const {
    auto it = regime_params_.find(regime);
    if (it != regime_params_.end()) {
        return it->second;
    }

    // Fallback to CHOPPY params if regime not found
    auto fallback = regime_params_.find(MarketRegime::CHOPPY);
    if (fallback != regime_params_.end()) {
        return fallback->second;
    }

    // Ultimate fallback: default constructor
    return RegimeParams();
}

void RegimeParameterManager::set_params_for_regime(MarketRegime regime, const RegimeParams& params) {
    if (params.is_valid()) {
        regime_params_[regime] = params;
    } else {
        std::cerr << "Warning: Invalid parameters for regime "
                  << MarketRegimeDetector::regime_to_string(regime) << std::endl;
    }
}

void RegimeParameterManager::load_default_params() {
    init_trending_up_params();
    init_trending_down_params();
    init_choppy_params();
    init_high_volatility_params();
    init_low_volatility_params();
}

// Default parameters for TRENDING_UP regime
// Optimized for capturing upward momentum
void RegimeParameterManager::init_trending_up_params() {
    RegimeParams params(
        0.55,   // buy_threshold (wide gap to capture trends)
        0.43,   // sell_threshold
        0.992,  // ewrls_lambda (moderate adaptation)
        0.08,   // bb_amplification_factor
        0.15,   // h1_weight (favor longer horizons in trends)
        0.60,   // h5_weight
        0.25,   // h10_weight
        20,     // bb_period
        2.25,   // bb_std_dev
        0.30,   // bb_proximity
        0.016   // regularization
    );
    regime_params_[MarketRegime::TRENDING_UP] = params;
}

// Default parameters for TRENDING_DOWN regime
// Similar to trending up but with slightly different thresholds
void RegimeParameterManager::init_trending_down_params() {
    RegimeParams params(
        0.56,   // buy_threshold (slightly higher to avoid catching falling knives)
        0.42,   // sell_threshold (more aggressive shorts)
        0.992,  // ewrls_lambda
        0.08,   // bb_amplification_factor
        0.15,   // h1_weight
        0.60,   // h5_weight
        0.25,   // h10_weight
        20,     // bb_period
        2.25,   // bb_std_dev
        0.30,   // bb_proximity
        0.016   // regularization
    );
    regime_params_[MarketRegime::TRENDING_DOWN] = params;
}

// Default parameters for CHOPPY regime
// Narrower thresholds to avoid whipsaws
void RegimeParameterManager::init_choppy_params() {
    RegimeParams params(
        0.57,   // buy_threshold (narrower gap to reduce trades)
        0.45,   // sell_threshold
        0.995,  // ewrls_lambda (slower adaptation in noise)
        0.05,   // bb_amplification_factor (less aggressive)
        0.20,   // h1_weight (favor shorter horizons in chop)
        0.50,   // h5_weight
        0.30,   // h10_weight
        25,     // bb_period (longer period for smoothing)
        2.5,    // bb_std_dev (wider bands)
        0.35,   // bb_proximity
        0.025   // regularization (higher to reduce overfitting)
    );
    regime_params_[MarketRegime::CHOPPY] = params;
}

// Default parameters for HIGH_VOLATILITY regime
// Wider thresholds and faster adaptation
void RegimeParameterManager::init_high_volatility_params() {
    RegimeParams params(
        0.58,   // buy_threshold (wider gap for volatile moves)
        0.40,   // sell_threshold
        0.990,  // ewrls_lambda (faster adaptation)
        0.12,   // bb_amplification_factor (more aggressive)
        0.25,   // h1_weight (favor short horizons in volatility)
        0.45,   // h5_weight
        0.30,   // h10_weight
        15,     // bb_period (shorter for responsiveness)
        2.0,    // bb_std_dev
        0.25,   // bb_proximity
        0.010   // regularization (lower to be more responsive)
    );
    regime_params_[MarketRegime::HIGH_VOLATILITY] = params;
}

// Default parameters for LOW_VOLATILITY regime
// Tighter thresholds, more conservative
void RegimeParameterManager::init_low_volatility_params() {
    RegimeParams params(
        0.54,   // buy_threshold (tighter gap for small moves)
        0.46,   // sell_threshold
        0.996,  // ewrls_lambda (very slow adaptation)
        0.04,   // bb_amplification_factor (conservative)
        0.20,   // h1_weight
        0.50,   // h5_weight
        0.30,   // h10_weight
        30,     // bb_period (longer for stable regime)
        2.5,    // bb_std_dev
        0.40,   // bb_proximity
        0.030   // regularization (higher for stability)
    );
    regime_params_[MarketRegime::LOW_VOLATILITY] = params;
}

bool RegimeParameterManager::load_from_file(const std::string& config_path) {
    // TODO: Implement JSON/YAML config file loading
    // For now, just use defaults
    std::cerr << "Config file loading not yet implemented: " << config_path << std::endl;
    return false;
}

bool RegimeParameterManager::save_to_file(const std::string& config_path) const {
    // TODO: Implement JSON/YAML config file saving
    std::cerr << "Config file saving not yet implemented: " << config_path << std::endl;
    return false;
}

} // namespace sentio

```

## 📄 **FILE 14 of 14**: src/strategy/signal_output.cpp

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

