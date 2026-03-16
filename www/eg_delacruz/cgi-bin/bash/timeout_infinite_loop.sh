#!/bin/bash

echo "Content-Type: text/html"
echo ""

echo "<html><body>"
echo "<h1>Infinite Loop CGI Test</h1>"
echo "<p>This script entered an infinite loop. Your server should kill it after timeout.</p>"
echo "<p>Looping now...</p>"
echo "</body></html>"

# Flush headers immediately
sleep 0.1

# Infinite loop
while true; do
    :   # no-op
done
