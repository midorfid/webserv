#!/usr/bin/python3

from datetime import datetime, timedelta
from zoneinfo import ZoneInfo
import sys
# import pytz

def body():
	body = "<!DOCTYPE html>"
	body += "<html>"
	body += "<head><title>Time</title></head>"
	body += "<body>"
	body += "<h1>Get the time</h1>"
	body += "<p>this could be usefull, if the time wasn't in the old Amsterdam timezone</p>"
	datetime_utc = datetime.now(ZoneInfo('Europe/Amsterdam')) + timedelta(minutes=20)
	body +=(f"The time used to be {datetime_utc.strftime('%Y:%m:%d %H:%M:%S')}")
	body += "</body>"
	body += "</html>"
	return body

payload = body()
print("Status: 200 OK", end="\r\n")
print("Content-Type: text/html", end="\r\n")
print("Content-Length:", len(payload), end="\r\n")
print("", end="\r\n")
print(payload, end="", flush=True)