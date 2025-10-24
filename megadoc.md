New chat

REGIME_FEATURES_MRD_IMPROVEMENT_REQUIREMENTS_MEGA.md
10,525 lines

md

review and advise with solid codebase


Diagnosed numerical instability in trading system architecture and formulated remedies.
13s




Looking at your Sentio Lite codebase, you have a critical numerical stability issue that's killing your trading performance after adding regime features. The system expanded from 42 to 54 features, but the EWRLS covariance matrix is exploding with condition numbers reaching 9.8e+07 (target is <1e6). This causes 90% of trades to be filtered out as "low confidence."

Immediate Fixes Required
1. Fix Regularization in ewrls_predictor.h (Line 29)
cpp
// CURRENT (BROKEN):
double regularization = 1e-6;       // Too weak for 54 features
double initial_variance = 100.0;    // Too high

// FIXED:
double regularization = 1e-3;       // 1000x stronger
double initial_variance = 10.0;     // 10x smaller
The regularization needs to scale with feature count. For 54 features, use:

cpp
double adaptive_reg = base_regularization * sqrt(n_features / 42.0);
2. Fix Feature Validation in regime_features.cpp
Your regime features lack proper validation. Add this after line 50:

cpp
// Validate probabilities sum to 1.0
double hmm_sum = hmm_probs[0] + hmm_probs[1] + hmm_probs[2];
if (abs(hmm_sum - 1.0) > 0.01) {
    // Renormalize
    for (auto& p : hmm_probs) p /= hmm_sum;
}
3. Critical Issue: K-means Initialization
Your k-means in regime_features.cpp uses quantiles for initialization, but this can produce identical centroids when data is concentrated. Replace with k-means++:

cpp
// Line 267: Better initialization
std::vector<double> centroids(k);
centroids[0] = data[rand() % data.size()];  // Random first

for (int i = 1; i < k; ++i) {
    // Choose next centroid proportional to squared distance
    std::vector<double> distances;
    for (auto& point : data) {
        double min_dist = std::numeric_limits<double>::max();
        for (int j = 0; j < i; ++j) {
            min_dist = std::min(min_dist, abs(point - centroids[j]));
        }
        distances.push_back(min_dist * min_dist);
    }
    // Weighted random selection
    // ... implement selection based on distances
}
4. Performance Problem: Repeated Calculations
In feature_extractor.cpp, you're recalculating the same moving averages multiple times. Cache them:

cpp
// Add member variables:
std::unordered_map<int, double> ma_cache_;
int cache_bar_id_ = -1;

// In extract():
if (bar.bar_id != cache_bar_id_) {
    ma_cache_.clear();
    cache_bar_id_ = bar.bar_id;
}
Architectural Issues
1. Feature Redundancy
Your 12 regime features likely have high correlation. The HMM state probabilities sum to 1.0, making one redundant. Same for volatility regime probabilities. Consider removing:

hmm_state_2_prob (can be derived from other two)
gmm_vol_high_prob (can be derived from other two)
This reduces to 52 features, improving numerical stability.

2. EWRLS Update Logic Flaw
In ewrls_predictor.cpp line 56, you clamp returns to [-1.0, 1.0], but this destroys information about large moves in leveraged ETFs. Instead:

cpp
// Winsorize at 3 standard deviations
double return_std = 0.02;  // Typical for 1-minute bars
actual_return = std::max(-3*return_std, std::min(3*return_std, actual_return));
3. Trade Filter Logic Error
In multi_symbol_trader.cpp, the warmup adjustment can set warmup_bars = 50, but feature extraction requires LOOKBACK = 50. This means you have zero bars for actual learning. Change minimum to:

cpp
if (config.warmup_bars < 100) {  // Was 50
    config.warmup_bars = 100;  // 50 for features + 50 for learning
}
Optimization Issues
1. Optuna Search Space Too Wide
Your Phase 1 parameters in parameter_spaces.py have ranges that are too wide:

python
'lambda_1bar': (0.95, 0.999)  # 50x range!
For minute-bar data, tighten to:

python
'lambda_1bar': (0.97, 0.99)   # More realistic range
2. Dangerous Warmup Mode
You have WarmupMode::TESTING with relaxed criteria. This is a footgun. Remove it entirely or add:

cpp
#ifdef PRODUCTION
    #error "TESTING mode not allowed in production builds"
#endif
Quick Test Protocol
Recompile with fixes:
bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
Test numerical stability:
bash
./build/sentio_lite mock --date 2025-10-20 --config config/strategy_phase1_best.json --verbose 2>&1 | grep "Condition number"
If condition numbers stay <1e6, re-run Optuna:
bash
python3 optimization/optuna_optimizer.py --phase 1 --trials 100 --dates 2025-10-14 2025-10-15 2025-10-16 2025-10-17 2025-10-20
The core issue is that your numerical methods weren't adapted for the higher dimensional feature space. These fixes should restore stability and improve MRD.








REGIME_FEATURES_MRD_IMPROVEMENT_REQUIREMENTS_MEGA.md
396.26 KB •10,525 lines
•
Formatting may be inconsistent from source
# REGIME_FEATURES_MRD_IMPROVEMENT_REQUIREMENTS - Complete Analysis

**Generated**: 2025-10-21 14:40:00
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/sentio_lite
**Source**: /Volumes/ExternalSSD/Dev/C++/sentio_lite/docs/REGIME_FEATURES_MRD_IMPROVEMENT_REQUIREMENTS.md
**Total Files**: 25

---

## ðŸ“‹ **TABLE OF CONTENTS**

1. [CMakeLists.txt](#file-1)
2. [REGIME_FEATURES_IMPLEMENTATION.md](#file-2)
3. [REGIME_FEATURES_OPTUNA_RUN.md](#file-3)
4. [config/strategy_phase1_best.json](#file-4)
5. [config/strategy_phase2_best.json](#file-5)
6. [docs/REGIME_ADAPTIVE_FEATURE_MODEL.md](#file-6)
7. [docs/REGIME_DETECTION_FEATURE_DESIGN.md](#file-7)
8. [docs/REGIME_FEATURES_FOR_EWRLS.md](#file-8)
9. [docs/REGIME_FEATURES_MRD_IMPROVEMENT_REQUIREMENTS.md](#file-9)
10. [include/predictor/ewrls_predictor.h](#file-10)
11. [include/predictor/feature_extractor.h](#file-11)
12. [include/predictor/multi_horizon_predictor.h](#file-12)
13. [include/predictor/regime_features.h](#file-13)
14. [include/trading/multi_symbol_trader.h](#file-14)
15. [optimization/objective_function.py](#file-15)
16. [optimization/optuna_optimizer.py](#file-16)
17. [optimization/parameter_spaces.py](#file-17)
18. [results/best_params_phase1.json](#file-18)
19. [src/main.cpp](#file-19)
20. [src/predictor/ewrls_predictor.cpp](#file-20)
21. [src/predictor/feature_extractor.cpp](#file-21)
22. [src/predictor/multi_horizon_predictor.cpp](#file-22)
23. [src/predictor/regime_features.cpp](#file-23)
24. [src/trading/multi_symbol_trader.cpp](#file-24)
25. [src/utils/config_loader.cpp](#file-25)

---

## ðŸ“„ **FILE 1 of 25**: CMakeLists.txt

**File Information**:
- **Path**: `CMakeLists.txt`
- **Size**: 189 lines
- **Modified**: 2025-10-21 14:01:47
- **Type**: txt
- **Permissions**: -rw-r--r--

```text
cmake_minimum_required(VERSION 3.16)
project(SentioLite VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# C++ Standard & Build Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# Compiler Flags
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

    # Optional: AddressSanitizer for debug builds
    option(ENABLE_ASAN "Enable AddressSanitizer in debug builds" OFF)
    if(ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
    endif()
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

# ============================================================================
# Dependencies
# ============================================================================
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(Threads REQUIRED)

# ============================================================================
# Include Directories
# ============================================================================
include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# Source Files - CORRECTED AND VALIDATED
# ============================================================================
set(CORE_SOURCES
    # Predictor components (robust online learning)
    src/predictor/ewrls_predictor.cpp          # Core EWRLS with numerical stability
    src/predictor/online_predictor.cpp         # Wrapper for 54-feature enforcement
    src/predictor/feature_extractor.cpp        # 54-feature extractor (8 time + 34 technical + 12 regime)
    src/predictor/regime_features.cpp          # Fast regime detection (k-means clustering)
    src/predictor/multi_horizon_predictor.cpp  # Multi-horizon prediction (1, 5, 10 bars)

    # Features (backward compatibility wrapper)
    src/features/unified_features.cpp          # Wrapper around FeatureExtractor

    # Trading engine
    src/trading/multi_symbol_trader.cpp        # Multi-symbol rotation trading
    src/trading/alpaca_cost_model.cpp          # Alpaca transaction cost model
    src/trading/trade_filter.cpp               # Trade frequency and holding period management

    # Utils
    src/utils/data_loader.cpp                  # Binary/CSV data loading
    src/utils/config_loader.cpp                # JSON configuration loader
)

# Validate that all source files exist
foreach(SOURCE ${CORE_SOURCES})
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${SOURCE}")
        message(FATAL_ERROR "Missing required source file: ${SOURCE}")
    endif()
endforeach()

# ============================================================================
# Libraries
# ============================================================================

# Core library (static)
add_library(sentio_core STATIC ${CORE_SOURCES})
target_link_libraries(sentio_core PUBLIC
    Eigen3::Eigen
    Threads::Threads
)

# Set include directories for the library
target_include_directories(sentio_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ============================================================================
# Executables
# ============================================================================

# Main trading system
add_executable(sentio_lite src/main.cpp)
target_link_libraries(sentio_lite PRIVATE
    sentio_core
    Eigen3::Eigen
    Threads::Threads
)

# Alpaca cost model demonstration (optional)
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    if(EXISTS "${CMAKE_SOURCE_DIR}/examples/alpaca_cost_demo.cpp")
        add_executable(alpaca_cost_demo examples/alpaca_cost_demo.cpp)
        target_link_libraries(alpaca_cost_demo PRIVATE
            sentio_core
            Threads::Threads
        )
        message(STATUS "Building example: alpaca_cost_demo")
    else()
        message(STATUS "Example alpaca_cost_demo.cpp not found, skipping")
    endif()
endif()

# ============================================================================
# Testing (Optional but recommended)
# ============================================================================
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(WARNING "BUILD_TESTS enabled but tests/ directory not found")
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS sentio_lite
    RUNTIME DESTINATION bin
)

install(TARGETS sentio_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# ============================================================================
# Configuration Summary
# ============================================================================
message(STATUS "========================================")
message(STATUS "Sentio Lite Configuration")
message(STATUS "========================================")
message(STATUS "Project:         ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Eigen3 version:  ${EIGEN3_VERSION}")

# Print source file count
list(LENGTH CORE_SOURCES NUM_SOURCES)
message(STATUS "Source files:    ${NUM_SOURCES}")

message(STATUS "========================================")
message(STATUS "Features:")
message(STATUS "  - 33 Features (8 time + 25 technical)")
message(STATUS "  - Multi-Horizon Prediction (1, 5, 10 bars)")
message(STATUS "  - EWRLS with numerical stability")
message(STATUS "  - Condition number monitoring")
message(STATUS "  - Adaptive regularization")
message(STATUS "  - Gradient clipping")
message(STATUS "  - Multi-Symbol Rotation Trading")
message(STATUS "  - Trade frequency management")
message(STATUS "  - Holding period constraints (min 5 bars)")
message(STATUS "  - Dynamic exit logic")
message(STATUS "  - Alpaca Cost Model")
message(STATUS "  - Binary & CSV Data Support")
message(STATUS "  - Adaptive Position Sizing")
message(STATUS "========================================")
message(STATUS "Build options:")
message(STATUS "  BUILD_EXAMPLES:  ${BUILD_EXAMPLES}")
message(STATUS "  BUILD_TESTS:     ${BUILD_TESTS}")
if(ENABLE_ASAN)
    message(STATUS "  ASAN enabled:    YES (Debug only)")
endif()
message(STATUS "========================================")

```

## ðŸ“„ **FILE 2 of 25**: REGIME_FEATURES_IMPLEMENTATION.md

**File Information**:
- **Path**: `REGIME_FEATURES_IMPLEMENTATION.md`
- **Size**: 232 lines
- **Modified**: 2025-10-21 14:03:43
- **Type**: md
- **Permissions**: -rw-r--r--

```text
# Regime Features Implementation Complete

## Summary
Successfully integrated 12 regime-aware features into EWRLS predictor to help improve MRD across all market conditions.

**Status:** âœ… IMPLEMENTED & TESTED
**Build:** âœ… COMPILED SUCCESSFULLY
**Test:** âœ… RUNS ON OCT 20 DATA

---

## What Was Implemented

### 1. Fast C++ Regime Feature Generator
**File:** `src/predictor/regime_features.cpp` + `include/predictor/regime_features.h`

**12 New Features:**
```cpp
// HMM-like Market State (3 features)
- hmm_state_0_prob      [0.0, 1.0]  // Trending up probability
- hmm_state_1_prob      [0.0, 1.0]  // Ranging probability
- hmm_state_2_prob      [0.0, 1.0]  // Trending down probability

// Volatility Regime (3 features)
- gmm_vol_low_prob      [0.0, 1.0]  // Low volatility probability
- gmm_vol_med_prob      [0.0, 1.0]  // Medium volatility probability
- gmm_vol_high_prob     [0.0, 1.0]  // High volatility probability

// Regime Stability (2 features)
- hmm_state_duration    [0, 120]    // Bars since last state change
- vol_regime_duration   [0, 120]    // Bars since last vol regime change

// Microstructure (4 features)
- vol_ratio_20_60       [-3, 3]     // 20-bar vol / 60-bar vol
- vol_zscore            [-3, 3]     // Volatility z-score
- price_vol_correlation [-1, 1]     // 20-bar price-volume correlation
- volume_zscore         [-3, 3]     // Volume z-score
```

**Algorithm:**
- Fast k-means clustering (k=3) on returns â†’ market states
- Fast k-means clustering (k=3) on rolling volatility â†’ vol regimes
- Soft probability assignment using inverse distance weighting
- Duration tracking for regime stability
- O(N) complexity, ~0.5ms overhead per bar

**Key Design Decision:**
- Implemented in pure C++ (no Python bridge needed)
- Uses simple k-means instead of full HMM/GMM (much faster)
- Provides same informative features for EWRLS learning

### 2. Integration with Feature Extractor
**File:** `src/predictor/feature_extractor.cpp`

**Changes:**
- Increased `NUM_FEATURES` from 42 â†’ 54
- Added 12 regime feature indices to `FeatureIndex` enum
- Calls `regime_features_.extract(bars)` after extracting other features
- Appends regime features to end of feature vector

**Feature Vector Layout:**
```
[0-7]    Time features (8)
[8-11]   Momentum features (4)
[12-14]  Volatility features (3)
[15-16]  Volume features (2)
[17-19]  Price position features (3)
[20-22]  Trend strength features (3)
[23-27]  Interaction features (5)
[28-30]  Acceleration features (3)
[31]     Log momentum (1)
[32-34]  Mean reversion features (3)
[35-40]  Bollinger Bands features (6)
[41]     Bias term (1)
[42-53]  Regime features (12)  â† NEW!
```

### 3. CMakeLists.txt Update
Added `src/predictor/regime_features.cpp` to build system.

---

## Performance

### Build & Execution
- **Compilation:** âœ… SUCCESS (1 minor warning fixed)
- **Oct 20 Test:** âœ… RUNS SUCCESSFULLY
- **Execution Time:** 667ms (no noticeable slowdown)
- **Regime Feature Overhead:** ~0.5ms per bar (negligible)

### Current Results on Oct 20
**Without Regime Features (Phase 2 Best):**
- MRD: -0.010% (7 trades, 3W/4L, profit factor 0.76)

**With Regime Features (Initial Test):**
- MRD: -0.01% (6 trades, 3W/3L, profit factor 0.78)

**Note:** Results are similar because EWRLS hasn't been retrained yet on the new 54-feature input. The current EWRLS weights are optimized for 42 features only.

---

## Next Steps to See Benefits

### 1. Retrain EWRLS with New Features
The regime features are being fed to EWRLS, but EWRLS needs to learn their patterns through training.

**Option A: Quick Test (No Optimization)**
Run mock backtest on Aug-Oct with current config. EWRLS will learn online:
```bash
./build/sentio_lite mock --date 2025-08-01 --config config/strategy_phase2_best.json
# Watch if MRD improves as EWRLS learns the new features
```

**Option B: Full Optimization (Recommended)**
Re-run Optuna Phase 1 & 2 with 54 features:
```bash
python3 optimization/objective_function.py \
    --dates 2025-10-14,2025-10-15,2025-10-16,2025-10-17,2025-10-20 \
    --trials 200 \
    --phase 1
```

EWRLS will learn:
- "When `hmm_state_1_prob` is high (ranging) â†’ use mean reversion patterns"
- "When `gmm_vol_high_prob` is high â†’ reduce position sizes"
- "When `hmm_state_duration` is low (regime change) â†’ lower confidence"
- etc.

### 2. Expected Improvements

**Hypothesis:** Regime features should help on Oct 20 specifically because:
- Oct 20 was a choppy/ranging day (poor performance across all configs)
- Regime features will help EWRLS detect: `hmm_state_1_prob=high` + `gmm_vol_high_prob=high`
- EWRLS learns: "This combination = mean reversion works, momentum fails"
- Result: Better predictions â†’ More profitable trades

**Conservative Estimate:**
- Oct 20: -0.49% â†’ +0.15% (+0.64% improvement)
- Good days (Oct 14-17): +0.25% â†’ +0.35% (+0.10% improvement)
- **Overall MRD on 5 days: +0.140% â†’ +0.250%** (+0.11% improvement)

---

## Files Created/Modified

### Created:
1. `include/predictor/regime_features.h` - Regime feature interface
2. `src/predictor/regime_features.cpp` - Fast k-means regime detection
3. `scripts/regime_features.py` - Python prototype (for reference, not used)
4. `scripts/test_regime_features_real_data.py` - Testing script
5. `REGIME_FEATURES_IMPLEMENTATION.md` - This document
6. `docs/REGIME_FEATURES_FOR_EWRLS.md` - Design document

### Modified:
1. `include/predictor/feature_extractor.h` - Added regime feature indices
2. `src/predictor/feature_extractor.cpp` - Integrated regime extraction
3. `CMakeLists.txt` - Added regime_features.cpp to build

---

## Key Technical Decisions

### Why C++ Instead of Python?
- **Performance:** C++ k-means is ~10-20x faster than Python HMM/GMM
- **Simplicity:** No Python bridge needed (no IPC overhead)
- **Reliability:** No external dependencies (hmmlearn, scikit-learn)
- **Deployment:** Single binary, no Python environment issues

### Why K-Means Instead of HMM/GMM?
- **Speed:** K-means is O(N*k) vs HMM O(NÂ²), GMM O(N*kÂ²)
- **Sufficient:** K-means provides similar regime grouping
- **Soft Probabilities:** Inverse distance weighting gives smooth transitions
- **Real-time Ready:** <1ms per bar, suitable for live trading

### Why 12 Features?
Based on design doc `REGIME_FEATURES_FOR_EWRLS.md`:
- **3 HMM states:** Captures trending vs ranging vs transitioning
- **3 Vol regimes:** Captures calm vs normal vs volatile markets
- **2 Durations:** Captures regime stability (stable = more predictable)
- **4 Microstructure:** Captures vol expansion, extremes, price-vol dynamics

**Total: 12 features** - enough to be informative, not so many that EWRLS overfits.

---

## Validation Checklist

- [x] Regime features compile without errors
- [x] Feature vector has correct 54 dimensions
- [x] Regime features extract valid values [0, 1] or [-3, 3]
- [x] Mock mode runs successfully on Oct 20
- [x] No performance degradation (<1ms overhead)
- [x] Documentation complete
- [ ] **TODO:** Re-run Optuna with 54 features
- [ ] **TODO:** Validate MRD improvement on Oct 14-20
- [ ] **TODO:** Test on full Aug-Oct dataset

---

## Usage

The regime features are now **automatically extracted** for every bar. No code changes needed.

Simply run as before:
```bash
./build/sentio_lite mock --date 2025-10-20
```

EWRLS will receive 54 features (42 original + 12 regime) and learn their patterns online.

For best results, retrain with Optuna:
```bash
python3 optimization/objective_function.py \
    --dates 2025-10-14,2025-10-15,2025-10-16,2025-10-17,2025-10-20 \
    --trials 200 \
    --phase 1 \
    --output results/optuna_phase1_with_regime.db
```

---

## Conclusion

âœ… **Regime features successfully integrated into Sentio Lite**

The system now provides EWRLS with 12 additional regime-aware features that capture:
- Market state (trending vs ranging)
- Volatility regime (calm vs volatile)
- Regime stability (recent change vs stable)
- Microstructure dynamics (vol expansion, correlations)

**Next:** Retrain EWRLS via Optuna to let it learn the new patterns and improve MRD across all market conditions.

```

## ðŸ“„ **FILE 3 of 25**: REGIME_FEATURES_OPTUNA_RUN.md

**File Information**:
- **Path**: `REGIME_FEATURES_OPTUNA_RUN.md`
- **Size**: 164 lines
- **Modified**: 2025-10-21 14:28:56
- **Type**: md
- **Permissions**: -rw-r--r--

```text
# Optuna Phase 1 with 54 Features (Regime-Enhanced)

## Optimization Configuration

**Date:** October 21, 2025
**Study Name:** phase1_54features_21warmup
**Database:** `results/optuna_phase1.db`

### Parameters
- **Phase:** 1 (Quick wins - core parameters)
- **Trials:** 200
- **Training Dates:** Oct 14, 15, 16, 17, 20 (5 days)
- **Warmup:** 1 day (Note: script uses hardcoded 1-day, not 21-day)
- **Features:** 54 (42 original + 12 regime)
- **Sampler:** TPESampler (Bayesian optimization)
- **Pruner:** MedianPruner

### Feature Set
```
Total: 54 features

Original (42):
- 8 time features (cyclical encoding)
- 34 technical features (momentum, volatility, volume, BB, etc.)

NEW Regime Features (12):
- 3 HMM state probabilities (trending/ranging detection)
- 3 Volatility regime probabilities (low/med/high)
- 2 Regime stability (duration tracking)
- 4 Microstructure (vol ratios, correlations, z-scores)
```

## Optimization Space (Phase 1)

### Core Parameters Being Optimized:
1. `buy_threshold` [0.3, 0.7] - Probability threshold for buy signals
2. `sell_threshold` [0.3, 0.7] - Probability threshold for sell signals
3. `lambda_1bar` [0.95, 0.999] - EWRLS forgetting factor for 1-bar predictor
4. `lambda_5bar` [0.95, 0.999] - EWRLS forgetting factor for 5-bar predictor
5. `lambda_10bar` [0.95, 0.999] - EWRLS forgetting factor for 10-bar predictor
6. `max_positions` [1, 2, 3, 4] - Maximum concurrent positions
7. `rotation_strength_delta` [0.005, 0.015] - Rotation threshold
8. `min_confirmations_required` [0, 1, 2] - Signal confirmation requirement
9. `stop_loss_pct` [-0.025, -0.008] - Stop loss percentage
10. `profit_target_pct` [0.02, 0.06] - Profit target percentage
11. `bb_proximity_threshold` [0.05, 0.5] - Bollinger Band proximity
12. `bb_amplification_factor` [0.05, 0.3] - BB signal amplification

**Total:** 12 parameters

## Expected Performance Improvements

### Baseline (42 features, Phase 2 Best config):
- **Oct 14-17 (good days):** +0.25% to +0.35% MRD
- **Oct 20 (bad day):** -0.010% MRD
- **Average (5 days):** ~+0.140% MRD

### Expected with 54 Features:
- **Oct 14-17:** +0.30% to +0.45% MRD (+0.05-0.10% improvement)
- **Oct 20:** +0.05% to +0.15% MRD (+0.06-0.16% improvement!)
- **Average (5 days):** ~+0.250% to +0.300% MRD (+0.11-0.16% improvement)

### Why Regime Features Help:

**On Good Days (Oct 14-17):**
- Detect: `hmm_state_0_prob` high (trending)
- EWRLS learns: "Strong trend + stable regime â†’ increase position size, ride winners"
- Result: Slightly better MRD

**On Bad Days (Oct 20):**
- Detect: `hmm_state_1_prob` high (ranging) + `gmm_vol_high_prob` high
- EWRLS learns: "Choppy + volatile â†’ be selective, use mean reversion"
- Result: MUCH better MRD (from -0.010% to potentially +0.10%)

## Preliminary Results (First 10 Trials)

From the optimization log:

| Trial | MRD | Trades/Day | Win Rate | Notes |
|-------|-----|------------|----------|-------|
| 0 | -0.002% | 2.2 | 23.3% | Very conservative, few trades |
| 1 | -0.140% | 88.8 | 6.4% | Too aggressive, many losses |
| 2 | -0.396% | 88.2 | 3.1% | Worst so far |
| 3 | -0.016% | 2.2 | 16.7% | Conservative |
| 4 | -0.016% | 2.2 | 16.7% | Similar to #3 |
| 5 | -0.018% | 2.4 | 15.2% | Conservative |
| 6 | -0.274% | 99.4 | 5.5% | Too aggressive |
| 7 | -0.016% | 2.2 | 16.7% | Conservative |
| 8 | -0.186% | 62.4 | 11.5% | Moderate losses |
| 9 | +0.000% | 2.2 | 23.3% | **BREAK EVEN!** |
| 10 | **+0.018%** | 2.2 | 23.3% | **FIRST POSITIVE!** âœ… |

**Best so far: Trial 10 with +0.018% MRD**

Early observations:
- Conservative strategies (2-3 trades/day) perform better
- Aggressive strategies (88+ trades/day) lose money
- Need higher win rates (20%+) for profitability
- Trial 10 shows regime features can achieve positive MRD!

## Status

**Currently running:** Trial 11+ / 200
**Estimated completion:** ~8-10 minutes total
**Progress:** Auto-saving to `results/optuna_phase1.db`

## Next Steps After Completion

1. **Select Best Trial:**
   ```bash
   python3 optimization/select_best_trial.py \
       --database results/optuna_phase1.db \
       --study phase1_54features_21warmup \
       --strategy robust
   ```

2. **Export Best Config:**
   - Extract best parameters
   - Save as `config/strategy_phase1_54feat_best.json`

3. **Validate on Oct 20:**
   ```bash
   ./build/sentio_lite mock --date 2025-10-20 \
       --config config/strategy_phase1_54feat_best.json \
       --warmup-days 21
   ```

4. **Compare Results:**
   - 42 features Phase 2 Best: -0.010% MRD on Oct 20
   - 54 features Phase 1 Best: ??? % MRD on Oct 20
   - **Target:** +0.10% or better

5. **Run Phase 2 if Successful:**
   ```bash
   python3 optimization/optuna_optimizer.py \
       --phase 2 \
       --trials 200 \
       --base-config config/strategy_phase1_54feat_best.json \
       --dates 2025-10-14 2025-10-15 2025-10-16 2025-10-17 2025-10-20
   ```

---

## Key Questions to Answer

1. **Did regime features improve MRD?**
   - Compare best 54-feat MRD vs best 42-feat MRD
   - Target: +0.10% to +0.15% improvement

2. **Did they help on Oct 20 specifically?**
   - Test best config on Oct 20 with 21-day warmup
   - Target: Turn -0.010% into +0.10% or better

3. **What lambda values work best with 54 features?**
   - Compare to 42-feature best: Î»â‚=0.979, Î»â‚…=0.981, Î»â‚â‚€=0.975
   - Do we need different forgetting factors with regime info?

4. **How many trades are optimal?**
   - Early trials suggest 2-3 trades/day works best
   - Aggressive strategies (88+ trades) fail

---

*This optimization run will determine if the 12 regime features successfully improve EWRLS performance, especially on difficult days like Oct 20.*

```

## ðŸ“„ **FILE 4 of 25**: config/strategy_phase1_best.json

**File Information**:
- **Path**: `config/strategy_phase1_best.json`
- **Size**: 79 lines
- **Modified**: 2025-10-21 14:25:19
- **Type**: json
- **Permissions**: -rw-r--r--

```text
{
  "_comment": "Sentio Lite Trading Strategy Configuration - Complete (71 parameters)",
  "_version": "1.0.2",
  "_description": "Baseline EWRLS v1.0.1 - All parameters externalized for Optuna optimization",
  "initial_capital": 100000.0,
  "max_positions": 3,
  "stop_loss_pct": -0.022848386562814495,
  "profit_target_pct": 0.043555592222521665,
  "bars_per_day": 391,
  "eod_liquidation": true,
  "min_bars_to_learn": 100,
  "lookback_window": 50,
  "horizon_config": {
    "lambda_1bar": 0.9611433605793056,
    "lambda_5bar": 0.9745639479591365,
    "lambda_10bar": 0.9745067358776138,
    "min_confidence": 0.4
  },
  "filter_config": {
    "min_bars_to_hold": 20,
    "typical_hold_period": 60,
    "max_bars_to_hold": 120,
    "min_prediction_for_entry": 0.0,
    "min_confidence_for_entry": 0.0
  },
  "enable_probability_scaling": true,
  "probability_scaling_factor": 50.0,
  "buy_threshold": 0.5992968469241474,
  "sell_threshold": 0.45137166806173395,
  "enable_bb_amplification": false,
  "bb_period": 20,
  "bb_std_dev": 2.0,
  "bb_proximity_threshold": 0.23784221275767636,
  "bb_amplification_factor": 0.14025910631760477,
  "enable_rotation": true,
  "rotation_strength_delta": 0.018306619084026022,
  "rotation_cooldown_bars": 10,
  "min_rank_strength": 0.001,
  "enable_signal_confirmation": true,
  "min_confirmations_required": 2,
  "rsi_oversold_threshold": 0.3,
  "rsi_overbought_threshold": 0.7,
  "bb_extreme_threshold": 0.8,
  "volume_surge_threshold": 1.2,
  "enable_price_based_exits": true,
  "exit_on_ma_crossover": true,
  "trailing_stop_percentage": 0.5,
  "ma_exit_period": 10,
  "enable_dual_ewrls": false,
  "dual_ewrls_ma_period": 20,
  "dual_ewrls_min_deviation": 0.005,
  "enable_mean_reversion_predictor": false,
  "reversion_factor": 0.5,
  "ma_period_1bar": 5,
  "ma_period_5bar": 10,
  "ma_period_10bar": 20,
  "position_sizing": {
    "win_multiplier": 1.3,
    "loss_multiplier": 0.7,
    "trade_history_size": 3
  },
  "cost_model": {
    "enable_cost_tracking": true,
    "default_avg_volume": 1000000.0,
    "default_volatility": 0.02,
    "base_slippage_bps": 1.0,
    "size_impact_factor": 0.5,
    "volatility_multiplier": 1.5,
    "time_of_day_factor": 1.0
  },
  "ewrls_config": {
    "initial_variance": 100.0,
    "max_variance": 1000.0,
    "max_gradient_norm": 1.0,
    "stability_check_interval": 100
  },
  "min_confidence_weight": 0.5030873473424331,
  "max_confidence_weight": 1.5831702036557977
}
```

## ðŸ“„ **FILE 5 of 25**: config/strategy_phase2_best.json

**File Information**:
- **Path**: `config/strategy_phase2_best.json`
- **Size**: 79 lines
- **Modified**: 2025-10-21 13:30:29
- **Type**: json
- **Permissions**: -rw-r--r--

```text
{
  "_comment": "Sentio Lite Trading Strategy Configuration - Complete (71 parameters)",
  "_version": "1.0.2",
  "_description": "Baseline EWRLS v1.0.1 - All parameters externalized for Optuna optimization",
  "initial_capital": 100000.0,
  "max_positions": 3,
  "stop_loss_pct": -0.014781044800365451,
  "profit_target_pct": 0.048322903111841814,
  "bars_per_day": 391,
  "eod_liquidation": true,
  "min_bars_to_learn": 100,
  "lookback_window": 50,
  "horizon_config": {
    "lambda_1bar": 0.9792797576724562,
    "lambda_5bar": 0.9809530469468962,
    "lambda_10bar": 0.9745245405728307,
    "min_confidence": 0.4
  },
  "filter_config": {
    "min_bars_to_hold": 12,
    "typical_hold_period": 60,
    "max_bars_to_hold": 189,
    "min_prediction_for_entry": 0.0,
    "min_confidence_for_entry": 0.0
  },
  "enable_probability_scaling": true,
  "probability_scaling_factor": 59.10770590330201,
  "buy_threshold": 0.5061810178271043,
  "sell_threshold": 0.47112857515378487,
  "enable_bb_amplification": false,
  "bb_period": 20,
  "bb_std_dev": 2.0,
  "bb_proximity_threshold": 0.10823379771832098,
  "bb_amplification_factor": 0.2924774630404986,
  "enable_rotation": true,
  "rotation_strength_delta": 0.005871254182522992,
  "rotation_cooldown_bars": 10,
  "min_rank_strength": 0.001,
  "enable_signal_confirmation": true,
  "min_confirmations_required": 2,
  "rsi_oversold_threshold": 0.39856204452396105,
  "rsi_overbought_threshold": 0.7997310174116078,
  "bb_extreme_threshold": 0.8348504492185607,
  "volume_surge_threshold": 1.0299274630604756,
  "enable_price_based_exits": true,
  "exit_on_ma_crossover": true,
  "trailing_stop_percentage": 0.6343395855673575,
  "ma_exit_period": 10,
  "enable_dual_ewrls": false,
  "dual_ewrls_ma_period": 20,
  "dual_ewrls_min_deviation": 0.005,
  "enable_mean_reversion_predictor": false,
  "reversion_factor": 0.5,
  "ma_period_1bar": 5,
  "ma_period_5bar": 10,
  "ma_period_10bar": 20,
  "position_sizing": {
    "win_multiplier": 1.3,
    "loss_multiplier": 0.7,
    "trade_history_size": 3
  },
  "cost_model": {
    "enable_cost_tracking": true,
    "default_avg_volume": 1000000.0,
    "default_volatility": 0.02,
    "base_slippage_bps": 1.0,
    "size_impact_factor": 0.5,
    "volatility_multiplier": 1.5,
    "time_of_day_factor": 1.0
  },
  "ewrls_config": {
    "initial_variance": 100.0,
    "max_variance": 1000.0,
    "max_gradient_norm": 1.0,
    "stability_check_interval": 100
  },
  "min_confidence_weight": 0.5030873473424331,
  "max_confidence_weight": 1.5831702036557977
}
```

## ðŸ“„ **FILE 6 of 25**: docs/REGIME_ADAPTIVE_FEATURE_MODEL.md

**File Information**:
- **Path**: `docs/REGIME_ADAPTIVE_FEATURE_MODEL.md`
- **Size**: 300 lines
- **Modified**: 2025-10-21 13:47:50
- **Type**: md
- **Permissions**: -rw-r--r--

```text
# Regime-Adaptive Feature Model for MRD Maximization

## Philosophy Shift
**OLD:** Detect bad regimes â†’ skip trading â†’ preserve capital
**NEW:** Detect ALL regimes â†’ adapt strategy â†’ exploit opportunities â†’ increase MRD

Every market condition has profitable trades - we just need different strategies.

---

## Market Regime Taxonomy (3 Regimes Ã— 3 Volatility Levels = 9 States)

### Primary Regimes (HMM):
1. **TRENDING_UP** (bullish momentum)
2. **TRENDING_DOWN** (bearish momentum)  
3. **RANGING** (mean-reverting, choppy)

### Volatility Overlays (GMM):
- **LOW_VOL** (tight ranges, scalping opportunities)
- **MEDIUM_VOL** (normal conditions)
- **HIGH_VOL** (large moves, risk/reward)

### Combined States (9 total):
```
TRENDING_UP + LOW_VOL    â†’ Grind higher (buy dips, small stops)
TRENDING_UP + MED_VOL    â†’ Strong trend (momentum, ride winners)
TRENDING_UP + HIGH_VOL   â†’ Volatile rally (quick profits, wide stops)

TRENDING_DOWN + LOW_VOL  â†’ Slow bleed (short bounces, tight stops)
TRENDING_DOWN + MED_VOL  â†’ Strong decline (fade rallies)
TRENDING_DOWN + HIGH_VOL â†’ Crash/panic (fade extremes, quick exits)

RANGING + LOW_VOL        â†’ Tight range (scalp extremes, many trades)
RANGING + MED_VOL        â†’ Normal chop (fade overbought/oversold)
RANGING + HIGH_VOL       â†’ Whipsaw (AVOID or very quick trades) âš ï¸
```

---

## Feature Vector Design: Regime-Adaptive Signals

### Core Principle:
**Each regime needs DIFFERENT trading signals**

### Feature Set (16 features):

#### A. Regime Identification (3 features)
```
1. primary_regime        [0, 1, 2]     # TREND_UP, TREND_DN, RANGE
2. volatility_regime     [0, 1, 2]     # LOW, MED, HIGH
3. regime_composite      [0-8]         # Combined state (9 states)
```

#### B. Regime-Specific Opportunity Scores (9 features)
**Key Innovation:** Generate different signals based on regime

```
# TRENDING regimes â†’ Momentum signals
4. trend_strength        [-1.0, 1.0]   # ADX-based (high = strong trend)
5. trend_acceleration    [-1.0, 1.0]   # Is trend speeding up?
6. pullback_opportunity  [0.0, 1.0]    # Dip in uptrend = buy signal

# RANGING regimes â†’ Mean reversion signals  
7. range_position        [0.0, 1.0]    # 0=bottom, 0.5=middle, 1.0=top
8. range_extreme         [0.0, 1.0]    # How far from mean? (0=mean, 1=extreme)
9. reversal_pressure     [-1.0, 1.0]   # RSI divergence, volume climax

# VOLATILITY signals â†’ Risk/reward adjustments
10. vol_expansion        [0.0, 1.0]    # Is volatility increasing?
11. vol_percentile       [0.0, 1.0]    # Current vol vs 60-bar history
12. vol_stability        [0, 60]       # Bars since vol regime change
```

#### C. Microstructure Features (4 features)
**Exploit intraday patterns**

```
13. volume_pressure      [-1.0, 1.0]   # Buying vs selling volume
14. price_momentum_1min  [-1.0, 1.0]   # Very short-term momentum
15. bid_ask_imbalance    [-1.0, 1.0]   # Order flow direction
16. tick_pressure        [-1.0, 1.0]   # Upticks vs downticks
```

---

## Strategy Adaptation Logic

### Instead of ONE strategy, use REGIME-SPECIFIC strategies:

```python
def get_trading_signal(regime_composite, features):
    """
    Generate different signals based on regime
    Returns: (signal_strength, confidence, position_size_mult, stop_mult)
    """
    
    if regime_composite == 0:  # TRENDING_UP + LOW_VOL
        # Strategy: Buy dips, small stops, hold longer
        signal = features['pullback_opportunity'] * 0.8
        confidence = features['trend_strength']
        size_mult = 1.5  # Larger positions in stable trends
        stop_mult = 0.7  # Tighter stops
        
    elif regime_composite == 1:  # TRENDING_UP + MED_VOL
        # Strategy: Ride momentum, normal parameters
        signal = features['trend_acceleration'] * 1.0
        confidence = features['trend_strength'] * 0.9
        size_mult = 1.0
        stop_mult = 1.0
        
    elif regime_composite == 2:  # TRENDING_UP + HIGH_VOL
        # Strategy: Quick profits, wider stops
        signal = features['pullback_opportunity'] * features['vol_expansion']
        confidence = 0.7  # Lower confidence in volatile trends
        size_mult = 0.8  # Smaller positions
        stop_mult = 1.5  # Wider stops
        
    elif regime_composite == 6:  # RANGING + LOW_VOL
        # Strategy: Scalp range extremes, many trades
        if features['range_position'] > 0.8:
            signal = -0.6  # Fade top of range
        elif features['range_position'] < 0.2:
            signal = 0.6   # Buy bottom of range
        else:
            signal = 0.0
        confidence = features['range_extreme']
        size_mult = 1.2  # More trades, smaller size each
        stop_mult = 0.5  # Very tight stops in range
        
    elif regime_composite == 7:  # RANGING + MED_VOL
        # Strategy: Mean reversion, wait for extremes
        signal = -features['range_position'] * 2 + 1  # Linear fade
        signal *= features['reversal_pressure']
        confidence = features['range_extreme'] * 0.8
        size_mult = 1.0
        stop_mult = 0.8
        
    elif regime_composite == 8:  # RANGING + HIGH_VOL (WHIPSAW)
        # Strategy: VERY selective, only trade clear extremes
        if features['range_extreme'] > 0.9 and abs(features['reversal_pressure']) > 0.7:
            signal = -features['range_position'] * 2 + 1
            confidence = 0.5  # Low confidence, but opportunity exists
            size_mult = 0.5  # Small positions
            stop_mult = 0.6  # Quick exits
        else:
            signal = 0.0  # Wait for setup
            
    # ... etc for all 9 regimes
    
    return signal, confidence, size_mult, stop_mult
```

---

## How This Increases MRD on Oct 20

### Oct 20 Analysis (Hypothetical):
**Regime:** RANGING + HIGH_VOL (choppy, volatile)

### Old Approach:
- Used TRENDING strategy on RANGING market â†’ losses
- Fixed position sizes â†’ too large for volatility
- Normal stops â†’ stopped out repeatedly

### NEW Approach with Regime Features:
1. **Detect regime:** RANGING + HIGH_VOL (composite state 8)
2. **Switch strategy:** 
   - Wait for extreme range positions (top/bottom 10%)
   - Fade extremes with mean reversion
   - Use 0.5Ã— position size (smaller risk)
   - Use 0.6Ã— stop size (quick exits on failure)
   - Trade BOTH directions (long bottoms, short tops)
3. **Result:** Turn -0.49% MRD into potentially +0.2% MRD

### Expected Trades on Oct 20 with Regime Adaptation:
Instead of 45 trending trades (6W, 29L):
- 20-30 mean-reversion trades
- 60% win rate (fading extremes works in ranging markets)
- Smaller avg loss (tight stops)
- Many small wins accumulate

---

## EWRLS Integration: Regime-Weighted Predictions

### Current EWRLS:
```cpp
prediction = Î»â‚Â·pred_1bar + Î»â‚…Â·pred_5bar + Î»â‚â‚€Â·pred_10bar
```

### Enhanced EWRLS with Regime Adaptation:
```cpp
// Step 1: Get base predictions
pred_1bar = ewrls_1.predict(features);
pred_5bar = ewrls_5.predict(features);
pred_10bar = ewrls_10.predict(features);

// Step 2: Detect regime
regime = regime_detector.get_composite_state(bars);

// Step 3: Adjust lambda weights based on regime
if (regime == TRENDING_MED) {
    // In trends, long-term predictions matter more
    Î»â‚ = 0.2;
    Î»â‚… = 0.3;
    Î»â‚â‚€ = 0.5;  // Favor 10-bar (trend continuation)
    
} else if (regime == RANGING_LOW) {
    // In ranges, short-term predictions matter more
    Î»â‚ = 0.6;   // Favor 1-bar (mean reversion)
    Î»â‚… = 0.3;
    Î»â‚â‚€ = 0.1;
    
} else if (regime == RANGING_HIGH) {
    // In whipsaw, be very selective
    Î»â‚ = 0.4;
    Î»â‚… = 0.4;
    Î»â‚â‚€ = 0.2;
}

// Step 4: Generate regime-specific signal
regime_signal = get_regime_signal(regime, features);

// Step 5: Combine
final_prediction = regime_signal * 0.3 + ewrls_prediction * 0.7;

// Step 6: Adjust position sizing
position_size = base_size * regime_size_multiplier[regime];
stop_loss = base_stop * regime_stop_multiplier[regime];
```

---

## Optuna Parameters for Regime Adaptation

### Phase 3: Regime-Adaptive Parameters

```python
# Regime-specific lambda weights (27 parameters: 9 regimes Ã— 3 lambdas)
'lambda_1bar_trending_up_low': [0.1, 0.9]
'lambda_5bar_trending_up_low': [0.1, 0.9]
'lambda_10bar_trending_up_low': [0.1, 0.9]
... (repeat for all 9 regimes)

# Regime-specific position sizing (9 parameters)
'size_mult_trending_up_low': [0.5, 2.0]
'size_mult_trending_up_med': [0.5, 2.0]
... (repeat for all 9 regimes)

# Regime-specific stops (9 parameters)
'stop_mult_trending_up_low': [0.3, 2.0]
... (repeat for all 9 regimes)

# Signal generation weights (4 parameters)
'trend_signal_weight': [0.0, 1.0]      # Weight of trend signals
'range_signal_weight': [0.0, 1.0]      # Weight of mean reversion
'vol_signal_weight': [0.0, 1.0]        # Weight of volatility signals
'micro_signal_weight': [0.0, 1.0]      # Weight of microstructure

# Regime detection sensitivity (3 parameters)
'hmm_n_states': [2, 3, 4]              # 2-4 primary regimes
'vol_n_clusters': [2, 3, 4]            # 2-4 volatility regimes  
'regime_confidence_threshold': [0.5, 0.9]  # Min confidence to switch
```

**Total new parameters:** ~50-60 (but high impact!)

---

## Expected MRD Improvement

### Current Results (Oct 20):
- Phase 1: -0.02% MRD (1 trade)
- Phase 2: -0.01% MRD (7 trades)

### With Regime Adaptation (Projection):
- Detect: RANGING + HIGH_VOL
- Strategy: Mean reversion with small positions
- Trades: 25-40 (more activity, smaller each)
- Win rate: 55-60% (mean reversion works in ranges)
- **Expected MRD: +0.15% to +0.30%** âœ“

### On Trending Days (Oct 14, 15, 16):
- Detect: TRENDING + MED_VOL
- Strategy: Momentum with larger positions
- **Expected MRD: +0.50% to +0.80%** (better than current +0.25%)

---

## Implementation Priority

1. **Build Python regime detector** (FastRegimeDetector class)
2. **Generate 16 regime features** per bar
3. **Add C++ regime state enum** (9 composite states)
4. **Implement regime-specific trading logic**
5. **Run Optuna Phase 3** to optimize all regime-specific parameters
6. **Backtest on Oct 14-20** to validate improvement

**Key Metric:** Does MRD improve across ALL days (not just good days)?


```

## ðŸ“„ **FILE 7 of 25**: docs/REGIME_DETECTION_FEATURE_DESIGN.md

**File Information**:
- **Path**: `docs/REGIME_DETECTION_FEATURE_DESIGN.md`
- **Size**: 196 lines
- **Modified**: 2025-10-21 13:45:08
- **Type**: md
- **Permissions**: -rw-r--r--

```text
# Fast Regime Detection Feature Design

## Overview
Add real-time regime detection to improve EWRLS predictions by identifying market conditions.

## Three Fast Methods (60-120 bar window)

### 1. **HMM (hmmlearn)** - Pattern-based
**Speed:** ~1-5ms per update
**States:** 2-3 regimes
**Features Input:**
- Log returns (volatility)
- Price momentum (ROC)
- Volume ratio

**Output:** Regime label (0=ranging, 1=trending, 2=volatile)

### 2. **GMM (sklearn)** - Volatility-based  
**Speed:** ~0.5-2ms per update
**Components:** 3 clusters
**Features Input:**
- Rolling volatility (20-bar std)
- ATR (14-bar)
- Return distribution shape

**Output:** Volatility regime (0=low, 1=medium, 2=high)

### 3. **Ruptures** - Change point detection
**Speed:** ~0.1-1ms per detection
**Window:** 60 bars
**Features Input:**
- Price series
- Volume series

**Output:** Bars since last regime change (0-60)

---

## Proposed Feature Vector for EWRLS

### Current Features (EWRLS):
- 1-bar prediction
- 5-bar prediction  
- 10-bar prediction
- Confidence weights

### NEW Regime Features (8 total):

```
1. hmm_regime           [0, 1, 2]      # HMM state
2. hmm_confidence       [0.0, 1.0]     # Probability of current state
3. gmm_vol_regime       [0, 1, 2]      # Volatility cluster
4. gmm_vol_score        [0.0, 1.0]     # Distance to cluster center
5. regime_stability     [0, 60]        # Bars since last change
6. regime_changed       [0, 1]         # Binary flag (changed in last 5 bars)
7. current_volatility   [0.0, inf]     # Normalized rolling std
8. volume_regime        [0.0, 3.0]     # Volume relative to average
```

---

## Integration Strategy

### Phase 1: Feature Generation (Python)
```python
# scripts/regime_detector.py
class FastRegimeDetector:
    def __init__(self, window=90):
        self.hmm = GaussianHMM(n_components=3)
        self.gmm = GaussianMixture(n_components=3)
        self.detector = ruptures.Binseg(model="rbf")
        self.window = window
        
    def update(self, prices, volumes, bar_idx):
        """Update with new bar, return regime features"""
        # Fast: only use last 'window' bars
        recent_prices = prices[-self.window:]
        recent_volumes = volumes[-self.window:]
        
        # Compute features
        returns = np.diff(np.log(recent_prices))
        vol = returns.std()
        
        # HMM regime (refit every 30 bars for speed)
        if bar_idx % 30 == 0:
            self.hmm.fit(returns.reshape(-1, 1))
        hmm_state = self.hmm.predict(returns[-10:].reshape(-1, 1))[-1]
        hmm_prob = self.hmm.predict_proba(returns[-10:].reshape(-1, 1))[-1].max()
        
        # GMM volatility regime
        vol_window = pd.Series(returns).rolling(20).std().dropna()
        self.gmm.fit(vol_window.values.reshape(-1, 1))
        vol_regime = self.gmm.predict(vol_window[-1:].reshape(-1, 1))[0]
        
        # Ruptures change detection
        change_points = self.detector.fit_predict(recent_prices, n_bkps=3)
        bars_since_change = len(recent_prices) - max(change_points[:-1])
        
        return {
            'hmm_regime': hmm_state,
            'hmm_confidence': hmm_prob,
            'gmm_vol_regime': vol_regime,
            'gmm_vol_score': 1.0,  # TODO: compute distance
            'regime_stability': bars_since_change,
            'regime_changed': int(bars_since_change < 5),
            'current_volatility': vol,
            'volume_regime': recent_volumes[-1] / recent_volumes.mean()
        }
```

### Phase 2: C++ Integration
```cpp
// include/prediction/regime_detector.h
struct RegimeFeatures {
    int hmm_regime;           // 0-2
    double hmm_confidence;    // 0.0-1.0
    int gmm_vol_regime;       // 0-2
    double gmm_vol_score;     // 0.0-1.0
    int regime_stability;     // bars since change
    bool regime_changed;      // flag
    double current_volatility;
    double volume_regime;
};

class RegimeDetector {
    // Call Python bridge for regime detection
    RegimeFeatures detect(const std::vector<Bar>& bars, int current_idx);
};
```

### Phase 3: EWRLS Enhancement
```cpp
// Modify EWRLS predictor to use regime features
struct EnhancedPrediction {
    double value;
    double confidence;
    RegimeFeatures regime;  // NEW
};

// In trading logic:
auto pred = ewrls.predict(bars, idx);
auto regime = detector.detect(bars, idx);

// Adjust confidence based on regime
if (regime.regime_changed) {
    pred.confidence *= 0.5;  // Lower confidence during regime change
}
if (regime.gmm_vol_regime == 2) {  // High volatility
    pred.confidence *= 0.7;  // Reduce confidence
}
```

---

## Performance Expectations

### Speed (per bar update):
- HMM: ~2ms (refit every 30 bars)
- GMM: ~1ms (refit every update)
- Ruptures: ~0.5ms
- **Total: ~3.5ms overhead**

### Memory:
- ~10KB per symbol (60-120 bar window)

### Benefit:
- **Better trade filtering** during regime changes
- **Adaptive confidence** based on market condition
- **Avoid bad trades** in choppy/transitioning markets

---

## Optuna Optimization

Add these parameters to Phase 2/3:

```python
'regime_change_penalty': [0.3, 0.5, 0.7, 1.0]      # Confidence multiplier
'high_vol_penalty': [0.5, 0.7, 0.9, 1.0]           # Confidence in high vol
'min_regime_stability': [5, 10, 15, 20]            # Min bars before trading
'regime_filter_enabled': [True, False]             # Enable/disable feature
```

---

## Expected Impact on Oct 20

Oct 20 showed negative returns across all configs. With regime detection:
- Detect if Oct 20 was a "regime change" day
- **Reduce trading activity** or skip trading entirely
- Preserve capital during unfavorable conditions

**Hypothesis:** Oct 20 likely had multiple regime changes (choppy market),
which caused all strategies to fail. Regime detection would have flagged
this and reduced/prevented trading.


```

## ðŸ“„ **FILE 8 of 25**: docs/REGIME_FEATURES_FOR_EWRLS.md

**File Information**:
- **Path**: `docs/REGIME_FEATURES_FOR_EWRLS.md`
- **Size**: 395 lines
- **Modified**: 2025-10-21 13:50:44
- **Type**: md
- **Permissions**: -rw-r--r--

```text
# Regime Features for EWRLS - Feature Model Only

## Core Principle
**We're NOT building a trading algo**
**We're adding features that help EWRLS make better predictions**

EWRLS will learn the patterns. We just provide informative features.

---

## Feature Design Philosophy

### Current EWRLS Input:
```
X = [price, volume, simple_features] â†’ EWRLS â†’ prediction
```

### Enhanced EWRLS Input:
```
X = [price, volume, simple_features, REGIME_FEATURES] â†’ EWRLS â†’ better_prediction
```

The regime features give EWRLS **context** about market conditions.

---

## Regime Feature Vector (12 features)

### Goal: Help EWRLS understand "what kind of market is this?"

### 1. Market State Features (3 features)

```python
# Feature 1: HMM State Probability
hmm_state_0_prob   [0.0, 1.0]    # Probability of being in state 0
hmm_state_1_prob   [0.0, 1.0]    # Probability of being in state 1  
hmm_state_2_prob   [0.0, 1.0]    # Probability of being in state 2
# Sum to 1.0
# EWRLS learns: "When state_0_prob is high, next-bar returns behave like X"
```

**What EWRLS learns:**
- State 0 might = trending up â†’ next bar likely positive
- State 1 might = ranging â†’ next bar mean-reverting
- State 2 might = trending down â†’ next bar likely negative

**EWRLS figures out the mapping, not us!**

### 2. Volatility Regime Features (3 features)

```python
# Feature 2: GMM Volatility Cluster Probabilities
gmm_vol_low_prob   [0.0, 1.0]    # Prob of low volatility regime
gmm_vol_med_prob   [0.0, 1.0]    # Prob of medium volatility
gmm_vol_high_prob  [0.0, 1.0]    # Prob of high volatility
# Sum to 1.0
# EWRLS learns: "When vol_high_prob is high, predictions need different confidence"
```

**What EWRLS learns:**
- In low vol: smaller price moves, tighter predictions
- In high vol: larger moves, wider predictions
- Adjust prediction magnitude based on vol regime

### 3. Regime Stability Features (2 features)

```python
# Feature 3: Regime Persistence
hmm_state_duration    [0, 120]       # Bars since last HMM state change
vol_regime_duration   [0, 120]       # Bars since last vol regime change

# EWRLS learns: "Early in new regime (low duration) = less predictable"
#               "Stable regime (high duration) = more predictable"
```

**What EWRLS learns:**
- Fresh regime changes â†’ lower confidence
- Stable regimes â†’ higher confidence
- Adjust prediction confidence dynamically

### 4. Market Microstructure Features (4 features)

```python
# Feature 4: Recent Volatility Context
vol_ratio_20_60   [-3.0, 3.0]    # Current 20-bar vol / 60-bar vol
                                   # >1 = expanding, <1 = contracting

vol_zscore        [-3.0, 3.0]     # Vol z-score vs 60-bar history
                                   # How extreme is current vol?

# Feature 5: Price-Volume Relationship  
price_vol_correlation  [-1.0, 1.0]   # 20-bar correlation
                                       # Positive = healthy trend
                                       # Negative = divergence

volume_zscore     [-3.0, 3.0]     # Volume z-score vs 60-bar history
                                   # Unusual volume = regime change?
```

**What EWRLS learns:**
- Expanding volatility â†’ adjust prediction scale
- Vol extremes (high z-score) â†’ less predictable
- Price-vol divergence â†’ potential reversal
- Volume spikes â†’ trend change or continuation?

---

## Complete Feature Vector for EWRLS

### Current Features (~10):
```
- Current price
- Recent returns (1, 5, 10 bars)
- Volume
- RSI
- BB position
- ... etc
```

### NEW Regime Features (+12):
```
1.  hmm_state_0_prob      [0.0, 1.0]
2.  hmm_state_1_prob      [0.0, 1.0]
3.  hmm_state_2_prob      [0.0, 1.0]
4.  gmm_vol_low_prob      [0.0, 1.0]
5.  gmm_vol_med_prob      [0.0, 1.0]
6.  gmm_vol_high_prob     [0.0, 1.0]
7.  hmm_state_duration    [0, 120]
8.  vol_regime_duration   [0, 120]
9.  vol_ratio_20_60       [-3, 3]
10. vol_zscore            [-3, 3]
11. price_vol_correlation [-1, 1]
12. volume_zscore         [-3, 3]
```

### Total EWRLS Features: ~22

---

## How EWRLS Uses These Features

### Example Learning Scenarios:

#### Scenario 1: Trending Market
```
Input features:
  hmm_state_0_prob = 0.9      # High confidence in state 0
  gmm_vol_med_prob = 0.8      # Medium volatility
  hmm_state_duration = 45     # Stable regime (45 bars)
  vol_ratio_20_60 = 1.1       # Slightly expanding
  price_vol_correlation = 0.7 # Strong positive

EWRLS learns:
  â†’ Next 1-bar return: +0.15% (momentum continues)
  â†’ Next 5-bar return: +0.45% (trend extends)
  â†’ Confidence: HIGH (stable regime)
```

#### Scenario 2: Regime Change
```
Input features:
  hmm_state_0_prob = 0.4      # Uncertain state
  hmm_state_1_prob = 0.35     # Almost equal probabilities
  hmm_state_2_prob = 0.25     # No clear regime
  hmm_state_duration = 2      # Just changed!
  vol_zscore = 2.5            # High volatility spike
  price_vol_correlation = -0.3 # Divergence

EWRLS learns:
  â†’ Next 1-bar return: 0.0% Â± 0.5% (unpredictable)
  â†’ Confidence: LOW (regime transition)
  â†’ Don't trade! (low confidence signals filtered)
```

#### Scenario 3: Ranging Market (Oct 20 case)
```
Input features:
  hmm_state_1_prob = 0.85     # High confidence in state 1 (ranging)
  gmm_vol_high_prob = 0.7     # High volatility
  hmm_state_duration = 30     # Stable ranging (30 bars)
  vol_ratio_20_60 = 1.8       # Volatility expanding
  price_vol_correlation = 0.1 # Weak correlation

EWRLS learns:
  â†’ When price at range top: next return = -0.2% (mean revert)
  â†’ When price at range bottom: next return = +0.2% (bounce)
  â†’ Confidence: MEDIUM (high vol reduces confidence)
  â†’ Trade mean reversion, small size
```

**Key: EWRLS figures out the patterns automatically from historical data!**

---

## Implementation: Pure Feature Generation

### Python Module (Fast, Clean)

```python
# scripts/regime_features.py
import numpy as np
from hmmlearn import hmm
from sklearn.mixture import GaussianMixture

class RegimeFeatureGenerator:
    """
    Generate regime-aware features for EWRLS
    NO trading logic - just features!
    """
    
    def __init__(self, window=90):
        self.window = window
        self.hmm = hmm.GaussianHMM(n_components=3, covariance_type="full")
        self.gmm = GaussianMixture(n_components=3)
        
        # State tracking
        self.last_hmm_state = None
        self.last_vol_regime = None
        self.hmm_state_duration = 0
        self.vol_regime_duration = 0
        
    def generate_features(self, prices, volumes, bar_idx):
        """
        Generate 12 regime features
        Returns: dict of features
        """
        # Use recent window
        p = prices[-self.window:]
        v = volumes[-self.window:]
        
        # Compute returns
        returns = np.diff(np.log(p))
        
        # === HMM State Features (3) ===
        # Refit HMM every 30 bars for speed
        if bar_idx % 30 == 0 or self.last_hmm_state is None:
            self.hmm.fit(returns.reshape(-1, 1))
        
        # Get state probabilities for last bar
        state_probs = self.hmm.predict_proba(returns[-10:].reshape(-1, 1))[-1]
        current_state = state_probs.argmax()
        
        # Track duration
        if current_state != self.last_hmm_state:
            self.hmm_state_duration = 0
            self.last_hmm_state = current_state
        else:
            self.hmm_state_duration += 1
        
        # === GMM Volatility Features (3) ===
        # Rolling volatility
        vol_series = pd.Series(returns).rolling(20).std().dropna()
        
        if len(vol_series) > 30:
            self.gmm.fit(vol_series.values.reshape(-1, 1))
            vol_probs = self.gmm.predict_proba(vol_series[-1:].reshape(-1, 1))[0]
            current_vol_regime = vol_probs.argmax()
            
            # Track duration
            if current_vol_regime != self.last_vol_regime:
                self.vol_regime_duration = 0
                self.last_vol_regime = current_vol_regime
            else:
                self.vol_regime_duration += 1
        else:
            vol_probs = [0.33, 0.33, 0.34]
            self.vol_regime_duration = 0
        
        # === Microstructure Features (4) ===
        current_vol_20 = vol_series[-1] if len(vol_series) > 0 else 0.01
        avg_vol_60 = vol_series[-60:].mean() if len(vol_series) >= 60 else 0.01
        vol_ratio = current_vol_20 / (avg_vol_60 + 1e-8)
        
        vol_mean_60 = vol_series[-60:].mean() if len(vol_series) >= 60 else 0.01
        vol_std_60 = vol_series[-60:].std() if len(vol_series) >= 60 else 0.01
        vol_zscore = (current_vol_20 - vol_mean_60) / (vol_std_60 + 1e-8)
        
        # Price-volume correlation (20 bars)
        if len(p) >= 20 and len(v) >= 20:
            price_vol_corr = np.corrcoef(p[-20:], v[-20:])[0, 1]
        else:
            price_vol_corr = 0.0
        
        vol_mean_60 = v[-60:].mean() if len(v) >= 60 else v.mean()
        vol_std_60 = v[-60:].std() if len(v) >= 60 else 1.0
        volume_zscore = (v[-1] - vol_mean_60) / (vol_std_60 + 1e-8)
        
        # Return feature dict
        return {
            # HMM state probabilities
            'hmm_state_0_prob': state_probs[0],
            'hmm_state_1_prob': state_probs[1],
            'hmm_state_2_prob': state_probs[2],
            
            # GMM volatility probabilities
            'gmm_vol_low_prob': vol_probs[0],
            'gmm_vol_med_prob': vol_probs[1],
            'gmm_vol_high_prob': vol_probs[2],
            
            # Regime stability
            'hmm_state_duration': min(self.hmm_state_duration, 120),
            'vol_regime_duration': min(self.vol_regime_duration, 120),
            
            # Microstructure
            'vol_ratio_20_60': np.clip(vol_ratio, -3, 3),
            'vol_zscore': np.clip(vol_zscore, -3, 3),
            'price_vol_correlation': np.clip(price_vol_corr, -1, 1),
            'volume_zscore': np.clip(volume_zscore, -3, 3),
        }
```

---

## C++ Integration (Feature Vector Extension)

```cpp
// In EWRLS predictor
struct ExtendedFeatures {
    // Existing features
    double price;
    double volume;
    double rsi;
    // ... etc
    
    // NEW: Regime features (12 total)
    double hmm_state_0_prob;
    double hmm_state_1_prob;
    double hmm_state_2_prob;
    double gmm_vol_low_prob;
    double gmm_vol_med_prob;
    double gmm_vol_high_prob;
    int hmm_state_duration;
    int vol_regime_duration;
    double vol_ratio_20_60;
    double vol_zscore;
    double price_vol_correlation;
    double volume_zscore;
};

// EWRLS just trains on extended feature vector
// It learns patterns automatically!
```

---

## Expected MRD Improvement

### Mechanism:
1. **Better predictions** - EWRLS has more context
2. **Better confidence** - Knows when to be certain/uncertain
3. **Better filtering** - Low confidence during regime changes
4. **Better adaptation** - Learns different patterns per regime

### Oct 20 Example:
**Without regime features:**
- EWRLS doesn't know it's a ranging/choppy day
- Uses trending patterns â†’ wrong predictions â†’ losses
- MRD: -0.49%

**With regime features:**
- EWRLS sees: hmm_state_1_prob=0.85, gmm_vol_high_prob=0.7
- Learns: "This combination means mean reversion works"
- Makes mean-reverting predictions â†’ correct â†’ profits
- MRD: +0.15% to +0.30%

**Improvement: +0.64% to +0.79%**

---

## No Algorithm Changes Required!

**Key insight:** We don't change the trading algorithm at all.
- Same EWRLS predictor
- Same confidence weighting
- Same entry/exit logic

**Only change:** EWRLS gets 12 more features to learn from.

EWRLS is smart enough to figure out:
- "When these features look like X, returns behave like Y"
- "When these features look like Z, be less confident"

---

## Implementation Steps

1. **Install packages:** `pip install hmmlearn scikit-learn`
2. **Create:** `scripts/regime_features.py` (feature generator)
3. **Modify C++:** Add 12 features to EWRLS input vector
4. **Call from C++:** Python bridge to get features each bar
5. **Retrain EWRLS:** Now has 22 features instead of 10
6. **Test:** Same Optuna, same Phase 1/2, just better predictions

**No new parameters to optimize!** EWRLS learns automatically.


```

## ðŸ“„ **FILE 9 of 25**: docs/REGIME_FEATURES_MRD_IMPROVEMENT_REQUIREMENTS.md

**File Information**:
- **Path**: `docs/REGIME_FEATURES_MRD_IMPROVEMENT_REQUIREMENTS.md`
- **Size**: 1084 lines
- **Modified**: 2025-10-21 14:38:44
- **Type**: md
- **Permissions**: -rw-r--r--

```text
# Requirements: MRD Improvement with Regime Features

**Document Version:** 1.0
**Date:** October 21, 2025
**Status:** Draft
**Priority:** High

---

## Executive Summary

This document outlines requirements for successfully integrating 12 regime-aware features into the EWRLS predictor to improve Mean Return per Day (MRD), especially on difficult market conditions like Oct 20, 2025.

**Current Status:**
- âœ… 12 regime features implemented and compiled
- âŒ **CRITICAL ISSUE:** EWRLS numerical instability with 54 features
- âŒ Trade count dropped from 10-20/day to 1-4/day (90% reduction)
- âŒ MRD degraded from +0.14% to near 0% on 5-day average

**Root Cause:**
Adding 12 features (42â†’54) increased EWRLS covariance matrix from 42Ã—42 to 54Ã—54, causing severe numerical instability with current regularization settings (1e-6 regularization is insufficient for 2,916 matrix elements).

**Goal:**
Fix numerical stability issues and achieve **MRD improvement from +0.14% to +0.25%** on 5-day average, with special focus on converting Oct 20's -0.49% to +0.10% or better.

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Requirements](#2-requirements)
3. [Technical Approach](#3-technical-approach)
4. [Implementation Plan](#4-implementation-plan)
5. [Testing & Validation](#5-testing--validation)
6. [Success Criteria](#6-success-criteria)
7. [Risk Mitigation](#7-risk-mitigation)
8. [Reference: Source Modules](#8-reference-source-modules)

---

## 1. Problem Statement

### 1.1 Background

The trading system uses EWRLS (Exponentially Weighted Recursive Least Squares) to predict returns at 1, 5, and 10-bar horizons. It previously used 42 features:
- 8 time features (cyclical encoding)
- 34 technical features (momentum, volatility, volume, Bollinger Bands, etc.)

**Enhancement:** Added 12 regime-aware features to help EWRLS understand market conditions:
- 3 HMM-like state probabilities (trending/ranging detection)
- 3 Volatility regime probabilities (low/med/high vol)
- 2 Regime stability features (duration tracking)
- 4 Microstructure features (vol ratios, correlations, z-scores)

### 1.2 Current Issues

#### Issue #1: Severe Numerical Instability
```
Observed Symptoms:
- Condition numbers: 6.6e+07 to 9.8e+07 (target: < 1e6)
- Constant variance explosions (max variance reaching 5,262)
- Continuous regularization warnings (10+ per warmup period)
- Covariance matrix becomes ill-conditioned immediately after initialization

Impact:
- EWRLS predictions become unreliable
- System filters out 90% of trades as "low confidence"
- Trade count: 10-20/day â†’ 1-4/day
- MRD: +0.14% â†’ near 0%
```

#### Issue #2: Insufficient Regularization for High-Dimensional Space
```
Current Settings (42 features):
- regularization = 1e-6
- initial_variance = 100.0
- Covariance matrix P: 42Ã—42 = 1,764 elements

New Settings (54 features):
- regularization = 1e-6 (SAME - TOO WEAK!)
- initial_variance = 100.0 (SAME - TOO HIGH!)
- Covariance matrix P: 54Ã—54 = 2,916 elements (+65% more elements)

Problem:
- Regularization strength didn't scale with dimensionality
- Initial variance creates unstable 100*I initialization
- More features â†’ more opportunities for numerical errors
```

#### Issue #3: Feature Space Validation Gaps
```
Missing Validations:
1. No check if regime features are within expected ranges [0,1] or [-3,3]
2. No validation of feature correlations (are regime features redundant?)
3. No monitoring of feature importance (are all 12 regime features useful?)
4. No feature normalization/standardization strategy
5. No dimensionality reduction considered (PCA, feature selection)
```

### 1.3 Expected vs. Actual Performance

| Metric | 42 Features | Expected (54) | Actual (54) | Gap |
|--------|-------------|---------------|-------------|-----|
| **5-Day Avg MRD** | +0.14% | +0.25% | ~0.00% | **-0.25%** âŒ |
| **Oct 20 MRD** | -0.49% | +0.10% | -0.02% | **-0.12%** âŒ |
| **Trades/Day** | 10-20 | 15-25 | 1-4 | **-11 to -21** âŒ |
| **Win Rate** | 40-50% | 50-60% | 0-40% | **-10% to -50%** âŒ |
| **Condition Number** | <1e6 âœ… | <1e6 | 6e7-9e7 | **60-90x worse** âŒ |

---

## 2. Requirements

### 2.1 Functional Requirements

#### FR-1: Numerical Stability
**Priority:** Critical
**Description:** EWRLS must maintain numerical stability with 54 features throughout all trading sessions.

**Acceptance Criteria:**
- [ ] Condition number < 1e6 for 95% of updates
- [ ] Condition number < 1e7 for 100% of updates
- [ ] Max variance stays below 1,000 (no explosions)
- [ ] Zero variance explosion warnings during normal operation
- [ ] Regularization warnings < 5% of updates

#### FR-2: Trade Activity Restoration
**Priority:** Critical
**Description:** System must generate sufficient trades for statistical significance.

**Acceptance Criteria:**
- [ ] Trades/day: 10-30 (minimum 10, target 15-20)
- [ ] At least 50 trades across 5-day test period
- [ ] No single day with < 3 trades
- [ ] Trade frequency comparable to 42-feature baseline

#### FR-3: MRD Performance Improvement
**Priority:** High
**Description:** Regime features must improve MRD, especially on difficult days.

**Acceptance Criteria:**
- [ ] 5-day average MRD: +0.20% to +0.30% (vs +0.14% baseline)
- [ ] Oct 20 MRD: > 0.00% (vs -0.49% baseline)
- [ ] No day worse than -0.10% MRD
- [ ] Sharpe ratio â‰¥ 0.0 (non-negative)
- [ ] Win rate: 45-55%

#### FR-4: Feature Quality Validation
**Priority:** Medium
**Description:** Regime features must provide useful signal, not noise.

**Acceptance Criteria:**
- [ ] All regime features within expected ranges (no NaN, Inf, out-of-range)
- [ ] Feature correlations < 0.9 (no severe multicollinearity)
- [ ] At least 8/12 regime features show non-zero weights in EWRLS
- [ ] Regime features contribute â‰¥ 15% of prediction variance

### 2.2 Non-Functional Requirements

#### NFR-1: Performance
- Execution time: < 10 minutes for 5-day backtest
- Regime feature overhead: < 1ms per bar
- Memory usage: < 200MB for single symbol

#### NFR-2: Robustness
- Handle missing/invalid regime features gracefully (fallback to neutral)
- Survive 60+ day continuous operation without numerical failure
- Auto-recovery from transient numerical issues

#### NFR-3: Maintainability
- Regime feature extraction isolated in `regime_features.cpp`
- EWRLS regularization configurable via JSON
- Feature validation logging for debugging
- Clear error messages for numerical issues

---

## 3. Technical Approach

### 3.1 Regularization Strategy (Critical)

#### Option A: Adaptive Regularization (RECOMMENDED)
**Concept:** Scale regularization with feature dimensionality and condition number.

```cpp
// Current (BROKEN):
double regularization = 1e-6;  // Fixed, too weak for 54 features

// Proposed (ADAPTIVE):
double base_regularization = 1e-4;  // Stronger baseline
double adaptive_reg = base_regularization * sqrt(n_features / 42.0);
// For 54 features: 1e-4 * sqrt(54/42) = 1.13e-4

// Further adapt based on condition number:
if (condition_number > 1e6) {
    adaptive_reg *= (condition_number / 1e6);  // Scale up
}

// Apply: denominator += adaptive_reg
```

**Benefits:**
- Automatically adjusts for dimensionality
- Responds to numerical issues dynamically
- No manual tuning per feature count

**Implementation:**
- Modify `src/predictor/ewrls_predictor.cpp::update()`
- Add `adaptive_regularization_factor` to config
- Log condition number and regularization strength

#### Option B: Regularization Schedule
**Concept:** Use strong regularization early, then relax as EWRLS learns.

```cpp
double get_regularization(size_t updates, size_t n_features) {
    double base = 1e-3;  // Strong initial
    double decay_factor = 0.995;  // Decay rate

    // Decay over first 1000 updates
    double schedule_reg = base * std::pow(decay_factor, std::min(updates, 1000));

    // Floor at 1e-5 * sqrt(n_features/42)
    double min_reg = 1e-5 * std::sqrt(n_features / 42.0);

    return std::max(schedule_reg, min_reg);
}
```

**Benefits:**
- Prevents early instability (most critical phase)
- Gradually increases learning capacity
- Well-established in adaptive filtering

**Implementation:**
- Add `use_regularization_schedule` config flag
- Track update count in EWRLS
- Log regularization strength over time

#### Option C: Ridge + Elastic Net (Advanced)
**Concept:** Combine L2 (ridge) with L1 (lasso) for feature selection.

```cpp
// Ridge (current): penalize large weights
theta_ -= learning_rate * regularization * theta_;

// Elastic net: Ridge + Lasso
theta_ -= learning_rate * (alpha * theta_ +  // L2
                          (1-alpha) * sign(theta_));  // L1

// Where alpha = 0.9 (mostly ridge, some lasso)
```

**Benefits:**
- L1 component encourages sparsity (zeros out useless features)
- May help if some regime features are redundant
- Automatic feature selection

**Drawbacks:**
- More complex implementation
- Requires tuning alpha parameter
- May be overkill if Option A/B work

### 3.2 Initial Covariance Matrix Strategy

#### Current Issue:
```cpp
P_ = I * 100.0  // 54Ã—54 identity scaled by 100
// Problem: Uniform high variance â†’ instability
```

#### Proposed Solutions:

**Solution 1: Smaller Initial Variance (Quick Fix)**
```cpp
// OLD: initial_variance = 100.0
// NEW: initial_variance = 10.0 (or even 1.0)

P_ = I * 10.0  // Much more conservative initialization
```

**Solution 2: Feature-Group Specific Initialization**
```cpp
// Different variance for different feature groups
P_ = diag([
    1.0,  1.0,  ...,  // Time features (8) - low variance, stable
    10.0, 10.0, ...,  // Technical features (34) - medium variance
    5.0,  5.0,  ...,  // Regime features (12) - medium-low (uncertain quality)
    1.0               // Bias term - low variance
])
```

**Rationale:**
- Time features are well-understood â†’ low uncertainty
- Technical features proven â†’ medium uncertainty
- Regime features NEW â†’ moderate uncertainty
- Bias term should be stable

**Solution 3: Diagonal Loading**
```cpp
// Instead of P = 100*I, use:
P_ = I * initial_variance + diag(feature_specific_variances)

// Where feature_specific_variances come from:
// 1. Historical feature variance from training data
// 2. Expert knowledge (time features less variable than volume)
// 3. Automatic estimation from first N bars
```

### 3.3 Feature Preprocessing & Validation

#### 3.3.1 Input Validation (Regime Features)

Add validation layer in `regime_features.cpp::extract()`:

```cpp
Eigen::VectorXd RegimeFeatures::extract(const std::vector<Bar>& bars) {
    Eigen::VectorXd features(NUM_REGIME_FEATURES);

    // ... existing feature calculation ...

    // VALIDATION LAYER
    for (size_t i = 0; i < NUM_REGIME_FEATURES; ++i) {
        double val = features(i);

        // Check for invalid values
        if (std::isnan(val) || std::isinf(val)) {
            LOG_WARNING << "Regime feature " << i << " is NaN/Inf, replacing with neutral";
            features(i) = get_neutral_value(i);  // 0.33 for probs, 0 for others
            validation_errors_++;
            continue;
        }

        // Check ranges
        if (i < 6) {  // Probabilities [0, 1]
            if (val < 0.0 || val > 1.0) {
                LOG_WARNING << "Regime prob feature " << i << " out of range: " << val;
                features(i) = std::clamp(val, 0.0, 1.0);
                validation_warnings_++;
            }
        } else if (i >= 8) {  // Microstructure features [-3, 3] or [-1, 1]
            double max_val = (i == 10) ? 1.0 : 3.0;  // correlation vs others
            if (std::abs(val) > max_val) {
                LOG_WARNING << "Regime feature " << i << " out of range: " << val;
                features(i) = std::clamp(val, -max_val, max_val);
                validation_warnings_++;
            }
        }
    }

    // Log statistics periodically
    if (bar_count_ % 1000 == 0) {
        LOG_INFO << "Regime features validation: "
                 << validation_errors_ << " errors, "
                 << validation_warnings_ << " warnings in last 1000 bars";
        validation_errors_ = 0;
        validation_warnings_ = 0;
    }

    return features;
}
```

#### 3.3.2 Feature Correlation Analysis

Add diagnostic mode to detect multicollinearity:

```cpp
class FeatureCorrelationAnalyzer {
public:
    void update(const Eigen::VectorXd& features) {
        feature_buffer_.push_back(features);

        if (feature_buffer_.size() >= 1000) {
            analyze();
            feature_buffer_.clear();
        }
    }

    void analyze() {
        // Compute correlation matrix
        Eigen::MatrixXd corr = compute_correlation(feature_buffer_);

        // Find highly correlated pairs
        for (size_t i = 0; i < features.size(); ++i) {
            for (size_t j = i+1; j < features.size(); ++j) {
                if (std::abs(corr(i, j)) > 0.9) {
                    LOG_WARNING << "High correlation detected: features "
                                << i << " and " << j << ": " << corr(i, j);

                    // Suggest feature removal
                    if (i >= 42 && j >= 42) {  // Both regime features
                        LOG_WARNING << "Consider removing one regime feature";
                    }
                }
            }
        }
    }
};
```

**Usage:** Run on training data (Aug-Oct), analyze correlations, decide if any regime features should be removed.

#### 3.3.3 Feature Normalization

Ensure all features on similar scales:

```cpp
class FeatureNormalizer {
private:
    Eigen::VectorXd running_mean_;
    Eigen::VectorXd running_std_;
    size_t count_ = 0;

public:
    Eigen::VectorXd normalize(const Eigen::VectorXd& features) {
        // Update running statistics
        if (count_ == 0) {
            running_mean_ = features;
            running_std_ = Eigen::VectorXd::Ones(features.size());
        } else {
            // Welford's online algorithm
            count_++;
            Eigen::VectorXd delta = features - running_mean_;
            running_mean_ += delta / count_;
            running_std_ = (running_std_ * (count_ - 1) +
                           delta.cwiseProduct(features - running_mean_)) / count_;
        }

        // Z-score normalization
        Eigen::VectorXd normalized = features;
        for (size_t i = 0; i < features.size(); ++i) {
            if (running_std_(i) > 1e-8) {
                normalized(i) = (features(i) - running_mean_(i)) /
                               std::sqrt(running_std_(i));
            }
        }

        return normalized;
    }
};
```

**Caveat:** May not be necessary if features already well-scaled, but worth testing.

### 3.4 Dimensionality Reduction (Optional)

If 54 features still cause issues, consider:

#### Option 1: Feature Selection (Remove Redundant Regime Features)

Test each regime feature individually:
1. Run Optuna with each regime feature removed (12 ablation studies)
2. Identify features that don't improve MRD
3. Remove least useful features (target: reduce 12 â†’ 8 regime features)

#### Option 2: PCA on Regime Features Only

```cpp
// Apply PCA to 12 regime features â†’ 6-8 principal components
// Keep original 42 features intact
// Total: 42 + 6-8 = 48-50 features (less than 54)

Eigen::MatrixXd regime_pca = pca.fit_transform(regime_features, n_components=8);
Eigen::VectorXd final_features = concat(original_42_features, regime_pca);
```

**Benefits:**
- Removes redundancy in regime features
- Reduces dimensionality
- Preserves maximum variance

**Drawbacks:**
- Lose interpretability of regime features
- PCA adds complexity
- May not be necessary if regularization fix works

---

## 4. Implementation Plan

### Phase 1: Quick Fixes (Week 1)
**Goal:** Restore trade activity and numerical stability

**Tasks:**
1. âœ… **Increase base regularization** (1e-6 â†’ 1e-3)
   - File: `include/predictor/ewrls_predictor.h`
   - Change default config
   - Rebuild and test

2. âœ… **Reduce initial variance** (100.0 â†’ 10.0)
   - File: `include/predictor/ewrls_predictor.h`
   - Change default config
   - Rebuild and test

3. **Add feature validation to regime features**
   - File: `src/predictor/regime_features.cpp`
   - Add NaN/Inf checks
   - Add range validation
   - Add warning logging

4. **Test on 5-day dataset**
   - Run: `./build/sentio_lite mock --date 2025-10-14 --warmup-days 1`
   - Check: condition numbers, trade count, MRD
   - Compare to 42-feature baseline

**Success Criteria:**
- [ ] Condition number < 1e7 (down from 6e7-9e7)
- [ ] Trades/day: 8-15 (up from 1-4)
- [ ] MRD: +0.05% to +0.15% (up from 0%)

### Phase 2: Adaptive Regularization (Week 2)
**Goal:** Implement smart regularization that adapts to conditions

**Tasks:**
1. **Implement adaptive regularization**
   - File: `src/predictor/ewrls_predictor.cpp`
   - Add `compute_adaptive_regularization()` method
   - Scale with sqrt(n_features / 42)
   - Scale with condition number if > 1e6

2. **Add regularization to config**
   - File: `include/common/config.h` (if exists)
   - Add `adaptive_regularization_enabled`
   - Add `base_regularization_strength`
   - Add `regularization_scaling_factor`

3. **Add logging/monitoring**
   - Log condition number every 100 updates
   - Log regularization strength every 100 updates
   - Export to CSV for analysis

4. **Re-run Optuna with adaptive regularization**
   ```bash
   python3 optimization/optuna_optimizer.py \
       --phase 1 --trials 200 \
       --dates 2025-10-14 2025-10-15 2025-10-16 2025-10-17 2025-10-20
   ```

**Success Criteria:**
- [ ] Condition number < 1e6 for 95%+ of updates
- [ ] Trades/day: 10-20
- [ ] MRD: +0.15% to +0.25%
- [ ] Zero variance explosion warnings

### Phase 3: Feature Analysis & Optimization (Week 3)
**Goal:** Ensure all 12 regime features are useful

**Tasks:**
1. **Feature correlation analysis**
   - Implement `FeatureCorrelationAnalyzer` class
   - Run on Aug-Oct data (2 months)
   - Identify correlated regime features (|r| > 0.9)

2. **Feature importance analysis**
   - Extract EWRLS theta weights
   - Compute feature importance (|weight| * std(feature))
   - Rank regime features by importance

3. **Ablation study** (if needed)
   - Remove least important regime features one-by-one
   - Re-run Optuna Phase 1
   - Compare MRD: 54 vs 52 vs 50 vs 48 features
   - Find optimal feature count

4. **Feature-specific initialization** (optional)
   - Implement diagonal P initialization
   - Group features: time (low var), technical (med), regime (med)
   - Test if improves stability

**Success Criteria:**
- [ ] All regime features have importance > 1% (or remove)
- [ ] No regime feature pairs with |correlation| > 0.9
- [ ] Final feature count: 48-54 (may remove 0-6 features)

### Phase 4: Extended Validation (Week 4)
**Goal:** Validate on longer time periods

**Tasks:**
1. **Extended backtest: Aug 1 - Oct 20** (60 days)
   ```bash
   # Test all 60 days
   for date in $(seq 2025-08-01 2025-10-20); do
       ./build/sentio_lite mock --date $date --config config/strategy_phase1_best.json
   done
   ```

2. **Analyze performance by regime**
   - Categorize days by regime (trending/ranging/volatile)
   - Compare MRD by regime type
   - Verify regime features help on ranging days

3. **Stability test: 60-day continuous run**
   - Simulate 60 days without restarts
   - Monitor for numerical drift
   - Check condition number over time

4. **Comparison report**
   ```
   Baseline (42 features):
   - 60-day Avg MRD: ???
   - Best day: ???
   - Worst day: ???
   - Sharpe: ???

   Enhanced (48-54 features):
   - 60-day Avg MRD: ???
   - Best day: ???
   - Worst day: ???
   - Sharpe: ???
   ```

**Success Criteria:**
- [ ] 60-day avg MRD: +0.20% to +0.30%
- [ ] Improvement on ranging days: +0.10% to +0.15%
- [ ] No numerical failures over 60 days
- [ ] Sharpe ratio > 0.5

---

## 5. Testing & Validation

### 5.1 Unit Tests

**Test 1: Regime Feature Ranges**
```cpp
TEST(RegimeFeatures, OutputRanges) {
    RegimeFeatures gen;

    // Generate features from sample bars
    auto features = gen.extract(sample_bars);

    // Test probability features [0, 1]
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_GE(features(i), 0.0);
        EXPECT_LE(features(i), 1.0);
    }

    // Test duration features [0, 120]
    EXPECT_GE(features(6), 0);
    EXPECT_LE(features(6), 120);
    EXPECT_GE(features(7), 0);
    EXPECT_LE(features(7), 120);

    // Test microstructure features [-3, 3] or [-1, 1]
    for (size_t i = 8; i < 12; ++i) {
        double max_val = (i == 10) ? 1.0 : 3.0;
        EXPECT_GE(features(i), -max_val);
        EXPECT_LE(features(i), max_val);
    }
}
```

**Test 2: EWRLS Numerical Stability**
```cpp
TEST(EWRLS, NumericalStability54Features) {
    EWRLSPredictor::Config config;
    config.regularization = 1e-3;  // New stronger regularization
    config.initial_variance = 10.0;  // New smaller initial variance

    EWRLSPredictor ewrls(54, config);

    // Update with random features 1000 times
    for (size_t i = 0; i < 1000; ++i) {
        Eigen::VectorXd features = Eigen::VectorXd::Random(54);
        double ret = (rand() % 100 - 50) / 10000.0;  // Random return [-0.5%, +0.5%]

        ewrls.update(features, ret);

        // Check condition number
        double cond = ewrls.get_condition_number();
        EXPECT_LT(cond, 1e7) << "Condition number too high at update " << i;
    }

    // Final stability check
    EXPECT_TRUE(ewrls.is_numerically_stable());
}
```

**Test 3: Feature Validation**
```cpp
TEST(RegimeFeatures, NaNHandling) {
    RegimeFeatures gen;

    // Create bars with edge cases
    std::vector<Bar> edge_case_bars = create_edge_case_bars();

    auto features = gen.extract(edge_case_bars);

    // No NaN/Inf allowed
    for (size_t i = 0; i < 12; ++i) {
        EXPECT_FALSE(std::isnan(features(i))) << "Feature " << i << " is NaN";
        EXPECT_FALSE(std::isinf(features(i))) << "Feature " << i << " is Inf";
    }
}
```

### 5.2 Integration Tests

**Test 4: 5-Day Backtest Comparison**
```bash
#!/bin/bash
# Compare 42 vs 54 features on same 5 days

echo "Testing 42-feature baseline..."
./build/sentio_lite mock --date 2025-10-14 --config config/strategy_phase2_best.json > results_42feat.txt
./build/sentio_lite mock --date 2025-10-15 --config config/strategy_phase2_best.json >> results_42feat.txt
# ... (repeat for all 5 days)

echo "Testing 54-feature enhanced..."
./build/sentio_lite mock --date 2025-10-14 --config config/strategy_phase1_best.json > results_54feat.txt
./build/sentio_lite mock --date 2025-10-15 --config config/strategy_phase1_best.json >> results_54feat.txt
# ... (repeat for all 5 days)

# Compare
python3 tools/compare_results.py results_42feat.txt results_54feat.txt
```

**Test 5: Condition Number Monitoring**
```bash
# Run with verbose logging
./build/sentio_lite mock --date 2025-10-14 --verbose 2>&1 | \
    grep "Condition number" | \
    awk '{print $NF}' > condition_numbers.txt

# Analyze
python3 -c "
import numpy as np
conds = np.loadtxt('condition_numbers.txt')
print(f'Mean: {conds.mean():.2e}')
print(f'Max: {conds.max():.2e}')
print(f'% < 1e6: {100 * (conds < 1e6).mean():.1f}%')
print(f'% < 1e7: {100 * (conds < 1e7).mean():.1f}%')
"
```

### 5.3 Performance Tests

**Test 6: Execution Time**
```bash
# Measure overhead of regime features
time ./build/sentio_lite mock --date 2025-10-14 --config config_42feat.json
# vs
time ./build/sentio_lite mock --date 2025-10-14 --config config_54feat.json

# Acceptable: < 10% slowdown
```

**Test 7: Memory Usage**
```bash
# Monitor memory during execution
/usr/bin/time -v ./build/sentio_lite mock --date 2025-10-14 2>&1 | grep "Maximum resident"

# Acceptable: < 200 MB
```

---

## 6. Success Criteria

### 6.1 Primary Metrics

| Metric | Baseline (42 feat) | Target (54 feat) | Must Exceed |
|--------|-------------------|------------------|-------------|
| **5-Day Avg MRD** | +0.140% | +0.200% to +0.300% | +0.140% |
| **Oct 20 MRD** | -0.490% | +0.100% to +0.200% | -0.100% |
| **Trades/Day** | 10-20 | 10-30 | 10 |
| **Win Rate** | 40-50% | 45-55% | 40% |
| **Sharpe Ratio** | 0.0 | > 0.5 | 0.0 |
| **Max Drawdown** | 0.10% | < 0.15% | 0.20% |

### 6.2 Stability Metrics

| Metric | Target | Threshold |
|--------|--------|-----------|
| **Condition Number (Median)** | < 1e6 | < 1e7 |
| **Condition Number (95th pct)** | < 5e6 | < 5e7 |
| **Variance Explosions** | 0 per day | < 5 per day |
| **Regularization Warnings** | < 5% of updates | < 20% of updates |
| **Failed Trades (numerical)** | 0 | < 1% |

### 6.3 Feature Quality Metrics

| Metric | Target |
|--------|--------|
| **Regime Features with Non-Zero Weight** | â‰¥ 10/12 |
| **Regime Features with Importance > 1%** | â‰¥ 8/12 |
| **Max Feature Correlation** | < 0.9 |
| **Feature Validation Errors** | 0 |
| **Feature Validation Warnings** | < 0.1% of bars |

---

## 7. Risk Mitigation

### 7.1 Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Regularization too strong â†’ EWRLS can't learn** | Medium | High | Adaptive regularization; start strong, decay to optimal level |
| **54 features inherently too many** | Low | High | Dimensionality reduction (PCA, feature selection) as fallback |
| **Regime features correlated/redundant** | Medium | Medium | Correlation analysis; remove redundant features |
| **Numerical issues on specific symbols** | Low | Medium | Per-symbol EWRLS config; symbol-specific regularization |

### 7.2 Performance Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Regime features work in backtest, fail live** | Medium | High | Extended 60-day validation; stress testing on edge cases |
| **Overfitting to Oct 14-20 period** | High | Medium | Test on Aug-Sep; use walk-forward validation |
| **MRD improvement not statistically significant** | Medium | Medium | Extend to 60-day test; compute confidence intervals |

### 7.3 Implementation Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Phase 1 quick fixes insufficient** | Medium | Medium | Have Phase 2 (adaptive reg) and Phase 3 (ablation) ready |
| **Optuna re-optimization takes too long** | Low | Low | Reduce trials to 100; use parallel jobs (--n-jobs 4) |
| **Config changes break backward compatibility** | Low | Medium | Version configs; maintain 42-feature fallback |

---

## 8. Reference: Source Modules

### 8.1 Core Implementation Files

#### Regime Features
| File | Lines | Purpose |
|------|-------|---------|
| `include/predictor/regime_features.h` | 112 | Regime feature interface & constants |
| `src/predictor/regime_features.cpp` | 450 | K-means regime detection, feature calculation |

**Key Functions:**
- `RegimeFeatures::extract()` - Main feature generation (line 22)
- `detect_market_state()` - HMM-like 3-state detection (line 120)
- `detect_volatility_regime()` - GMM-like vol clustering (line 155)
- `calculate_microstructure_features()` - Vol ratios, correlations (line 190)

#### EWRLS Predictor
| File | Lines | Purpose |
|------|-------|---------|
| `include/predictor/ewrls_predictor.h` | 127 | EWRLS interface & config struct |
| `src/predictor/ewrls_predictor.cpp` | 280 | Recursive least squares implementation |

**Key Functions:**
- `EWRLSPredictor::update()` - EWRLS weight update (line 40)
- `ensure_numerical_stability()` - Condition number checking (line 150)
- `apply_regularization()` - Ridge regularization (line 180)
- `get_condition_number()` - Eigenvalue-based stability metric (line 220)

**Critical Config (line 29-36):**
```cpp
struct Config {
    double lambda = 0.99;
    double regularization = 1e-6;        // â† PROBLEM: Too weak for 54 features
    double initial_variance = 100.0;     // â† PROBLEM: Too high for stability
    double max_variance = 1000.0;
    double max_gradient_norm = 1.0;
    bool use_adaptive_regularization = true;
    size_t stability_check_interval = 100;
};
```

#### Feature Extraction
| File | Lines | Purpose |
|------|-------|---------|
| `include/predictor/feature_extractor.h` | 214 | Feature extraction interface (42â†’54 features) |
| `src/predictor/feature_extractor.cpp` | 450 | Technical indicator calculation + regime integration |

**Key Changes:**
- Line 33: `NUM_FEATURES = 54` (was 42)
- Line 42-53: Added 12 regime feature indices
- Line 114-119: Regime feature extraction call
- Line 192: Added `RegimeFeatures regime_features_` member

#### Multi-Horizon Predictor
| File | Lines | Purpose |
|------|-------|---------|
| `include/predictor/multi_horizon_predictor.h` | 230 | 1/5/10-bar prediction orchestration |
| `src/predictor/multi_horizon_predictor.cpp` | 320 | Horizon-specific EWRLS management |

**Key Functions:**
- `predict()` - Generate predictions at all horizons (line 60)
- `update()` - Update predictors with realized returns (line 120)
- `should_enter()` - Entry signal quality check (line 80)

### 8.2 Configuration & Optimization

#### Configuration Loading
| File | Lines | Purpose |
|------|-------|---------|
| `include/common/config.h` | 180 | Config structure definitions |
| `src/utils/config_loader.cpp` | 450 | JSON config parsing |

**Key Sections:**
- `TradingConfig::horizon_config` - Lambda values for 1/5/10-bar (line 45)
- `TradingConfig::ewrls_config` - EWRLS stability params (line 120)

#### Optuna Optimization
| File | Lines | Purpose |
|------|-------|---------|
| `optimization/optuna_optimizer.py` | 560 | Bayesian hyperparameter optimization |
| `optimization/parameter_spaces.py` | 220 | Search space definitions for Phase 1-4 |
| `optimization/objective_function.py` | 390 | Objective function (MRD calculation) |

**Phase 1 Parameters (12 total):**
- Lines 45-60: Core parameters (buy/sell thresholds, lambdas)
- Lines 110-120: Position sizing & risk parameters
- Lines 145-155: Bollinger Band parameters

### 8.3 Trading System

#### Main Trading Loop
| File | Lines | Purpose |
|------|-------|---------|
| `src/main.cpp` | 800 | Main entry point, mode selection |
| `include/trading/multi_symbol_trader.h` | 450 | Multi-symbol rotation trading system |
| `src/trading/multi_symbol_trader.cpp` | 1800 | Trading logic, position management |

**Key Functions:**
- `process_bar()` - Per-bar trading decisions (line 650)
- `evaluate_entry_signals()` - Entry signal generation (line 800)
- `manage_exits()` - Exit logic (stop loss, profit target, signals) (line 1100)

#### Build System
| File | Lines | Purpose |
|------|-------|---------|
| `CMakeLists.txt` | 160 | Build configuration |

**Key Changes:**
- Line 53: Added `src/predictor/regime_features.cpp` to build

### 8.4 Data & Testing

#### Data Files
| File | Purpose |
|------|---------|
| `data/*_RTH_NH.bin` | Binary market data (Aug 1 - Oct 21, 2025) |
| `config/symbols.conf` | 12-symbol universe (TQQQ, SQQQ, etc.) |

#### Test Configurations
| File | Purpose |
|------|---------|
| `config/strategy_phase2_best.json` | 42-feature baseline config |
| `config/strategy_phase1_best.json` | 54-feature optimized config (Trial #140) |
| `results/best_params_phase1.json` | Optuna Phase 1 best trial metadata |

### 8.5 Documentation

| File | Purpose |
|------|---------|
| `docs/REGIME_FEATURES_FOR_EWRLS.md` | Original design spec (12 features) |
| `docs/REGIME_ADAPTIVE_FEATURE_MODEL.md` | Alternative regime-adaptive trading design |
| `docs/REGIME_DETECTION_FEATURE_DESIGN.md` | Fast regime detection methods (HMM, GMM, Ruptures) |
| `REGIME_FEATURES_IMPLEMENTATION.md` | Implementation completion summary |
| `REGIME_FEATURES_OPTUNA_RUN.md` | Optuna Phase 1 run documentation |
| `docs/REGIME_FEATURES_MRD_IMPROVEMENT_REQUIREMENTS.md` | **This document** |

---

## 9. Appendix

### 9.1 Quick Reference: Condition Number

**What it means:**
```
Condition number = Î»_max / Î»_min (ratio of largest to smallest eigenvalue)

< 1e3:   Excellent (well-conditioned)
1e3-1e6: Good (acceptable)
1e6-1e9: Poor (ill-conditioned, may have issues)
> 1e9:   Severe (nearly singular, numerically unstable)
```

**Our situation:**
```
42 features: ~1e5 to 1e6   âœ… Good
54 features: 6e7 to 9e7    âŒ Severe (60-90x worse!)
```

### 9.2 Quick Reference: Regularization Strength

**Rule of thumb:**
```
regularization â‰ˆ 1e-6 to 1e-4 * sqrt(n_features)

For 42 features: 1e-6 * sqrt(42) = 6.5e-6  âœ… Current default works
For 54 features: 1e-6 * sqrt(54) = 7.3e-6  âŒ Still too weak!

Better for 54: 1e-4 * sqrt(54) = 7.3e-4   âœ… Proposed
```

### 9.3 Equations Reference

**EWRLS Update (Recursive Least Squares):**
```
1. Prediction error:
   e(t) = y(t) - Î¸(t-1)áµ€ x(t)

2. Kalman gain:
   k(t) = P(t-1) x(t) / [Î» + xáµ€(t) P(t-1) x(t) + reg]
                                                    â†‘
                                               Regularization term

3. Weight update:
   Î¸(t) = Î¸(t-1) + k(t) e(t)

4. Covariance update:
   P(t) = [P(t-1) - k(t) xáµ€(t) P(t-1)] / Î»

Where:
- Î» = forgetting factor (0.95-0.999)
- reg = regularization strength
- P = covariance matrix (nÃ—n)
- Î¸ = weight vector (nÃ—1)
- x(t) = feature vector (nÃ—1)
- y(t) = target return (scalar)
```

**Condition Number:**
```
Îº(P) = ||P|| Ã— ||Pâ»Â¹|| = Î»_max / Î»_min

Where:
- Î»_max = largest eigenvalue of P
- Î»_min = smallest eigenvalue of P

Îº â†’ âˆž means P is nearly singular (uninvertible)
```

### 9.4 Testing Commands Cheat Sheet

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8

# Single day test
./build/sentio_lite mock --date 2025-10-20 \
    --config config/strategy_phase1_best.json \
    --warmup-days 1

# 5-day test with Optuna
python3 optimization/optuna_optimizer.py \
    --phase 1 --trials 200 \
    --dates 2025-10-14 2025-10-15 2025-10-16 2025-10-17 2025-10-20 \
    --study-name phase1_54features

# Check condition numbers
./build/sentio_lite mock --date 2025-10-14 --verbose 2>&1 | \
    grep "Condition number" | \
    awk '{print $NF}' | \
    sort -n | tail -20  # Show worst 20

# Memory/time profiling
/usr/bin/time -v ./build/sentio_lite mock --date 2025-10-14
```

---

## 10. Approval & Sign-off

| Role | Name | Signature | Date |
|------|------|-----------|------|
| **Technical Lead** | | | |
| **QA Lead** | | | |
| **Product Owner** | | | |

---

**Document History:**

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-10-21 | AI Assistant | Initial requirements document |

---

**Next Steps:**
1. Review and approve this requirements document
2. Implement Phase 1 quick fixes (regularization & initial variance)
3. Test and validate on 5-day dataset
4. Proceed to Phase 2 if Phase 1 insufficient
5. Document results and lessons learned

```

## ðŸ“„ **FILE 10 of 25**: include/predictor/ewrls_predictor.h

**File Information**:
- **Path**: `include/predictor/ewrls_predictor.h`
- **Size**: 126 lines
- **Modified**: 2025-10-21 14:33:51
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include <Eigen/Dense>
#include <cstddef>
#include <cmath>

namespace sentio {

/**
 * Exponentially Weighted Recursive Least Squares (EWRLS) Predictor
 *
 * Robust online learning algorithm with numerical stability enhancements:
 * - Adaptive regularization to prevent matrix ill-conditioning
 * - Condition number monitoring and automatic correction
 * - Gradient clipping to prevent weight explosions
 * - Variance explosion prevention
 * - Periodic stability checks
 *
 * Key properties:
 * - O(n^2) update complexity where n = number of features
 * - Adapts to changing market conditions via lambda (forgetting factor)
 * - No batch training required - learns incrementally
 * - Production-ready with long-running stability
 */
class EWRLSPredictor {
public:
    /**
     * Configuration for EWRLS with stability controls
     */
    struct Config {
        double lambda = 0.99;               // Forgetting factor (0.95-0.999)
        double regularization = 1e-3;       // Ridge regularization strength (increased for 54 features)
        double initial_variance = 10.0;     // Initial P diagonal value (reduced for stability with 54 features)
        double max_variance = 1000.0;       // Maximum P diagonal value (prevent explosion)
        double max_gradient_norm = 1.0;     // Gradient clipping threshold
        bool use_adaptive_regularization = true;  // Enable adaptive regularization
        size_t stability_check_interval = 100;    // Check every N updates
    };

    /**
     * Constructor with lambda only (uses default config)
     */
    EWRLSPredictor(size_t n_features, double lambda = 0.99);

    /**
     * Constructor with full configuration
     */
    EWRLSPredictor(size_t n_features, const Config& config);

    /**
     * Make prediction for given feature vector
     * @param features Input feature vector (must match n_features)
     * @return Predicted return (can be positive or negative)
     */
    double predict(const Eigen::VectorXd& features) const;

    /**
     * Update model with observed outcome
     * @param features Input feature vector
     * @param actual_return Realized return (will be clamped to [-1, 1])
     *
     * Includes automatic numerical stability checks and corrections
     */
    void update(const Eigen::VectorXd& features, double actual_return);

    /**
     * Reset predictor to initial state
     */
    void reset();

    /**
     * Get current model weights (for inspection/debugging)
     */
    const Eigen::VectorXd& weights() const { return theta_; }

    /**
     * Get number of updates performed
     */
    size_t update_count() const { return updates_; }

    /**
     * Get condition number of covariance matrix
     * Values > 1e6 indicate potential numerical issues
     */
    double get_condition_number() const;

    /**
     * Check if predictor is numerically stable
     * Returns false if intervention is needed
     */
    bool is_numerically_stable() const;

    /**
     * Get configuration
     */
    const Config& config() const { return config_; }

private:
    Eigen::VectorXd theta_;         // Model weights (n_features)
    Eigen::MatrixXd P_;             // Covariance matrix (n_features x n_features)
    Config config_;                 // Configuration parameters
    size_t n_features_;             // Number of features
    size_t updates_;                // Number of updates performed

    // Stability tracking
    double min_eigenvalue_;         // Minimum eigenvalue of P
    double max_eigenvalue_;         // Maximum eigenvalue of P

    /**
     * Ensure numerical stability of covariance matrix
     * Checks condition number and applies regularization if needed
     */
    void ensure_numerical_stability();

    /**
     * Apply regularization to covariance matrix
     * Adds identity matrix scaled by regularization parameter
     */
    void apply_regularization();

    /**
     * Compute eigenvalues for stability monitoring
     */
    void update_eigenvalue_bounds();
};

} // namespace sentio

```

## ðŸ“„ **FILE 11 of 25**: include/predictor/feature_extractor.h

**File Information**:
- **Path**: `include/predictor/feature_extractor.h`
- **Size**: 231 lines
- **Modified**: 2025-10-21 14:00:52
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "core/bar.h"
#include "utils/circular_buffer.h"
#include "predictor/regime_features.h"
#include <Eigen/Dense>
#include <optional>
#include <array>

namespace trading {

/**
 * Enhanced Feature Extractor - 42 Technical + Time + Bollinger Band Features
 *
 * Extracts comprehensive set of proven technical indicators for online learning:
 * - 8 Time features (cyclical encoding: hour, minute, day-of-week, day-of-month)
 * - Multi-timeframe momentum (1, 3, 5, 10 bars)
 * - Volatility measures (realized vol, ATR)
 * - Volume analysis (surge, relative volume)
 * - Price position indicators (range position, channel position)
 * - Trend strength (RSI-like, directional momentum)
 * - Interaction terms (momentum * volatility, etc.)
 * - Mean reversion indicators (deviation from MA at 5, 10, 20 periods)
 * - Bollinger Bands (6 features: mean_dev, sd_pct, upper_dev, lower_dev, %B, bandwidth)
 *
 * Optimized for:
 * - O(1) incremental updates via CircularBuffer
 * - Minimal memory footprint (50-bar lookback)
 * - Production-ready stability (handles edge cases)
 */
class FeatureExtractor {
public:
    // Public constants for feature dimensions
    static constexpr size_t LOOKBACK = 50;      // Lookback window size
    static constexpr size_t NUM_FEATURES = 54;  // 8 time + 28 technical + 6 Bollinger + 12 regime

    /**
     * Feature Index Enum - Robust indexing for feature vector
     *
     * Use this enum instead of magic numbers to access features.
     * Example: features(FeatureExtractor::RSI_14) instead of features(20)
     *
     * This makes code self-documenting and prevents errors when features change.
     */
    enum FeatureIndex {
        // Time features (0-7)
        HourSin = 0,
        HourCos = 1,
        MinuteSin = 2,
        MinuteCos = 3,
        DayOfWeekSin = 4,
        DayOfWeekCos = 5,
        DayOfMonthSin = 6,
        DayOfMonthCos = 7,

        // Momentum features (8-11)
        Momentum_1 = 8,
        Momentum_3 = 9,
        Momentum_5 = 10,
        Momentum_10 = 11,

        // Volatility features (12-14)
        Volatility_10 = 12,
        Volatility_20 = 13,
        ATR_14 = 14,

        // Volume features (15-16)
        VolumeSurge = 15,
        RelativeVolume_20 = 16,

        // Price position features (17-19)
        PricePosition_50 = 17,
        ChannelPosition_20 = 18,
        ChannelPosition_10 = 19,

        // Trend strength features (20-22)
        RSI_14 = 20,
        DirectionalMomentum_10 = 21,
        DirectionalMomentum_20 = 22,

        // Interaction features (23-27)
        Mom1_x_Vol10 = 23,
        Mom5_x_Vol10 = 24,
        Mom10_x_VolSurge = 25,
        RSI_x_Vol = 26,
        PricePos_x_Direction = 27,

        // Acceleration features (28-30)
        MomentumAccelShort = 28,
        MomentumAccelLong = 29,
        VolatilityChange = 30,

        // Derived features (31)
        LogMomentum = 31,

        // Mean reversion features (32-34)
        MA_Dev_5 = 32,
        MA_Dev_10 = 33,
        MA_Dev_20 = 34,

        // Bollinger Bands features (35-40)
        BB20_MeanDev = 35,          // (close - bb_mean) / close
        BB20_SdPct = 36,            // bb_sd / close
        BB20_UpperDev = 37,         // (close - bb_upper) / close
        BB20_LowerDev = 38,         // (close - bb_lower) / close
        BB20_PercentB = 39,         // Position within bands (0-1)
        BB20_Bandwidth = 40,        // Band width ratio

        // Bias term (41)
        Bias = 41,

        // Regime features (42-53)
        Regime_HMM_State_0 = 42,
        Regime_HMM_State_1 = 43,
        Regime_HMM_State_2 = 44,
        Regime_Vol_Low = 45,
        Regime_Vol_Med = 46,
        Regime_Vol_High = 47,
        Regime_HMM_Duration = 48,
        Regime_Vol_Duration = 49,
        Regime_Vol_Ratio = 50,
        Regime_Vol_ZScore = 51,
        Regime_Price_Vol_Corr = 52,
        Regime_Volume_ZScore = 53
    };

    FeatureExtractor();

    /**
     * Extract features from new bar
     * @param bar New OHLCV bar
     * @return Feature vector (std::nullopt during warmup period)
     *
     * Returns std::nullopt if less than LOOKBACK bars have been seen.
     * Once warmed up, always returns valid 33-dimensional feature vector.
     */
    std::optional<Eigen::VectorXd> extract(const Bar& bar);

    /**
     * Access price history (for debugging/inspection)
     */
    const CircularBuffer<Bar>& history() const { return history_; }

    /**
     * Check if warmup period is complete
     */
    bool is_ready() const { return bar_count_ >= LOOKBACK; }

    /**
     * Get number of bars processed
     */
    size_t bar_count() const { return bar_count_; }

    /**
     * Reset to initial state
     */
    void reset();

    /**
     * Get feature names (for debugging/logging)
     */
    static std::vector<std::string> get_feature_names();

    /**
     * Calculate z-score for mean reversion (public for z-score strategy)
     * @param prices Price history
     * @param period Period for MA and stddev calculation
     * @return Z-score: (price - MA) / (stddev * price)
     */
    double calculate_z_score(const std::vector<Price>& prices, int period) const;

    /**
     * Calculate ADX (Average Directional Index) for regime detection
     * @param bars Bar history
     * @param period ADX calculation period (typically 14)
     * @return ADX value (0-100, <25 = ranging, >40 = strong trend)
     *
     * ADX measures trend strength (not direction):
     * - ADX < 20: Weak/no trend (choppy, mean reversion works well)
     * - ADX 20-25: Emerging trend
     * - ADX 25-40: Strong trend
     * - ADX > 40: Very strong trend (mean reversion fails)
     */
    double calculate_adx(const std::vector<Bar>& bars, int period = 14) const;

private:
    // Member variables
    CircularBuffer<Bar> history_;
    double prev_close_;
    size_t bar_count_;

    // Regime feature extractor
    RegimeFeatures regime_features_;

    // Time feature calculations (cyclical encoding)
    void calculate_time_features(Timestamp timestamp, Eigen::VectorXd& features, int& idx) const;

    // Core feature calculations
    double calculate_momentum(const std::vector<Price>& prices, int period) const;
    double calculate_volatility(const std::vector<Price>& prices, int period) const;
    double calculate_atr(const std::vector<Bar>& bars, int period) const;
    double calculate_volume_surge(const std::vector<Volume>& volumes) const;
    double calculate_relative_volume(const std::vector<Volume>& volumes, int period) const;
    double calculate_price_position(const std::vector<Bar>& bars, Price current_price) const;
    double calculate_channel_position(const std::vector<Bar>& bars, int period) const;
    double calculate_rsi_like(const std::vector<Price>& prices, int period) const;
    double calculate_directional_momentum(const std::vector<Price>& prices, int period) const;

    // Mean reversion features
    double calculate_ma_deviation(const std::vector<Price>& prices, int period) const;

    // Bollinger Bands features
    struct BollingerBands {
        double mean = 0.0;
        double sd = 0.0;
        double upper = 0.0;
        double lower = 0.0;
        double percent_b = 0.5;
        double bandwidth = 0.0;
    };
    BollingerBands calculate_bollinger_bands(const std::vector<Price>& prices, int period = 20, double k = 2.0) const;

    // Williams %R (used in anticipatory feature)
    double calculate_williams_r(const std::vector<Bar>& bars, int period = 14) const;

    // Utility helpers
    std::vector<Price> get_closes() const;
    std::vector<Volume> get_volumes() const;
    std::vector<Bar> get_bars() const;
};

} // namespace trading

```

## ðŸ“„ **FILE 12 of 25**: include/predictor/multi_horizon_predictor.h

**File Information**:
- **Path**: `include/predictor/multi_horizon_predictor.h`
- **Size**: 224 lines
- **Modified**: 2025-10-17 15:53:01
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "predictor/online_predictor.h"
#include "predictor/feature_extractor.h"
#include <memory>
#include <array>
#include <string>
#include <Eigen/Dense>

namespace trading {

/**
 * Multi-Horizon Predictor - Predicts returns at 1, 5, and 10 bar horizons
 *
 * Maintains separate EWRLS predictors for different time horizons to capture
 * both short-term momentum and longer-term trends. This allows the trading
 * system to:
 * - Enter positions when multiple horizons align
 * - Determine optimal holding periods based on predicted return paths
 * - Exit when signal quality degrades at relevant horizon
 *
 * Key features:
 * - Three independent predictors (1, 5, 10 bars ahead)
 * - Different lambda values for each horizon (faster for short-term)
 * - Uncertainty quantification for each prediction
 * - Signal quality metrics (confidence, z-score, signal-to-noise)
 */
class MultiHorizonPredictor {
public:
    /**
     * Prediction with quality metrics
     */
    struct PredictionQuality {
        double prediction = 0.0;        // Predicted return
        double uncertainty = 0.0;       // Prediction uncertainty (std dev)
        double confidence = 0.0;        // Confidence score [0, 1]
        double z_score = 0.0;          // Prediction / uncertainty
        double signal_to_noise = 0.0;  // |prediction| / uncertainty

        bool is_high_quality(double min_confidence = 0.6,
                           double min_z_score = 1.2,
                           double min_sn_ratio = 2.0) const {
            return confidence >= min_confidence &&
                   std::abs(z_score) >= min_z_score &&
                   signal_to_noise >= min_sn_ratio;
        }
    };

    /**
     * Multi-horizon prediction result
     */
    struct MultiHorizonPrediction {
        PredictionQuality pred_1bar;    // Next bar prediction
        PredictionQuality pred_5bar;    // 5-bar cumulative prediction
        PredictionQuality pred_10bar;   // 10-bar cumulative prediction

        int optimal_horizon = 1;        // Best risk/reward horizon (1, 5, or 10)
        double expected_return = 0.0;   // Expected return at optimal horizon
        double expected_volatility = 0.0;  // Expected volatility over hold period

        /**
         * Check if multiple horizons agree on direction
         * Returns true if at least 2 out of 3 horizons agree
         */
        bool horizons_agree() const {
            int positive_count = 0;
            int negative_count = 0;

            if (pred_1bar.prediction > 0) positive_count++; else negative_count++;
            if (pred_5bar.prediction > 0) positive_count++; else negative_count++;
            if (pred_10bar.prediction > 0) positive_count++; else negative_count++;

            // At least 2 out of 3 must agree
            return positive_count >= 2 || negative_count >= 2;
        }

        /**
         * Check if signal is strong enough to enter
         * RELAXED CRITERIA: Only requires strong 5-bar signal and directional agreement
         */
        bool should_enter(double min_prediction = 0.002,
                         double min_confidence = 0.6) const {
            // Require 5-bar prediction to exceed minimum threshold
            if (std::abs(pred_5bar.prediction) < min_prediction) {
                return false;
            }

            // Require good confidence at 5-bar horizon
            if (pred_5bar.confidence < min_confidence) {
                return false;
            }

            // Only require directional agreement between 1-bar and 5-bar
            // (removed requirement for all 3 horizons to agree)
            if ((pred_1bar.prediction > 0) != (pred_5bar.prediction > 0)) {
                return false;
            }

            // Removed 3x ratio requirement - was too restrictive

            return true;
        }

        /**
         * Suggest optimal holding period based on predictions
         */
        int suggested_hold_period() const {
            // Use optimal_horizon as base, but add buffer
            if (optimal_horizon == 10) {
                return 10;
            } else if (optimal_horizon == 5) {
                return 5;
            } else {
                return 3;  // Minimum meaningful hold
            }
        }
    };

    /**
     * Configuration for multi-horizon predictor
     */
    struct Config {
        // EWRLS parameters per horizon
        double lambda_1bar;      // Fast adaptation for 1-bar
        double lambda_5bar;     // Medium adaptation for 5-bar
        double lambda_10bar;    // Slow adaptation for 10-bar

        // Uncertainty estimation (for confidence calculations)
        double initial_uncertainty;  // 1% initial uncertainty
        double uncertainty_decay;    // Decay factor for uncertainty

        // Quality thresholds
        double min_confidence;
        double min_z_score;
        double min_signal_to_noise;

        Config()
            : lambda_1bar(0.99)
            , lambda_5bar(0.995)
            , lambda_10bar(0.998)
            , initial_uncertainty(0.01)
            , uncertainty_decay(0.95)
            , min_confidence(0.6)
            , min_z_score(1.2)
            , min_signal_to_noise(2.0) {}
    };

    /**
     * Constructor
     * @param symbol Symbol identifier (for debugging)
     * @param config Configuration parameters
     */
    explicit MultiHorizonPredictor(const std::string& symbol, const Config& config = Config());

    /**
     * Make predictions at all horizons
     * @param features Input feature vector (33 dimensions)
     * @return Multi-horizon prediction with quality metrics
     */
    MultiHorizonPrediction predict(const Eigen::VectorXd& features);

    /**
     * Update predictors with realized returns
     * @param features Feature vector used for prediction
     * @param return_1bar Actual 1-bar return
     * @param return_5bar Actual 5-bar cumulative return (if available)
     * @param return_10bar Actual 10-bar cumulative return (if available)
     *
     * Note: For 5-bar and 10-bar updates, pass NaN if not yet available
     */
    void update(const Eigen::VectorXd& features,
                double return_1bar,
                double return_5bar = std::numeric_limits<double>::quiet_NaN(),
                double return_10bar = std::numeric_limits<double>::quiet_NaN());

    /**
     * Reset all predictors
     */
    void reset();

    /**
     * Get configuration
     */
    const Config& config() const { return config_; }

    /**
     * Get symbol identifier
     */
    const std::string& symbol() const { return symbol_; }

    /**
     * Get update counts for each horizon
     */
    std::array<size_t, 3> update_counts() const;

private:
    std::string symbol_;
    Config config_;

    // Separate predictors for each horizon
    std::unique_ptr<OnlinePredictor> predictor_1bar_;
    std::unique_ptr<OnlinePredictor> predictor_5bar_;
    std::unique_ptr<OnlinePredictor> predictor_10bar_;

    // Uncertainty tracking (simple exponentially weighted variance)
    std::array<double, 3> prediction_errors_;  // Running prediction errors
    std::array<double, 3> uncertainties_;      // Estimated uncertainties

    /**
     * Calculate prediction quality metrics
     */
    PredictionQuality calculate_quality(double prediction, double uncertainty) const;

    /**
     * Update uncertainty estimate based on prediction error
     */
    void update_uncertainty(int horizon_idx, double error);

    /**
     * Determine optimal horizon based on Sharpe-like metric
     */
    int determine_optimal_horizon(const MultiHorizonPrediction& pred) const;
};

} // namespace trading

```

## ðŸ“„ **FILE 13 of 25**: include/predictor/regime_features.h

**File Information**:
- **Path**: `include/predictor/regime_features.h`
- **Size**: 131 lines
- **Modified**: 2025-10-21 13:59:23
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "core/bar.h"
#include "utils/circular_buffer.h"
#include <Eigen/Dense>
#include <array>
#include <vector>

namespace trading {

/**
 * Fast Regime Feature Generator - 12 additional features for EWRLS
 *
 * Adds regime-aware features to help EWRLS make better predictions:
 * - 3 HMM-like state probabilities (trending/ranging detection)
 * - 3 volatility regime probabilities (low/med/high)
 * - 2 regime stability features (duration)
 * - 4 microstructure features (vol ratios, correlations)
 *
 * Fast implementation without Python dependencies:
 * - Simple k-means clustering instead of full HMM/GMM
 * - Rolling window statistics
 * - O(1) incremental updates where possible
 */
class RegimeFeatures {
public:
    static constexpr size_t NUM_REGIME_FEATURES = 12;
    static constexpr size_t WINDOW_SIZE = 90;  // Rolling window for regime detection

    /**
     * Regime feature indices (added to existing 42 features)
     */
    enum RegimeFeatureIndex {
        HMM_State_0_Prob = 0,        // Probability of state 0 (trending up)
        HMM_State_1_Prob = 1,        // Probability of state 1 (ranging)
        HMM_State_2_Prob = 2,        // Probability of state 2 (trending down)

        GMM_Vol_Low_Prob = 3,        // Probability of low volatility
        GMM_Vol_Med_Prob = 4,        // Probability of medium volatility
        GMM_Vol_High_Prob = 5,       // Probability of high volatility

        HMM_State_Duration = 6,      // Bars since last state change
        Vol_Regime_Duration = 7,     // Bars since last vol regime change

        Vol_Ratio_20_60 = 8,         // 20-bar vol / 60-bar vol
        Vol_ZScore = 9,              // Vol z-score vs 60-bar history
        Price_Vol_Correlation = 10,   // 20-bar price-volume correlation
        Volume_ZScore = 11           // Volume z-score vs 60-bar history
    };

    RegimeFeatures();

    /**
     * Extract regime features from price/volume history
     * @param bars Recent bar history (at least 90 bars for reliable detection)
     * @return 12-dimensional feature vector
     *
     * Returns neutral features if not enough data
     */
    Eigen::VectorXd extract(const std::vector<Bar>& bars);

    /**
     * Reset state
     */
    void reset();

    /**
     * Check if ready (enough data)
     */
    bool is_ready() const { return bar_count_ >= 30; }

    /**
     * Get feature names for logging
     */
    static std::vector<std::string> get_feature_names();

private:
    size_t bar_count_ = 0;

    // State tracking
    int last_hmm_state_ = -1;
    int last_vol_regime_ = -1;
    int hmm_state_duration_ = 0;
    int vol_regime_duration_ = 0;

    // Internal feature calculations

    /**
     * Fast HMM-like state detection using k-means on returns
     * Returns: [prob_state_0, prob_state_1, prob_state_2]
     */
    std::array<double, 3> detect_market_state(const std::vector<double>& returns);

    /**
     * Fast volatility regime detection using k-means on rolling vol
     * Returns: [prob_low, prob_med, prob_high]
     */
    std::array<double, 3> detect_volatility_regime(const std::vector<double>& returns);

    /**
     * Calculate rolling volatility
     */
    std::vector<double> calculate_rolling_volatility(const std::vector<double>& returns, int window);

    /**
     * Calculate correlation between two series
     */
    double calculate_correlation(const std::vector<double>& x, const std::vector<double>& y);

    /**
     * Calculate z-score
     */
    double calculate_zscore(double value, const std::vector<double>& history);

    /**
     * Simple k-means clustering (k=3)
     * Returns: cluster assignments [0, 1, 2]
     */
    std::vector<int> kmeans_cluster(const std::vector<double>& data, int k = 3);

    /**
     * Calculate soft probabilities from cluster assignment
     * Uses distance to cluster centers
     */
    std::array<double, 3> calculate_cluster_probabilities(
        double value,
        const std::vector<double>& data,
        const std::vector<int>& clusters
    );
};

} // namespace trading

```

## ðŸ“„ **FILE 14 of 25**: include/trading/multi_symbol_trader.h

**File Information**:
- **Path**: `include/trading/multi_symbol_trader.h`
- **Size**: 650 lines
- **Modified**: 2025-10-20 05:51:14
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "core/types.h"
#include "core/bar.h"
#include "predictor/multi_horizon_predictor.h"
#include "predictor/feature_extractor.h"
#include "trading/position.h"
#include "trading/trade_history.h"
#include "trading/alpaca_cost_model.h"
#include "trading/trade_filter.h"
#include "utils/isotonic_calibrator.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <deque>
#include <numeric>

namespace trading {

/**
 * Prediction Data - Stores multi-horizon prediction and associated information
 */
struct PredictionData {
    MultiHorizonPredictor::MultiHorizonPrediction prediction;  // Multi-horizon prediction
    Eigen::VectorXd features;     // Feature vector (33 dimensions)
    Price current_price;          // Current price
};

/**
 * Trading Configuration
 */
struct TradingConfig {
    double initial_capital = 100000.0;
    size_t max_positions = 3;
    double stop_loss_pct = -0.02;      // -2%
    double profit_target_pct = 0.05;   // 5%
    size_t min_bars_to_learn = 50;     // Warmup period
    size_t lookback_window = 50;
    int bars_per_day = 391;            // 9:30 AM - 4:00 PM inclusive (391 bars)
    bool eod_liquidation = true;
    double win_multiplier = 1.3;
    double loss_multiplier = 0.7;
    size_t trade_history_size = 3;     // Track last N trades for adaptive sizing

    // Multi-horizon prediction settings
    MultiHorizonPredictor::Config horizon_config;

    // Trade filter settings
    TradeFilter::Config filter_config;

    // Cost model settings
    bool enable_cost_tracking = true;  // Enable Alpaca cost model
    double default_avg_volume = 1000000.0;  // Default average daily volume
    double default_volatility = 0.02;  // Default 2% daily volatility

    // Probability-based trading (from online_trader)
    bool enable_probability_scaling = true;   // Convert predictions to probabilities
    double probability_scaling_factor = 50.0; // Tanh scaling factor
    double buy_threshold = 0.55;              // Probability threshold for entry (v1.0 baseline)
    double sell_threshold = 0.45;             // Probability threshold for shorts (v1.0 baseline)

    // Bollinger Band amplification (from online_trader)
    // DISABLED: Testing showed neutral impact (same +0.04% MRD with or without)
    // Marginal improvement on winning days (+0.01-0.02%) when disabled
    // Simpler configuration without manual rule overlays - let EWRLS features decide
    // See BB_AMPLIFICATION_TEST.md for detailed analysis
    bool enable_bb_amplification = false;     // Disabled - no clear benefit, adds complexity
    int bb_period = 20;                       // BB period
    double bb_std_dev = 2.0;                  // BB standard deviations
    double bb_proximity_threshold = 0.30;     // Within 30% of band for boost
    double bb_amplification_factor = 0.10;    // Boost probability by this much

    // Rotation strategy configuration (from online_trader)
    bool enable_rotation = true;              // Enable rank-based rotation
    double rotation_strength_delta = 0.01;    // Minimum improvement (100 bps) to rotate (v1.0 baseline)
    int rotation_cooldown_bars = 10;          // Prevent re-entry after rotation
    double min_rank_strength = 0.001;         // Minimum signal strength (10 bps) to hold

    // Mean reversion predictor configuration
    bool enable_mean_reversion_predictor = false; // Use deviation-based targets instead of raw returns (EXPERIMENTAL)
    double reversion_factor = 0.5;                // Expected reversion strength (0.5 = 50% reversion to MA)
    int ma_period_1bar = 5;                       // MA period for 1-bar predictions (short-term mean)
    int ma_period_5bar = 10;                      // MA period for 5-bar predictions (medium-term mean)
    int ma_period_10bar = 20;                     // MA period for 10-bar predictions (longer-term mean)

    // Signal confirmation configuration (expert recommendation)
    bool enable_signal_confirmation = true;       // Require multiple confirming indicators before entry
    int min_confirmations_required = 1;           // BALANCED: At least 1 of 3 confirmations (2 was too strict)
    double rsi_oversold_threshold = 0.30;         // RSI below this = oversold (bullish signal)
    double rsi_overbought_threshold = 0.70;       // RSI above this = overbought (bearish signal)
    double bb_extreme_threshold = 0.80;           // Within 20% of BB band = extreme (80% from center)
    double volume_surge_threshold = 1.2;          // Volume 20% above average = surge confirmation

    // Price-based exit configuration (mean reversion completion)
    bool enable_price_based_exits = true;         // Exit when mean reversion completes
    bool exit_on_ma_crossover = true;             // Exit when price crosses back through MA
    double trailing_stop_percentage = 0.50;       // Trail stop at 50% of max profit
    int ma_exit_period = 10;                      // MA period for exit crossover detection

    // Dual EWRLS strategy (separate models for above/below MA - handles mean reversion non-linearity)
    // DISABLED: Testing showed dual EWRLS performs 475% worse than baseline (-0.15% vs +0.04% MRD)
    // See DUAL_EWRLS_RESULTS.md for detailed analysis
    bool enable_dual_ewrls = false;               // Use separate EWRLS models for above/below MA
    int dual_ewrls_ma_period = 20;                // MA period for determining above/below state
    double dual_ewrls_min_deviation = 0.005;      // Minimum deviation (0.5%) to use dual models

    // Market Regime Detection Filter (REQ-FILTER-002)
    // Mean reversion works best in LOW ADX (choppy/ranging) markets
    // Avoid trading in HIGH ADX (strong trending) markets where mean reversion fails
    bool enable_regime_filter = false;            // Enable ADX-based regime detection (EXPERIMENTAL)
    int adx_period = 14;                          // ADX calculation period (10-30 bars)
    double max_adx_for_entry = 30.0;              // Max ADX to allow entry (20-40, low ADX = ranging)
    double min_adx_for_exit = 40.0;               // If ADX spikes above this, force exit (30-50)

    // Isotonic Probability Calibration (REQ-SIG-001)
    // Learns true relationship between raw predictions and actual outcomes
    // More flexible than fixed tanh() scaling - adapts to data per symbol
    bool enable_isotonic_calibration = false;     // Enable isotonic regression calibration (EXPERIMENTAL)
    int calibration_min_samples = 100;            // Minimum observations before using calibrator (50-200)
    int calibration_window = 300;                 // Historical window size for calibration (100-500)

    // Multi-Horizon Agreement Filter (REQ-FILTER-001)
    // Require multiple prediction horizons to agree on direction before entry
    // Filters conflicting signals (e.g., short-term reversal + medium-term continuation)
    bool enable_horizon_agreement = false;        // Enable multi-horizon consensus filter (EXPERIMENTAL)
    int min_horizons_agreeing = 2;                // Minimum horizons agreeing (2=majority, 3=unanimous)

    // Prediction Strength Filter (REQ-SIG-002)
    // Require minimum absolute prediction magnitude to filter weak/noisy signals
    // Focus capital on high-conviction predictions (|pred| > threshold)
    bool enable_prediction_strength_filter = false;  // Enable prediction magnitude filter (EXPERIMENTAL)
    double min_abs_prediction = 0.0;              // Minimum |prediction| required for entry (0.0005-0.005)

    // Warmup configuration mode
    enum class WarmupMode {
        PRODUCTION,  // Strict criteria - SAFE FOR LIVE TRADING
        TESTING      // Relaxed criteria - DEVELOPMENT/TESTING ONLY
    };

    // Warmup configuration for improved pre-live validation
    struct WarmupConfig {
        bool enabled = true;                     // Enable warmup phase (DEFAULT)
        int observation_days = 1;                // Learn without trading
        int simulation_days = 2;                 // Paper trade before live

        // Configuration mode (CRITICAL: Set to PRODUCTION before live trading!)
        WarmupMode mode = WarmupMode::PRODUCTION;

        // Go-live criteria (values set based on mode)
        double min_sharpe_ratio;                 // Minimum Sharpe ratio to go live
        double max_drawdown;                     // Maximum drawdown allowed
        int min_trades = 20;                     // Minimum trades to evaluate
        bool require_positive_return;            // Require positive return to go live

        // State preservation
        bool preserve_predictor_state = true;    // Keep EWRLS weights
        bool preserve_trade_history = true;      // Keep trade history for sizing
        double history_decay_factor = 0.7;       // Weight historical trades at 70%

        // Constructor: Initialize based on mode
        WarmupConfig() {
            set_mode(WarmupMode::PRODUCTION);  // DEFAULT TO PRODUCTION (SAFE)
        }

        // Set mode and apply corresponding criteria
        void set_mode(WarmupMode m) {
            mode = m;
            if (mode == WarmupMode::PRODUCTION) {
                // PRODUCTION: Strict criteria - SAFE FOR LIVE TRADING
                min_sharpe_ratio = 0.3;          // Minimum 0.3 Sharpe ratio
                max_drawdown = 0.15;             // Maximum 15% drawdown
                require_positive_return = true;  // Must be profitable
            } else {
                // TESTING: Relaxed criteria - DEVELOPMENT/TESTING ONLY
                min_sharpe_ratio = -2.0;         // Very lenient (allows testing approval logic)
                max_drawdown = 0.30;             // Lenient 30% drawdown
                require_positive_return = false; // Allow negative returns
            }
        }

        // Get mode name for logging
        std::string get_mode_name() const {
            return mode == WarmupMode::PRODUCTION ? "PRODUCTION (STRICT)" : "TESTING (RELAXED)";
        }
    } warmup;

    // ========== AGGRESSIVE KELLY CRITERION POSITION SIZING ==========
    // REQ-1: Enhanced Kelly Fraction with Confidence Scaling
    // Target: +0.10-0.16% MRD improvement from better capital allocation

    struct AggressiveKellyConfig {
        // Master switch for aggressive Kelly features
        bool enable_aggressive_kelly = false;  // Enable enhanced Kelly sizing (DEFAULT: OFF for safety)

        // Kelly fraction scaling based on signal confidence
        double base_kelly_fraction = 0.45;     // Base Kelly (up from 0.25) - more aggressive
        double max_kelly_fraction = 0.65;      // Maximum Kelly for high-confidence signals
        double min_kelly_fraction = 0.25;      // Minimum Kelly for low-confidence signals (conservative floor)

        // Confidence thresholds for Kelly scaling
        double low_confidence_threshold = 0.15;   // Below this = weak signal (use min Kelly)
        double high_confidence_threshold = 0.30;  // Above this = strong signal (use max Kelly)

        // Use actual trade history for win/loss ratio (adaptive)
        bool use_actual_trade_stats = true;    // Use rolling trade history vs hardcoded values
        int trade_stats_window = 50;           // Rolling window size for stats calculation
    } aggressive_kelly;

    // REQ-1.4: Signal Strength Boost Multiplier
    // Increase position size for strong prediction magnitudes
    struct SignalBoostConfig {
        bool enable_signal_boost = false;      // Enable signal strength boosting

        // Prediction magnitude thresholds (in decimal, e.g., 0.01 = 100 bps)
        double weak_prediction_threshold = 0.003;    // 30 bps - weak signal
        double strong_prediction_threshold = 0.010;  // 100 bps - strong signal

        // Multipliers based on signal strength
        double weak_signal_reduction = 0.7;          // Reduce weak signals to 70%
        double strong_signal_boost = 1.8;            // Boost strong signals to 180%
        double max_signal_boost = 2.5;               // Cap boost at 250% to prevent over-concentration
    } signal_boost;

    // REQ-2: Multi-Horizon Consensus Boost
    // Boost positions when all 3 prediction horizons agree
    struct ConsensusBoostConfig {
        bool enable_consensus_boost = false;   // Enable multi-horizon consensus detection

        // Multipliers based on consensus strength
        double weak_consensus_multiplier = 1.0;      // No boost if horizons disagree
        double partial_consensus_multiplier = 1.3;   // Partial boost for directional agreement
        double full_consensus_multiplier = 1.6;      // Full boost for agreement + high confidence
        double perfect_consensus_multiplier = 2.0;   // Max boost for perfect consensus

        // Consensus detection threshold
        double consensus_confidence_threshold = 0.20; // All horizons need >20% confidence
    } consensus_boost;

    // REQ-3: Volatility Opportunity Sizing (for leveraged ETFs)
    // Boost size when high volatility + strong signal converge
    struct VolatilityOpportunityConfig {
        bool enable_vol_opportunity = false;   // Enable volatility opportunity mode

        // Volatility thresholds and multipliers
        double vol_opportunity_threshold = 2.0;        // High vol = 2x average
        double high_vol_strong_signal_boost = 1.5;     // Boost for high vol + strong signal
        double high_vol_weak_signal_reduce = 0.6;      // Reduce for high vol + weak signal

        // Signal strength thresholds for volatility mode
        double strong_signal_threshold = 0.65;         // Prediction strength > 65% = strong
        double weak_signal_threshold = 0.53;           // Prediction strength < 53% = weak
    } vol_opportunity;

    // REQ-4: Winning Streak Acceleration
    // Boost positions during winning streaks
    struct WinningStreakConfig {
        bool enable_winning_streak = false;    // Enable streak-based sizing

        int min_wins_for_boost = 2;            // Need 2+ consecutive wins for boost
        double winning_streak_boost = 1.4;     // Boost per consecutive win (exponential)
        double max_winning_boost = 2.2;        // Cap streak boost at 220%
    } winning_streak;

    // REQ-5: Hard Risk Limits and Circuit Breakers
    // Critical safety controls for aggressive sizing
    struct RiskLimitsConfig {
        // Position-level limits
        double max_single_position_pct = 0.40;     // Maximum 40% in any single position
        double min_position_pct = 0.10;            // Minimum 10% or don't trade

        // Daily risk limits
        double max_daily_loss_pct = -0.015;        // Emergency stop at -1.5% daily loss
        double circuit_breaker_loss_pct = -0.008;  // Reduce sizing at -0.8% daily loss

        // Drawdown limits
        double max_drawdown_from_peak = -0.020;    // Emergency stop at -2.0% drawdown

        // Multiplier safety caps
        double max_total_multiplier = 3.0;         // Hard cap on combined multipliers
        double min_total_multiplier = 0.5;         // Hard floor on combined multipliers
    } risk_limits;

    // Trading phase tracking
    enum Phase {
        WARMUP_OBSERVATION,   // Days 1-2: Learning only
        WARMUP_SIMULATION,    // Days 3-7: Paper trading
        WARMUP_COMPLETE,      // Warmup done, ready for live
        LIVE_TRADING          // Actually trading
    };
    Phase current_phase = LIVE_TRADING;  // Default to live (warmup disabled)

    TradingConfig() {
        // Set reasonable defaults for multi-horizon (RESPONSIVE for minute-bar mean reversion)
        // Expert recommendation: Faster lambdas needed for HFT mean reversion (not slow trend following)
        horizon_config.lambda_1bar = 0.98;   // 34 bar half-life (34 minutes) - responsive to recent regime shifts
        horizon_config.lambda_5bar = 0.99;   // 69 bar half-life (1.15 hours) - medium-term pattern detection
        horizon_config.lambda_10bar = 0.995; // 138 bar half-life (2.3 hours) - longer-term trend context
        horizon_config.min_confidence = 0.4; // Lower threshold for consistency (was 0.5)

        // Set reasonable defaults for trade filter (SELECTIVE for probability-based trading)
        // INCREASED to reduce churning - signal quality should drive exits, not time
        filter_config.min_bars_to_hold = 20;   // Was 5 - too aggressive
        filter_config.typical_hold_period = 60;  // Was 20
        filter_config.max_bars_to_hold = 120;    // Was 60
        filter_config.min_prediction_for_entry = 0.0;     // Disabled (use probability threshold)
        filter_config.min_confidence_for_entry = 0.0;     // Disabled (use probability threshold)
    }
};

/**
 * Daily results structure for multi-day tracking
 */
struct DailyResults {
    int day_number;             // 1, 2, 3, ...
    double start_equity;        // Equity at start of day
    double end_equity;          // Equity at end of day
    double daily_return;        // Return for this day
    int trades_today;           // Trades completed today
    int winning_trades_today;   // Winning trades today
    int losing_trades_today;    // Losing trades today
};

/**
 * Multi-Symbol Online Trading System
 *
 * Features:
 * - Online learning per symbol (EWRLS with 25 features)
 * - Dynamic position management (max N concurrent positions)
 * - Automatic stop-loss and profit targets
 * - Adaptive position sizing based on recent performance
 * - EOD liquidation option
 * - Rotation strategy (top N by predicted return)
 *
 * Usage:
 *   MultiSymbolTrader trader(symbols, config);
 *   for (each bar) {
 *       trader.on_bar(market_data);
 *   }
 *   auto results = trader.get_results();
 */
class MultiSymbolTrader {
private:
    std::vector<Symbol> symbols_;
    TradingConfig config_;
    double cash_;

    // Exit tracking data for price-based exits
    struct ExitTrackingData {
        double entry_ma = 0.0;           // MA value at entry time
        double max_profit_pct = 0.0;     // Maximum profit % seen
        Price max_profit_price = 0.0;    // Price where max profit occurred
        bool is_long = true;             // Direction of position
    };

    // Per-symbol components
    std::unordered_map<Symbol, std::unique_ptr<MultiHorizonPredictor>> predictors_;
    std::unordered_map<Symbol, std::unique_ptr<MultiHorizonPredictor>> predictors_above_ma_;  // Dual EWRLS: above MA model
    std::unordered_map<Symbol, std::unique_ptr<MultiHorizonPredictor>> predictors_below_ma_;  // Dual EWRLS: below MA model
    std::unordered_map<Symbol, std::unique_ptr<FeatureExtractor>> extractors_;
    std::unordered_map<Symbol, std::unique_ptr<IsotonicCalibrator>> calibrators_;  // REQ-SIG-001: Probability calibration
    std::unordered_map<Symbol, PositionWithCosts> positions_;
    std::unordered_map<Symbol, ExitTrackingData> exit_tracking_;  // Price-based exit tracking
    std::unordered_map<Symbol, std::unique_ptr<TradeHistory>> trade_history_;
    std::unordered_map<Symbol, MarketContext> market_context_;  // Market microstructure data

    // Multi-horizon return tracking for predictor updates
    std::unordered_map<Symbol, std::deque<double>> price_history_;  // Track for multi-bar returns

    // Trade filtering and frequency management
    std::unique_ptr<TradeFilter> trade_filter_;

    // Complete trade log for export (not circular, keeps all trades)
    std::vector<TradeRecord> all_trades_log_;

    size_t bars_seen_;       // Total bars including warmup
    size_t trading_bars_;    // Trading bars only (excludes warmup) - used for EOD timing
    int total_trades_;
    double total_transaction_costs_;  // Track cumulative costs

    // Daily tracking (for multi-day testing)
    std::vector<DailyResults> daily_results_;
    double daily_start_equity_;       // Equity at start of current day
    int daily_start_trades_;          // Trades at start of current day
    int daily_winning_trades_;        // Winning trades today
    int daily_losing_trades_;         // Losing trades today

    // Equity tracking for drawdown calculation
    double equity_high_water_mark_;   // Track peak equity for drawdown
    double max_drawdown_observed_;    // Maximum drawdown observed

    // Warmup phase tracking
    struct SimulationMetrics {
        std::vector<TradeRecord> simulated_trades;
        double starting_equity = 0.0;
        double current_equity = 0.0;
        double max_equity = 0.0;
        double max_drawdown = 0.0;
        int observation_bars_complete = 0;
        int simulation_bars_complete = 0;

        void update_drawdown() {
            max_equity = std::max(max_equity, current_equity);
            double drawdown = (max_equity > 0) ?
                (max_equity - current_equity) / max_equity : 0.0;
            max_drawdown = std::max(max_drawdown, drawdown);
        }

        double calculate_sharpe() const {
            if (simulated_trades.size() < 2) return 0.0;

            std::vector<double> returns;
            for (const auto& trade : simulated_trades) {
                returns.push_back(trade.pnl_pct);
            }

            double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
            double sq_sum = std::inner_product(returns.begin(), returns.end(), returns.begin(), 0.0);
            double stdev = std::sqrt(sq_sum / returns.size() - mean * mean);

            return (stdev > 0) ? (mean / stdev) * std::sqrt(252) : 0.0; // Annualized
        }
    };

    SimulationMetrics warmup_metrics_;

    // Rotation tracking (from online_trader)
    std::unordered_map<Symbol, int> rotation_cooldowns_;  // Bars until can re-enter after rotation

    // Phase management methods
    void update_phase();
    void handle_observation_phase(const std::unordered_map<Symbol, Bar>& market_data);
    void handle_simulation_phase(const std::unordered_map<Symbol, PredictionData>& predictions,
                                const std::unordered_map<Symbol, Bar>& market_data);
    void handle_live_phase(const std::unordered_map<Symbol, PredictionData>& predictions,
                          const std::unordered_map<Symbol, Bar>& market_data);
    bool evaluate_warmup_complete();
    void print_warmup_summary();

public:
    /**
     * Constructor
     * @param symbols List of symbols to trade
     * @param config Trading configuration (optional, uses defaults if not provided)
     */
    explicit MultiSymbolTrader(const std::vector<Symbol>& symbols,
                              const TradingConfig& config = TradingConfig());

    /**
     * Process new market data bar
     * @param market_data Map of symbol -> bar for current timestamp
     *
     * Steps:
     * 1. Extract features and make predictions for each symbol
     * 2. Update predictors with realized returns
     * 3. Update existing positions (check stop-loss/profit targets)
     * 4. Make trading decisions (rotation to top N predicted symbols)
     * 5. EOD liquidation if enabled
     */
    void on_bar(const std::unordered_map<Symbol, Bar>& market_data);

    /**
     * Get current equity (cash + position values)
     */
    double get_equity(const std::unordered_map<Symbol, Bar>& market_data) const;

    /**
     * Backtest results structure
     */
    struct BacktestResults {
        double total_return;        // Total return as fraction
        double mrd;                 // Mean Return per Day
        double final_equity;        // Final equity value
        int total_trades;           // Number of completed trades
        int winning_trades;         // Number of profitable trades
        int losing_trades;          // Number of losing trades
        double win_rate;            // Fraction of winning trades
        double avg_win;             // Average win amount
        double avg_loss;            // Average loss amount
        double profit_factor;       // Gross profit / gross loss
        double max_drawdown;        // Maximum drawdown as fraction
        double sharpe_ratio;        // Sharpe ratio (annualized)

        // Cost tracking
        double total_transaction_costs;  // Sum of all transaction costs
        double avg_cost_per_trade;       // Average cost per trade
        double cost_as_pct_of_volume;    // Costs as % of total volume traded
        double net_return_after_costs;   // Return after accounting for costs

        // Daily breakdown (for multi-day testing)
        std::vector<DailyResults> daily_breakdown;
    };

    /**
     * Get backtest results
     */
    BacktestResults get_results() const;

    /**
     * Get current positions (for monitoring)
     */
    const std::unordered_map<Symbol, PositionWithCosts>& positions() const { return positions_; }

    /**
     * Get current cash
     */
    double cash() const { return cash_; }

    /**
     * Get configuration
     */
    const TradingConfig& config() const { return config_; }

    /**
     * Get all trades (for export)
     */
    std::vector<TradeRecord> get_all_trades() const {
        return all_trades_log_;
    }

private:
    /**
     * Update drawdown tracking with current equity
     */
    void update_drawdown(double current_equity) {
        // Update high water mark
        if (current_equity > equity_high_water_mark_) {
            equity_high_water_mark_ = current_equity;
        }

        // Calculate current drawdown
        if (equity_high_water_mark_ > 0) {
            double current_drawdown = (equity_high_water_mark_ - current_equity) / equity_high_water_mark_;
            max_drawdown_observed_ = std::max(max_drawdown_observed_, current_drawdown);
        }
    }

    /**
     * Make trading decisions based on predictions
     */
    void make_trades(const std::unordered_map<Symbol, PredictionData>& predictions,
                    const std::unordered_map<Symbol, Bar>& market_data);

    /**
     * Update existing positions (check exit conditions with trade filter)
     */
    void update_positions(const std::unordered_map<Symbol, Bar>& market_data,
                         const std::unordered_map<Symbol, PredictionData>& predictions);

    /**
     * Calculate position size for a symbol using Kelly Criterion and adaptive sizing
     */
    double calculate_position_size(const Symbol& symbol, const PredictionData& pred_data);

    /**
     * Enter new position
     */
    void enter_position(const Symbol& symbol, Price price, Timestamp time, double capital, uint64_t bar_id);

    /**
     * Check if a new position is compatible with existing positions
     * (prevents inverse/contradictory positions like TQQQ + SQQQ)
     */
    bool is_position_compatible(const Symbol& new_symbol) const;

    /**
     * Exit existing position
     */
    double exit_position(const Symbol& symbol, Price price, Timestamp time, uint64_t bar_id);

    /**
     * Liquidate all positions
     */
    void liquidate_all(const std::unordered_map<Symbol, Bar>& market_data, const std::string& reason);

    /**
     * Update market context for cost calculations
     */
    void update_market_context(const Symbol& symbol, const Bar& bar);

    /**
     * Calculate minutes from market open (9:30 AM ET)
     */
    int calculate_minutes_from_open(Timestamp ts) const;

    /**
     * Convert prediction to probability using tanh scaling (from online_trader)
     */
    double prediction_to_probability(double prediction) const;

    /**
     * Apply Bollinger Band amplification to probability
     */
    double apply_bb_amplification(double probability, const Symbol& symbol,
                                  const Bar& bar, bool is_long) const;

    /**
     * Calculate Bollinger Bands for a symbol
     */
    struct BBands {
        double middle = 0.0;
        double upper = 0.0;
        double lower = 0.0;
    };
    BBands calculate_bollinger_bands(const Symbol& symbol, const Bar& current_bar) const;

    /**
     * Check signal confirmation using multiple indicators (expert recommendation)
     * @param symbol Symbol to check
     * @param bar Current bar
     * @param features Feature vector containing RSI, volume, etc.
     * @param is_long True for long signals, false for short signals
     * @return Number of confirmations (0-3: RSI, BB, Volume)
     */
    int check_signal_confirmations(const Symbol& symbol, const Bar& bar,
                                   const Eigen::VectorXd& features, bool is_long) const;

    /**
     * Calculate moving average for exit detection
     * @param symbol Symbol to calculate MA for
     * @return MA value, or 0.0 if insufficient data
     */
    double calculate_exit_ma(const Symbol& symbol) const;

    /**
     * Check if position should exit based on price-based logic
     * @param symbol Symbol to check
     * @param current_price Current price
     * @param exit_reason Output parameter for exit reason
     * @return True if should exit
     */
    bool should_exit_on_price(const Symbol& symbol, Price current_price, std::string& exit_reason);

    /**
     * Find weakest current position for rotation (from online_trader)
     * Returns symbol with lowest signal strength, or empty string if no positions
     */
    Symbol find_weakest_position(const std::unordered_map<Symbol, PredictionData>& predictions) const;

    /**
     * Update rotation cooldowns (decrement each bar)
     */
    void update_rotation_cooldowns();

    /**
     * Check if symbol is in rotation cooldown
     */
    bool in_rotation_cooldown(const Symbol& symbol) const;

};

} // namespace trading

```

## ðŸ“„ **FILE 15 of 25**: optimization/objective_function.py

**File Information**:
- **Path**: `optimization/objective_function.py`
- **Size**: 386 lines
- **Modified**: 2025-10-21 11:38:53
- **Type**: py
- **Permissions**: -rw-r--r--

```text
#!/usr/bin/env python3
"""
Objective Function for Optuna Optimization
Runs backtests with trial parameters and extracts performance metrics
"""

import subprocess
import json
import re
import os
import sys
from typing import Dict, List, Tuple
from pathlib import Path


def load_base_config(config_path: str = "config/strategy.json") -> Dict:
    """Load base configuration from JSON file"""
    with open(config_path, 'r') as f:
        return json.load(f)


def merge_trial_params(base_config: Dict, trial_params: Dict) -> Dict:
    """
    Merge trial parameters into base configuration
    Handles nested configs (horizon_config, filter_config, aggressive_kelly, etc.)
    """
    config = base_config.copy()

    for key, value in trial_params.items():
        # Handle nested configs
        if key.startswith('lambda_'):
            # Horizon config parameters
            if 'horizon_config' not in config:
                config['horizon_config'] = {}
            config['horizon_config'][key] = value

        elif key in ['min_bars_to_hold', 'typical_hold_period', 'max_bars_to_hold',
                     'min_prediction_for_entry', 'min_confidence_for_entry']:
            # Filter config parameters
            if 'filter_config' not in config:
                config['filter_config'] = {}
            config['filter_config'][key] = value

        elif key in ['win_multiplier', 'loss_multiplier', 'trade_history_size']:
            # Position sizing parameters
            if 'position_sizing' not in config:
                config['position_sizing'] = {}
            config['position_sizing'][key] = value

        elif key in ['enable_cost_tracking', 'default_avg_volume', 'default_volatility',
                     'base_slippage_bps', 'size_impact_factor', 'volatility_multiplier',
                     'time_of_day_factor']:
            # Cost model parameters
            if 'cost_model' not in config:
                config['cost_model'] = {}
            config['cost_model'][key] = value

        elif key in ['initial_variance', 'max_variance', 'max_gradient_norm',
                     'stability_check_interval']:
            # EWRLS config parameters
            if 'ewrls_config' not in config:
                config['ewrls_config'] = {}
            config['ewrls_config'][key] = value

        else:
            # Top-level parameters
            config[key] = value

    return config


def write_trial_config(config: Dict, trial_id: int, study_id: int = None,
                       output_dir: str = "results") -> str:
    """
    Write trial configuration to JSON file with unique naming

    Args:
        config: Complete configuration dictionary
        trial_id: Trial number within study
        study_id: Study ID to prevent overwriting between studies
        output_dir: Output directory

    Returns:
        Path to written config file
    """
    os.makedirs(output_dir, exist_ok=True)

    # Include study_id in filename to prevent overwriting across studies
    if study_id is not None:
        config_path = f"{output_dir}/study{study_id:03d}_trial_{trial_id:04d}_config.json"
    else:
        config_path = f"{output_dir}/trial_{trial_id:04d}_config.json"

    with open(config_path, 'w') as f:
        json.dump(config, f, indent=2)

    return config_path


def run_backtest_single_date(date: str, config_path: str, trial_id: int,
                             warmup_days: int = 1, verbose: bool = False) -> Dict:
    """
    Run backtest for a single date with proper warmup protocol

    Warmup protocol:
    - 1 day warmup (simple EWRLS learning only, no ML exits)
    - 1 day test (this date - measure MRD)

    Returns:
        Dict with keys: date, mrd, success, error_msg, and all metrics
    """
    results_file = f"results/trial_{trial_id:04d}_{date}.json"

    # Simple warmup: just 1 day of EWRLS learning, NO ML exit training
    cmd = [
        './build/sentio_lite', 'mock',
        '--date', date,
        '--warmup-days', '1',        # Simple 1-day warmup (no ML)
        '--config', config_path,
        '--results-file', results_file,
        '--no-dashboard'
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120  # 2 minute timeout per day
        )

        if verbose:
            print(f"  [{date}] Command: {' '.join(cmd)}")
            if result.returncode != 0:
                print(f"  [{date}] stderr: {result.stderr[:200]}")

        # Check if results file was created
        if not os.path.exists(results_file):
            return {
                'date': date,
                'mrd': 0.0,
                'success': False,
                'error_msg': 'Results file not created'
            }

        # Load results from JSON
        with open(results_file, 'r') as f:
            results = json.load(f)

        # Extract metrics from nested performance object
        perf = results.get('performance', {})

        return {
            'date': date,
            'mrd': perf.get('mrd', 0.0),
            'total_return': perf.get('total_return', 0.0),
            'final_equity': perf.get('final_equity', 100000.0),
            'total_trades': perf.get('total_trades', 0),
            'win_rate': perf.get('win_rate', 0.0),
            'profit_factor': perf.get('profit_factor', 0.0),
            'max_drawdown': perf.get('max_drawdown', 0.0),
            'sharpe_ratio': perf.get('sharpe_ratio', 0.0),
            'success': True,
            'error_msg': None
        }

    except subprocess.TimeoutExpired:
        return {
            'date': date,
            'mrd': 0.0,
            'success': False,
            'error_msg': 'Timeout (>120s)'
        }
    except Exception as e:
        return {
            'date': date,
            'mrd': 0.0,
            'success': False,
            'error_msg': str(e)
        }


def run_backtest_multi_date(dates: List[str], config_path: str, trial_id: int,
                            warmup_days: int = 1, verbose: bool = False) -> Dict:
    """
    Run backtest across multiple dates as INDEPENDENT daily tests

    **NEW INDEPENDENT DAILY PROTOCOL:**
    For each test day:
    - 1 day observation (learning only, no trading)
    - 10 days simulation trading (train Daily Exit Learner with sufficient data)
    - 1 day test (measure MRD for this day)
    - Reset and repeat for next day

    This matches real production trading where:
    - Each day is independent with fresh warmup
    - No state contamination between test days
    - Daily performance is measured and averaged
    - Total: 11 warmup days + 1 test day per test date

    Returns:
        Dict with aggregated metrics: mrd_avg (average of all daily MRDs), etc.
    """
    if not dates:
        raise ValueError("Must provide at least one date")

    sorted_dates = sorted(dates)
    num_test_days = len(dates)

    if verbose:
        print(f"  Running INDEPENDENT daily tests for {num_test_days} days")
        print(f"    Each day: 1 observation + 10 simulation + 1 test")
        print(f"    Test dates: {', '.join(sorted_dates)}")

    # Run independent backtest for each day
    daily_results = []
    failed_days = 0

    for test_date in sorted_dates:
        result = run_backtest_single_date(
            date=test_date,
            config_path=config_path,
            trial_id=trial_id,
            warmup_days=1,  # This triggers --enable-warmup --warmup-obs-days 1 --warmup-sim-days 10
            verbose=verbose
        )

        if result['success']:
            daily_results.append(result)
        else:
            failed_days += 1
            if verbose:
                print(f"    âŒ Day {test_date} failed: {result.get('error_msg', 'Unknown error')}")

    # Aggregate metrics across all successful days
    if not daily_results:
        if verbose:
            print(f"    âŒ All {num_test_days} days failed")
        return {
            'mrd_avg': -999.0,
            'mrd_std': 0.0,
            'total_return': -999.0,
            'sharpe_ratio': -999.0,
            'max_drawdown': 1.0,
            'profit_factor': 0.0,
            'win_rate': 0.0,
            'avg_trades_per_day': 0.0,
            'success_rate': 0.0,
            'num_days': 0,
            'failed_days': failed_days
        }

    # Calculate averages
    mrds = [r['mrd'] for r in daily_results]
    returns = [r['total_return'] for r in daily_results]
    trades = [r['total_trades'] for r in daily_results]
    win_rates = [r['win_rate'] for r in daily_results]
    profit_factors = [r['profit_factor'] for r in daily_results]
    max_drawdowns = [r['max_drawdown'] for r in daily_results]
    sharpe_ratios = [r['sharpe_ratio'] for r in daily_results]

    import numpy as np
    mrd_avg = float(np.mean(mrds))
    mrd_std = float(np.std(mrds)) if len(mrds) > 1 else 0.0
    total_return_avg = float(np.mean(returns))
    avg_trades_per_day = float(np.mean(trades))
    win_rate_avg = float(np.mean(win_rates))
    profit_factor_avg = float(np.mean(profit_factors))
    max_drawdown_avg = float(np.mean(max_drawdowns))
    sharpe_ratio_avg = float(np.mean(sharpe_ratios))

    success_rate = len(daily_results) / num_test_days

    if verbose:
        print(f"    âœ… Aggregated results ({len(daily_results)}/{num_test_days} days):")
        print(f"       MRD: {mrd_avg*100:+.3f}% Â± {mrd_std*100:.3f}%")
        print(f"       Trades/day: {avg_trades_per_day:.1f}")
        print(f"       Sharpe: {sharpe_ratio_avg:.2f}")
        print(f"       Win rate: {win_rate_avg*100:.1f}%")

    return {
        'mrd_avg': mrd_avg,
        'mrd_std': mrd_std,
        'total_return': total_return_avg,
        'sharpe_ratio': sharpe_ratio_avg,
        'max_drawdown': max_drawdown_avg,
        'profit_factor': profit_factor_avg,
        'win_rate': win_rate_avg,
        'avg_trades_per_day': avg_trades_per_day,
        'success_rate': success_rate,
        'num_days': len(daily_results),
        'failed_days': failed_days
    }


def evaluate_trial(trial_params: Dict, trial_id: int, dates: List[str],
                   base_config_path: str = "config/strategy.json",
                   warmup_days: int = 1, verbose: bool = False,
                   study_id: int = None) -> tuple[Dict, Dict]:
    """
    Main evaluation function for a trial

    Args:
        trial_params: Parameters suggested by Optuna
        trial_id: Trial number
        dates: List of dates to test (e.g., ['2025-10-13', '2025-10-14', ...])
        base_config_path: Path to base configuration file
        warmup_days: Warmup period before each test date
        verbose: Print detailed progress
        study_id: Optuna study ID for unique file naming

    Returns:
        Tuple of (metrics, complete_config) for storage in Optuna database
    """
    if verbose:
        print(f"\n{'='*60}")
        print(f"Trial {trial_id}")
        print(f"{'='*60}")
        print(f"Parameters: {trial_params}")

    # Load base config and merge trial params
    base_config = load_base_config(base_config_path)
    trial_config = merge_trial_params(base_config, trial_params)

    # Write trial config to file with unique naming
    config_path = write_trial_config(trial_config, trial_id, study_id)

    if verbose:
        print(f"Config written to: {config_path}")
        print(f"Testing dates: {', '.join(dates)}")

    # Run backtests
    metrics = run_backtest_multi_date(dates, config_path, trial_id, warmup_days, verbose)

    if verbose:
        print(f"\nResults:")
        print(f"  Average MRD:     {metrics['mrd_avg']*100:+.3f}%")
        print(f"  MRD Std Dev:     {metrics['mrd_std']*100:.3f}%")
        print(f"  Sharpe Ratio:    {metrics['sharpe_ratio']:.2f}")
        print(f"  Profit Factor:   {metrics['profit_factor']:.2f}")
        print(f"  Win Rate:        {metrics['win_rate']*100:.1f}%")
        print(f"  Trades/Day:      {metrics['avg_trades_per_day']:.1f}")
        print(f"  Max Drawdown:    {metrics['max_drawdown']*100:.2f}%")
        print(f"  Success Rate:    {metrics['success_rate']*100:.0f}% ({metrics['num_days']}/{metrics['num_days']+metrics['failed_days']} days)")

    # Return both metrics and complete config for database storage
    return metrics, trial_config


def cleanup_trial_files(trial_id: int, keep_config: bool = False):
    """Clean up temporary trial files"""
    if not keep_config:
        config_file = f"results/trial_{trial_id:04d}_config.json"
        if os.path.exists(config_file):
            os.remove(config_file)

    # Optionally clean up individual date results
    # (Keep for now for debugging)


if __name__ == '__main__':
    # Test the objective function
    print("Testing objective function...")

    # Test with baseline parameters
    test_params = {
        'buy_threshold': 0.55,
        'sell_threshold': 0.45,
        'lambda_1bar': 0.98,
        'lambda_5bar': 0.99,
        'lambda_10bar': 0.995
    }

    test_dates = ['2025-10-13', '2025-10-14', '2025-10-15']

    metrics = evaluate_trial(
        trial_params=test_params,
        trial_id=9999,
        dates=test_dates,
        warmup_days=1,
        verbose=True
    )

    print(f"\n{'='*60}")
    print("Test completed successfully!")
    print(f"Average MRD: {metrics['mrd_avg']*100:+.3f}%")

```

## ðŸ“„ **FILE 16 of 25**: optimization/optuna_optimizer.py

**File Information**:
- **Path**: `optimization/optuna_optimizer.py`
- **Size**: 439 lines
- **Modified**: 2025-10-19 23:53:54
- **Type**: py
- **Permissions**: -rw-r--r--

```text
#!/usr/bin/env python3
"""
Optuna Optimization Framework for Sentio Lite
Multi-phase Bayesian optimization targeting +0.50% MRD
"""

import optuna
from optuna.samplers import TPESampler
from optuna.pruners import MedianPruner
import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List
import time

# Import our optimization modules
from parameter_spaces import (
    suggest_phase1_params,
    suggest_phase2_params,
    suggest_phase3_params,
    suggest_phase4_params,
    PRIORITY_1_PARAMS,
    PRIORITY_2_PARAMS,
    PRIORITY_3_PARAMS,
    PRIORITY_4_PARAMS
)
from objective_function import evaluate_trial


class OptunaOptimizer:
    """Main optimizer class for Sentio Lite parameter optimization"""

    def __init__(self, phase: int, n_trials: int, study_name: str = None,
                 dates: List[str] = None, base_config: Dict = None,
                 warmup_days: int = 1, verbose: bool = False, n_jobs: int = 1):
        """
        Initialize optimizer

        Args:
            phase: Optimization phase (1-4)
            n_trials: Number of trials to run
            study_name: Name for the Optuna study (default: auto-generated)
            dates: List of dates for training (default: Oct 13-15, 2025)
            base_config: Base configuration dict (or path to load from phase N-1)
            warmup_days: Warmup period for backtests
            verbose: Print detailed progress
            n_jobs: Number of parallel jobs (default: 1)
        """
        self.phase = phase
        self.n_trials = n_trials
        self.study_name = study_name or f"sentio_lite_phase{phase}_{int(time.time())}"
        self.dates = dates or ['2025-10-13', '2025-10-14', '2025-10-15']  # Training set
        self.base_config = base_config
        self.warmup_days = warmup_days
        self.verbose = verbose
        self.n_jobs = n_jobs

        # Storage path for Optuna study
        self.storage_path = f"sqlite:///results/optuna_phase{phase}.db"

        # Create results directory
        Path("results").mkdir(exist_ok=True)

        # Set up sampler based on phase
        # Use TPE sampler for all phases (intelligent Bayesian optimization)
        # Phase 1 gets more startup trials for exploration
        n_startup = 30 if phase == 1 else 20
        self.sampler = TPESampler(
            n_startup_trials=n_startup,  # Random exploration first
            n_ei_candidates=48,           # Increased for better exploration
            multivariate=True,            # Use multivariate TPE
            group=True,                   # Group correlated parameters
            constant_liar=True,           # For parallel execution
            seed=42                       # Reproducibility
        )

        # Set up adaptive pruner
        # Prune trials that underperform after testing on 2+ days
        self.pruner = MedianPruner(
            n_startup_trials=10,    # Don't prune first 10 trials
            n_warmup_steps=1,       # Allow at least 1 day before pruning
            interval_steps=1        # Check after each day
        )

        print(f"\n{'='*70}")
        print(f"Optuna Optimizer - Phase {phase}")
        print(f"{'='*70}")
        print(f"Study name:     {self.study_name}")
        print(f"Storage:        {self.storage_path}")
        print(f"Trials:         {n_trials}")
        print(f"Parallel jobs:  {self.n_jobs}")
        print(f"Training dates: {', '.join(self.dates)}")
        print(f"Sampler:        {type(self.sampler).__name__}")
        print(f"Pruner:         {type(self.pruner).__name__}")
        print(f"{'='*70}\n")


    def objective(self, trial: optuna.Trial) -> float:
        """
        Objective function wrapper for Optuna

        Args:
            trial: Optuna trial object

        Returns:
            Objective value (we want to MAXIMIZE average MRD)
        """
        # Suggest parameters based on phase
        if self.phase == 1:
            params = suggest_phase1_params(trial)
        elif self.phase == 2:
            params = suggest_phase2_params(trial, self.base_config)
        elif self.phase == 3:
            params = suggest_phase3_params(trial, self.base_config)
        elif self.phase == 4:
            params = suggest_phase4_params(trial, self.base_config)
        else:
            raise ValueError(f"Invalid phase: {self.phase}")

        # Run backtest evaluation
        # Get study_id from trial's study attribute
        study_id = trial.study._study_id if hasattr(trial.study, '_study_id') else None

        metrics, complete_config = evaluate_trial(
            trial_params=params,
            trial_id=trial.number,
            dates=self.dates,
            warmup_days=self.warmup_days,
            verbose=self.verbose,
            study_id=study_id
        )

        # Log metrics to Optuna
        trial.set_user_attr('mrd_avg', metrics['mrd_avg'])
        trial.set_user_attr('mrd_std', metrics['mrd_std'])
        trial.set_user_attr('sharpe_ratio', metrics['sharpe_ratio'])
        trial.set_user_attr('profit_factor', metrics['profit_factor'])
        trial.set_user_attr('win_rate', metrics['win_rate'])
        trial.set_user_attr('max_drawdown', metrics['max_drawdown'])
        trial.set_user_attr('avg_trades_per_day', metrics['avg_trades_per_day'])
        trial.set_user_attr('success_rate', metrics['success_rate'])

        # CRITICAL: Store complete config in database for exact reproducibility
        # Note: Optuna JSON-encodes user attributes automatically, so pass Python objects directly
        # Do NOT use json.dumps() here or it will be double-encoded
        trial.set_user_attr('complete_config_json', complete_config)
        trial.set_user_attr('training_dates', self.dates)

        # Apply constraints (prune if violated)
        # 1. Must have successful backtests on all days
        if metrics['success_rate'] < 1.0:
            raise optuna.TrialPruned(f"Failed on {metrics['failed_days']} days")

        # 2. Must have reasonable trading activity (not zero trades)
        if metrics['avg_trades_per_day'] < 1.0:
            raise optuna.TrialPruned("Too few trades (< 1/day)")

        # 3. Must not have excessive drawdown (> 10%)
        if metrics['max_drawdown'] > 0.10:
            raise optuna.TrialPruned(f"Excessive drawdown: {metrics['max_drawdown']*100:.1f}%")

        # 4. Profit factor should be positive (optional constraint for later phases)
        if self.phase >= 2 and metrics['profit_factor'] < 0.3:
            raise optuna.TrialPruned(f"Low profit factor: {metrics['profit_factor']:.2f}")

        # Single-objective optimization: Maximize MRD only
        # Return MRD as the objective value (no composite score)
        return metrics['mrd_avg']


    def run(self):
        """Run optimization study"""
        # Create or load study
        study = optuna.create_study(
            study_name=self.study_name,
            storage=self.storage_path,
            direction='maximize',  # Maximize MRD
            sampler=self.sampler,
            pruner=self.pruner,
            load_if_exists=True
        )

        # Run optimization
        print(f"Starting optimization with {self.n_trials} trials...")
        print(f"Objective: Maximize MRD (Mean Return per Day)")
        if self.n_jobs > 1:
            print(f"  Parallel execution: {self.n_jobs} workers")
        print(f"Training on: {', '.join(self.dates)}\n")

        study.optimize(
            self.objective,
            n_trials=self.n_trials,
            n_jobs=self.n_jobs,  # Enable parallel execution
            show_progress_bar=True,
            catch=(Exception,)  # Continue on errors
        )

        # Print results
        self.print_results(study)

        # Save best parameters
        self.save_best_params(study)

        return study


    def select_robust_trial(self, study: optuna.Study) -> optuna.Trial:
        """
        Select most robust trial from top 5 by MRD

        Strategy:
        1. Get top 5 trials by MRD (objective value)
        2. Score each by robustness (Sharpe, consistency, drawdown, trade frequency)
        3. Return trial with highest robustness score

        Args:
            study: Completed Optuna study

        Returns:
            Most robust trial from top 5
        """
        # Get all completed trials
        completed = [t for t in study.trials if t.state == optuna.trial.TrialState.COMPLETE]

        if not completed:
            raise ValueError("No completed trials found")

        # Get top 5 by MRD
        top_5_by_mrd = sorted(completed, key=lambda t: t.value, reverse=True)[:5]

        print(f"\n{'='*70}")
        print(f"Top 5 Trials by MRD (before robustness selection):")
        print(f"{'='*70}")
        for i, trial in enumerate(top_5_by_mrd, 1):
            print(f"{i}. Trial #{trial.number:3d}: MRD={trial.value*100:+.3f}%, "
                  f"Sharpe={trial.user_attrs['sharpe_ratio']:+.2f}, "
                  f"PF={trial.user_attrs['profit_factor']:.2f}, "
                  f"Trades/day={trial.user_attrs['avg_trades_per_day']:.1f}")

        # Score each trial by robustness
        def robustness_score(trial):
            """
            Composite robustness score (higher is better)

            Weights:
            - Sharpe ratio: 3.0 (primary: risk-adjusted returns)
            - Consistency: 2.0 (1 / mrd_std, penalize volatility)
            - Safety: 2.0 (1 / max_drawdown, reward low drawdown)
            - Profitability: 1.0 (profit factor)
            - Activity: 1.0 (reward higher trade frequency, shows signal quality)
            """
            sharpe = trial.user_attrs['sharpe_ratio']
            mrd_std = trial.user_attrs['mrd_std']
            max_dd = trial.user_attrs['max_drawdown']
            pf = trial.user_attrs['profit_factor']
            trades_per_day = trial.user_attrs['avg_trades_per_day']

            # Normalize trade frequency (prefer 30-100 trades/day, penalize extremes)
            if trades_per_day < 10:
                trade_score = trades_per_day / 10.0  # Penalize too few trades
            elif trades_per_day > 150:
                trade_score = 150.0 / trades_per_day  # Penalize excessive trading
            else:
                trade_score = 1.0  # Optimal range

            score = (
                sharpe * 3.0 +                          # Risk-adjusted returns (most important)
                (1.0 / (mrd_std + 0.001)) * 2.0 +      # Consistency (penalize volatility)
                (1.0 / (max_dd + 0.001)) * 2.0 +       # Safety (reward low drawdown)
                pf * 1.0 +                              # Profitability
                trade_score * 1.0                       # Activity level
            )
            return score

        # Select trial with highest robustness score
        best_robust = max(top_5_by_mrd, key=robustness_score)

        # Find rank in MRD ordering
        mrd_rank = top_5_by_mrd.index(best_robust) + 1

        print(f"\n{'='*70}")
        print(f"ðŸŽ¯ ROBUST SELECTION: Trial #{best_robust.number}")
        print(f"{'='*70}")
        print(f"  MRD Rank:        #{mrd_rank} of top 5")
        print(f"  MRD:             {best_robust.value*100:+.3f}%")
        print(f"  Sharpe Ratio:    {best_robust.user_attrs['sharpe_ratio']:+.2f} â­")
        print(f"  MRD Std Dev:     {best_robust.user_attrs['mrd_std']*100:.3f}%")
        print(f"  Max Drawdown:    {best_robust.user_attrs['max_drawdown']*100:.2f}%")
        print(f"  Profit Factor:   {best_robust.user_attrs['profit_factor']:.2f}")
        print(f"  Trades/Day:      {best_robust.user_attrs['avg_trades_per_day']:.1f}")
        print(f"  Robustness:      {robustness_score(best_robust):.2f} (composite score)")

        if mrd_rank > 1:
            print(f"\n  â„¹ï¸  Selected #{mrd_rank} by MRD for superior robustness")

        return best_robust

    def print_results(self, study: optuna.Study):
        """Print optimization results"""
        print(f"\n{'='*70}")
        print(f"Optimization Complete - Phase {self.phase}")
        print(f"{'='*70}\n")

        # Use robust selection to pick best trial
        best_trial = self.select_robust_trial(study)

        print(f"\nSelected Parameters:")
        for param, value in best_trial.params.items():
            print(f"  {param:30s} {value}")

        # Statistics
        print(f"\nOptimization Statistics:")
        print(f"  Total trials:    {len(study.trials)}")
        print(f"  Completed:       {len([t for t in study.trials if t.state == optuna.trial.TrialState.COMPLETE])}")
        print(f"  Pruned:          {len([t for t in study.trials if t.state == optuna.trial.TrialState.PRUNED])}")
        print(f"  Failed:          {len([t for t in study.trials if t.state == optuna.trial.TrialState.FAIL])}")


    def save_best_params(self, study: optuna.Study):
        """Save best parameters to config file (using robust selection)"""
        # Use robust selection to get best trial
        best_trial = self.select_robust_trial(study)
        best_params = best_trial.params
        best_metrics = best_trial.user_attrs

        output = {
            'phase': self.phase,
            'study_name': self.study_name,
            'timestamp': time.strftime('%Y-%m-%d %H:%M:%S'),
            'training_dates': self.dates,
            'best_trial': best_trial.number,
            'best_mrd': best_trial.value,
            'best_params': best_params,
            'best_metrics': best_metrics,
            'selection_method': 'robust_top5'  # Document selection method
        }

        output_file = f"results/best_params_phase{self.phase}.json"
        with open(output_file, 'w') as f:
            json.dump(output, f, indent=2)

        print(f"\nâœ… Best parameters saved to: {output_file}")

        # Also save as config for next phase
        if self.base_config:
            # Merge best params into base config
            from objective_function import merge_trial_params
            next_config = merge_trial_params(self.base_config, best_params)
        else:
            # Load base config and merge
            from objective_function import load_base_config, merge_trial_params
            base = load_base_config()
            next_config = merge_trial_params(base, best_params)

        config_file = f"config/strategy_phase{self.phase}_best.json"
        with open(config_file, 'w') as f:
            json.dump(next_config, f, indent=2)

        print(f"âœ… Full config saved to: {config_file}")
        print(f"   (Use this as base for Phase {self.phase + 1})")


def main():
    parser = argparse.ArgumentParser(
        description='Optuna optimization for Sentio Lite trading system',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Phase 1: Quick wins (grid search, 1000 trials)
  python3 optimization/optuna_optimizer.py --phase 1 --trials 1000

  # Phase 2: Medium impact (Bayesian, 300 trials, inherit from Phase 1)
  python3 optimization/optuna_optimizer.py --phase 2 --trials 300 \\
      --base-config config/strategy_phase1_best.json

  # Phase 3: Advanced tuning (200 trials)
  python3 optimization/optuna_optimizer.py --phase 3 --trials 200 \\
      --base-config config/strategy_phase2_best.json

  # Quick test run (10 trials)
  python3 optimization/optuna_optimizer.py --phase 1 --trials 10 --verbose
        """
    )

    parser.add_argument('--phase', type=int, required=True, choices=[1, 2, 3, 4],
                       help='Optimization phase (1=Quick wins, 2=Medium, 3=Advanced, 4=Experimental)')
    parser.add_argument('--trials', type=int, required=True,
                       help='Number of trials to run')
    parser.add_argument('--study-name', type=str,
                       help='Custom study name (default: auto-generated)')
    parser.add_argument('--base-config', type=str,
                       help='Base config path (for phase 2+)')
    parser.add_argument('--dates', type=str, nargs='+',
                       default=['2025-10-13', '2025-10-14', '2025-10-15'],
                       help='Training dates (default: Oct 13-15, 2025)')
    parser.add_argument('--warmup-days', type=int, default=1,
                       help='Warmup days for backtests (default: 1)')
    parser.add_argument('--n-jobs', type=int, default=1,
                       help='Number of parallel jobs (default: 1, use 4 for speed)')
    parser.add_argument('--verbose', action='store_true',
                       help='Print detailed progress')

    args = parser.parse_args()

    # Load base config if provided
    base_config = None
    if args.base_config:
        from objective_function import load_base_config
        base_config = load_base_config(args.base_config)
        print(f"Loaded base config from: {args.base_config}\n")
    elif args.phase > 1:
        print(f"âš ï¸  WARNING: Phase {args.phase} should inherit from Phase {args.phase-1}")
        print(f"   Consider using --base-config config/strategy_phase{args.phase-1}_best.json\n")

    # Create optimizer
    optimizer = OptunaOptimizer(
        phase=args.phase,
        n_trials=args.trials,
        study_name=args.study_name,
        dates=args.dates,
        base_config=base_config,
        warmup_days=args.warmup_days,
        n_jobs=args.n_jobs,
        verbose=args.verbose
    )

    # Run optimization
    study = optimizer.run()

    print(f"\n{'='*70}")
    print("Optimization complete!")
    print(f"{'='*70}\n")

    return 0


if __name__ == '__main__':
    sys.exit(main())

```

## ðŸ“„ **FILE 17 of 25**: optimization/parameter_spaces.py

**File Information**:
- **Path**: `optimization/parameter_spaces.py`
- **Size**: 173 lines
- **Modified**: 2025-10-20 06:51:20
- **Type**: py
- **Permissions**: -rw-r--r--

```text
"""
Parameter Search Spaces for Optuna Optimization
Defines search ranges for tunable parameters organized by priority
Focus: More trades with better risk management (no aggressive Kelly)
"""

from typing import Dict, Any
import optuna

# Priority 1: Core Trading Parameters (High Impact, Fast Testing)
# Target: +0.10-0.15% MRD improvement through higher trade frequency
# Strategy: Lower entry thresholds, optimize 3-4 positions, tight risk management
PRIORITY_1_PARAMS = {
    # Entry thresholds (LOWERED for more trades)
    'buy_threshold': (0.45, 0.60),          # Entry threshold for longs (was 0.50-0.65)
    'sell_threshold': (0.30, 0.48),         # Entry threshold for shorts (was 0.35-0.50)

    # EWRLS learning rates
    'lambda_1bar': (0.95, 0.99),            # EWRLS lambda 1-bar horizon
    'lambda_5bar': (0.96, 0.995),           # EWRLS lambda 5-bar horizon
    'lambda_10bar': (0.97, 0.999),          # EWRLS lambda 10-bar horizon

    # Portfolio size (NEW: test 3-4 positions)
    'max_positions': (3, 4),                # Number of concurrent positions

    # Rotation
    'rotation_strength_delta': (0.005, 0.020),  # Rotation improvement threshold
    'min_confirmations_required': (0, 2),       # Signal confirmation count

    # Risk management (CRITICAL for more trades)
    'stop_loss_pct': (-0.025, -0.008),      # Stop loss (tighter range)
    'profit_target_pct': (0.020, 0.060),    # Profit target (tighter range)

    # BB Amplification
    'bb_proximity_threshold': (0.1, 0.5),   # Distance from band to trigger amplification
    'bb_amplification_factor': (0.05, 0.3), # Probability boost at band extremes
}

# Priority 2: Medium Impact (Medium Impact, Medium Risk, Medium Cost)
# Target: +0.20-0.30% MRD cumulatively
PRIORITY_2_PARAMS = {
    'min_bars_to_hold': (10, 40),           # Minimum holding period
    'max_bars_to_hold': (60, 200),          # Maximum holding period
    'rsi_oversold_threshold': (0.20, 0.40), # RSI oversold level
    'rsi_overbought_threshold': (0.60, 0.80),  # RSI overbought level
    'bb_extreme_threshold': (0.70, 0.90),   # Bollinger band extreme
    'volume_surge_threshold': (1.0, 1.5),   # Volume surge multiplier
    'trailing_stop_percentage': (0.30, 0.70),  # Trailing stop %
    'probability_scaling_factor': (20.0, 100.0),  # Tanh scaling factor
}

# Priority 3: Advanced Tuning (Low/Medium Impact, Higher Risk, Variable Cost)
# Target: +0.40-0.50% MRD cumulatively
PRIORITY_3_PARAMS = {
    'max_positions': (2, 5),                # Portfolio size
    'rotation_cooldown_bars': (5, 20),      # Rotation cooldown
    'min_rank_strength': (0.0005, 0.005),   # Min signal strength
    'lookback_window': (30, 100),           # Feature lookback
    'win_multiplier': (1.0, 2.0),           # Position sizing win mult
    'loss_multiplier': (0.5, 1.0),          # Position sizing loss mult
    'ma_exit_period': (5, 20),              # MA exit period
    'bb_period': (10, 30),                  # Bollinger band period
}

# Priority 4: Experimental (Unknown Impact, High Risk, Expensive)
# Only use if Priority 1-3 insufficient
PRIORITY_4_PARAMS = {
    'initial_variance': (10.0, 1000.0),     # EWRLS initial variance
    'max_variance': (100.0, 10000.0),       # EWRLS max variance
    'max_gradient_norm': (0.5, 5.0),        # Gradient clipping
    'reversion_factor': (0.3, 0.8),         # Mean reversion factor
}


def suggest_phase1_params(trial: optuna.Trial) -> Dict[str, Any]:
    """Suggest Priority 1 parameters (Core trading params, no aggressive Kelly)"""
    params = {}

    # Priority 1 parameters only
    for name, (low, high) in PRIORITY_1_PARAMS.items():
        if isinstance(low, int):
            params[name] = trial.suggest_int(name, low, high)
        else:
            params[name] = trial.suggest_float(name, low, high)

    return params


def suggest_phase2_params(trial: optuna.Trial, base_config: Dict[str, Any]) -> Dict[str, Any]:
    """Suggest Priority 2 parameters (inherit from Phase 1)"""
    params = base_config.copy()

    for name, (low, high) in PRIORITY_2_PARAMS.items():
        if isinstance(low, int):
            params[name] = trial.suggest_int(name, low, high)
        else:
            params[name] = trial.suggest_float(name, low, high)

    return params


def suggest_phase3_params(trial: optuna.Trial, base_config: Dict[str, Any]) -> Dict[str, Any]:
    """Suggest Priority 3 parameters (inherit from Phase 2)"""
    params = base_config.copy()

    for name, (low, high) in PRIORITY_3_PARAMS.items():
        if isinstance(low, int):
            params[name] = trial.suggest_int(name, low, high)
        else:
            params[name] = trial.suggest_float(name, low, high)

    return params


def suggest_phase4_params(trial: optuna.Trial, base_config: Dict[str, Any]) -> Dict[str, Any]:
    """Suggest Priority 4 parameters (experimental)"""
    params = base_config.copy()

    for name, (low, high) in PRIORITY_4_PARAMS.items():
        if isinstance(low, int):
            params[name] = trial.suggest_int(name, low, high)
        else:
            params[name] = trial.suggest_float(name, low, high, log=True)  # Log scale for variances

    return params


def get_param_count(phase: int) -> int:
    """Get number of parameters for each phase"""
    counts = {
        1: len(PRIORITY_1_PARAMS),
        2: len(PRIORITY_1_PARAMS) + len(PRIORITY_2_PARAMS),
        3: len(PRIORITY_1_PARAMS) + len(PRIORITY_2_PARAMS) + len(PRIORITY_3_PARAMS),
        4: len(PRIORITY_1_PARAMS) + len(PRIORITY_2_PARAMS) + len(PRIORITY_3_PARAMS) + len(PRIORITY_4_PARAMS),
    }
    return counts.get(phase, 0)


def print_search_space_summary():
    """Print summary of search spaces"""
    print("=" * 70)
    print("Parameter Search Space Summary")
    print("=" * 70)
    print()

    print(f"Priority 1 (Quick Wins): {len(PRIORITY_1_PARAMS)} parameters")
    for name, (low, high) in PRIORITY_1_PARAMS.items():
        print(f"  {name:30s} [{low:8.4f}, {high:8.4f}]")
    print()

    print(f"Priority 2 (Medium Impact): {len(PRIORITY_2_PARAMS)} parameters")
    for name, (low, high) in PRIORITY_2_PARAMS.items():
        print(f"  {name:30s} [{low:8.4f}, {high:8.4f}]")
    print()

    print(f"Priority 3 (Advanced Tuning): {len(PRIORITY_3_PARAMS)} parameters")
    for name, (low, high) in PRIORITY_3_PARAMS.items():
        print(f"  {name:30s} [{low:8.4f}, {high:8.4f}]")
    print()

    print(f"Priority 4 (Experimental): {len(PRIORITY_4_PARAMS)} parameters")
    for name, (low, high) in PRIORITY_4_PARAMS.items():
        print(f"  {name:30s} [{low:8.4f}, {high:8.4f}]")
    print()

    total = (len(PRIORITY_1_PARAMS) + len(PRIORITY_2_PARAMS) +
             len(PRIORITY_3_PARAMS) + len(PRIORITY_4_PARAMS))
    print(f"Total parameters: {total}")
    print("=" * 70)


if __name__ == '__main__':
    print_search_space_summary()

```

## ðŸ“„ **FILE 18 of 25**: results/best_params_phase1.json

**File Information**:
- **Path**: `results/best_params_phase1.json`
- **Size**: 125 lines
- **Modified**: 2025-10-21 14:25:19
- **Type**: json
- **Permissions**: -rw-r--r--

```text
{
  "phase": 1,
  "study_name": "phase1_54features_21warmup",
  "timestamp": "2025-10-21 14:25:19",
  "training_dates": [
    "2025-10-14",
    "2025-10-15",
    "2025-10-16",
    "2025-10-17",
    "2025-10-20"
  ],
  "best_trial": 140,
  "best_mrd": 0.00026,
  "best_params": {
    "buy_threshold": 0.5992968469241474,
    "sell_threshold": 0.45137166806173395,
    "lambda_1bar": 0.9611433605793056,
    "lambda_5bar": 0.9745639479591365,
    "lambda_10bar": 0.9745067358776138,
    "max_positions": 3,
    "rotation_strength_delta": 0.018306619084026022,
    "min_confirmations_required": 2,
    "stop_loss_pct": -0.022848386562814495,
    "profit_target_pct": 0.043555592222521665,
    "bb_proximity_threshold": 0.23784221275767636,
    "bb_amplification_factor": 0.14025910631760477
  },
  "best_metrics": {
    "avg_trades_per_day": 2.4,
    "complete_config_json": {
      "_comment": "Sentio Lite Trading Strategy Configuration - Complete (71 parameters)",
      "_version": "1.0.2",
      "_description": "Baseline EWRLS v1.0.1 - All parameters externalized for Optuna optimization",
      "initial_capital": 100000.0,
      "max_positions": 3,
      "stop_loss_pct": -0.022848386562814495,
      "profit_target_pct": 0.043555592222521665,
      "bars_per_day": 391,
      "eod_liquidation": true,
      "min_bars_to_learn": 100,
      "lookback_window": 50,
      "horizon_config": {
        "lambda_1bar": 0.9611433605793056,
        "lambda_5bar": 0.9745639479591365,
        "lambda_10bar": 0.9745067358776138,
        "min_confidence": 0.4
      },
      "filter_config": {
        "min_bars_to_hold": 20,
        "typical_hold_period": 60,
        "max_bars_to_hold": 120,
        "min_prediction_for_entry": 0.0,
        "min_confidence_for_entry": 0.0
      },
      "enable_probability_scaling": true,
      "probability_scaling_factor": 50.0,
      "buy_threshold": 0.5992968469241474,
      "sell_threshold": 0.45137166806173395,
      "enable_bb_amplification": false,
      "bb_period": 20,
      "bb_std_dev": 2.0,
      "bb_proximity_threshold": 0.23784221275767636,
      "bb_amplification_factor": 0.14025910631760477,
      "enable_rotation": true,
      "rotation_strength_delta": 0.018306619084026022,
      "rotation_cooldown_bars": 10,
      "min_rank_strength": 0.001,
      "enable_signal_confirmation": true,
      "min_confirmations_required": 2,
      "rsi_oversold_threshold": 0.3,
      "rsi_overbought_threshold": 0.7,
      "bb_extreme_threshold": 0.8,
      "volume_surge_threshold": 1.2,
      "enable_price_based_exits": true,
      "exit_on_ma_crossover": true,
      "trailing_stop_percentage": 0.5,
      "ma_exit_period": 10,
      "enable_dual_ewrls": false,
      "dual_ewrls_ma_period": 20,
      "dual_ewrls_min_deviation": 0.005,
      "enable_mean_reversion_predictor": false,
      "reversion_factor": 0.5,
      "ma_period_1bar": 5,
      "ma_period_5bar": 10,
      "ma_period_10bar": 20,
      "position_sizing": {
        "win_multiplier": 1.3,
        "loss_multiplier": 0.7,
        "trade_history_size": 3
      },
      "cost_model": {
        "enable_cost_tracking": true,
        "default_avg_volume": 1000000.0,
        "default_volatility": 0.02,
        "base_slippage_bps": 1.0,
        "size_impact_factor": 0.5,
        "volatility_multiplier": 1.5,
        "time_of_day_factor": 1.0
      },
      "ewrls_config": {
        "initial_variance": 100.0,
        "max_variance": 1000.0,
        "max_gradient_norm": 1.0,
        "stability_check_interval": 100
      },
      "min_confidence_weight": 0.5030873473424331,
      "max_confidence_weight": 1.5831702036557977
    },
    "max_drawdown": 0.00041999999999999996,
    "mrd_avg": 0.00026,
    "mrd_std": 0.0007337574531137656,
    "profit_factor": 399.6,
    "sharpe_ratio": 0.0,
    "success_rate": 1.0,
    "training_dates": [
      "2025-10-14",
      "2025-10-15",
      "2025-10-16",
      "2025-10-17",
      "2025-10-20"
    ],
    "win_rate": 0.4
  },
  "selection_method": "robust_top5"
}
```

## ðŸ“„ **FILE 19 of 25**: src/main.cpp

**File Information**:
- **Path**: `src/main.cpp`
- **Size**: 1520 lines
- **Modified**: 2025-10-21 14:09:22
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "trading/multi_symbol_trader.h"
#include "trading/trading_mode.h"
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
#include <thread>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <set>

using namespace trading;
using json = nlohmann::json;  // For FIFO bar parsing in live mode

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

    // Date for testing (mock mode)
    std::string test_date;  // YYYY-MM-DD format (single day or recent if empty)

    // Warmup period
    int warmup_days = 2;     // Default 2 days (minimum 1)
    size_t warmup_bars = 0;  // Calculated from warmup_days
    bool auto_adjust_warmup = true;  // Auto-adjust if insufficient data

    // Dashboard generation (enabled by default)
    bool generate_dashboard = true;
    std::string dashboard_script = "scripts/rotation_trading_dashboard_html.py";
    std::string results_file = "results.json";
    std::string trades_file = "trades.jsonl";
    std::string dashboard_output = "trading_dashboard.html";  // Will be updated with timestamp

    // Configuration file
    std::string config_file = "config/strategy.json";

    // Trading parameters
    TradingConfig trading;
};

void print_usage(const char* program_name) {
    std::cout << "Sentio Lite - Multi-Symbol Rotation Trading\n\n"
              << "Two Modes (share exact same trading logic):\n"
              << "  mock  - Test on historical data (default: most recent date)\n"
              << "  live  - Real-time paper trading via Alpaca/Polygon\n\n"
              << "Usage: " << program_name << " <mock|live> [options]\n\n"
              << "Common Options:\n"
              << "  --warmup-days N      Warmup days before trading (default: 2, minimum: 1)\n"
              << "  --enable-warmup      Enable warmup system (observation + simulation phases)\n"
              << "  --warmup-obs-days N  Observation phase days (default: 2, learning only)\n"
              << "  --warmup-sim-days N  Simulation phase days (default: 5, paper trading)\n"
              << "  --warmup-mode MODE   Warmup mode: production or testing (default: production)\n"
              << "                       âš ï¸  PRODUCTION = strict criteria (SAFE for live)\n"
              << "                       âš ï¸  TESTING = relaxed criteria (DEVELOPMENT ONLY)\n"
              << "  --capital AMOUNT     Initial capital (default: 100000)\n"
              << "  --max-positions N    Max concurrent positions (default: 3)\n"
              << "  --config PATH        Configuration file path (default: config/strategy.json)\n"
              << "  --no-dashboard       Disable HTML dashboard report (enabled by default)\n"
              << "  --verbose            Show detailed progress\n\n"
              << "Mock Mode Options:\n"
              << "  --date YYYY-MM-DD    Test specific date (if omitted, uses most recent date)\n"
              << "  --data-dir DIR       Data directory (default: data)\n"
              << "  --extension EXT      File extension: .bin or .csv (default: .bin)\n\n"
              << "Live Mode Options:\n"
              << "  --fifo PATH          FIFO pipe path (default: /tmp/alpaca_bars.fifo)\n"
              << "  --websocket TYPE     Websocket: alpaca or polygon (default: alpaca)\n\n"
              << "Trading Parameters:\n"
              << "  --stop-loss PCT      Stop loss percentage (default: -0.02)\n"
              << "  --profit-target PCT  Profit target percentage (default: 0.05)\n"
              << "  --lambda LAMBDA      EWRLS forgetting factor (default: 0.98)\n\n"
              << "Output Options:\n"
              << "  --results-file FILE  Results JSON file (default: results.json)\n"
              << "  --help               Show this help message\n\n"
              << "Examples:\n\n"
              << "  # Mock mode - test most recent date\n"
              << "  " << program_name << " mock\n\n"
              << "  # Mock mode - test specific date\n"
              << "  " << program_name << " mock --date 2024-10-15\n\n"
              << "  # Mock mode - test without dashboard\n"
              << "  " << program_name << " mock --date 2024-10-15 --no-dashboard\n\n"
              << "  # Live mode - paper trading\n"
              << "  " << program_name << " live\n\n"
              << "  # Live mode - with custom warmup period\n"
              << "  " << program_name << " live --warmup-days 5\n\n"
              << "Symbol Configuration:\n"
              << "  Symbols are loaded from config/symbols.conf\n"
              << "  Edit config/symbols.conf to change the symbol list\n\n"
              << "Key Insight:\n"
              << "  Mock and live modes share the EXACT same trading logic.\n"
              << "  Research and optimize in mock mode, then run live with confidence!\n";
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

    if (mode_arg != "mock" && mode_arg != "live") {
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

    // First pass: extract --config flag if provided
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config.config_file = argv[++i];
        }
    }

    // Load trading configuration from JSON (falls back to defaults if missing)
    std::cout << "\n";
    config.trading = ConfigLoader::loadFromJSON(config.config_file);
    std::cout << "\n";

    // Parse remaining options (these can override JSON config)
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        // Config file (already processed in first pass, skip here)
        else if (arg == "--config" && i + 1 < argc) {
            ++i;  // Skip the path argument
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
        // Date option (mock mode)
        else if (arg == "--date" && i + 1 < argc) {
            config.test_date = argv[++i];
        }
        // Warmup
        else if (arg == "--warmup-days" && i + 1 < argc) {
            config.warmup_days = std::stoi(argv[++i]);
        }
        else if (arg == "--no-auto-adjust-warmup") {
            config.auto_adjust_warmup = false;
        }
        else if (arg == "--enable-warmup") {
            config.trading.warmup.enabled = true;
        }
        else if (arg == "--warmup-obs-days" && i + 1 < argc) {
            config.trading.warmup.observation_days = std::stoi(argv[++i]);
        }
        else if (arg == "--warmup-sim-days" && i + 1 < argc) {
            config.trading.warmup.simulation_days = std::stoi(argv[++i]);
        }
        else if (arg == "--warmup-mode" && i + 1 < argc) {
            std::string mode_str = argv[++i];
            std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(), ::tolower);
            if (mode_str == "production") {
                config.trading.warmup.set_mode(TradingConfig::WarmupMode::PRODUCTION);
            } else if (mode_str == "testing") {
                config.trading.warmup.set_mode(TradingConfig::WarmupMode::TESTING);
                std::cerr << "âš ï¸  WARNING: Warmup mode set to TESTING (relaxed criteria)\n";
                std::cerr << "âš ï¸  NOT SAFE FOR LIVE TRADING! Use 'production' mode for real money.\n";
            } else {
                std::cerr << "Invalid warmup mode: " << mode_str << " (use 'production' or 'testing')\n";
                return false;
            }
        }
        // Trading parameters
        else if (arg == "--capital" && i + 1 < argc) {
            config.capital = std::stod(argv[++i]);
            config.trading.initial_capital = config.capital;
        }
        else if (arg == "--max-positions" && i + 1 < argc) {
            config.trading.max_positions = std::stoul(argv[++i]);
        }
        else if (arg == "--stop-loss" && i + 1 < argc) {
            config.trading.stop_loss_pct = std::stod(argv[++i]);
        }
        else if (arg == "--profit-target" && i + 1 < argc) {
            config.trading.profit_target_pct = std::stod(argv[++i]);
        }
        else if (arg == "--lambda" && i + 1 < argc) {
            // Set all lambda values to the same (can be customized further if needed)
            double lambda = std::stod(argv[++i]);
            config.trading.horizon_config.lambda_1bar = lambda;
            config.trading.horizon_config.lambda_5bar = lambda;
            config.trading.horizon_config.lambda_10bar = lambda;
        }
        else if (arg == "--min-threshold" && i + 1 < argc) {
            config.trading.filter_config.min_prediction_for_entry = std::stod(argv[++i]);
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

    // Calculate warmup bars
    config.warmup_bars = config.warmup_days * config.trading.bars_per_day;

    return true;
}

void generate_dashboard(const std::string& results_file, const std::string& script_path,
                        const std::string& trades_file, const std::string& output_file,
                        const std::string& data_dir, double initial_capital,
                        const std::string& test_date) {
    std::cout << "\nGenerating dashboard...\n";

    // Build command with all required arguments
    std::ostringstream cmd;
    cmd << "python3 " << script_path
        << " --trades " << trades_file
        << " --output " << output_file
        << " --start-equity " << std::fixed << std::setprecision(0) << initial_capital
        << " --data-dir " << data_dir
        << " --results " << results_file;

    if (!test_date.empty()) {
        cmd << " --start-date " << test_date;
        cmd << " --end-date " << test_date;
    }

    std::string command = cmd.str();
    int ret = system(command.c_str());

    if (ret != 0) {
        std::cerr << "âš ï¸  Dashboard generation failed (code: " << ret << ")\n";
        std::cerr << "   Command: " << command << "\n";
    } else {
        std::cout << "âœ… Dashboard generated: " << output_file << "\n";

        // Auto-open dashboard in default browser
        std::cout << "ðŸŒ Opening dashboard in browser...\n";
        std::string open_cmd = "open \"" + output_file + "\"";
        int open_ret = system(open_cmd.c_str());

        if (open_ret != 0) {
            std::cerr << "âš ï¸  Failed to open dashboard automatically\n";
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
        struct tm* timeinfo = localtime(&time);
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

// Filter bars to specific date (and warmup period before it)
void filter_to_date(std::unordered_map<Symbol, std::vector<Bar>>& all_data,
                   const std::string& date_str, size_t warmup_bars, int bars_per_day, bool verbose = false) {
    if (all_data.empty()) return;

    // Get trading days from first symbol
    const auto& first_symbol_bars = all_data.begin()->second;
    std::vector<std::string> trading_days = get_trading_days(first_symbol_bars);

    // Calculate warmup days needed
    int warmup_days = (warmup_bars + bars_per_day - 1) / bars_per_day;

    // Find warmup start date by counting backwards from target date
    std::string warmup_start_date = find_warmup_start_date(trading_days, date_str, warmup_days);

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
        std::cout << "  Target date: " << date_str << "\n";
        std::cout << "  Warmup days needed: " << warmup_days << "\n";
        std::cout << "  Warmup start date: " << warmup_start_date << "\n";
    }

    // Filter each symbol to this date range
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

        // Determine test date
        std::string test_date = config.test_date;

        if (test_date.empty()) {
            test_date = get_most_recent_date(all_data);
            std::cout << "Testing most recent date: " << test_date << "\n";
        } else {
            std::cout << "Testing specific date: " << test_date << "\n";
        }

        // Find minimum number of bars across all symbols
        size_t min_bars = std::numeric_limits<size_t>::max();
        for (const auto& [symbol, bars] : all_data) {
            min_bars = std::min(min_bars, bars.size());
        }

        std::cout << "\nData Statistics:\n";
        std::cout << "  Total bars available: " << min_bars << " (~"
                  << (min_bars / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Requested warmup: " << config.warmup_bars << " bars (~"
                  << (config.warmup_bars / config.trading.bars_per_day) << " days)\n";

        // ====== CRITICAL FIX: Smart warmup adjustment ======
        size_t original_warmup = config.warmup_bars;

        if (config.auto_adjust_warmup && min_bars < config.warmup_bars + 100) {
            // For single-day testing, use minimal warmup
            if (min_bars <= config.trading.bars_per_day) {
                // Single day: use 50 bars warmup (enough for features)
                config.warmup_bars = 50;
                std::cout << "\nâš ï¸  Single-day test detected. Adjusting warmup to "
                         << config.warmup_bars << " bars\n";
            } else {
                // Multiple days: use proportional warmup
                config.warmup_bars = std::min(
                    static_cast<size_t>(min_bars * 0.3),  // Use 30% for warmup
                    static_cast<size_t>(config.trading.bars_per_day * 2)  // Max 2 days
                );
                std::cout << "\nâš ï¸  Insufficient data for requested warmup.\n";
                std::cout << "   Auto-adjusting warmup to " << config.warmup_bars
                         << " bars (~" << (config.warmup_bars / config.trading.bars_per_day)
                         << " days)\n";
            }
        }

        // Ensure minimum bars for feature extraction (50 bars required)
        if (config.warmup_bars < 50) {
            config.warmup_bars = 50;
        }

        if (min_bars < config.warmup_bars) {
            std::cerr << "\nâŒ ERROR: Not enough data!\n";
            std::cerr << "   Available: " << min_bars << " bars\n";
            std::cerr << "   Required: " << config.warmup_bars << " bars (minimum for features)\n";
            return 1;
        }

        // Filter to test date using actual timestamps (not bar count!)
        std::cout << "\nFiltering to test date (including warmup period)...\n";
        filter_to_date(all_data, test_date, config.warmup_bars, config.trading.bars_per_day, config.verbose);

        // Show filtered bar counts
        for (const auto& [symbol, bars] : all_data) {
            std::cout << "  " << symbol << ": " << bars.size() << " bars\n";
        }

        // CRITICAL: Recalculate min_bars after filtering!
        min_bars = std::numeric_limits<size_t>::max();
        for (const auto& [symbol, bars] : all_data) {
            if (bars.size() < min_bars) {
                min_bars = bars.size();
            }
        }

        // ========================================
        // DATA AVAILABILITY VALIDATION
        // ========================================

        // Calculate minimum required bars (warmup + at least 1 trading day)
        size_t min_required_bars = config.warmup_bars + config.trading.bars_per_day;

        if (min_bars == 0) {
            std::cerr << "\nâŒ ERROR: No data available for the specified date!\n";
            std::cerr << "\nDebug Information:\n";
            std::cerr << "  Requested: " << test_date << "\n";
            std::cerr << "  Data directory: " << config.data_dir << "\n";
            std::cerr << "  File extension: " << config.extension << "\n";
            std::cerr << "\nPossible causes:\n";
            std::cerr << "  1. Date out of range (check available dates in data files)\n";
            std::cerr << "  2. Incorrect date format (use YYYY-MM-DD)\n";
            std::cerr << "  3. Data files are empty or corrupted\n";
            return 1;
        }

        if (min_bars < min_required_bars) {
            std::cerr << "\nâŒ ERROR: Insufficient data for the specified date(s)!\n";
            std::cerr << "\nData Availability:\n";
            std::cerr << "  Available bars: " << min_bars << " (~"
                     << (min_bars / config.trading.bars_per_day) << " days)\n";
            std::cerr << "  Required bars:  " << min_required_bars << " (~"
                     << (min_required_bars / config.trading.bars_per_day) << " days)\n";
            std::cerr << "    - Warmup:     " << config.warmup_bars << " bars ("
                     << config.warmup_days << " days)\n";
            std::cerr << "    - Trading:    " << config.trading.bars_per_day
                     << " bars (1 day minimum)\n";
            std::cerr << "\nDebug Information:\n";
            std::cerr << "  Requested: " << test_date << "\n";

            // Show per-symbol breakdown
            std::cerr << "\n  Bars per symbol after filtering:\n";
            for (const auto& [symbol, bars] : all_data) {
                std::cerr << "    " << symbol << ": " << bars.size() << " bars\n";
            }

            std::cerr << "\nSuggestions:\n";
            std::cerr << "  1. Reduce warmup period: --warmup-days 1\n";
            std::cerr << "  2. Choose a different date range with more data\n";
            std::cerr << "  3. Check that data files contain the requested dates\n";
            return 1;
        }

        // Calculate trading days available
        size_t trading_bars = min_bars - config.warmup_bars;
        size_t trading_days = trading_bars / config.trading.bars_per_day;

        std::cout << "\nâœ… Data validation passed:\n";
        std::cout << "  Total bars:    " << min_bars << " (~"
                  << (min_bars / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Warmup:        " << config.warmup_bars << " bars ("
                  << config.warmup_days << " days)\n";
        std::cout << "  Trading:       " << trading_bars << " bars (~"
                  << trading_days << " days)\n";

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

        std::cout << "\nRunning MOCK mode (" << min_bars << " bars)...\n";
        std::cout << "  Warmup: " << config.warmup_bars << " bars (~"
                  << (config.warmup_bars / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Trading: " << (min_bars - config.warmup_bars) << " bars (~"
                  << ((min_bars - config.warmup_bars) / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Features: 54 features (8 time + 34 technical + 12 regime)\n";
        std::cout << "  Predictor: Multi-Horizon EWRLS (1/5/10 bars, Î»="
                  << config.trading.horizon_config.lambda_1bar << "/"
                  << config.trading.horizon_config.lambda_5bar << "/"
                  << config.trading.horizon_config.lambda_10bar << ")\n";
        std::cout << "  Strategy: Multi-symbol rotation (top " << config.trading.max_positions << ")\n";
        std::cout << "  Min prediction threshold: " << config.trading.filter_config.min_prediction_for_entry << "\n";
        std::cout << "  Min holding period: " << config.trading.filter_config.min_bars_to_hold << " bars\n\n";

        // Adjust min_bars_to_learn based on warmup
        // CRITICAL FIX: Add 1 to skip overnight gap between last warmup bar and first test day bar
        // This ensures we trade the FULL test day (all 391 bars) instead of including the last bar of warmup day
        // Example: warmup_bars=1173 means bars 0-1172 (3 days), test day starts at bar 1173+1=1174
        config.trading.min_bars_to_learn = config.warmup_bars + 1;

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
                std::cout << "  âœ… Warmup complete (" << config.warmup_bars
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
            std::cout << "\nâš ï¸  NO TRADES EXECUTED - Debugging Info:\n";
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

        ResultsExporter::export_json(
            results, trader, config.results_file,
            symbols_str, "MOCK",
            test_date, test_date
        );

        if (!config.generate_dashboard) {
            // Only show export confirmation when dashboard is disabled
            std::cout << "\nâœ… Results exported to: " << config.results_file << "\n";
        }

        // Export trades for dashboard (only if dashboard enabled)
        if (config.generate_dashboard) {
            export_trades_jsonl(trader, "trades.jsonl");
            std::cout << "\nâœ… Results exported to: " << config.results_file << "\n";
            std::cout << "âœ… Trades exported to: trades.jsonl\n";
        }

        // Print results
        std::cout << "\n";
        std::cout << "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n";
        std::cout << "â•‘                 MOCK MODE Results                          â•‘\n";
        std::cout << "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n";
        std::cout << "\n";

        std::cout << "Test Summary:\n";
        std::cout << "  Test Date:          " << test_date << "\n";
        std::cout << "  Warmup:             " << (config.warmup_bars / config.trading.bars_per_day) << " days\n";
        std::cout << "  Trading Period:     " << ((min_bars - config.warmup_bars) / config.trading.bars_per_day) << " days\n";
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
            std::cout << "ðŸŸ¢ Excellent (ready for live)\n";
        } else if (results.total_return > 0.01 && results.win_rate > 0.50) {
            std::cout << "ðŸŸ¡ Good (consider more testing)\n";
        } else if (results.total_return > 0.0) {
            std::cout << "ðŸŸ  Moderate (needs optimization)\n";
        } else {
            std::cout << "ðŸ”´ Poor (not ready for live)\n";
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

            // Generate unique dashboard filename
            std::string date_for_filename = config.test_date.empty() ? "latest" : config.test_date;
            std::string dashboard_file = "logs/dashboard/dashboard_" +
                                        date_for_filename + "_" + std::string(timestamp) + ".html";

            generate_dashboard(
                config.results_file,
                config.dashboard_script,
                config.trades_file,
                dashboard_file,
                config.data_dir,
                config.capital,
                test_date
            );
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nâŒ Error: " << e.what() << "\n\n";
        return 1;
    }
}

int run_live_mode(Config& config) {
    std::cout << "\n";
    std::cout << "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n";
    std::cout << "â•‘              LIVE MODE (Paper Trading)                     â•‘\n";
    std::cout << "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n";
    std::cout << "\n";

    try {
        // FIFO pipe path (created by WebSocket bridge)
        const std::string fifo_path = "/tmp/alpaca_bars.fifo";

        std::cout << "Live Trading Configuration:\n";
        std::cout << "  FIFO Pipe:      " << fifo_path << " (will be created by bridge)\n";
        std::cout << "  Symbols (" << config.symbols.size() << "): ";
        for (size_t i = 0; i < config.symbols.size(); ++i) {
            std::cout << config.symbols[i];
            if (i < config.symbols.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
        std::cout << "  Max Positions:  " << config.trading.max_positions << "\n";
        std::cout << "  Stop Loss:      " << (config.trading.stop_loss_pct * 100) << "%\n";
        std::cout << "  Profit Target:  " << (config.trading.profit_target_pct * 100) << "%\n";
        std::cout << "  EOD Close:      3:58 PM (bar 388/391)\n";
        std::cout << "\n";

        // ====================================================================
        // WARMUP PHASE: Load and process historical data
        // ====================================================================

        std::cout << "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n";
        std::cout << "â•‘  PHASE 1: WARMUP (Historical Data)                        â•‘\n";
        std::cout << "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n";
        std::cout << "\n";

        std::cout << "Loading warmup data from " << config.data_dir << "...\n";
        std::cout << "  Warmup period: " << config.warmup_days << " day(s) = "
                  << config.warmup_bars << " bars\n\n";

        // Load historical data for all symbols
        auto warmup_data = DataLoader::load_from_directory(
            config.data_dir,
            config.symbols,
            config.extension
        );

        // Verify all symbols loaded
        for (const auto& symbol : config.symbols) {
            if (warmup_data.find(symbol) == warmup_data.end() || warmup_data[symbol].empty()) {
                std::cerr << "âŒ ERROR: Failed to load warmup data for " << symbol << "\n";
                std::cerr << "   Check data directory: " << config.data_dir << "\n";
                return 1;
            }
        }

        // Find minimum bars available
        size_t min_warmup_bars = std::numeric_limits<size_t>::max();
        for (const auto& [symbol, bars] : warmup_data) {
            min_warmup_bars = std::min(min_warmup_bars, bars.size());
        }

        std::cout << "Historical data loaded:\n";
        for (const auto& [symbol, bars] : warmup_data) {
            std::cout << "  " << symbol << ": " << bars.size() << " bars\n";
        }
        std::cout << "\n";

        if (min_warmup_bars < config.warmup_bars) {
            std::cerr << "âš ï¸  WARNING: Insufficient warmup data available\n";
            std::cerr << "   Available: " << min_warmup_bars << " bars\n";
            std::cerr << "   Requested: " << config.warmup_bars << " bars\n";
            std::cerr << "   Using:     " << min_warmup_bars << " bars for warmup\n\n";
            config.warmup_bars = min_warmup_bars;
        }

        // Initialize trader BEFORE warmup
        MultiSymbolTrader trader(config.symbols, config.trading);

        // Set min_bars_to_learn so trading starts AFTER warmup
        config.trading.min_bars_to_learn = config.warmup_bars + 1;

        // Process warmup bars (most recent N bars from historical data)
        std::cout << "Processing warmup bars (learning only, no trading)...\n";

        size_t warmup_start_idx = min_warmup_bars - config.warmup_bars;
        size_t bars_processed = 0;

        for (size_t i = warmup_start_idx; i < min_warmup_bars; ++i) {
            // Create market snapshot for this warmup bar
            std::unordered_map<Symbol, Bar> market_snapshot;
            for (const auto& symbol : config.symbols) {
                market_snapshot[symbol] = warmup_data[symbol][i];
            }

            // Process bar (models learn, but no trading)
            trader.on_bar(market_snapshot);
            bars_processed++;

            // Progress indicator
            if (bars_processed % 100 == 0 || bars_processed == config.warmup_bars) {
                std::cout << "  Processed " << bars_processed << "/" << config.warmup_bars
                         << " warmup bars...\r" << std::flush;
            }
        }

        std::cout << "\nâœ… Warmup complete! Models ready for live trading.\n";
        std::cout << "   EWRLS models initialized with " << config.warmup_bars << " bars of history\n";
        std::cout << "\n";

        // ====================================================================
        // PHASE 1.5: MID-DAY RESTART (Today's Bars Catch-Up & Position Reconciliation)
        // ====================================================================

        const std::string restart_data_path = "/tmp/restart_data.json";
        if (std::filesystem::exists(restart_data_path)) {
            std::cout << "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n";
            std::cout << "â•‘  PHASE 1.5: MID-DAY RESTART (Catch-Up & Reconciliation)   â•‘\n";
            std::cout << "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n";
            std::cout << "\n";

            try {
                // Load restart data JSON
                std::ifstream restart_file(restart_data_path);
                json restart_data = json::parse(restart_file);

                // ================================================================
                // Step 1: Process today's bars (9:30 AM â†’ NOW) for catch-up
                // ================================================================

                auto todays_bars_json = restart_data["todays_bars"];

                // Count total bars to process
                size_t total_catchup_bars = 0;
                for (const auto& symbol : config.symbols) {
                    if (todays_bars_json.contains(symbol)) {
                        total_catchup_bars = std::max(total_catchup_bars,
                                                     todays_bars_json[symbol].size());
                    }
                }

                if (total_catchup_bars > 0) {
                    std::cout << "ðŸ”„ WARMUP: Processing today's missing bars (OBSERVATION ONLY)...\n";
                    std::cout << "  Bars to process: " << total_catchup_bars
                             << " (9:30 AM â†’ NOW)\n";
                    std::cout << "  Mode: Warmup observation (no trades executed)\n";
                    std::cout << "  Purpose: Update EWRLS predictor with recent price movements\n\n";

                    // CRITICAL: Set trader to WARMUP_OBSERVATION phase (learns from data, no trades)
                    auto original_phase = config.trading.current_phase;
                    config.trading.current_phase = TradingConfig::WARMUP_OBSERVATION;

                    // Process each bar timestamp
                    for (size_t i = 0; i < total_catchup_bars; ++i) {
                        std::unordered_map<Symbol, Bar> market_snapshot;

                        // Build snapshot for all symbols at this timestamp
                        for (const auto& symbol : config.symbols) {
                            if (todays_bars_json.contains(symbol) &&
                                i < todays_bars_json[symbol].size()) {

                                auto& bar_json = todays_bars_json[symbol][i];

                                Bar bar;
                                bar.timestamp = std::chrono::system_clock::time_point(
                                    std::chrono::milliseconds(bar_json["timestamp_ms"].get<int64_t>())
                                );
                                bar.open = bar_json["open"].get<double>();
                                bar.high = bar_json["high"].get<double>();
                                bar.low = bar_json["low"].get<double>();
                                bar.close = bar_json["close"].get<double>();
                                bar.volume = bar_json["volume"].get<uint64_t>();
                                bar.bar_id = bar_json["bar_id"].get<uint64_t>();

                                market_snapshot[symbol] = bar;
                            }
                        }

                        // Only process if we have data for all symbols
                        // on_bar() will use handle_observation_phase() due to WARMUP_OBSERVATION mode
                        if (market_snapshot.size() == config.symbols.size()) {
                            trader.on_bar(market_snapshot);

                            // Progress indicator
                            if ((i + 1) % 10 == 0 || (i + 1) == total_catchup_bars) {
                                std::cout << "  Processed " << (i + 1) << "/"
                                         << total_catchup_bars << " catch-up bars...\r"
                                         << std::flush;
                            }
                        }
                    }

                    // Restore original phase (back to LIVE_TRADING for actual trading)
                    config.trading.current_phase = original_phase;

                    std::cout << "\nâœ… Warmup complete! EWRLS predictor caught up with "
                             << total_catchup_bars << " bars of today's price data.\n";
                    std::cout << "   (No trades executed - observation only)\n";
                    std::cout << "   Phase restored to LIVE_TRADING - ready to execute real trades\n\n";
                } else {
                    std::cout << "â„¹ï¸  No today's bars to catch up (restarting before market open)\n\n";
                }

                // ================================================================
                // Step 2: Reconcile existing Alpaca positions
                // ================================================================

                auto existing_positions = restart_data["existing_positions"];

                if (existing_positions.size() > 0) {
                    std::cout << "Reconciling existing positions...\n";
                    std::cout << "  Found " << existing_positions.size()
                             << " existing position(s) in Alpaca account:\n\n";

                    for (const auto& pos : existing_positions) {
                        std::string symbol = pos["symbol"].get<std::string>();
                        std::string side = pos["side"].get<std::string>();
                        double quantity = pos["quantity"].get<double>();
                        double entry_price = pos["entry_price"].get<double>();
                        double current_price = pos["current_price"].get<double>();
                        double unrealized_pl = pos["unrealized_pl"].get<double>();

                        std::cout << "    " << symbol << ":\n";
                        std::cout << "      Side:          " << side << "\n";
                        std::cout << "      Quantity:      " << std::fixed
                                 << std::setprecision(0) << quantity << " shares\n";
                        std::cout << "      Entry Price:   $" << std::setprecision(2)
                                 << entry_price << "\n";
                        std::cout << "      Current Price: $" << current_price << "\n";
                        std::cout << "      Unrealized P&L: $" << std::showpos
                                 << unrealized_pl << std::noshowpos << "\n\n";

                        // Add position to trader's internal position book
                        // NOTE: Trader will manage stop-loss and profit targets from this point
                        // Position reconciliation happens automatically via trader's internal state
                    }

                    std::cout << "âœ… Position reconciliation complete!\n";
                    std::cout << "   Trader will manage stop-loss and profit targets\n";
                    std::cout << "   EOD liquidation will close all positions at 3:58 PM\n\n";

                    // WARNING: The actual position reconciliation into trader's position book
                    // requires extending MultiSymbolTrader with a reconcile_position() method.
                    // For now, this logs the positions. The trader will see real positions
                    // via Alpaca API when making trading decisions.

                } else {
                    std::cout << "âœ… No existing positions to reconcile (clean slate)\n\n";
                }

                std::cout << "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n";
                std::cout << "â•‘  RESUMING LIVE TRADING FROM CURRENT TIME                   â•‘\n";
                std::cout << "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n";
                std::cout << "\n";

            } catch (json::parse_error& e) {
                std::cerr << "âš ï¸  WARNING: Failed to parse restart data: " << e.what() << "\n";
                std::cerr << "   Continuing with normal startup...\n\n";
            } catch (std::exception& e) {
                std::cerr << "âš ï¸  WARNING: Error during restart reconciliation: " << e.what() << "\n";
                std::cerr << "   Continuing with normal startup...\n\n";
            }
        }

        // ====================================================================
        // LIVE PHASE: Read from FIFO and trade in real-time
        // ====================================================================

        std::cout << "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n";
        std::cout << "â•‘  PHASE 2: LIVE TRADING (Real-Time Data)                   â•‘\n";
        std::cout << "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n";
        std::cout << "\n";

        // Open FIFO pipe for reading
        std::cout << "Opening FIFO pipe for live data...\n";
        std::cout << "  FIFO Path:     " << fifo_path << "\n";
        std::cout << "  Note: This will block until WebSocket bridge opens for writing\n";
        std::cout << "\n";

        // Try to open FIFO with retry logic
        std::ifstream fifo;
        int retry_count = 0;
        constexpr int max_retries = 3;

        while (retry_count < max_retries) {
            fifo.open(fifo_path);

            if (fifo.is_open()) {
                break;
            }

            retry_count++;
            std::cerr << "âš ï¸  WARNING: Failed to open FIFO (attempt " << retry_count << "/" << max_retries << ")\n";

            if (retry_count < max_retries) {
                std::cerr << "   Retrying in 2 seconds...\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }

        if (!fifo.is_open()) {
            std::cerr << "\n";
            std::cerr << "âŒ ERROR: Failed to open FIFO pipe after " << max_retries << " attempts\n";
            std::cerr << "   FIFO path: " << fifo_path << "\n";
            std::cerr << "\n";
            std::cerr << "Troubleshooting:\n";
            std::cerr << "  1. Check if WebSocket bridge is running:\n";
            std::cerr << "     ps aux | grep polygon_websocket\n";
            std::cerr << "  2. Check if FIFO exists:\n";
            std::cerr << "     ls -l " << fifo_path << "\n";
            std::cerr << "  3. Check bridge logs:\n";
            std::cerr << "     tail -50 logs/live/bridge_*.log\n";
            std::cerr << "\n";
            return 1;
        }

        std::cout << "âœ… Connected to WebSocket bridge\n";
        std::cout << "â³ Waiting for first bar from FIFO...\n";
        std::cout << "   (If stuck here, check: tail -f logs/live/bridge_*.log)\n";
        std::cout << "\n";

        // Market snapshot (accumulate bars for all 12 symbols)
        std::unordered_map<Symbol, Bar> market_snapshot;
        std::unordered_map<Symbol, Timestamp> last_bar_time;

        size_t bar_count = 0;
        size_t bars_received = 0;
        std::string line;
        bool first_bar_received = false;
        int64_t first_bar_timestamp_ms = 0;  // Track first bar's timestamp for session date

        // Track current minute for synchronization
        int64_t current_minute_ms = 0;

        // Track when we started accumulating bars for current minute (for timeout)
        auto minute_start_time = std::chrono::steady_clock::now();

        // Read bars from FIFO (blocks until WebSocket bridge sends data)
        while (std::getline(fifo, line)) {
            if (!first_bar_received) {
                first_bar_received = true;
                std::cout << "âœ… First bar received! Live trading active.\n\n";
            }
            if (line.empty()) continue;

            try {
                // Parse JSON bar from WebSocket bridge
                json bar_json = json::parse(line);

                std::string symbol = bar_json["symbol"];
                uint64_t bar_id = bar_json["bar_id"];
                int64_t timestamp_ms = bar_json["timestamp_ms"];

                // Capture first bar's timestamp for session date
                if (first_bar_timestamp_ms == 0) {
                    first_bar_timestamp_ms = timestamp_ms;
                }

                double open = bar_json["open"];
                double high = bar_json["high"];
                double low = bar_json["low"];
                double close = bar_json["close"];
                uint64_t volume = bar_json["volume"];

                // Convert timestamp (ms to seconds)
                Timestamp timestamp = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(timestamp_ms)
                );

                // Create bar
                Bar bar;
                bar.bar_id = bar_id;
                bar.timestamp = timestamp;
                bar.open = open;
                bar.high = high;
                bar.low = low;
                bar.close = close;
                bar.volume = volume;

                // Round timestamp to nearest minute for synchronization
                int64_t bar_minute = (timestamp_ms / 60000) * 60000;

                // If this bar is from a new minute, clear old snapshot
                if (current_minute_ms == 0) {
                    current_minute_ms = bar_minute;
                    minute_start_time = std::chrono::steady_clock::now();  // Reset timeout timer
                } else if (bar_minute != current_minute_ms) {
                    // Check if we should process the incomplete snapshot from previous minute BEFORE clearing
                    auto elapsed_time_prev = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - minute_start_time).count();

                    if (!market_snapshot.empty() && market_snapshot.size() >= 8) {
                        // Process incomplete snapshot from previous minute with timeout
                        std::cout << "â±ï¸  [TIMEOUT] Processing " << market_snapshot.size() << "/"
                                  << config.symbols.size() << " symbols after " << elapsed_time_prev << "s wait\n";
                        std::cout << "   Missing symbols: ";
                        for (const auto& sym : config.symbols) {
                            if (market_snapshot.find(sym) == market_snapshot.end()) {
                                std::cout << sym << " ";
                            }
                        }
                        std::cout << "\n";

                        // Process the incomplete snapshot
                        bar_count++;
                        trader.on_bar(market_snapshot);

                        // Show bar progress
                        auto current_results = trader.get_results();
                        double equity = trader.get_equity(market_snapshot);
                        double return_pct = (equity - config.capital) / config.capital * 100;

                        Timestamp reference_time = market_snapshot.begin()->second.timestamp;
                        auto time_t_val = std::chrono::system_clock::to_time_t(reference_time);
                        struct tm* timeinfo = localtime(&time_t_val);
                        char time_str[10];
                        strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);

                        std::cout << "[Bar " << bar_count << "] " << time_str
                                  << " | Equity: $" << std::fixed << std::setprecision(2) << equity
                                  << " (" << std::showpos << return_pct << std::noshowpos << "%) "
                                  << "| Trades: " << current_results.total_trades
                                  << " | Positions: " << trader.positions().size()
                                  << std::endl;
                    }

                    // New minute - clear old bars and start fresh
                    if (!market_snapshot.empty()) {
                        // Log transition
                        std::cout << "[MINUTE TRANSITION] Clearing " << market_snapshot.size()
                                  << " stale bars from previous minute\n";
                    }
                    market_snapshot.clear();
                    current_minute_ms = bar_minute;
                    minute_start_time = std::chrono::steady_clock::now();  // Reset timeout timer
                }

                // Update market snapshot
                market_snapshot[symbol] = bar;
                last_bar_time[symbol] = timestamp;
                bars_received++;

                // Check if we should process the current snapshot:
                // 1. We have all 12 symbols (ideal case), OR
                // 2. Timeout: we've been waiting >10s and have at least 8 symbols (67%)
                //
                // Rationale:
                // - Bars normally arrive within 1-2 seconds after minute closes
                // - If a bar doesn't arrive in 10s, it's not coming (no trades that minute)
                // - 8/12 symbols = 67% coverage, enough for rotation decisions
                // - Missing symbols are just excluded from that bar's analysis
                auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - minute_start_time).count();

                bool has_all_symbols = (market_snapshot.size() == config.symbols.size());
                bool timeout_reached = (elapsed_time > 10 && market_snapshot.size() >= 8);

                if (has_all_symbols || timeout_reached) {
                    // Log if we're processing due to timeout
                    if (timeout_reached && !has_all_symbols) {
                        std::cout << "â±ï¸  [TIMEOUT] Processing " << market_snapshot.size() << "/"
                                  << config.symbols.size() << " symbols after " << elapsed_time << "s wait\n";
                        std::cout << "   Missing symbols: ";
                        for (const auto& sym : config.symbols) {
                            if (market_snapshot.find(sym) == market_snapshot.end()) {
                                std::cout << sym << " ";
                            }
                        }
                        std::cout << "\n";
                    }

                    // Verify all bars are from the same minute (synchronized)
                    bool synchronized = true;
                    Timestamp reference_time = market_snapshot.begin()->second.timestamp;

                    for (const auto& [sym, bar] : market_snapshot) {
                        auto diff = std::abs(std::chrono::duration_cast<std::chrono::seconds>(
                            bar.timestamp - reference_time).count());
                        if (diff > 60) {  // Allow 60 second tolerance
                            synchronized = false;
                            break;
                        }
                    }

                    if (synchronized) {
                        bar_count++;

                        // Process synchronized bar
                        trader.on_bar(market_snapshot);

                        // Progress update
                        if (bar_count % 10 == 0 || bar_count < 5) {
                            auto current_results = trader.get_results();
                            double equity = trader.get_equity(market_snapshot);
                            double return_pct = (equity - config.capital) / config.capital * 100;

                            // Format timestamp
                            auto time_t_val = std::chrono::system_clock::to_time_t(reference_time);
                            struct tm* timeinfo = localtime(&time_t_val);
                            char time_str[10];
                            strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);

                            std::cout << "[Bar " << bar_count << "] " << time_str
                                      << " | Equity: $" << std::fixed << std::setprecision(2) << equity
                                      << " (" << std::showpos << return_pct << std::noshowpos << "%) "
                                      << "| Trades: " << current_results.total_trades
                                      << " | Positions: " << trader.positions().size()
                                      << std::endl;
                        }

                        // Check current bar time
                        auto time_t_val_check = std::chrono::system_clock::to_time_t(reference_time);
                        struct tm* timeinfo_check = localtime(&time_t_val_check);
                        int current_hour = timeinfo_check->tm_hour;
                        int current_min = timeinfo_check->tm_min;

                        // EOD: Stop accepting new positions at bar 388 (3:58 PM) or at 3:58 PM by time
                        if (bar_count == 388 || (current_hour == 15 && current_min >= 58)) {
                            std::cout << "\nâ° [3:58 PM] EOD LIQUIDATION - Closing all positions...\n";
                            std::cout << "   No new positions will be opened.\n\n";
                        }

                        // Market close at bar 391 (4:00 PM) OR if time is >= 4:00 PM
                        if (bar_count >= 391 || current_hour >= 16) {
                            std::cout << "\nâ° [4:00 PM] MARKET CLOSE - Trading session complete\n";
                            std::cout << "   (Bar " << bar_count << " at " << current_hour << ":"
                                      << std::setfill('0') << std::setw(2) << current_min << ")\n\n";
                            break;
                        }

                        // Clear snapshot for next minute
                        market_snapshot.clear();

                        // If we processed due to timeout, advance to next minute to avoid
                        // accumulating late bars from the same minute
                        if (timeout_reached && !has_all_symbols) {
                            current_minute_ms += 60000;  // Move to next minute
                            minute_start_time = std::chrono::steady_clock::now();
                        }
                    } else {
                        // Synchronization failed (bars >60s apart despite minute bucketing)
                        // This shouldn't happen often with minute bucketing, but handle it gracefully
                        std::cerr << "âš ï¸  WARNING: Synchronization failed despite minute bucketing\n";
                        std::cerr << "   Clearing snapshot and waiting for next complete minute\n";
                        market_snapshot.clear();
                        current_minute_ms = 0;  // Reset minute tracker
                    }
                }

            } catch (json::parse_error& e) {
                std::cerr << "âš ï¸  JSON parse error: " << e.what() << "\n";
                continue;
            } catch (std::exception& e) {
                std::cerr << "âš ï¸  Error processing bar: " << e.what() << "\n";
                continue;
            }
        }

        // Get final results
        auto results = trader.get_results();

        // Export results JSON
        std::string symbols_str;
        for (size_t i = 0; i < config.symbols.size(); ++i) {
            symbols_str += config.symbols[i];
            if (i < config.symbols.size() - 1) symbols_str += ",";
        }

        // Get trading session date from first bar's timestamp (works for both real & mock live mode)
        time_t time = static_cast<time_t>(first_bar_timestamp_ms / 1000);  // Convert ms to seconds
        struct tm* timeinfo = localtime(&time);
        char date_buffer[11];
        strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", timeinfo);
        std::string today(date_buffer);

        ResultsExporter::export_json(
            results, trader, config.results_file,
            symbols_str, "LIVE",
            today, today
        );

        // Export trades for dashboard
        export_trades_jsonl(trader, "trades.jsonl");

        std::cout << "âœ… Results exported to: " << config.results_file << "\n";
        std::cout << "âœ… Trades exported to: trades.jsonl\n\n";

        // Print summary
        std::cout << "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n";
        std::cout << "â•‘                 LIVE MODE Results                          â•‘\n";
        std::cout << "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n";
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
        std::cout << "  Profit Factor:      " << results.profit_factor << "\n";
        std::cout << "\n";

        std::cout << "Bars Processed:       " << bar_count << " (bars received: " << bars_received << ")\n";
        std::cout << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nâŒ Error: " << e.what() << "\n\n";
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

    // 1. Validate warmup period (minimum 1 day)
    if (config.warmup_days < 1) {
        std::cerr << "âŒ ERROR: Warmup period must be at least 1 day (got: "
                  << config.warmup_days << ")\n";
        std::cerr << "   Use --warmup-days N where N >= 1\n";
        return 1;
    }

    // 2. For MOCK mode, --date is optional (defaults to most recent date)
    // No validation needed since we'll auto-select the most recent date if not specified

    std::cout << "\n";
    std::cout << "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—\n";
    std::cout << "â•‘         Sentio Lite - Rotation Trading System             â•‘\n";
    std::cout << "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n";
    std::cout << "\n";

    // Print configuration
    std::cout << "Configuration:\n";
    std::cout << "  Mode: " << to_string(config.mode);
    if (config.mode == TradingMode::LIVE) {
        std::cout << " (âš ï¸  NOT YET IMPLEMENTED)";
    }
    std::cout << "\n";

    std::cout << "  Symbols (" << config.symbols.size() << "): ";
    for (size_t i = 0; i < config.symbols.size(); ++i) {
        std::cout << config.symbols[i];
        if (i < config.symbols.size() - 1) std::cout << ", ";
    }
    std::cout << "\n";

    std::cout << "  Warmup Period: " << config.warmup_days << " days ("
              << config.warmup_bars << " bars)\n";
    std::cout << "  Initial Capital: $" << std::fixed << std::setprecision(2)
              << config.capital << "\n";
    std::cout << "  Max Positions: " << config.trading.max_positions << "\n";
    std::cout << "  Stop Loss: " << (config.trading.stop_loss_pct * 100) << "%\n";
    std::cout << "  Profit Target: " << (config.trading.profit_target_pct * 100) << "%\n";

    if (config.generate_dashboard) {
        std::cout << "  Dashboard: Enabled\n";
    }
    std::cout << "\n";

    // Run appropriate mode
    if (config.mode == TradingMode::MOCK) {
        return run_mock_mode(config);
    } else {
        return run_live_mode(config);
    }
}

```

## ðŸ“„ **FILE 20 of 25**: src/predictor/ewrls_predictor.cpp

**File Information**:
- **Path**: `src/predictor/ewrls_predictor.cpp`
- **Size**: 228 lines
- **Modified**: 2025-10-17 14:33:58
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "predictor/ewrls_predictor.h"
#include <Eigen/Eigenvalues>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace sentio {

EWRLSPredictor::EWRLSPredictor(size_t n_features, double lambda)
    : EWRLSPredictor(n_features, Config{lambda}) {}

EWRLSPredictor::EWRLSPredictor(size_t n_features, const Config& config)
    : theta_(Eigen::VectorXd::Zero(n_features))
    , P_(Eigen::MatrixXd::Identity(n_features, n_features) * config.initial_variance)
    , config_(config)
    , n_features_(n_features)
    , updates_(0)
    , min_eigenvalue_(config.initial_variance)
    , max_eigenvalue_(config.initial_variance) {

    if (config_.lambda <= 0.0 || config_.lambda > 1.0) {
        throw std::invalid_argument("Lambda must be in (0, 1], got " + std::to_string(config_.lambda));
    }

    if (n_features == 0) {
        throw std::invalid_argument("Number of features must be > 0");
    }
}

double EWRLSPredictor::predict(const Eigen::VectorXd& features) const {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error(
            "Feature size mismatch: expected " + std::to_string(n_features_) +
            " but got " + std::to_string(features.size())
        );
    }
    return theta_.dot(features);
}

void EWRLSPredictor::update(const Eigen::VectorXd& features, double actual_return) {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error(
            "Feature size mismatch: expected " + std::to_string(n_features_) +
            " but got " + std::to_string(features.size())
        );
    }

    // Input validation - skip invalid inputs
    for (Eigen::Index i = 0; i < features.size(); ++i) {
        if (std::isnan(features(i)) || std::isinf(features(i))) {
            return;  // Skip this update
        }
    }

    if (std::isnan(actual_return) || std::isinf(actual_return)) {
        return;  // Skip invalid returns
    }

    // Clamp extreme returns to prevent numerical issues
    // Most single-bar returns should be < 100%
    actual_return = std::max(-1.0, std::min(1.0, actual_return));

    // EWRLS update with numerical stability
    double error = actual_return - predict(features);

    // Calculate gain vector with regularization
    Eigen::VectorXd Px = P_ * features;
    double denominator = config_.lambda + features.dot(Px);

    // Add small regularization to prevent division issues
    denominator += config_.regularization;

    if (denominator < 1e-10) {
        // Severe numerical issue - apply stronger regularization
        apply_regularization();
        return;
    }

    // Compute Kalman gain
    Eigen::VectorXd k = Px / denominator;

    // Update weights with gradient clipping
    Eigen::VectorXd weight_update = k * error;
    double update_norm = weight_update.norm();
    if (update_norm > config_.max_gradient_norm) {
        // Gradient clipping to prevent weight explosion
        weight_update = weight_update * (config_.max_gradient_norm / update_norm);
    }
    theta_ += weight_update;

    // Validate theta after update
    for (Eigen::Index i = 0; i < theta_.size(); ++i) {
        if (std::isnan(theta_(i)) || std::isinf(theta_(i))) {
            // Weight corruption detected - reset
            std::cerr << "Warning: Weight corruption detected, resetting EWRLS predictor" << std::endl;
            reset();
            return;
        }
    }

    // Update covariance matrix using Joseph form for better numerical stability
    // P_new = (I - k*x') * P * (I - k*x')' / lambda + k*k' * R
    // Simplified form: P = (P - k * x' * P) / lambda
    Eigen::MatrixXd P_new = (P_ - k * features.transpose() * P_) / config_.lambda;

    // Ensure symmetry (critical for numerical stability)
    P_ = (P_new + P_new.transpose()) / 2.0;

    // Validate P after update
    bool p_valid = true;
    for (Eigen::Index i = 0; i < P_.rows() && p_valid; ++i) {
        for (Eigen::Index j = 0; j < P_.cols() && p_valid; ++j) {
            if (std::isnan(P_(i, j)) || std::isinf(P_(i, j))) {
                p_valid = false;
            }
        }
    }

    if (!p_valid) {
        // Covariance matrix corruption - reset
        std::cerr << "Warning: Covariance matrix corruption detected, resetting" << std::endl;
        reset();
        return;
    }

    // Periodic stability check
    updates_++;
    if (updates_ % config_.stability_check_interval == 0) {
        ensure_numerical_stability();
    }
}

void EWRLSPredictor::ensure_numerical_stability() {
    // Update eigenvalue bounds for condition number
    update_eigenvalue_bounds();

    // Check condition number
    double condition_number = get_condition_number();

    if (condition_number > 1e6 || min_eigenvalue_ < config_.regularization) {
        // Matrix is ill-conditioned, apply regularization
        apply_regularization();

        // Log warning (but limit spam)
        static size_t warning_count = 0;
        if (warning_count < 10) {
            std::cerr << "Warning [" << warning_count << "]: EWRLS covariance matrix ill-conditioned. "
                     << "Condition number: " << condition_number
                     << ", min eigenvalue: " << min_eigenvalue_
                     << ". Applying regularization." << std::endl;
            warning_count++;
        }
    }

    // Prevent variance explosion
    double max_diagonal = P_.diagonal().maxCoeff();
    if (max_diagonal > config_.max_variance) {
        // Scale down the entire matrix to keep maximum variance bounded
        double scale = config_.max_variance / max_diagonal;
        P_ *= scale;

        static size_t scale_warning_count = 0;
        if (scale_warning_count < 5) {
            std::cerr << "Warning: Variance explosion detected (max=" << max_diagonal
                     << "), scaling P by " << scale << std::endl;
            scale_warning_count++;
        }
    }
}

void EWRLSPredictor::apply_regularization() {
    if (config_.use_adaptive_regularization) {
        // Adaptive regularization based on condition
        double reg_strength = config_.regularization;

        if (min_eigenvalue_ < 1e-6) {
            // Matrix is severely ill-conditioned, use stronger regularization
            reg_strength = 0.01;
        } else if (min_eigenvalue_ < 1e-4) {
            // Moderate ill-conditioning
            reg_strength = 0.001;
        }

        // Add regularization to diagonal (Ridge regression)
        P_ += Eigen::MatrixXd::Identity(n_features_, n_features_) * reg_strength;
    } else {
        // Fixed regularization
        P_ += Eigen::MatrixXd::Identity(n_features_, n_features_) * config_.regularization;
    }

    // Update eigenvalue bounds after regularization
    update_eigenvalue_bounds();
}

void EWRLSPredictor::update_eigenvalue_bounds() {
    // Compute eigenvalues to check condition
    // Note: This is O(n^3) but only called periodically
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(P_);

    if (solver.info() == Eigen::Success) {
        auto eigenvalues = solver.eigenvalues();
        min_eigenvalue_ = eigenvalues.minCoeff();
        max_eigenvalue_ = eigenvalues.maxCoeff();
    } else {
        // Eigenvalue computation failed - this is a serious issue
        std::cerr << "Error: Failed to compute eigenvalues, resetting predictor" << std::endl;
        reset();
    }
}

double EWRLSPredictor::get_condition_number() const {
    return max_eigenvalue_ / (min_eigenvalue_ + 1e-10);
}

bool EWRLSPredictor::is_numerically_stable() const {
    double cond = get_condition_number();
    return cond < 1e6 && min_eigenvalue_ > 1e-8;
}

void EWRLSPredictor::reset() {
    theta_.setZero();
    P_ = Eigen::MatrixXd::Identity(n_features_, n_features_) * config_.initial_variance;
    updates_ = 0;
    min_eigenvalue_ = config_.initial_variance;
    max_eigenvalue_ = config_.initial_variance;
}

} // namespace sentio

```

## ðŸ“„ **FILE 21 of 25**: src/predictor/feature_extractor.cpp

**File Information**:
- **Path**: `src/predictor/feature_extractor.cpp`
- **Size**: 586 lines
- **Modified**: 2025-10-21 14:02:15
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "predictor/feature_extractor.h"
#include "core/math_utils.h"
#include <algorithm>
#include <cmath>

namespace trading {

FeatureExtractor::FeatureExtractor()
    : history_(LOOKBACK), prev_close_(0.0), bar_count_(0) {}

std::optional<Eigen::VectorXd> FeatureExtractor::extract(const Bar& bar) {
    history_.push_back(bar);
    bar_count_++;

    // Need full lookback window for reliable features
    if (!is_ready()) {
        prev_close_ = bar.close;
        return std::nullopt;
    }

    // Get historical data
    auto prices = get_closes();
    auto volumes = get_volumes();
    auto bars = get_bars();

    Eigen::VectorXd features(NUM_FEATURES);
    int idx = 0;

    // ===== TIME FEATURES (0-7) =====
    // Cyclical encoding for intraday patterns (from online_trader)
    calculate_time_features(bar.timestamp, features, idx);

    // ===== MOMENTUM FEATURES (8-11) =====
    // Short-term to longer-term momentum
    features(idx++) = calculate_momentum(prices, 1);   // 1-bar return
    features(idx++) = calculate_momentum(prices, 3);   // 3-bar return
    features(idx++) = calculate_momentum(prices, 5);   // 5-bar return
    features(idx++) = calculate_momentum(prices, 10);  // 10-bar return

    // ===== VOLATILITY FEATURES (12-14) =====
    int vol_idx = idx;
    features(idx++) = calculate_volatility(prices, 10);  // 10-bar realized vol
    features(idx++) = calculate_volatility(prices, 20);  // 20-bar realized vol
    features(idx++) = calculate_atr(bars, 14);           // Average True Range

    // ===== VOLUME FEATURES (15-16) =====
    int vol_surge_idx = idx;
    features(idx++) = calculate_volume_surge(volumes);           // Recent vs average
    features(idx++) = calculate_relative_volume(volumes, 20);    // Normalized volume

    // ===== PRICE POSITION FEATURES (17-19) =====
    int price_pos_idx = idx;
    features(idx++) = calculate_price_position(bars, bar.close);    // Position in 50-bar range
    features(idx++) = calculate_channel_position(bars, 20);         // Position in 20-bar range
    features(idx++) = calculate_channel_position(bars, 10);         // Position in 10-bar range

    // ===== TREND STRENGTH FEATURES (20-22) =====
    int rsi_idx = idx;
    features(idx++) = calculate_rsi_like(prices, 14);              // 14-period RSI-like
    int dir_mom_idx = idx;
    features(idx++) = calculate_directional_momentum(prices, 10);  // Directional strength
    features(idx++) = calculate_directional_momentum(prices, 20);  // Longer-term direction

    // ===== INTERACTION TERMS (23-27) =====
    // These capture non-linear relationships
    features(idx++) = features(8) * features(vol_idx);       // 1-bar momentum * 10-bar volatility
    features(idx++) = features(10) * features(vol_idx);      // 5-bar momentum * volatility
    features(idx++) = features(11) * features(vol_surge_idx); // 10-bar momentum * volume surge
    features(idx++) = features(rsi_idx) * features(vol_idx); // RSI * volatility
    features(idx++) = features(price_pos_idx) * features(dir_mom_idx); // Price position * direction

    // ===== ACCELERATION FEATURES (28-30) =====
    // Rate of change of momentum
    features(idx++) = calculate_momentum(prices, 2) - calculate_momentum(prices, 5);
    features(idx++) = calculate_momentum(prices, 5) - calculate_momentum(prices, 10);
    features(idx++) = features(vol_idx) - features(vol_idx + 1);  // Vol change (10-bar vs 20-bar)

    // ===== DERIVED FEATURES (31) =====
    features(idx++) = std::log(1.0 + std::abs(features(11)));  // Log-scaled momentum (10-bar)

    // ===== MEAN REVERSION FEATURES (32-34) =====
    // Deviation from moving averages - critical for mean reversion trading
    // Positive = price above MA (expect reversion down), Negative = price below MA (expect reversion up)
    features(idx++) = calculate_ma_deviation(prices, 5);   // Short-term MA deviation (5 bars)
    features(idx++) = calculate_ma_deviation(prices, 10);  // Medium-term MA deviation (10 bars)
    features(idx++) = calculate_ma_deviation(prices, 20);  // Longer-term MA deviation (20 bars)

    // ===== BOLLINGER BANDS FEATURES (35-40) =====
    // Calculate Bollinger Bands (20-period, k=2)
    BollingerBands bb = calculate_bollinger_bands(prices, 20, 2.0);
    Price current_price = bar.close;

    // BB Mean Deviation: (close - bb_mean) / close
    features(idx++) = (current_price != 0.0) ? (current_price - bb.mean) / current_price : 0.0;

    // BB Standard Deviation %: bb_sd / close
    features(idx++) = (current_price != 0.0) ? bb.sd / current_price : 0.0;

    // BB Upper Deviation: (close - bb_upper) / close
    features(idx++) = (current_price != 0.0) ? (current_price - bb.upper) / current_price : 0.0;

    // BB Lower Deviation: (close - bb_lower) / close
    features(idx++) = (current_price != 0.0) ? (current_price - bb.lower) / current_price : 0.0;

    // BB %B: Position within bands (0-1)
    features(idx++) = bb.percent_b;

    // BB Bandwidth: Normalized band width
    features(idx++) = bb.bandwidth;

    // ===== BIAS TERM (41) =====
    features(idx++) = 1.0;  // Bias term (always 1.0)

    // ===== REGIME FEATURES (42-53) =====
    // Extract 12 regime-aware features using fast k-means clustering
    auto regime_features = regime_features_.extract(bars);
    for (size_t i = 0; i < RegimeFeatures::NUM_REGIME_FEATURES; ++i) {
        features(idx++) = regime_features(i);
    }

    prev_close_ = bar.close;
    return features;
}

void FeatureExtractor::calculate_time_features(Timestamp timestamp, Eigen::VectorXd& features, int& idx) const {
    // Convert timestamp to time_t (seconds since epoch)
    auto duration = timestamp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    time_t time = static_cast<time_t>(seconds);

    // Convert to GMT/UTC time structure
    struct tm* time_info = gmtime(&time);

    if (time_info) {
        double hour = time_info->tm_hour;           // 0-23
        double minute = time_info->tm_min;          // 0-59
        double day_of_week = time_info->tm_wday;    // 0-6 (Sunday=0)
        double day_of_month = time_info->tm_mday;   // 1-31

        // Cyclical encoding using sine/cosine to preserve continuity
        // E.g., 23:59 is close to 00:00
        constexpr double PI = 3.14159265358979323846;

        features(idx++) = std::sin(2.0 * PI * hour / 24.0);           // hour_sin
        features(idx++) = std::cos(2.0 * PI * hour / 24.0);           // hour_cos
        features(idx++) = std::sin(2.0 * PI * minute / 60.0);         // minute_sin
        features(idx++) = std::cos(2.0 * PI * minute / 60.0);         // minute_cos
        features(idx++) = std::sin(2.0 * PI * day_of_week / 7.0);     // dow_sin
        features(idx++) = std::cos(2.0 * PI * day_of_week / 7.0);     // dow_cos
        features(idx++) = std::sin(2.0 * PI * day_of_month / 31.0);   // dom_sin
        features(idx++) = std::cos(2.0 * PI * day_of_month / 31.0);   // dom_cos
    } else {
        // If time parsing fails, fill with zeros (neutral cyclical values)
        for (int i = 0; i < 8; ++i) {
            features(idx++) = 0.0;
        }
    }
}

double FeatureExtractor::calculate_momentum(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n <= static_cast<size_t>(period)) return 0.0;

    Price current = prices[n - 1];
    Price past = prices[n - 1 - period];

    if (past == 0 || std::abs(past) < 1e-10) return 0.0;
    return (current - past) / past;
}

double FeatureExtractor::calculate_volatility(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2 || static_cast<size_t>(period) > n) return 0.0;

    std::vector<double> returns;
    size_t start = n - period;
    for (size_t i = start + 1; i < n; ++i) {
        if (prices[i-1] != 0 && std::abs(prices[i-1]) > 1e-10) {
            returns.push_back((prices[i] - prices[i-1]) / prices[i-1]);
        }
    }

    if (returns.empty()) return 0.0;
    return MathUtils::stddev(returns);
}

double FeatureExtractor::calculate_atr(const std::vector<Bar>& bars, int period) const {
    size_t n = bars.size();
    if (n < 2 || static_cast<size_t>(period) > n) return 0.0;

    std::vector<double> true_ranges;
    size_t start = n - period;

    for (size_t i = start; i < n; ++i) {
        double high_low = bars[i].high - bars[i].low;
        double high_close = (i > 0) ? std::abs(bars[i].high - bars[i-1].close) : 0.0;
        double low_close = (i > 0) ? std::abs(bars[i].low - bars[i-1].close) : 0.0;
        double tr = std::max({high_low, high_close, low_close});
        true_ranges.push_back(tr);
    }

    if (true_ranges.empty()) return 0.0;

    // Normalize by current price
    Price current_price = bars[n-1].close;
    if (current_price == 0 || std::abs(current_price) < 1e-10) return 0.0;

    return MathUtils::mean(true_ranges) / current_price;
}

double FeatureExtractor::calculate_volume_surge(const std::vector<Volume>& volumes) const {
    if (volumes.empty()) return 1.0;

    // Compare recent volume (last 5 bars) to average
    size_t n = volumes.size();
    size_t recent_window = std::min(static_cast<size_t>(5), n);

    double recent_avg = 0.0;
    for (size_t i = n - recent_window; i < n; ++i) {
        recent_avg += static_cast<double>(volumes[i]);
    }
    recent_avg /= recent_window;

    double total_avg = 0.0;
    for (const auto& v : volumes) {
        total_avg += static_cast<double>(v);
    }
    total_avg /= volumes.size();

    if (total_avg == 0 || std::abs(total_avg) < 1e-10) return 1.0;
    return recent_avg / total_avg;
}

double FeatureExtractor::calculate_relative_volume(const std::vector<Volume>& volumes, int period) const {
    size_t n = volumes.size();
    if (n == 0) return 0.0;

    size_t window = std::min(static_cast<size_t>(period), n);
    double avg_volume = 0.0;

    for (size_t i = n - window; i < n; ++i) {
        avg_volume += static_cast<double>(volumes[i]);
    }
    avg_volume /= window;

    double current_volume = static_cast<double>(volumes[n-1]);

    if (avg_volume == 0 || std::abs(avg_volume) < 1e-10) return 0.0;
    return (current_volume - avg_volume) / avg_volume;
}

double FeatureExtractor::calculate_price_position(const std::vector<Bar>& bars,
                                                   Price current_price) const {
    if (bars.empty()) return 0.5;

    std::vector<double> highs, lows;
    for (const auto& bar : bars) {
        highs.push_back(bar.high);
        lows.push_back(bar.low);
    }

    double high_n = MathUtils::max(highs);
    double low_n = MathUtils::min(lows);
    double range = high_n - low_n;

    if (range < 1e-8) return 0.5;
    return (current_price - low_n) / range;
}

double FeatureExtractor::calculate_channel_position(const std::vector<Bar>& bars, int period) const {
    size_t n = bars.size();
    if (n == 0) return 0.5;

    size_t window = std::min(static_cast<size_t>(period), n);
    std::vector<double> highs, lows;

    for (size_t i = n - window; i < n; ++i) {
        highs.push_back(bars[i].high);
        lows.push_back(bars[i].low);
    }

    double high_n = MathUtils::max(highs);
    double low_n = MathUtils::min(lows);
    double range = high_n - low_n;

    Price current_price = bars[n-1].close;

    if (range < 1e-8) return 0.5;
    return (current_price - low_n) / range;
}

double FeatureExtractor::calculate_rsi_like(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2) return 0.5;

    size_t window = std::min(static_cast<size_t>(period), n - 1);
    std::vector<double> gains, losses;

    for (size_t i = n - window; i < n; ++i) {
        if (prices[i-1] != 0 && std::abs(prices[i-1]) > 1e-10) {
            double ret = (prices[i] - prices[i-1]) / prices[i-1];
            if (ret > 0) {
                gains.push_back(ret);
                losses.push_back(0.0);
            } else {
                gains.push_back(0.0);
                losses.push_back(-ret);
            }
        }
    }

    if (gains.empty()) return 0.5;

    double avg_gain = MathUtils::mean(gains);
    double avg_loss = MathUtils::mean(losses);

    if (avg_loss < 1e-8) return 1.0;
    if (avg_gain < 1e-8) return 0.0;

    // Normalize to [0, 1] range like RSI
    double rs = avg_gain / avg_loss;
    return rs / (1.0 + rs);
}

double FeatureExtractor::calculate_directional_momentum(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2 || static_cast<size_t>(period) >= n) return 0.0;

    int up_moves = 0;
    int down_moves = 0;
    size_t start = n - period - 1;

    for (size_t i = start + 1; i < n; ++i) {
        if (prices[i] > prices[i-1]) up_moves++;
        else if (prices[i] < prices[i-1]) down_moves++;
    }

    int total_moves = up_moves + down_moves;
    if (total_moves == 0) return 0.0;

    // Return net directional bias: +1 all up, -1 all down, 0 neutral
    return static_cast<double>(up_moves - down_moves) / total_moves;
}

double FeatureExtractor::calculate_ma_deviation(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n == 0 || static_cast<size_t>(period) > n) return 0.0;

    // Calculate simple moving average
    double sum = 0.0;
    for (int i = 0; i < period; ++i) {
        sum += prices[n - 1 - i];
    }
    double ma = sum / period;

    if (ma == 0 || std::abs(ma) < 1e-10) return 0.0;

    // Calculate normalized deviation: (price - MA) / MA
    // Positive: price above MA (overbought, expect reversion down)
    // Negative: price below MA (oversold, expect reversion up)
    Price current_price = prices[n - 1];
    return (current_price - ma) / ma;
}

double FeatureExtractor::calculate_z_score(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n == 0 || static_cast<size_t>(period) > n) return 0.0;

    // Calculate simple moving average
    double sum = 0.0;
    for (int i = 0; i < period; ++i) {
        sum += prices[n - 1 - i];
    }
    double ma = sum / period;

    // Calculate standard deviation
    double sum_sq = 0.0;
    for (int i = 0; i < period; ++i) {
        double diff = prices[n - 1 - i] - ma;
        sum_sq += diff * diff;
    }
    double stddev = std::sqrt(sum_sq / period);

    if (stddev < 1e-10) return 0.0;

    // Calculate standard z-score: (price - MA) / stddev
    // This measures how many standard deviations the price is from the mean
    Price current_price = prices[n - 1];
    return (current_price - ma) / stddev;
}

void FeatureExtractor::reset() {
    history_.clear();
    prev_close_ = 0.0;
    bar_count_ = 0;
    regime_features_.reset();
}

std::vector<Price> FeatureExtractor::get_closes() const {
    std::vector<Price> closes;
    closes.reserve(history_.size());
    for (size_t i = 0; i < history_.size(); ++i) {
        closes.push_back(history_[i].close);
    }
    return closes;
}

std::vector<Volume> FeatureExtractor::get_volumes() const {
    std::vector<Volume> volumes;
    volumes.reserve(history_.size());
    for (size_t i = 0; i < history_.size(); ++i) {
        volumes.push_back(history_[i].volume);
    }
    return volumes;
}

std::vector<Bar> FeatureExtractor::get_bars() const {
    return history_.to_vector();
}

FeatureExtractor::BollingerBands FeatureExtractor::calculate_bollinger_bands(
    const std::vector<Price>& prices, int period, double k) const {

    BollingerBands bb;

    if (prices.size() < static_cast<size_t>(period)) {
        return bb;  // Return default values if insufficient data
    }

    // Calculate mean (simple moving average)
    double sum = 0.0;
    size_t start_idx = prices.size() - period;
    for (size_t i = start_idx; i < prices.size(); ++i) {
        sum += prices[i];
    }
    bb.mean = sum / period;

    // Calculate standard deviation
    double sum_sq_diff = 0.0;
    for (size_t i = start_idx; i < prices.size(); ++i) {
        double diff = prices[i] - bb.mean;
        sum_sq_diff += diff * diff;
    }
    bb.sd = std::sqrt(sum_sq_diff / period);

    // Calculate upper and lower bands
    bb.upper = bb.mean + k * bb.sd;
    bb.lower = bb.mean - k * bb.sd;

    // Calculate %B: position within bands (0 = lower band, 1 = upper band)
    Price current_price = prices.back();
    double band_range = bb.upper - bb.lower;
    bb.percent_b = (band_range > 1e-8) ? (current_price - bb.lower) / band_range : 0.5;

    // Calculate bandwidth: normalized band width
    bb.bandwidth = (bb.mean > 1e-8) ? (bb.upper - bb.lower) / bb.mean : 0.0;

    return bb;
}

double FeatureExtractor::calculate_williams_r(const std::vector<Bar>& bars, int period) const {
    if (bars.size() < static_cast<size_t>(period)) {
        return -50.0;  // Return neutral value
    }

    // Get the last 'period' bars
    size_t start_idx = bars.size() - period;

    // Find highest high and lowest low
    Price highest_high = bars[start_idx].high;
    Price lowest_low = bars[start_idx].low;

    for (size_t i = start_idx; i < bars.size(); ++i) {
        if (bars[i].high > highest_high) highest_high = bars[i].high;
        if (bars[i].low < lowest_low) lowest_low = bars[i].low;
    }

    // Calculate Williams %R
    Price current_close = bars.back().close;
    double range = highest_high - lowest_low;

    // Williams %R = -100 * (Highest High - Close) / (Highest High - Lowest Low)
    // Range: -100 (oversold) to 0 (overbought)
    return (range > 1e-8) ? -100.0 * (highest_high - current_close) / range : -50.0;
}

std::vector<std::string> FeatureExtractor::get_feature_names() {
    return {
        // Time (0-7) - Cyclical encoding
        "hour_sin", "hour_cos", "minute_sin", "minute_cos",
        "dow_sin", "dow_cos", "dom_sin", "dom_cos",
        // Momentum (8-11)
        "momentum_1", "momentum_3", "momentum_5", "momentum_10",
        // Volatility (12-14)
        "volatility_10", "volatility_20", "atr_14",
        // Volume (15-16)
        "volume_surge", "relative_volume_20",
        // Price Position (17-19)
        "price_position_50", "channel_position_20", "channel_position_10",
        // Trend Strength (20-22)
        "rsi_14", "directional_momentum_10", "directional_momentum_20",
        // Interactions (23-27)
        "mom1_x_vol10", "mom5_x_vol10", "mom10_x_volsurge", "rsi_x_vol", "pricepos_x_direction",
        // Acceleration (28-30)
        "momentum_accel_short", "momentum_accel_long", "volatility_change",
        // Derived (31)
        "log_momentum",
        // Mean Reversion (32-34)
        "ma_dev_5", "ma_dev_10", "ma_dev_20",
        // Bollinger Bands (35-40)
        "bb20_mean_dev", "bb20_sd_pct", "bb20_upper_dev", "bb20_lower_dev", "bb20_percent_b", "bb20_bandwidth",
        // Bias (41)
        "bias"
    };
}

double FeatureExtractor::calculate_adx(const std::vector<Bar>& bars, int period) const {
    if (bars.size() < static_cast<size_t>(period + 1)) {
        return 0.0;  // Not enough data
    }

    // Step 1: Calculate directional movement (+DM, -DM) and True Range
    std::vector<double> plus_dm, minus_dm, tr;

    for (size_t i = 1; i < bars.size(); ++i) {
        // Plus Directional Movement: max(high[i] - high[i-1], 0)
        double pdm = std::max(bars[i].high - bars[i-1].high, 0.0);

        // Minus Directional Movement: max(low[i-1] - low[i], 0)
        double mdm = std::max(bars[i-1].low - bars[i].low, 0.0);

        // If both movements occur, only the larger one is used (the other set to 0)
        if (pdm > mdm) {
            mdm = 0.0;
        } else if (mdm > pdm) {
            pdm = 0.0;
        } else {
            // Equal movement (or both zero) - both set to zero
            pdm = 0.0;
            mdm = 0.0;
        }

        plus_dm.push_back(pdm);
        minus_dm.push_back(mdm);

        // True Range: max(high-low, |high-close_prev|, |low-close_prev|)
        double hl = bars[i].high - bars[i].low;
        double hc = std::abs(bars[i].high - bars[i-1].close);
        double lc = std::abs(bars[i].low - bars[i-1].close);
        tr.push_back(std::max({hl, hc, lc}));
    }

    // Step 2: Smooth +DM, -DM, TR using Wilder's smoothing (EMA-like)
    // Initial smooth value = sum of first 'period' values
    double smooth_pdm = 0.0, smooth_mdm = 0.0, smooth_tr = 0.0;

    for (int i = 0; i < period && i < static_cast<int>(plus_dm.size()); ++i) {
        smooth_pdm += plus_dm[i];
        smooth_mdm += minus_dm[i];
        smooth_tr += tr[i];
    }

    // Continue Wilder's smoothing: smooth[i] = smooth[i-1] - smooth[i-1]/period + current_value
    for (size_t i = period; i < plus_dm.size(); ++i) {
        smooth_pdm = smooth_pdm - smooth_pdm / period + plus_dm[i];
        smooth_mdm = smooth_mdm - smooth_mdm / period + minus_dm[i];
        smooth_tr = smooth_tr - smooth_tr / period + tr[i];
    }

    // Step 3: Calculate +DI and -DI
    double plus_di = (smooth_tr != 0.0) ? (smooth_pdm / smooth_tr) * 100.0 : 0.0;
    double minus_di = (smooth_tr != 0.0) ? (smooth_mdm / smooth_tr) * 100.0 : 0.0;

    // Step 4: Calculate DX (Directional Index)
    double di_sum = plus_di + minus_di;
    double dx = (di_sum != 0.0) ? (std::abs(plus_di - minus_di) / di_sum) * 100.0 : 0.0;

    // Step 5: For full ADX, we would smooth DX over 'period' bars
    // However, since this is called per-symbol and we don't maintain state,
    // we'll return DX as an approximation (single-point ADX)
    // For production, consider adding ADX state tracking per symbol

    return dx;  // Return DX as ADX approximation (0-100)
}

} // namespace trading

```

## ðŸ“„ **FILE 22 of 25**: src/predictor/multi_horizon_predictor.cpp

**File Information**:
- **Path**: `src/predictor/multi_horizon_predictor.cpp`
- **Size**: 176 lines
- **Modified**: 2025-10-17 14:43:13
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "predictor/multi_horizon_predictor.h"
#include "predictor/ewrls_predictor.h"
#include <cmath>
#include <algorithm>

namespace trading {

MultiHorizonPredictor::MultiHorizonPredictor(const std::string& symbol, const Config& config)
    : symbol_(symbol)
    , config_(config) {

    // Create EWRLS config for each horizon with appropriate lambda
    sentio::EWRLSPredictor::Config ewrls_config_1bar;
    ewrls_config_1bar.lambda = config_.lambda_1bar;
    ewrls_config_1bar.regularization = 1e-6;
    ewrls_config_1bar.use_adaptive_regularization = true;

    sentio::EWRLSPredictor::Config ewrls_config_5bar;
    ewrls_config_5bar.lambda = config_.lambda_5bar;
    ewrls_config_5bar.regularization = 1e-6;
    ewrls_config_5bar.use_adaptive_regularization = true;

    sentio::EWRLSPredictor::Config ewrls_config_10bar;
    ewrls_config_10bar.lambda = config_.lambda_10bar;
    ewrls_config_10bar.regularization = 1e-6;
    ewrls_config_10bar.use_adaptive_regularization = true;

    // Create predictors
    predictor_1bar_ = std::make_unique<OnlinePredictor>(
        OnlinePredictor::NUM_FEATURES, ewrls_config_1bar);
    predictor_5bar_ = std::make_unique<OnlinePredictor>(
        OnlinePredictor::NUM_FEATURES, ewrls_config_5bar);
    predictor_10bar_ = std::make_unique<OnlinePredictor>(
        OnlinePredictor::NUM_FEATURES, ewrls_config_10bar);

    // Initialize uncertainties
    prediction_errors_.fill(0.0);
    uncertainties_.fill(config_.initial_uncertainty);
}

MultiHorizonPredictor::MultiHorizonPrediction MultiHorizonPredictor::predict(
    const Eigen::VectorXd& features) {

    MultiHorizonPrediction result;

    // Get predictions from each horizon
    double pred_1 = predictor_1bar_->predict(features);
    double pred_5 = predictor_5bar_->predict(features);
    double pred_10 = predictor_10bar_->predict(features);

    // Calculate quality metrics
    result.pred_1bar = calculate_quality(pred_1, uncertainties_[0]);
    result.pred_5bar = calculate_quality(pred_5, uncertainties_[1]);
    result.pred_10bar = calculate_quality(pred_10, uncertainties_[2]);

    // Determine optimal horizon
    result.optimal_horizon = determine_optimal_horizon(result);

    // Set expected return and volatility based on optimal horizon
    if (result.optimal_horizon == 1) {
        result.expected_return = result.pred_1bar.prediction;
        result.expected_volatility = result.pred_1bar.uncertainty;
    } else if (result.optimal_horizon == 5) {
        result.expected_return = result.pred_5bar.prediction;
        result.expected_volatility = result.pred_5bar.uncertainty;
    } else {  // 10
        result.expected_return = result.pred_10bar.prediction;
        result.expected_volatility = result.pred_10bar.uncertainty;
    }

    return result;
}

void MultiHorizonPredictor::update(const Eigen::VectorXd& features,
                                   double return_1bar,
                                   double return_5bar,
                                   double return_10bar) {

    // Update 1-bar predictor (always available)
    if (std::isfinite(return_1bar)) {
        double pred_1 = predictor_1bar_->predict(features);
        double error_1 = return_1bar - pred_1;
        predictor_1bar_->update(features, return_1bar);
        update_uncertainty(0, error_1);
    }

    // Update 5-bar predictor (if 5 bars have passed)
    if (std::isfinite(return_5bar)) {
        double pred_5 = predictor_5bar_->predict(features);
        double error_5 = return_5bar - pred_5;
        predictor_5bar_->update(features, return_5bar);
        update_uncertainty(1, error_5);
    }

    // Update 10-bar predictor (if 10 bars have passed)
    if (std::isfinite(return_10bar)) {
        double pred_10 = predictor_10bar_->predict(features);
        double error_10 = return_10bar - pred_10;
        predictor_10bar_->update(features, return_10bar);
        update_uncertainty(2, error_10);
    }
}

void MultiHorizonPredictor::reset() {
    predictor_1bar_->reset();
    predictor_5bar_->reset();
    predictor_10bar_->reset();
    prediction_errors_.fill(0.0);
    uncertainties_.fill(config_.initial_uncertainty);
}

std::array<size_t, 3> MultiHorizonPredictor::update_counts() const {
    return {
        predictor_1bar_->update_count(),
        predictor_5bar_->update_count(),
        predictor_10bar_->update_count()
    };
}

MultiHorizonPredictor::PredictionQuality MultiHorizonPredictor::calculate_quality(
    double prediction, double uncertainty) const {

    PredictionQuality quality;
    quality.prediction = prediction;
    quality.uncertainty = std::max(uncertainty, 1e-6);  // Avoid division by zero

    // Z-score: standardized prediction
    quality.z_score = prediction / quality.uncertainty;

    // Signal-to-noise ratio
    quality.signal_to_noise = std::abs(prediction) / quality.uncertainty;

    // Confidence: sigmoid-like function based on signal-to-noise ratio
    // Maps [0, inf) -> [0, 1] with inflection around SNR = 2.0
    double snr_normalized = quality.signal_to_noise / 2.0;
    quality.confidence = snr_normalized / (1.0 + snr_normalized);

    return quality;
}

void MultiHorizonPredictor::update_uncertainty(int horizon_idx, double error) {
    // Exponentially weighted moving average of squared errors
    double squared_error = error * error;
    prediction_errors_[horizon_idx] = config_.uncertainty_decay * prediction_errors_[horizon_idx] +
                                      (1.0 - config_.uncertainty_decay) * squared_error;

    // Uncertainty is the square root of the EWMA of squared errors
    uncertainties_[horizon_idx] = std::sqrt(prediction_errors_[horizon_idx] +
                                           config_.initial_uncertainty * config_.initial_uncertainty);
}

int MultiHorizonPredictor::determine_optimal_horizon(const MultiHorizonPrediction& pred) const {
    // Calculate Sharpe-like ratio for each horizon
    // Return / uncertainty, adjusted for horizon length

    double sharpe_1 = pred.pred_1bar.prediction / (pred.pred_1bar.uncertainty + 1e-6);
    double sharpe_5 = pred.pred_5bar.prediction / (pred.pred_5bar.uncertainty * std::sqrt(5.0) + 1e-6);
    double sharpe_10 = pred.pred_10bar.prediction / (pred.pred_10bar.uncertainty * std::sqrt(10.0) + 1e-6);

    // Also consider signal quality
    double score_1 = sharpe_1 * pred.pred_1bar.confidence;
    double score_5 = sharpe_5 * pred.pred_5bar.confidence;
    double score_10 = sharpe_10 * pred.pred_10bar.confidence;

    // Prefer 5-bar horizon as default (good balance)
    // Only choose others if significantly better
    if (score_5 >= score_1 * 0.9 && score_5 >= score_10 * 0.9) {
        return 5;
    } else if (score_10 > score_1 && score_10 > score_5) {
        return 10;
    } else {
        return 1;
    }
}

} // namespace trading

```

## ðŸ“„ **FILE 23 of 25**: src/predictor/regime_features.cpp

**File Information**:
- **Path**: `src/predictor/regime_features.cpp`
- **Size**: 381 lines
- **Modified**: 2025-10-21 14:00:08
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "predictor/regime_features.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

namespace trading {

RegimeFeatures::RegimeFeatures() {
    reset();
}

void RegimeFeatures::reset() {
    bar_count_ = 0;
    last_hmm_state_ = -1;
    last_vol_regime_ = -1;
    hmm_state_duration_ = 0;
    vol_regime_duration_ = 0;
}

Eigen::VectorXd RegimeFeatures::extract(const std::vector<Bar>& bars) {
    Eigen::VectorXd features(NUM_REGIME_FEATURES);

    bar_count_ = bars.size();

    // Not enough data - return neutral features
    if (bars.size() < 30) {
        features << 0.33, 0.33, 0.34,  // HMM states
                    0.33, 0.33, 0.34,  // Vol regimes
                    0, 0,              // Durations
                    1.0, 0.0, 0.0, 0.0; // Microstructure
        return features;
    }

    // Extract prices and volumes
    std::vector<double> prices, volumes;
    for (const auto& bar : bars) {
        prices.push_back(bar.close);
        volumes.push_back(bar.volume);
    }

    // Calculate log returns
    std::vector<double> returns;
    for (size_t i = 1; i < prices.size(); ++i) {
        double ret = std::log(prices[i] / (prices[i-1] + 1e-10));
        returns.push_back(ret);
    }

    if (returns.empty()) {
        features << 0.33, 0.33, 0.34, 0.33, 0.33, 0.34, 0, 0, 1.0, 0.0, 0.0, 0.0;
        return features;
    }

    // ===================================================================
    // 1. HMM-like Market State Detection (3 features)
    // ===================================================================
    auto hmm_probs = detect_market_state(returns);

    // Track state duration
    int current_hmm_state = std::distance(hmm_probs.begin(),
                                         std::max_element(hmm_probs.begin(), hmm_probs.end()));
    if (current_hmm_state != last_hmm_state_) {
        hmm_state_duration_ = 0;
        last_hmm_state_ = current_hmm_state;
    } else {
        hmm_state_duration_++;
    }

    // ===================================================================
    // 2. Volatility Regime Detection (3 features)
    // ===================================================================
    auto vol_probs = detect_volatility_regime(returns);

    // Track vol regime duration
    int current_vol_regime = std::distance(vol_probs.begin(),
                                          std::max_element(vol_probs.begin(), vol_probs.end()));
    if (current_vol_regime != last_vol_regime_) {
        vol_regime_duration_ = 0;
        last_vol_regime_ = current_vol_regime;
    } else {
        vol_regime_duration_++;
    }

    // ===================================================================
    // 3. Microstructure Features (4 features)
    // ===================================================================

    // 3a. Volatility ratio (20-bar / 60-bar)
    auto rolling_vol = calculate_rolling_volatility(returns, 20);
    double vol_ratio = 1.0;
    if (rolling_vol.size() >= 60 && rolling_vol.back() > 1e-10) {
        double vol_20 = rolling_vol.back();
        double vol_60_avg = 0.0;
        for (size_t i = rolling_vol.size() - 60; i < rolling_vol.size(); ++i) {
            vol_60_avg += rolling_vol[i];
        }
        vol_60_avg /= 60.0;
        if (vol_60_avg > 1e-10) {
            vol_ratio = vol_20 / vol_60_avg;
        }
    }
    vol_ratio = std::clamp(vol_ratio, -3.0, 3.0);

    // 3b. Volatility z-score
    double vol_zscore = 0.0;
    if (rolling_vol.size() >= 60) {
        std::vector<double> recent_vol(rolling_vol.end() - 60, rolling_vol.end());
        vol_zscore = calculate_zscore(rolling_vol.back(), recent_vol);
    }
    vol_zscore = std::clamp(vol_zscore, -3.0, 3.0);

    // 3c. Price-volume correlation (20 bars)
    double price_vol_corr = 0.0;
    if (prices.size() >= 20 && volumes.size() >= 20) {
        std::vector<double> recent_prices(prices.end() - 20, prices.end());
        std::vector<double> recent_volumes(volumes.end() - 20, volumes.end());
        price_vol_corr = calculate_correlation(recent_prices, recent_volumes);
    }
    price_vol_corr = std::clamp(price_vol_corr, -1.0, 1.0);

    // 3d. Volume z-score
    double volume_zscore = 0.0;
    if (volumes.size() >= 60) {
        std::vector<double> recent_volumes(volumes.end() - 60, volumes.end());
        volume_zscore = calculate_zscore(volumes.back(), recent_volumes);
    }
    volume_zscore = std::clamp(volume_zscore, -3.0, 3.0);

    // Assemble feature vector
    features << hmm_probs[0], hmm_probs[1], hmm_probs[2],
                vol_probs[0], vol_probs[1], vol_probs[2],
                std::min(hmm_state_duration_, 120),
                std::min(vol_regime_duration_, 120),
                vol_ratio, vol_zscore, price_vol_corr, volume_zscore;

    return features;
}

std::array<double, 3> RegimeFeatures::detect_market_state(const std::vector<double>& returns) {
    if (returns.size() < 30) {
        return {0.33, 0.33, 0.34};
    }

    // Use k-means clustering on returns to detect 3 states
    auto clusters = kmeans_cluster(returns, 3);

    // Calculate soft probabilities for current return
    double current_return = returns.back();
    auto probs = calculate_cluster_probabilities(current_return, returns, clusters);

    return probs;
}

std::array<double, 3> RegimeFeatures::detect_volatility_regime(const std::vector<double>& returns) {
    if (returns.size() < 30) {
        return {0.33, 0.33, 0.34};
    }

    // Calculate rolling volatility
    auto rolling_vol = calculate_rolling_volatility(returns, 20);

    if (rolling_vol.size() < 30) {
        return {0.33, 0.33, 0.34};
    }

    // Use k-means clustering on volatility to detect 3 regimes
    auto clusters = kmeans_cluster(rolling_vol, 3);

    // Calculate soft probabilities for current volatility
    double current_vol = rolling_vol.back();
    auto probs = calculate_cluster_probabilities(current_vol, rolling_vol, clusters);

    return probs;
}

std::vector<double> RegimeFeatures::calculate_rolling_volatility(
    const std::vector<double>& returns, int window) {

    std::vector<double> rolling_vol;

    for (size_t i = window; i <= returns.size(); ++i) {
        // Calculate std dev of last 'window' returns
        double sum = 0.0;
        double sum_sq = 0.0;
        int count = 0;

        for (size_t j = i - window; j < i; ++j) {
            sum += returns[j];
            sum_sq += returns[j] * returns[j];
            count++;
        }

        double mean = sum / count;
        double variance = (sum_sq / count) - (mean * mean);
        double std_dev = std::sqrt(std::max(0.0, variance));

        rolling_vol.push_back(std_dev);
    }

    return rolling_vol;
}

double RegimeFeatures::calculate_correlation(
    const std::vector<double>& x,
    const std::vector<double>& y) {

    if (x.size() != y.size() || x.size() < 2) {
        return 0.0;
    }

    size_t n = x.size();

    // Calculate means
    double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / n;
    double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / n;

    // Calculate covariance and standard deviations
    double cov = 0.0, var_x = 0.0, var_y = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;
        cov += dx * dy;
        var_x += dx * dx;
        var_y += dy * dy;
    }

    double std_x = std::sqrt(var_x / n);
    double std_y = std::sqrt(var_y / n);

    if (std_x < 1e-10 || std_y < 1e-10) {
        return 0.0;
    }

    return cov / (n * std_x * std_y);
}

double RegimeFeatures::calculate_zscore(double value, const std::vector<double>& history) {
    if (history.size() < 2) {
        return 0.0;
    }

    double mean = std::accumulate(history.begin(), history.end(), 0.0) / history.size();

    double sum_sq_diff = 0.0;
    for (double v : history) {
        double diff = v - mean;
        sum_sq_diff += diff * diff;
    }
    double std_dev = std::sqrt(sum_sq_diff / history.size());

    if (std_dev < 1e-10) {
        return 0.0;
    }

    return (value - mean) / std_dev;
}

std::vector<int> RegimeFeatures::kmeans_cluster(const std::vector<double>& data, int k) {
    if (data.size() < static_cast<size_t>(k)) {
        return std::vector<int>(data.size(), 0);
    }

    // Initialize centroids using quantiles
    std::vector<double> sorted_data = data;
    std::sort(sorted_data.begin(), sorted_data.end());

    std::vector<double> centroids(k);
    for (int i = 0; i < k; ++i) {
        size_t idx = (sorted_data.size() * (i + 1)) / (k + 1);
        centroids[i] = sorted_data[std::min(idx, sorted_data.size() - 1)];
    }

    // Run k-means for 10 iterations (fast convergence)
    std::vector<int> assignments(data.size());

    for (int iter = 0; iter < 10; ++iter) {
        // Assignment step
        for (size_t i = 0; i < data.size(); ++i) {
            double min_dist = std::numeric_limits<double>::max();
            int best_cluster = 0;

            for (int c = 0; c < k; ++c) {
                double dist = std::abs(data[i] - centroids[c]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = c;
                }
            }

            assignments[i] = best_cluster;
        }

        // Update step
        std::vector<double> new_centroids(k, 0.0);
        std::vector<int> counts(k, 0);

        for (size_t i = 0; i < data.size(); ++i) {
            int c = assignments[i];
            new_centroids[c] += data[i];
            counts[c]++;
        }

        for (int c = 0; c < k; ++c) {
            if (counts[c] > 0) {
                centroids[c] = new_centroids[c] / counts[c];
            }
        }
    }

    return assignments;
}

std::array<double, 3> RegimeFeatures::calculate_cluster_probabilities(
    double value,
    const std::vector<double>& data,
    const std::vector<int>& clusters) {

    // Calculate cluster centers
    std::vector<double> centroids(3, 0.0);
    std::vector<int> counts(3, 0);

    for (size_t i = 0; i < data.size(); ++i) {
        int c = clusters[i];
        centroids[c] += data[i];
        counts[c]++;
    }

    for (int c = 0; c < 3; ++c) {
        if (counts[c] > 0) {
            centroids[c] /= counts[c];
        }
    }

    // Calculate distances to each cluster center
    std::array<double, 3> distances;
    for (int c = 0; c < 3; ++c) {
        distances[c] = std::abs(value - centroids[c]);
    }

    // Convert distances to probabilities (inverse distance weighting)
    // Use softmax-like transformation
    std::array<double, 3> probs;
    double sum_exp = 0.0;
    constexpr double temperature = 0.1;  // Controls sharpness of probabilities

    for (int c = 0; c < 3; ++c) {
        probs[c] = std::exp(-distances[c] / (temperature + 1e-10));
        sum_exp += probs[c];
    }

    // Normalize
    if (sum_exp > 1e-10) {
        for (int c = 0; c < 3; ++c) {
            probs[c] /= sum_exp;
        }
    } else {
        probs = {0.33, 0.33, 0.34};
    }

    return probs;
}

std::vector<std::string> RegimeFeatures::get_feature_names() {
    return {
        "hmm_state_0_prob",
        "hmm_state_1_prob",
        "hmm_state_2_prob",
        "gmm_vol_low_prob",
        "gmm_vol_med_prob",
        "gmm_vol_high_prob",
        "hmm_state_duration",
        "vol_regime_duration",
        "vol_ratio_20_60",
        "vol_zscore",
        "price_vol_correlation",
        "volume_zscore"
    };
}

} // namespace trading

```

## ðŸ“„ **FILE 24 of 25**: src/trading/multi_symbol_trader.cpp

**File Information**:
- **Path**: `src/trading/multi_symbol_trader.cpp`
- **Size**: 1898 lines
- **Modified**: 2025-10-20 06:50:08
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "trading/multi_symbol_trader.h"
#include "core/bar_id_utils.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cmath>
#include <ctime>
#include <map>

/**
 * Calculate simple moving average from price history
 * @param history Price history (most recent price at back)
 * @param period Number of bars for MA calculation
 * @return Moving average, or NaN if insufficient data
 */
static double calculate_moving_average(const std::deque<double>& history, int period) {
    if (static_cast<int>(history.size()) < period) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double sum = 0.0;
    // Sum the last 'period' prices
    for (int i = 0; i < period; ++i) {
        sum += history[history.size() - 1 - i];
    }

    return sum / period;
}

namespace trading {

// Helper function: Check if bar timestamp indicates end of trading day
// Returns true if timestamp is at or after 3:59 PM ET (market close at 4:00 PM)
static bool is_end_of_day(Timestamp timestamp) {
    auto time_seconds = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    time_t time = static_cast<time_t>(time_seconds);
    struct tm* tm_info = localtime(&time);

    // Market close is 16:00 (4:00 PM)
    // Trigger EOD at last minute (15:59 or 16:00)
    return (tm_info->tm_hour == 15 && tm_info->tm_min >= 59) ||
           (tm_info->tm_hour >= 16);
}

// Helper function: Extract date from timestamp (YYYYMMDD format)
static int64_t extract_date_from_timestamp(Timestamp timestamp) {
    auto time_seconds = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    time_t time = static_cast<time_t>(time_seconds);
    struct tm* tm_info = localtime(&time);

    return (tm_info->tm_year + 1900) * 10000 +
           (tm_info->tm_mon + 1) * 100 +
           tm_info->tm_mday;
}

MultiSymbolTrader::MultiSymbolTrader(const std::vector<Symbol>& symbols,
                                     const TradingConfig& config)
    : symbols_(symbols),
      config_(config),
      cash_(config.initial_capital),
      bars_seen_(0),
      trading_bars_(0),
      total_trades_(0),
      total_transaction_costs_(0.0),
      daily_start_equity_(config.initial_capital),
      daily_start_trades_(0),
      daily_winning_trades_(0),
      daily_losing_trades_(0),
      equity_high_water_mark_(config.initial_capital),
      max_drawdown_observed_(0.0) {

    // Initialize trade filter
    trade_filter_ = std::make_unique<TradeFilter>(config_.filter_config);

    // Initialize per-symbol components
    for (const auto& symbol : symbols_) {
        // Multi-horizon predictor (1, 5, 10 bars ahead)
        predictors_[symbol] = std::make_unique<MultiHorizonPredictor>(
            symbol, config_.horizon_config);

        // Dual EWRLS: Separate predictors for above/below MA scenarios
        if (config_.enable_dual_ewrls) {
            predictors_above_ma_[symbol] = std::make_unique<MultiHorizonPredictor>(
                symbol, config_.horizon_config);
            predictors_below_ma_[symbol] = std::make_unique<MultiHorizonPredictor>(
                symbol, config_.horizon_config);
        }

        // Feature extractor with 50-bar lookback
        extractors_[symbol] = std::make_unique<FeatureExtractor>();

        // Trade history for adaptive sizing
        trade_history_[symbol] = std::make_unique<TradeHistory>(config_.trade_history_size);

        // Initialize market context with defaults
        market_context_[symbol] = MarketContext(
            config_.default_avg_volume,
            config_.default_volatility,
            30  // Default 30 minutes from open
        );

        // Initialize price history for multi-bar return calculations
        price_history_[symbol] = std::deque<double>();
    }
}

void MultiSymbolTrader::on_bar(const std::unordered_map<Symbol, Bar>& market_data) {
    bars_seen_++;

    // Step 0: COMPREHENSIVE BarID Validation - Ensure all symbols are synchronized
    int64_t reference_timestamp_ms = -1;
    std::string reference_symbol;
    std::vector<std::string> missing_symbols;
    std::vector<std::string> validated_symbols;

    // Check 1: Verify all expected symbols are present
    for (const auto& symbol : symbols_) {
        if (market_data.find(symbol) == market_data.end()) {
            missing_symbols.push_back(symbol);
        }
    }

    if (!missing_symbols.empty()) {
        std::cerr << "  [WARNING] Bar " << bars_seen_ << ": Missing symbols: ";
        for (const auto& sym : missing_symbols) std::cerr << sym << " ";
        std::cerr << std::endl;
    }

    // Check 2: Validate bar_id synchronization across all symbols
    for (const auto& [symbol, bar] : market_data) {
        // Check 2a: Valid bar_id
        if (bar.bar_id == 0) {
            throw std::runtime_error(
                "CRITICAL: Symbol " + symbol + " has invalid bar_id (0) at bar " +
                std::to_string(bars_seen_) + ". Data integrity compromised!"
            );
        }

        // Check 2b: Extract timestamp from bar_id
        int64_t bar_id_timestamp_ms = extract_timestamp_ms(bar.bar_id);

        // Check 2c: Verify bar_id timestamp matches bar's actual timestamp
        int64_t bar_timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            bar.timestamp.time_since_epoch()).count();

        if (bar_id_timestamp_ms != bar_timestamp_ms) {
            throw std::runtime_error(
                "CRITICAL: Symbol " + symbol + " bar_id timestamp (" +
                std::to_string(bar_id_timestamp_ms) + ") doesn't match bar.timestamp (" +
                std::to_string(bar_timestamp_ms) + ") at bar " + std::to_string(bars_seen_) +
                ". This indicates corrupted bar_id generation!"
            );
        }

        // Check 2d: Cross-symbol timestamp synchronization
        if (reference_timestamp_ms == -1) {
            // First valid bar - use as reference
            reference_timestamp_ms = bar_id_timestamp_ms;
            reference_symbol = symbol;
        } else {
            // Verify timestamp matches reference (all symbols must be at same time)
            if (bar_id_timestamp_ms != reference_timestamp_ms) {
                throw std::runtime_error(
                    "CRITICAL: Bar timestamp mismatch! " + reference_symbol +
                    " has timestamp " + std::to_string(reference_timestamp_ms) +
                    " but " + symbol + " has timestamp " + std::to_string(bar_id_timestamp_ms) +
                    " at bar " + std::to_string(bars_seen_) +
                    ". This indicates bar misalignment across symbols - CANNOT TRADE SAFELY!"
                );
            }
        }

        validated_symbols.push_back(symbol);
    }

    // Check 3: Verify bar sequence (detect time gaps)
    static int64_t last_timestamp_ms = -1;
    if (last_timestamp_ms != -1 && reference_timestamp_ms != -1) {
        int64_t time_gap_ms = reference_timestamp_ms - last_timestamp_ms;
        // Expect 1-minute bars (60000ms), warn if gap > 5 minutes
        if (time_gap_ms > 300000) {
            std::cerr << "  [WARNING] Large time gap detected: "
                     << (time_gap_ms / 60000) << " minutes between bars "
                     << (bars_seen_ - 1) << " and " << bars_seen_ << std::endl;
        }
    }
    last_timestamp_ms = reference_timestamp_ms;

    // Validation passed - log periodically for confidence
    if (bars_seen_ % 100 == 0) {
        std::cout << "  [SYNC-CHECK] Bar " << bars_seen_
                 << ": All " << validated_symbols.size() << " symbols synchronized at timestamp "
                 << reference_timestamp_ms << std::endl;
    }

    // Step 1: Update market context for cost calculations
    for (const auto& symbol : symbols_) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            update_market_context(symbol, it->second);
        }
    }

    // Step 2: Update price history for multi-bar return calculations
    for (const auto& symbol : symbols_) {
        auto it = market_data.find(symbol);
        if (it == market_data.end()) continue;

        const Bar& bar = it->second;
        auto& history = price_history_[symbol];
        history.push_back(bar.close);

        // Keep only last 20 bars (enough for 10-bar returns with buffer)
        while (history.size() > 20) {
            history.pop_front();
        }
    }

    // Step 3: Extract features and make multi-horizon predictions
    std::unordered_map<Symbol, PredictionData> predictions;

    for (const auto& symbol : symbols_) {
        auto it = market_data.find(symbol);
        if (it == market_data.end()) continue;

        const Bar& bar = it->second;
        auto features = extractors_[symbol]->extract(bar);

        if (features.has_value()) {
            // Make multi-horizon prediction (dual EWRLS or standard)
            MultiHorizonPredictor::MultiHorizonPrediction pred;

            if (config_.enable_dual_ewrls) {
                // Dual EWRLS: Use appropriate model based on price position relative to MA
                auto& history = price_history_[symbol];
                if (history.size() >= static_cast<size_t>(config_.dual_ewrls_ma_period)) {
                    double ma = calculate_moving_average(history, config_.dual_ewrls_ma_period);
                    double deviation = (bar.close - ma) / ma;

                    // Use appropriate predictor based on current state
                    if (std::abs(deviation) > config_.dual_ewrls_min_deviation) {
                        if (bar.close > ma) {
                            pred = predictors_above_ma_[symbol]->predict(features.value());
                        } else {
                            pred = predictors_below_ma_[symbol]->predict(features.value());
                        }
                    } else {
                        // Near mean - use standard predictor
                        pred = predictors_[symbol]->predict(features.value());
                    }
                } else {
                    // Insufficient history for dual model - use standard
                    pred = predictors_[symbol]->predict(features.value());
                }
            } else {
                // Standard single predictor
                pred = predictors_[symbol]->predict(features.value());
            }

            predictions[symbol] = {pred, features.value(), bar.close};

            // Update predictor with realized returns or mean reversion targets
            if (bars_seen_ > 1) {
                auto& history = price_history_[symbol];

                double target_1bar = std::numeric_limits<double>::quiet_NaN();
                double target_5bar = std::numeric_limits<double>::quiet_NaN();
                double target_10bar = std::numeric_limits<double>::quiet_NaN();

                if (config_.enable_mean_reversion_predictor) {
                    // MEAN REVERSION MODE: Only learn from meaningful deviations (expert fix)
                    //
                    // Key insight: The predictor should learn the PATTERN between deviation and returns
                    // But only train when there's significant deviation from MA (not noise)
                    // This helps the predictor focus on real mean reversion setups

                    // 1-bar target: Short-term mean reversion
                    if (history.size() >= 2) {
                        double prev_price = history[history.size() - 2];
                        double ma = calculate_moving_average(history, config_.ma_period_1bar);
                        if (prev_price > 0 && !std::isnan(ma) && ma > 0) {
                            // Calculate deviation from MA
                            double deviation = (prev_price - ma) / ma;

                            // EXPERT FIX: Only train when deviation is meaningful (> 0.1% = 10 bps)
                            // This prevents learning from random noise around the mean
                            if (std::abs(deviation) > 0.001) {
                                // Actual return from t-1 to t
                                double actual_return = (bar.close - prev_price) / prev_price;
                                target_1bar = actual_return;
                                // Predictor will learn: when deviation is X%, return tends to be Y%
                            }
                        }
                    }

                    // 5-bar target: Medium-term mean reversion
                    if (history.size() >= 6) {
                        double price_5bars_ago = history[history.size() - 6];
                        double ma = calculate_moving_average(history, config_.ma_period_5bar);
                        if (price_5bars_ago > 0 && !std::isnan(ma) && ma > 0) {
                            double deviation = (price_5bars_ago - ma) / ma;
                            // Only train on meaningful deviations
                            if (std::abs(deviation) > 0.001) {
                                double actual_return = (bar.close - price_5bars_ago) / price_5bars_ago;
                                target_5bar = actual_return;
                            }
                        }
                    }

                    // 10-bar target: Longer-term mean reversion
                    if (history.size() >= 11) {
                        double price_10bars_ago = history[history.size() - 11];
                        double ma = calculate_moving_average(history, config_.ma_period_10bar);
                        if (price_10bars_ago > 0 && !std::isnan(ma) && ma > 0) {
                            double deviation = (price_10bars_ago - ma) / ma;
                            // Only train on meaningful deviations
                            if (std::abs(deviation) > 0.001) {
                                double actual_return = (bar.close - price_10bars_ago) / price_10bars_ago;
                                target_10bar = actual_return;
                            }
                        }
                    }
                } else {
                    // RAW RETURN MODE: Original behavior (predict simple returns)

                    // Calculate 1-bar return
                    if (history.size() >= 2) {
                        double prev_price = history[history.size() - 2];
                        if (prev_price > 0) {
                            target_1bar = (bar.close - prev_price) / prev_price;
                        }
                    }

                    // Calculate 5-bar return
                    if (history.size() >= 6) {
                        double price_5bars_ago = history[history.size() - 6];
                        if (price_5bars_ago > 0) {
                            target_5bar = (bar.close - price_5bars_ago) / price_5bars_ago;
                        }
                    }

                    // Calculate 10-bar return
                    if (history.size() >= 11) {
                        double price_10bars_ago = history[history.size() - 11];
                        if (price_10bars_ago > 0) {
                            target_10bar = (bar.close - price_10bars_ago) / price_10bars_ago;
                        }
                    }
                }

                // Update multi-horizon predictor with targets (dual EWRLS or standard)
                if (config_.enable_dual_ewrls) {
                    // Dual EWRLS: Train only the model that matches current price position
                    auto& history = price_history_[symbol];
                    if (history.size() >= static_cast<size_t>(config_.dual_ewrls_ma_period) + 1) {
                        // Use PREVIOUS price to determine which model to train
                        double prev_price = history[history.size() - 2];
                        double ma = calculate_moving_average(history, config_.dual_ewrls_ma_period);
                        double deviation = (prev_price - ma) / ma;

                        // Train the appropriate model based on price position
                        if (std::abs(deviation) > config_.dual_ewrls_min_deviation) {
                            if (prev_price > ma) {
                                // Train "above MA" model
                                predictors_above_ma_[symbol]->update(features.value(), target_1bar, target_5bar, target_10bar);
                            } else {
                                // Train "below MA" model
                                predictors_below_ma_[symbol]->update(features.value(), target_1bar, target_5bar, target_10bar);
                            }
                        }
                        // Also train standard predictor (always)
                        predictors_[symbol]->update(features.value(), target_1bar, target_5bar, target_10bar);
                    } else {
                        // Insufficient history - train standard predictor only
                        predictors_[symbol]->update(features.value(), target_1bar, target_5bar, target_10bar);
                    }
                } else {
                    // Standard single predictor
                    predictors_[symbol]->update(features.value(), target_1bar, target_5bar, target_10bar);
                }
            }
        }
    }

    // Step 4: Update trade filter bars held counter
    trade_filter_->update_bars_held(static_cast<int>(bars_seen_));

    // Step 5: Update existing positions (check exit conditions with trade filter)
    update_positions(market_data, predictions);

    // Step 6: Update warmup phase and execute phase-specific logic
    update_phase();

    // Step 6b: Update rotation cooldowns (from online_trader)
    update_rotation_cooldowns();

    switch(config_.current_phase) {
        case TradingConfig::WARMUP_OBSERVATION:
            handle_observation_phase(market_data);
            break;

        case TradingConfig::WARMUP_SIMULATION:
            handle_simulation_phase(predictions, market_data);
            break;

        case TradingConfig::WARMUP_COMPLETE:
        case TradingConfig::LIVE_TRADING:
            handle_live_phase(predictions, market_data);
            break;
    }

    // Step 7: EOD liquidation (use timestamp-based detection)
    // Detect end of day based on bar timestamp (3:59-4:00 PM ET)
    // This is more robust than modulo arithmetic which fails with missing bars
    static int64_t last_trading_date = 0;
    static int64_t last_eod_date = 0;  // Track last EOD to prevent duplicate triggers
    int64_t current_trading_date = extract_date_from_timestamp(
        market_data.begin()->second.timestamp);
    bool is_eod = is_end_of_day(market_data.begin()->second.timestamp);

    // Only trigger EOD once per day (when we first see EOD timestamp)
    bool should_trigger_eod = is_eod && (current_trading_date != last_eod_date);

    if (config_.eod_liquidation && trading_bars_ > 0 && should_trigger_eod) {
        int day_num = trading_bars_ / config_.bars_per_day;
        last_eod_date = current_trading_date;  // Mark this day as processed

        // Log day boundary transition
        std::cout << "\n[DAY BOUNDARY] Transitioning to day " << day_num << " â†’ "
                  << (day_num + 1) << " (bar " << bars_seen_ << ")\n";

        // Log position states before EOD liquidation
        std::cout << "  [POSITION STATES BEFORE EOD]:\n";
        for (const auto& symbol : symbols_) {
            const auto& state = trade_filter_->get_position_state(symbol);
            std::cout << "    " << symbol << ": "
                      << (state.has_position ? "HOLDING" : "FLAT")
                      << " | last_exit_bar: " << state.last_exit_bar
                      << " | bars_held: " << state.bars_held << "\n";
        }

        // Liquidate all positions
        liquidate_all(market_data, "EOD");

        // Calculate end-of-day equity
        double end_equity = get_equity(market_data);

        // Calculate daily return
        double daily_return = (daily_start_equity_ > 0) ?
                             (end_equity - daily_start_equity_) / daily_start_equity_ : 0.0;

        // Store daily results
        DailyResults daily;
        daily.day_number = day_num;
        daily.start_equity = daily_start_equity_;
        daily.end_equity = end_equity;
        daily.daily_return = daily_return;
        daily.trades_today = total_trades_ - daily_start_trades_;
        daily.winning_trades_today = daily_winning_trades_;
        daily.losing_trades_today = daily_losing_trades_;
        daily_results_.push_back(daily);

        // Print daily summary
        std::cout << "  [EOD] Day " << day_num << " complete:"
                  << " Equity: $" << std::fixed << std::setprecision(2) << end_equity
                  << " (" << std::showpos << (daily_return * 100) << std::noshowpos << "%)"
                  << " | Trades: " << daily.trades_today
                  << " (W:" << daily.winning_trades_today
                  << " L:" << daily.losing_trades_today << ")\n";

        // Reset daily counters for next day
        daily_start_equity_ = end_equity;
        daily_start_trades_ = total_trades_;
        daily_winning_trades_ = 0;
        daily_losing_trades_ = 0;

        // Reset trade filter's daily frequency limits for next trading day
        trade_filter_->reset_daily_limits(static_cast<int>(bars_seen_));

        // Verify filter reset worked
        auto stats = trade_filter_->get_trade_stats(static_cast<int>(bars_seen_));
        std::cout << "  [FILTER RESET] Trades today: " << stats.trades_today
                  << " (should be 0)\n";

        // Log position states after reset
        std::cout << "  [POSITION STATES AFTER RESET]:\n";
        for (const auto& symbol : symbols_) {
            const auto& state = trade_filter_->get_position_state(symbol);
            std::cout << "    " << symbol << ": "
                      << (state.has_position ? "HOLDING" : "FLAT")
                      << " | last_exit_bar: " << state.last_exit_bar
                      << " (should be -999 for FLAT positions)\n";
        }
        std::cout << "\n";
    }

    // Update drawdown tracking (after all trading for this bar)
    double current_equity = get_equity(market_data);
    update_drawdown(current_equity);

    // Update last trading date for next iteration
    last_trading_date = current_trading_date;
}

void MultiSymbolTrader::make_trades(const std::unordered_map<Symbol, PredictionData>& predictions,
                                    const std::unordered_map<Symbol, Bar>& market_data) {

    // Enhanced Debug: Detailed trade analysis (every 50 bars when no positions)
    static size_t debug_counter = 0;
    if (positions_.empty() && bars_seen_ % 50 == 0) {
        std::cout << "\n[TRADE ANALYSIS] Bar " << bars_seen_ << ":\n";

        // Sort by 5-bar prediction strength
        std::vector<std::pair<Symbol, const PredictionData*>> debug_ranked;
        for (const auto& [symbol, pred] : predictions) {
            debug_ranked.emplace_back(symbol, &pred);
        }
        std::sort(debug_ranked.begin(), debug_ranked.end(),
                  [](const auto& a, const auto& b) {
                      return std::abs(a.second->prediction.pred_5bar.prediction) >
                             std::abs(b.second->prediction.pred_5bar.prediction);
                  });

        // Show top 5 with detailed rejection reasons
        for (size_t i = 0; i < std::min(size_t(5), debug_ranked.size()); ++i) {
            const auto& symbol = debug_ranked[i].first;
            const auto& pred = *debug_ranked[i].second;

            // Calculate probability
            double probability = prediction_to_probability(pred.prediction.pred_5bar.prediction);
            bool is_long = pred.prediction.pred_5bar.prediction > 0;

            // Apply BB amplification if data available
            auto bar_it = market_data.find(symbol);
            double probability_with_bb = probability;
            if (bar_it != market_data.end()) {
                probability_with_bb = apply_bb_amplification(probability, symbol, bar_it->second, is_long);
            }

            bool passes_prob = is_long ? (probability_with_bb > config_.buy_threshold)
                                       : (probability_with_bb < config_.sell_threshold);
            bool can_enter = trade_filter_->can_enter_position(
                symbol, static_cast<int>(bars_seen_), pred.prediction);

            std::cout << "  " << symbol
                      << " | 5-bar: " << std::fixed << std::setprecision(2)
                      << (pred.prediction.pred_5bar.prediction * 10000) << " bps"
                      << " | conf: " << (pred.prediction.pred_5bar.confidence * 100) << "%"
                      << " | prob: " << (probability * 100) << "%"
                      << (probability_with_bb != probability ?
                          " -> " + std::to_string(int(probability_with_bb * 100)) + "% (BB)" : "")
                      << " | thresh: " << (passes_prob ? "PASS" : "BLOCKED")
                      << " | filter: " << (can_enter ? "PASS" : "BLOCKED")
                      << "\n";
        }
    }

    // Rank symbols by 5-bar predicted return (absolute value for rotation)
    std::vector<std::pair<Symbol, double>> ranked;
    for (const auto& [symbol, pred] : predictions) {
        // Use ABSOLUTE VALUE of 5-bar prediction for ranking (strongest signals first)
        ranked.emplace_back(symbol, std::abs(pred.prediction.pred_5bar.prediction));
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Get top N symbols that pass probability threshold
    std::vector<Symbol> top_symbols;
    for (size_t i = 0; i < ranked.size(); ++i) {
        // Stop if we have enough symbols
        if (top_symbols.size() >= config_.max_positions) {
            break;
        }

        const auto& symbol = ranked[i].first;
        const auto& pred_data = predictions.at(symbol);

        // Convert prediction to probability
        double probability = prediction_to_probability(pred_data.prediction.pred_5bar.prediction);

        // Apply Bollinger Band amplification if enabled
        bool is_long = pred_data.prediction.pred_5bar.prediction > 0;
        auto bar_it = market_data.find(symbol);
        if (bar_it != market_data.end()) {
            probability = apply_bb_amplification(probability, symbol, bar_it->second, is_long);
        }

        // Check probability threshold (from online_trader)
        bool passes_probability = is_long ? (probability > config_.buy_threshold)
                                           : (probability < config_.sell_threshold);

        // Also check trade filter (optional, can be disabled by setting min_prediction_for_entry = 0)
        bool passes_filter = trade_filter_->can_enter_position(
                symbol, static_cast<int>(bars_seen_), pred_data.prediction);

        // REQ-FILTER-002: Market Regime Detection (ADX-based)
        // Mean reversion works best in LOW ADX (choppy/ranging) markets
        // Reject entries when ADX > max_adx_for_entry (strong trend = mean reversion fails)
        bool passes_regime = true;
        if (config_.enable_regime_filter) {
            auto extractor_it = extractors_.find(symbol);
            if (extractor_it != extractors_.end()) {
                // Get bar history from feature extractor
                auto bars = extractor_it->second->history().to_vector();

                // Calculate ADX for regime detection
                double adx = extractor_it->second->calculate_adx(bars, config_.adx_period);

                // Block entry if ADX too high (strong trend)
                if (adx > config_.max_adx_for_entry) {
                    passes_regime = false;
                }
            }
        }

        // REQ-FILTER-001: Multi-Horizon Agreement Filter
        // Reject entries when prediction horizons disagree on direction
        // E.g., 1-bar says buy but 5-bar/10-bar say sell = conflicting signal
        bool passes_horizon_agreement = true;
        if (config_.enable_horizon_agreement) {
            // Check direction for each horizon
            bool h1_bullish = pred_data.prediction.pred_1bar.prediction > 0;
            bool h5_bullish = pred_data.prediction.pred_5bar.prediction > 0;
            bool h10_bullish = pred_data.prediction.pred_10bar.prediction > 0;

            // Count how many horizons agree
            int bullish_count = (h1_bullish ? 1 : 0) + (h5_bullish ? 1 : 0) + (h10_bullish ? 1 : 0);
            int bearish_count = 3 - bullish_count;
            int max_agreement = std::max(bullish_count, bearish_count);

            // Require at least min_horizons_agreeing to pass
            if (max_agreement < config_.min_horizons_agreeing) {
                passes_horizon_agreement = false;
            }
        }

        // REQ-SIG-002: Prediction Strength Filter
        // Reject weak predictions near zero (noise, not signal)
        // Focus on high-conviction signals with strong magnitude
        bool passes_strength_filter = true;
        if (config_.enable_prediction_strength_filter) {
            double abs_prediction = std::abs(pred_data.prediction.pred_5bar.prediction);
            if (abs_prediction < config_.min_abs_prediction) {
                passes_strength_filter = false;
            }
        }

        if (passes_probability && passes_filter && passes_regime && passes_horizon_agreement && passes_strength_filter) {
            top_symbols.push_back(symbol);
        }
    }

    // ========================================================================
    // RANK-BASED ROTATION LOGIC (from online_trader)
    // ========================================================================
    // Entry follows 3 modes:
    // 1. Fill empty slots with top-ranked signals
    // 2. Hold positions if they remain in top N
    // 3. Rotate out weak positions ONLY if significantly better signal available

    // Step 1: Enter new positions if we have empty slots
    for (const auto& symbol : top_symbols) {
        if (positions_.size() >= config_.max_positions) break;

        // Skip if already holding
        if (positions_.find(symbol) != positions_.end()) {
            continue;
        }

        // Skip if in rotation cooldown
        if (in_rotation_cooldown(symbol)) {
            continue;
        }

        const auto& pred_data = predictions.at(symbol);

        // Calculate position size
        double size = calculate_position_size(symbol, pred_data);

        // Make sure we have enough cash
        if (size > cash_ * 0.95) {
            size = cash_ * 0.95;
        }

        if (size > 100) {  // Minimum position size $100
            auto it = market_data.find(symbol);
            if (it != market_data.end()) {
                // Check position compatibility (prevent inverse positions)
                if (!is_position_compatible(symbol)) {
                    continue;  // Skip this symbol (message already logged)
                }

                // Check signal confirmations (RSI, BB, Volume)
                bool is_long = pred_data.prediction.pred_5bar.prediction > 0;
                int confirmations = check_signal_confirmations(symbol, it->second, pred_data.features, is_long);
                if (confirmations < config_.min_confirmations_required) {
                    if (bars_seen_ % 100 == 0) {  // Only log occasionally to reduce noise
                        std::cout << "  [CONFIRMATION BLOCKED] " << symbol
                                  << " (" << confirmations << "/" << config_.min_confirmations_required << ")\n";
                    }
                    continue;  // Skip - insufficient confirmations
                }

                enter_position(symbol, it->second.close, it->second.timestamp, size, it->second.bar_id);

                // Record entry with trade filter
                trade_filter_->record_entry(
                    symbol,
                    static_cast<int>(bars_seen_),
                    pred_data.prediction.pred_5bar.prediction,
                    it->second.close
                );

                // Log entry with multi-horizon info
                std::cout << "  [ENTRY] " << symbol
                         << " at $" << std::fixed << std::setprecision(2)
                         << it->second.close
                         << " | 1-bar: " << std::setprecision(4)
                         << (pred_data.prediction.pred_1bar.prediction * 100) << "%"
                         << " | 5-bar: " << (pred_data.prediction.pred_5bar.prediction * 100) << "%"
                         << " | conf: " << std::setprecision(2)
                         << (pred_data.prediction.pred_5bar.confidence * 100) << "%\n";
            }
        }
    }

    // Step 2: Check if rotation is warranted (all slots filled + better signal available)
    if (config_.enable_rotation && positions_.size() >= config_.max_positions) {
        // Find the next best signal not currently held
        for (size_t i = 0; i < ranked.size(); ++i) {
            const auto& [candidate_symbol, candidate_strength] = ranked[i];

            // Skip if already holding
            if (positions_.find(candidate_symbol) != positions_.end()) {
                continue;
            }

            // Skip if doesn't pass filters (same as entry check)
            const auto& pred_data = predictions.at(candidate_symbol);
            double probability = prediction_to_probability(pred_data.prediction.pred_5bar.prediction);
            bool is_long = pred_data.prediction.pred_5bar.prediction > 0;
            auto bar_it = market_data.find(candidate_symbol);
            if (bar_it != market_data.end()) {
                probability = apply_bb_amplification(probability, candidate_symbol, bar_it->second, is_long);
            }
            bool passes_probability = is_long ? (probability > config_.buy_threshold)
                                               : (probability < config_.sell_threshold);
            bool passes_filter = trade_filter_->can_enter_position(
                    candidate_symbol, static_cast<int>(bars_seen_), pred_data.prediction);

            if (!passes_probability || !passes_filter) {
                continue;  // Candidate doesn't meet entry criteria
            }

            // Skip if in rotation cooldown
            if (in_rotation_cooldown(candidate_symbol)) {
                continue;
            }

            // Find weakest current position
            Symbol weakest = find_weakest_position(predictions);
            if (weakest.empty()) {
                break;  // No positions to rotate
            }

            // Get weakest strength and prediction
            double weakest_pred = predictions.at(weakest).prediction.pred_5bar.prediction;
            double weakest_strength = std::abs(weakest_pred);

            // CRITICAL: Only rotate if signals have SAME direction
            // Rotating from LONG â†’ SHORT (or vice versa) is a signal reversal, not a rotation!
            double candidate_pred = pred_data.prediction.pred_5bar.prediction;
            bool same_direction = (weakest_pred > 0 && candidate_pred > 0) ||
                                 (weakest_pred < 0 && candidate_pred < 0);

            if (!same_direction) {
                continue;  // Don't rotate opposite directions - wait for signal deterioration to exit
            }

            // Check if rotation is justified by strength delta
            double strength_delta = candidate_strength - weakest_strength;

            if (strength_delta >= config_.rotation_strength_delta) {
                // ROTATION JUSTIFIED - exit weakest and enter stronger signal
                auto it = market_data.find(weakest);
                if (it != market_data.end()) {
                    std::cout << "  [ROTATION] OUT: " << weakest
                             << " (strength: " << std::fixed << std::setprecision(4)
                             << (weakest_strength * 10000) << " bps)"
                             << " â†’ IN: " << candidate_symbol
                             << " (strength: " << (candidate_strength * 10000) << " bps)"
                             << " | Delta: " << (strength_delta * 10000) << " bps\n";

                    // Exit weakest
                    exit_position(weakest, it->second.close, it->second.timestamp, it->second.bar_id);

                    // Set rotation cooldown for the exited symbol
                    rotation_cooldowns_[weakest] = config_.rotation_cooldown_bars;

                    // Enter stronger candidate (same logic as regular entry)
                    double size = calculate_position_size(candidate_symbol, pred_data);
                    if (size > cash_ * 0.95) {
                        size = cash_ * 0.95;
                    }

                    if (size > 100) {
                        auto entry_it = market_data.find(candidate_symbol);
                        if (entry_it != market_data.end()) {
                            if (is_position_compatible(candidate_symbol)) {
                                // Check signal confirmations for rotation entry too
                                bool is_long_rotation = pred_data.prediction.pred_5bar.prediction > 0;
                                int rotation_confirmations = check_signal_confirmations(
                                    candidate_symbol, entry_it->second, pred_data.features, is_long_rotation);

                                if (rotation_confirmations < config_.min_confirmations_required) {
                                    std::cout << "  [ROTATION BLOCKED] " << candidate_symbol
                                              << " (confirmations: " << rotation_confirmations
                                              << "/" << config_.min_confirmations_required << ")\n";
                                    break;  // Don't rotate if new position doesn't meet confirmation criteria
                                }

                                enter_position(candidate_symbol, entry_it->second.close,
                                             entry_it->second.timestamp, size, entry_it->second.bar_id);

                                trade_filter_->record_entry(
                                    candidate_symbol,
                                    static_cast<int>(bars_seen_),
                                    pred_data.prediction.pred_5bar.prediction,
                                    entry_it->second.close
                                );

                                std::cout << "  [ENTRY] " << candidate_symbol
                                         << " at $" << std::fixed << std::setprecision(2)
                                         << entry_it->second.close
                                         << " (via rotation, confirmations: " << rotation_confirmations << ")\n";
                            }
                        }
                    }

                    break;  // Only one rotation per bar
                }
            } else {
                // Not enough improvement - stop checking
                break;
            }
        }
    }
}

void MultiSymbolTrader::update_positions(
    const std::unordered_map<Symbol, Bar>& market_data,
    const std::unordered_map<Symbol, PredictionData>& predictions) {

    std::vector<Symbol> to_exit;

    for (const auto& [symbol, pos] : positions_) {
        auto bar_it = market_data.find(symbol);
        if (bar_it == market_data.end()) continue;

        Price current_price = bar_it->second.close;

        // Get current prediction for this symbol (if available)
        auto pred_it = predictions.find(symbol);
        if (pred_it == predictions.end()) {
            // No prediction available, use fallback logic (emergency stop loss only)
            double pnl_pct = pos.pnl_percentage(current_price);
            if (pnl_pct <= config_.stop_loss_pct) {
                to_exit.push_back(symbol);
            }
            continue;
        }

        const auto& pred_data = pred_it->second;

        // Check price-based exits first (MA crossover, trailing stop)
        std::string price_exit_reason;
        if (should_exit_on_price(symbol, current_price, price_exit_reason)) {
            to_exit.push_back(symbol);
            continue;  // Skip trade filter check if price-based exit triggered
        }

        // REQ-FILTER-002: Forced exit on ADX spike (strong trend emerging)
        // Mean reversion positions should be closed when market shifts to trending
        if (config_.enable_regime_filter) {
            auto extractor_it = extractors_.find(symbol);
            if (extractor_it != extractors_.end()) {
                // Get bar history from feature extractor
                auto bars = extractor_it->second->history().to_vector();

                // Calculate ADX for regime detection
                double adx = extractor_it->second->calculate_adx(bars, config_.adx_period);

                // Force exit if ADX spikes above threshold (trend emerging)
                if (adx > config_.min_adx_for_exit) {
                    to_exit.push_back(symbol);
                    continue;  // Skip trade filter check if regime-based exit triggered
                }
            }
        }

        // Check if we should exit using trade filter
        // This handles: emergency stops, profit targets, max hold, signal quality, etc.
        bool should_exit = trade_filter_->should_exit_position(
            symbol,
            static_cast<int>(bars_seen_),
            pred_data.prediction,
            current_price
        );

        if (should_exit) {
            to_exit.push_back(symbol);
        }
    }

    // Execute exits
    for (const auto& symbol : to_exit) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            double pnl_pct = positions_[symbol].pnl_percentage(it->second.close);
            int bars_held = trade_filter_->get_bars_held(symbol);

            // Determine exit reason for logging
            std::string reason = "Unknown";
            if (pnl_pct < config_.filter_config.emergency_stop_loss_pct) {
                reason = "EmergencyStop";
            } else if (pnl_pct > config_.filter_config.profit_target_multiple * 0.01) {
                reason = "ProfitTarget";
            } else if (bars_held >= config_.filter_config.max_bars_to_hold) {
                reason = "MaxHold";
            } else if (bars_held >= config_.filter_config.min_bars_to_hold) {
                reason = "SignalExit";
            } else {
                reason = "EarlyExit";
            }

            exit_position(symbol, it->second.close, it->second.timestamp, it->second.bar_id);

            // Log exit with details
            std::cout << "  [EXIT] " << symbol
                     << " at $" << std::fixed << std::setprecision(2)
                     << it->second.close
                     << " | P&L: " << std::setprecision(2) << (pnl_pct * 100) << "%"
                     << " | Held: " << bars_held << " bars"
                     << " | Reason: " << reason << "\n";
        }
    }
}

double MultiSymbolTrader::calculate_position_size(const Symbol& symbol, const PredictionData& pred_data) {
    // Simple Kelly Criterion Position Sizing (25% fractional Kelly baseline)
    // Focus: More trades with smaller, well-managed positions

    double confidence = pred_data.prediction.pred_5bar.confidence;
    double signal_strength = std::abs(pred_data.prediction.pred_5bar.prediction);

    // Available capital (95% of cash, keep 5% for slippage/fees)
    double available_capital = cash_ * 0.95;

    // Base Kelly calculation (25% fractional Kelly)
    double win_probability = std::max(0.51, std::min(0.95, confidence));
    double expected_win_pct = 0.02;   // 2% average win
    double expected_loss_pct = 0.015; // 1.5% average loss
    double win_loss_ratio = expected_win_pct / expected_loss_pct;

    double p = win_probability;
    double q = 1.0 - p;
    double kelly_fraction = (p * win_loss_ratio - q) / win_loss_ratio;
    kelly_fraction = std::max(0.0, std::min(1.0, kelly_fraction));

    // Use 25% of Kelly (conservative for mean reversion)
    double recommended_pct = kelly_fraction * 0.25;
    recommended_pct = std::max(0.05, std::min(0.30, recommended_pct));

    double position_capital = available_capital * recommended_pct;

    // Simple multipliers for signal quality
    double multiplier = 1.0;

    // Signal strength adjustment
    if (signal_strength > 0.005) {
        multiplier *= 1.3;  // Strong signal
    } else if (signal_strength < 0.002) {
        multiplier *= 0.8;  // Weak signal
    }

    // Recent trade history adjustment
    if (trade_history_.count(symbol)) {
        auto& history = *trade_history_[symbol];
        if (history.size() >= 3) {
            int consecutive_wins = 0;
            int consecutive_losses = 0;

            for (size_t i = 0; i < std::min(size_t(3), history.size()); ++i) {
                if (history[i].pnl > 0) {
                    if (consecutive_losses == 0) consecutive_wins++;
                    else break;
                } else {
                    if (consecutive_wins == 0) consecutive_losses++;
                    else break;
                }
            }

            if (consecutive_wins >= 3) {
                multiplier *= config_.win_multiplier;  // Scale up on streak
            } else if (consecutive_losses >= 3) {
                multiplier *= config_.loss_multiplier; // Scale down on losses
            }
        }
    }

    // Apply multiplier
    position_capital *= multiplier;

    // Hard limits
    double max_position = available_capital * 0.35;  // Max 35% per position
    double min_position = available_capital * 0.05;  // Min 5% to be worth it

    position_capital = std::min(position_capital, max_position);

    if (position_capital < min_position) {
        return 0.0;  // Too small, skip trade
    }

    // Final safety
    position_capital = std::min(position_capital, available_capital);

    return position_capital;
}

bool MultiSymbolTrader::is_position_compatible(const Symbol& new_symbol) const {
    // Define inverse ETF pairs (leveraged bull/bear pairs)
    static const std::map<std::string, std::string> inverse_pairs = {
        // 3x Tech (NASDAQ-100)
        {"TQQQ", "SQQQ"}, {"SQQQ", "TQQQ"},

        // 3x Small Cap (Russell 2000)
        {"TNA", "TZA"}, {"TZA", "TNA"},

        // 3x Semiconductors
        {"SOXL", "SOXS"}, {"SOXS", "SOXL"},

        // 2x S&P 500
        {"SSO", "SDS"}, {"SDS", "SSO"},

        // Volatility
        {"UVXY", "SVIX"}, {"SVIX", "UVXY"},

        // 3x Energy
        {"ERX", "ERY"}, {"ERY", "ERX"},

        // 3x Financials
        {"FAS", "FAZ"}, {"FAZ", "FAS"},

        // 3x S&P 500
        {"SPXL", "SPXS"}, {"SPXS", "SPXL"}
    };

    // Check if new symbol would create contradictory position
    for (const auto& [symbol, pos] : positions_) {
        // Look up if current position has an inverse pair
        auto it = inverse_pairs.find(symbol);
        if (it != inverse_pairs.end()) {
            // Check if new symbol is the inverse of current position
            if (it->second == new_symbol) {
                // Inverse position blocked - always log this important safety check
                std::cout << "  âš ï¸  POSITION BLOCKED: " << new_symbol
                          << " is inverse of existing position " << symbol << "\n";
                return false;  // Inverse position not allowed
            }
        }
    }

    return true;  // Compatible with existing positions
}

void MultiSymbolTrader::enter_position(const Symbol& symbol, Price price,
                                       Timestamp time, double capital, uint64_t bar_id) {
    if (capital > cash_) {
        capital = cash_;  // Don't over-leverage
    }

    int shares = static_cast<int>(capital / price);

    if (shares <= 0) return;

    // Calculate entry costs if enabled
    AlpacaCostModel::TradeCosts entry_costs;
    if (config_.enable_cost_tracking) {
        const auto& ctx = market_context_[symbol];
        entry_costs = AlpacaCostModel::calculate_trade_cost(
            symbol, price, shares, true,  // is_buy = true
            ctx.avg_daily_volume,
            ctx.current_volatility,
            ctx.minutes_from_open,
            false  // is_short_sale = false
        );
    }

    double total_cost = shares * price + entry_costs.total_cost;

    if (total_cost <= cash_) {
        PositionWithCosts pos(shares, price, time, bar_id);
        pos.entry_costs = entry_costs;

        // Pre-calculate estimated exit costs
        if (config_.enable_cost_tracking) {
            const auto& ctx = market_context_[symbol];
            pos.estimated_exit_costs = AlpacaCostModel::calculate_trade_cost(
                symbol, price, shares, false,  // is_buy = false (selling)
                ctx.avg_daily_volume,
                ctx.current_volatility,
                ctx.minutes_from_open,
                false
            );
        }

        positions_[symbol] = pos;
        cash_ -= total_cost;
        total_transaction_costs_ += entry_costs.total_cost;

        // Initialize exit tracking for price-based exits
        if (config_.enable_price_based_exits) {
            ExitTrackingData tracking;
            tracking.entry_ma = calculate_exit_ma(symbol);
            tracking.max_profit_pct = 0.0;
            tracking.max_profit_price = price;
            tracking.is_long = (shares > 0);
            exit_tracking_[symbol] = tracking;
        }
    }
}

double MultiSymbolTrader::exit_position(const Symbol& symbol, Price price, Timestamp time, uint64_t bar_id) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) return 0.0;

    const PositionWithCosts& pos = it->second;

    // Calculate exit costs if enabled
    AlpacaCostModel::TradeCosts exit_costs;
    if (config_.enable_cost_tracking) {
        const auto& ctx = market_context_[symbol];
        exit_costs = AlpacaCostModel::calculate_trade_cost(
            symbol, price, pos.shares, false,  // is_buy = false (selling)
            ctx.avg_daily_volume,
            ctx.current_volatility,
            ctx.minutes_from_open,
            false
        );
    }

    double proceeds = pos.shares * price - exit_costs.total_cost;
    double gross_pnl = pos.shares * (price - pos.entry_price);
    double net_pnl = gross_pnl - pos.entry_costs.total_cost - exit_costs.total_cost;
    double pnl_pct = net_pnl / (pos.shares * pos.entry_price + pos.entry_costs.total_cost);

    // Record trade for adaptive sizing (now includes bar_ids)
    // Use net_pnl for trade record
    TradeRecord trade(net_pnl, pnl_pct, pos.entry_time, time, symbol,
                     pos.shares, pos.entry_price, price, pos.entry_bar_id, bar_id);
    trade_history_[symbol]->push_back(trade);

    // Also add to complete trade log for export
    all_trades_log_.push_back(trade);

    // Memory management: Limit trade log size to prevent unbounded growth
    // Keep most recent 10,000 trades, archive older ones
    if (all_trades_log_.size() > 10000) {
        // Remove oldest 5,000 trades (keep newest 5,000)
        all_trades_log_.erase(
            all_trades_log_.begin(),
            all_trades_log_.begin() + 5000
        );
    }

    // Track daily wins/losses
    if (net_pnl > 0) {
        daily_winning_trades_++;
    } else if (net_pnl < 0) {
        daily_losing_trades_++;
    }

    cash_ += proceeds;
    total_transaction_costs_ += exit_costs.total_cost;
    positions_.erase(it);
    exit_tracking_.erase(symbol);  // Clean up exit tracking
    total_trades_++;

    // Notify trade filter that position is closed
    trade_filter_->record_exit(symbol, static_cast<int>(bars_seen_));

    return net_pnl;
}

void MultiSymbolTrader::liquidate_all(const std::unordered_map<Symbol, Bar>& market_data,
                                      const std::string& reason) {
    std::vector<Symbol> symbols_to_exit;
    for (const auto& [symbol, pos] : positions_) {
        symbols_to_exit.push_back(symbol);
    }

    for (const auto& symbol : symbols_to_exit) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            exit_position(symbol, it->second.close, it->second.timestamp, it->second.bar_id);
        }
    }
}

double MultiSymbolTrader::get_equity(const std::unordered_map<Symbol, Bar>& market_data) const {
    double equity = cash_;

    for (const auto& [symbol, pos] : positions_) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            equity += pos.market_value(it->second.close);
        }
    }

    return equity;
}

MultiSymbolTrader::BacktestResults MultiSymbolTrader::get_results() const {
    BacktestResults results;

    // Collect all trades across all symbols
    std::vector<TradeRecord> all_trades;
    for (const auto& [symbol, history] : trade_history_) {
        for (size_t i = 0; i < history->size(); ++i) {
            all_trades.push_back((*history)[i]);
        }
    }

    results.total_trades = total_trades_;
    results.winning_trades = 0;
    results.losing_trades = 0;
    double gross_profit = 0.0;
    double gross_loss = 0.0;

    for (const auto& trade : all_trades) {
        if (trade.is_win()) {
            results.winning_trades++;
            gross_profit += trade.pnl;
        } else if (trade.is_loss()) {
            results.losing_trades++;
            gross_loss += std::abs(trade.pnl);
        }
    }

    results.win_rate = (results.total_trades > 0)
                       ? static_cast<double>(results.winning_trades) / results.total_trades
                       : 0.0;

    results.avg_win = (results.winning_trades > 0)
                      ? gross_profit / results.winning_trades
                      : 0.0;

    results.avg_loss = (results.losing_trades > 0)
                       ? gross_loss / results.losing_trades
                       : 0.0;

    results.profit_factor = (gross_loss > 0)
                            ? gross_profit / gross_loss
                            : (gross_profit > 0 ? 999.0 : 0.0);

    // Calculate equity metrics
    // Note: For accurate final_equity, need last market_data - so this is approximate
    results.final_equity = cash_;
    for (const auto& [symbol, pos] : positions_) {
        // Use entry price as approximation (ideally should use last known price)
        results.final_equity += pos.market_value(pos.entry_price);
    }

    results.total_return = (config_.initial_capital > 0)
                          ? (results.final_equity - config_.initial_capital) / config_.initial_capital
                          : 0.0;

    // Calculate MRD using trading_bars_ (excludes warmup period)
    double days_traded = static_cast<double>(trading_bars_) / config_.bars_per_day;
    results.mrd = (days_traded > 0) ? results.total_return / days_traded : 0.0;

    // Use tracked max drawdown
    results.max_drawdown = max_drawdown_observed_;

    // Calculate Sharpe ratio from daily returns
    if (daily_results_.size() >= 2) {
        // Calculate mean and std dev of daily returns
        std::vector<double> daily_returns;
        for (const auto& daily : daily_results_) {
            daily_returns.push_back(daily.daily_return);
        }

        double mean_return = 0.0;
        for (double ret : daily_returns) {
            mean_return += ret;
        }
        mean_return /= daily_returns.size();

        double variance = 0.0;
        for (double ret : daily_returns) {
            double diff = ret - mean_return;
            variance += diff * diff;
        }
        // FIXED: Use Bessel's correction (N-1) for sample variance
        variance /= (daily_returns.size() - 1);

        double std_dev = std::sqrt(variance);

        // Annualized Sharpe ratio (assuming 252 trading days per year)
        // For daily returns: multiply by sqrt(252)
        results.sharpe_ratio = (std_dev > 1e-8) ? (mean_return / std_dev) * std::sqrt(252.0) : 0.0;

        // Sanity check: Sharpe > 10 is suspicious for daily trading
        if (results.sharpe_ratio > 10.0) {
            std::cerr << "WARNING: Sharpe ratio " << results.sharpe_ratio
                      << " seems unrealistically high. Check calculation and data quality." << std::endl;
        }
    } else {
        // Not enough data for Sharpe calculation
        results.sharpe_ratio = 0.0;
    }

    // Cost tracking
    results.total_transaction_costs = total_transaction_costs_;
    results.avg_cost_per_trade = (results.total_trades > 0)
                                 ? total_transaction_costs_ / results.total_trades
                                 : 0.0;

    // Calculate total volume traded
    double total_volume = 0.0;
    for (const auto& trade : all_trades_log_) {
        total_volume += trade.shares * trade.entry_price;  // Entry volume
        total_volume += trade.shares * trade.exit_price;   // Exit volume
    }
    results.cost_as_pct_of_volume = (total_volume > 0)
                                    ? (total_transaction_costs_ / total_volume) * 100.0
                                    : 0.0;

    // Net return after costs
    results.net_return_after_costs = results.total_return;  // Already includes costs in cash

    // Daily breakdown
    results.daily_breakdown = daily_results_;

    return results;
}

void MultiSymbolTrader::update_market_context(const Symbol& symbol, const Bar& bar) {
    auto& ctx = market_context_[symbol];

    // Update time-based context
    ctx.minutes_from_open = calculate_minutes_from_open(bar.timestamp);

    // Update spread if available (bar.high - bar.low is a proxy)
    // In production, use actual bid/ask data
    ctx.update_spread(bar.low, bar.high);

    // Update volatility using simple rolling estimate
    // In production, use more sophisticated volatility estimation
    // For now, keep default or calculate from recent price changes
    if (extractors_[symbol]->bar_count() >= 20) {
        const auto& history = extractors_[symbol]->history();
        size_t count = history.size();
        if (count >= 20) {
            // Calculate 20-bar volatility
            double sum_returns_sq = 0.0;
            int n_returns = 0;
            for (size_t i = count - 19; i < count; ++i) {
                if (history[i-1].close > 0) {
                    double ret = (history[i].close - history[i-1].close) / history[i-1].close;
                    sum_returns_sq += ret * ret;
                    n_returns++;
                }
            }
            if (n_returns > 0) {
                double variance = sum_returns_sq / n_returns;
                ctx.current_volatility = std::sqrt(variance);
            }
        }
    }

    // In production, update avg_daily_volume from actual market data
    // For now, keep the default value
}

int MultiSymbolTrader::calculate_minutes_from_open(Timestamp ts) const {
    // Convert timestamp to time_t for date/time manipulation
    auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        ts.time_since_epoch()
    ).count();

    // Get time of day in milliseconds
    constexpr int64_t ms_per_day = 24LL * 60 * 60 * 1000;
    int64_t time_of_day_ms = time_ms % ms_per_day;

    // Market opens at 9:30 AM ET = 9.5 hours = 34200 seconds = 34200000 ms
    // Note: This assumes timestamps are in ET. Adjust for timezone if needed.
    constexpr int64_t market_open_ms = 9LL * 60 * 60 * 1000 + 30 * 60 * 1000;

    int64_t minutes_from_open = (time_of_day_ms - market_open_ms) / (60 * 1000);

    // Clamp to [0, 390] (regular trading hours)
    if (minutes_from_open < 0) minutes_from_open = 0;
    if (minutes_from_open > 390) minutes_from_open = 390;

    return static_cast<int>(minutes_from_open);
}

double MultiSymbolTrader::prediction_to_probability(double prediction) const {
    if (!config_.enable_probability_scaling) {
        return prediction;  // No scaling, use raw prediction
    }

    // Convert prediction to probability using tanh scaling (from online_trader)
    // probability = 0.5 + 0.5 * tanh(prediction * scaling_factor)
    // This maps small predictions to probabilities around 0.5
    // and amplifies them based on scaling_factor
    double scaled = std::tanh(prediction * config_.probability_scaling_factor);
    return 0.5 + 0.5 * scaled;
}

MultiSymbolTrader::BBands MultiSymbolTrader::calculate_bollinger_bands(
    const Symbol& symbol, const Bar& current_bar) const {

    BBands bands;

    // Get recent price history
    auto it = extractors_.find(symbol);
    if (it == extractors_.end()) {
        return bands;  // No data
    }

    const auto& history = it->second->history();
    size_t count = history.size();

    if (count < static_cast<size_t>(config_.bb_period)) {
        return bands;  // Not enough data
    }

    // Calculate SMA (middle band)
    double sum = 0.0;
    for (size_t i = count - config_.bb_period; i < count; ++i) {
        sum += history[i].close;
    }
    bands.middle = sum / config_.bb_period;

    // Calculate standard deviation
    double sum_sq = 0.0;
    for (size_t i = count - config_.bb_period; i < count; ++i) {
        double diff = history[i].close - bands.middle;
        sum_sq += diff * diff;
    }
    double std_dev = std::sqrt(sum_sq / config_.bb_period);

    // Calculate upper and lower bands
    bands.upper = bands.middle + (config_.bb_std_dev * std_dev);
    bands.lower = bands.middle - (config_.bb_std_dev * std_dev);

    return bands;
}

double MultiSymbolTrader::apply_bb_amplification(
    double probability, const Symbol& symbol,
    const Bar& bar, bool is_long) const {

    if (!config_.enable_bb_amplification) {
        return probability;  // No amplification
    }

    // Calculate BB bands
    BBands bands = calculate_bollinger_bands(symbol, bar);

    if (bands.middle == 0.0) {
        return probability;  // No valid bands
    }

    double current_price = bar.close;
    double band_width = bands.upper - bands.lower;

    if (band_width == 0.0) {
        return probability;  // Invalid band width
    }

    // Calculate proximity to bands
    // For long: boost if near lower band (oversold)
    // For short: boost if near upper band (overbought)

    if (is_long) {
        // Distance from lower band
        double distance_from_lower = (current_price - bands.lower) / band_width;

        // If within proximity threshold of lower band, amplify
        if (distance_from_lower < config_.bb_proximity_threshold) {
            double boost = config_.bb_amplification_factor * (1.0 - distance_from_lower / config_.bb_proximity_threshold);
            return std::min(0.99, probability + boost);
        }
    } else {
        // Distance from upper band
        double distance_from_upper = (bands.upper - current_price) / band_width;

        // If within proximity threshold of upper band, amplify
        if (distance_from_upper < config_.bb_proximity_threshold) {
            double boost = config_.bb_amplification_factor * (1.0 - distance_from_upper / config_.bb_proximity_threshold);
            return std::max(0.01, probability - boost);  // For shorts, reduce probability
        }
    }

    return probability;
}

int MultiSymbolTrader::check_signal_confirmations(
    const Symbol& symbol, const Bar& bar,
    const Eigen::VectorXd& features, bool is_long) const {

    if (!config_.enable_signal_confirmation) {
        return config_.min_confirmations_required;  // Pass if disabled
    }

    int confirmations = 0;

    // === 1. RSI CONFIRMATION ===
    // Use robust enum instead of magic number (FeatureExtractor::RSI_14)
    // RSI in [0, 1] range: 0 = extremely oversold, 1 = extremely overbought
    double rsi = features(FeatureExtractor::RSI_14);

    if (is_long) {
        // For longs, want RSI < oversold threshold (buy the dip)
        if (rsi < config_.rsi_oversold_threshold) {
            confirmations++;
        }
    } else {
        // For shorts, want RSI > overbought threshold (sell the rip)
        if (rsi > config_.rsi_overbought_threshold) {
            confirmations++;
        }
    }

    // === 2. BOLLINGER BAND CONFIRMATION ===
    // Check if price is near band extremes (mean reversion setup)
    BBands bands = calculate_bollinger_bands(symbol, bar);

    if (bands.middle > 0.0) {  // Valid bands
        double band_width = bands.upper - bands.lower;
        if (band_width > 0.0) {
            // Calculate position within bands: 0 = lower band, 0.5 = middle, 1 = upper band
            double bb_position = (bar.close - bands.lower) / band_width;

            if (is_long) {
                // For longs, want price near lower band (oversold)
                // bb_position < 0.15 means within 15% of lower band
                if (bb_position < (1.0 - config_.bb_extreme_threshold)) {
                    confirmations++;
                }
            } else {
                // For shorts, want price near upper band (overbought)
                // bb_position > 0.85 means within 15% of upper band
                if (bb_position > config_.bb_extreme_threshold) {
                    confirmations++;
                }
            }
        }
    }

    // === 3. VOLUME SURGE CONFIRMATION ===
    // Use robust enum instead of magic number (FeatureExtractor::VolumeSurge)
    // volume_surge > 1.0 means above average, > 1.3 means significant surge
    double volume_surge = features(FeatureExtractor::VolumeSurge);

    if (volume_surge > config_.volume_surge_threshold) {
        confirmations++;
    }

    return confirmations;
}

double MultiSymbolTrader::calculate_exit_ma(const Symbol& symbol) const {
    auto it = extractors_.find(symbol);
    if (it == extractors_.end()) {
        return 0.0;  // No data
    }

    const auto& history = it->second->history();
    size_t count = history.size();

    if (count < static_cast<size_t>(config_.ma_exit_period)) {
        return 0.0;  // Not enough data
    }

    // Calculate simple moving average
    double sum = 0.0;
    for (size_t i = count - config_.ma_exit_period; i < count; ++i) {
        sum += history[i].close;
    }

    return sum / config_.ma_exit_period;
}

bool MultiSymbolTrader::should_exit_on_price(const Symbol& symbol, Price current_price, std::string& exit_reason) {
    if (!config_.enable_price_based_exits) {
        return false;  // Feature disabled
    }

    auto pos_it = positions_.find(symbol);
    if (pos_it == positions_.end()) {
        return false;  // No position
    }

    auto track_it = exit_tracking_.find(symbol);
    if (track_it == exit_tracking_.end()) {
        return false;  // No tracking data (shouldn't happen)
    }

    const auto& pos = pos_it->second;
    auto& tracking = track_it->second;

    // Update max profit tracking
    double current_profit_pct = pos.pnl_percentage(current_price);
    if (current_profit_pct > tracking.max_profit_pct) {
        tracking.max_profit_pct = current_profit_pct;
        tracking.max_profit_price = current_price;
    }

    // === EXIT CONDITION 1: MA CROSSOVER (Mean Reversion Complete) ===
    if (config_.exit_on_ma_crossover && tracking.entry_ma > 0.0) {
        double current_ma = calculate_exit_ma(symbol);
        if (current_ma > 0.0) {
            bool crossed_ma = false;

            if (tracking.is_long) {
                // For longs: entered below MA, exit when price crosses ABOVE MA
                crossed_ma = (current_price > current_ma) && (pos.entry_price < tracking.entry_ma);
            } else {
                // For shorts: entered above MA, exit when price crosses BELOW MA
                crossed_ma = (current_price < current_ma) && (pos.entry_price > tracking.entry_ma);
            }

            if (crossed_ma) {
                exit_reason = "MA_Crossover";
                return true;
            }
        }
    }

    // === EXIT CONDITION 2: TRAILING STOP (Lock in profits) ===
    if (tracking.max_profit_pct > 0.0) {
        // Trail stop at configured percentage of max profit
        double trail_threshold = tracking.max_profit_pct * config_.trailing_stop_percentage;

        if (current_profit_pct < trail_threshold) {
            exit_reason = "TrailingStop";
            return true;
        }
    }

    return false;
}

// =============================================================================
// Warmup Phase Management
// =============================================================================

void MultiSymbolTrader::update_phase() {
    if (!config_.warmup.enabled) {
        config_.current_phase = TradingConfig::LIVE_TRADING;
        return;
    }

    int days_complete = bars_seen_ / config_.bars_per_day;

    if (days_complete < config_.warmup.observation_days) {
        config_.current_phase = TradingConfig::WARMUP_OBSERVATION;
    }
    else if (days_complete < config_.warmup.observation_days + config_.warmup.simulation_days) {
        // Transition to simulation
        if (config_.current_phase == TradingConfig::WARMUP_OBSERVATION) {
            std::cout << "\nðŸ“Š Transitioning from OBSERVATION to SIMULATION phase\n";
            warmup_metrics_.starting_equity = cash_;
            warmup_metrics_.current_equity = cash_;
            warmup_metrics_.max_equity = cash_;
        }
        config_.current_phase = TradingConfig::WARMUP_SIMULATION;
    }
    else {
        // Check if we meet go-live criteria
        if (config_.current_phase == TradingConfig::WARMUP_SIMULATION) {
            if (evaluate_warmup_complete()) {
                config_.current_phase = TradingConfig::WARMUP_COMPLETE;
                std::cout << "\nâœ… WARMUP COMPLETE - Ready for live trading\n";
                print_warmup_summary();
            } else {
                std::cout << "\nâŒ Warmup criteria not met - extending simulation\n";
                // Stay in simulation
            }
        }
    }
}

void MultiSymbolTrader::handle_observation_phase(const std::unordered_map<Symbol, Bar>& market_data) {
    warmup_metrics_.observation_bars_complete++;

    if (bars_seen_ % 100 == 0) {
        std::cout << "  [OBSERVATION] Bar " << bars_seen_
                  << " - Learning patterns, no trades\n";
    }
}

void MultiSymbolTrader::handle_simulation_phase(
    const std::unordered_map<Symbol, PredictionData>& predictions,
    const std::unordered_map<Symbol, Bar>& market_data) {

    warmup_metrics_.simulation_bars_complete++;

    // Run normal trading logic (reuse existing code)
    if (bars_seen_ > config_.min_bars_to_learn || trading_bars_ > 0) {
        trading_bars_++;

        // Track equity before trades
        double pre_trade_equity = get_equity(market_data);

        // Run normal trading
        make_trades(predictions, market_data);

        // Track equity after trades
        warmup_metrics_.current_equity = get_equity(market_data);
        warmup_metrics_.update_drawdown();

        // Record simulated trades (they're already in all_trades_log_)
        if (all_trades_log_.size() > warmup_metrics_.simulated_trades.size()) {
            warmup_metrics_.simulated_trades = all_trades_log_;
        }
    }

    if (bars_seen_ % 100 == 0) {
        double sim_return = warmup_metrics_.starting_equity > 0 ?
            (warmup_metrics_.current_equity - warmup_metrics_.starting_equity) /
            warmup_metrics_.starting_equity * 100 : 0.0;

        std::cout << "  [SIMULATION] Bar " << bars_seen_
                  << " | Equity: $" << std::fixed << std::setprecision(2)
                  << warmup_metrics_.current_equity
                  << " (" << std::showpos << sim_return << "%" << std::noshowpos << ")"
                  << " | Trades: " << warmup_metrics_.simulated_trades.size() << "\n";
    }
}

void MultiSymbolTrader::handle_live_phase(
    const std::unordered_map<Symbol, PredictionData>& predictions,
    const std::unordered_map<Symbol, Bar>& market_data) {

    // Normal trading - exactly as before warmup was added
    if (bars_seen_ > config_.min_bars_to_learn || trading_bars_ > 0) {
        trading_bars_++;
        make_trades(predictions, market_data);
    }
}

bool MultiSymbolTrader::evaluate_warmup_complete() {
    const auto& cfg = config_.warmup;
    const auto& metrics = warmup_metrics_;

    // CRITICAL WARNING: Alert if using TESTING mode
    if (cfg.mode == TradingConfig::WarmupMode::TESTING) {
        std::cout << "\nâš ï¸  WARNING: Warmup in TESTING mode (relaxed criteria)\n";
        std::cout << "âš ï¸  NOT SAFE FOR LIVE TRADING - Use PRODUCTION mode for real money!\n\n";
    }

    // Check minimum trades
    if (static_cast<int>(metrics.simulated_trades.size()) < cfg.min_trades) {
        std::cout << "  âŒ Too few trades: " << metrics.simulated_trades.size()
                  << " < " << cfg.min_trades << "\n";
        return false;
    }

    // Check Sharpe ratio
    double sharpe = metrics.calculate_sharpe();
    if (sharpe < cfg.min_sharpe_ratio) {
        std::cout << "  âŒ Sharpe too low: " << std::fixed << std::setprecision(2)
                  << sharpe << " < " << cfg.min_sharpe_ratio
                  << " [Mode: " << cfg.get_mode_name() << "]\n";
        return false;
    }

    // Check drawdown
    if (metrics.max_drawdown > cfg.max_drawdown) {
        std::cout << "  âŒ Drawdown too high: " << (metrics.max_drawdown * 100)
                  << "% > " << (cfg.max_drawdown * 100) << "%"
                  << " [Mode: " << cfg.get_mode_name() << "]\n";
        return false;
    }

    // Check profitability
    double total_return = metrics.starting_equity > 0 ?
        (metrics.current_equity - metrics.starting_equity) / metrics.starting_equity : 0.0;

    if (cfg.require_positive_return && total_return < 0) {
        std::cout << "  âŒ Negative return: " << (total_return * 100) << "%\n";
        return false;
    }

    // All checks passed
    std::cout << "  âœ… All warmup criteria met [Mode: " << cfg.get_mode_name() << "]\n";
    return true;
}

void MultiSymbolTrader::print_warmup_summary() {
    std::cout << "\n========== WARMUP SUMMARY ==========\n";
    std::cout << "Observation: " << config_.warmup.observation_days << " days\n";
    std::cout << "Simulation: " << config_.warmup.simulation_days << " days\n";
    std::cout << "\nResults:\n";

    double total_return = warmup_metrics_.starting_equity > 0 ?
        (warmup_metrics_.current_equity - warmup_metrics_.starting_equity) /
        warmup_metrics_.starting_equity : 0.0;

    std::cout << "  Return: " << std::fixed << std::setprecision(2)
              << (total_return * 100) << "%\n";
    std::cout << "  Sharpe: " << warmup_metrics_.calculate_sharpe() << "\n";
    std::cout << "  Max DD: " << (warmup_metrics_.max_drawdown * 100) << "%\n";
    std::cout << "  Trades: " << warmup_metrics_.simulated_trades.size() << "\n";

    // Win/loss breakdown
    int wins = 0, losses = 0;
    for (const auto& trade : warmup_metrics_.simulated_trades) {
        if (trade.pnl > 0) wins++;
        else if (trade.pnl < 0) losses++;
    }

    if (!warmup_metrics_.simulated_trades.empty()) {
        std::cout << "  Win Rate: " << std::fixed << std::setprecision(1)
                  << (100.0 * wins / warmup_metrics_.simulated_trades.size())
                  << "% (" << wins << "W/" << losses << "L)\n";
    }

    std::cout << "\nâœ… All criteria met - ready for live\n";
    std::cout << "====================================\n\n";
}

// ============================================================================
// ROTATION LOGIC (from online_trader)
// ============================================================================

Symbol MultiSymbolTrader::find_weakest_position(
    const std::unordered_map<Symbol, PredictionData>& predictions) const {

    if (positions_.empty()) {
        return "";
    }

    Symbol weakest_symbol;
    double min_strength = std::numeric_limits<double>::max();

    for (const auto& [symbol, position] : positions_) {
        // Get current signal strength for this position
        auto pred_it = predictions.find(symbol);
        if (pred_it == predictions.end()) {
            continue;  // No prediction available - skip
        }

        // Use 5-bar prediction strength (absolute value)
        double strength = std::abs(pred_it->second.prediction.pred_5bar.prediction);

        if (strength < min_strength) {
            min_strength = strength;
            weakest_symbol = symbol;
        }
    }

    return weakest_symbol;
}

void MultiSymbolTrader::update_rotation_cooldowns() {
    // Decrement all cooldowns
    for (auto& [symbol, cooldown] : rotation_cooldowns_) {
        if (cooldown > 0) {
            cooldown--;
        }
    }

    // Remove expired cooldowns (cleanup)
    for (auto it = rotation_cooldowns_.begin(); it != rotation_cooldowns_.end(); ) {
        if (it->second <= 0) {
            it = rotation_cooldowns_.erase(it);
        } else {
            ++it;
        }
    }
}

bool MultiSymbolTrader::in_rotation_cooldown(const Symbol& symbol) const {
    auto it = rotation_cooldowns_.find(symbol);
    return (it != rotation_cooldowns_.end() && it->second > 0);
}

} // namespace trading

```

## ðŸ“„ **FILE 25 of 25**: src/utils/config_loader.cpp

**File Information**:
- **Path**: `src/utils/config_loader.cpp`
- **Size**: 171 lines
- **Modified**: 2025-10-20 06:50:36
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "utils/config_loader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace trading {

// Helper to safely get a value or use a default
template<typename T>
T get_val(const json& j, const std::string& key, const T& default_val) {
    return j.value(key, default_val);
}

TradingConfig ConfigLoader::loadFromJSON(const std::string& json_path) {
    TradingConfig config; // Start with hardcoded defaults

    std::ifstream f(json_path);
    if (!f.is_open()) {
        std::cerr << "âš ï¸  WARNING: Could not open config file " << json_path
                  << "\n    Using default hardcoded parameters." << std::endl;
        return config; // Return defaults
    }

    try {
        json data = json::parse(f);

        std::cout << "ðŸ“„ Loading trading config from " << json_path << std::endl;

        // --- Top-Level Parameters ---
        config.initial_capital = get_val(data, "initial_capital", config.initial_capital);
        config.max_positions = get_val(data, "max_positions", config.max_positions);
        config.stop_loss_pct = get_val(data, "stop_loss_pct", config.stop_loss_pct);
        config.profit_target_pct = get_val(data, "profit_target_pct", config.profit_target_pct);
        config.bars_per_day = get_val(data, "bars_per_day", config.bars_per_day);
        config.eod_liquidation = get_val(data, "eod_liquidation", config.eod_liquidation);
        config.min_bars_to_learn = get_val(data, "min_bars_to_learn", config.min_bars_to_learn);
        config.lookback_window = get_val(data, "lookback_window", config.lookback_window);

        // --- Horizon Config ---
        if (data.contains("horizon_config")) {
            const auto& hc = data["horizon_config"];
            auto& h_config = config.horizon_config;
            h_config.lambda_1bar = get_val(hc, "lambda_1bar", h_config.lambda_1bar);
            h_config.lambda_5bar = get_val(hc, "lambda_5bar", h_config.lambda_5bar);
            h_config.lambda_10bar = get_val(hc, "lambda_10bar", h_config.lambda_10bar);
            h_config.min_confidence = get_val(hc, "min_confidence", h_config.min_confidence);
        }

        // --- Filter Config ---
        if (data.contains("filter_config")) {
            const auto& fc = data["filter_config"];
            auto& f_config = config.filter_config;
            f_config.min_bars_to_hold = get_val(fc, "min_bars_to_hold", f_config.min_bars_to_hold);
            f_config.typical_hold_period = get_val(fc, "typical_hold_period", f_config.typical_hold_period);
            f_config.max_bars_to_hold = get_val(fc, "max_bars_to_hold", f_config.max_bars_to_hold);
            f_config.min_prediction_for_entry = get_val(fc, "min_prediction_for_entry", f_config.min_prediction_for_entry);
            f_config.min_confidence_for_entry = get_val(fc, "min_confidence_for_entry", f_config.min_confidence_for_entry);
        }

        // --- Strategy Parameters ---
        config.enable_probability_scaling = get_val(data, "enable_probability_scaling", config.enable_probability_scaling);
        config.probability_scaling_factor = get_val(data, "probability_scaling_factor", config.probability_scaling_factor);
        config.buy_threshold = get_val(data, "buy_threshold", config.buy_threshold);
        config.sell_threshold = get_val(data, "sell_threshold", config.sell_threshold);

        // --- Bollinger Band Amplification ---
        config.enable_bb_amplification = get_val(data, "enable_bb_amplification", config.enable_bb_amplification);
        config.bb_period = get_val(data, "bb_period", config.bb_period);
        config.bb_std_dev = get_val(data, "bb_std_dev", config.bb_std_dev);
        config.bb_proximity_threshold = get_val(data, "bb_proximity_threshold", config.bb_proximity_threshold);
        config.bb_amplification_factor = get_val(data, "bb_amplification_factor", config.bb_amplification_factor);

        // --- Rotation Parameters ---
        config.enable_rotation = get_val(data, "enable_rotation", config.enable_rotation);
        config.rotation_strength_delta = get_val(data, "rotation_strength_delta", config.rotation_strength_delta);
        config.rotation_cooldown_bars = get_val(data, "rotation_cooldown_bars", config.rotation_cooldown_bars);
        config.min_rank_strength = get_val(data, "min_rank_strength", config.min_rank_strength);

        // --- Signal Confirmation ---
        config.enable_signal_confirmation = get_val(data, "enable_signal_confirmation", config.enable_signal_confirmation);
        config.min_confirmations_required = get_val(data, "min_confirmations_required", config.min_confirmations_required);
        config.rsi_oversold_threshold = get_val(data, "rsi_oversold_threshold", config.rsi_oversold_threshold);
        config.rsi_overbought_threshold = get_val(data, "rsi_overbought_threshold", config.rsi_overbought_threshold);
        config.bb_extreme_threshold = get_val(data, "bb_extreme_threshold", config.bb_extreme_threshold);
        config.volume_surge_threshold = get_val(data, "volume_surge_threshold", config.volume_surge_threshold);

        // --- Exit Parameters ---
        config.enable_price_based_exits = get_val(data, "enable_price_based_exits", config.enable_price_based_exits);
        config.exit_on_ma_crossover = get_val(data, "exit_on_ma_crossover", config.exit_on_ma_crossover);
        config.trailing_stop_percentage = get_val(data, "trailing_stop_percentage", config.trailing_stop_percentage);
        config.ma_exit_period = get_val(data, "ma_exit_period", config.ma_exit_period);

        // --- Dual EWRLS (Experimental) ---
        config.enable_dual_ewrls = get_val(data, "enable_dual_ewrls", config.enable_dual_ewrls);
        config.dual_ewrls_ma_period = get_val(data, "dual_ewrls_ma_period", config.dual_ewrls_ma_period);
        config.dual_ewrls_min_deviation = get_val(data, "dual_ewrls_min_deviation", config.dual_ewrls_min_deviation);

        // --- Mean Reversion Predictor ---
        config.enable_mean_reversion_predictor = get_val(data, "enable_mean_reversion_predictor", config.enable_mean_reversion_predictor);
        config.reversion_factor = get_val(data, "reversion_factor", config.reversion_factor);
        config.ma_period_1bar = get_val(data, "ma_period_1bar", config.ma_period_1bar);
        config.ma_period_5bar = get_val(data, "ma_period_5bar", config.ma_period_5bar);
        config.ma_period_10bar = get_val(data, "ma_period_10bar", config.ma_period_10bar);

        // --- Position Sizing ---
        if (data.contains("position_sizing")) {
            const auto& ps = data["position_sizing"];
            config.win_multiplier = get_val(ps, "win_multiplier", config.win_multiplier);
            config.loss_multiplier = get_val(ps, "loss_multiplier", config.loss_multiplier);
            config.trade_history_size = get_val(ps, "trade_history_size", config.trade_history_size);
        }

        // --- Cost Model ---
        if (data.contains("cost_model")) {
            const auto& cm = data["cost_model"];
            config.enable_cost_tracking = get_val(cm, "enable_cost_tracking", config.enable_cost_tracking);
            config.default_avg_volume = get_val(cm, "default_avg_volume", config.default_avg_volume);
            config.default_volatility = get_val(cm, "default_volatility", config.default_volatility);
            // Note: Slippage params are in AlpacaCostModel::SlippageConfig, not TradingConfig
            // These would need to be added to TradingConfig if we want to tune them
        }

        // --- EWRLS Config ---
        // Note: EWRLS::Config is separate, would need refactoring to tune these
        // For now, they remain in EWRLS::Config with hardcoded defaults

        // --- Market Regime Filter (REQ-FILTER-002) ---
        config.enable_regime_filter = get_val(data, "enable_regime_filter", config.enable_regime_filter);
        config.adx_period = get_val(data, "adx_period", config.adx_period);
        config.max_adx_for_entry = get_val(data, "max_adx_for_entry", config.max_adx_for_entry);
        config.min_adx_for_exit = get_val(data, "min_adx_for_exit", config.min_adx_for_exit);

        // --- Isotonic Calibration (REQ-SIG-001) ---
        config.enable_isotonic_calibration = get_val(data, "enable_isotonic_calibration", config.enable_isotonic_calibration);
        config.calibration_min_samples = get_val(data, "calibration_min_samples", config.calibration_min_samples);
        config.calibration_window = get_val(data, "calibration_window", config.calibration_window);

        // --- Multi-Horizon Agreement Filter (REQ-FILTER-001) ---
        config.enable_horizon_agreement = get_val(data, "enable_horizon_agreement", config.enable_horizon_agreement);
        config.min_horizons_agreeing = get_val(data, "min_horizons_agreeing", config.min_horizons_agreeing);

        // --- Prediction Strength Filter (REQ-SIG-002) ---
        config.enable_prediction_strength_filter = get_val(data, "enable_prediction_strength_filter", config.enable_prediction_strength_filter);
        config.min_abs_prediction = get_val(data, "min_abs_prediction", config.min_abs_prediction);

        // Aggressive Kelly and boost features removed - back to simple Kelly baseline

        std::cout << "âœ… Successfully loaded trading config (100+ parameters)!" << std::endl;
        std::cout << "   Buy threshold: " << config.buy_threshold
                  << " | Sell threshold: " << config.sell_threshold << std::endl;
        std::cout << "   Lambda (1/5/10): " << config.horizon_config.lambda_1bar
                  << "/" << config.horizon_config.lambda_5bar
                  << "/" << config.horizon_config.lambda_10bar << std::endl;

    } catch (json::parse_error& e) {
        std::cerr << "âŒ ERROR: Failed to parse config file " << json_path << "\n"
                  << "   " << e.what() << "\n"
                  << "   Using default hardcoded parameters." << std::endl;
        return TradingConfig(); // Return defaults
    } catch (std::exception& e) {
        std::cerr << "âŒ ERROR: Exception while loading config: " << e.what() << "\n"
                  << "   Using default hardcoded parameters." << std::endl;
        return TradingConfig(); // Return defaults
    }

    return config;
}

} // namespace trading

```

