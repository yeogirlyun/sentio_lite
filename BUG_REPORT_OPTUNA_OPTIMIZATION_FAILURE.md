# Bug Report: Optuna Optimization Trials Failing with Zero Trades

**Date:** 2025-10-18
**Status:** 🔴 **CRITICAL** - Blocking automated parameter optimization
**Priority:** HIGH
**Component:** Optuna Integration (`tools/optimize_warmup.py`)

---

## Executive Summary

The Optuna-based parameter optimization system fails to generate any trades during optimization trials, despite identical manual command execution working correctly. All 100 trials return `Score=-15.000 | Return=-100.00% | PF=0.00 | WR=0.0% | Trades=0`, indicating complete failure to execute trades during the optimization period.

**Critical Impact:**
- Cannot automatically find optimal parameters for live/mock trading
- Blocks daily automated optimization workflow
- Prevents systematic parameter tuning for warmup system

---

## Purpose and Objectives

### Primary Goal
Implement an automated Optuna-based optimization system that:
1. **Optimizes trading parameters** (max_positions, stop_loss, profit_target, lambda)
2. **Optimizes warmup configuration** (enable_warmup, warmup_obs_days, warmup_sim_days)
3. **Uses historical data** (e.g., Sept 4 - Oct 16, 2025) for optimization
4. **Tests on target date** (e.g., Oct 17, 2025) with optimal parameters
5. **Reports MRD** (Mean Return per Day) and other performance metrics

### Expected Workflow
```bash
# Evening: Optimize for tomorrow
python3 tools/optimize_warmup.py \
    --test-date 2025-10-17 \
    --n-trials 100 \
    --optimization-days 30 \
    --mode mock

# Expected output:
#   - 100 trials exploring parameter space
#   - Each trial: backtest on Sept 4-Oct 16
#   - Best parameters saved to optimal_params.json
#   - Final test on Oct 17 with best parameters
#   - Report MRD, profit factor, Sharpe, etc.

# Morning: Deploy with optimized parameters
./scripts/launch_live.sh
```

### Current Objective (Session Goal)
**User Request:** "Run the optimization and find the best parameter and then test on yesterday's Oct 17th, 2025 and see how much we perform in terms of MRD"

---

## Issue Description

### Symptom
When running optimization via `optimize_warmup.py`, **all trials fail to generate any trades**:

```
Trial 0: Score=-15.000 | Return=-100.00% | PF=0.00 | WR=0.0% | Trades=0
Trial 1: Score=-15.000 | Return=-100.00% | PF=0.00 | WR=0.0% | Trades=0
Trial 2: Score=-15.000 | Return=-100.00% | PF=0.00 | WR=0.0% | Trades=0
...
Trial 99: Score=-15.000 | Return=-100.00% | PF=0.00 | WR=0.0% | Trades=0
```

### Expected Behavior
Each trial should:
1. Run backtest on Sept 4-Oct 16 (30 trading days)
2. Generate 3000+ trades (based on manual testing)
3. Return metrics: total_return, profit_factor, win_rate, total_trades
4. Calculate composite score for optimization

### Actual Behavior
- **Total trades:** 0 (should be ~3160 based on manual test)
- **Return:** -100.00% (penalty value for failed runs)
- **Score:** -15.0 (penalty score defined in code)
- **Result:** Optimization fails completely

---

## Evidence: Working vs. Failing Cases

### ✅ WORKING: Manual Execution

**Command:**
```bash
./build/sentio_lite mock \
    --start-date 2025-09-04 \
    --end-date 2025-10-16 \
    --warmup-days 1 \
    --no-dashboard \
    --results-file results.json \
    --capital 100000.0 \
    --max-positions 4 \
    --stop-loss -0.011 \
    --profit-target 0.065 \
    --lambda 0.991 \
    --enable-warmup \
    --warmup-obs-days 1 \
    --warmup-sim-days 9
```

