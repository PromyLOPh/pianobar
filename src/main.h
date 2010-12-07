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

#ifndef _MAIN_H
#define _MAIN_H

#include <piano.h>
#include <waitress.h>

#include <sys/select.h>

#include "player.h"
#include "settings.h"

typedef struct {
	fd_set *readSet;
	int *maxFd;
	int *selectFds;
	FILE *ctlFd;
} UserInputSettings;

typedef struct {
	PianoHandle_t ph;
	WaitressHandle_t waith;
	struct audioPlayer player;
	BarSettings_t settings;
	/* first item is current song */
	PianoSong_t *playlist;
	PianoSong_t *songHistory;
	PianoStation_t *curStation;
	char doQuit;
	UserInputSettings userInputSettings;
} BarApp_t;
#ifdef __cplusplus
extern "C"{
#endif

void run(BarApp_t *app);
void loadUser(BarApp_t *app);
void loadProxy(BarApp_t *app);
int  loginUser(BarApp_t *app);
int  loadStations(BarApp_t *app);
void loadStation(BarApp_t *app);
void loadPlaylist(BarApp_t *app);
void playSong(BarApp_t *app, pthread_t *playerThread);
void handlePlayerEnded(BarApp_t *app, pthread_t *playerThread);
void moveSongToHistory(BarApp_t *app);
void handleUserInput(BarApp_t *app);
void outputTime(BarApp_t *app);

#ifdef __cplusplus
}
#endif


#endif /* _MAIN_H */

