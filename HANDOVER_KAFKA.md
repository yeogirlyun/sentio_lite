### Sentio Lite — Kafka Sidecar Handover (SIGOR v2.0)

This repository now includes a Python Kafka sidecar that streams minute-by-minute data for the iOS app team.

Artifacts
- Producer script: `tools/kafka_sidecar.py`
- Compose: `docker/docker-compose.sidecar.yml`
- Dockerized engine: `Dockerfile` + `docker/entrypoint.sh`

Run (quick start)
```bash
cd docker
docker compose -f docker-compose.sidecar.yml up --build
```

What it does
- Builds and runs `sentio_lite` (SIGOR, single-day mock on TEST_DATE), producing `results.json` and `trades.jsonl`.
- Starts a local Kafka (KRaft) at `kafka:9092`.
- Runs the Python sidecar, which reads `results.json` and `trades.jsonl` and publishes:
  - `sentio.prices.minute.v1` (per symbol)
  - `sentio.positions.state.v1` (compacted)
  - `sentio.portfolio.minute.v1` (compacted)
  - `sentio.trades.executed.v1` (on trade events)
  - `sentio.heartbeat.v1`

Configuration
- Engine env (container `sentio`):
  - `STRATEGY` (default `sigor`), `TEST_DATE`, `SIM_DAYS`, `DATA_DIR`, `EXT`, `DASHBOARD`.
- Sidecar env (container `sidecar`):
  - `KAFKA_BOOTSTRAP_SERVERS` (default `kafka:9092`)
  - `TOPIC_PREFIX` (default `sentio.`)
  - `RESULTS_PATH` (default `/app/results.json`)
  - `TRADES_PATH` (default `/app/trades.jsonl`)
  - `REPLAY_SPEED_MS` (default `60` → 1 market minute per 60ms)
  - `STRATEGY` (default `SIGOR`), `ENV` (default `MOCK`)

Topic keys & retention
- Key = `symbol` for prices/positions/trades; `portfolioId` for portfolio; `runId` for heartbeat.
- Enable compaction for `positions.state.v1` and `portfolio.minute.v1`.
- Keep trades for 7–30 days for replay.

Consuming (examples)
```bash
# Start a console consumer
docker exec -it $(docker ps -qf name=kafka) bash -lc \
  "kafka-console-consumer.sh --bootstrap-server localhost:9092 --topic sentio.prices.minute.v1 --from-beginning | head"
```

Notes
- Sidecar currently omits `sentio.signals.minute.v1` until the engine outputs per-bar signals in `results.json`.
- All timestamps are emitted as minute-bucketed ET-style ISO (`YYYY-MM-DDTHH:MM:00Z`).
- For real-time, set `REPLAY_SPEED_MS=60000` to mimic 1:1 minute cadence.

Live mode via Polygon
```bash
cd docker
export POLYGON_API_KEY=xxxxx
docker compose -f docker-compose.polygon.yml up
```
This streams live minute prices for the 12 symbols to Kafka (`sentio.prices.minute.v1`) with heartbeats. As we wire the engine’s live mode, the sidecar will also publish positions/portfolio/trades.