**Output:**
```
Test Period:        2025-09-04 to 2025-10-16
Warmup:             1 days
Trading Period:     31 days

Performance:
  Final Equity:       $92470.98
  Total Return:       -7.53%
  MRD (Daily):        -0.24% per day

Trade Statistics:
  Total Trades:       3160
  Winning Trades:     8
  Losing Trades:      22
  Win Rate:           0.3%
  Profit Factor:      0.47

✅ Results exported to: results.json
```

**Results JSON (verified):**
```json
{
  "performance": {
    "final_equity": 92470.9765,
    "total_return": -0.0753,
    "mrd": -0.0024,
    "total_trades": 3160,
    "winning_trades": 8,
    "losing_trades": 22,
    "win_rate": 0.0025,
    "profit_factor": 0.4734
  }
}
```

### ❌ FAILING: Optuna Execution

**Command:**
```bash
python3 tools/optimize_warmup.py \
    --test-date 2025-10-17 \
    --n-trials 100 \
    --optimization-days 30 \
    --mode mock
```

**Output (every single trial):**
```
Trial 0: Score=-15.000 | Return=-100.00% | PF=0.00 | WR=0.0% | Trades=0
Trial 1: Score=-15.000 | Return=-100.00% | PF=0.00 | WR=0.0% | Trades=0
...
```

**Optimal Params JSON (shows failure):**
```json
{
  "parameters": {
    "max_positions": 4,
    "stop_loss": -0.011,
    "profit_target": 0.065,
    "lambda": 0.991,
    "enable_warmup": true,
    "warmup_obs_days": 1,
    "warmup_sim_days": 9
  },
  "metrics": {
    "total_return": -100.0,
    "profit_factor": 0.0,
    "win_rate": 0.0,
    "sharpe_proxy": -10.0,
    "total_trades": 0
  },
  "score": -15.0
}
```

---

## Investigation History

### Issue 1: JSON Export Not Working (RESOLVED ✅)

**Problem:** Results JSON was only exported when `generate_dashboard = true`, but optimization uses `--no-dashboard`.

**Root Cause:** `src/main.cpp:768-788`
```cpp
// BEFORE FIX (BROKEN):
if (config.generate_dashboard) {
    ResultsExporter::export_json(results, trader, config.results_file, ...);
    std::cout << "\n✅ Results exported to: " << config.results_file << "\n";
}
```

**Fix Applied:** `src/main.cpp:767-793` (committed at 02:17)
```cpp
// AFTER FIX (WORKING):
// Export results JSON (always, for optimization and analysis)
std::string symbols_str;
for (size_t i = 0; i < config.symbols.size(); ++i) {
    symbols_str += config.symbols[i];
    if (i < config.symbols.size() - 1) symbols_str += ",";
}

ResultsExporter::export_json(
    results, trader, config.results_file,
    symbols_str, "MOCK", start_for_export, end_for_export
);

if (!config.generate_dashboard) {
    std::cout << "\n✅ Results exported to: " << config.results_file << "\n";
}

// Export trades for dashboard (only if dashboard enabled)
if (config.generate_dashboard) {
    export_trades_jsonl(trader, "trades.jsonl");
    std::cout << "\n✅ Results exported to: " << config.results_file << "\n";
    std::cout << "✅ Trades exported to: trades.jsonl\n";
}
```

**Verification:**
```bash
# Binary rebuilt at 02:17
ls -lh build/sentio_lite
# -rwxr-xr-x  1 yeogirlyun  staff   380K Oct 18 02:17 ./build/sentio_lite

# Manual test confirms JSON export works
./build/sentio_lite mock --start-date 2025-09-04 --end-date 2025-10-16 \
    --warmup-days 1 --no-dashboard --results-file results.json ...
# ✅ Results exported to: results.json

cat results.json | head -5
# {
#   "metadata": { "timestamp": "2025-10-18 02:19:53", ... },
#   "performance": { "total_trades": 3160, ... }
# }
```

