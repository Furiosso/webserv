#!/usr/bin/env python3
"""
Tiny POST tester for CGI endpoints. Usage: ./tools/test_cgi_post.py [URL]
Sends a small binary payload and prints status and response.
"""
import sys
import urllib.request
import urllib.error

url = sys.argv[1] if len(sys.argv) > 1 else 'http://127.0.0.1:9999/cgi-bin/test.py'
body = b"Hello from python CGI POST tester\n\x00\x01\x02"
req = urllib.request.Request(url, data=body, method='POST')
req.add_header('Content-Type', 'application/octet-stream')
req.add_header('Content-Length', str(len(body)))
try:
    with urllib.request.urlopen(req, timeout=10) as resp:
        data = resp.read()
        print('HTTP', resp.getcode())
        print('Headers:')
        for k, v in resp.getheaders():
            print(k + ':', v)
        print('\nBody:')
        sys.stdout.buffer.write(data)
        print()
except urllib.error.HTTPError as e:
    print('HTTPError', e.code, e.reason)
    try:
        print(e.read().decode('utf-8', 'replace'))
    except Exception:
        pass
except Exception as e:
    print('Error:', e)
    sys.exit(1)
