#!/bin/sh

echocmd="/bin/echo -n"
ctlfile="$HOME/.config/pianobar/ctl"

# pianobar running? echo would block otherwise
ps -C 'pianobar' > /dev/null

if [ $? -ne 0 ]; then
	echo 'naughty.notify({title = "pianobar", text = "Not running"})' | awesome-client -
	exit 1;
fi

case "$1" in
	pp)
		$echocmd p > $ctlfile
		;;
	next)
		$echocmd n > $ctlfile
		;;
	love)
		$echocmd + > $ctlfile
		;;
	ban)
		$echocmd - > $ctlfile
		;;
esac
