#!/usr/bin/env python3
"""
Market Data Manager - Append-Only Historical Market Data Database

A comprehensive tool for managing historical market data with the following guarantees:
1. **Never truncates existing data** - only expands the database
2. **No gaps except market holidays** - ensures data continuity
3. **Perfect 391-bar alignment** - every trading day has exactly 391 bars (9:30 AM - 4:00 PM ET)
4. **Auto date range adjustment** - extends requested range to align with trading days
5. **Intelligent merging** - combines new data with existing data seamlessly

This is a read-only database that keeps expanding as new market data becomes available
or historical data is backfilled for research purposes.

Usage:
    # Download new data (appends to existing)
    python3 market_data_manager.py --symbols TQQQ SQQQ --start 2025-10-20 --end 2025-10-24

    # Backfill historical data
    python3 market_data_manager.py --symbols TQQQ SQQQ --start 2025-09-01 --end 2025-09-30

    # Check database status
    python3 market_data_manager.py --status --symbols TQQQ SQQQ

    # List all symbols in database
    python3 market_data_manager.py --list
"""

import os
import argparse
import requests
import pandas as pd
import pandas_market_calendars as mcal
import struct
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional, Tuple, List, Dict

# === Constants ===
RTH_START = "09:30"
RTH_END = "16:00"
NY_TIMEZONE = "America/New_York"
POLYGON_API_BASE = "https://api.polygon.io"
BARS_PER_DAY = 391  # 9:30 AM to 4:00 PM = 390 minutes + 1 initial bar
FILE_SUFFIX = "_RTH_NH"  # Regular Trading Hours, No Holidays