**Status:** ✅ FIXED - Results JSON now exports correctly with `--no-dashboard`

---

### Issue 2: Optuna Trials Return Zero Trades (CURRENT ISSUE ❌)

**Problem:** Despite JSON export fix, all Optuna trials fail to generate trades.

**Timeline:**
- **02:17** - Binary rebuilt with JSON export fix
- **02:19** - Manual test: ✅ 3160 trades, results.json created
- **02:22** - Optuna test (50 trials): ❌ All trials show 0 trades
- **02:27** - Optuna test (100 trials): ❌ All trials show 0 trades
- **02:40** - Manual test (same params): ✅ 3160 trades again

**Key Observation:** The **exact same command** works manually but fails through Optuna.

---

## Source Code Analysis

### 1. Optimization Script: `tools/optimize_warmup.py`

**File:** `tools/optimize_warmup.py` (458 lines)

#### Relevant Sections:

**Constructor (Lines 40-73):** Sets up optimization period
```python
def __init__(self, test_date: str, mode: str = "mock",
             optimization_days: int = 30, warmup_days: int = 1):
    self.test_date = datetime.strptime(test_date, "%Y-%m-%d")
    self.mode = mode
    self.optimization_days = optimization_days
    self.warmup_days = warmup_days

    # Calculate optimization period (before test date)
    self.opt_end_date = self.test_date - timedelta(days=1)
    # Add extra days to account for weekends (multiply by 1.4)
    calendar_days = int(optimization_days * 1.4)
    self.opt_start_date = self.opt_end_date - timedelta(days=calendar_days)

    # Ensure we don't go before available data (April 1, 2025)
    min_date = datetime(2025, 4, 1)
    if self.opt_start_date < min_date:
        self.opt_start_date = min_date
```

**For test_date = 2025-10-17:**
- opt_end_date = 2025-10-16
- opt_start_date = 2025-09-04 (after accounting for weekends)
- ✅ Dates are correct

**Backtest Execution (Lines 74-136):** Runs binary with parameters
```python
def run_backtest(self, params: Dict[str, Any]) -> Optional[Dict[str, float]]:
    cmd = [
        BINARY_PATH,  # "./build/sentio_lite"
        "mock",
        "--start-date", self.opt_start_date.strftime("%Y-%m-%d"),
        "--end-date", self.opt_end_date.strftime("%Y-%m-%d"),
        "--warmup-days", str(self.warmup_days),
        "--no-dashboard",
        "--results-file", RESULTS_FILE,  # "results.json"

        # Trading parameters
        "--capital", str(params["capital"]),
        "--max-positions", str(params["max_positions"]),
        "--stop-loss", str(params["stop_loss"]),
        "--profit-target", str(params["profit_target"]),
        "--lambda", str(params["lambda"]),
    ]

    # Add warmup parameters
    if params.get("enable_warmup", False):
        cmd.extend([
            "--enable-warmup",
            "--warmup-obs-days", str(params["warmup_obs_days"]),
            "--warmup-sim-days", str(params["warmup_sim_days"]),
        ])

    try:
        # Run backtest
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120
        )

        if result.returncode != 0:
            print(f"  ❌ Backtest failed: {result.stderr[:200]}")
            return None

        # Parse results from JSON file
        if os.path.exists(RESULTS_FILE):
            with open(RESULTS_FILE, 'r') as f:
                results = json.load(f)
            return results
        else:
            # Parse from stdout if JSON file not available
            return self._parse_output(result.stdout)
```

**Potential Issues:**
1. ⚠️ **Timeout:** 120 seconds might be too short for 30-day backtest with warmup
2. ⚠️ **File overwrite:** All trials write to same `results.json` - race condition?
3. ⚠️ **Error suppression:** If `returncode != 0`, returns None but doesn't show full error

