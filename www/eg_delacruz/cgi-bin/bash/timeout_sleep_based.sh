#!/bin/bash

# You may change SLEEP_TIME externally by calling:
#   SLEEP=10 ./slow_timeout.sh
SLEEP_TIME="${SLEEP:-10}"

echo "Content-Type: text/html"
echo ""

echo "<html><body>"
echo "<h1>Slow CGI Timeout Test</h1>"
echo "<p>Sleeping for ${SLEEP_TIME} seconds...</p>"
echo "</body></html>"

sleep "$SLEEP_TIME"

echo "Done sleeping."
