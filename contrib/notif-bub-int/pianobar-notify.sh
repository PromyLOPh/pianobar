#!/bin/bash

# Copyright (c) 2011
# Artur de S. L. Malabarba

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

###### USAGE: ######
# 
# This is an event script. Place it somewhere convenient for you and
# add the line 'event_command = /PATH/TO/pianobar-notify.sh' to your
# pianobar config file.
#
# Also check if this matches you config folder
fold="$HOME/.config/pianobar"

# You should also place the control-pianobar.sh script in the
# config folder (or modify the following variable accordingly).
controlpianobar="$fold/control-pianobar.sh"

# Also place the pandora.jpg file in the same folder, or modify de
# following variable.
blankicon="$fold/pandora.jpg"

# Some of the following was copied from eventcmd.sh
notify="notify-send --hint=int:transient:1"
logf="$fold/log"
ctlf="$fold/ctl"
an="$fold/artname"
np="$fold/nowplaying"
ds="$fold/durationstation"
dse="$fold/durationstationexplain"
su="$fold/showupcoming"
stl="$fold/stationlist"
ip="$fold/isplaying"
ine="$fold/ignextevent"

while read L; do
    k="`echo "$L" | cut -d '=' -f 1`"
    v="`echo "$L" | cut -d '=' -f 2`"
    export "$k=$v"
done < <(grep -e '^\(title\|artist\|album\|stationName\|pRet\|pRetStr\|wRet\|wRetStr\|songDuration\|songPlayed\|rating\|coverArt\|stationCount\|station[0-9]\+\)=' /dev/stdin) # don't overwrite $1...


[[ "$rating" == 1 ]] && like="(like)"
playpause="||"

[[ "$(cat "$ip")" == 1 ]] && playpause="|>"
icon=`echo "$artist-$album.jpg" | sed 's/\//_/g'`
songPlayed="$(($songDuration-$songPlayed))"
dursec="$(($(($songDuration-$(($songDuration/60000*60000))))/1000))"
plasec="$(($(($songPlayed-$(($songPlayed/60000*60000))))/1000))"

if [[ $dursec -lt 10 ]]; then
    duration="$(($songDuration/60000)):"0$dursec
else
    duration="$(($songDuration/60000)):"$dursec
fi

if [[ $plasec -lt 10 ]]; then
    played="$(($songPlayed/60000)):"0$plasec
else
    played="$(($songPlayed/60000)):"$plasec
fi

if [[ $songDuration -gt 10 ]]; then
    echo -e "\-$played/$duration \t $playpause
Album: $album 
Station: $stationName" > "$ds"
else
    echo "
Album: $album
Station: $stationName" > "$ds"
fi
echo "$artist - $title $like" > "$np"


case "$1" in
    songstart)
	   echo "1" > "$ip"
	   cd "$fold/albumart" 

	   if [[ ! -e  "$icon" ]]; then 
		  echo "No icon yet" >> test
		  if [[  -n "$coverArt" ]]; then
			 echo "Getting cover art from $coverArt" >> test
	    		 wget -q -O "$icon" "$coverArt" 
			 echo "$fold/albumart/$icon" > $an
		  else
			 echo "$blankicon" > $an
		  fi
	   else
		  echo "$fold/albumart/$icon" > $an
	   fi

	   echo "Running $notify -t 7000 -i \"`cat $an`\" \"`cat $np`\" \"`cat $ds`\"" >> test;
	   $notify -t 7000 -i "`cat $an`" "`cat $np`" "`cat $ds`"
	   echo "" > "$logf"

	   if [[ -e "$su" ]]; then
		  $controlpianobar u 7 &
		  rm -f "$su"
	   fi;;
    
    songexplain)
	   cp "$ds" "$dse"
	   tail -1 "$logf" | grep --text "(i) We're" | sed 's/.*(i).*features/*/' | sed 's/,/\n*/g' | sed 's/and \([^,]*\)\./\n* \1/' | sed 's/\* many other similarities.*/* and more./' >> "$dse"
	   $notify -t 15000 -i "`cat $an`" "`cat $np`" "`cat $dse`";;
    
    songlove)
	   if [[ -e "$ine" ]]; then
		  $notify -t 2500  "Song Liked" ""
		  rm -f "$ine"
	   else
		  $notify -t 2500 -i "`cat $an`" "Song Liked" "$artist - $title"
	   fi;;
    
    songban)
	   if [[ -e "$ine" ]]; then
		  $notify -t 2500  "Song Banned" ""
		  rm -f "$ine"
	   else
		  $notify -t 2500 -i "`cat $an`" "Song Banned" "$artist - $title"
	   fi;;
    
    songshelf)
	   if [[ -e "$ine" ]]; then
		  $notify -t 2500  "Song Put Away" ""
		  rm -f "$ine"
	   else
		  $notify -t 2500 -i "`cat $an`" "Song Put Away" "$artist - $title"
	   fi;;
    
    stationfetchplaylist)
	   echo "1" > "$su";;
    
    usergetstations)
	   if [[ $stationCount -gt 0 ]]; then
		  rm -f "$stl"
		  for stnum in $(seq 0 $(($stationCount-1))); do
			 echo "$stnum) "$(eval "echo \$station$stnum") >> "$stl"
		  done
	   fi
	   echo "$($zenity --entry --title="Switch Station" --text="$(cat "$stl")")" > "$ctlf"
	   ;;
    
    userlogin)
	   if [ "$pRet" -ne 1 ]; then
		  $notify  -t 1500 "Login ERROR 1" "$pRetStr"
		  $notify  -t 6000 "Restarting Tor" "Input root password, wait a few seconds, and try running pianobar again."
            sleep 1.5
		  xterm -e sudo rc.d restart tor && $notify -t 1000 "Success!"
	    # if [[ $? -eq 0 ]]; then
	    #     $notify  "Failure" "Sorry, that's all I can do."
	    # else
	    #     $notify  "Success" "I think it worked. Try running pianobar again."
	    # fi		
	   elif [ "$wRet" -ne 1 ]; then
		  $notify  "Login ERROR 2" "$wRetStr"
	   else
		  $notify -t 2000 "Login Successful" "Fetching Stations..."
	   fi
	   ;;
    
    songfinish)
	   exit;;
    
    *)
	   if [ "$pRet" -ne 1 ]; then
		  $notify  -i "$blankicon" "Pianobar - ERROR" "$1 failed: $pRetStr"
	   elif [ "$wRet" -ne 1 ]; then
		  $notify  -i "$blankicon" "Pianobar - ERROR" "$1 failed: $wRetStr"
	   else
		  $notify  -i "$blankicon" "$1" "fill $2"
	   fi;;
esac
