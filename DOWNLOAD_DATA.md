# Downloading Market Data for Sentio Lite

## Quick Start

### Option 1: Use online_trader Data (Recommended)

If you already have online_trader data:

```bash
# Create symlink to online_trader data
ln -s ../online_trader/data/equities data

# Test
cd build
./sentio_lite --symbols 10
```

### Option 2: Download with Sentio Lite Scripts

Download fresh data using Polygon API:

```bash
# Download 10 symbols (recommended)
./scripts/download_10_symbols.sh

# Or download 6 core symbols (faster)
./scripts/download_6_symbols_sentio.sh

# Or specify custom date range
./scripts/download_10_symbols.sh 2024-10-01 2024-10-31
```

---

## The 10 Recommended Symbols ⭐

### Nasdaq-100 (QQQ) - 3x Leveraged
- **TQQQ** - ProShares UltraPro QQQ (3x bull)
- **SQQQ** - ProShares UltraPro Short QQQ (3x bear)

### S&P 500 (SPY) - 2x Leveraged (Recommended)
- **SSO** - ProShares Ultra S&P 500 (2x bull)
- **SDS** - ProShares UltraShort S&P 500 (2x bear)
- **Note:** 2x leverage provides better stability than 3x for rotation trading

### Russell 2000 (IWM) - 3x Leveraged
- **TNA** - Direxion Daily Small Cap Bull 3x (3x bull)
- **TZA** - Direxion Daily Small Cap Bear 3x (3x bear)

### Financial Sector (XLF) - 3x Leveraged
- **FAS** - Direxion Daily Financial Bull 3x (3x bull)
- **FAZ** - Direxion Daily Financial Bear 3x (3x bear)

### Volatility (VIX)
- **UVXY** - ProShares Ultra VIX Short-Term Futures (1.5x VIX call)
- **SVXY** - ProShares Short VIX Short-Term Futures (-0.5x VIX put)

### Additional Symbols (14-symbol mode)
- **UPRO** - ProShares UltraPro S&P 500 (3x bull)
- **SPXS** - Direxion Daily S&P 500 Bear 3x (3x bear)
- **ERX** - Direxion Daily Energy Bull 3x
- **ERY** - Direxion Daily Energy Bear 3x
- **NUGT** - Direxion Daily Gold Miners Bull 3x
- **DUST** - Direxion Daily Gold Miners Bear 3x

---

## Setup: Polygon API Key

### 1. Get Free API Key

Visit: https://polygon.io/
- Sign up for free account
- Get your API key from dashboard
- Free tier: 5 API calls/minute

### 2. Configure API Key

**Option A: Create config.env**

```bash
cd /Volumes/ExternalSSD/Dev/C++/sentio_lite
echo 'export POLYGON_API_KEY="your_key_here"' > config.env
```

**Option B: Use online_trader config**

```bash
# Copy from online_trader
cp ../online_trader/config.env .
```

**Option C: Set in environment**

```bash
export POLYGON_API_KEY="your_key_here"
```

---

## Download Scripts

### download_10_symbols.sh ⭐ Recommended

Downloads all 10 recommended symbols for optimal rotation trading.

**Symbols:** TQQQ, SQQQ, SSO, SDS, TNA, TZA, FAS, FAZ, UVXY, SVXY

**Usage:**
```bash
# Default: Sept 15 - Oct 31, 2024
./scripts/download_10_symbols.sh

# Custom date range
./scripts/download_10_symbols.sh 2024-10-01 2024-10-31

# Specific month
./scripts/download_10_symbols.sh 2024-10-01 2024-10-31
```

**Output:**
- Downloads to `data/` directory
- Binary format (.bin files)
- Minute-level data
- ~50-100MB per symbol per month

**Expected Time:**
- With free API: ~8-12 minutes (rate limited)
- With paid API: ~2-3 minutes

### download_6_symbols_sentio.sh

Downloads 6 core symbols for faster testing.

**Symbols:** TQQQ, SQQQ, UPRO, SDS, UVXY, SVXY

**Usage:**
```bash
./scripts/download_6_symbols_sentio.sh

# Custom dates
./scripts/download_6_symbols_sentio.sh 2024-10-01 2024-10-31
```

---

## Manual Download

### Using online_trader Tools

```bash
# From online_trader directory
cd ../online_trader

# Download 6 symbols
./scripts/download_6_symbols.sh 30  # Last 30 days

# Or download 10 symbols
./scripts/download_10_symbols.sh 2024-09-15 2024-10-31

# Or download 14 symbols
./scripts/download_14_symbols.sh 2024-09-15 2024-10-31

# Data will be in online_trader/data/equities/
# Use symlink or copy to sentio_lite
```

### Using data_downloader.py Directly

```bash
# Single symbol
python3 tools/data_downloader.py \
  --start 2024-10-01 \
  --end 2024-10-31 \
  --outdir data \
  --timespan minute \
  --multiplier 1 \
  TQQQ

# Multiple symbols
for sym in TQQQ SQQQ UPRO SDS; do
  python3 tools/data_downloader.py \
    --start 2024-10-01 \
    --end 2024-10-31 \
    --outdir data \
    --timespan minute \
    --multiplier 1 \
    $sym
done
```

---

## Verifying Downloaded Data

### Check Downloaded Files

```bash
# List data files
ls -lh data/

# Expected files (binary format):
# TQQQ.bin, SQQQ.bin, SSO.bin, SDS.bin, etc.

# Or CSV format:
# TQQQ_RTH_NH.csv, SQQQ_RTH_NH.csv, etc.
```

### Count Bars

```bash
# For binary files
python3 -c "
import struct
with open('data/TQQQ.bin', 'rb') as f:
    count = struct.unpack('Q', f.read(8))[0]
    print(f'TQQQ: {count} bars')
"

# Expected for 1 month: ~8000-10000 bars
# (390 bars/day × ~21 trading days)
```

