/*
Copyright (c) 2008-2018
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

#include <stdbool.h>

#include <piano.h>

#include "settings.h"
#include "player.h"
#include "main.h"
#include "ui_readline.h"
#include "ui_types.h"

typedef void (*BarUiSelectStationCallback_t) (BarApp_t *app, char *buf);

void BarUiMsg (const BarSettings_t *, const BarUiMsg_t, const char *, ...) __attribute__((format(printf, 3, 4)));
PianoStation_t *BarUiSelectStation (BarApp_t *, PianoStation_t *, const char *,
		BarUiSelectStationCallback_t, bool);
PianoSong_t *BarUiSelectSong (const BarApp_t * const app,
		PianoSong_t *startSong, BarReadlineFds_t *input);
PianoArtist_t *BarUiSelectArtist (BarApp_t *, PianoArtist_t *);
char *BarUiSelectMusicId (BarApp_t *, PianoStation_t *, const char *);
void BarUiPrintStation (const BarSettings_t *, PianoStation_t *);
void BarUiPrintSong (const BarSettings_t *, const PianoSong_t *, 
		const PianoStation_t *);
size_t BarUiListSongs (const BarApp_t * const app,
		const PianoSong_t *song, const char *filter);
void BarUiStartEventCmd (const BarSettings_t *, const char *,
		const PianoStation_t *, const PianoSong_t *, player_t *,
		PianoStation_t *, PianoReturn_t, CURLcode);
bool BarUiPianoCall (BarApp_t * const, const PianoRequestType_t,
		void *, PianoReturn_t *, CURLcode *);
void BarUiHistoryPrepend (BarApp_t *app, PianoSong_t *song);

