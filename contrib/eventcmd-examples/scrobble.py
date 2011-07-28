#!/usr/bin/env python

"""
Last.fm scrobbling for Pianobar, the command-line Pandora client.

Copyright (c) 2011
Jon Pierce <jon@jonpierce.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Dependencies:
1) https://github.com/PromyLOPh/pianobar/
2) http://python.org/
3) http://code.google.com/p/pylast/
4) http://www.last.fm/api/account

Installation:
1) Copy this script and pylast.py to the Pianobar config directory, ~/.config/pianobar/, and make sure this script is executable
2) Supply your own Last.fm credentials below
3) Update Pianobar's config file to use this script as its event_command
"""

import sys
import time

API_KEY = "################################"
API_SECRET = "################################"
USERNAME = "########"
PASSWORD = "########"
THRESHOLD = 50 # the percentage of the song that must have been played to scrobble

def main():

  event = sys.argv[1]
  lines = sys.stdin.readlines()
  fields = dict([line.strip().split("=", 1) for line in lines])
  
  # fields: title, artist, album, songDuration, songPlayed, rating, stationName, pRet, pRetStr, wRet, wRetStr
  artist = fields["artist"]
  title = fields["title"]
  song_duration = int(fields["songDuration"])
  song_played = int(fields["songPlayed"])

  # events: songstart, songfinish, ???
  if event == "songfinish" and 100.0 * song_played / song_duration > THRESHOLD:
    song_started = int(time.time() - song_played / 1000.0)
    import pylast
    network = pylast.LastFMNetwork(api_key = API_KEY, api_secret = API_SECRET, username = USERNAME, password_hash = pylast.md5(PASSWORD))
    network.scrobble(artist = artist, title = title, timestamp = song_started)

if __name__ == "__main__":
  main()

