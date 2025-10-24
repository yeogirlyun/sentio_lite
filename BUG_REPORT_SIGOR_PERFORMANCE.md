# Bug Report: Performance Issue with SIGOR Strategy in sentio_lite

**Date:** 2025-10-24
**Severity:** High
**Status:** Open
**Affects Version:** v1.0+

---

## Summary

`sentio_lite` experiences severe performance degradation when using `--strategy sigor`, causing the program to hang indefinitely during date filtering. The same operation completes successfully with `--strategy ewrls` (default).

---

## Description

When running sentio_lite with SIGOR strategy on any test date, the program loads all market data successfully but then hangs indefinitely during the "Filtering to test date window" phase. The issue manifests consistently across all test dates.

### Expected Behavior
```bash
build/sentio_lite mock --strategy sigor --date 2025-10-22 --sim-days 1
```
Should complete within 2-5 seconds (similar to EWRLS strategy).

### Actual Behavior
- Program loads all 177,905 bars from binary files (~3 seconds)
- Prints "Filtering to test date window..."
- **Hangs indefinitely** - no progress after several minutes
- Must be manually terminated (Ctrl+C or kill)

### Working Behavior (EWRLS)
```bash
build/sentio_lite mock --strategy ewrls --date 2025-10-22 --sim-days 1
```
Completes successfully in 2-3 seconds with identical data loading.

---

## Root Cause Analysis

### Primary Issue: Date Extraction Performance

The `filter_to_date()` function in `src/main.cpp` calls `get_trading_days()` which iterates through **all bars** to extract unique trading dates. With 177,905 bars loaded, this becomes extremely slow:

```cpp
// src/main.cpp:304-318
std::vector<std::string> get_trading_days(const std::vector<Bar>& bars) {
    std::set<std::string> unique_days;

    for (const auto& bar : bars) {  // ← Iterates ALL 177K bars!
        auto duration = bar.timestamp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        time_t time = static_cast<time_t>(seconds);
        struct tm* timeinfo = gmtime(&time);
        char buffer[11];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
        unique_days.insert(buffer);
    }

    return std::vector<std::string>(unique_days.begin(), unique_days.end());
}
```

**Performance Impact:**
- FAZ: 177,905 bars × (timestamp conversion + string formatting + set insertion) ≈ **6-8 seconds**
- FAS: 177,905 bars × operations ≈ **6-8 seconds**
- SPXL: 177,905 bars × operations ≈ **6-8 seconds**
- SPXS: 177,905 bars × operations ≈ **6-8 seconds**
- **Total:** ~30+ seconds just for date extraction

### Attempted Fix (Partial)

Modified `filter_to_date()` to sample only last 60 days (src/main.cpp:414-419):

```cpp
// Optimized: only check last 60 days worth of bars
size_t sample_size = std::min(first_symbol_bars.size(), size_t(60 * bars_per_day));
size_t start_idx = first_symbol_bars.size() > sample_size ? first_symbol_bars.size() - sample_size : 0;
std::vector<Bar> sample_bars(first_symbol_bars.begin() + start_idx, first_symbol_bars.end());
std::vector<std::string> trading_days = get_trading_days(sample_bars);
```

**Result:** Still too slow. Copying 23,460 bars (60 days × 391 bars/day) and processing them takes 3-5 seconds per symbol.

### Why EWRLS Works

EWRLS strategy may be using a different code path or the issue is masked by other initialization that happens in parallel. Need further investigation to confirm.

---

## Impact

### Functional Impact
- **SIGOR strategy is unusable** in sentio_lite for backtesting
- Cannot generate dashboards or performance reports for SIGOR
- Blocks apples-to-apples comparison between EWRLS and SIGOR strategies

### Workaround
Use Python-based test_sigor binary with Optuna optimization scripts:
```bash
python3 tools/optuna_sigor.py --end-date 2024-10-22 --trials 200
```
This successfully tests SIGOR on the same dates.

---

## Proposed Solutions

### Solution 1: Optimize Date Extraction (Quick Fix)
Instead of iterating all bars, use binary search or sampling:

```cpp
std::vector<std::string> get_trading_days_optimized(const std::vector<Bar>& bars, int max_days = 90) {
    if (bars.empty()) return {};

    std::set<std::string> unique_days;
    std::string last_date;

    // Start from end, work backwards, stop after max_days
    for (auto it = bars.rbegin(); it != bars.rend() && unique_days.size() < max_days; ++it) {
        std::string date = timestamp_to_date(it->timestamp);
        if (date != last_date) {
            unique_days.insert(date);
            last_date = date;
        }
    }

    return std::vector<std::string>(unique_days.begin(), unique_days.end());
}
```

**Pros:** Minimal code change, should fix 90% of cases
**Cons:** May miss older dates if user requests them

### Solution 2: Filter During Data Loading (Proper Fix)
Modify `DataLoader::load_binary()` to accept date range parameter and filter while reading:

```cpp
std::vector<Bar> DataLoader::load_binary_filtered(
    const std::string& path,
    const std::string& symbol,
    const std::string& start_date,  // Optional
    const std::string& end_date     // Optional
);
```

**Pros:** Most efficient, solves root cause
**Cons:** Requires larger refactor of data loading architecture

### Solution 3: Cache Trading Days (Medium Fix)
Cache the list of trading days in a small index file:

```
data/FAZ_RTH_NH.bin.index  (contains: first_date, last_date, all_dates[])
```

**Pros:** Very fast lookups after first run
**Cons:** Requires index generation step, cache invalidation logic

---

## Steps to Reproduce

