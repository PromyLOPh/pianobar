#!/bin/bash
#
# Usage
# =====
# 
# Set
# 
# 	event_command = /path/to/multi.sh
# 
# in pianobar’s config file. Then create the directory
# ~/.config/pianobar/eventcmd.d/, move your eventcmd scripts there and make
# them executable (chmod +x). They will be run in an unspecified order the same
# way the would have been run if pianobar called them directly (i.e. using
# event_command).

STDIN=`mktemp ${TMPDIR:-/tmp}/pianobar.XXXXXX`
cat >> $STDIN

for F in ~/.config/pianobar/eventcmd.d/*; do
	if [ -x "$F" ]; then
		"$F" $@ < "$STDIN"
	fi
done

rm "$STDIN"

