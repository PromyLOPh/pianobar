#!/bin/bash
STDIN=/tmp/eventcmd_stdin.$$
cat > $STDIN
echo "------------------" >> /tmp/eventcmd.log
date >> /tmp/eventcmd.log
env >> /tmp/eventcmd.log
echo "Args: $@" >> /tmp/eventcmd.log
echo -n "STDIN: " >> /tmp/eventcmd.log
cat $STDIN >> /tmp/eventcmd.log

# For each script you want to run (be sure to uncomment):
#cat $STDIN | /path/to/script "$@" 2>&1 >> /tmp/eventcmd.log

rm $STDIN
echo "------------------" >> /tmp/eventcmd.log
