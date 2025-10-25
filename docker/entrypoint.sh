#!/bin/sh
set -e

# Ensure directories
mkdir -p /app/logs/dashboard

cmd="/usr/local/bin/sentio_lite mock --strategy sigor --date ${TEST_DATE} --data-dir ${DATA_DIR} --extension ${EXT}"

if [ "${DASHBOARD}" = "0" ]; then
  cmd="$cmd --no-dashboard"
fi

if [ -n "${EXTRA_ARGS}" ]; then
  cmd="$cmd ${EXTRA_ARGS}"
fi

echo "Running: $cmd"
exec sh -c "$cmd"


