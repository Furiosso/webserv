#!/usr/bin/env bash
# CGI POST focused tests for webserv
BASE="http://localhost:9999"
FAIL=0

check(){
  NAME="$1"; shift
  EXPECT="$1"; shift
  CMD=("$@")
  echo "==> $NAME"
  TMP_RESP=$(mktemp /tmp/wsg_cgi_test.XXXXXX)
  HTTP_CODE=$("${CMD[@]}" -o "$TMP_RESP" 2>/dev/null -w "%{http_code}")
  if [ "$HTTP_CODE" = "$EXPECT" ]; then
    echo "  [OK] got $HTTP_CODE"
  else
    echo "  [FAIL] got $HTTP_CODE expected $EXPECT"
    echo "----- response -----"
    cat "$TMP_RESP"
    echo "--------------------"
    FAIL=1
  fi
  rm -f "$TMP_RESP"
}

# small binary helper
dd if=/dev/zero bs=1024 count=1 2>/dev/null | tr '\0' 'B' > /tmp/wsg_cgi_1k.bin

# 1) simple form-encoded POST to CGI
check "CGI form POST (200)" 200 curl -s -i -X POST "$BASE/cgi-bin/test.py" --data 'a=1&b=2' -H "Content-Type: application/x-www-form-urlencoded"

# 2) raw binary POST to CGI
check "CGI binary POST (200)" 200 curl -s -i -X POST "$BASE/cgi-bin/test.py" --data-binary @/tmp/wsg_cgi_1k.bin -H "Content-Type: application/octet-stream"

# 3) Expect: 100-continue header
check "CGI Expect 100-continue (200)" 200 curl -s -i -X POST "$BASE/cgi-bin/test.py" --data 'x' -H "Expect: 100-continue"

# 4) Chunked transfer encoding (use printf raw request via netcat) - try via curl with --chunked
CHUNKED_RESP=$(printf "POST /cgi-bin/test.py HTTP/1.1\r\nHost: localhost:9999\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n" | nc -q 1 localhost 9999 2>/dev/null)
CHUNKED_CODE=$(echo "$CHUNKED_RESP" | awk '/^HTTP\//{code=$2} END{print code}')
echo "==> CGI chunked POST (200)"
if [ "$CHUNKED_CODE" = "200" ]; then
  echo "  [OK] got $CHUNKED_CODE"
else
  echo "  [FAIL] got $CHUNKED_CODE expected 200"
  echo "----- response -----"
  echo "$CHUNKED_RESP"
  echo "--------------------"
  FAIL=1
fi

# 5) Large body test to make sure server handles more data and pipes to CGI
dd if=/dev/urandom bs=1024 count=64 2>/dev/null > /tmp/wsg_cgi_64k.bin
check "CGI large POST (200)" 200 curl -s -i -X POST "$BASE/cgi-bin/test.py" --data-binary @/tmp/wsg_cgi_64k.bin -H "Content-Type: application/octet-stream" --max-time 30

# summary
if [ $FAIL -ne 0 ]; then
  echo "Some CGI POST tests failed"
  exit 1
else
  echo "All CGI POST tests passed"
  exit 0
fi
