# 10 Recommended Symbols for Sentio Lite

## Overview

Sentio Lite uses **10 carefully selected leveraged ETFs** for multi-symbol rotation trading. These symbols provide:
- ✅ Broad market coverage (Nasdaq, S&P 500, Russell 2000, Finance, VIX)
- ✅ Both bull and bear exposure
- ✅ High liquidity and volume
- ✅ Optimal risk/reward balance (2x-3x leverage)

---

## The 10 Symbols

### 1-2. Nasdaq-100 (QQQ) - 3x Leverage

**TQQQ** - ProShares UltraPro QQQ (3x Bull)
- 3x daily performance of Nasdaq-100
- Tech-heavy exposure
- Very high volume, liquid

**SQQQ** - ProShares UltraPro Short QQQ (3x Bear)
- 3x inverse daily performance of Nasdaq-100
- Profits when tech falls
- Perfect hedge to TQQQ

### 3-4. S&P 500 (SPY) - 2x Leverage ⭐ RECOMMENDED

**SSO** - ProShares Ultra S&P 500 (2x Bull)
- 2x daily performance of S&P 500
- **Lower volatility than 3x (UPRO)**
- Better for rotation strategies
- More stable than 3x alternatives

**SDS** - ProShares UltraShort S&P 500 (2x Bear)
- 2x inverse daily performance of S&P 500
- **Lower volatility than 3x (SPXS)**
- Better risk management
- Smoother equity curve

**Why 2x instead of 3x?**
- Lower decay from volatility
- More consistent performance
- Better for daily rotation
- Reduced drawdown risk

### 5-6. Russell 2000 (IWM) - 3x Leverage

**TNA** - Direxion Daily Small Cap Bull 3x
- 3x daily performance of Russell 2000
- Small-cap exposure
- High volatility, high potential

**TZA** - Direxion Daily Small Cap Bear 3x
- 3x inverse Russell 2000
- Profits from small-cap weakness
- Diversification from large-cap

### 7-8. Financial Sector (XLF) - 3x Leverage

**FAS** - Direxion Daily Financial Bull 3x
- 3x daily performance of financial sector
- Banks, brokers, insurance
- Interest rate sensitivity

**FAZ** - Direxion Daily Financial Bear 3x
- 3x inverse financial sector
- Profits from financial weakness
- Sector rotation opportunity

### 9-10. Volatility (VIX)

**UVXY** - ProShares Ultra VIX Short-Term Futures (1.5x)
- 1.5x VIX futures performance
- Spikes during market stress
- Crisis alpha

**SVXY** - ProShares Short VIX Short-Term Futures (-0.5x)
- Inverse VIX exposure
- Profits from low volatility
- Mean reversion play

---

## Comparison: 10 vs 12 vs 14 Symbols

### 10 Symbols (Recommended) ⭐

**TQQQ, SQQQ, SSO, SDS, TNA, TZA, FAS, FAZ, UVXY, SVXY**

✅ **Pros:**
- Optimal balance of diversity and manageability
- Uses 2x SPY (more stable)
- Lower overall portfolio volatility
- Better risk-adjusted returns
- Easier to track and monitor
- Lower computational overhead

❌ **Cons:**
- Slightly less diversification than 12/14
- Missing energy and gold miners sectors

### 12 Symbols (Original)

**Add: UPRO, SPXS** (3x SPY bull/bear)

✅ **Pros:**
- More S&P 500 leverage options
- Can choose 2x or 3x based on market

❌ **Cons:**
- UPRO/SPXS have higher volatility
- More decay from 3x leverage
- Overlaps with SSO/SDS (same underlying)
- Increased complexity without much benefit

### 14 Symbols (Maximum)

**Add: UPRO, SPXS, ERX, ERY, NUGT, DUST**

✅ **Pros:**
- Maximum diversification
- Energy sector exposure (ERX/ERY)
- Gold miners exposure (NUGT/DUST)
- More rotation opportunities

❌ **Cons:**
- Too many symbols for most strategies
- Increased computational cost
- Some symbols less liquid
- Harder to manage and monitor
- Diminishing returns

---

## Why These 10 Are Optimal

### 1. Market Coverage

- **Tech:** TQQQ/SQQQ (Nasdaq)
- **Large Cap:** SSO/SDS (S&P 500)
- **Small Cap:** TNA/TZA (Russell)
- **Sector:** FAS/FAZ (Finance)
- **Volatility:** UVXY/SVXY (VIX)

### 2. Risk Management

- Uses **2x SPY** instead of 3x for better stability
- Balanced leverage across sectors
- Natural hedges (bull/bear pairs)
- Volatility protection (UVXY/SVXY)

### 3. Liquidity

All 10 symbols have:
- ✅ High daily volume (>$100M+)
- ✅ Tight bid-ask spreads
- ✅ Deep order books
- ✅ Easy entry/exit

### 4. Rotation Efficiency

- Distinct market segments
- Low correlation between some pairs
- Clear trend signals
- Good for top-N selection

---

## Performance Characteristics

### Volatility (Annualized)

| Symbol | Type | Volatility | Risk Level |
|--------|------|------------|------------|
| SSO | 2x SPY Bull | ~30-40% | Medium |
| SDS | 2x SPY Bear | ~30-40% | Medium |
| TQQQ | 3x QQQ Bull | ~60-80% | High |
| SQQQ | 3x QQQ Bear | ~60-80% | High |
| TNA | 3x IWM Bull | ~70-90% | Very High |
| TZA | 3x IWM Bear | ~70-90% | Very High |
| FAS | 3x XLF Bull | ~60-80% | High |
| FAZ | 3x XLF Bear | ~60-80% | High |
| UVXY | 1.5x VIX | ~80-100%+ | Extreme |
| SVXY | -0.5x VIX | ~40-60% | High |

