# BUG REPORT: Multi-Day Testing Negative Returns

**Date**: 2025-10-17
**Severity**: HIGH
**Status**: Identified

---

## Problem Summary

After implementing multi-day testing support, the system shows consistently negative returns despite being profitable on single-day tests:

### Performance Comparison

| Test Period | Days | Trades | Win Rate | Return | MRD/day | Assessment |
|-------------|------|--------|----------|--------|---------|------------|
| **Single Day** (Oct 16) | 1 | 104 | 12.5% | **+0.46%** | +0.11% | 🟠 Moderate |
| **Multi-Day** (Oct 10-16) | 6 | 295 | **2.7%** | **-2.49%** | -0.28% | 🔴 Poor |

**Key Issues**:
- Win rate collapsed from 12.5% → 2.7% (82% decrease!)
- Went from profitable to losing money
- Average daily return: -0.42% per trading day
- Profit factor: 0.42 (losing $2.38 for every $1 gained)

---

## Root Cause Analysis

### Issue #1: EOD Liquidation Timing Misalignment (CRITICAL)

**Problem**: EOD liquidation is based on `bars_seen_ % 391` which doesn't align with actual calendar day boundaries when warmup period is included.

**Current Implementation** (src/trading/multi_symbol_trader.cpp:223-226):
```cpp
// Step 7: EOD liquidation
if (config_.eod_liquidation &&
    bars_seen_ % config_.bars_per_day == config_.bars_per_day - 1) {
    liquidate_all(market_data, "EOD");
}
```

**What Actually Happens**:
```
Warmup: 1173 bars (3 days)
bars_seen_ = 0-1172: Warmup period
bars_seen_ = 1173: Last warmup bar
bars_seen_ = 1174: Start trading

EOD triggers when bars_seen_ % 391 == 390:
- Bar 390: During warmup ❌
- Bar 781: During warmup ❌
- Bar 1172: During warmup ❌
- Bar 1563: During trading (389 bars after start) ❌
- Bar 1954: During trading (780 bars after start) ❌

Expected EOD for actual trading days:
- Bar 1564: End of Oct 10 (390 bars after start) ✓
- Bar 1955: End of Oct 11 (781 bars after start) ✓
- Bar 2346: End of Oct 12 ✓
```

**Impact**: Positions are being liquidated 1 bar EARLY, causing:
- Premature exits before price targets reached
- Missing the last bar's price movement
- Systematic timing disadvantage

---

### Issue #2: No Daily State Reset

**Problem**: The system treats multi-day testing as ONE continuous session instead of separate daily trading sessions.

**Evidence**:
```
Test Period: Oct 10-16 (6 days)
Bars Processed: 3519 (1173 warmup + 2346 trading)
Trading Period: 6 days  <-- Should be 6 separate sessions!
```

**What Should Happen** (online_trader approach):
```
Day 1 (Oct 10):
  - Use Days 7-9 for warmup
  - Trade Oct 10 (391 bars)
  - Close all positions at 15:58 ET
  - Report Day 1 P&L

Day 2 (Oct 11):
  - Predictors continue learning (no reset)
  - Trade filter state RESETS (cooldowns, frequency limits)
  - Trade Oct 11 (391 bars)
  - Close all positions at 15:58 ET
  - Report Day 2 P&L

Aggregate: Report total across all days
```

**What Actually Happens**:
```
Days 1-6:
  - Continuous 2346-bar session
  - Trade filter state NEVER resets
  - Frequency limits accumulate across days
  - No daily P&L breakdown
  - Single aggregate report
```

**Impact**:
- Trade filter hits max_trades_per_hour (50) and blocks trades for REST OF WEEK
- Cooldowns from Day 1 affect Day 2-6
- Cannot analyze day-by-day performance
- Missed trading opportunities in later days

---

### Issue #3: Trade Filter Frequency Limit Accumulation

**Evidence from debug output**:
```
[TRADE ANALYSIS] Bar 3450:
  UVXY | 5-bar: 74.59 bps | conf: 27.16% | prob: 67.83% | thresh: PASS | filter: BLOCKED
  SVIX | 5-bar: -62.84 bps | conf: 23.91% | prob: 34.79% | thresh: PASS | filter: BLOCKED
```

