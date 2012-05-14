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

# To use this script, place this script, pianobar-notify.sh, and
# pandora.jpg in your config folder and configure the three variables below
# according to your needs.
#
# # # Start pianobar by running 'control-pianobar.sh p'
# 
# These should match YOUR configs
  # Your config folder
  fold="$HOME/.config/pianobar"   
  # The pianobar executable
  pianobar="$HOME/pianobar/pianobar" 
  # A blank icon to show when no albumart is found. I prefer to use
  # the actual pandora icon for this, which you can easily find and
  # download yourself.
  blankicon="$fold/pandora.jpg"		

# You probably shouldn't mess with these (or anything else)
notify="notify-send --hint=int:transient:1"
logf="$fold/log"
ctlf="$fold/ctl"
an="$fold/artname"
np="$fold/nowplaying"
ds="$fold/durationstation"
ip="$fold/isplaying"
ine="$fold/ignextevent"
stl="$fold/stationlist"

[[ -n "$2" ]] &&  sleep "$2"

echo "" > "$logf"
case $1 in
    p|pause|play)
	   if [[ -n `pidof pianobar` ]]; then
		  echo -n "p" > "$ctlf"
		  if [[ "$(cat $ip)" == 1 ]]; then
			 echo "0" > "$ip"
			 $notify -t 2500 -i  "`cat $an`" "Song Paused" "`cat $fold/nowplaying`"
		  else
			 echo "1" > "$ip"
			 $notify -t 2500 -i "`cat $an`" "Song Playing" "`cat $fold/nowplaying`"
		  fi
	   else
		  mkdir -p "$fold/albumart"
		  rm "$logf" 2> /dev/null
		  rm "$ctlf" 2> /dev/null
		  mkfifo "$ctlf"
		  $notify -t 2500 "Starting Pianobar" "Logging in..."
		  "$pianobar" | tee "$logf"
	   fi;;
    
    love|l|+)
	   echo -n "+" > "$ctlf" ;;
    
    ban|b|-|hate)
	   echo -n "-" > "$ctlf" ;;
    
    next|n)
	   echo -n "n" > "$ctlf" ;;
    
    stop|quit|q)
	   $notify -t 1000 "Quitting Pianobar"
	   echo -n "q" > "$ctlf" 
	   echo "0" > "$ip"
	   sleep 1
	   if [[ -n $(pidof pianobar) ]]; then
		  $notify -t 1000 "Oops" "Something went wrong. \n Force quitting..."
		  kill -9 $(pidof pianobar)
		  if [[ -n $(pidof pianobar) ]]; then
			 $notify -t 2000 "I'm Sorry" "I don't know what's happening. Could you try killing it manually?"
		  else
			 $notify -t 2000 "Success" "Pianobar closed."
		  fi
	   fi;;
    
    explain|e)
	   echo -n "e" > "$ctlf" ;;
    
    playing|current|c)
	   sleep 1
	   time="$(grep  "#" "$logf" --text | tail -1 | sed 's/.*# \+-\([0-9:]\+\)\/\([0-9:]\+\)/\\\\-\1\\\/\2/')"
	   $notify -t 5000 -i "`cat $an`" "$(cat "$np")" "$(sed "1 s/.*/$time/" "$ds")";;
    
    nextstation|ns)
	   stat="$(grep --text "^Station: " "$ds" | sed 's/Station: //')"
	   newnum="$((`grep --text "$stat" "$stl" | sed 's/\([0-9]\+\)).*/\1/'`+1))"
	   newstt="$(sed "s/^$newnum) \(.*\)/-->\1/" "$stl" | sed 's/^[0-9]\+) \(.*\)/* \1/')"
	   if [[ -z "$(grep "^-->" "$newstt")" ]]; then
		  newnum=0
		  newstt="$(sed "s/^$newnum) \(.*\)/-->\1/" "$stl" | sed 's/^[0-9]\+) \(.*\)/* \1/')"
	   fi
	   echo "s$newnum" > "$ctlf"
	   $notify -t 2000 "Switching station" "$newstt";;
    
    prevstation|ps)
	   stat="$(grep --text "^Station: " "$ds" | sed 's/Station: //')"
	   newnum="$((`grep --text "$stat" "$stl" | sed 's/\([0-9]\+\)).*/\1/'`-1))"
	   [[ "$newnum" -lt 0 ]] && newnum=$(($(wc -l < "$stl")-1))
	   newstt="$(sed "s/^$newnum) \(.*\)/-->\1/" "$stl" | sed 's/^[0-9]\+) \(.*\)/* \1/')"
	   echo "s$newnum" > "$ctlf"
	   $notify -t 2000 "Switching station" "$newstt";;
    
    switchstation|ss)
	   text="$(grep --text "[0-9]\+)" "$logf" | sed 's/.*\t\(.*)\) *\(Q \+\)\?\([^ ].*\)/\1 \3/')""\n \n Type a number."
	   newnum="$($zenity --entry --title="Switch Station" --text="$(cat "$stl")\n Pick a number.")"
	   if [[ -n "$newnum" ]]; then
		  newstt="$(sed "s/^$newnum) \(.*\)/-->\1/" "$stl" | sed 's/^[0-9]\+) \(.*\)/* \1/')"
		  echo "s$newnum" > "$ctlf"
		  $notify -t 2000 "Switching station" "$newstt"
	   fi;;
    
    upcoming|queue|u)
	   echo -n "u" > "$ctlf"
	   sleep .5
	   list="$(grep --text '[0-9])' $logf | sed 's/.*\t [0-9])/*/')"
	   if [[ -z "$list" ]]; then
		  $notify "No Upcoming Songs" "This is probably the last song in the list."
	   else
		  $notify -t 5000 "Upcoming Songs" "$list"
	   fi;;
    
    "history"|h)
	   echo -n "h"  > "$ctlf"
	   text="$(grep --text "[0-9]\+)" "$logf" | sed 's/.*\t\(.*) *[^ ].*\)/\1/')""\n \n Type a number."
	   snum="$($zenity --entry --title="History" --text="$text")"
	   if [[ -n "$snum" ]]; then
		  echo "1" > "$ine"
		  echo  "$snum"  > "$ctlf"
		  echo -n "$($zenity --entry --title="Do what?" --text="Love[+], Ban[-], or Tired[t].")" > "$ctlf"
	   else
		  echo "" > "$ctlf"
	   fi;;
    
    *)

	   
	   echo -e "
    \\033[1mWhat's the point?\\033[0m
    This script takes a single argument. Possible arguments are:
\\033[1;34m    play, love, hate, next, stop, explain, playing, upcoming, 
  history, (next|prev|switch)station\\033[0m. 
  The behavior is mostly the same as running the respective action 
  inside pianobar, except you interact with notification bubbles. 

    THIS INTERACTION WILL ONLY WORK CORRECTLY IF YOU START PIANOBAR
  THROUGH THE SCRIPT! SEE **USAGE** FOR INSTRUCTION.

    This script is meant to be used as a hidden interface for
  pianobar. It is invoked by keyboard shortcuts assigned by you, and
  interacts with you through the use of pretty notification
  bubbles. The point is that you are able to interact with pianobar
  without having to focus the terminal you invoked it in. It also
  shows album art, which pianobar can't do since it is terminal-
  focused. If you prefer, you could also assign aliases instead of
  keyboard shortcuts.

    \\033[1;31mIMPORTANT:\\033[0;31m 
    This script depends on two commands: zenity and notify-send. As of
  this writting libnotify-bin provides a buggy version of the
  notify-send command in Ubuntu, so some options of the command don't
  work. On Ubuntu, it is recommended that you install some patched[1]
  version of the package that fixes the bug. Without this fix, you
  might not get album art, and all your notification will have the
  same duration of 10 seconds, which gets annoying fast. On other
  systems, you'll get varying behavior.  It is also recommended that
  you create a file \".notify-osd\" in your \$HOME with the line
  \"bubble-icon-size = 70px\". This will make the album art icons
  bigger and more visible, but be aware that it will also affect
  notifications from other software.\\033[0;30m

    \\033[1mUSAGE:\\033[0m
  Bind \"PATH/TO/control-pianobar.sh <argument>\" to a hotkey in your
  distro's keyboard shortcut manager. Start pianobar with the command
  \"PATH/TO/control-pianobar.sh p\" (or the appropriate hotkey.

    \\033[1m Suggestions:\\033[0m
  Bind this key		- To this command
 \\033[1;34m Media Play/Pause\\033[0m	- control-pianobar.sh p;
 \\033[1;34m Media Stop\\033[0m		- control-pianobar.sh quit;
 \\033[1;34m Media Previous\\033[0m	- control-pianobar.sh history;
 \\033[1;34m Media Next\\033[0m		- control-pianobar.sh next;
 \\033[1;34m Ctrl + Media Previous\\033[0m - control-pianobar.sh previousstation;
 \\033[1;34m Ctrl + Media Next\\033[0m	- control-pianobar.sh nextstation;
 \\033[1;34m Browser Favorites\\033[0m	- control-pianobar.sh love;
 \\033[1;34m Browser Stop\\033[0m		- control-pianobar.sh ban;
 \\033[1;34m Browser Search\\033[0m	- control-pianobar.sh explain;
 \\033[1;34m Super + Search\\033[0m	- control-pianobar.sh switchstation;

    This script does take a second argument, but the user need not
  worry about it. It's used by notify-pianobar.sh to make some
  notifications behave right.  If a second argument is provided, it
  must be a positive integer. This number is the number of seconds the
  script waits before running.

  [1]From the date of this writting (2011) I am using a patched version
  found in the ppa ppa:leolik/leolik. I take no responsibility
  regarding the contents of this ppa, I'm simply stating it's the one
  I used.
  http://www.webupd8.org/2010/05/finally-easy-way-to-customize-notify.html";;
esac
