# Commit Notes — 2025-10-25 — SIGOR Live Resilience, Mock-Live Parity, Runtime Failure Reporting

This commit delivers a robust SIGOR-only trading stack with live/mock-live parity, automated resilience testing, and runtime failure reporting for real sessions.

## Summary

- SIGOR-only cleanup
  - Removed EWRLS code paths and artifacts; SIGOR is the only strategy.
  - Removed `--sim-days` across code, scripts, and docs.
  - Updated build and CLI to reflect SIGOR-only.

- Mock-live parity with live
  - `mock-live` routes to the exact live loop; engine cannot distinguish mock vs live.
  - `scripts/replay_fifo_from_results.py` streams minute bars from a results file to `/tmp/alpaca_bars.fifo` with automatic FIFO reopen on `BrokenPipeError` (survives engine restarts).
  - `scripts/launch_mock_live.sh` orchestrates replay + engine, generates dashboard on completion, and optionally emails it (`--email-to`).
  - Live loop now exports `results.json` and `trades.jsonl` at session end (both live and mock-live flows produce dashboards reliably).

- Dashboard and email
  - Maintained ET/RTH filtering and robust marker snapping in `scripts/rotation_trading_dashboard_html.py`.
  - `scripts/send_dashboard_email.py` sends dashboards using `GMAIL_USER`/`GMAIL_APP_PASSWORD`.

- Resilience test suite
  - `scripts/mocklive_resilience_test.sh` runs 5 restart schemes end-to-end:
    1) Kill/restart engine while stream continues
    2) Pause/resume stream
    3) Remove/recreate FIFO mid-run
    4) Late engine start without warmup cache
    5) Multiple stop/start cycles in one session
  - Writes a timestamped report under `logs/` and saves failure artifacts under `logs/resilience_failures/`.

- Runtime failure reporting in live
  - Per-bar exceptions: write WARN incident to `logs/live/failure_WARN_<ts>.log` containing:
    - Recent 50 raw FIFO lines, symbols expected/present, bars/snapshots counters, and current positions.
  - Top-level exceptions: write FATAL report to `logs/live/failure_FATAL_<ts>.log`.
  - Per-bar incidents do not halt the session; only FATAL stops the loop.

- Robustness fixes and refactors
  - Guarded `predictions.at(...)` accesses (now use `find` with checks) to avoid key-not-found during partial snapshots in restarts.
  - Removed EWRLS dependencies in indicators by computing BB/MA/vol from `price_history_`.
  - DRY’d config parsing helpers; simplified trade history trimming.
  - Added `--config DIR` to select configuration directory.

## Files of Note

- `src/main.cpp`: live-loop export on completion; runtime failure reporter; `--config DIR` support.
- `src/trading/multi_symbol_trader.cpp`: guarded prediction lookups; SIGOR-only paths.
- `scripts/replay_fifo_from_results.py`: resilient FIFO writer with auto-reopen.
- `scripts/launch_mock_live.sh`: end-to-end runner with optional email.
- `scripts/send_dashboard_email.py`: Gmail-based email sender.
- `scripts/mocklive_resilience_test.sh`: automated restart/resilience suite.

## How to Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

# Run mock-live at 60x
EMAIL_TO="you@example.com" ./scripts/launch_mock_live.sh --date 2025-10-24 --speed-ms 1000

# Run automated resilience suite
./scripts/mocklive_resilience_test.sh --date 2025-10-24 --speed-ms 800
```

## Outcomes

- Mock-live session completes with dashboard and optional email.
- All five restart schemes validated; engine resumes cleanly.
- Real-time sessions now log detailed incident reports, enabling rapid triage and test coverage additions for unseen failures.


