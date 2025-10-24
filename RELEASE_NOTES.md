### v2.0 — 2025-10-24

Sentio Lite v2.0 delivers a complete SIGOR strategy integration with immediate trading from the open, plus ET-consistent dashboard rendering.

- Core
  - SIGOR runs without warmup/simulation (rule-based): trades start at 9:30 ET.
  - Warmup phases are disabled when `--strategy sigor` is specified.
  - `min_bars_to_learn` set to 0 for SIGOR; phase manager bypassed to LIVE.

- Dashboard
  - All timestamps converted to ET; charts filtered to RTH (9:30–16:00 ET, weekdays).
  - ENTRY/EXIT markers snap to nearest ET bar within ±60s to avoid alignment issues.

- Results (reference)
  - Test date: 2025-10-22 (single day)
  - Starting equity: $100,000
  - Final equity: $101,052.27
  - Total return: +1.05%
  - Trades: 102
  - Dashboard: `logs/dashboard/dashboard_mock_SIGOR_2025-10-22_<timestamp>.html`

- How to reproduce
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/sentio_lite mock --strategy sigor --date 2025-10-22 --sim-days 0
python3 scripts/rotation_trading_dashboard_html.py \
  --trades trades.jsonl \
  --output logs/dashboard/replot.html \
  --data-dir data \
  --results results.json \
  --start-equity 100000
```

- Files of note
  - `src/main.cpp`: apply SIGOR-specific runtime settings (no warmup/sim).
  - `src/trading/multi_symbol_trader.cpp`: force LIVE for SIGOR; skip phases.
  - `include/trading/multi_symbol_trader.h`: clarify LIVE default; SIGOR notes.
  - `scripts/rotation_trading_dashboard_html.py`: ET/RTH filtering and marker snapping.

- Rollback / compare
  - Tag: `v2.0`
  - Compare against prior commit: `git diff v2.0^ v2.0`
  - Roll back working tree to this version:
    - `git checkout v2.0`

Notes:
- For SIGOR, you can optionally add a small intraday warmup (e.g., first 10 bars) later; current default is 0 to trade from the open.


