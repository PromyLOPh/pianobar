#!/bin/bash

# For this to work, whatever pianobar is using as event_command needs to do
# something like:
# print $sfh "$1. $2\n" if (/station(\d+)=(.+)$/);
# into /tmp/pianobar_stations

STATION=$(cat /tmp/pianobar_stations | \
	dmenu -nf '#888888' -nb '#222222' -sf '#ffffff' -i \
	-sb '#285577' -p 'choose station:' -fn 'Terminus 8' | \
	sed -e 's/\([0-9]\+\)\..*/\1/')


[[ -n $STATION ]] && echo "s$STATION" > ~/.config/pianobar/ctl