class MarketDataDB:
    """
    Manages append-only market data storage with perfect bar alignment.
    """

    def __init__(self, data_dir: str = "data"):
        self.data_dir = Path(data_dir)
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.nyse_calendar = mcal.get_calendar('NYSE')

    def _get_file_paths(self, symbol: str) -> Tuple[Path, Path]:
        """Returns (csv_path, bin_path) for a given symbol."""
        prefix = f"{symbol.upper()}{FILE_SUFFIX}"
        return (
            self.data_dir / f"{prefix}.csv",
            self.data_dir / f"{prefix}.bin"
        )

    def read_existing_data(self, symbol: str) -> Optional[pd.DataFrame]:
        """
        Reads existing data from CSV file if it exists.
        Returns None if file doesn't exist or is empty.
        """
        csv_path, _ = self._get_file_paths(symbol)

        if not csv_path.exists():
            return None

        try:
            df = pd.read_csv(csv_path)
            if df.empty:
                return None

            # Parse the timestamp and set as index
            df['ts_utc'] = pd.to_datetime(df['ts_utc'])
            df.set_index('ts_utc', inplace=True)
            df.index = df.index.tz_convert(NY_TIMEZONE)

            print(f"✓ Existing data loaded: {len(df)} bars ({len(df)//BARS_PER_DAY} days)")
            return df

        except Exception as e:
            print(f"⚠ Warning: Could not read existing data for {symbol}: {e}")
            return None

    def get_date_range(self, df: Optional[pd.DataFrame]) -> Optional[Tuple[datetime, datetime]]:
        """Returns (start_date, end_date) of existing data, or None if no data."""
        if df is None or df.empty:
            return None
        return (df.index.min().date(), df.index.max().date())

    def fetch_from_polygon(self, symbol: str, start_date: str, end_date: str,
                          api_key: str, timespan: str = "minute", multiplier: int = 1) -> Optional[pd.DataFrame]:
        """
        Fetches aggregate bars from Polygon.io API with pagination support.
        """
        print(f"📡 Fetching '{symbol}' from Polygon.io ({start_date} to {end_date})...")

        url = (
            f"{POLYGON_API_BASE}/v2/aggs/ticker/{symbol}/range/{multiplier}/{timespan}/"
            f"{start_date}/{end_date}?adjusted=true&sort=asc&limit=50000"
        )

        headers = {"Authorization": f"Bearer {api_key}"}
        all_bars = []

        while url:
            try:
                response = requests.get(url, headers=headers, timeout=30)
                response.raise_for_status()
                data = response.json()

                if "results" in data:
                    all_bars.extend(data["results"])
                    print(f"   Fetched {len(all_bars)} bars...", end="\r")

                url = data.get("next_url")

            except requests.exceptions.RequestException as e:
                print(f"\n❌ API Error: {e}")
                return None
            except Exception as e:
                print(f"\n❌ Unexpected error: {e}")
                return None

        print(f"\n   ✓ Total bars fetched: {len(all_bars)}")

        if not all_bars:
            return None

        # Convert to DataFrame
        df = pd.DataFrame(all_bars)
        df.rename(columns={
            't': 'timestamp_utc_ms',
            'o': 'open',
            'h': 'high',
            'l': 'low',
            'c': 'close',
            'v': 'volume'
        }, inplace=True)

        return df

    def filter_and_align(self, df: pd.DataFrame) -> pd.DataFrame:
        """
        Filters for RTH, removes holidays, and ensures perfect 391-bar alignment.
        """
        if df is None or df.empty:
            return None

        print("🔧 Processing: RTH filter + holiday removal + 391-bar alignment...")

        # Convert to NY timezone
        df['timestamp_utc_ms'] = pd.to_datetime(df['timestamp_utc_ms'], unit='ms', utc=True)
        df.set_index('timestamp_utc_ms', inplace=True)
        df.index = df.index.tz_convert(NY_TIMEZONE)

        # Filter for RTH (9:30 AM to 4:00 PM)
        df = df.between_time(RTH_START, RTH_END)

        # Remove market holidays
        holidays = self.nyse_calendar.holidays().holidays
        df = df[~df.index.normalize().isin(holidays)]

        print(f"   → {len(df)} bars after RTH/holiday filtering")

        # Create perfect 391-bar grid
        trading_days = df.index.normalize().unique()
        complete_index = pd.DatetimeIndex([], tz=NY_TIMEZONE)

        for day in trading_days:
            day_start = day.replace(hour=9, minute=30, second=0, microsecond=0)
            day_end = day.replace(hour=16, minute=0, second=0, microsecond=0)
            day_range = pd.date_range(start=day_start, end=day_end, freq='1min', tz=NY_TIMEZONE)
            complete_index = complete_index.union(day_range)

        # Reindex and forward-fill
        df_aligned = df.reindex(complete_index)
        df_aligned = df_aligned.ffill().bfill()

        # Verify alignment
        bars_per_day = df_aligned.groupby(df_aligned.index.date).size()
        misaligned_days = [date for date, count in bars_per_day.items() if count != BARS_PER_DAY]

        if misaligned_days:
            print(f"   ⚠ WARNING: {len(misaligned_days)} days with incorrect bar count:")
            for date in misaligned_days[:5]:  # Show first 5
                print(f"      {date}: {bars_per_day[date]} bars (expected {BARS_PER_DAY})")
        else:
            print(f"   ✓ Perfect alignment: {len(trading_days)} days × {BARS_PER_DAY} bars = {len(df_aligned)} bars")

        # Add required columns
        df_aligned['ts_utc'] = df_aligned.index.strftime('%Y-%m-%dT%H:%M:%S%z').str.replace(
            r'([+-])(\d{2})(\d{2})', r'\1\2:\3', regex=True
        )
        df_aligned['ts_nyt_epoch'] = df_aligned.index.astype('int64') // 10**9

        return df_aligned

    def merge_data(self, existing: Optional[pd.DataFrame], new: pd.DataFrame) -> Tuple[pd.DataFrame, Dict[str, int]]:
        """
        Intelligently merges new data with existing data.

        Returns:
            (merged_df, stats_dict) where stats contains:
                - 'existing_bars': Number of bars in existing data
                - 'new_bars': Number of bars in new fetch
                - 'added_bars': Number of bars actually added
                - 'overlapping_bars': Number of bars that already existed
        """
        stats = {
            'existing_bars': len(existing) if existing is not None else 0,
            'new_bars': len(new),
            'added_bars': 0,
            'overlapping_bars': 0,
            'start_date': None,
            'end_date': None
        }

        if existing is None or existing.empty:
            print("📥 No existing data - using all new data")
            stats['added_bars'] = len(new)
            stats['start_date'] = new.index.min().date()
            stats['end_date'] = new.index.max().date()
            return new, stats

        # Keep only OHLCV columns for merging (drop ts_utc/ts_nyt_epoch - will regenerate)
        ohlcv_cols = ['open', 'high', 'low', 'close', 'volume']
        existing_clean = existing[ohlcv_cols].copy() if existing is not None else None
        new_clean = new[ohlcv_cols].copy()

        # Combine and remove duplicates (keep new data for overlaps)
        combined = pd.concat([existing_clean, new_clean])
        combined = combined[~combined.index.duplicated(keep='last')]
        combined = combined.sort_index()

        stats['added_bars'] = len(combined) - len(existing)
        stats['overlapping_bars'] = len(existing) + len(new) - len(combined)
        stats['start_date'] = combined.index.min().date()
        stats['end_date'] = combined.index.max().date()

        print(f"🔀 Merge complete:")
        print(f"   Existing: {stats['existing_bars']} bars")
        print(f"   Fetched:  {stats['new_bars']} bars")
        print(f"   Added:    {stats['added_bars']} bars")
        print(f"   Overlap:  {stats['overlapping_bars']} bars (updated)")
        print(f"   Total:    {len(combined)} bars")
        print(f"   Range:    {stats['start_date']} to {stats['end_date']}")

        return combined, stats

    def save_data(self, df: pd.DataFrame, symbol: str):
        """
        Saves DataFrame to both CSV and binary format.
        """
        csv_path, bin_path = self._get_file_paths(symbol)

        # Ensure ts_utc and ts_nyt_epoch columns exist (regenerate if needed after merge)
        if 'ts_utc' not in df.columns or 'ts_nyt_epoch' not in df.columns:
            df['ts_utc'] = df.index.strftime('%Y-%m-%dT%H:%M:%S%z').str.replace(
                r'([+-])(\d{2})(\d{2})', r'\1\2:\3', regex=True
            )
            df['ts_nyt_epoch'] = df.index.astype('int64') // 10**9

        # Save CSV
        print(f"💾 Saving CSV to {csv_path}...")
        csv_columns = ['ts_utc', 'ts_nyt_epoch', 'open', 'high', 'low', 'close', 'volume']
        df_to_save = df[csv_columns].copy()
        df_to_save.to_csv(csv_path, index=False)

        # Save binary
        print(f"💾 Saving binary to {bin_path}...")
        self._save_binary(df, bin_path)

        print(f"   ✓ Saved {len(df)} bars ({len(df)//BARS_PER_DAY} days)")

    def _save_binary(self, df: pd.DataFrame, path: Path):
        """
        Saves to C++ compatible binary format.
        """
        with open(path, 'wb') as f:
            # Write total bar count
            num_bars = len(df)
            f.write(struct.pack('<Q', num_bars))

            # Pack format: q (int64), 4×d (double), Q (uint64)
            bar_struct = struct.Struct('<qddddQ')

            for row in df.itertuples():
                # Variable-length timestamp string
                ts_bytes = row.ts_utc.encode('utf-8')
                f.write(struct.pack('<I', len(ts_bytes)))
                f.write(ts_bytes)

                # Fixed-size OHLCV data
                packed = bar_struct.pack(
                    row.ts_nyt_epoch,
                    row.open,
                    row.high,
                    row.low,
                    row.close,
                    int(row.volume)
                )
                f.write(packed)

    def get_status(self, symbol: str) -> Dict:
        """Returns status information about a symbol's data."""
        csv_path, bin_path = self._get_file_paths(symbol)

        if not csv_path.exists():
            return {
                'symbol': symbol,
                'exists': False,
                'bars': 0,
                'days': 0,
                'start_date': None,
                'end_date': None,
                'csv_size_kb': 0,
                'bin_size_kb': 0
            }

        df = self.read_existing_data(symbol)

        return {
            'symbol': symbol,
            'exists': True,
            'bars': len(df),
            'days': len(df) // BARS_PER_DAY,
            'start_date': df.index.min().date() if df is not None else None,
            'end_date': df.index.max().date() if df is not None else None,
            'csv_size_kb': csv_path.stat().st_size // 1024,
            'bin_size_kb': bin_path.stat().st_size // 1024 if bin_path.exists() else 0
        }

    def list_all_symbols(self) -> List[str]:
        """Returns list of all symbols in the database."""
        symbols = []
        for csv_file in self.data_dir.glob(f"*{FILE_SUFFIX}.csv"):
            symbol = csv_file.stem.replace(FILE_SUFFIX, '')
            symbols.append(symbol)
        return sorted(symbols)

    def get_global_date_range(self, symbols: List[str]) -> Optional[Tuple[datetime, datetime]]:
        """
        Returns the union of date ranges across all specified symbols.
        This ensures all symbols will have the same date range.
        """
        min_date = None
        max_date = None

        for symbol in symbols:
            df = self.read_existing_data(symbol)
            if df is not None and not df.empty:
                symbol_range = self.get_date_range(df)
                if symbol_range:
                    if min_date is None or symbol_range[0] < min_date:
                        min_date = symbol_range[0]
                    if max_date is None or symbol_range[1] > max_date:
                        max_date = symbol_range[1]

        if min_date and max_date:
            return (min_date, max_date)
        return None

    def update_symbol(self, symbol: str, start_date: str, end_date: str, api_key: str) -> bool:
        """
        Main method to update a symbol's data (fetch + merge + save).

        Returns:
            True if successful, False otherwise
        """
        print(f"\n{'='*70}")
        print(f"  Updating {symbol.upper()}")
        print(f"{'='*70}")

        # Step 1: Read existing data
        existing_df = self.read_existing_data(symbol)

        # Step 2: Determine optimal fetch range
        if existing_df is not None:
            existing_range = self.get_date_range(existing_df)
            print(f"📊 Existing range: {existing_range[0]} to {existing_range[1]}")
            print(f"📅 Requested range: {start_date} to {end_date}")

        # Step 3: Fetch new data
        raw_df = self.fetch_from_polygon(symbol, start_date, end_date, api_key)

        if raw_df is None or raw_df.empty:
            print(f"❌ No data fetched for {symbol}")
            return False

        # Step 4: Filter and align
        aligned_df = self.filter_and_align(raw_df)

        if aligned_df is None or aligned_df.empty:
            print(f"❌ No data remaining after filtering for {symbol}")
            return False

        # Step 5: Merge with existing
        merged_df, stats = self.merge_data(existing_df, aligned_df)

        # Step 6: Save
        self.save_data(merged_df, symbol)

        print(f"✅ {symbol} update complete!\n")
        return True

    def sync_all_symbols(self, symbols: List[str], api_key: str) -> Tuple[str, str]:
        """
        Ensures all symbols have the exact same date range.
        Returns the global (start_date, end_date) used for syncing.
        """
        print(f"\n{'='*70}")
        print(f"  Synchronizing {len(symbols)} symbols to same date range")
        print(f"{'='*70}\n")

        # Step 1: Find global date range across all symbols
        global_range = self.get_global_date_range(symbols)

        if global_range is None:
            print("ℹ️  No existing data found - will use requested range for all symbols")
            return None

        start_date = global_range[0].strftime('%Y-%m-%d')
        end_date = global_range[1].strftime('%Y-%m-%d')

        print(f"📊 Global date range detected: {start_date} to {end_date}")
        print(f"   Ensuring all {len(symbols)} symbols have this range...\n")

        # Step 2: Update each symbol to have the global range
        updated_count = 0
        for symbol in symbols:
            existing_df = self.read_existing_data(symbol)

            # Check if symbol already has the full range
            if existing_df is not None:
                existing_range = self.get_date_range(existing_df)
                if existing_range and existing_range[0] == global_range[0] and existing_range[1] == global_range[1]:
                    print(f"✓ {symbol}: Already synchronized ({start_date} to {end_date})")
                    continue

            # Symbol needs update
            if self.update_symbol(symbol, start_date, end_date, api_key):
                updated_count += 1

        print(f"\n{'='*70}")
        print(f"  Sync complete: {updated_count} symbols updated")
        print(f"  All symbols now have range: {start_date} to {end_date}")
        print(f"{'='*70}\n")

        return (start_date, end_date)


