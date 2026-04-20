#!/usr/bin/env bash
# Basic POST tests for webserv (adjust BASE if needed)
BASE="http://localhost:9999"
FAIL=0

check(){
  NAME="$1"; shift
  EXPECT="$1"; shift
  CMD=("$@")
  echo "==> $NAME"
  # run curl and capture status and full response
  RESP=$("${CMD[@]}" 2>/dev/null)
  # pick the last HTTP status line (handles intermediate 100 Continue)
  CODE=$(echo "$RESP" | awk '/^HTTP\//{code=$2} END{print code}')
  if [ "$CODE" = "$EXPECT" ]; then
    echo "  [OK] got $CODE"
  else
    echo "  [FAIL] got $CODE expected $EXPECT"
    echo "----- response -----"
    echo "$RESP"
    echo "--------------------"
    FAIL=1
  fi
}

# Ensure tmp file for binary tests
dd if=/dev/zero bs=1024 count=2 2>/dev/null | tr '\0' 'A' > /tmp/wsg_test_2k.bin

# Tests
check "Create new file (201)" 201 curl -s -i -X POST "$BASE/upload/newfile_test.txt" --data 'hello'
check "POST to CGI (200)" 200 curl -s -i -X POST "$BASE/cgi-bin/test.py" --data 'a=1' -H "Content-Type: application/x-www-form-urlencoded"
check "Expect 100-continue (201/200)" 201 curl -s -i -X POST "$BASE/upload/expect.txt" --data 'x' -H "Expect: 100-continue"
check "Upload small binary (201)" 201 curl -s -i -X POST "$BASE/upload/bin.bin" --data-binary @/tmp/wsg_test_2k.bin -H "Content-Type: application/octet-stream"

# summary
if [ $FAIL -ne 0 ]; then
  echo "Some tests failed"
  exit 1
else
  echo "All basic tests passed"
  exit 0
fi
