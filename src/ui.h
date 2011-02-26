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

typedef enum {MSG_NONE, MSG_INFO, MSG_PLAYING, MSG_TIME, MSG_ERR,
		MSG_QUESTION, MSG_LIST} uiMsg_t;

void BarUiMsg (uiMsg_t type, const char *format, ...);
PianoReturn_t BarUiPrintPianoStatus (PianoReturn_t ret);
PianoStation_t *BarUiSelectStation (PianoHandle_t *, const char *,
		BarStationSorting_t, BarReadlineFds_t *);
PianoSong_t *BarUiSelectSong (const BarSettings_t *, PianoSong_t *,
		BarReadlineFds_t *);
PianoArtist_t *BarUiSelectArtist (PianoArtist_t *, BarReadlineFds_t *);
char *BarUiSelectMusicId (BarApp_t *, char *);
void BarStationFromGenre (BarApp_t *);
void BarUiPrintStation (PianoStation_t *);
void BarUiPrintSong (const BarSettings_t *, const PianoSong_t *, 
		const PianoStation_t *);
size_t BarUiListSongs (const BarSettings_t *, const PianoSong_t *);
void BarUiStartEventCmd (const BarSettings_t *, const char *,
		const PianoStation_t *, const PianoSong_t *, const struct audioPlayer *,
		PianoStation_t *, PianoReturn_t, WaitressReturn_t);
int BarUiPianoCall (BarApp_t * const, PianoRequestType_t,
		void *, PianoReturn_t *, WaitressReturn_t *);
void BarUiHistoryPrepend (BarApp_t *app, PianoSong_t *song);

#endif /* _UI_H */
