# Integration Plan: Base Implementation + sentio_lite + online_trader

## Component Analysis

### User's Base Implementation ✅
**Strengths:**
- Complete trading system end-to-end
- CircularBuffer (efficient, cache-friendly)
- MathUtils (clean statistics utilities)
- MultiSymbolTrader (complete trading logic)
- Position management with stop-loss/profit targets
- Adaptive position sizing based on trade history
- EOD liquidation logic
- CSV data loader
- Working main.cpp

**Specifications:**
- 10 features (momentum, volatility, volume, RSI-like)
- EWRLS predictor (lambda=0.98)
- Max 3 concurrent positions
- 2% stop-loss, 5% profit target
- 100-bar warmup period
- Daily reset (390 bars/day)

### Existing sentio_lite ✅
**Strengths:**
- UnifiedFeatures with **25 indicators** (more comprehensive)
- More sophisticated feature engineering
- Better organized namespace structure
- Production-ready feature calculation

**Features include:**
- Multiple momentum timeframes
- Advanced volatility (ATR, realized vol)
- Volume analysis
- Price position indicators
- RSI
- Interaction terms

### online_trader Enhancements 📚
**Available for integration:**
- Binary data format (.bin) - much faster than CSV
- Multi-symbol data manager
- Configuration system (JSON)
- Comprehensive logging
- More advanced backend components
- Live trading support (if needed)

---

## Integration Strategy

### Phase 1: Core Utilities (MERGE)
**Action:** Integrate best utilities from both

1. **MathUtils** (from base) → `include/core/math_utils.h`
   - Keep as-is, very useful

2. **CircularBuffer** (from base) → `include/utils/circular_buffer.h`
   - More efficient than std::deque
   - Replace deque in UnifiedFeatures

3. **Types & Bar** (already compatible)
   - Keep existing types.h/bar.h
   - Add timestamp_ms helpers if missing

### Phase 2: Features (ENHANCE base with sentio_lite)
**Action:** Use UnifiedFeatures (25 indicators) instead of FeatureExtractor (10)

**Approach:**
1. Keep base's FeatureExtractor interface
2. Replace implementation with UnifiedFeatures logic
3. Expand from 10 → 25 features
4. Use CircularBuffer instead of deque for efficiency

**Result:** Best of both worlds
- Base's clean interface
- sentio_lite's comprehensive features
- Better performance with CircularBuffer

### Phase 3: Predictor (MERGE)
**Action:** Merge OnlinePredictor with EWRLSPredictor

**Both implementations are nearly identical:**
- EWRLS algorithm (same)
- Lambda forgetting factor (same)
- Update equations (same)

**Differences:**
- Base: `OnlinePredictor` with 10 features default
- sentio_lite: `EWRLSPredictor` with 25 features default

**Decision:** Use base's OnlinePredictor, update default to 25 features

### Phase 4: Trading Logic (FROM base)
**Action:** Integrate MultiSymbolTrader as-is with minor enhancements

**Components to add:**
1. **Position** (from base) → `include/trading/position.h`
2. **TradeHistory** (from base) → `include/trading/trade_history.h`
3. **MultiSymbolTrader** (from base) → `include/trading/multi_symbol_trader.h`

**Enhancements:**
- Add logging (simple cout for now, can upgrade later)
- Add configuration struct instead of hardcoded constants
- Keep all the trading logic (stop-loss, profit targets, adaptive sizing, EOD)

### Phase 5: Data Loading (ADD from online_trader)
**Action:** Add binary data reader for performance

**Implementation:**
1. Copy `data_io.cpp` from online_trader (binary format)
2. Keep CSV loader as fallback
3. Auto-detect format based on file extension

**Benefits:**
- 10-100x faster than CSV
- Smaller file sizes
- Used by all online_trader tools/scripts

### Phase 6: Main Application (CREATE)
**Action:** Create comprehensive main.cpp

**Features:**
1. Command-line argument parsing
2. Multi-symbol support
3. Both CSV and binary data loading
4. Configuration via command line
5. Real-time progress updates
6. Detailed results output

---

## File Structure After Integration

```
sentio_lite/
├── include/
│   ├── core/
│   │   ├── types.h              ✅ Existing
│   │   ├── bar.h                ✅ Existing
│   │   └── math_utils.h         ➕ From base
│   ├── utils/
│   │   ├── circular_buffer.h    ➕ From base
│   │   └── data_io.h            ➕ From online_trader
│   ├── predictor/
│   │   ├── online_predictor.h   ➕ From base (renamed from EWRLS)
│   │   └── feature_extractor.h  🔧 Enhanced (25 features)
│   └── trading/
│       ├── position.h           ➕ From base
│       ├── trade_history.h      ➕ From base
│       └── multi_symbol_trader.h ➕ From base
├── src/
│   ├── core/
│   │   └── data_io.cpp          ➕ From online_trader
│   ├── predictor/
│   │   ├── online_predictor.cpp ➕ From base
│   │   └── feature_extractor.cpp 🔧 Enhanced
│   ├── trading/
│   │   └── multi_symbol_trader.cpp ➕ From base
│   └── main.cpp                  ➕ New (comprehensive)
├── config/
│   └── default_config.json      ➕ New
├── CMakeLists.txt               🔧 Updated
└── [docs, tools, scripts...]    ✅ Already migrated
```