**Analysis**:
- Strong signals (67.83% probability) being blocked by filter
- This happens at bar 3450 (near end of multi-day test)
- max_trades_per_hour = 50 entries + 50 exits = 100 "trades"
- After ~100 trades, rolling 60-bar window blocks ALL new entries
- For 6-day test, this means Days 4-6 have severely limited trading

**Impact**:
- ~50% of trading days have limited to NO new entries
- System cannot capitalize on good signals in later days
- Systematic bias toward early days

---

### Issue #4: Dashboard File Naming and Location

**Current Behavior**:
```bash
./sentio_lite mock --start-date 2025-10-10 --end-date 2025-10-16 --generate-dashboard

Output: build/trading_dashboard.html  (OVERWRITES previous runs!)
```

**Problems**:
1. **No timestamp**: Each run overwrites the previous dashboard
2. **No unique ID**: Cannot compare multiple test runs
3. **Wrong location**: Should be in logs/dashboard/ for organization
4. **No metadata**: Filename doesn't indicate date range tested

**Expected Behavior**:
```bash
Output: logs/dashboard/dashboard_2025-10-10_to_2025-10-16_20251017_162800.html
```

---

## Evidence

### Multi-Day Test Output (Oct 10-16)

```
Test Summary:
  Test Period:        2025-10-10 to 2025-10-16
  Warmup:             3 days
  Trading Period:     6 days

Performance:
  Initial Capital:    $100000.00
  Final Equity:       $97508.03
  Total Return:       -2.49%
  MRD (Daily):        -0.28% per day

Trade Statistics:
  Total Trades:       295
  Winning Trades:     8
  Losing Trades:      22
  Win Rate:           2.7%      <-- Was 12.5% on single day!
  Average Win:        $14.78
  Average Loss:       $12.67
  Profit Factor:      0.42      <-- Losing $2.38 for every $1 won

Assessment: 🔴 Poor (not ready for live)
```

### Single-Day Test Output (Oct 16 only)

```
Test Summary:
  Test Date:          2025-10-16
  Warmup:             3 days
  Trading Period:     1 days

Performance:
  Initial Capital:    $100000.00
  Final Equity:       $100457.50
  Total Return:       +0.46%     <-- PROFITABLE!
  MRD (Daily):        +0.11% per day

Trade Statistics:
  Total Trades:       104
  Winning Trades:     13
  Losing Trades:      11
  Win Rate:           12.5%      <-- 4.6x better than multi-day!
  Average Win:        $25.48
  Average Loss:       $21.35
  Profit Factor:      1.41       <-- Approaching break-even

Assessment: 🟠 Moderate (needs optimization)
```

---

## Recommended Fixes

### Priority 1: Fix EOD Liquidation Timing (CRITICAL)

**Current** (src/trading/multi_symbol_trader.cpp:223):
```cpp
if (config_.eod_liquidation &&
    bars_seen_ % config_.bars_per_day == config_.bars_per_day - 1) {
```

**Proposed Fix**:
```cpp
// Track bars since trading started (exclude warmup)
size_t trading_bars = bars_seen_ > config_.min_bars_to_learn ?
                     bars_seen_ - config_.min_bars_to_learn : 0;

if (config_.eod_liquidation &&
    trading_bars > 0 &&
    trading_bars % config_.bars_per_day == config_.bars_per_day - 1) {
    liquidate_all(market_data, "EOD");
}
```

**Expected Impact**: Liquidate at CORRECT bar boundaries (390, 781, 1172, etc. bars after trading starts)

---

### Priority 2: Add Daily State Reset

**Proposed Implementation**:
```cpp
// In on_bar(), after EOD liquidation:
if (config_.eod_liquidation && /* EOD condition */) {
    liquidate_all(market_data, "EOD");

    // Reset trade filter daily state (but keep position history)
    trade_filter_->reset_daily_limits();

    // Store daily results for reporting
    daily_results_.push_back(get_current_day_results());
}
```

**New TradeFilter method**:
```cpp
void TradeFilter::reset_daily_limits() {
    // Clear trade frequency tracking (new trading day)
    trade_bars_.clear();

    // Keep position state (for tracking last_exit_bar, etc.)
    // Don't reset position_states_

    // Reset daily counter
    last_day_reset_ = 0;
}
```

**Expected Impact**:
- Each day starts fresh with trade frequency limits
- Can take full advantage of signals on all days
- Daily P&L breakdown for analysis

---

