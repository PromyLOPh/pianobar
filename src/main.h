/*
Copyright (c) 2008-2011
	Lars-Dominik Braun <lars@6xq.net>

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
*/

#pragma once

#include <curl/curl.h>

#include <piano.h>

#include "player.h"
#include "settings.h"
#include "ui_readline.h"

typedef struct {
	PianoHandle_t ph;
	CURL *http;
	player_t player;
	BarSettings_t settings;
	/* first item is current song */
	PianoSong_t *playlist;
	PianoSong_t *songHistory;
	/* station of current song and station used to fetch songs from if playlist
	 * is empty */
	PianoStation_t *curStation, *nextStation;
	sig_atomic_t doQuit;
	BarReadlineFds_t input;
	unsigned int playerErrors;
} BarApp_t;

#include <signal.h>
extern sig_atomic_t *interrupted;

