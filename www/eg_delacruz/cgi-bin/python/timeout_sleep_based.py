#!/usr/bin/env python3
import time

time.sleep(10)

print("Content-Type: text/plain\r\n")
print("This CGI should have timed out, but if you see this, the timeout failed.")
