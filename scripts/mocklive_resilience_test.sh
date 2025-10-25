#!/bin/bash
set -u

# Sentio Lite - Mock-Live Resilience Test Suite
# Runs a battery of restart/resilience scenarios against the mock-live pipeline.
# Produces a summary report and failure artifacts under logs/.

FIFO=/tmp/alpaca_bars.fifo
RESULTS=""
DATE=""
SPEED_MS=800
TIMEOUT=45
MIN_SNAPSHOTS=20
FEED="fifo"   # fifo | zmq
ZMQ_BIND="tcp://127.0.0.1:5555"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --results) RESULTS="$2"; shift 2 ;;
    --date) DATE="$2"; shift 2 ;;
    --speed-ms) SPEED_MS="$2"; shift 2 ;;
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --min-snapshots) MIN_SNAPSHOTS="$2"; shift 2 ;;
    --feed) FEED="$2"; shift 2 ;;
    --zmq-bind) ZMQ_BIND="$2"; shift 2 ;;
    *) shift ;;
  esac
done

mkdir -p logs/live logs/dashboard logs/resilience_failures

ts() { date +"%Y-%m-%d %H:%M:%S"; }
log() { echo "[$(ts)] $*"; }

ensure_results() {
  if [[ -n "$RESULTS" && -f "$RESULTS" ]]; then
    return 0
  fi
  if [[ -n "$DATE" ]]; then
    RESULTS="results_${DATE}.json"
    if [[ ! -f "$RESULTS" ]]; then
      log "Generating results for $DATE → $RESULTS"
      ./build/sentio_lite mock --date "$DATE" --no-dashboard --results-file "$RESULTS" >/dev/null
    fi
    return 0
  fi
  echo "Provide --results or --date" >&2; return 1
}

ensure_fifo() {
  rm -f "$FIFO"
  mkfifo "$FIFO"
}

start_replayer() {
  local logf="logs/live/replayer_$1.log"
  log "Starting replayer → $logf (speed=${SPEED_MS}ms, feed=${FEED})"
  if [[ "$FEED" == "fifo" ]]; then
    python3 scripts/replay_fifo_from_results.py --results "$RESULTS" --fifo "$FIFO" --speed-ms "$SPEED_MS" > "$logf" 2>&1 &
  else
    python3 tools/zmq_replay_from_results.py --results "$RESULTS" --bind "$ZMQ_BIND" --speed-ms "$SPEED_MS" > "$logf" 2>&1 &
  fi
  echo $! > /tmp/replayer_pid
}

stop_replayer() {
  if [[ -f /tmp/replayer_pid ]]; then kill "$(cat /tmp/replayer_pid)" 2>/dev/null || true; fi
}

start_engine() {
  local logf="$1"
  log "Starting engine → $logf"
  ./build/sentio_lite mock-live --date 2000-01-01 > "$logf" 2>&1 &
  echo $! > /tmp/engine_pid
}

stop_engine() {
  if [[ -f /tmp/engine_pid ]]; then kill "$(cat /tmp/engine_pid)" 2>/dev/null || true; fi
}

count_snapshots() {
  local logf="$1"
  rg -n "Status Update] Snapshot" "$logf" 2>/dev/null | wc -l | awk '{print $1}'
}

await_snapshots() {
  local logf="$1"; local min="$2"; local to="$3"
  local waited=0
  while [[ $waited -lt $to ]]; do
    local n; n=$(count_snapshots "$logf")
    if [[ "$n" -ge "$min" ]]; then return 0; fi
    sleep 2; waited=$((waited+2))
  done
  return 1
}

record_failure() {
  local name="$1"; shift
  local dir="logs/resilience_failures/${name}_$(date +%Y%m%d_%H%M%S)"
  mkdir -p "$dir"
  log "Recording failure artifacts → $dir"
  [[ -f logs/live/replayer_${name}.log ]] && cp "logs/live/replayer_${name}.log" "$dir/"
  for f in "$@"; do [[ -f "$f" ]] && cp "$f" "$dir/"; done
}

