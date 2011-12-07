#!/bin/bash
STDIN=/tmp/eventcmd_stdin.$$
cat > $STDIN

# For each script you want to run (be sure to uncomment):
#cat $STDIN | /path/to/script "$@"

rm $STDIN
