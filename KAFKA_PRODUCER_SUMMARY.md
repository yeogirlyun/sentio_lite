### Sentio Lite – Kafka Producer Summary (SIGOR v2.0)

Purpose
- Deliver minute-by-minute data for the iOS app via Kafka so users can watch our intraday trading system live.
- Two operating modes:
  - replay: deterministic playback from historical outputs (`results.json`, `trades.jsonl`).
  - polygon: live streaming of minute bars from Polygon; engine live wiring to follow for signals/trades/positions.

Current status (implemented)
- Python sidecar producer (confluent-kafka) with two modes: `replay` and `polygon`.
- Dockerized SIGOR engine and compose for a full local stack.
- Topics and payloads defined (JSON v1), documented in `HANDOVER_KAFKA.md`.

Artifacts
- tools/kafka_sidecar.py
- docker/docker-compose.sidecar.yml (engine + Kafka + sidecar; historical replay)
- docker/docker-compose.polygon.yml (Kafka + sidecar; live Polygon prices)
- Dockerfile, docker/entrypoint.sh
- HANDOVER_KAFKA.md (how to run, topics, examples)

Topics (v1)
- sentio.prices.minute.v1  (key=symbol)
- sentio.positions.state.v1  (key=symbol, compacted)
- sentio.portfolio.minute.v1  (key=portfolioId, compacted)
- sentio.trades.executed.v1  (key=symbol)
- sentio.heartbeat.v1  (key=runId, compacted)

Producer modes
1) replay (done)
   - Inputs: `results.json` (embedded minute price_data) + `trades.jsonl`.
   - Output cadence: every minute bucket across the test day (accelerated by REPLAY_SPEED_MS).
   - Publishes: prices, trades, positions, portfolio, heartbeat.

2) polygon (phase 1 done)
   - Inputs: Polygon WebSocket minute aggregates for 12 symbols.
   - Publishes: prices + heartbeat (now). Signals/trades/positions will publish when engine live mode is wired.

Run commands
- Historical replay (accelerated):
  - `cd docker && docker compose -f docker-compose.sidecar.yml up --build`
  - Env: `REPLAY_SPEED_MS` (60 default; 60000 for real-time pace)
- Polygon live prices:
  - `cd docker && export POLYGON_API_KEY=xxxxx && docker compose -f docker-compose.polygon.yml up`

Integration options for the app team
- Option A (now):
  - Replay: requires this repo (or prebuilt engine image) + data/.
  - Polygon (prices-only): requires only sidecar + API key; no engine needed.
- Option B (recommended post-decision):
  - Publish prebuilt images (engine + sidecar) and place a compose file in `sentio_lite_app` so the app team runs everything without accessing this repo.

Decisions pending
1) Live signals/trades/positions in Polygon mode:
   - A1: Add engine live mode (ingest Polygon → generate SIGOR outputs) and include the engine container in the live compose.
   - A2: Publish a prebuilt `sentio_lite` image (GHCR) referenced by the app repo compose; no source access needed.

Open items / next steps
- Implement engine live mode wiring (Polygon → SIGOR → outputs) and update sidecar to publish positions/portfolio/trades in live.
- Optionally publish images (`ghcr.io/<org>/sentio-lite:2.1`) and copy compose into `sentio_lite_app`.
- Add `sentio.signals.minute.v1` once the engine exposes bar-level signals to the sidecar.

Assumptions
- Timestamps are emitted as minute buckets with ET intent.
- JSON payloads for MVP; can migrate to Avro/Protobuf later.

Contacts
- Strategy/engine: this repo (`sentio_lite`).
- App integration: `sentio_lite_app` repo.


