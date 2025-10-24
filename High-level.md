### High-level
- Added robust SIGOR strategy support end-to-end (config loading, execution, reporting).
- Fixed SIGOR-specific runtime bugs and dashboard generation/plotting.
- Produced a working SIGOR mock run and HTML dashboard for 2025-10-22.
- Enriched results.json so the dashboard can be rebuilt from one file.
- Also updated your personal grok rename config.

### Key code changes
- Strategy integration and runtime
  - `src/main.cpp`
    - Pre-parse `--strategy` and load strategy-specific config:
      - SIGOR: load model params from `config/sigor_params.json`; trading params from `config/sigor_trading_params.json` if present, else fallback to `config/trading_params.json`.
    - Default `sim_days` to 0.
    - Strategy-aware prints; dashboard filename now includes strategy.
    - Dashboard generation passes correct config path per strategy.
  - `include/trading/trading_mode.h`: added `MOCK_LIVE` mode parsing/labels (currently aliased to mock run).
  - `include/utils/config_loader.h`:
    - `SigorConfigLoader`: parse optional `warmup_bars`.
  - `src/trading/multi_symbol_trader.cpp`
    - Guarded extractor usage for SIGOR (no null deref).
    - Skip signal confirmations when `strategy==SIGOR` to avoid feature OOB.
  - `include/predictor/sigor_predictor_adapter.h`
    - Removed 5/10/20-bar scaling; all horizons mirror 1-bar signal to avoid probability saturation.

- Results and dashboard
  - `include/utils/results_exporter.h`
    - Added `strategy_name` to config.
    - Embedded complete `trades` and per-symbol filtered `price_data` (OHLCV + bar_id).
    - Added nested `config.sigor` block with all SIGOR model parameters.
    - Signature changed; callers now pass filtered bars.
  - `scripts/rotation_trading_dashboard_html.py`
    - Robust UTC parsing; drop invalid timestamps; RTH filtering unchanged.
    - Dashboard header includes strategy; parameter section:
      - If `strategy_name` starts with SIGOR and `config.sigor` present, show SIGOR detector weights/windows (`k`, `w_*`, `win_*`, `orb_opening_bars`, `vol_window`, `warmup_bars`).
      - Else show EWRLS grid.
    - Corrected trade marker alignment:
      - Convert bar timestamps to ET, build minute map, and snap each trade to nearest bar within ±60s.
    - File name examples produced:
      - `logs/dashboard/dashboard_mock_SIGOR_2025-10-22_20251024_083236.html`

### What’s verified working
- SIGOR mock run
  - Command:
    - `./build/sentio_lite mock --strategy sigor --date 2025-10-22 --sim-days 0`
  - Outputs:
    - `results.json` (now self-contained: strategy, trades, price_data)
    - `trades.jsonl`
    - Dashboard: `logs/dashboard/dashboard_mock_SIGOR_2025-10-22_20251024_083236.html`
- Trade markers align with the price line (timestamp mapping fixed).
- Config panel shows SIGOR config file path and parameters when running SIGOR.

### Known limitations / follow‑ups
- Trading parameters for SIGOR execution currently default to `config/trading_params.json` unless you create `config/sigor_trading_params.json`. Recommend adding and optimizing SIGOR-specific portfolio rules (profit target, stop loss, rotation deltas).
- `tools/optuna_sigor.py`: consider extending to optimize trading parameters (profit_target_pct, stop_loss_pct, rotation_strength_delta, max_positions) alongside model params, and write to `sigor_trading_params.json`.
- Dashboard Python file has PEP8 lint warnings; safe to ignore for now.
- GH push: local token was invalid. After you re-auth with `gh`, run a push (details below).

### Rebuild and run on the new drive
1) Build
- `rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`

2) Run SIGOR for a test day
- `./build/sentio_lite mock --strategy sigor --date 2025-10-22 --sim-days 0`

3) Find outputs
- `results.json`, `trades.jsonl`, and a dashboard in `logs/dashboard/dashboard_mock_SIGOR_YYYY-MM-DD_<timestamp>.html`

4) Optional: Regenerate dashboard from `results.json`
- `python3 scripts/rotation_trading_dashboard_html.py --trades trades.jsonl --output logs/dashboard/replot.html --start-equity 100000 --data-dir data --results results.json --config config/sigor_params.json --start-date 2025-10-22 --end-date 2025-10-22`

5) Data assumptions
- `data` contains minute bar RTH-NH `.bin` files (e.g., `SOXL_RTH_NH.bin`); `bars_per_day=391`.

### GitHub push
- Authenticate once:
  - `gh auth login -h github.com` (choose HTTPS, browser or token; grant repo scope).
- Then push:
  - `git push origin main`

### Minor earlier change (your personal tool)
- Updated `~/bin/grok-rename-patterns.conf` to include:
  - English Sub, Uncencered, Uncen leak, Uncen Leak, Chinese Sub, English
- `grok-rename-file.py` uses these as prefix removals at start-of-filename.

If you want, I can re-run on the new drive after you copy data and authenticate GH.