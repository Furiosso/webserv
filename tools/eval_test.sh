#!/usr/bin/env bash
# evaluation script for webserv project
# Generates evaluation_report.txt with automated checks described in the assignment

REPORT="evaluation_report.txt"
SERVER_LOG="logs/webserv_server.log"
SERVER_PIDFILE="logs/webserv.pid"
BUILD_LOG="logs/build_output.txt"
GREP_IO="logs/grep_io.txt"
GREP_ERRNO="logs/grep_errno.txt"
GREP_POLL="logs/grep_poll.txt"

mkdir -p logs
: > "$REPORT"

echo "Webserv evaluation script" | tee -a "$REPORT"
date | tee -a "$REPORT"

append() { echo "$1" | tee -a "$REPORT"; }
append "\n--- Compilation ---\n"

# Compile
append "Running make..."
if make -j2 > "$BUILD_LOG" 2>&1; then
    append "Compile: OK"
else
    append "Compile: FAILED (see $BUILD_LOG)"
fi
append "Build log: $(pwd)/$BUILD_LOG"

append "\n--- Static code checks (I/O calls) ---\n"
# grep for I/O syscalls
grep -Rn --line-number -E "recv\(|read\(|send\(|write\(" srcs inc > "$GREP_IO" || true
append "Found I/O syscall occurrences (saved to $GREP_IO):"
append "$(wc -l < $GREP_IO) lines"
append "$(sed -n '1,200p' $GREP_IO)"

# grep for errno usage
grep -Rn --line-number -E "\berrno\b|perror\(|strerror\(" srcs inc > "$GREP_ERRNO" || true
append "Found errno/perror/strerror occurrences (saved to $GREP_ERRNO):"
append "$(wc -l < $GREP_ERRNO) lines"
append "$(sed -n '1,200p' $GREP_ERRNO)"

# grep for poll/select/epoll
grep -Rn --line-number -E "\bpoll\(|\bselect\(|epoll_wait\(|kqueue\(|kevent\(" srcs inc > "$GREP_POLL" || true
append "Found event loop APIs (saved to $GREP_POLL):"
append "$(wc -l < $GREP_POLL) lines"
append "$(sed -n '1,200p' $GREP_POLL)"

append "\n--- Start server ---\n"
# ensure any previous server is not running
pkill -f ./webserv || true
sleep 1
: > "$SERVER_LOG"
# start server in background
append "Starting server: ./webserv configs/default.conf"
./webserv configs/default.conf > "$SERVER_LOG" 2>&1 &
SV_PID=$!
echo $SV_PID > "$SERVER_PIDFILE"
append "Server PID: $SV_PID"
append "Waiting 1.5s for server to bind..."
sleep 1.5

append "Server log tail (first 200 lines):"
if [ -f "$SERVER_LOG" ]; then
    sed -n '1,200p' "$SERVER_LOG" | tee -a "$REPORT"
else
    append "No $SERVER_LOG found"
fi

# discover listening addresses from server log
ADDRS=()
while read -r line; do
    if echo "$line" | grep -q "Listening on"; then
        # parse something like Listening on 127.0.0.1:9999
        addr=$(echo "$line" | sed -n 's/.*Listening on \([^ ]*\).*/\1/p')
        if [ -n "$addr" ]; then
            ADDRS+=($addr)
        fi
    fi
done < "$SERVER_LOG"