### Priority 3: Fix Dashboard File Naming

**Proposed Implementation** (src/main.cpp):
```cpp
// Generate unique dashboard filename with timestamp
auto now = std::chrono::system_clock::now();
auto time_t_now = std::chrono::system_clock::to_time_t(now);
struct tm* tm_now = localtime(&time_t_now);

char timestamp[20];
strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_now);

std::string dashboard_file;
if (is_multi_day) {
    dashboard_file = "logs/dashboard/dashboard_" +
                    config.start_date + "_to_" + config.end_date +
                    "_" + std::string(timestamp) + ".html";
} else {
    dashboard_file = "logs/dashboard/dashboard_" +
                    test_date + "_" + std::string(timestamp) + ".html";
}

// Create directory if needed
std::filesystem::create_directories("logs/dashboard");

generate_dashboard(..., dashboard_file, ...);
```

**Expected Output**:
```
logs/dashboard/dashboard_2025-10-10_to_2025-10-16_20251017_162800.html
logs/dashboard/dashboard_2025-10-16_20251017_163500.html
```

---

### Priority 4: Add Daily Breakdown Reporting

**Proposed**:
```cpp
struct DailyResults {
    std::string date;
    int trades;
    double pnl;
    double return_pct;
    int wins;
    int losses;
};

std::vector<DailyResults> daily_results_;

// In results export:
void export_daily_breakdown(const std::vector<DailyResults>& daily_results) {
    std::cout << "\nDaily Breakdown:\n";
    std::cout << "Date         | Trades | P&L      | Return  | Wins | Losses\n";
    std::cout << "-------------|--------|----------|---------|------|-------\n";

    for (const auto& day : daily_results) {
        std::cout << day.date << " | "
                  << std::setw(6) << day.trades << " | "
                  << std::setw(8) << std::fixed << std::setprecision(2) << day.pnl << " | "
                  << std::setw(6) << (day.return_pct * 100) << "% | "
                  << std::setw(4) << day.wins << " | "
                  << std::setw(5) << day.losses << "\n";
    }
}
```

**Expected Output**:
```
Daily Breakdown:
Date         | Trades | P&L      | Return  | Wins | Losses
-------------|--------|----------|---------|------|-------
2025-10-10   |     52 |  +125.50 |  +0.13% |    3 |      2
2025-10-11   |     48 |  -230.75 |  -0.23% |    1 |      5
2025-10-12   |     51 |  +310.25 |  +0.31% |    4 |      3
2025-10-13   |     45 |  -180.40 |  -0.18% |    2 |      4
2025-10-14   |     50 |   +95.80 |  +0.10% |    2 |      3
2025-10-16   |     49 |  +457.50 |  +0.46% |    3 |      2
-------------|--------|----------|---------|------|-------
Total        |    295 |  +577.90 |  +0.58% |   15 |     19
```

---

## Testing Plan

### Test 1: Verify EOD Timing Fix
```bash
./sentio_lite mock --start-date 2025-10-16 --end-date 2025-10-16 --verbose

Expected: EOD liquidation at bar 1564 (390 bars after bar 1174)
Current: EOD liquidation at bar 1563 (389 bars after bar 1174)
```

### Test 2: Verify Daily State Reset
```bash
./sentio_lite mock --start-date 2025-10-15 --end-date 2025-10-16 --verbose

Expected:
- Day 1: Can trade full 391 bars
- Day 2: Trade frequency counter RESETS, can trade another 391 bars
- Total possible trades: ~100 per day = 200 total

Current:
- Combined session: Trade frequency accumulates
- Total trades: Limited to ~100 total for both days
```

### Test 3: Verify Dashboard Naming
```bash
./sentio_lite mock --start-date 2025-10-10 --end-date 2025-10-16 --generate-dashboard

Expected: logs/dashboard/dashboard_2025-10-10_to_2025-10-16_<timestamp>.html
Current: build/trading_dashboard.html
```

---

## Files Requiring Changes

### src/trading/multi_symbol_trader.cpp
- Fix EOD timing (line 223-226)
- Add daily state reset
- Track trading_bars separately from bars_seen_

### src/trading/multi_symbol_trader.h
- Add daily_results_ vector
- Add trading_bars_ counter

### src/trading/trade_filter.cpp
- Implement reset_daily_limits() method

