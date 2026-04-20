#!/usr/bin/env bash
# Simple chunked request tester for webserv
# Usage: ./tests/chunked_test.sh [host] [port] [path] [mode]
#  mode: good (default) | bad
# Examples:
#  ./tests/chunked_test.sh localhost 9999 /upload/chunked.txt good
#  ./tests/chunked_test.sh localhost 9999 /upload/chunked.txt bad

HOST=${1:-127.0.0.1}
PORT=${2:-9999}
PATH=${3:-/upload/chunked.txt}
MODE=${4:-good}

NC_CMD="nc"
# prefer nc -q 0 (gnu-netcat) or nc -N (openbsd)
if nc -h >/dev/null 2>&1; then
  :
fi

echo "Sending chunked request to http://$HOST:$PORT$PATH (mode=$MODE)"

if [ "$MODE" = "good" ]; then
  # two chunks: "Hello" and " World"
  request=$(printf '%s' "POST $PATH HTTP/1.1\r\nHost: $HOST:$PORT\r\nUser-Agent: chunked-test/1.0\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n")
elif [ "$MODE" = "bad" ]; then
  # malformed chunk: declare 6 but send only 5 bytes -> invalid
  request=$(printf '%s' "POST $PATH HTTP/1.1\r\nHost: $HOST:$PORT\r\nUser-Agent: chunked-test/1.0\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n6\r\nHello\r\n0\r\n\r\n")
else
  echo "Unknown mode: $MODE" >&2
  exit 2
fi

# Try using nc with -q 0 (GNU netcat) or -N (OpenBSD netcat) if available
if command -v nc >/dev/null 2>&1; then
  # test whether nc supports -q
  if nc -h 2>&1 | grep -q "-q"; then
    echo "$request" | nc -q 0 $HOST $PORT
  else
    # fallback to -N or plain nc
    if nc -h 2>&1 | grep -q "-N"; then
      echo "$request" | nc -N $HOST $PORT
    else
      echo "$request" | nc $HOST $PORT
    fi
  fi
else
  if command -v python3 >/dev/null 2>&1; then
    python3 - <<PYEOF
import sys, socket
data = sys.stdin.read()
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("${HOST}", int(${PORT})))
sock.sendall(data.encode('utf-8'))
sock.shutdown(socket.SHUT_WR)
resp = []
while True:
    chunk = sock.recv(4096)
    if not chunk:
        break
    resp.append(chunk)
sys.stdout.write(b''.join(resp).decode('utf-8', errors='replace'))
sock.close()
PYEOF
  else
    echo "nc not found and python3 not available. Install netcat or python3." >&2
    exit 3
  fi
fi