**Objective Function (Lines 173-240):** Evaluates each trial
```python
def objective(self, trial: optuna.Trial) -> float:
    # Suggest parameters
    params = {
        "capital": 100000.0,  # Fixed
        "max_positions": trial.suggest_int("max_positions", 2, 8),
        "stop_loss": trial.suggest_float("stop_loss", -0.03, -0.01, step=0.001),
        "profit_target": trial.suggest_float("profit_target", 0.02, 0.08, step=0.005),
        "lambda": trial.suggest_float("lambda", 0.980, 0.998, step=0.001),
        "enable_warmup": trial.suggest_categorical("enable_warmup", [True, False]),
        "warmup_obs_days": trial.suggest_int("warmup_obs_days", 1, 5),
        "warmup_sim_days": trial.suggest_int("warmup_sim_days", 3, 10),
    }

    # Run backtest
    results = self.run_backtest(params)

    if results is None:
        return -1000.0  # Penalty for failed runs

    # Extract metrics
    total_return = results.get('total_return', -100.0)
    profit_factor = results.get('profit_factor', 0.0)
    win_rate = results.get('win_rate', 0.0)
    total_trades = results.get('total_trades', 0)
    mrd = results.get('mrd', -100.0)

    # Calculate Sharpe proxy
    sharpe_proxy = mrd if mrd > -50 else -10.0

    # Multi-objective score
    score = (
        sharpe_proxy * 0.5 +
        (profit_factor - 1.0) * 10 * 0.25 +
        (win_rate - 50) * 0.15 +
        (min(total_trades, 100) / 100) * 10 * 0.1
    )

    # Report
    trial.set_user_attr("total_return", total_return)
    trial.set_user_attr("profit_factor", profit_factor)
    trial.set_user_attr("win_rate", win_rate)
    trial.set_user_attr("sharpe_proxy", sharpe_proxy)
    trial.set_user_attr("total_trades", total_trades)

    print(f"  Trial {trial.number}: Score={score:.3f} | Return={total_return:.2f}% | "
          f"PF={profit_factor:.2f} | WR={win_rate:.1f}% | Trades={total_trades}")

    return score
```

**For zero trades:**
- total_return = -100.0 (default)
- profit_factor = 0.0 (default)
- win_rate = 0.0 (default)
- total_trades = 0
- sharpe_proxy = -10.0 (since mrd would be -100.0)
- **score = -10.0 * 0.5 + (0.0 - 1.0) * 10 * 0.25 + (0.0 - 50) * 0.15 + 0 = -5.0 - 2.5 - 7.5 = -15.0** ✅ Matches observed value!

**This confirms:** The penalty score is being calculated correctly, meaning `results = None` or metrics are defaults.

**Optimization Runner (Lines 242-281):**
```python
def optimize(self, n_trials: int = 100, n_jobs: int = 1) -> optuna.Study:
    print(f"\n🔍 Starting optimization with {n_trials} trials...")
    print(f"=" * 70)

    study = optuna.create_study(
        direction="maximize",
        sampler=TPESampler(seed=42),
        pruner=MedianPruner(n_startup_trials=10, n_warmup_steps=20)
    )

    study.optimize(
        self.objective,
        n_trials=n_trials,
        n_jobs=n_jobs,  # Default: 1 (sequential)
        show_progress_bar=True
    )
```

**Note:** `n_jobs=1` means trials run sequentially, not in parallel. No race condition on results.json.

---

### 2. C++ Binary: `src/main.cpp`

**File:** `src/main.cpp` (lines 767-793 relevant)

