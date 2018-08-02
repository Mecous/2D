#!/bin/sh

rm -f /tmp/HELIOS*.log /tmp/HELIOS*.ocl /tmp/HELIOS*.dcl

exec ./start.sh --offline-logging --debug --debug-server-connect ${1+"$@"}
