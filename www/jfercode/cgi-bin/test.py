#!/usr/bin/env python3
import os
from urllib.parse import parse_qs
import sys
import cgi

# Required header for CGI response
print("Content-Type: text/html\r\n")

# Detect request method
method = os.environ.get("REQUEST_METHOD", "GET")

print("<html><head><title>CGI Test</title></head><body>")
print(f"<h1>CGI Script Executed</h1>")
print(f"<p>Request Method: {method}</p>")

if method == "GET":
    # GET -> data comes from QUERY_STRING
    query = os.environ.get("QUERY_STRING", "")
    print(f"<h2>GET Data</h2>")
    if query:
        print(f"<p>Raw Query: {query}</p>")
        params = parse_qs(query)
        for k, v in params.items():
            values = ", ".join(v)
            print(f"<p>{k} = {values}</p>")
    else:
        print("<p>No query string received</p>")

elif method == "POST":
    # POST -> data comes from stdin, with CONTENT_LENGTH
    length = int(os.environ.get("CONTENT_LENGTH", "0"))
    post_data = sys.stdin.read(length) if length > 0 else ""
    print(f"<h2>POST Data</h2>")
    if post_data:
        print(f"<p>Raw Body: {post_data}</p>")
        params = cgi.parse_qs(post_data)
        for k, v in params.items():
            print(f"<p>{k} = {v}</p>")
    else:
        print("<p>No POST body received</p>")

else:
    print("<p>Unsupported Method</p>")

print("<hr>")
print("<a href=\"/\">Back to Home</a>")
print("</body></html>")