**JSON Export (Fixed Section):**
```cpp
// Export results JSON (always, for optimization and analysis)
std::string symbols_str;
for (size_t i = 0; i < config.symbols.size(); ++i) {
    symbols_str += config.symbols[i];
    if (i < config.symbols.size() - 1) symbols_str += ",";
}

std::string start_for_export = is_multi_day ? config.start_date : test_date;
std::string end_for_export = is_multi_day ? config.end_date : test_date;

ResultsExporter::export_json(
    results, trader, config.results_file,
    symbols_str, "MOCK",
    start_for_export, end_for_export
);

if (!config.generate_dashboard) {
    // Only show export confirmation when dashboard is disabled
    std::cout << "\n✅ Results exported to: " << config.results_file << "\n";
}

// Export trades for dashboard (only if dashboard enabled)
if (config.generate_dashboard) {
    export_trades_jsonl(trader, "trades.jsonl");
    std::cout << "\n✅ Results exported to: " << config.results_file << "\n";
    std::cout << "✅ Trades exported to: trades.jsonl\n";
}
```

**Status:** ✅ This section works correctly (verified with manual test)

---

### 3. Warmup System: `include/trading/multi_symbol_trader.h`

**File:** `include/trading/multi_symbol_trader.h` (lines 67-83)

**Warmup Configuration:**
```cpp
// Warmup configuration for improved pre-live validation
struct WarmupConfig {
    bool enabled = false;                    // Enable warmup phase
    int observation_days = 2;                // Learn without trading
    int simulation_days = 5;                 // Paper trade before live

    // Go-live criteria (evaluated after simulation)
    // NOTE: Relaxed for testing approval logic. Restore to strict for production:
    //   min_sharpe_ratio = 0.3, max_drawdown = 0.15, require_positive_return = true
    double min_sharpe_ratio = -2.0;          // TESTING: Very lenient (-2.0, restore to 0.3)
    double max_drawdown = 0.30;              // TESTING: Lenient (30%, restore to 15%)
    int min_trades = 20;                     // Minimum trades to evaluate
    bool require_positive_return = false;    // TESTING: Allow negative (restore to true)

    // State preservation
    bool preserve_predictor_state = true;    // Keep EWRLS weights
    bool preserve_trade_history = true;      // Keep trade history for sizing
    double history_decay_factor = 0.7;       // Weight historical trades at 70%
} warmup;
```

**Phase Management:** `src/trading/multi_symbol_trader.cpp:1026-1196`

