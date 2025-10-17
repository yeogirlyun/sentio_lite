# BUG_REPORT_NO_TRADES_DESPITE_SIGNALS - Complete Analysis

**Generated**: 2025-10-17 02:08:51
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: /Volumes/ExternalSSD/Dev/C++/online_trader/BUG_REPORT_NO_TRADES_DESPITE_SIGNALS.md
**Total Files**: 14

---

## 📋 **TABLE OF CONTENTS**

1. [config/rotation_strategy.json](#file-1)
2. [include/backend/rotation_signal_scorer.h](#file-2)
3. [include/common/types.h](#file-3)
4. [include/features/unified_feature_engine.h](#file-4)
5. [include/learning/online_predictor.h](#file-5)
6. [include/strategy/online_ensemble_strategy.h](#file-6)
7. [include/strategy/rotation_position_manager.h](#file-7)
8. [include/strategy/signal_aggregator.h](#file-8)
9. [include/strategy/signal_output.h](#file-9)
10. [src/backend/rotation_signal_scorer.cpp](#file-10)
11. [src/backend/rotation_trading_backend.cpp](#file-11)
12. [src/learning/online_predictor.cpp](#file-12)
13. [src/strategy/multi_symbol_oes_manager.cpp](#file-13)
14. [src/strategy/rotation_position_manager.cpp](#file-14)

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

## 📄 **FILE 2 of 14**: include/backend/rotation_signal_scorer.h

**File Information**:
- **Path**: `include/backend/rotation_signal_scorer.h`
- **Size**: 241 lines
- **Modified**: 2025-10-17 01:42:23
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include "strategy/signal_output.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

namespace sentio {

/**
 * @brief Unified signal strength scoring system for rotation trading
 *
 * Combines probability distance, model confidence from covariance,
 * expected magnitude, and historical reliability into a single
 * comparable rotation score for optimal position selection.
 */
class RotationSignalScorer {
public:
    struct ThresholdPair {
        double buy_threshold;
        double sell_threshold;
    };

    struct PredictionMetrics {
        double predicted_return;
        double prediction_variance;      // From covariance diagonal
        double model_convergence;        // From trace of P matrix
        double feature_stability;        // From off-diagonal correlations
    };

    struct SymbolProfile {
        double daily_volatility;
        double policy_boost;             // From config "leverage_boosts"
        std::string sector;
        bool has_decay;                  // True for inverse/leveraged ETFs
    };

    struct SymbolSignalScore {
        std::string symbol;

        // Decomposed scores for transparency
        double technical_score;          // Pure model-based (0-1)
        double policy_boost;             // From config (0.8-1.6)
        double expected_profit;          // Technical × policy × magnitude
        double risk_adjusted_score;      // Adjusted for volatility/correlation
        double rotation_score;           // Final comparable score

        // Components
        double raw_probability;          // Model output
        double threshold_distance;       // How far from threshold (normalized)
        double model_confidence;         // From covariance matrix
        double reliability_factor;       // Historical accuracy
        SignalType signal_type;          // LONG/SHORT/NEUTRAL

        bool operator<(const SymbolSignalScore& other) const {
            return rotation_score < other.rotation_score;
        }

        bool operator>(const SymbolSignalScore& other) const {
            return rotation_score > other.rotation_score;
        }
    };

    struct ScoringConfig {
        double technical_weight;
        double policy_weight;
        double min_technical_threshold;
        bool boost_only_signals;
        double correlation_penalty_weight;

        ScoringConfig()
            : technical_weight(0.7)
            , policy_weight(0.3)
            , min_technical_threshold(0.3)
            , boost_only_signals(true)
            , correlation_penalty_weight(0.1) {}
    };

    RotationSignalScorer();
    RotationSignalScorer(const ScoringConfig& config);

    // Main scoring interface
    std::vector<SymbolSignalScore> score_all_signals(
        const std::map<std::string, SignalOutput>& signals,
        const std::map<std::string, ThresholdPair>& thresholds,
        const std::map<std::string, PredictionMetrics>& predictions);

    // Update symbol profile (volatility, leverage, etc.)
    void update_symbol_profile(const std::string& symbol, const SymbolProfile& profile);

    // Update reliability tracking with realized performance
    void update_symbol_performance(const std::string& symbol, bool correct, double return_val);

    // Get current performance stats for a symbol
    struct SymbolPerformance {
        int total_predictions = 0;
        int correct_predictions = 0;
        double avg_return = 0.0;
        double return_std = 0.01;
        double prediction_variance = 0.1;

        void update(bool correct, double return_val);
        double get_accuracy() const;
    };

    const SymbolPerformance& get_symbol_performance(const std::string& symbol) const;

private:
    // Calculate individual score for one symbol
    SymbolSignalScore calculate_rotation_score(
        const std::string& symbol,
        const SignalOutput& signal,
        const ThresholdPair& thresholds,
        const PredictionMetrics& prediction);

    // Pure technical scoring (model-based, no policy)
    double calculate_technical_score(
        double threshold_distance,
        double model_confidence,
        double reliability_factor);

    // Expected profit calculation
    double calculate_expected_profit(
        double technical_score,
        double policy_boost,
        const PredictionMetrics& pred,
        const std::string& symbol);

    // Risk adjustments
    double apply_risk_adjustments(
        double expected_profit,
        const std::string& symbol);

    // Final score combining technical + policy
    double compute_final_score(const SymbolSignalScore& s);

    // Scoring components
    double calculate_threshold_distance(double probability, const ThresholdPair& thresholds);
    double calculate_model_confidence(const PredictionMetrics& pred);
    double get_symbol_reliability(const std::string& symbol);
    SignalType determine_signal_type(double probability, const ThresholdPair& thresholds);

    // Helper functions
    double calculate_correlation_penalty(const std::string& symbol) const;
    double sigmoid(double x);

    // State tracking
    ScoringConfig config_;
    std::map<std::string, SymbolPerformance> symbol_performance_;
    std::map<std::string, SymbolProfile> symbol_profiles_;
    std::set<std::string> current_positions_;  // For correlation penalty
};

/**
 * @brief Rotation decision manager
 *
 * Makes rotation decisions based on unified signal scores,
 * applying correlation filtering and position sizing logic.
 */
class RotationDecisionManager {
public:
    struct RotationDecision {
        std::vector<std::string> enter_symbols;
        std::vector<std::string> exit_symbols;
        std::map<std::string, double> position_sizes;
        std::map<std::string, RotationSignalScorer::SymbolSignalScore> symbol_scores;
        std::string reasoning;
    };

    struct Position {
        std::string symbol;
        double size;
        double entry_price;
        int bars_held;
        SignalType direction;
    };

    RotationDecisionManager(RotationSignalScorer& scorer, int max_positions = 3);

    // Main decision interface
    RotationDecision make_rotation_decision(
        const std::map<std::string, SignalOutput>& signals,
        const std::map<std::string, RotationSignalScorer::ThresholdPair>& thresholds,
        const std::map<std::string, RotationSignalScorer::PredictionMetrics>& predictions,
        const std::map<std::string, Position>& current_positions);

    // Configuration
    void set_min_rotation_score(double score) { min_rotation_score_ = score; }
    void set_max_positions(int max) { max_positions_ = max; }
    void set_improvement_threshold(double thresh) { improvement_threshold_ = thresh; }

    // Correlation matrix for diversification
    void set_correlation(const std::string& sym1, const std::string& sym2, double corr);
    double get_correlation(const std::string& sym1, const std::string& sym2) const;

private:
    // Filtering and selection
    std::vector<RotationSignalScorer::SymbolSignalScore> apply_correlation_filter(
        const std::vector<RotationSignalScorer::SymbolSignalScore>& scores);

    std::vector<RotationSignalScorer::SymbolSignalScore> apply_sector_diversification(
        const std::vector<RotationSignalScorer::SymbolSignalScore>& scores);

    // Position sizing
    double calculate_position_size(const RotationSignalScorer::SymbolSignalScore& score);

    // Rotation logic
    bool should_rotate(
        const Position& current,
        const RotationSignalScorer::SymbolSignalScore& candidate);

    Position find_weakest_position(
        const std::map<std::string, Position>& positions,
        const std::vector<RotationSignalScorer::SymbolSignalScore>& qualified);

    double get_current_score(const std::string& symbol) const;

    // Reasoning generation
    std::string generate_reasoning(
        const std::vector<RotationSignalScorer::SymbolSignalScore>& all_scores,
        const std::vector<RotationSignalScorer::SymbolSignalScore>& qualified,
        const RotationDecision& decision);

    RotationSignalScorer& scorer_;
    int max_positions_;
    double min_rotation_score_;
    double improvement_threshold_;
    double max_correlation_;

    // Correlation matrix
    std::map<std::pair<std::string, std::string>, double> correlations_;

    // Cache of current scores for rotation decisions
    mutable std::map<std::string, double> current_scores_;
};

} // namespace sentio

```

## 📄 **FILE 3 of 14**: include/common/types.h

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

## 📄 **FILE 4 of 14**: include/features/unified_feature_engine.h

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

## 📄 **FILE 5 of 14**: include/learning/online_predictor.h

**File Information**:
- **Path**: `include/learning/online_predictor.h`
- **Size**: 159 lines
- **Modified**: 2025-10-17 01:45:38
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

    // Symbol-specific initialization (forward declaration to avoid circular dependency)
    struct SymbolProfile;
    void initialize_with_symbol_profile(const Eigen::VectorXd& feature_stds, double volatility_scaling, double suggested_lambda);

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);
    
    // Diagnostics
    double get_recent_rmse() const;
    double get_directional_accuracy() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }

    // Covariance accessor for threshold adaptation
    std::vector<double> get_covariance_diagonal() const;
    
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

    // Covariance accessor for threshold adaptation (weighted ensemble average)
    std::vector<double> get_covariance_diagonal() const;

    // Comprehensive prediction metrics for unified scoring
    struct PredictionMetrics {
        double predicted_return;
        double prediction_variance;      // Avg diagonal of P matrix
        double model_convergence;        // 1.0 / (1.0 + trace(P))
        double feature_stability;        // Based on off-diagonal correlations

        PredictionMetrics()
            : predicted_return(0.0)
            , prediction_variance(0.1)
            , model_convergence(0.5)
            , feature_stability(0.5) {}
    };

    PredictionMetrics get_prediction_metrics(const std::vector<double>& features) const;

private:
    size_t num_features_;
    std::vector<std::unique_ptr<OnlinePredictor>> predictors_;
    std::vector<HorizonConfig> configs_;
};

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 6 of 14**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`
- **Size**: 245 lines
- **Modified**: 2025-10-17 01:01:27
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
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

    // Predictor access (for threshold adaptation)
    learning::MultiHorizonPredictor* get_predictor() {
        return ensemble_predictor_.get();
    }
    const learning::MultiHorizonPredictor* get_predictor() const {
        return ensemble_predictor_.get();
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

    // Private methods
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    void cleanup_stale_pending_updates(int current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);

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

## 📄 **FILE 7 of 14**: include/strategy/rotation_position_manager.h

**File Information**:
- **Path**: `include/strategy/rotation_position_manager.h`
- **Size**: 260 lines
- **Modified**: 2025-10-15 13:27:52
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/signal_aggregator.h"
#include "common/types.h"
#include <vector>
#include <string>
#include <map>
#include <set>

namespace sentio {

/**
 * @brief Simple rotation-based position manager
 *
 * Replaces the 7-state Position State Machine with a simpler rotation strategy:
 * 1. Hold top N signals (default: 2-3)
 * 2. When new signal ranks higher, rotate out lowest
 * 3. Exit positions that fall below rank threshold
 *
 * Design Principle:
 * "Capital flows to the strongest signals"
 *
 * This is 80% simpler than PSM (~300 lines vs 800 lines):
 * - No complex state transitions
 * - No entry/exit/reentry logic
 * - Just: "Is this signal in top N? Yes → hold, No → exit"
 *
 * Benefits:
 * - More responsive to signal changes
 * - Higher turnover = more opportunities
 * - Simpler to understand and debug
 * - Better MRD in multi-symbol rotation
 *
 * Usage:
 *   RotationPositionManager rpm(config);
 *   auto decisions = rpm.make_decisions(ranked_signals, current_positions);
 *   // decisions = {ENTER_LONG, EXIT, HOLD, etc.}
 */
class RotationPositionManager {
public:
    struct Config {
        int max_positions = 3;             // Hold top N signals (default: 3)
        int min_rank_to_hold = 5;          // Exit if rank falls below this
        double min_strength_to_enter = 0.50;  // Minimum strength to enter
        double min_strength_to_hold = 0.45;   // Minimum strength to hold (lower than entry)
        double min_strength_to_exit = 0.40;   // Minimum strength to exit (hysteresis)

        // Rotation thresholds
        double rotation_strength_delta = 0.10;  // New signal must be 10% stronger to rotate
        int rotation_cooldown_bars = 5;    // Wait N bars before rotating same symbol
        int minimum_hold_bars = 5;         // Minimum bars to hold position (anti-churning)

        // Position sizing
        bool equal_weight = true;          // Equal weight all positions
        bool volatility_weight = false;    // Weight by inverse volatility (future)
        double capital_per_position = 0.33;  // 33% per position (for 3 positions)

        // Risk management
        bool enable_profit_target = true;
        double profit_target_pct = 0.03;   // 3% profit target per position
        bool enable_stop_loss = true;
        double stop_loss_pct = 0.015;      // 1.5% stop loss per position

        // EOD liquidation
        bool eod_liquidation = true;       // Always exit at EOD (3:58 PM ET)
        int eod_exit_time_minutes = 358;   // 3:58 PM = minute 358 from 9:30 AM
    };

    /**
     * @brief Current position state
     */
    struct Position {
        std::string symbol;
        SignalType direction;     // LONG or SHORT
        double entry_price;
        double current_price;
        double pnl;              // Unrealized P&L
        double pnl_pct;          // Unrealized P&L %
        int bars_held;           // Bars since entry
        int minimum_hold_bars = 30;  // CRITICAL FIX: Minimum 30 bars (30 min) to prevent premature exits
        int entry_rank;          // Rank when entered
        int current_rank;        // Current rank
        double entry_strength;   // Strength when entered
        double current_strength; // Current strength
        uint64_t entry_timestamp_ms;
    };

    /**
     * @brief Position decision
     */
    enum class Decision {
        HOLD,           // Keep current position
        EXIT,           // Exit position
        ENTER_LONG,     // Enter new long position
        ENTER_SHORT,    // Enter new short position
        ROTATE_OUT,     // Exit to make room for better signal
        PROFIT_TARGET,  // Exit due to profit target
        STOP_LOSS,      // Exit due to stop loss
        EOD_EXIT        // Exit due to end-of-day
    };

    struct PositionDecision {
        std::string symbol;
        Decision decision;
        std::string reason;
        SignalAggregator::RankedSignal signal;  // Associated signal (if any)
        Position position;  // Associated position (if any)
    };

    explicit RotationPositionManager(const Config& config);
    ~RotationPositionManager() = default;

    /**
     * @brief Make position decisions based on ranked signals
     *
     * Core logic:
     * 1. Check existing positions for exit conditions
     * 2. Rank incoming signals
     * 3. Rotate if better signal available
     * 4. Enter new positions if slots available
     *
     * @param ranked_signals Ranked signals from SignalAggregator
     * @param current_prices Current prices for symbols
     * @param current_time_minutes Minutes since market open (for EOD check)
     * @return Vector of position decisions
     */
    std::vector<PositionDecision> make_decisions(
        const std::vector<SignalAggregator::RankedSignal>& ranked_signals,
        const std::map<std::string, double>& current_prices,
        int current_time_minutes = 0
    );

    /**
     * @brief Execute position decision (update internal state)
     *
     * @param decision Position decision
     * @param execution_price Price at which decision was executed
     * @return true if execution successful
     */
    bool execute_decision(const PositionDecision& decision, double execution_price);

    /**
     * @brief Update position prices
     *
     * Called each bar to update unrealized P&L.
     *
     * @param current_prices Current prices for all symbols
     */
    void update_prices(const std::map<std::string, double>& current_prices);

    /**
     * @brief Get current positions
     *
     * @return Map of symbol → position
     */
    const std::map<std::string, Position>& get_positions() const { return positions_; }

    /**
     * @brief Get position count
     *
     * @return Number of open positions
     */
    int get_position_count() const { return static_cast<int>(positions_.size()); }

    /**
     * @brief Check if symbol has position
     *
     * @param symbol Symbol ticker
     * @return true if position exists
     */
    bool has_position(const std::string& symbol) const {
        return positions_.count(symbol) > 0;
    }

    /**
     * @brief Get total unrealized P&L
     *
     * @return Total unrealized P&L across all positions
     */
    double get_total_unrealized_pnl() const;

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration
     */
    void update_config(const Config& new_config) { config_ = new_config; }

    /**
     * @brief Get statistics
     */
    struct Stats {
        int total_decisions;
        int holds;
        int exits;
        int entries;
        int rotations;
        int profit_targets;
        int stop_losses;
        int eod_exits;
        double avg_bars_held;
        double avg_pnl_pct;
    };

    Stats get_stats() const { return stats_; }
    void reset_stats() { stats_ = Stats(); }

private:
    /**
     * @brief Check if position should be exited
     *
     * @param position Position to check
     * @param ranked_signals Current ranked signals
     * @param current_time_minutes Minutes since market open
     * @return Decision (HOLD, EXIT, PROFIT_TARGET, STOP_LOSS, EOD_EXIT)
     */
    Decision check_exit_conditions(
        const Position& position,
        const std::vector<SignalAggregator::RankedSignal>& ranked_signals,
        int current_time_minutes
    );

    /**
     * @brief Find signal for symbol in ranked list
     *
     * @param symbol Symbol ticker
     * @param ranked_signals Ranked signals
     * @return Pointer to signal (nullptr if not found)
     */
    const SignalAggregator::RankedSignal* find_signal(
        const std::string& symbol,
        const std::vector<SignalAggregator::RankedSignal>& ranked_signals
    ) const;

    /**
     * @brief Check if rotation is needed
     *
     * @param ranked_signals Current ranked signals
     * @return true if rotation should occur
     */
    bool should_rotate(const std::vector<SignalAggregator::RankedSignal>& ranked_signals);

    /**
     * @brief Find weakest position to rotate out
     *
     * @return Symbol of weakest position
     */
    std::string find_weakest_position() const;

    Config config_;
    std::map<std::string, Position> positions_;
    Stats stats_;

    // Rotation cooldown tracking
    std::map<std::string, int> rotation_cooldown_;  // symbol → bars remaining
    std::map<std::string, int> exit_cooldown_;      // symbol → bars since exit (anti-churning)
    int current_bar_{0};
};

} // namespace sentio

```

## 📄 **FILE 8 of 14**: include/strategy/signal_aggregator.h

**File Information**:
- **Path**: `include/strategy/signal_aggregator.h`
- **Size**: 185 lines
- **Modified**: 2025-10-15 08:57:13
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/signal_output.h"
#include "common/types.h"
#include <vector>
#include <string>
#include <map>

namespace sentio {

/**
 * @brief Aggregates and ranks signals from multiple symbols
 *
 * Takes raw signals from 6 OES instances and:
 * 1. Applies leverage boost (1.5x for leveraged ETFs)
 * 2. Calculates signal strength: probability × confidence × leverage_boost
 * 3. Ranks signals by strength
 * 4. Filters by minimum strength threshold
 *
 * This is the CORE of the rotation strategy - the best signals win.
 *
 * Design Principle:
 * "Let the signals compete - highest strength gets capital"
 *
 * Usage:
 *   SignalAggregator aggregator(config);
 *   auto ranked = aggregator.rank_signals(all_signals);
 *   // Top N signals will be held by RotationPositionManager
 */
class SignalAggregator {
public:
    struct Config {
        // Leverage boost factors
        std::map<std::string, double> leverage_boosts = {
            {"TQQQ", 1.5},
            {"SQQQ", 1.5},
            {"UPRO", 1.5},
            {"SDS", 1.4},   // -2x, slightly less boost
            {"UVXY", 1.3},  // Volatility, more unpredictable
            {"SVIX", 1.3}
        };

        // Signal filtering
        double min_probability = 0.51;     // Minimum probability for consideration
        double min_confidence = 0.55;      // Minimum confidence for consideration
        double min_strength = 0.40;        // Minimum combined strength

        // Correlation filtering (future enhancement)
        bool enable_correlation_filter = false;
        double max_correlation = 0.85;     // Reject if correlation > 0.85

        // Signal quality thresholds
        bool filter_stale_signals = true;  // Filter signals from stale data
        double max_staleness_seconds = 120.0;  // Max 2 minutes old
    };

    /**
     * @brief Ranked signal with calculated strength
     */
    struct RankedSignal {
        std::string symbol;
        SignalOutput signal;
        double leverage_boost;      // Applied leverage boost factor
        double strength;            // probability × confidence × leverage_boost
        double staleness_weight;    // Staleness factor (1.0 = fresh, 0.0 = very old)
        int rank;                   // 1 = strongest, 2 = second, etc.

        // For sorting
        bool operator<(const RankedSignal& other) const {
            return strength > other.strength;  // Descending order
        }
    };

    explicit SignalAggregator(const Config& config);
    ~SignalAggregator() = default;

    /**
     * @brief Rank all signals by strength
     *
     * Applies leverage boost, calculates strength, filters weak signals,
     * and returns ranked list (strongest first).
     *
     * @param signals Map of symbol → signal
     * @param staleness_weights Optional staleness weights (from DataManager)
     * @return Vector of ranked signals (sorted by strength, descending)
     */
    std::vector<RankedSignal> rank_signals(
        const std::map<std::string, SignalOutput>& signals,
        const std::map<std::string, double>& staleness_weights = {}
    );

    /**
     * @brief Get top N signals
     *
     * @param ranked_signals Ranked signals (from rank_signals)
     * @param n Number of top signals to return
     * @return Top N signals
     */
    std::vector<RankedSignal> get_top_n(
        const std::vector<RankedSignal>& ranked_signals,
        int n
    ) const;

    /**
     * @brief Filter signals by direction (LONG or SHORT only)
     *
     * @param ranked_signals Ranked signals
     * @param direction Direction to filter (LONG or SHORT)
     * @return Filtered signals
     */
    std::vector<RankedSignal> filter_by_direction(
        const std::vector<RankedSignal>& ranked_signals,
        SignalType direction
    ) const;

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration
     */
    void update_config(const Config& new_config) { config_ = new_config; }

    /**
     * @brief Get configuration
     *
     * @return Current configuration
     */
    const Config& get_config() const { return config_; }

    /**
     * @brief Get statistics
     */
    struct Stats {
        int total_signals_processed;
        int signals_filtered;
        int signals_ranked;
        std::map<std::string, int> signals_per_symbol;
        double avg_strength;
        double max_strength;
    };

    Stats get_stats() const { return stats_; }
    void reset_stats() { stats_ = Stats(); }

private:
    /**
     * @brief Calculate signal strength with EMA smoothing
     *
     * @param symbol Symbol ticker (for EMA tracking)
     * @param signal Signal output
     * @param leverage_boost Leverage boost factor
     * @param staleness_weight Staleness weight (1.0 = fresh)
     * @return Combined strength score (smoothed)
     */
    double calculate_strength(
        const std::string& symbol,
        const SignalOutput& signal,
        double leverage_boost,
        double staleness_weight
    );

    /**
     * @brief Check if signal passes filters
     *
     * @param signal Signal output
     * @return true if signal passes all filters
     */
    bool passes_filters(const SignalOutput& signal) const;

    /**
     * @brief Get leverage boost for symbol
     *
     * @param symbol Symbol ticker
     * @return Leverage boost factor (1.0 if not found)
     */
    double get_leverage_boost(const std::string& symbol) const;

    Config config_;
    Stats stats_;
    int bars_processed_ = 0;  // Track bars for cold-start warmup
    std::map<std::string, double> smoothed_strengths_;  // EMA of strengths (anti-churning)
    double smoothing_alpha_ = 0.3;  // EMA factor (0.3 = 30% new, 70% old)
};

} // namespace sentio

```

## 📄 **FILE 9 of 14**: include/strategy/signal_output.h

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

## 📄 **FILE 10 of 14**: src/backend/rotation_signal_scorer.cpp

**File Information**:
- **Path**: `src/backend/rotation_signal_scorer.cpp`
- **Size**: 583 lines
- **Modified**: 2025-10-17 01:43:31
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "backend/rotation_signal_scorer.h"
#include "common/utils.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace sentio {

//==============================================================================
// SymbolPerformance Implementation
//==============================================================================

void RotationSignalScorer::SymbolPerformance::update(bool correct, double return_val) {
    total_predictions++;
    if (correct) correct_predictions++;

    // Update running statistics with exponential weighting
    double alpha = 0.05;
    avg_return = (1 - alpha) * avg_return + alpha * return_val;
    double variance = (return_val - avg_return) * (return_val - avg_return);
    return_std = std::sqrt((1 - alpha) * return_std * return_std + alpha * variance);
}

double RotationSignalScorer::SymbolPerformance::get_accuracy() const {
    if (total_predictions == 0) return 0.5;
    return static_cast<double>(correct_predictions) / total_predictions;
}

//==============================================================================
// RotationSignalScorer Implementation
//==============================================================================

RotationSignalScorer::RotationSignalScorer()
    : config_() {
    // Initialize with default config
}

RotationSignalScorer::RotationSignalScorer(const ScoringConfig& config)
    : config_(config) {
    // Initialize with provided config
}

std::vector<RotationSignalScorer::SymbolSignalScore>
RotationSignalScorer::score_all_signals(
    const std::map<std::string, SignalOutput>& signals,
    const std::map<std::string, ThresholdPair>& thresholds,
    const std::map<std::string, PredictionMetrics>& predictions) {

    std::vector<SymbolSignalScore> scores;

    for (const auto& [symbol, signal] : signals) {
        // Skip if we don't have thresholds or predictions for this symbol
        if (thresholds.find(symbol) == thresholds.end() ||
            predictions.find(symbol) == predictions.end()) {
            continue;
        }

        auto score = calculate_rotation_score(
            symbol, signal,
            thresholds.at(symbol),
            predictions.at(symbol)
        );

        // Only include non-neutral signals
        if (score.signal_type != SignalType::NEUTRAL) {
            scores.push_back(score);
        }
    }

    // Sort by rotation score (highest first)
    std::sort(scores.begin(), scores.end(),
        [](const SymbolSignalScore& a, const SymbolSignalScore& b) {
            return a.rotation_score > b.rotation_score;
        });

    return scores;
}

void RotationSignalScorer::update_symbol_profile(
    const std::string& symbol,
    const SymbolProfile& profile) {
    symbol_profiles_[symbol] = profile;
}

void RotationSignalScorer::update_symbol_performance(
    const std::string& symbol,
    bool correct,
    double return_val) {
    symbol_performance_[symbol].update(correct, return_val);
}

const RotationSignalScorer::SymbolPerformance&
RotationSignalScorer::get_symbol_performance(const std::string& symbol) const {
    static const SymbolPerformance default_perf;
    auto it = symbol_performance_.find(symbol);
    if (it == symbol_performance_.end()) {
        return default_perf;
    }
    return it->second;
}

RotationSignalScorer::SymbolSignalScore
RotationSignalScorer::calculate_rotation_score(
    const std::string& symbol,
    const SignalOutput& signal,
    const ThresholdPair& thresholds,
    const PredictionMetrics& prediction) {

    SymbolSignalScore score;
    score.symbol = symbol;
    score.raw_probability = signal.probability;

    // 1. Calculate threshold distance (normalized)
    score.threshold_distance = calculate_threshold_distance(
        signal.probability, thresholds);

    // 2. Extract model confidence from covariance
    score.model_confidence = calculate_model_confidence(prediction);

    // 3. Get historical reliability
    score.reliability_factor = get_symbol_reliability(symbol);

    // 4. Determine signal type
    score.signal_type = determine_signal_type(signal.probability, thresholds);

    // 5. Calculate PURE technical score (model only, no policy)
    score.technical_score = calculate_technical_score(
        score.threshold_distance,
        score.model_confidence,
        score.reliability_factor);

    // 6. Get policy boost from config
    auto it = symbol_profiles_.find(symbol);
    score.policy_boost = (it != symbol_profiles_.end()) ? it->second.policy_boost : 1.0;

    // 7. Calculate expected profit (combines technical + policy + magnitude)
    score.expected_profit = calculate_expected_profit(
        score.technical_score,
        score.policy_boost,
        prediction,
        symbol);

    // 8. Apply risk adjustments
    score.risk_adjusted_score = apply_risk_adjustments(
        score.expected_profit,
        symbol);

    // 9. Final rotation score
    score.rotation_score = score.risk_adjusted_score;

    return score;
}

double RotationSignalScorer::calculate_threshold_distance(
    double probability,
    const ThresholdPair& thresholds) {

    // Distance from threshold, normalized by gap size
    if (probability > thresholds.buy_threshold) {
        // Long signal: distance above buy threshold
        double distance = probability - thresholds.buy_threshold;
        return distance / (1.0 - thresholds.buy_threshold);  // Normalize to [0,1]

    } else if (probability < thresholds.sell_threshold) {
        // Short signal: distance below sell threshold
        double distance = thresholds.sell_threshold - probability;
        return distance / thresholds.sell_threshold;  // Normalize to [0,1]

    } else {
        // Neutral zone
        return 0.0;
    }
}

double RotationSignalScorer::calculate_model_confidence(
    const PredictionMetrics& pred) {

    // Combine multiple confidence factors from covariance matrix

    // 1. Prediction variance (lower = more confident)
    double variance_confidence = 1.0 / (1.0 + pred.prediction_variance);

    // 2. Model convergence (from trace of P matrix)
    double convergence_confidence = pred.model_convergence;

    // 3. Feature stability (from off-diagonal correlations)
    double stability_confidence = pred.feature_stability;

    // Weighted combination
    return 0.4 * variance_confidence +
           0.4 * convergence_confidence +
           0.2 * stability_confidence;
}

double RotationSignalScorer::calculate_technical_score(
    double threshold_distance,
    double model_confidence,
    double reliability_factor) {

    // Pure model-based score (NO volatility/leverage)
    // Range: 0 to ~1.0

    // Geometric mean of confidence factors
    double confidence_component = std::sqrt(model_confidence * reliability_factor);

    // Combine signal strength with confidence
    double technical_score = threshold_distance * confidence_component;

    return technical_score;
}

double RotationSignalScorer::calculate_expected_profit(
    double technical_score,
    double policy_boost,
    const PredictionMetrics& pred,
    const std::string& symbol) {

    // Don't boost weak signals
    if (technical_score < config_.min_technical_threshold) {
        return technical_score * 0.5;  // Penalty for weak signals
    }

    // Base expected return from technical score
    double base_return = technical_score * 0.01;  // Scale to 1% max base

    // Get symbol profile
    auto it = symbol_profiles_.find(symbol);
    if (it == symbol_profiles_.end()) {
        return base_return * policy_boost;
    }

    const auto& profile = it->second;

    // IMPORTANT: Only scale by volatility for NON-boosted symbols
    // Boosted symbols (TQQQ, UVXY) already encode leverage/volatility in boost factor
    double vol_mult = 1.0;
    if (policy_boost < 1.1) {  // Standard stock (no policy boost)
        vol_mult = profile.daily_volatility / 0.01;  // Scale by volatility
    }

    // Apply policy boost
    double boosted_return = base_return * policy_boost * vol_mult;

    // Decay penalty for inverse/leveraged ETFs
    if (profile.has_decay) {
        boosted_return *= 0.9;  // 10% penalty for time decay risk
    }

    return boosted_return;
}

double RotationSignalScorer::apply_risk_adjustments(
    double expected_profit,
    const std::string& symbol) {

    // Sharpe-like risk adjustment
    auto it = symbol_performance_.find(symbol);
    if (it != symbol_performance_.end() && it->second.total_predictions > 20) {
        double risk = it->second.return_std + 0.001;  // Avoid division by zero
        expected_profit /= risk;  // Risk-adjusted return
    }

    // Correlation penalty to avoid concentration
    double corr_penalty = calculate_correlation_penalty(symbol);
    expected_profit *= (1.0 - config_.correlation_penalty_weight * corr_penalty);

    return expected_profit;
}

double RotationSignalScorer::get_symbol_reliability(const std::string& symbol) {
    auto it = symbol_performance_.find(symbol);
    if (it == symbol_performance_.end() || it->second.total_predictions < 20) {
        return 0.5;  // Neutral for new symbols
    }

    const auto& history = it->second;

    // Combine multiple reliability metrics
    double accuracy = history.get_accuracy();

    double consistency = 1.0 - std::min(0.9, history.prediction_variance);

    // Sharpe-like metric for this symbol
    double risk_adjusted_return = history.avg_return / (history.return_std + 0.001);

    return 0.5 * accuracy +
           0.3 * consistency +
           0.2 * sigmoid(risk_adjusted_return);
}

SignalType RotationSignalScorer::determine_signal_type(
    double probability,
    const ThresholdPair& thresholds) {

    if (probability > thresholds.buy_threshold) {
        return SignalType::LONG;
    } else if (probability < thresholds.sell_threshold) {
        return SignalType::SHORT;
    } else {
        return SignalType::NEUTRAL;
    }
}

double RotationSignalScorer::compute_final_score(const SymbolSignalScore& s) {
    // Already computed as risk_adjusted_score
    // This is a pass-through for clarity
    return s.risk_adjusted_score;
}

double RotationSignalScorer::calculate_correlation_penalty(const std::string& symbol) const {
    // If we already hold correlated positions, penalize adding more
    // For now, simple heuristic based on symbol type

    if (current_positions_.empty()) {
        return 0.0;  // No penalty if no positions
    }

    // Count leveraged ETFs in current positions
    int leveraged_count = 0;
    for (const auto& pos : current_positions_) {
        if (pos.find("TQQ") != std::string::npos ||
            pos.find("SQQ") != std::string::npos ||
            pos.find("TNA") != std::string::npos ||
            pos.find("TZA") != std::string::npos ||
            pos.find("ERX") != std::string::npos ||
            pos.find("ERY") != std::string::npos ||
            pos.find("FAS") != std::string::npos ||
            pos.find("FAZ") != std::string::npos) {
            leveraged_count++;
        }
    }

    // If trying to add another leveraged ETF when we already have some
    bool is_leveraged = (symbol.find("TQQ") != std::string::npos ||
                        symbol.find("SQQ") != std::string::npos ||
                        symbol.find("TNA") != std::string::npos ||
                        symbol.find("TZA") != std::string::npos ||
                        symbol.find("ERX") != std::string::npos ||
                        symbol.find("ERY") != std::string::npos ||
                        symbol.find("FAS") != std::string::npos ||
                        symbol.find("FAZ") != std::string::npos);

    if (is_leveraged && leveraged_count > 0) {
        return std::min(0.5, leveraged_count * 0.2);  // 20% penalty per existing leveraged position
    }

    return 0.0;
}

double RotationSignalScorer::sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

//==============================================================================
// RotationDecisionManager Implementation
//==============================================================================

RotationDecisionManager::RotationDecisionManager(
    RotationSignalScorer& scorer,
    int max_positions)
    : scorer_(scorer),
      max_positions_(max_positions),
      min_rotation_score_(0.3),
      improvement_threshold_(0.2),
      max_correlation_(0.8) {
}

RotationDecisionManager::RotationDecision
RotationDecisionManager::make_rotation_decision(
    const std::map<std::string, SignalOutput>& signals,
    const std::map<std::string, RotationSignalScorer::ThresholdPair>& thresholds,
    const std::map<std::string, RotationSignalScorer::PredictionMetrics>& predictions,
    const std::map<std::string, Position>& current_positions) {

    RotationDecision decision;

    // 1. Score all signals
    auto scores = scorer_.score_all_signals(signals, thresholds, predictions);

    // Cache scores for later lookup
    current_scores_.clear();
    for (const auto& score : scores) {
        current_scores_[score.symbol] = score.rotation_score;
        decision.symbol_scores[score.symbol] = score;
    }

    // 2. Filter by minimum score threshold
    std::vector<RotationSignalScorer::SymbolSignalScore> qualified;
    for (const auto& score : scores) {
        if (score.rotation_score > min_rotation_score_) {
            qualified.push_back(score);
        }
    }

    // 3. Apply additional filters
    qualified = apply_correlation_filter(qualified);
    qualified = apply_sector_diversification(qualified);

    // 4. Select top N and make rotation decisions
    int to_select = std::min(max_positions_, static_cast<int>(qualified.size()));

    for (int i = 0; i < to_select; i++) {
        const auto& score = qualified[i];

        // Check if we need to rotate out of a position
        if (static_cast<int>(current_positions.size()) >= max_positions_) {
            auto weakest = find_weakest_position(current_positions, qualified);
            if (should_rotate(weakest, score)) {
                decision.exit_symbols.push_back(weakest.symbol);
                decision.enter_symbols.push_back(score.symbol);
            }
        } else {
            // We have room for a new position
            decision.enter_symbols.push_back(score.symbol);
        }

        // Calculate position size based on score
        decision.position_sizes[score.symbol] = calculate_position_size(score);
    }

    // 5. Generate reasoning
    decision.reasoning = generate_reasoning(scores, qualified, decision);

    return decision;
}

void RotationDecisionManager::set_correlation(
    const std::string& sym1,
    const std::string& sym2,
    double corr) {
    auto key = std::make_pair(
        std::min(sym1, sym2),
        std::max(sym1, sym2)
    );
    correlations_[key] = corr;
}

double RotationDecisionManager::get_correlation(
    const std::string& sym1,
    const std::string& sym2) const {

    auto key = std::make_pair(
        std::min(sym1, sym2),
        std::max(sym1, sym2)
    );

    auto it = correlations_.find(key);
    if (it != correlations_.end()) {
        return it->second;
    }

    // Default: assume moderate correlation for same type instruments
    return 0.5;
}

std::vector<RotationSignalScorer::SymbolSignalScore>
RotationDecisionManager::apply_correlation_filter(
    const std::vector<RotationSignalScorer::SymbolSignalScore>& scores) {

    std::vector<RotationSignalScorer::SymbolSignalScore> filtered;

    for (const auto& score : scores) {
        bool too_correlated = false;

        for (const auto& existing : filtered) {
            if (get_correlation(score.symbol, existing.symbol) > max_correlation_) {
                too_correlated = true;
                break;
            }
        }

        if (!too_correlated) {
            filtered.push_back(score);
        }
    }

    return filtered;
}

std::vector<RotationSignalScorer::SymbolSignalScore>
RotationDecisionManager::apply_sector_diversification(
    const std::vector<RotationSignalScorer::SymbolSignalScore>& scores) {

    // For now, pass through (could add sector limits later)
    return scores;
}

double RotationDecisionManager::calculate_position_size(
    const RotationSignalScorer::SymbolSignalScore& score) {

    // Kelly-inspired sizing based on confidence and expected return
    double kelly_fraction = score.model_confidence *
                           score.expected_profit /
                           (score.expected_profit + 0.01);

    // Scale by rotation score (use 1 / (1 + e^(-x)) sigmoid formula inline)
    double x = (score.rotation_score - 0.5) * 2;
    double score_multiplier = 1.0 / (1.0 + std::exp(-x));

    // Apply maximum position limits
    double base_size = 1.0 / max_positions_;  // Equal weight baseline
    double adjusted_size = base_size * (0.5 + score_multiplier);

    return std::min(0.5, adjusted_size);  // Cap at 50%
}

bool RotationDecisionManager::should_rotate(
    const Position& current,
    const RotationSignalScorer::SymbolSignalScore& candidate) {

    // Get current position's latest score
    double current_score = get_current_score(current.symbol);

    // Require significant improvement to rotate
    return (candidate.rotation_score > current_score * (1.0 + improvement_threshold_));
}

RotationDecisionManager::Position
RotationDecisionManager::find_weakest_position(
    const std::map<std::string, Position>& positions,
    const std::vector<RotationSignalScorer::SymbolSignalScore>& qualified) {

    Position weakest;
    double weakest_score = 1e9;

    for (const auto& [symbol, pos] : positions) {
        double score = get_current_score(symbol);
        if (score < weakest_score) {
            weakest_score = score;
            weakest = pos;
        }
    }

    return weakest;
}

double RotationDecisionManager::get_current_score(const std::string& symbol) const {
    auto it = current_scores_.find(symbol);
    if (it != current_scores_.end()) {
        return it->second;
    }
    return 0.0;  // Unknown symbols have zero score
}

std::string RotationDecisionManager::generate_reasoning(
    const std::vector<RotationSignalScorer::SymbolSignalScore>& all_scores,
    const std::vector<RotationSignalScorer::SymbolSignalScore>& qualified,
    const RotationDecision& decision) {

    std::ostringstream oss;

    oss << "Signal Scoring: " << all_scores.size() << " total, "
        << qualified.size() << " qualified (score>" << std::fixed
        << std::setprecision(2) << min_rotation_score_ << "). ";

    if (!decision.enter_symbols.empty()) {
        oss << "Entering: ";
        for (size_t i = 0; i < decision.enter_symbols.size(); i++) {
            const auto& sym = decision.enter_symbols[i];
            if (i > 0) oss << ", ";
            oss << sym;
            if (decision.symbol_scores.count(sym)) {
                oss << "(score=" << std::setprecision(2)
                    << decision.symbol_scores.at(sym).rotation_score << ")";
            }
        }
        oss << ". ";
    }

    if (!decision.exit_symbols.empty()) {
        oss << "Exiting: ";
        for (size_t i = 0; i < decision.exit_symbols.size(); i++) {
            if (i > 0) oss << ", ";
            oss << decision.exit_symbols[i];
        }
        oss << ". ";
    }

    return oss.str();
}

} // namespace sentio

```

## 📄 **FILE 11 of 14**: src/backend/rotation_trading_backend.cpp

**File Information**:
- **Path**: `src/backend/rotation_trading_backend.cpp`
- **Size**: 1384 lines
- **Modified**: 2025-10-17 02:00:53
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

    // Create unified signal scoring system
    RotationSignalScorer::ScoringConfig scorer_config;
    scorer_config.technical_weight = 0.7;
    scorer_config.policy_weight = 0.3;
    scorer_config.min_technical_threshold = 0.3;
    scorer_config.boost_only_signals = true;
    scorer_config.correlation_penalty_weight = 0.1;

    signal_scorer_ = std::make_unique<RotationSignalScorer>(scorer_config);
    utils::log_info("Created RotationSignalScorer (unified scoring system)");

    decision_manager_ = std::make_unique<RotationDecisionManager>(
        *signal_scorer_,
        config_.rotation_config.max_positions
    );
    decision_manager_->set_min_rotation_score(0.4);
    decision_manager_->set_improvement_threshold(0.15);
    utils::log_info("Created RotationDecisionManager");

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

        // Configure symbol profiles for unified scoring system
        configure_symbol_profiles(symbol_bars);
        utils::log_info("✓ Symbol profiles configured for unified scoring");
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

    // Open log files with buffering (15-20% I/O performance improvement)
    if (config_.log_all_signals) {
        signal_log_ = std::make_unique<BufferedLogWriter>(config_.signal_log_path);
        if (!signal_log_->is_open()) {
            utils::log_error("Failed to open signal log: " + config_.signal_log_path);
            std::cout << "❌ Failed to open signal log: " << config_.signal_log_path << std::endl;
            return false;
        }
    }

    if (config_.log_all_decisions) {
        decision_log_ = std::make_unique<BufferedLogWriter>(config_.decision_log_path);
        if (!decision_log_->is_open()) {
            utils::log_error("Failed to open decision log: " + config_.decision_log_path);
            std::cout << "❌ Failed to open decision log: " << config_.decision_log_path << std::endl;
            return false;
        }
    }

    trade_log_ = std::make_unique<BufferedLogWriter>(config_.trade_log_path);
    if (!trade_log_->is_open()) {
        utils::log_error("Failed to open trade log: " + config_.trade_log_path);
        std::cout << "❌ Failed to open trade log: " << config_.trade_log_path << std::endl;
        return false;
    }

    position_log_ = std::make_unique<BufferedLogWriter>(config_.position_log_path);
    if (!position_log_->is_open()) {
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

    // Flush buffered log files (auto-closed in destructor)
    if (signal_log_) signal_log_->flush();
    if (decision_log_) decision_log_->flush();
    if (trade_log_) trade_log_->flush();
    if (position_log_) position_log_->flush();

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
    // === UNIFIED SCORING SYSTEM INTEGRATION ===
    // NOTE: This is an INITIAL implementation that extracts prediction metrics
    // from the EWRLS ensemble. Future enhancements will add symbol profiles
    // and configure leverage boosts from config.

    // Step 1: Extract thresholds and prediction metrics for each symbol
    std::map<std::string, RotationSignalScorer::ThresholdPair> thresholds;
    std::map<std::string, RotationSignalScorer::PredictionMetrics> predictions;

    for (const auto& [symbol, signal] : signals) {
        // Get OES instance
        auto* oes = oes_manager_->get_oes_instance(symbol);
        if (!oes) {
            utils::log_warning("No OES instance for " + symbol);
            continue;
        }

        // Get predictor
        auto* predictor = oes->get_predictor();
        if (!predictor) {
            utils::log_warning("No predictor for " + symbol);
            continue;
        }

        // Get latest features from UnifiedFeatureEngine
        auto* feature_engine = oes->get_feature_engine();
        if (!feature_engine) {
            utils::log_warning("No feature engine for " + symbol);
            continue;
        }

        // Extract real features (not dummy!)
        std::vector<double> features = feature_engine->features_vector();

        // Validate feature count (should be 126 for full unified engine)
        if (features.empty()) {
            utils::log_warning("Empty feature vector for " + symbol);
            continue;
        }

        // Extract prediction metrics (includes covariance-based model confidence)
        auto pred_metrics = predictor->get_prediction_metrics(features);

        // Convert to scorer's PredictionMetrics format
        RotationSignalScorer::PredictionMetrics scorer_metrics;
        scorer_metrics.predicted_return = pred_metrics.predicted_return;
        scorer_metrics.prediction_variance = pred_metrics.prediction_variance;
        scorer_metrics.model_convergence = pred_metrics.model_convergence;
        scorer_metrics.feature_stability = pred_metrics.feature_stability;
        predictions[symbol] = scorer_metrics;

        // Extract adaptive thresholds from OES instance
        // Each OES maintains its own adaptive thresholds calibrated for win rate
        RotationSignalScorer::ThresholdPair threshold_pair;
        threshold_pair.buy_threshold = oes->get_current_buy_threshold();
        threshold_pair.sell_threshold = oes->get_current_sell_threshold();
        thresholds[symbol] = threshold_pair;
    }

    // Step 2: Use unified scoring system to score all signals
    auto scored_signals = signal_scorer_->score_all_signals(signals, thresholds, predictions);

    // Step 3: Sort by rotation score (descending)
    std::sort(scored_signals.begin(), scored_signals.end(),
             [](const auto& a, const auto& b) {
                 return a.rotation_score > b.rotation_score;
             });

    // Step 4: Convert to legacy RankedSignal format for compatibility
    std::vector<SignalAggregator::RankedSignal> ranked_signals;
    ranked_signals.reserve(scored_signals.size());

    for (size_t i = 0; i < scored_signals.size(); ++i) {
        const auto& scored = scored_signals[i];

        SignalAggregator::RankedSignal ranked;
        ranked.symbol = scored.symbol;

        // Copy the original signal
        auto sig_it = signals.find(scored.symbol);
        if (sig_it != signals.end()) {
            ranked.signal = sig_it->second;
        }

        // Use rotation_score as the strength metric
        ranked.strength = scored.rotation_score;
        ranked.leverage_boost = scored.policy_boost;
        ranked.staleness_weight = 1.0;  // TODO: Wire up staleness from data manager
        ranked.rank = static_cast<int>(i + 1);

        ranked_signals.push_back(ranked);
    }

    // Log top 3 scored signals for debugging
    int log_count = std::min(3, static_cast<int>(ranked_signals.size()));
    for (int i = 0; i < log_count; ++i) {
        const auto& ranked = ranked_signals[i];
        utils::log_info("[UNIFIED_SCORING] #" + std::to_string(i + 1) + ": " +
                       ranked.symbol + " strength=" + std::to_string(ranked.strength) +
                       " boost=" + std::to_string(ranked.leverage_boost));
    }

    return ranked_signals;
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
    if (!signal_log_ || !signal_log_->is_open()) {
        return;
    }

    json j;
    j["timestamp_ms"] = signal.timestamp_ms;
    j["bar_id"] = signal.bar_id;
    j["symbol"] = symbol;
    j["signal"] = static_cast<int>(signal.signal_type);
    j["probability"] = signal.probability;
    j["confidence"] = signal.confidence;

    signal_log_->write(j.dump());
}

void RotationTradingBackend::log_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!decision_log_ || !decision_log_->is_open()) {
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

    decision_log_->write(j.dump());
}

void RotationTradingBackend::log_trade(
    const RotationPositionManager::PositionDecision& decision,
    double execution_price,
    int shares,
    double realized_pnl,
    double realized_pnl_pct
) {
    if (!trade_log_ || !trade_log_->is_open()) {
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

    trade_log_->write(j.dump());
}

void RotationTradingBackend::log_positions() {
    if (!position_log_ || !position_log_->is_open()) {
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

    position_log_->write(j.dump());
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

// === Symbol Profile Configuration ===

void RotationTradingBackend::configure_symbol_profiles(
    const std::map<std::string, std::vector<Bar>>& symbol_bars
) {
    utils::log_info("Configuring symbol profiles for unified scoring...");

    for (const auto& [symbol, bars] : symbol_bars) {
        if (bars.size() < 20) {
            utils::log_warning("Insufficient data for " + symbol + " volatility calculation");
            continue;
        }

        // Calculate daily volatility from warmup bars
        std::vector<double> returns;
        returns.reserve(bars.size() - 1);

        for (size_t i = 1; i < bars.size(); ++i) {
            double ret = (bars[i].close - bars[i-1].close) / bars[i-1].close;
            returns.push_back(ret);
        }

        // Compute standard deviation
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double sq_sum = 0.0;
        for (double ret : returns) {
            sq_sum += (ret - mean) * (ret - mean);
        }
        double volatility = std::sqrt(sq_sum / returns.size());

        symbol_volatilities_[symbol] = volatility;

        // Get leverage boost from config (default to 1.0)
        double policy_boost = 1.0;
        auto boost_it = config_.leverage_boosts.find(symbol);
        if (boost_it != config_.leverage_boosts.end()) {
            policy_boost = boost_it->second;
        }

        // Configure symbol profile in scorer
        RotationSignalScorer::SymbolProfile profile;
        profile.daily_volatility = volatility;
        profile.policy_boost = policy_boost;
        profile.sector = "";  // TODO: Add sector classification
        profile.has_decay = is_leveraged_symbol(symbol);

        signal_scorer_->update_symbol_profile(symbol, profile);

        utils::log_info("  " + symbol + ": vol=" + std::to_string(volatility) +
                       ", boost=" + std::to_string(policy_boost) +
                       (profile.has_decay ? " [LEVERAGED]" : ""));
    }

    utils::log_info("Symbol profiles configured for " + std::to_string(symbol_volatilities_.size()) +
                   " symbols");
}

bool RotationTradingBackend::is_leveraged_symbol(const std::string& symbol) const {
    // Detect leveraged/inverse ETFs by common prefixes/suffixes
    // 2x/3x leveraged: TQQQ, SQQQ, UPRO, SPXU, TNA, TZA, FAS, FAZ, ERX, ERY, etc.
    // VIX: UVXY, SVXY, VXX, SVIX
    // Inverse: SDS, PSQ, SH, DOG, etc.

    static const std::vector<std::string> leveraged_symbols = {
        "TQQQ", "SQQQ", "UPRO", "SPXU", "TNA", "TZA",
        "FAS", "FAZ", "ERX", "ERY", "NUGT", "DUST",
        "JNUG", "JDST", "LABU", "LABD", "TECL", "TECS",
        "UDOW", "SDOW", "UMDD", "SMDD", "URTY", "SRTY",
        "SSO", "SDS", "PSQ", "QID", "SH", "DOG",
        "UVXY", "SVXY", "VXX", "SVIX", "VIXY"
    };

    return std::find(leveraged_symbols.begin(), leveraged_symbols.end(), symbol)
           != leveraged_symbols.end();
}

} // namespace sentio

```

## 📄 **FILE 12 of 14**: src/learning/online_predictor.cpp

**File Information**:
- **Path**: `src/learning/online_predictor.cpp`
- **Size**: 471 lines
- **Modified**: 2025-10-17 01:46:10
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

void OnlinePredictor::initialize_with_symbol_profile(
    const Eigen::VectorXd& feature_stds,
    double volatility_scaling,
    double suggested_lambda) {

    // Reinitialize covariance matrix with symbol-specific scaling
    P_ = Eigen::MatrixXd::Zero(num_features_, num_features_);

    for (size_t i = 0; i < num_features_; ++i) {
        // Scale diagonal by feature-specific variance
        double feature_variance = feature_stds[i] * feature_stds[i];

        // Combine with volatility scaling
        P_(i, i) = config_.initial_variance * feature_variance * volatility_scaling;

        // Ensure minimum uncertainty
        if (P_(i, i) < 1.0) {
            P_(i, i) = 1.0;
        }
    }

    // Update learning rate based on suggested lambda
    current_lambda_ = suggested_lambda;

    utils::log_info("Symbol-specific initialization: lambda=" + std::to_string(current_lambda_) +
                   ", vol_scaling=" + std::to_string(volatility_scaling));
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

std::vector<double> OnlinePredictor::get_covariance_diagonal() const {
    std::vector<double> diagonal(num_features_);
    for (size_t i = 0; i < num_features_; ++i) {
        diagonal[i] = P_(i, i);
    }
    return diagonal;
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

std::vector<double> MultiHorizonPredictor::get_covariance_diagonal() const {
    if (predictors_.empty()) {
        return std::vector<double>(num_features_, 100.0);  // Default high uncertainty
    }

    // Compute weighted average of covariance diagonals
    std::vector<double> weighted_diagonal(num_features_, 0.0);
    double total_weight = 0.0;

    for (size_t i = 0; i < predictors_.size(); ++i) {
        if (!predictors_[i]->is_ready()) continue;

        auto diagonal = predictors_[i]->get_covariance_diagonal();
        double weight = configs_[i].weight;

        for (size_t j = 0; j < num_features_; ++j) {
            weighted_diagonal[j] += diagonal[j] * weight;
        }
        total_weight += weight;
    }

    // Normalize by total weight
    if (total_weight > 0.0) {
        for (size_t j = 0; j < num_features_; ++j) {
            weighted_diagonal[j] /= total_weight;
        }
    } else {
        // No ready predictors yet, return high uncertainty
        std::fill(weighted_diagonal.begin(), weighted_diagonal.end(), 100.0);
    }

    return weighted_diagonal;
}

MultiHorizonPredictor::PredictionMetrics
MultiHorizonPredictor::get_prediction_metrics(const std::vector<double>& features) const {
    PredictionMetrics metrics;

    if (predictors_.empty()) {
        return metrics;  // Return defaults
    }

    // Get prediction for predicted_return
    double total_prediction = 0.0;
    double total_weight = 0.0;

    // Accumulate metrics from all ready predictors
    double total_variance = 0.0;
    double total_trace = 0.0;
    double total_off_diagonal = 0.0;
    int ready_count = 0;

    for (size_t i = 0; i < predictors_.size(); ++i) {
        if (!predictors_[i]->is_ready()) continue;

        // Get prediction
        auto pred = predictors_[i]->predict(features);
        double weight = configs_[i].weight;
        total_prediction += pred.predicted_return * weight;
        total_weight += weight;

        // Get covariance diagonal for variance estimate
        auto cov_diag = predictors_[i]->get_covariance_diagonal();
        double avg_variance = 0.0;
        for (double v : cov_diag) {
            avg_variance += v;
        }
        avg_variance /= cov_diag.size();
        total_variance += avg_variance * weight;

        // Trace is sum of diagonal (for convergence metric)
        double trace = 0.0;
        for (double v : cov_diag) {
            trace += v;
        }
        total_trace += trace * weight;

        // Feature stability: lower off-diagonals = more stable
        // Use simple heuristic: stability = 1.0 / (1.0 + avg_variance)
        double stability = 1.0 / (1.0 + avg_variance);
        total_off_diagonal += stability * weight;

        ready_count++;
    }

    if (total_weight > 0.0 && ready_count > 0) {
        metrics.predicted_return = total_prediction / total_weight;
        metrics.prediction_variance = total_variance / total_weight;
        metrics.model_convergence = 1.0 / (1.0 + (total_trace / total_weight) / num_features_);
        metrics.feature_stability = total_off_diagonal / total_weight;
    }

    return metrics;
}

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 13 of 14**: src/strategy/multi_symbol_oes_manager.cpp

**File Information**:
- **Path**: `src/strategy/multi_symbol_oes_manager.cpp`
- **Size**: 513 lines
- **Modified**: 2025-10-17 01:02:26
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

    // Initialize per-symbol threshold manager
    learning::SymbolAdaptiveThresholdManager::Config threshold_config;
    threshold_manager_ = std::make_unique<learning::SymbolAdaptiveThresholdManager>(threshold_config);
    utils::log_info("  Adaptive threshold manager initialized");

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

        // Get adaptive thresholds based on covariance uncertainty
        auto predictor = it->second->get_predictor();
        double symbol_vol = (symbol_volatilities_.count(symbol) > 0)
                           ? symbol_volatilities_.at(symbol)
                           : 0.20;  // 20% default
        int leverage = get_symbol_leverage(symbol);

        auto [buy_threshold, sell_threshold] = threshold_manager_->get_thresholds(
            symbol, predictor, symbol_vol, leverage
        );

        // Update OES thresholds with adaptive values
        // Get the base config for this symbol
        OnlineEnsembleStrategy::OnlineEnsembleConfig temp_config;
        if (config_.symbol_configs.count(symbol) > 0) {
            temp_config = config_.symbol_configs.at(symbol);
        } else {
            temp_config = config_.base_config;
        }

        // Set adaptive thresholds
        temp_config.buy_threshold = buy_threshold;
        temp_config.sell_threshold = sell_threshold;
        it->second->update_config(temp_config);

        // DEBUG: Log adaptive thresholds (first few times per symbol)
        static std::map<std::string, int> threshold_log_count;
        if (threshold_log_count[symbol] < 3) {
            utils::log_info("  " + symbol + ": Adaptive thresholds = [" +
                           std::to_string(buy_threshold) + ", " +
                           std::to_string(sell_threshold) + "], vol=" +
                           std::to_string(symbol_vol * 100.0) + "%, lev=" +
                           std::to_string(leverage) + "x");
            threshold_log_count[symbol]++;
        }

        // Generate signal with adaptive thresholds
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

    // Compute symbol volatility from warmup bars
    if (bars.size() >= 20) {
        // Compute log returns
        double sum_sq = 0.0;
        double mean = 0.0;
        std::vector<double> log_returns;
        log_returns.reserve(bars.size() - 1);

        for (size_t i = 1; i < bars.size(); ++i) {
            double log_ret = std::log(bars[i].close / bars[i-1].close);
            log_returns.push_back(log_ret);
            mean += log_ret;
        }
        mean /= log_returns.size();

        for (double ret : log_returns) {
            sum_sq += (ret - mean) * (ret - mean);
        }

        double daily_vol = std::sqrt(sum_sq / (log_returns.size() - 1));
        double annualized_vol = daily_vol * std::sqrt(252.0);

        symbol_volatilities_[symbol] = annualized_vol;
        utils::log_info("  " + symbol + ": Computed volatility = " +
                       std::to_string(annualized_vol * 100.0) + "%");
    } else {
        // Default to moderate volatility
        symbol_volatilities_[symbol] = 0.20;  // 20% default
    }

    // Feed bars one by one
    for (size_t i = 0; i < bars.size(); ++i) {
        it->second->on_bar(bars[i]);
    }

    // Check if ready
    bool ready = it->second->is_ready();
    if (ready) {
        utils::log_info("  " + symbol + ": Warmup complete - ready for trading");
    } else {
        utils::log_warning("  " + symbol + ": Warmup incomplete - needs more data");
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

int MultiSymbolOESManager::get_symbol_leverage(const std::string& symbol) const {
    // Detect leverage from symbol name
    // 3x leveraged ETFs
    if (symbol == "TQQQ" || symbol == "SQQQ" ||
        symbol == "UPRO" || symbol == "SPXU" ||
        symbol == "TNA" || symbol == "TZA" ||
        symbol == "FAS" || symbol == "FAZ" ||
        symbol == "NUGT" || symbol == "DUST" ||
        symbol == "ERX" || symbol == "ERY") {
        // Determine direction (long vs short)
        if (symbol == "TQQQ" || symbol == "UPRO" || symbol == "TNA" ||
            symbol == "FAS" || symbol == "NUGT" || symbol == "ERX") {
            return 3;   // 3x bullish
        } else {
            return -3;  // 3x bearish
        }
    }

    // 2x leveraged ETFs
    if (symbol == "SSO" || symbol == "SDS" ||
        symbol == "QLD" || symbol == "QID") {
        if (symbol == "SSO" || symbol == "QLD") {
            return 2;   // 2x bullish
        } else {
            return -2;  // 2x bearish
        }
    }

    // -1x inverse (non-leveraged short)
    if (symbol == "PSQ" || symbol == "SH" ||
        symbol == "DOG" || symbol == "RWM") {
        return -1;
    }

    // SVXY is special (-0.5x short VIX futures)
    if (symbol == "SVXY") {
        return -1;  // Treat as inverse
    }

    // UVXY is 1.5x long VIX futures (round to 2x for safety)
    if (symbol == "UVXY" || symbol == "VIXY") {
        return 2;  // High volatility, treat as leveraged
    }

    // Default: 1x (unleveraged long)
    return 1;
}

} // namespace sentio

```

## 📄 **FILE 14 of 14**: src/strategy/rotation_position_manager.cpp

**File Information**:
- **Path**: `src/strategy/rotation_position_manager.cpp`
- **Size**: 550 lines
- **Modified**: 2025-10-15 13:59:55
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "strategy/rotation_position_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>

namespace sentio {

RotationPositionManager::RotationPositionManager(const Config& config)
    : config_(config) {

    utils::log_info("RotationPositionManager initialized");
    utils::log_info("  Max positions: " + std::to_string(config_.max_positions));
    utils::log_info("  Min strength to enter: " + std::to_string(config_.min_strength_to_enter));
    utils::log_info("  Min strength to hold: " + std::to_string(config_.min_strength_to_hold));
    utils::log_info("  Rotation delta: " + std::to_string(config_.rotation_strength_delta));
}

std::vector<RotationPositionManager::PositionDecision>
RotationPositionManager::make_decisions(
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals,
    const std::map<std::string, double>& current_prices,
    int current_time_minutes
)  {
    // DIAGNOSTIC: Log every call to make_decisions
    int current_positions = get_position_count();
    utils::log_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    utils::log_info("make_decisions() CALLED - Bar " + std::to_string(current_bar_ + 1) +
                   ", Time: " + std::to_string(current_time_minutes) + "min" +
                   ", Current positions: " + std::to_string(current_positions) +
                   ", Max positions: " + std::to_string(config_.max_positions));
    utils::log_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    std::vector<PositionDecision> decisions;

    current_bar_++;
    stats_.total_decisions++;

    // Update exit cooldowns (decrement all)
    for (auto& [symbol, cooldown] : exit_cooldown_) {
        if (cooldown > 0) cooldown--;
    }

    // Step 1: Check existing positions for exit conditions
    std::set<std::string> symbols_to_exit;

    for (auto& [symbol, position] : positions_) {
        position.bars_held++;

        // Update current price
        if (current_prices.count(symbol) > 0) {
            position.current_price = current_prices.at(symbol);

            // Calculate P&L
            if (position.direction == SignalType::LONG) {
                position.pnl = position.current_price - position.entry_price;
                position.pnl_pct = position.pnl / position.entry_price;
            } else {  // SHORT
                position.pnl = position.entry_price - position.current_price;
                position.pnl_pct = position.pnl / position.entry_price;
            }
        }

        // Update current rank and strength
        const auto* signal = find_signal(symbol, ranked_signals);
        if (signal) {
            position.current_rank = signal->rank;
            position.current_strength = signal->strength;
            utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                           " signal found: rank=" + std::to_string(signal->rank) +
                           ", strength=" + std::to_string(signal->strength));
        } else {
            utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                           " signal NOT found in ranked list (" +
                           std::to_string(ranked_signals.size()) + " signals available)");

            // Don't immediately mark for exit - keep previous rank/strength
            // During cold-start (first 200 bars), don't decay - allow predictor to stabilize
            if (current_bar_ > 200) {
                utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                               " applying decay (post-warmup)");
                // Only decay strength gradually to allow time for signal to return
                position.current_strength *= 0.95;  // 5% decay per bar

                // Only mark for exit if strength decays below hold threshold
                if (position.current_strength < config_.min_strength_to_hold) {
                    position.current_rank = 9999;
                    utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                                   " strength fell below hold threshold -> marking for exit");
                }
            } else {
                utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                               " in warmup period - keeping previous rank/strength unchanged");
            }
            // Otherwise keep previous rank and strength unchanged during warmup
        }

        // Check exit conditions
        Decision decision = check_exit_conditions(position, ranked_signals, current_time_minutes);

        if (decision != Decision::HOLD) {
            PositionDecision pd;
            pd.symbol = symbol;
            pd.decision = decision;
            pd.position = position;

            switch (decision) {
                case Decision::EXIT:
                    pd.reason = "Rank fell below threshold (" + std::to_string(position.current_rank) + ")";
                    stats_.exits++;
                    break;
                case Decision::PROFIT_TARGET:
                    pd.reason = "Profit target hit (" + std::to_string(position.pnl_pct * 100.0) + "%)";
                    stats_.profit_targets++;
                    break;
                case Decision::STOP_LOSS:
                    pd.reason = "Stop loss hit (" + std::to_string(position.pnl_pct * 100.0) + "%)";
                    stats_.stop_losses++;
                    break;
                case Decision::EOD_EXIT:
                    pd.reason = "End of day liquidation";
                    stats_.eod_exits++;
                    break;
                default:
                    break;
            }

            decisions.push_back(pd);
            symbols_to_exit.insert(symbol);
        } else {
            // HOLD decision
            PositionDecision pd;
            pd.symbol = symbol;
            pd.decision = Decision::HOLD;
            pd.position = position;
            pd.reason = "Holding (rank=" + std::to_string(position.current_rank) +
                       ", strength=" + std::to_string(position.current_strength) + ")";
            decisions.push_back(pd);
            stats_.holds++;
        }
    }

    // CRITICAL FIX: Don't erase positions here!
    // execute_decision() will erase them after successful execution.
    // If we erase here, execute_decision() will fail because position doesn't exist!

    // Set exit cooldown for exited symbols
    for (const auto& symbol : symbols_to_exit) {
        exit_cooldown_[symbol] = 10;  // 10-bar cooldown after exit (anti-churning)
        utils::log_info("[EXIT DECISION] " + symbol + " marked for exit, cooldown set");
    }

    // Step 2: Consider new entries
    // Re-check position count before entries (may have changed due to exits)
    current_positions = get_position_count();

    // FIX 3: CRITICAL - Enforce max_positions hard limit
    if (current_positions >= config_.max_positions) {
        utils::log_info("╔══════════════════════════════════════════════════════════╗");
        utils::log_info("║ MAX POSITIONS REACHED - BLOCKING NEW ENTRIES            ║");
        utils::log_info("║ Current: " + std::to_string(current_positions) +
                       " / Max: " + std::to_string(config_.max_positions) + "                                      ║");
        utils::log_info("║ Returning " + std::to_string(decisions.size()) + " decisions (exits/holds only)              ║");
        utils::log_info("╚══════════════════════════════════════════════════════════╝");
        return decisions;  // Skip entire entry section
    }

    int available_slots = config_.max_positions - current_positions;

    // CRITICAL FIX: Prevent new entries near EOD to avoid immediate liquidation
    int bars_until_eod = config_.eod_exit_time_minutes - current_time_minutes;
    if (bars_until_eod <= 30 && available_slots > 0) {
        utils::log_info("╔══════════════════════════════════════════════════════════╗");
        utils::log_info("║ NEAR EOD - BLOCKING NEW ENTRIES                         ║");
        utils::log_info("║ Bars until EOD: " + std::to_string(bars_until_eod) +
                       " (< 30 bar minimum hold)                     ║");
        utils::log_info("║ Returning " + std::to_string(decisions.size()) + " decisions (exits/holds only)              ║");
        utils::log_info("╚══════════════════════════════════════════════════════════╝");
        available_slots = 0;  // Block all new entries
    }

    if (available_slots > 0) {
        // Find top signals not currently held
        for (const auto& ranked_signal : ranked_signals) {
            if (available_slots <= 0) {
                break;
            }

            const auto& symbol = ranked_signal.symbol;

            // Skip if already have position
            if (has_position(symbol)) {
                continue;
            }

            // Skip if in rotation cooldown
            if (rotation_cooldown_.count(symbol) > 0 && rotation_cooldown_[symbol] > 0) {
                rotation_cooldown_[symbol]--;
                continue;
            }

            // Skip if in exit cooldown (anti-churning)
            if (exit_cooldown_.count(symbol) > 0 && exit_cooldown_[symbol] > 0) {
                continue;  // Don't re-enter immediately after exit
            }

            // Check minimum strength
            if (ranked_signal.strength < config_.min_strength_to_enter) {
                break;  // Signals are sorted, so no point checking further
            }

            // Check minimum rank
            if (ranked_signal.rank > config_.min_rank_to_hold) {
                break;
            }

            // FIX 4: Enhanced position entry validation
            if (current_prices.count(symbol) == 0) {
                utils::log_error("[ENTRY VALIDATION] No price for " + symbol + " - cannot enter position");
                utils::log_error("  Available prices for " + std::to_string(current_prices.size()) + " symbols:");

                // List available symbols for debugging
                int count = 0;
                for (const auto& [sym, price] : current_prices) {
                    if (count++ < 10) {  // Show first 10
                        utils::log_error("    " + sym + " @ $" + std::to_string(price));
                    }
                }
                continue;
            }

            // Validate price is reasonable
            double price = current_prices.at(symbol);
            if (price <= 0.0 || price > 1000000.0) {  // Sanity check
                utils::log_error("[ENTRY VALIDATION] Invalid price for " + symbol + ": $" +
                               std::to_string(price) + " - skipping");
                continue;
            }

            // Enter position
            PositionDecision pd;
            pd.symbol = symbol;
            pd.decision = (ranked_signal.signal.signal_type == SignalType::LONG) ?
                         Decision::ENTER_LONG : Decision::ENTER_SHORT;
            pd.signal = ranked_signal;
            pd.reason = "Entering (rank=" + std::to_string(ranked_signal.rank) +
                       ", strength=" + std::to_string(ranked_signal.strength) + ")";

            utils::log_info("[ENTRY] " + symbol + " @ $" + std::to_string(price) +
                          " (rank=" + std::to_string(ranked_signal.rank) +
                          ", strength=" + std::to_string(ranked_signal.strength) + ")");

            utils::log_info(">>> ADDING ENTRY DECISION: " + symbol +
                          " (decision #" + std::to_string(decisions.size() + 1) + ")" +
                          ", available_slots=" + std::to_string(available_slots) +
                          " -> " + std::to_string(available_slots - 1));

            decisions.push_back(pd);
            stats_.entries++;

            available_slots--;
        }
    }

    // Step 3: Check if rotation needed (better signal available)
    if (available_slots == 0 && should_rotate(ranked_signals)) {
        // Find weakest current position
        std::string weakest = find_weakest_position();

        if (!weakest.empty()) {
            // Find strongest non-held signal
            for (const auto& ranked_signal : ranked_signals) {
                if (has_position(ranked_signal.symbol)) {
                    continue;
                }

                // Check if significantly stronger
                auto& weakest_pos = positions_.at(weakest);
                double strength_delta = ranked_signal.strength - weakest_pos.current_strength;

                if (strength_delta >= config_.rotation_strength_delta) {
                    // Rotate out weakest
                    PositionDecision exit_pd;
                    exit_pd.symbol = weakest;
                    exit_pd.decision = Decision::ROTATE_OUT;
                    exit_pd.position = weakest_pos;
                    exit_pd.reason = "Rotating out for stronger signal (" +
                                    ranked_signal.symbol + ", delta=" +
                                    std::to_string(strength_delta) + ")";
                    decisions.push_back(exit_pd);
                    stats_.rotations++;

                    // CRITICAL FIX: Don't erase here! Let execute_decision() handle it.
                    // positions_.erase(weakest);  // ← REMOVED
                    utils::log_info("[ROTATION] " + weakest + " marked for rotation out");

                    // Enter new position
                    PositionDecision enter_pd;
                    enter_pd.symbol = ranked_signal.symbol;
                    enter_pd.decision = (ranked_signal.signal.signal_type == SignalType::LONG) ?
                                       Decision::ENTER_LONG : Decision::ENTER_SHORT;
                    enter_pd.signal = ranked_signal;
                    enter_pd.reason = "Entering via rotation (rank=" +
                                     std::to_string(ranked_signal.rank) +
                                     ", strength=" + std::to_string(ranked_signal.strength) + ")";
                    decisions.push_back(enter_pd);
                    stats_.entries++;

                    // Set cooldown for rotated symbol
                    rotation_cooldown_[weakest] = config_.rotation_cooldown_bars;

                    break;  // Only rotate one per bar
                }
            }
        }
    }

    // DIAGNOSTIC: Log all decisions being returned
    utils::log_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    utils::log_info("make_decisions() RETURNING " + std::to_string(decisions.size()) + " decisions:");
    for (size_t i = 0; i < decisions.size(); i++) {
        const auto& d = decisions[i];
        std::string decision_type;
        switch (d.decision) {
            case Decision::ENTER_LONG: decision_type = "ENTER_LONG"; break;
            case Decision::ENTER_SHORT: decision_type = "ENTER_SHORT"; break;
            case Decision::EXIT: decision_type = "EXIT"; break;
            case Decision::HOLD: decision_type = "HOLD"; break;
            case Decision::ROTATE_OUT: decision_type = "ROTATE_OUT"; break;
            case Decision::PROFIT_TARGET: decision_type = "PROFIT_TARGET"; break;
            case Decision::STOP_LOSS: decision_type = "STOP_LOSS"; break;
            case Decision::EOD_EXIT: decision_type = "EOD_EXIT"; break;
            default: decision_type = "UNKNOWN"; break;
        }
        utils::log_info("  [" + std::to_string(i+1) + "] " + d.symbol + ": " + decision_type);
    }
    utils::log_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    return decisions;
}

bool RotationPositionManager::execute_decision(
    const PositionDecision& decision,
    double execution_price
) {
    switch (decision.decision) {
        case Decision::ENTER_LONG:
        case Decision::ENTER_SHORT:
            {
                Position pos;
                pos.symbol = decision.symbol;
                pos.direction = decision.signal.signal.signal_type;
                pos.entry_price = execution_price;
                pos.current_price = execution_price;
                pos.pnl = 0.0;
                pos.pnl_pct = 0.0;
                pos.bars_held = 0;
                pos.entry_rank = decision.signal.rank;
                pos.current_rank = decision.signal.rank;
                pos.entry_strength = decision.signal.strength;
                pos.current_strength = decision.signal.strength;
                pos.entry_timestamp_ms = decision.signal.signal.timestamp_ms;

                positions_[decision.symbol] = pos;

                utils::log_info("Entered " + decision.symbol + " " +
                              (pos.direction == SignalType::LONG ? "LONG" : "SHORT") +
                              " @ " + std::to_string(execution_price));
                return true;
            }

        case Decision::EXIT:
        case Decision::ROTATE_OUT:
        case Decision::PROFIT_TARGET:
        case Decision::STOP_LOSS:
        case Decision::EOD_EXIT:
            {
                if (positions_.count(decision.symbol) > 0) {
                    auto& pos = positions_.at(decision.symbol);

                    // Calculate final P&L
                    double final_pnl_pct = 0.0;
                    if (pos.direction == SignalType::LONG) {
                        final_pnl_pct = (execution_price - pos.entry_price) / pos.entry_price;
                    } else {
                        final_pnl_pct = (pos.entry_price - execution_price) / pos.entry_price;
                    }

                    utils::log_info("Exited " + decision.symbol + " " +
                                  (pos.direction == SignalType::LONG ? "LONG" : "SHORT") +
                                  " @ " + std::to_string(execution_price) +
                                  " (P&L: " + std::to_string(final_pnl_pct * 100.0) + "%, " +
                                  "bars: " + std::to_string(pos.bars_held) + ")");

                    // Update stats
                    stats_.avg_bars_held = (stats_.avg_bars_held * stats_.exits + pos.bars_held) /
                                          (stats_.exits + 1);
                    stats_.avg_pnl_pct = (stats_.avg_pnl_pct * stats_.exits + final_pnl_pct) /
                                        (stats_.exits + 1);

                    // CRITICAL FIX: Always erase after successful exit execution
                    // (Old code had special case for ROTATE_OUT, but that was part of the bug)
                    positions_.erase(decision.symbol);
                    utils::log_info("[EXECUTED EXIT] " + decision.symbol + " removed from positions");

                    return true;
                }
                return false;
            }

        case Decision::HOLD:
            // Nothing to do
            return true;

        default:
            return false;
    }
}

void RotationPositionManager::update_prices(
    const std::map<std::string, double>& current_prices
) {
    for (auto& [symbol, position] : positions_) {
        if (current_prices.count(symbol) > 0) {
            position.current_price = current_prices.at(symbol);

            // Update P&L
            if (position.direction == SignalType::LONG) {
                position.pnl = position.current_price - position.entry_price;
                position.pnl_pct = position.pnl / position.entry_price;
            } else {
                position.pnl = position.entry_price - position.current_price;
                position.pnl_pct = position.pnl / position.entry_price;
            }
        }
    }
}

double RotationPositionManager::get_total_unrealized_pnl() const {
    double total = 0.0;
    for (const auto& [symbol, position] : positions_) {
        total += position.pnl;
    }
    return total;
}

// === Private Methods ===

RotationPositionManager::Decision RotationPositionManager::check_exit_conditions(
    const Position& position,
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals,
    int current_time_minutes
) {
    // CRITICAL: Enforce minimum holding period to prevent churning
    if (position.bars_held < position.minimum_hold_bars) {
        // Only allow exit for critical conditions during minimum hold
        if (config_.enable_stop_loss && position.pnl_pct <= -config_.stop_loss_pct) {
            return Decision::STOP_LOSS;  // Allow stop loss
        }
        if (config_.eod_liquidation && current_time_minutes >= config_.eod_exit_time_minutes) {
            return Decision::EOD_EXIT;  // Allow EOD exit
        }
        return Decision::HOLD;  // Force hold otherwise
    }

    // Check EOD exit
    if (config_.eod_liquidation && current_time_minutes >= config_.eod_exit_time_minutes) {
        return Decision::EOD_EXIT;
    }

    // Check profit target
    if (config_.enable_profit_target && position.pnl_pct >= config_.profit_target_pct) {
        return Decision::PROFIT_TARGET;
    }

    // Check stop loss
    if (config_.enable_stop_loss && position.pnl_pct <= -config_.stop_loss_pct) {
        return Decision::STOP_LOSS;
    }

    // Check if rank fell below threshold
    if (position.current_rank > config_.min_rank_to_hold) {
        return Decision::EXIT;
    }

    // HYSTERESIS: Use different threshold for exit vs hold
    // This creates a "dead zone" to prevent oscillation
    double exit_threshold = config_.min_strength_to_exit;  // Lower than entry threshold
    if (position.current_strength < exit_threshold) {
        return Decision::EXIT;
    }

    return Decision::HOLD;
}

const SignalAggregator::RankedSignal* RotationPositionManager::find_signal(
    const std::string& symbol,
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals
) const {
    for (const auto& rs : ranked_signals) {
        if (rs.symbol == symbol) {
            return &rs;
        }
    }
    return nullptr;
}

bool RotationPositionManager::should_rotate(
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals
) {
    // Only rotate if at max capacity
    if (get_position_count() < config_.max_positions) {
        return false;
    }

    // Find strongest non-held signal
    for (const auto& ranked_signal : ranked_signals) {
        if (!has_position(ranked_signal.symbol)) {
            // Check if significantly stronger than weakest position
            std::string weakest = find_weakest_position();
            if (!weakest.empty()) {
                auto& weakest_pos = positions_.at(weakest);
                double strength_delta = ranked_signal.strength - weakest_pos.current_strength;

                return (strength_delta >= config_.rotation_strength_delta);
            }
        }
    }

    return false;
}

std::string RotationPositionManager::find_weakest_position() const {
    if (positions_.empty()) {
        return "";
    }

    std::string weakest;
    double min_strength = std::numeric_limits<double>::max();

    for (const auto& [symbol, position] : positions_) {
        if (position.current_strength < min_strength) {
            min_strength = position.current_strength;
            weakest = symbol;
        }
    }

    return weakest;
}

} // namespace sentio

```

