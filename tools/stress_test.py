#!/usr/bin/env python3
"""
Simple Python stress tester: spawn N threads that repeatedly GET a URL for T seconds.
Prints summary: total requests, failures, success rate.
"""
import sys
import threading
import time
import urllib.request

if len(sys.argv) < 4:
    print("Usage: stress_test.py <url> <concurrency> <seconds>")
    sys.exit(2)

url = sys.argv[1]
concurrency = int(sys.argv[2])
duration = int(sys.argv[3])

stop_time = time.time() + duration
lock = threading.Lock()
stats = { 'total': 0, 'ok': 0, 'fail': 0 }


def worker():
    global stats
    while time.time() < stop_time:
        try:
            resp = urllib.request.urlopen(url, timeout=5)
            with lock:
                stats['total'] += 1
                if resp.getcode() == 200:
                    stats['ok'] += 1
                else:
                    stats['fail'] += 1
        except Exception:
            with lock:
                stats['total'] += 1
                stats['fail'] += 1

threads = []
for i in range(concurrency):
    t = threading.Thread(target=worker)
    t.daemon = True
    t.start()
    threads.append(t)

for t in threads:
    t.join(timeout=duration + 2)

print("Stress summary for {}: concurrency={}, duration={}".format(url, concurrency, duration))
print("Total requests: {}  OK: {}  Fail: {}".format(stats['total'], stats['ok'], stats['fail']))
if stats['total'] > 0:
    print("Availability: {:.2f}%".format(100.0 * stats['ok'] / stats['total']))
else:
    print("No requests performed")