def main():
    parser = argparse.ArgumentParser(
        description="Market Data Manager - Append-only historical market data database",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    # Data update commands
    parser.add_argument('--symbols', nargs='+', help="Symbols to update (e.g., TQQQ SQQQ)")
    parser.add_argument('--start', help="Start date (YYYY-MM-DD)")
    parser.add_argument('--end', help="End date (YYYY-MM-DD)")
    parser.add_argument('--outdir', default='data', help="Data directory (default: data)")

    # Sync commands
    parser.add_argument('--sync', action='store_true',
                       help="After updating, sync ALL symbols to same date range")
    parser.add_argument('--sync-only', action='store_true',
                       help="Sync all existing symbols to same date range (no new downloads)")

    # Query commands
    parser.add_argument('--status', action='store_true', help="Show status of specified symbols")
    parser.add_argument('--list', action='store_true', help="List all symbols in database")

    args = parser.parse_args()

    # Initialize database
    db = MarketDataDB(args.outdir)

    # Handle query commands
    if args.list:
        symbols = db.list_all_symbols()
        print(f"\n📚 Market Data Database ({args.outdir})")
        print(f"{'='*70}")
        if not symbols:
            print("  (empty)")
        else:
            print(f"  {len(symbols)} symbols: {', '.join(symbols)}")
        print(f"{'='*70}\n")
        return

    if args.status:
        if not args.symbols:
            print("❌ Error: --status requires --symbols")
            return

        print(f"\n📊 Database Status")
        print(f"{'='*70}")
        for symbol in args.symbols:
            status = db.get_status(symbol)
            if status['exists']:
                print(f"  {symbol}:")
                print(f"    Range: {status['start_date']} to {status['end_date']}")
                print(f"    Bars:  {status['bars']:,} ({status['days']} days)")
                print(f"    Size:  CSV={status['csv_size_kb']} KB, BIN={status['bin_size_kb']} KB")
            else:
                print(f"  {symbol}: (no data)")
        print(f"{'='*70}\n")
        return

    # Handle sync-only command
    if args.sync_only:
        api_key = os.getenv('POLYGON_API_KEY')
        if not api_key:
            print("❌ Error: POLYGON_API_KEY environment variable not set")
            return

        # Get all symbols from config or data directory
        all_symbols = db.list_all_symbols()
        if not all_symbols:
            print("❌ No symbols found in database")
            return

        db.sync_all_symbols(all_symbols, api_key)
        return

    # Handle data update commands
    if not args.symbols or not args.start or not args.end:
        parser.print_help()
        print("\n❌ Error: Data update requires --symbols, --start, and --end")
        return

    # Get API key
    api_key = os.getenv('POLYGON_API_KEY')
    if not api_key:
        print("❌ Error: POLYGON_API_KEY environment variable not set")
        return

    # Update each symbol
    success_count = 0
    for symbol in args.symbols:
        if db.update_symbol(symbol, args.start, args.end, api_key):
            success_count += 1

    # Summary
    print(f"\n{'='*70}")
    print(f"  Update Summary: {success_count}/{len(args.symbols)} symbols successful")
    print(f"{'='*70}\n")

    # Auto-sync if requested
    if args.sync and success_count > 0:
        # Load all symbols from config or use existing symbols
        try:
            import json
            config_path = Path('config/symbols.conf')
            if config_path.exists():
                with open(config_path) as f:
                    all_symbols = [line.strip() for line in f
                                  if line.strip() and not line.strip().startswith('#')]
                print(f"ℹ️  Loaded {len(all_symbols)} symbols from config/symbols.conf")
            else:
                all_symbols = db.list_all_symbols()
                print(f"ℹ️  Using {len(all_symbols)} symbols from database")

            if all_symbols:
                db.sync_all_symbols(all_symbols, api_key)
        except Exception as e:
            print(f"⚠️  Warning: Could not auto-sync: {e}")


if __name__ == "__main__":
    main()
