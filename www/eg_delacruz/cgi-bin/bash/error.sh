#!/bin/bash

# Send headers first (standard CGI behavior)
echo "Content-Type: text/html"
echo ""

echo "<html><body>"
echo "<h1>CGI Error Test</h1>"
echo "<p>This script will intentionally trigger an error.</p>"
echo "</body></html>"

# Flush output
sleep 0.1

# Simulate a real error by referencing an undefined command
echo "Triggering error now..." >&2
nonexistent_command  # Will cause: "command not found"

# This line should never execute
echo "If you see this, the error script didn't work." >&2
