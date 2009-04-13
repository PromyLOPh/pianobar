#!/bin/bash

# create variables
while read L; do
	k="`echo "$L" | cut -d '=' -f 1`"
	v="`echo "$L" | cut -d '=' -f 2`"
	export "$k=$v"
done < <(grep -e '^\(title\|artist\|album\|stationName\|pRet\|pRetStr\)=' /dev/stdin) # don't overwrite $1...

case "$1" in
	songstart)
		#echo 'naughty.notify({title = "pianobar", text = "Now playing: ' "$title" ' by ' "$artist" '"})' | awesome-client -
		#echo "$title -- $artist" > $HOME/.config/pianobar/nowplaying
		# or whatever you like...
		;;
	*)
		if [ $pRet -ne 0 ]; then
			echo "naughty.notify({title = \"pianobar\", text = \"$1 failed: $pRetStr\"})" | awesome-client -
		fi
		;;
esac

