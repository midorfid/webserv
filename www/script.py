#!/usr/bin/python3

from datetime import datetime, timedelta
import pytz

def body():
	body = "<!DOCTYPE html>"
	body += "<html>"
	body += "<head><title>Time</title></head>"
	body += "<body>"
	body += "<h1>Get the time</h1>"
	body += "<p>this could be usefull, if the time wasn't in the old Amsterdam timezone</p>"
	UTC = pytz.utc
	datetime_utc = datetime.now(UTC) + timedelta(minutes=20)
	body +=(f"The time used to be {datetime_utc.strftime('%Y:%m:%d %H:%M:%S')}")
	body += "</body>"
	body += "</html>"
	return body

payload = body()
print("HTTP/1.1 200 OK")
print("Content-Type: text/html")
print("Content-Length:", len(payload))
print("")
print(payload)