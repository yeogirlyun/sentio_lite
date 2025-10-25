# Warmup Mode Comparison Experiment

## Quick Start

### 1. Demo Both Warmup Modes (2 minutes)
```bash
./scripts/demo_warmup_modes.sh
```

This runs SIGOR on Oct 24 with both warmup modes to show the immediate differences.

### 2. Run Full Optimization Comparison (2-4 hours)
```bash
./scripts/compare_warmup_modes.sh
```

This runs **200 trials** of Optuna optimization for each warmup mode and compares the results.

## What's Being Tested?

### Prev-Day Warmup (Historical Default)
- **Warmup Source**: Last 60 bars of **previous trading day**
- **Example**: For Oct 24 test, warmup on bars 332-391 of Oct 23
- **Trading Window**: Full test day (bars 1-391)
- **Hypothesis**: Mature indicators from prior day provide better signals

### Intraday Warmup (Fresh Start)
- **Warmup Source**: First 60 bars of **test day itself**
- **Example**: For Oct 24 test, warmup on bars 1-60 of Oct 24
- **Trading Window**: Reduced test day (bars 61-391)
- **Hypothesis**: Fresh indicators adapting to current day perform better

## Implementation

### Bug Fix Applied
**File**: `src/main.cpp:242-248`

**Before** (BROKEN):
```cpp
// Rule-based: disable warmup/simulation
config.warmup_bars_specified = 0;      // ❌ Overrides --warmup-bars flag!
config.intraday_warmup = true;         // ❌ Forces intraday mode!
```

**After** (FIXED):
```cpp
// Rule-based: disable learning/simulation warmup (but allow detector warmup bars)
config.trading.min_bars_to_learn = 0;
config.trading.warmup.enabled = false;
// NOTE: warmup_bars_specified and intraday_warmup are NOT overridden
// They can be controlled via --warmup-bars and --intraday-warmup flags
```

This fix allows the optimization script to control warmup mode via command-line arguments.

### Python Script Interface
**File**: `scripts/optimize_sigor_combined.py`

**New Arguments**:
```python
--warmup-mode {prevday,intraday}  # Warmup source selection
--warmup-bars N                   # Number of warmup bars (default: 60)
```

**Example Usage**:
```bash
# Prev-day warmup
python3 scripts/optimize_sigor_combined.py \
    --end-date 10-24 --trials 200 \
    --warmup-mode prevday --warmup-bars 60

# Intraday warmup
python3 scripts/optimize_sigor_combined.py \
    --end-date 10-24 --trials 200 \
    --warmup-mode intraday --warmup-bars 60
```

## Expected Outcomes

### Scenario 1: Prev-Day Wins
**Insight**: SIGOR benefits from mature indicators at market open
- Detectors need warmup period to stabilize
- Previous day's momentum carries into next day
- Opening hour trades are valuable

**Action**: Keep prev-day warmup as default

### Scenario 2: Intraday Wins
**Insight**: SIGOR performs better adapting to fresh conditions
- Each day should be treated independently
- Opening hour is not profitable anyway
- Fresh indicators reduce overfitting

**Action**: Switch to intraday warmup as default

### Scenario 3: Similar Performance
**Insight**: Warmup mode is not a critical factor
- SIGOR's 8-detector ensemble is robust
- Focus optimization on weights/windows instead
- Keep current default (prev-day)

## Results Files

After running `./scripts/compare_warmup_modes.sh`:

```
results/warmup_comparison/
├── prevday_warmup_results.json          # Full Optuna results (prev-day)
├── intraday_warmup_results.json         # Full Optuna results (intraday)
├── prevday_warmup_200trials.log         # Console output (prev-day)
└── intraday_warmup_200trials.log        # Console output (intraday)
```

## Analysis Metrics

The comparison script automatically calculates:

1. **Best Evaluation MRD**: Performance on 5 test days (Oct 18-24)
2. **Best Validation MRD**: Performance on 10 prior days
3. **MRD Difference**: Intraday - Prevday (positive = intraday wins)
4. **Winner Declaration**: Based on evaluation MRD

## Next Steps

1. Run the full comparison experiment
2. Analyze parameter differences between winners
3. Test on additional date ranges for robustness
4. Update production config with winning mode
5. Document decision in `config/sigor_params.json`

## Files Modified

1. `src/main.cpp` - Fixed warmup flag override bug
2. `scripts/optimize_sigor_combined.py` - Added warmup mode arguments
3. `scripts/compare_warmup_modes.sh` - Comparison runner (NEW)
4. `scripts/demo_warmup_modes.sh` - Quick demo (NEW)
5. `docs/WARMUP_MODE_EXPERIMENT.md` - Detailed experiment plan (NEW)

## Historical Context

All previous SIGOR optimizations (including the 2000-trial production winner) used **hardcoded intraday warmup** due to the bug in `main.cpp`. This experiment is the first fair comparison between warmup modes.

## Technical Details

### C++ Engine Flags
```bash
--warmup-bars N        # Number of warmup bars (default: 100)
--intraday-warmup      # Flag to enable intraday mode (default: prev-day)
```

### Warmup Bar Calculation
- **Prev-day**: Warmup ends at bar 391 of previous day (4:00 PM)
- **Intraday**: Warmup on bars 1-N of test day, trading starts at bar N+1

### MRD Comparability
Both modes calculate MRD on the **same time window**:
- **Prev-day**: MRD across all 391 bars of test day
- **Intraday**: MRD across bars 61-391 of test day (scaled to full day)

This ensures fair comparison despite different trading windows.
