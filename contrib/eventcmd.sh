#!/bin/bash

# create variables
while read L; do
	k="`echo "$L" | cut -d '=' -f 1`"
	v="`echo "$L" | cut -d '=' -f 2`"
	export "$k=$v"
done < <(grep -e '^\(title\|artist\|album\|stationName\|pRet\|pRetStr\|wRet\|wRetStr\|songDuration\|songPlayed\|rating\|coverArt\)=' /dev/stdin) # don't overwrite $1...

case "$1" in
#	songstart)
#		echo 'naughty.notify({title = "pianobar", text = "Now playing: ' "$title" ' by ' "$artist" '"})' | awesome-client -
#		echo "$title -- $artist" > $HOME/.config/pianobar/nowplaying
#		# or whatever you like...
#		;;

#	songfinish)
#		# scrobble if 75% of song have been played, but only if the song hasn't
#		# been banned
#		if [ -n "$songDuration" ] &&
#				[ $(echo "scale=4; ($songPlayed/$songDuration*100)>50" | bc) -eq 1 ] &&
#				[ "$rating" -ne 2 ]; then
#			# scrobbler-helper is part of the Audio::Scrobble package at cpan
#			# "pia" is the last.fm client identifier of "pianobar", don't use
#			# it for anything else, please
#			scrobbler-helper -P pia -V 1.0 "$title" "$artist" "$album" "" "" "" "$((songDuration/1000))" &
#		fi
#		;;

	*)
		if [ "$pRet" -ne 1 ]; then
			echo "naughty.notify({title = \"pianobar\", text = \"$1 failed: $pRetStr\"})" | awesome-client -
		elif [ "$wRet" -ne 1 ]; then
			echo "naughty.notify({title = \"pianobar\", text = \"$1 failed: Network error: $wRetStr\"})" | awesome-client -
		fi
		;;
esac