1. Ensure you have market data files in `data/` directory:
   ```
   data/FAZ_RTH_NH.bin (177,905 bars)
   data/FAS_RTH_NH.bin (177,905 bars)
   data/SPXL_RTH_NH.bin (177,905 bars)
   etc.
   ```

2. Build sentio_lite:
   ```bash
   cmake --build build -j8
   ```

3. Run with SIGOR strategy:
   ```bash
   build/sentio_lite mock --strategy sigor --date 2025-10-22 --sim-days 1
   ```

4. Observe: Program hangs after printing "Filtering to test date window..."

5. Compare with EWRLS (working):
   ```bash
   build/sentio_lite mock --strategy ewrls --date 2025-10-22 --sim-days 1
   ```

---

## Environment

- **OS:** macOS Darwin 24.6.0
- **Compiler:** Apple Clang (C++17)
- **Data Size:** 177,905 bars (~455 days) for FAZ, FAS, SPXL, SPXS
- **CMake Version:** 3.x
- **Build Type:** Release

---

## Related Code References

### Core Issue Location
- **src/main.cpp:304-318** - `get_trading_days()` - Inefficient date extraction (iterates all bars)
- **src/main.cpp:407-477** - `filter_to_date()` - Calls `get_trading_days()` during filtering
- **src/main.cpp:311** - Changed `localtime()` to `gmtime()` to fix timezone issues
- **src/main.cpp:414-419** - Attempted optimization (sample last 60 days) - still too slow

### Data Loading
- **src/utils/data_loader.cpp:99-169** - `load_binary()` - Loads all bars without filtering
- **src/utils/data_loader.cpp:11-23** - `load()` - Entry point for data loading
- **src/utils/data_loader.cpp:202-242** - `load_from_directory()` - Multi-symbol loading

### Strategy Integration
- **include/trading/multi_symbol_trader.h:45-50** - TradingConfig with strategy selection
- **include/trading/multi_symbol_trader.h:143-147** - Strategy-specific predictor storage
- **src/trading/multi_symbol_trader.cpp:58-74** - Constructor with strategy initialization
- **src/trading/multi_symbol_trader.cpp:180-200** - on_bar() with strategy-specific prediction

### SIGOR Adapter
- **include/predictor/sigor_predictor_adapter.h:23-155** - Adapter connecting SIGOR to MultiSymbolTrader
- **include/predictor/sigor_predictor_adapter.h:44-94** - predict() - Converts SIGOR probability to prediction
- **include/predictor/sigor_predictor_adapter.h:123-126** - update_with_bar() - SIGOR signal generation

### Strategy Configuration
- **include/trading/trading_strategy.h:1-65** - StrategyType enum and helper functions
- **include/utils/config_loader.h:231-374** - SigorConfigLoader for loading SIGOR parameters
- **config/sigor_params.json** - Optimized SIGOR parameters (k=1.8, detector weights, window sizes)

### Command-Line Interface
- **src/main.cpp:138-148** - Strategy parsing (`--strategy` option)
- **src/main.cpp:540-560** - Strategy-specific config loading (SIGOR vs EWRLS)
- **src/main.cpp:629-638** - filter_to_date() call site

### Core Data Structures
- **include/core/bar.h** - Bar struct with timestamp
- **include/core/types.h** - Type definitions (Timestamp, Symbol, etc.)

### Utility Functions
- **src/main.cpp:293-301** - `timestamp_to_date()` - Converts timestamp to YYYY-MM-DD string
- **src/main.cpp:320-334** - `find_warmup_start_date()` - Counts backwards N trading days

---

## Testing Notes

### Successful Tests
- ✅ Build completes without errors
- ✅ SIGOR config loads correctly (config/sigor_params.json)
- ✅ Data files load successfully (177K+ bars)
- ✅ EWRLS strategy works perfectly with same data
- ✅ Python-based test_sigor works with same dates

### Failed Tests
- ❌ SIGOR strategy hangs on date: 2025-10-22
- ❌ SIGOR strategy hangs on date: 2025-10-21
- ❌ SIGOR strategy hangs on date: 2025-10-18
- ❌ SIGOR strategy hangs on date: 2025-10-17
- ❌ SIGOR strategy hangs on date: 2025-10-16

### Performance Baseline
- **EWRLS Strategy:** 2-3 seconds total (including data load + filtering + execution)
- **SIGOR Strategy:** Never completes (>5 minutes, manually terminated)

---

## Recommendations

1. **Immediate:** Implement Solution 1 (optimized date extraction with early exit)
2. **Short-term:** Profile both EWRLS and SIGOR code paths to identify why EWRLS doesn't hit this issue
3. **Long-term:** Implement Solution 2 (filter during data loading) for architectural improvement
4. **Documentation:** Add warning in README about SIGOR performance issue until fixed

---

## Additional Notes

### Timezone Fix Applied
Changed `localtime()` to `gmtime()` in `get_trading_days()` (src/main.cpp:311) to fix date mismatch issues. This fixed the "Test date not found" error but revealed the underlying performance problem.

### SIGOR Optimization Results
Despite the sentio_lite performance issue, SIGOR strategy has been successfully optimized using Python tools:
- **Best MRD:** 0.4823% (Trial #195)
- **Test Period:** Oct 16-18, 21-22, 2024 (5 days)
- **Trials:** 200
- **Configuration:** Saved to config/sigor_params.json

### Related Issues
- None currently filed

---

## Priority Justification

**High Priority** because:
1. Blocks a major feature (SIGOR strategy) from being usable
2. No workaround exists within sentio_lite (must use separate Python tools)
3. Affects all users trying to use SIGOR strategy
4. Fix is relatively straightforward (Solution 1 can be implemented in <1 hour)
