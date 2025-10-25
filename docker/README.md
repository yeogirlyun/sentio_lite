### Docker Usage (SIGOR v2.0)

Build image:
```bash
docker build -t sentio-lite:2.0 .
```

Run with mounted data directory and logs:
```bash
docker run --rm \
  -v $(pwd)/data:/app/data:ro \
  -v $(pwd)/logs:/app/logs \
  -e STRATEGY=sigor \
  -e TEST_DATE=2025-10-22 \
  -e SIM_DAYS=0 \
  sentio-lite:2.0
```

Environment variables:
- `STRATEGY` (default: `sigor`)
- `TEST_DATE` (YYYY-MM-DD)
- `SIM_DAYS` (default: 0)
- `DATA_DIR` (default: `/app/data`)
- `EXT` (default: `.bin`)
- `DASHBOARD` (1 to enable, 0 to disable)
- `EXTRA_ARGS` (any extra flags)


