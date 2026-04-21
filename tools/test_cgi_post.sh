#!/bin/sh
# Simple POST test for a CGI endpoint. Usage: ./tools/test_cgi_post.sh [URL]
URL=${1:-http://127.0.0.1:9999/cgi-bin/test.py}
TMP=$(mktemp /tmp/cgipost.XXXX)
printf "Hello CGI POST from curl\n" > "$TMP"
echo "Posting $TMP to $URL"
HTTP_CODE=$(curl -sS --max-time 10 -o /tmp/cgi_post_response.txt -w "%{http_code}" -H "Content-Type: application/octet-stream" --data-binary @"$TMP" "$URL")
echo "HTTP: $HTTP_CODE"
echo "Response body:"
cat /tmp/cgi_post_response.txt
rm -f "$TMP" /tmp/cgi_post_response.txt
exit 0
