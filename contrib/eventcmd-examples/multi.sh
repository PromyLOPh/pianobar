#!/bin/bash
#
# Executes all scripts located in ~/.config/pianobar/eventcmd.d/ as if they
# were called by pianobar directly

STDIN=`mktemp ${TMPDIR:-/tmp}/pianobar.XXXXXX`
cat >> $STDIN

for F in ~/.config/pianobar/eventcmd.d/*; do
	if [ -x "$F" ]; then
		"$F" $@ < "$STDIN"
	fi
done

rm "$STDIN"