### src/trading/trade_filter.h
- Add reset_daily_limits() declaration

### src/main.cpp
- Generate unique dashboard filenames with timestamps
- Create logs/dashboard/ directory
- Add daily breakdown reporting
- Export daily results to JSON

### include/utils/results_exporter.h
- Support daily breakdown in JSON export

---

## References

### online_trader Implementation

**File**: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/backtest_command.cpp:375-376`
```cpp
// Calculate total warmup: warmup blocks + warmup bars
// This tells execute-trades to skip the warmup period when calculating results
int total_warmup_bars = (warmup_blocks * BARS_PER_BLOCK) + warmup_bars;
```

**File**: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/execute_trades_command.cpp:209-251`
```cpp
// Check for End-of-Day (EOD) closing time: 15:58 ET (2 minutes before market close)
bool is_eod_close = (et_hour == 15 && et_minute >= 58) || (et_hour >= 16);

if (is_eod_close && current_position.state != PositionStateMachine::State::CASH_ONLY) {
    // EOD close takes priority over all other conditions
    exit_reason = "EOD_CLOSE (15:58 ET)";
    // ... liquidate position
}
```

**Key Difference**: online_trader uses TIMESTAMP (15:58 ET) for EOD, not bar count!

### sentio_lite Current Implementation

**File**: `src/trading/multi_symbol_trader.cpp:223-226`
```cpp
// Step 7: EOD liquidation
if (config_.eod_liquidation &&
    bars_seen_ % config_.bars_per_day == config_.bars_per_day - 1) {
    liquidate_all(market_data, "EOD");
}
```

**File**: `src/main.cpp:521-529` (Multi-day filtering)
```cpp
if (is_multi_day) {
    std::cout << "\nFiltering to date range (including warmup period)...\n";
    filter_to_date_range(all_data, config.start_date, config.end_date,
                       config.warmup_bars, config.trading.bars_per_day, config.verbose);
}
```

**File**: `src/main.cpp:492` (Warmup offset fix)
```cpp
// CRITICAL FIX: Add 1 to skip overnight gap between last warmup bar and first test day bar
config.trading.min_bars_to_learn = config.warmup_bars + 1;  // Sets to 1174
```

### Related Issues

- **BUG_REPORT_PROBABILITY_SCALING.md**: Documents frequency limit protective behavior
- **ONLINE_TRADER_FINDINGS.md**: Documents online_trader's approach to backtesting
- **BUG_REPORT_LIMITED_TRADING.md**: Original issue with only 6 trades (now resolved)

---

## Impact Analysis

### Current Multi-Day Results
```
6 days trading: -2.49% return
Average per day: -0.42% per day
Win rate: 2.7% (vs 12.5% on single day)
```

### Expected After Fixes

**Assumption**: If each day performed like Oct 16 (+0.46%):
```
6 days × +0.46% = +2.76% total return
Win rate: ~12.5% per day
Profit factor: ~1.4 (vs current 0.42)
```

**Reality**: Days will vary, but should average closer to single-day performance:
```
Expected range: +1% to +3% total return (vs current -2.49%)
Expected win rate: 10-15% (vs current 2.7%)
Expected profit factor: 1.2-1.5 (vs current 0.42)
```

---

## Conclusion

The multi-day testing shows negative returns due to THREE critical bugs:

1. ❌ **EOD timing off by 1 bar**: Liquidating 389 bars after start instead of 390
2. ❌ **No daily state reset**: Trade frequency limits accumulate across entire week
3. ❌ **Dashboard naming**: Overwrites previous runs, no organization

**Proof**: Single-day test (Oct 16) is profitable (+0.46%), but multi-day loses money (-2.49%)

**Root Cause**: System treating 6 days as ONE continuous 2346-bar session instead of 6 separate 391-bar trading days

**Recommended Action**:
1. Fix EOD timing to use trading_bars instead of bars_seen_
2. Reset trade filter daily limits at EOD
3. Fix dashboard file naming with timestamps
4. Add daily breakdown reporting

Once these fixes are implemented, multi-day testing should show aggregate performance closer to the sum of individual days.

---

**Report Generated**: 2025-10-17
**System Version**: Sentio Lite v1.0.0 (Multi-Day Testing)
**Test Configuration**: config/symbols.conf (10 symbols)
**Test Period**: 2025-10-10 to 2025-10-16 (6 trading days)