# fallback to 127.0.0.1:9999 if none found
if [ ${#ADDRS[@]} -eq 0 ]; then
    append "No 'Listening on' lines found in server log; using default 127.0.0.1:9999"
    ADDRS+=("127.0.0.1:9999")
fi

append "Using addresses: ${ADDRS[*]}"

# helper to run a curl and append results
run_curl() {
    # Usage: run_curl "desc" [curl-opts...] URL
    desc="$1"; shift
    if [ $# -eq 0 ]; then
        append "run_curl: missing URL for $desc"
        return 1
    fi
    # last arg is URL
    n=$#
    url="${!n}"
    if [ $n -gt 1 ]; then
        opts=("${@:1:$n-1}")
    else
        opts=()
    fi
    append "\n>>> $desc : $url"
    append "curl -i -sS --max-time 10 ${opts[*]} $url"
    curl -i -sS --max-time 10 "${opts[@]}" "$url" 2>> "$SERVER_LOG" | sed -n '1,200p' | tee -a "$REPORT"
}

# create sample files
append "\n--- Prepare test files ---"
TMPDIR="/tmp/webserv_eval"
mkdir -p "$TMPDIR"
head -c 102400 /dev/urandom > "$TMPDIR/test.bin" || dd if=/dev/urandom of="$TMPDIR/test.bin" bs=1024 count=100 > /dev/null 2>&1 || true
sha1sum "$TMPDIR/test.bin" | tee -a "$REPORT"

echo "\n--- Functional tests per address ---" | tee -a "$REPORT"
for addr in "${ADDRS[@]}"; do
    host=$(echo $addr | cut -d: -f1)
    port=$(echo $addr | cut -d: -f2)
    base="http://$host:$port"
    append "\n== Testing $base =="

    run_curl "GET root" "$base/"
    run_curl "GET index.html" "$base/index.html"

    # POST upload
    append "\n-- Upload test --"
    posturl="$base/upload/test.bin"
    append "POSTing $TMPDIR/test.bin to $posturl"
    # perform POST with timeout and capture HTTP code
    http_out=$(mktemp)
    http_code=$(curl -sS --max-time 60 --data-binary @"$TMPDIR/test.bin" -H "Content-Type: application/octet-stream" -X POST -w "%{http_code}" -o "$http_out" "$posturl" 2>> "$SERVER_LOG" || true)
    append "POST http code: $http_code"
    if [ "$http_code" != "200" ] && [ "$http_code" != "201" ]; then
        append "POST failed or returned unexpected status $http_code; see $http_out"
    fi
    # retrieve via GET
    outget="$TMPDIR/downloaded_test.bin"
    curl -sS --max-time 30 "$base/upload/test.bin" -o "$outget" 2>> "$SERVER_LOG" || append "GET of uploaded file failed"
    if [ -f "$outget" ]; then
        sha1sum "$outget" | tee -a "$REPORT"
    else
        append "Downloaded file not found"
    fi

    # DELETE
    run_curl "DELETE upload/test.bin" -X "DELETE" "$base/upload/test.bin"

    # Unknown method (FOO) - should not crash
    append "\n-- Unknown method test --"
    curl -i -sS -X FOO "$base/" 2>> "$REPORT" | sed -n '1,40p' | tee -a "$REPORT"

    # Nonexistent URL
    run_curl "GET nonexistent" "$base/not_a_real_path_abc123"

    # CGI tests (if present)
    run_curl "CGI GET test" "$base/jfercode/cgi-bin/test.py"
    run_curl "CGI POST test" -X "POST" "$base/jfercode/cgi-bin/test.py"

done

    append "\n--- Stress test (siege or python tester) ---"
test_url="${ADDRS[0]}/"
if command -v siege > /dev/null; then
    append "siege found; running brief availability test (20s, concurrency 10)..."
    siege -b -c10 -t20S "$test_url" > logs/siege_output.txt 2>&1 || true
    sed -n '1,200p' logs/siege_output.txt | tee -a "$REPORT"
else
    append "siege not installed; using python stress tester tools/stress_test.py"
    # use moderate concurrency/duration
    ./tools/stress_test.py "$test_url" 20 20 > logs/stress_output.txt 2>&1 || true
    sed -n '1,200p' logs/stress_output.txt | tee -a "$REPORT"
fi

append "\n--- Memory monitoring (during short interval) ---"
if ps -p $SV_PID > /dev/null; then
    append "Monitoring RSS of PID $SV_PID for 10 seconds"
    maxrss=0
    for i in {1..10}; do
        rss=$(ps -p $SV_PID -o rss=)
        rss=${rss:-0}
        [ $rss -gt $maxrss ] && maxrss=$rss
        sleep 1
    done
    append "Max RSS (KB): $maxrss"
else
    append "Server PID $SV_PID not running for memory monitoring"
fi

append "\n--- Socket summary ---"
for addr in "${ADDRS[@]}"; do
    host=$(echo $addr | cut -d: -f1)
    port=$(echo $addr | cut -d: -f2)
    append "Sockets for $host:$port"
    ss -tn state all sport = :$port | sed -n '1,200p' | tee -a "$REPORT"
done

append "\n--- Cleanup: stopping server ---"
if [ -f "$SERVER_PIDFILE" ]; then
    pid=$(cat "$SERVER_PIDFILE")
    append "Killing server PID $pid"
    kill $pid >/dev/null 2>&1 || true
    sleep 1
    if ps -p $pid > /dev/null; then
        append "PID still running; sending SIGKILL"
        kill -9 $pid >/dev/null 2>&1 || true
    fi
    rm -f "$SERVER_PIDFILE"
fi

append "\nEvaluation complete. Report saved to $REPORT"

exit 0
