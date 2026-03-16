#!/usr/bin/env python3
import sys
import os

# Write something to stdout BEFORE failing (simulate partial output)
print("Content-Type: text/html")
print()
print("<html><body><h1>Starting CGI...</h1>")

# Write to stderr (server should read this)
sys.stderr.write("CGI ERROR: Something went wrong before full output!\n")
sys.stderr.flush()

# Crash the cgi script
raise RuntimeError("Intentional CGI crash for server error-handling tests")

# This will never be executed
print("<p>This should never appear.</p>")
print("</body></html>")
