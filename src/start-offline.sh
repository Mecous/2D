#!/bin/sh

exec ./start.sh --offline-client-mode --debug --debug-server-logging ${1+"$@"}