### Test with sentio_lite

```bash
cd build

# Test with downloaded data
./sentio_lite --symbols 10

# Or test specific symbols
./sentio_lite --symbols TQQQ,SQQQ,SSO,SDS

# Verbose mode to see data loading
./sentio_lite --symbols 10 --verbose
```

---

## Data Formats

### Binary Format (.bin) - Recommended

**Pros:**
- 10-100x faster loading
- Smaller file size
- Used by sentio_lite by default

**Structure:**
```
size_t count             // Number of bars
For each bar:
  int64_t timestamp_ms   // Timestamp in milliseconds
  size_t symbol_len      // Symbol name length
  char* symbol           // Symbol name
  double open, high, low, close
  int64_t volume
```

### CSV Format (.csv) - Fallback

**Format:**
```csv
timestamp_ms,symbol,open,high,low,close,volume
1609459200000,TQQQ,132.43,133.61,131.72,132.69,99116600
```

**Usage:**
```bash
./sentio_lite --symbols 12 --extension .csv
```

---

## Troubleshooting

### Issue: API Key Not Found

```
❌ ERROR: POLYGON_API_KEY not set
```

**Solution:**
```bash
# Check if config.env exists
cat config.env

# Create if missing
echo 'export POLYGON_API_KEY="your_key_here"' > config.env

# Test
source config.env
echo $POLYGON_API_KEY
```

### Issue: Rate Limit Exceeded

```
Error 429: Too Many Requests
```

**Solutions:**
1. Wait 1 minute between downloads
2. Add delays to script:
   ```bash
   for sym in TQQQ SQQQ; do
     python3 tools/data_downloader.py ... $sym
     sleep 15  # Wait 15 seconds
   done
   ```
3. Upgrade to paid Polygon plan

### Issue: Download Fails

```
❌ TQQQ download failed
```

**Check:**
```bash
# Test single symbol manually
python3 tools/data_downloader.py \
  --start 2024-10-01 \
  --end 2024-10-31 \
  --outdir data \
  TQQQ

# Check error message
# Common issues:
# - Invalid date range
# - Symbol doesn't exist
# - Network error
```

### Issue: No Data Files Created

```
No data files found
```

**Check:**
```bash
# Verify output directory
ls -la data/

# Check script output
./scripts/download_12_symbols.sh 2>&1 | tee download.log

# Look for errors in log
grep "ERROR\|Failed" download.log
```

### Issue: Data File Corrupted

```
Error loading data: Invalid binary format
```

**Solution:**
```bash
# Remove and re-download
rm data/TQQQ.bin
python3 tools/data_downloader.py --start 2024-10-01 --end 2024-10-31 --outdir data TQQQ
```

---

## Best Practices

### 1. Download Sufficient History

Include warmup period in download:

```bash
# For 3-day warmup + October test
./scripts/download_12_symbols.sh 2024-09-24 2024-10-31

# For 5-day warmup
./scripts/download_12_symbols.sh 2024-09-20 2024-10-31
```

### 2. Use Binary Format

Always use binary for production:

```bash
# Download creates .bin by default
./scripts/download_12_symbols.sh

# Test
./sentio_lite --symbols 12 --extension .bin
```

### 3. Verify After Download

```bash
# Check file sizes (should be 50-100MB each)
ls -lh data/*.bin

# Test load
./sentio_lite --symbols 12 --verbose | head -20
```

### 4. Keep Data Fresh

```bash
# Re-download weekly for latest data
./scripts/download_12_symbols.sh

# Or download specific recent period
./scripts/download_12_symbols.sh 2024-10-15 2024-10-31
```

### 5. Backup Data

```bash
# Backup downloaded data
tar -czf data_backup_$(date +%Y%m%d).tar.gz data/

# Restore
tar -xzf data_backup_20241017.tar.gz
```

---

## Data Sources

### Polygon.io (Recommended)
- **Free Tier:** 5 calls/min, delayed data
- **Paid Tier:** Unlimited calls, real-time data
- **Coverage:** All US stocks and ETFs
- **Format:** JSON (converted to binary/CSV)

### Alpaca (Alternative)
- **Free:** Paper trading account
- **Data:** Alpaca Data API
- **Coverage:** US stocks
- **Setup:** Requires Alpaca account

---

## Storage Requirements

### Per Symbol (1 month, minute bars)

- **Binary:** ~50-80 MB
- **CSV:** ~100-150 MB

### 10 Symbols (1 month)

- **Binary:** ~500-800 MB (~0.75 GB)
- **CSV:** ~1.0-1.5 GB

### 10 Symbols (1 year)

- **Binary:** ~6-9 GB
- **CSV:** ~12-18 GB

**Recommendation:** Use binary format to save disk space.

---

## Quick Reference

```bash
# Setup
echo 'export POLYGON_API_KEY="your_key"' > config.env

# Download 10 symbols (recommended)
./scripts/download_10_symbols.sh

# Download 6 symbols (faster)
./scripts/download_6_symbols_sentio.sh

# Custom date range
./scripts/download_10_symbols.sh 2024-10-01 2024-10-31

# Verify
ls -lh data/*.bin

# Test
cd build && ./sentio_lite --symbols 10

# Use online_trader data
ln -s ../online_trader/data/equities data
```

---

## Next Steps

1. **Get API Key:** https://polygon.io/
2. **Configure:** `echo 'export POLYGON_API_KEY="..."' > config.env`
3. **Download:** `./scripts/download_10_symbols.sh`
4. **Test:** `cd build && ./sentio_lite --symbols 10`
5. **Trade:** `./sentio_lite --symbols 10 --generate-dashboard`

---

**Ready to download data! 📊**
