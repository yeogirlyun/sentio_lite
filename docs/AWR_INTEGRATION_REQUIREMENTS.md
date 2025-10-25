# AWR Strategy Integration Requirements

**Document Version:** 1.0
**Date:** 2025-10-25
**Status:** Design Phase
**Author:** Claude Code

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [AWR Strategy Overview](#awr-strategy-overview)
3. [Integration Architecture](#integration-architecture)
4. [Detailed Requirements](#detailed-requirements)
5. [Implementation Plan](#implementation-plan)
6. [Testing Strategy](#testing-strategy)
7. [Reference Materials](#reference-materials)

---

## 1. Executive Summary

### 1.1 Purpose

This document specifies the requirements for integrating the **AWR (Anticipatory Williams RSI)** strategy into the Sentio Lite trading system as an independent, first-class strategy alongside SIGOR.

### 1.2 Goals

- **Independent Strategy**: AWR should be selectable via `--strategy awr` command-line option
- **Multi-Symbol Rotation**: AWR must support the same 12-symbol rotation trading as SIGOR
- **Fair Comparison**: Enable apples-to-apples performance comparison between AWR and SIGOR
- **Production Ready**: Full integration with live trading, dashboard generation, and configuration management
- **Maintainability**: Clean separation of concerns, easy to extend with additional strategies in the future

### 1.3 Non-Goals

- Hybrid strategies combining AWR and SIGOR signals (future work)
- Parameter optimization framework for AWR (use existing tools)
- AWR-specific risk management beyond existing framework

---

## 2. AWR Strategy Overview

### 2.1 Core Concept

AWR is an **anticipatory crossover strategy** based on the interaction between Williams %R and RSI indicators near Bollinger Band extremes.

**Signal Philosophy:**
- **Bullish**: Williams %R crosses up over RSI near Bollinger Lower Band → mean reversion upward
- **Bearish**: Williams %R crosses down under RSI near Bollinger Upper Band → mean reversion downward
- **Anticipatory**: Signal strength is proportional to crossover stage (approaching, crossing, fresh)

### 2.2 Technical Indicators

#### Williams %R
```
Williams %R = ((Highest High - Close) / (Highest High - Lowest Low)) × -100
Range: [-100, 0]
Period: 14 bars (default)
```

#### RSI (Wilder's EMA Method)
```
RSI = 100 - (100 / (1 + RS))
where RS = Average Gain / Average Loss (Wilder's smoothing)
Range: [0, 100]
Period: 14 bars (default)
```

#### Bollinger Bands
```
Middle Band = 20-period SMA
Upper Band = Middle + (2 × StdDev)
Lower Band = Middle - (2 × StdDev)
```

### 2.3 Crossover Detection

AWR uses **three-state crossover detection**:

1. **CROSSING** (Strongest)
   - Williams %R is crossing RSI threshold at current bar
   - Immediate signal, highest strength multiplier (1.0)

2. **APPROACHING** (Strong)
   - Williams %R and RSI are converging (distance decreasing)
   - Distance within threshold (5 percentage points)
   - Strength multiplier: 0.7

3. **FRESH** (Strong but Decaying)
   - Recently crossed (within last 3 bars)
   - Linear decay over fresh window
   - Strength multiplier: 0.7 × (1 - bars_since_cross / fresh_bars)

### 2.4 Signal Calculation

```cpp
// Base probability starts at 0.5 (neutral)
double base_prob = 0.5;

// Band proximity factor (0.0 to 1.0)
if (price_percentile < 30.0) {
    lower_proximity = (30.0 - price_percentile) / 30.0;  // Bullish zone
} else if (price_percentile > 70.0) {
    upper_proximity = (price_percentile - 70.0) / 30.0;  // Bearish zone
}

// Bullish signal strength
if (crossing_up) {
    bullish_signal = 1.0 * lower_proximity;
} else if (approaching_up) {
    bullish_signal = 0.7 * lower_proximity;
} else if (fresh_up) {
    double freshness = 1.0 - (bars_since_cross_up / 3.0);
    bullish_signal = 0.7 * freshness * lower_proximity;
}

// Final probability
probability = base_prob + (bullish_signal * 0.3) - (bearish_signal * 0.3);

// Direction
is_long = probability > 0.52;
is_short = probability < 0.48;
is_neutral = !is_long && !is_short;
```

### 2.5 Default Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `williams_period` | 14 | Williams %R lookback period |
| `rsi_period` | 14 | RSI period (Wilder's EMA) |
| `bb_period` | 20 | Bollinger Bands SMA period |
| `bb_stddev` | 2.0 | Bollinger Bands standard deviations |
| `approach_threshold` | 5 | Distance threshold for "approaching" (percentage points) |
| `fresh_bars` | 3 | Bars after cross considered "fresh" |
| `lower_band_zone` | 30.0 | Price percentile defining lower reversal zone |
| `upper_band_zone` | 70.0 | Price percentile defining upper reversal zone |
| `crossing_strength` | 1.0 | Maximum strength when crossing |
| `approaching_strength` | 0.7 | Strength when approaching |
| `fresh_strength` | 0.7 | Strength when recently crossed |
| `min_confidence` | 0.6 | Minimum confidence for trade entry |

---

## 3. Integration Architecture

### 3.1 System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        main.cpp                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Command Line Parsing                                   │  │
│  │ - Parse --strategy {sigor|awr}                        │  │
│  │ - Load appropriate config files                       │  │
│  └───────────────┬───────────────────────────────────────┘  │
│                  │                                           │
│                  ▼                                           │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ TradingConfig                                         │  │
│  │ - strategy: StrategyType (SIGOR | AWR)               │  │
│  │ - sigor_config: SigorConfig (if SIGOR)               │  │
│  │ - awr_config: WilliamsRsiConfig (if AWR)             │  │
│  │ - trading params, position sizing, filters           │  │
│  └───────────────┬───────────────────────────────────────┘  │
│                  │                                           │
│                  ▼                                           │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ MultiSymbolTrader                                     │  │
│  │ ┌─────────────────────────────────────────────────┐  │  │
│  │ │ Symbol Rotation Loop (bar-by-bar)               │  │  │
│  │ │                                                   │  │  │
│  │ │  for each symbol:                                │  │  │
│  │ │    if (strategy == SIGOR):                       │  │  │
│  │ │      signal = generate_sigor_signal(bar)         │  │  │
│  │ │    else if (strategy == AWR):                    │  │  │
│  │ │      signal = generate_awr_signal(bar)           │  │  │
│  │ │                                                   │  │  │
│  │ │    Store signal for rotation selection           │  │  │
│  │ │                                                   │  │  │
│  │ │  Select best symbol based on signal strength     │  │  │
│  │ │  Execute trades with position sizing             │  │  │
│  │ └─────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘

Strategy Implementations:
┌────────────────────────┐      ┌────────────────────────┐
│   SigorStrategy        │      │  WilliamsRsiStrategy   │
│                        │      │                        │
│ - 7 detectors          │      │ - Williams %R          │
│ - Log-odds fusion      │      │ - RSI (Wilder's EMA)   │
│ - Multi-timeframe      │      │ - Bollinger Bands      │
│ - Regime detection     │      │ - Crossover detection  │
└────────────────────────┘      └────────────────────────┘
```

### 3.2 Data Flow

```
Bar Data → Strategy Signal Generation → Signal Aggregation → Symbol Selection → Trade Execution
          (SIGOR or AWR based on config)    (per symbol)      (best signal)     (position sizing)
```

### 3.3 Configuration Flow

```
Command Line (--strategy awr)
    ↓
config/awr_params.json → WilliamsRsiConfig
    ↓
config/trading_params.json → TradingConfig
    ↓
MultiSymbolTrader initialization
    ↓
WilliamsRsiStrategy instance per symbol
```

---

## 4. Detailed Requirements

### 4.1 Code Structure Changes

#### 4.1.1 Strategy Type Enumeration
**File:** `include/trading/trading_strategy.h`

**Status:** ✅ COMPLETED

```cpp
enum class StrategyType {
    SIGOR,   // Rule-based SIGOR ensemble
    AWR      // Anticipatory Williams RSI crossover
};

// Helper functions updated to support AWR
inline StrategyType parse_strategy_type(const std::string& str);
inline std::string to_string(StrategyType strategy);
inline std::string get_strategy_display_name(StrategyType strategy);
inline std::string get_strategy_config_path(StrategyType strategy);
```

#### 4.1.2 Trading Configuration
**File:** `include/trading/trading_config.h`

**Current Structure:**
```cpp
struct TradingConfig {
    StrategyType strategy = StrategyType::SIGOR;
    SigorConfig sigor_config;  // SIGOR-specific params
    // ... other trading params
};
```

**Required Changes:**
```cpp
struct TradingConfig {
    StrategyType strategy = StrategyType::SIGOR;

    // Strategy-specific configurations
    SigorConfig sigor_config;              // SIGOR parameters
    WilliamsRsiConfig awr_config;          // AWR parameters

    // Common trading parameters
    double initial_capital = 100000.0;
    PositionSizingConfig position_sizing;
    TradeFilterConfig trade_filter;
    WarmupConfig warmup;
    // ... other params
};
```

#### 4.1.3 Configuration Loader
**File:** `include/utils/config_loader.h`

**New Class Required:**
```cpp
class AwrConfigLoader {
public:
    /**
     * Load AWR configuration from JSON file
     * @param filepath Path to awr_params.json
     * @return WilliamsRsiConfig structure
     */
    static WilliamsRsiConfig load(const std::string& filepath);

    /**
     * Print AWR configuration for verification
     * @param config AWR configuration
     * @param filepath Source file path
     */
    static void print_config(const WilliamsRsiConfig& config,
                            const std::string& filepath);
};
```

**Implementation:** `src/utils/config_loader.cpp`

```cpp
WilliamsRsiConfig AwrConfigLoader::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open AWR config: " + filepath);
    }

    nlohmann::json j;
    file >> j;

    WilliamsRsiConfig config;
    auto params = j["parameters"];

    config.williams_period = params.value("williams_period", 14);
    config.rsi_period = params.value("rsi_period", 14);
    config.bb_period = params.value("bb_period", 20);
    config.bb_stddev = params.value("bb_stddev", 2.0);
    config.approach_threshold = params.value("approach_threshold", 5);
    config.fresh_bars = params.value("fresh_bars", 3);
    config.lower_band_zone = params.value("lower_band_zone", 30.0);
    config.upper_band_zone = params.value("upper_band_zone", 70.0);
    config.crossing_strength = params.value("crossing_strength", 1.0);
    config.approaching_strength = params.value("approaching_strength", 0.7);
    config.fresh_strength = params.value("fresh_strength", 0.7);

    return config;
}
```

#### 4.1.4 Multi-Symbol Trader
**File:** `include/trading/multi_symbol_trader.h`

**Current Structure:**
```cpp
class MultiSymbolTrader {
private:
    SigorStrategy sigor_;
    std::map<std::string, SigorState> sigor_states_;
};
```

**Required Changes:**
```cpp
class MultiSymbolTrader {
private:
    TradingConfig config_;

    // Strategy instances (only one active based on config.strategy)
    std::unique_ptr<SigorStrategy> sigor_;
    std::unique_ptr<WilliamsRsiStrategy> awr_;

    // Strategy-specific state (polymorphic approach alternative)
    std::map<std::string, SigorState> sigor_states_;
    // AWR doesn't need per-symbol state (stateful in WilliamsRsiStrategy)

    /**
     * Generate signal for current bar based on active strategy
     * @param bar Current bar data
     * @param symbol Symbol being evaluated
     * @return Generic signal structure for rotation selection
     */
    SignalInfo generate_signal(const Bar& bar, const std::string& symbol);
};
```

**Signal Info Structure** (Unified for rotation):
```cpp
struct SignalInfo {
    std::string symbol;
    std::chrono::system_clock::time_point timestamp;

    // Unified signal representation
    double probability;      // 0.0 to 1.0 (0.5 = neutral)
    double confidence;       // 0.0 to 1.0
    bool is_long;
    bool is_short;
    bool is_neutral;

    // Strategy-specific data (optional)
    std::string strategy_type;  // "SIGOR" or "AWR"
    nlohmann::json strategy_details;  // For logging/debugging
};
```

#### 4.1.5 Main Application
**File:** `src/main.cpp`

**Command Line Parsing Changes:**

```cpp
// Line 154-161: Add strategy parsing
for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];

    // Strategy selection
    if (arg == "--strategy" && i + 1 < argc) {
        config.strategy_str = argv[++i];
        config.strategy = parse_strategy_type(config.strategy_str);
    }
    // ... existing options
}
```

**Configuration Loading Changes:**

```cpp
// Line 220-248: Replace SIGOR-only loading
try {
    std::string trading_params_path = config.config_dir + "/trading_params.json";

    if (config.strategy == StrategyType::SIGOR) {
        std::string sigor_params_path = config.config_dir + "/sigor_params.json";
        std::string sigor_trading_path = config.config_dir + "/sigor_trading_params.json";

        // Load trading config (SIGOR-specific or default)
        if (std::filesystem::exists(sigor_trading_path)) {
            config.trading = ConfigLoader::load(sigor_trading_path);
        } else {
            config.trading = ConfigLoader::load(trading_params_path);
        }

        // Load SIGOR model parameters
        config.trading.sigor_config = SigorConfigLoader::load(sigor_params_path);
        config.trading.strategy = StrategyType::SIGOR;

        std::cout << "\n📊 SIGOR Strategy Configuration Loaded\n";
        SigorConfigLoader::print_config(config.trading.sigor_config, sigor_params_path);

        // SIGOR: disable warmup for immediate trading
        config.warmup_bars_specified = 0;
        config.intraday_warmup = true;
        config.trading.min_bars_to_learn = 0;
        config.trading.warmup.enabled = false;

    } else if (config.strategy == StrategyType::AWR) {
        std::string awr_params_path = config.config_dir + "/awr_params.json";
        std::string awr_trading_path = config.config_dir + "/awr_trading_params.json";

        // Load trading config (AWR-specific or default)
        if (std::filesystem::exists(awr_trading_path)) {
            config.trading = ConfigLoader::load(awr_trading_path);
        } else {
            config.trading = ConfigLoader::load(trading_params_path);
        }

        // Load AWR model parameters
        config.trading.awr_config = AwrConfigLoader::load(awr_params_path);
        config.trading.strategy = StrategyType::AWR;

        std::cout << "\n📊 AWR Strategy Configuration Loaded\n";
        AwrConfigLoader::print_config(config.trading.awr_config, awr_params_path);

        // AWR: requires warmup for indicator calculation
        // Minimum warmup = max(williams_period, rsi_period, bb_period) + fresh_bars
        int min_warmup = std::max({
            config.trading.awr_config.williams_period,
            config.trading.awr_config.rsi_period,
            config.trading.awr_config.bb_period
        }) + config.trading.awr_config.fresh_bars;

        if (config.warmup_bars_specified < min_warmup) {
            config.warmup_bars_specified = min_warmup;
            std::cout << "ℹ️  AWR warmup adjusted to minimum: " << min_warmup << " bars\n";
        }
    }

    config.capital = config.trading.initial_capital;

} catch (const std::exception& e) {
    std::cerr << "Error loading configuration: " << e.what() << "\n";
    return 1;
}
```

**Help Text Update:**

```cpp
// Line 77-119: Update help text
void print_usage(const char* program_name) {
    std::cout << "Sentio Lite - Multi-Strategy Intraday Trading\n\n"
              << "Supported Strategies:\n"
              << "  - SIGOR: Rule-based ensemble (7 detectors, log-odds fusion)\n"
              << "  - AWR: Anticipatory Williams RSI crossover\n\n"
              << "Usage: " << program_name << " mock --date MM-DD --strategy {sigor|awr} [options]\n\n"
              << "Required Options:\n"
              << "  --date MM-DD         Test date (year is fixed to 2025)\n"
              << "  --strategy NAME      Strategy: sigor (default) or awr\n\n"
              // ... rest of help text
}
```

### 4.2 Multi-Symbol Rotation Integration

#### 4.2.1 Signal Generation

The `MultiSymbolTrader` needs to generate signals differently based on strategy:

```cpp
SignalInfo MultiSymbolTrader::generate_signal(const Bar& bar, const std::string& symbol) {
    SignalInfo signal;
    signal.symbol = symbol;
    signal.timestamp = bar.timestamp;

    if (config_.strategy == StrategyType::SIGOR) {
        // SIGOR signal generation
        auto sigor_signal = sigor_->generate_signal(bar, symbol, sigor_states_[symbol]);

        signal.probability = sigor_signal.probability;
        signal.confidence = sigor_signal.confidence;
        signal.is_long = sigor_signal.is_long;
        signal.is_short = sigor_signal.is_short;
        signal.is_neutral = sigor_signal.is_neutral;
        signal.strategy_type = "SIGOR";

        // Store SIGOR-specific details
        signal.strategy_details["detectors_long"] = sigor_signal.num_detectors_long;
        signal.strategy_details["detectors_short"] = sigor_signal.num_detectors_short;

    } else if (config_.strategy == StrategyType::AWR) {
        // AWR signal generation
        auto awr_signal = awr_->generate_signal(bar, symbol);

        signal.probability = awr_signal.probability;
        signal.confidence = awr_signal.confidence;
        signal.is_long = awr_signal.is_long;
        signal.is_short = awr_signal.is_short;
        signal.is_neutral = awr_signal.is_neutral;
        signal.strategy_type = "AWR";

        // Store AWR-specific details
        signal.strategy_details["williams_r"] = awr_signal.williams_r;
        signal.strategy_details["rsi"] = awr_signal.rsi;
        signal.strategy_details["price_percentile"] = awr_signal.price_percentile;
        signal.strategy_details["is_crossing_up"] = awr_signal.is_crossing_up;
        signal.strategy_details["is_crossing_down"] = awr_signal.is_crossing_down;
    }

    return signal;
}
```

#### 4.2.2 Symbol Selection

Symbol selection logic remains unchanged - both strategies provide:
- `probability` (higher = stronger bullish, lower = stronger bearish)
- `confidence` (0.0 to 1.0)
- Direction flags (`is_long`, `is_short`, `is_neutral`)

The rotation algorithm selects the symbol with the highest combined signal strength.

### 4.3 Configuration Files

#### 4.3.1 AWR Parameters
**File:** `config/awr_params.json`

**Status:** ✅ COMPLETED (already created)

#### 4.3.2 AWR Trading Parameters (Optional)
**File:** `config/awr_trading_params.json`

If AWR requires different trading parameters (position sizing, filters) from SIGOR:

```json
{
  "description": "Trading Configuration for AWR Strategy",
  "initial_capital": 100000.0,
  "position_sizing": {
    "base_position_pct": 0.95,
    "confidence_scaling": true,
    "min_confidence": 0.6,
    "max_position_pct": 0.99
  },
  "trade_filter": {
    "min_holding_bars": 5,
    "max_trades_per_day": 50,
    "min_bars_between_trades": 1
  },
  "transaction_costs": {
    "commission_per_share": 0.0,
    "slippage_pct": 0.001
  }
}
```

---

## 5. Implementation Plan

### 5.1 Phase 1: Core Infrastructure (COMPLETED)
- [x] Update `StrategyType` enum with AWR
- [x] Update helper functions (`parse_strategy_type`, `to_string`, etc.)
- [x] Create `config/awr_params.json`

### 5.2 Phase 2: Configuration Layer
**Estimated Time:** 2-3 hours

#### Files to Modify:
1. `include/trading/trading_config.h`
   - Add `WilliamsRsiConfig awr_config` field

2. `include/utils/config_loader.h`
   - Add `AwrConfigLoader` class declaration

3. `src/utils/config_loader.cpp`
   - Implement `AwrConfigLoader::load()`
   - Implement `AwrConfigLoader::print_config()`

#### Deliverables:
- AWR configuration can be loaded from JSON
- Configuration validation and error handling
- Configuration printing for verification

### 5.3 Phase 3: Multi-Symbol Trader Integration
**Estimated Time:** 4-6 hours

#### Files to Modify:
1. `include/trading/multi_symbol_trader.h`
   - Add `std::unique_ptr<WilliamsRsiStrategy> awr_` member
   - Add `SignalInfo` structure
   - Update method signatures

2. `src/trading/multi_symbol_trader.cpp`
   - Implement strategy-agnostic `generate_signal()`
   - Initialize AWR strategy when `config.strategy == AWR`
   - Update symbol selection to use `SignalInfo`
   - Update trade execution to work with both strategies

#### Key Considerations:
- **State Management**: SIGOR uses `SigorState` per symbol, AWR is stateful internally
- **Warmup Handling**: AWR requires indicator warmup, SIGOR doesn't
- **Signal Compatibility**: Both must produce comparable probability/confidence scores

### 5.4 Phase 4: Main Application Updates
**Estimated Time:** 2-3 hours

#### Files to Modify:
1. `src/main.cpp`
   - Add `--strategy` command-line parsing
   - Implement AWR config loading branch
   - Update warmup logic for AWR
   - Update help text

#### Deliverables:
- `./build/sentio_lite mock --date 10-16 --strategy awr` works
- Proper error handling for missing configs
- Clear user feedback on loaded strategy

### 5.5 Phase 5: Testing & Validation
**Estimated Time:** 3-4 hours

#### Test Cases:
1. **Unit Tests**
   - AWR config loading
   - Signal generation consistency
   - Indicator calculations (Williams %R, RSI, BB)

2. **Integration Tests**
   - Multi-symbol rotation with AWR
   - Strategy switching (SIGOR → AWR)
   - Dashboard generation with AWR

3. **Performance Tests**
   - Run AWR on test dates: 2025-10-16, 10-17, 10-21, 10-22
   - Compare MRD with SIGOR
   - Validate fair comparison (same rotation logic)

### 5.6 Phase 6: Documentation & Release
**Estimated Time:** 2 hours

#### Deliverables:
- Update README with AWR strategy info
- Create AWR user guide
- Update dashboard to show strategy-specific metrics
- Release notes for v2.1 (AWR integration)

---

## 6. Testing Strategy

### 6.1 Unit Testing

#### AWR Config Loading
```cpp
TEST(AwrConfigLoader, LoadsDefaultParameters) {
    auto config = AwrConfigLoader::load("config/awr_params.json");
    EXPECT_EQ(config.williams_period, 14);
    EXPECT_EQ(config.rsi_period, 14);
    EXPECT_DOUBLE_EQ(config.crossing_strength, 1.0);
}
```

#### Signal Generation
```cpp
TEST(WilliamsRsiStrategy, GeneratesValidSignals) {
    WilliamsRsiConfig config;
    WilliamsRsiStrategy strategy(config);

    // Feed warmup bars
    for (const auto& bar : warmup_bars) {
        strategy.generate_signal(bar, "TQQQ");
    }

    // Test signal
    auto signal = strategy.generate_signal(test_bar, "TQQQ");
    EXPECT_GE(signal.probability, 0.0);
    EXPECT_LE(signal.probability, 1.0);
    EXPECT_GE(signal.confidence, 0.0);
    EXPECT_LE(signal.confidence, 1.0);
}
```

### 6.2 Integration Testing

#### Multi-Symbol Rotation
```bash
# Test AWR with 12-symbol rotation
./build/sentio_lite mock --date 10-16 --strategy awr

# Expected output:
# - Loads AWR config
# - Runs rotation across all symbols
# - Generates results.json with AWR metrics
# - Creates dashboard showing AWR performance
```

#### Strategy Comparison
```bash
# Run both strategies on same date
./build/sentio_lite mock --date 10-16 --strategy sigor --results-file results_sigor.json
./build/sentio_lite mock --date 10-16 --strategy awr --results-file results_awr.json

# Compare results
python3 scripts/compare_strategies.py results_sigor.json results_awr.json
```

### 6.3 Performance Validation

#### Test Dates
- 2025-10-16
- 2025-10-17
- 2025-10-21
- 2025-10-22

#### Metrics to Validate
- **MRD (Mean Return per Day)**: Primary performance metric
- **Win Rate**: Percentage of profitable trades
- **Profit Factor**: Gross profit / gross loss
- **Trade Count**: Number of trades executed
- **Sharpe Ratio**: Risk-adjusted returns

#### Acceptance Criteria
- AWR runs successfully on all test dates
- No crashes or errors
- MRD is comparable to SIGOR (within same order of magnitude)
- Dashboard generates correctly

---

## 7. Reference Materials

### 7.1 Source Code Modules (Headers and Implementation)

#### Strategy Layer
| File | Description | Status |
|------|-------------|--------|
| `include/strategy/williams_rsi_strategy.h` | AWR strategy header, signal structures | ✅ Exists |
| `src/strategy/williams_rsi_strategy.cpp` | AWR strategy implementation | ✅ Exists |
| `include/strategy/sigor_strategy.h` | SIGOR strategy header | ✅ Exists |
| `src/strategy/sigor_strategy.cpp` | SIGOR strategy implementation | ✅ Exists |

#### Trading Layer
| File | Description | Status |
|------|-------------|--------|
| `include/trading/trading_strategy.h` | Strategy type enum and helpers | ✅ Updated |
| `include/trading/trading_config.h` | Trading configuration structure | 🔧 Needs Update |
| `include/trading/trading_mode.h` | Trading mode enum (Mock/Live) | ✅ Exists |
| `include/trading/multi_symbol_trader.h` | Multi-symbol rotation trader | 🔧 Needs Update |
| `src/trading/multi_symbol_trader.cpp` | Rotation trading implementation | 🔧 Needs Update |
| `include/trading/alpaca_cost_model.h` | Transaction cost model | ✅ Exists |
| `src/trading/alpaca_cost_model.cpp` | Cost model implementation | ✅ Exists |
| `include/trading/trade_filter.h` | Trade frequency/holding filters | ✅ Exists |
| `src/trading/trade_filter.cpp` | Trade filter implementation | ✅ Exists |

#### Utilities Layer
| File | Description | Status |
|------|-------------|--------|
| `include/utils/config_loader.h` | Configuration loading utilities | 🔧 Needs Update |
| `src/utils/config_loader.cpp` | Config loader implementation | 🔧 Needs Update |
| `include/utils/config_reader.h` | Symbol config reader | ✅ Exists |
| `include/utils/data_loader.h` | Binary/CSV data loading | ✅ Exists |
| `src/utils/data_loader.cpp` | Data loader implementation | ✅ Exists |
| `include/utils/date_filter.h` | Date filtering utilities | ✅ Exists |
| `include/utils/results_exporter.h` | Results JSON export | ✅ Exists |

#### Core Data Structures
| File | Description | Status |
|------|-------------|--------|
| `include/core/bar.h` | Bar data structure (OHLCV + timestamp) | ✅ Exists |

#### Main Application
| File | Description | Status |
|------|-------------|--------|
| `src/main.cpp` | Main entry point, CLI parsing | 🔧 Needs Update |

#### Test Programs
| File | Description | Status |
|------|-------------|--------|
| `src/test_williams_rsi.cpp` | AWR single-symbol test | ✅ Exists |
| `src/test_sigor.cpp` | SIGOR single-symbol test | ✅ Exists |

### 7.2 Key Algorithm References

#### Williams %R Calculation
**File:** `src/strategy/williams_rsi_strategy.cpp`
**Lines:** 102-120
**Function:** `WilliamsRsiStrategy::calculate_williams_r()`

#### RSI (Wilder's EMA)
**File:** `src/strategy/williams_rsi_strategy.cpp`
**Lines:** 122-162
**Function:** `WilliamsRsiStrategy::calculate_rsi()`

#### Bollinger Bands
**File:** `src/strategy/williams_rsi_strategy.cpp`
**Lines:** 164-178
**Function:** `WilliamsRsiStrategy::calculate_bollinger_bands()`

#### Crossover Detection
**File:** `src/strategy/williams_rsi_strategy.cpp`
**Lines:** 190-239
**Function:** `WilliamsRsiStrategy::detect_crossovers()`

#### Signal Probability Calculation
**File:** `src/strategy/williams_rsi_strategy.cpp`
**Lines:** 243-288
**Function:** `WilliamsRsiStrategy::calculate_probability()`

#### SIGOR Signal Generation
**File:** `src/strategy/sigor_strategy.cpp`
**Function:** `SigorStrategy::generate_signal()`

#### Multi-Symbol Rotation
**File:** `src/trading/multi_symbol_trader.cpp`
**Function:** `MultiSymbolTrader::run_rotation_trading()`

### 7.3 Design Patterns

#### Strategy Pattern
- `StrategyType` enum defines available strategies
- Each strategy implements signal generation interface
- `MultiSymbolTrader` selects strategy at runtime

#### Factory Pattern (Implicit)
- Config loading creates appropriate strategy instance
- Strategy-specific configs loaded based on type

#### State Pattern
- SIGOR maintains per-symbol state (`SigorState`)
- AWR maintains internal indicator state

---

## 8. Risk Assessment

### 8.1 Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Signal incompatibility | Medium | High | Use `SignalInfo` wrapper to normalize signals |
| Performance regression | Low | Medium | Run benchmarks before/after integration |
| State management bugs | Medium | High | Thorough testing with state transitions |
| Config loading errors | Low | Medium | Robust error handling and validation |

### 8.2 Business Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| AWR underperforms SIGOR | Medium | Low | AWR is research tool, not production yet |
| Increased maintenance | Low | Low | Clean separation of concerns |
| User confusion | Low | Low | Clear documentation and help text |

---

## 9. Success Criteria

### 9.1 Functional Requirements
- [x] AWR selectable via `--strategy awr`
- [ ] AWR runs with 12-symbol rotation
- [ ] AWR produces comparable results to SIGOR
- [ ] Dashboard shows AWR-specific metrics
- [ ] Configuration loads correctly

### 9.2 Performance Requirements
- AWR completes test dates in reasonable time (<10s per day)
- No memory leaks or crashes
- Signal generation performance similar to SIGOR

### 9.3 Quality Requirements
- Code coverage >80% for new code
- No compiler warnings
- Passes all integration tests
- Documentation complete and accurate

---

## 10. Appendix

### 10.1 Glossary

| Term | Definition |
|------|------------|
| **AWR** | Anticipatory Williams RSI - crossover-based strategy |
| **SIGOR** | Signal-OR Ensemble - rule-based detector fusion |
| **MRD** | Mean Return per Day - primary performance metric |
| **RTH** | Regular Trading Hours (9:30 AM - 4:00 PM ET) |
| **Rotation Trading** | Selecting best symbol from pool based on signals |
| **Anticipatory** | Signal generated before/during crossover, not just after |

### 10.2 Configuration Example

Complete AWR test command:
```bash
./build/sentio_lite mock \
  --date 10-16 \
  --strategy awr \
  --config config \
  --results-file results_awr_10-16.json \
  --warmup-bars 40 \
  --verbose
```

Expected output:
```
📊 AWR Strategy Configuration Loaded
  Williams %R Period: 14
  RSI Period: 14 (Wilder's EMA)
  Bollinger Bands: 20 period, 2.0 stddev
  Approach Threshold: 5 points
  Fresh Cross Window: 3 bars

🔄 Multi-Symbol Rotation Trading
  Symbols: 12 (TQQQ, SQQQ, TNA, ...)
  Warmup: 40 bars

✅ Trading Complete
  MRD: 0.XXXX%
  Trades: XX
  Win Rate: XX.X%
```

### 10.3 Future Enhancements

- **Hybrid Strategies**: Combine SIGOR and AWR signals
- **Parameter Optimization**: Optuna-based optimization for AWR
- **Ensemble**: Multi-strategy voting/averaging
- **Live Trading**: AWR live trading support (currently mock only)
- **Additional Strategies**: Momentum, mean-reversion, ML-based

---

**End of Document**
