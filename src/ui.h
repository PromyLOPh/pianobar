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

#ifndef _UI_H
#define _UI_H

#include <piano.h>
#include <waitress.h>

#include "settings.h"
#include "player.h"
#include "main.h"
#include "ui_readline.h"
#include "ui_types.h"

typedef void (*BarUiSelectStationCallback_t) (BarApp_t *app, char *buf);

void BarUiMsg (const BarSettings_t *, const BarUiMsg_t, const char *, ...);
PianoStation_t *BarUiSelectStation (BarApp_t *, PianoStation_t *, const char *,
		BarUiSelectStationCallback_t);
PianoSong_t *BarUiSelectSong (const BarSettings_t *, PianoSong_t *,
		BarReadlineFds_t *);
PianoArtist_t *BarUiSelectArtist (BarApp_t *, PianoArtist_t *);
char *BarUiSelectMusicId (BarApp_t *, PianoStation_t *, PianoSong_t *, const char *);
void BarStationFromGenre (BarApp_t *);
void BarUiPrintStation (const BarSettings_t *, PianoStation_t *);
void BarUiPrintSong (const BarSettings_t *, const PianoSong_t *, 
		const PianoStation_t *);
size_t BarUiListSongs (const BarSettings_t *, const PianoSong_t *, const char *);
void BarUiStartEventCmd (const BarSettings_t *, const char *,
		const PianoStation_t *, const PianoSong_t *, const struct audioPlayer *,
		PianoStation_t *, PianoReturn_t, WaitressReturn_t);
int BarUiPianoCall (BarApp_t * const, PianoRequestType_t,
		void *, PianoReturn_t *, WaitressReturn_t *);
void BarUiHistoryPrepend (BarApp_t *app, PianoSong_t *song);

#endif /* _UI_H */
