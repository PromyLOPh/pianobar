/*
Copyright (c) 2008-2010
	Lars-Dominik Braun <PromyLOPh@lavabit.com>

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

#include "settings.h"
#include "player.h"

typedef enum {MSG_NONE, MSG_INFO, MSG_PLAYING, MSG_TIME, MSG_ERR,
		MSG_QUESTION, MSG_LIST} uiMsg_t;

void BarUiMsg (uiMsg_t type, const char *format, ...);
PianoReturn_t BarUiPrintPianoStatus (PianoReturn_t ret);
PianoStation_t *BarUiSelectStation (PianoHandle_t *ph, const char *prompt,
		FILE *curFd);
PianoSong_t *BarUiSelectSong (PianoSong_t *startSong, FILE *curFd);
PianoArtist_t *BarUiSelectArtist (PianoArtist_t *startArtist, FILE *curFd);
char *BarUiSelectMusicId (PianoHandle_t *ph, FILE *curFd, char *);
void BarStationFromGenre (PianoHandle_t *ph, FILE *curFd);
void BarUiPrintStation (PianoStation_t *);
void BarUiPrintSong (PianoSong_t *, PianoStation_t *);
void BarUiStartEventCmd (const BarSettings_t *, const char *,
		const PianoStation_t *, const PianoSong_t *,
		const struct audioPlayer *, PianoReturn_t);

#endif /* _UI_H */