### Average Daily Volume

| Symbol | Avg Volume | Liquidity |
|--------|-----------|-----------|
| TQQQ | 50M+ shares | Excellent |
| SQQQ | 40M+ shares | Excellent |
| SSO | 3M+ shares | Very Good |
| SDS | 2M+ shares | Very Good |
| TNA | 15M+ shares | Excellent |
| TZA | 10M+ shares | Excellent |
| FAS | 5M+ shares | Excellent |
| FAZ | 3M+ shares | Very Good |
| UVXY | 25M+ shares | Excellent |
| SVXY | 8M+ shares | Excellent |

---

## Trading Strategy with 10 Symbols

### Rotation Logic

1. **Feature Extraction:** Extract 25 features per symbol
2. **Prediction:** EWRLS predicts next-bar return
3. **Ranking:** Rank all 10 by predicted return
4. **Selection:** Take top 3 (default `--max-positions 3`)
5. **Execution:** Rotate into top 3, exit others
6. **Risk Mgmt:** Stop-loss (-2%), profit target (+5%)

### Example Rotation

**Market Scenario:** Tech rally, low volatility

**Predictions:**
1. TQQQ: +0.8% (highest)
2. SSO: +0.5%
3. SVXY: +0.4%
4. FAS: +0.2%
5. ...others lower or negative

**Action:**
- Enter TQQQ, SSO, SVXY
- Exit any other positions

### Sector Diversification

The 10 symbols span different sectors:
- **Technology:** TQQQ/SQQQ
- **Broad Market:** SSO/SDS
- **Small Cap:** TNA/TZA
- **Finance:** FAS/FAZ
- **Volatility:** UVXY/SVXY

This ensures rotation opportunities across market conditions.

---

## Configuration Options

### Default (Recommended)

```bash
./sentio_lite --symbols 10
```

**Uses all 10 symbols, max 3 positions**

### Conservative (Lower Risk)

```bash
./sentio_lite --symbols 6 --max-positions 2
```

**Uses 6 core symbols (TQQQ, SQQQ, UPRO, SDS, UVXY, SVXY), max 2 positions**

### Aggressive (Higher Risk)

```bash
./sentio_lite --symbols 14 --max-positions 4
```

**Uses all 14 symbols including energy and gold, max 4 positions**

---

## Download Data

### Quick Download (10 Symbols)

```bash
./scripts/download_10_symbols.sh
```

**Downloads:**
- All 10 recommended symbols
- Default: Sept 15 - Oct 31, 2024
- Binary format (.bin)
- ~500-800 MB total

### Custom Date Range

```bash
./scripts/download_10_symbols.sh 2024-10-01 2024-10-31
```

### Alternative: Use online_trader Data

```bash
# Symlink to online_trader data
ln -s ../online_trader/data/equities data

# Test
./sentio_lite --symbols 10
```

---

## Testing Different Symbol Counts

### Test 6 vs 10 vs 14

```bash
# 6 symbols
./sentio_lite --symbols 6 --results-file results_6.json

# 10 symbols (recommended)
./sentio_lite --symbols 10 --results-file results_10.json

# 14 symbols
./sentio_lite --symbols 14 --results-file results_14.json

# Compare results
diff results_6.json results_10.json
```

### Test Different Position Counts

```bash
# Max 2 positions
./sentio_lite --symbols 10 --max-positions 2

# Max 3 positions (recommended)
./sentio_lite --symbols 10 --max-positions 3

# Max 4 positions
./sentio_lite --symbols 10 --max-positions 4
```

---

## Why NOT UPRO/SPXS?

### UPRO vs SSO (Bull SPY)

| Metric | SSO (2x) | UPRO (3x) |
|--------|----------|-----------|
| **Leverage** | 2x | 3x |
| **Volatility** | ~35% | ~55% |
| **Decay** | Lower | Higher |
| **Drawdown** | -40% typical | -60% typical |
| **Recovery** | Faster | Slower |
| **Rotation** | Better | Worse |

### SPXS vs SDS (Bear SPY)

| Metric | SDS (2x) | SPXS (3x) |
|--------|----------|-----------|
| **Leverage** | 2x inverse | 3x inverse |
| **Volatility** | ~35% | ~55% |
| **Decay** | Lower | Higher |
| **Daily Reset** | Less impact | More impact |
| **Holding** | More forgiving | Punishing |

### Key Insight: Volatility Decay

Leveraged ETFs reset daily, causing decay:
- **2x:** ~5-10% annual decay
- **3x:** ~15-25% annual decay

For **rotation trading** (holding 1-5 days):
- 2x is more stable
- 3x has unnecessary extra risk
- Better risk-adjusted returns with 2x

---

## Summary

### The 10 Recommended Symbols

1. **TQQQ** - 3x QQQ bull
2. **SQQQ** - 3x QQQ bear
3. **SSO** - 2x SPY bull ⭐
4. **SDS** - 2x SPY bear ⭐
5. **TNA** - 3x Russell bull
6. **TZA** - 3x Russell bear
7. **FAS** - 3x Finance bull
8. **FAZ** - 3x Finance bear
9. **UVXY** - VIX call
10. **SVXY** - VIX put

### Key Benefits

✅ Optimal diversification (5 market segments)
✅ Balanced leverage (2x for SPY, 3x for others)
✅ Lower overall volatility
✅ Better risk-adjusted returns
✅ Easier to manage and monitor
✅ Proven liquidity and volume

### Quick Start

```bash
# Download data
./scripts/download_10_symbols.sh

# Test
cd build
./sentio_lite --symbols 10

# Generate dashboard
./sentio_lite --symbols 10 --generate-dashboard
```

---

**Trade smarter with 10 optimized symbols! 📈**
