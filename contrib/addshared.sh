#!/bin/sh

# extract shared station id from station homepage and add it to pianobar

ctl="$HOME/.config/pianobar/ctl"

if [ -z "$1" ]; then
	echo "Usage: `basename $0` <station url>"
	exit 1
fi

curl -s "$1" | sed -nre "s#.*launchStationFromId\('([0-9]+)'\).*#j\1#gp" > "$ctl"

exit 0