---

## Implementation Steps

### Step 1: Add Utilities
- [x] Copy math_utils.h from base
- [ ] Copy circular_buffer.h from base
- [ ] Verify compilation

### Step 2: Enhance Features
- [ ] Update feature_extractor to use CircularBuffer
- [ ] Expand features from 10 → 25
- [ ] Keep base's interface, enhance implementation
- [ ] Test feature extraction

### Step 3: Add Predictor
- [ ] Copy online_predictor.h/cpp from base
- [ ] Update default features to 25
- [ ] Verify EWRLS equations
- [ ] Test predictions

### Step 4: Add Trading Components
- [ ] Add position.h
- [ ] Add trade_history.h
- [ ] Add multi_symbol_trader.h/cpp
- [ ] Update to use 25 features
- [ ] Test trading logic

### Step 5: Add Data I/O
- [ ] Copy binary data reader from online_trader
- [ ] Add CSV loader from base
- [ ] Create unified data loading interface
- [ ] Test both formats

### Step 6: Create Main Application
- [ ] Implement main.cpp with argument parsing
- [ ] Support both CSV and binary
- [ ] Add progress reporting
- [ ] Add results summary
- [ ] Test end-to-end

### Step 7: Configuration
- [ ] Create config structure
- [ ] Add JSON config file (optional)
- [ ] Command-line overrides
- [ ] Test configuration

### Step 8: Build & Test
- [ ] Update CMakeLists.txt
- [ ] Build project
- [ ] Run with sample data
- [ ] Validate results

---

## Configuration Parameters

### Trading Parameters (from base, will be configurable)
```cpp
struct TradingConfig {
    // Capital
    double initial_capital = 100000.0;

    // Position management
    size_t max_positions = 3;
    double stop_loss_pct = -0.02;      // -2%
    double profit_target_pct = 0.05;   // 5%

    // Learning
    size_t min_bars_to_learn = 100;
    size_t lookback_window = 50;       // Feature extraction
    double lambda = 0.98;              // EWRLS forgetting factor

    // Execution
    int bars_per_day = 390;            // Market hours
    bool eod_liquidation = true;       // Close all positions EOD

    // Position sizing
    double capital_per_position = 0.95 / max_positions;
    double win_multiplier = 1.3;       // After consecutive wins
    double loss_multiplier = 0.7;      // After consecutive losses
};
```

---

## Key Enhancements Over Base

### 1. Better Features (10 → 25)
- More momentum timeframes
- Advanced volatility (ATR)
- Better volume analysis
- More interaction terms

### 2. Binary Data Support
- 10-100x faster loading
- Compatible with online_trader tools
- Smaller files

### 3. Configuration System
- No hardcoded constants
- Easy parameter tuning
- Reproducible experiments

### 4. Better Code Organization
- Clear namespaces
- Modular design
- Easy to extend

### 5. Integration with online_trader Tools
- Use existing optimization scripts
- Compatible data format
- Can leverage dashboards

---

## Backwards Compatibility

### With Base Implementation
✅ Same trading logic
✅ Same predictor algorithm
✅ Enhanced features (10 → 25) but compatible
✅ CSV loading still supported

### With online_trader
✅ Binary data format compatible
✅ Can use online_trader tools
✅ Can reference full implementation
✅ Same PROJECT_RULES.md compliance

---

## Testing Strategy

### Unit Tests (Future)
1. Feature extraction accuracy
2. Predictor convergence
3. Position management logic
4. Data loading (CSV & binary)

### Integration Tests
1. End-to-end backtest with sample data
2. Multi-symbol coordination
3. EOD liquidation
4. Adaptive sizing

### Performance Tests
1. Feature extraction speed
2. Data loading speed (CSV vs binary)
3. Overall throughput
4. Memory usage

---

## Next Steps

1. **Immediate:** Start with Step 1 (Add Utilities)
2. **Today:** Complete Steps 1-4 (core components)
3. **Next:** Steps 5-8 (data I/O, main, testing)

---

## Expected Outcome

**A minimal but production-ready trading system with:**
- ✅ Proven trading logic from base implementation
- ✅ Enhanced features (25 indicators) from sentio_lite
- ✅ High-performance data loading from online_trader
- ✅ Clean, modular, extensible architecture
- ✅ Full compatibility with online_trader tools ecosystem
- ✅ Easy to understand and maintain (~2000 LOC)

**Performance targets:**
- Load 1 year of data: <1 second (binary), <10 seconds (CSV)
- Process 1M bars: <5 seconds
- Memory usage: <100MB for typical backtest
- Feature extraction: <10μs per bar

---

**Status:** Ready to begin implementation
**Estimated time:** 2-3 hours for complete integration
**Risk:** Low (all components proven individually)