**Potential Issue:** When warmup is enabled:
- Observation phase: NO trades (learning only)
- Simulation phase: Paper trades (don't affect equity)
- Only after passing go-live criteria → LIVE trades

**If warmup never completes → Zero trades!**

---

## Hypothesis: Root Causes

### Hypothesis 1: Warmup System Blocks All Trading ⭐ **MOST LIKELY**

**Theory:** When `--enable-warmup` is used during optimization, the warmup system might:
1. Enter observation phase (no trades)
2. Enter simulation phase (paper trades)
3. **Fail go-live criteria** (e.g., not enough sim trades, Sharpe too low)
4. Never transition to LIVE trading
5. Result: 0 real trades in results

**Evidence:**
- Manual test WITH warmup: Shows observation/simulation phases in output
- Optimization test: No visibility into warmup phase progression
- Default warmup params in trials: `warmup_obs_days: 1, warmup_sim_days: 9`
  - 1 day obs + 9 days sim = 10 days warmup
  - **Only 21 days left for actual trading!** (out of 31-day period)

**Test This:**
```bash
# Run optimization WITHOUT warmup
python3 tools/optimize_warmup.py \
    --test-date 2025-10-17 \
    --n-trials 10 \
    --mode mock
# But force enable_warmup = False in objective function
```

### Hypothesis 2: Timeout Issues

**Theory:** 120-second timeout might be insufficient for:
- 30-day backtest
- 10 symbols
- Warmup system (observation + simulation)
- Verbose output capture

**Evidence:**
- Manual test took ~2.2 seconds (fast, so timeout unlikely)
- But if subprocess.run() times out, it kills process and returns None

**Counter-evidence:**
- We would see timeout messages in output
- Not observed

### Hypothesis 3: File Parsing Issues

**Theory:** Results JSON might be malformed or incomplete when Optuna reads it.

**Evidence:**
- Manual inspection shows correct JSON format
- Python json.load() would raise exception if malformed
- No exceptions observed

**Counter-evidence:**
- Manual test creates valid JSON
- Same binary, same command

### Hypothesis 4: Working Directory / Path Issues

**Theory:** Optuna might run from different directory, causing:
- Binary path resolution to fail
- Data files not found
- Results file written to wrong location

**Evidence:**
- BINARY_PATH = "./build/sentio_lite" (relative path)
- Data loaded from `data/` (relative path)
- If cwd is wrong, binary would fail

**Test This:**
```python
# In run_backtest(), add:
print(f"  CWD: {os.getcwd()}")
print(f"  Binary exists: {os.path.exists(BINARY_PATH)}")
```

### Hypothesis 5: Return Code Masking Errors

**Theory:** Binary returns non-zero exit code, but error message is lost.

**Evidence:**
```python
if result.returncode != 0:
    print(f"  ❌ Backtest failed: {result.stderr[:200]}")
    return None
```
Only prints first 200 chars of stderr, might hide critical info.

**Test This:**
```python
# Print full stderr and stdout
print(f"  STDOUT: {result.stdout}")
print(f"  STDERR: {result.stderr}")
print(f"  Return code: {result.returncode}")
```

---

## Proposed Solutions

### Solution 1: Debug Logging in Optuna Script (IMMEDIATE)

**Modify `tools/optimize_warmup.py:74-136`:**

```python
def run_backtest(self, params: Dict[str, Any]) -> Optional[Dict[str, float]]:
    # BUILD COMMAND
    cmd = [...]

    # DEBUG: Print command and environment
    print(f"  🔧 DEBUG: CWD = {os.getcwd()}")
    print(f"  🔧 DEBUG: Binary exists = {os.path.exists(BINARY_PATH)}")
    print(f"  🔧 DEBUG: Command = {' '.join(cmd)}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)

        # DEBUG: Print full output
        print(f"  🔧 DEBUG: Return code = {result.returncode}")
        if result.returncode != 0:
            print(f"  🔧 DEBUG: STDERR = {result.stderr}")
            print(f"  🔧 DEBUG: STDOUT = {result.stdout}")
            return None

        # DEBUG: Check if results file exists
        print(f"  🔧 DEBUG: results.json exists = {os.path.exists(RESULTS_FILE)}")

        if os.path.exists(RESULTS_FILE):
            with open(RESULTS_FILE, 'r') as f:
                results = json.load(f)

            # DEBUG: Print parsed results
            print(f"  🔧 DEBUG: Parsed total_trades = {results.get('performance', {}).get('total_trades', 'MISSING')}")

            return results.get('performance', {})  # Return performance dict
        else:
            print(f"  🔧 DEBUG: results.json NOT FOUND, falling back to stdout parsing")
            return self._parse_output(result.stdout)

    except subprocess.TimeoutExpired:
        print(f"  ⏱️  Backtest timeout after 120 seconds")
        print(f"  🔧 DEBUG: This might indicate warmup is blocking")
        return None
```

### Solution 2: Disable Warmup During Optimization

**Rationale:** Warmup system might interfere with parameter exploration. Optimize trading params first, then add warmup later.

**Modify `tools/optimize_warmup.py:183-198`:**

```python
def objective(self, trial: optuna.Trial) -> float:
    params = {
        "capital": 100000.0,
        "max_positions": trial.suggest_int("max_positions", 2, 8),
        "stop_loss": trial.suggest_float("stop_loss", -0.03, -0.01, step=0.001),
        "profit_target": trial.suggest_float("profit_target", 0.02, 0.08, step=0.005),
        "lambda": trial.suggest_float("lambda", 0.980, 0.998, step=0.001),

        # TEMPORARILY DISABLE WARMUP FOR OPTIMIZATION
        "enable_warmup": False,
        # "enable_warmup": trial.suggest_categorical("enable_warmup", [True, False]),
        # "warmup_obs_days": trial.suggest_int("warmup_obs_days", 1, 5),
        # "warmup_sim_days": trial.suggest_int("warmup_sim_days", 3, 10),
    }
```

### Solution 3: Fix JSON Parsing

**Issue:** `results` is the full JSON dict, but we're accessing it incorrectly.

**Current code (Lines 207-211):**
```python
total_return = results.get('total_return', -100.0)
profit_factor = results.get('profit_factor', 0.0)
win_rate = results.get('win_rate', 0.0)
total_trades = results.get('total_trades', 0)
mrd = results.get('mrd', -100.0)
```

**Actual JSON structure:**
```json
{
  "metadata": { ... },
  "performance": {
    "total_return": -0.0753,
    "profit_factor": 0.4734,
    "win_rate": 0.0025,
    "total_trades": 3160,
    "mrd": -0.0024
  },
  "config": { ... }
}
```

**Fix:**
```python
# Extract from 'performance' sub-dict
perf = results.get('performance', {})
total_return = perf.get('total_return', -100.0) * 100  # Convert to percentage
profit_factor = perf.get('profit_factor', 0.0)
win_rate = perf.get('win_rate', 0.0) * 100  # Convert to percentage
total_trades = perf.get('total_trades', 0)
mrd = perf.get('mrd', -100.0) * 100  # Convert to percentage
```

### Solution 4: Increase Timeout

**Change from 120s to 600s (10 minutes):**

```python
result = subprocess.run(
    cmd,
    capture_output=True,
    text=True,
    timeout=600  # 10 minutes
)
```

---

## Testing Plan

### Test 1: Verify JSON Parsing Fix
```bash
# Run single trial with debug output
python3 tools/optimize_warmup.py \
    --test-date 2025-10-17 \
    --n-trials 1 \
    --mode mock

# Expected: See full debug output, identify exact failure point
```

### Test 2: Test Without Warmup
```bash
# Modify optimize_warmup.py to set enable_warmup = False
# Run 10 trials
python3 tools/optimize_warmup.py \
    --test-date 2025-10-17 \
    --n-trials 10 \
    --mode mock

# Expected: Trades > 0 if warmup was the issue
```

### Test 3: Manual Reproduction
```bash
# Extract exact command from trial 0 debug output
# Run manually
./build/sentio_lite mock --start-date 2025-09-04 --end-date 2025-10-16 \
    [exact parameters from trial 0]

# Compare results
```

---

## Current Status

**Manual Execution:**
- ✅ Binary works correctly
- ✅ JSON export works
- ✅ Results: 3160 trades, -7.53% return on Sept 4-Oct 16
- ✅ Test on Oct 17: 104 trades, -0.30% MRD

**Automated Optimization:**
- ❌ All 100 trials: 0 trades
- ❌ Optimization unusable
- ⚠️ Root cause unknown (multiple hypotheses)

---

## Workarounds (Temporary)

### Option A: Use Default Parameters for Now
```bash
# Skip optimization, use manually-tuned defaults
./build/sentio_lite mock --date 2025-10-17 --warmup-days 1 --no-dashboard

# Result: -0.30% MRD (poor but functional)
```

### Option B: Manual Parameter Sweep
```bash
# Test discrete parameter combinations manually
for max_pos in 2 4 6; do
  for stop in -0.01 -0.015 -0.02; do
    for target in 0.03 0.05 0.07; do
      ./build/sentio_lite mock --date 2025-10-17 \
        --max-positions $max_pos \
        --stop-loss $stop \
        --profit-target $target \
        --results-file "results_${max_pos}_${stop}_${target}.json"
    done
  done
done

# Analyze all results manually
```

### Option C: Simplify Optuna Script
```python
# Remove warmup optimization entirely
# Only optimize: max_positions, stop_loss, profit_target, lambda
# Hardcode: enable_warmup = False
```

---

## References

### Modified Files (This Session)

1. **`src/main.cpp:767-793`**
   - Fixed JSON export to always run (regardless of `--no-dashboard`)
   - Commit time: 02:17
   - Status: ✅ WORKING

### Relevant Source Files

2. **`tools/optimize_warmup.py`**
   - Lines 40-73: Constructor, date calculation
   - Lines 74-136: `run_backtest()` - subprocess execution
   - Lines 138-171: `_parse_output()` - stdout parsing (fallback)
   - Lines 173-240: `objective()` - Optuna objective function
   - Lines 242-281: `optimize()` - Optuna study runner
   - Lines 283-301: `save_best_params()` - JSON persistence
   - Lines 303-364: `run_final_test()` - test with optimal params
   - Lines 367-457: `main()` - CLI entry point

3. **`include/trading/multi_symbol_trader.h`**
   - Lines 67-83: `WarmupConfig` struct
   - Lines 85-91: `Phase` enum (OBSERVATION, SIMULATION, COMPLETE, LIVE)
   - Lines 175-206: `SimulationMetrics` struct

4. **`src/trading/multi_symbol_trader.cpp`**
   - Lines 249-265: `on_bar()` - phase routing
   - Lines 1026-1152: `update_phase()` - phase transitions
   - Lines 1059-1096: `evaluate_warmup_complete()` - go-live criteria

5. **`scripts/optimize_for_tomorrow.sh`**
   - Automation wrapper for daily optimization
   - Lines 62-67: Calls `optimize_warmup.py`

6. **`scripts/launch_live.sh`**
   - Deployment script for optimized parameters
   - Lines 36-47: Extracts params from JSON
   - Lines 84-101: Executes binary with params

### Data Files

7. **`data/*.bin`**
   - Binary market data (10 symbols)
   - Date range: April 1 - October 18, 2025 (139 days)
   - Symbols: TQQQ, SQQQ, SSO, SDS, TNA, TZA, UVXY, SVIX, SOXS, SOXL

### Documentation

8. **`OPTUNA_OPTIMIZATION_GUIDE.md`** (25KB)
   - Complete usage guide
   - Parameter search space
   - Production workflow

9. **`OPTUNA_IMPLEMENTATION_COMPLETE.md`** (15KB)
   - Implementation summary
   - Architecture overview
   - Validation results

10. **`WARMUP_SYSTEM_IMPLEMENTATION.md`**
    - Warmup phase design
    - Go-live criteria
    - State preservation

---

## Next Steps (Recommended Priority)

### Priority 1: Debug Logging (30 minutes)
1. Add comprehensive debug output to `optimize_warmup.py`
2. Run single trial, capture all output
3. Identify exact failure point

### Priority 2: Fix JSON Parsing (15 minutes)
1. Update `objective()` to access `results['performance']`
2. Test with manual JSON file
3. Verify metrics extraction

### Priority 3: Test Without Warmup (15 minutes)
1. Disable warmup in optimization
2. Run 10 trials
3. Confirm if warmup is blocking trades

### Priority 4: Comprehensive Fix (1-2 hours)
1. Implement all fixes identified in debugging
2. Run 100-trial optimization
3. Verify all trials generate trades
4. Test on Oct 17, report MRD

---

## Appendix: Environment Information

**System:**
- macOS 24.6.0 (Darwin)
- Python 3.x
- Optuna 3.x
- C++17 compiler

**Binary:**
- Path: `./build/sentio_lite`
- Built: 2025-10-18 02:17
- Size: 380KB

**Working Directory:**
- `/Volumes/ExternalSSD/Dev/C++/sentio_lite`

**Available Data:**
- 139 days (April 1 - October 18, 2025)
- 10 symbols, ~54,349 bars each
- Binary format (.bin files)

---

**Report End**