run_scheme_1() {
  local name="s1"
  if [[ "$FEED" == "fifo" ]]; then ensure_fifo; fi
  start_replayer "$name"
  local elog="logs/live/engine_${name}.log"
  start_engine "$elog"
  sleep 6
  stop_engine
  sleep 1
  start_engine "$elog"
  if await_snapshots "$elog" "$MIN_SNAPSHOTS" "$TIMEOUT"; then
    log "Scheme 1 PASS"
    return 0
  else
    log "Scheme 1 FAIL"
    record_failure "$name" "$elog"
    return 1
  fi
}

run_scheme_2() {
  local name="s2"
  if [[ "$FEED" == "fifo" ]]; then ensure_fifo; fi
  start_replayer "$name"
  local elog="logs/live/engine_${name}.log"
  start_engine "$elog"
  sleep 6
  kill -STOP "$(cat /tmp/replayer_pid)"
  sleep 4
  kill -CONT "$(cat /tmp/replayer_pid)"
  if await_snapshots "$elog" "$MIN_SNAPSHOTS" "$TIMEOUT"; then
    log "Scheme 2 PASS"
    return 0
  else
    log "Scheme 2 FAIL"
    record_failure "$name" "$elog"
    return 1
  fi
}

run_scheme_3() {
  local name="s3"
  ensure_fifo
  start_replayer "$name"
  local elog="logs/live/engine_${name}.log"
  start_engine "$elog"
  sleep 6
  kill -STOP "$(cat /tmp/replayer_pid)"
  ensure_fifo
  kill -CONT "$(cat /tmp/replayer_pid)"
  if await_snapshots "$elog" "$MIN_SNAPSHOTS" "$TIMEOUT"; then
    log "Scheme 3 PASS"
    return 0
  else
    log "Scheme 3 FAIL"
    record_failure "$name" "$elog"
    return 1
  fi
}

run_scheme_4() {
  local name="s4"
  ensure_fifo
  # advance stream with dummy reader
  python3 scripts/replay_fifo_from_results.py --results "$RESULTS" --fifo "$FIFO" --speed-ms "$SPEED_MS" > "logs/live/replayer_${name}.log" 2>&1 & echo $! > /tmp/replayer_pid
  (cat "$FIFO" > /dev/null) & echo $! > /tmp/dummy_reader
  sleep 8
  kill "$(cat /tmp/dummy_reader)" 2>/dev/null || true
  local elog="logs/live/engine_${name}.log"
  start_engine "$elog"
  if await_snapshots "$elog" "$MIN_SNAPSHOTS" "$TIMEOUT"; then
    log "Scheme 4 PASS"
    return 0
  else
    log "Scheme 4 FAIL"
    record_failure "$name" "$elog"
    return 1
  fi
}

run_scheme_5() {
  local name="s5"
  ensure_fifo
  start_replayer "$name"
  local pass=0
  for i in 1 2 3; do
    local elog="logs/live/engine_${name}_${i}.log"
    start_engine "$elog"
    sleep 5
    stop_engine
    if ! grep -q "Error processing bar" "$elog" 2>/dev/null; then pass=$((pass+1)); fi
  done
  if [[ $pass -eq 3 ]]; then
    log "Scheme 5 PASS"
    return 0
  else
    log "Scheme 5 FAIL"
    record_failure "$name" logs/live/engine_${name}_*.log
    return 1
  fi
}

main() {
  ensure_results || exit 1
  local summary="logs/resilience_report_$(date +%Y%m%d_%H%M%S).txt"
  : > "$summary"
  log "Starting resilience test (results=$RESULTS, speed=${SPEED_MS}ms)" | tee -a "$summary"
  local failures=0

  for fn in run_scheme_1 run_scheme_2 run_scheme_3 run_scheme_4 run_scheme_5; do
    stop_engine; stop_replayer
    if ! $fn; then failures=$((failures+1)); fi
  done

  stop_engine; stop_replayer
  if [[ $failures -eq 0 ]]; then
    log "All schemes PASS" | tee -a "$summary"
    exit 0
  else
    log "$failures scheme(s) FAILED - see logs/resilience_failures" | tee -a "$summary"
    exit 2
  fi
}

main "$@"


