#!/usr/bin/python3
import sys
import time

# Notice there is NO Content-Length header here!
sys.stdout.write("Status: 200 OK\r\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.flush()

# Stream 3 chunks of data, pausing for 1 second between each
for i in range(3):
    sys.stdout.write(f"Data stream packet {i}...\n")
    sys.stdout.flush()
    time.sleep(1)