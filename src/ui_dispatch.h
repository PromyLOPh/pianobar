/*
Copyright (c) 2010-2011
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

#ifndef _UI_DISPATCH_H
#define _UI_DISPATCH_H

/* bit-mask */
typedef enum {
	BAR_DC_UNDEFINED = 0,
	BAR_DC_GLOBAL = 1, /* top-level action */
	BAR_DC_STATION = 2, /* station selected */
	BAR_DC_SONG = 4, /* song selected */
} BarUiDispatchContext_t;

#include "settings.h"
#include "main.h"

typedef void (*BarKeyShortcutFunc_t) (BarApp_t *, PianoStation_t *,
		PianoSong_t *, BarUiDispatchContext_t);

typedef struct {
	BarUiDispatchContext_t context;
	BarKeyShortcutFunc_t function;
	char *helpText;
	char defaultKey;
	char *configKey;
} BarUiDispatchAction_t;

#include "ui_act.h"

/* see settings.h */
static const BarUiDispatchAction_t dispatchActions[BAR_KS_COUNT] = {
		{BAR_DC_UNDEFINED, BarUiActHelp, NULL, '?', "act_help"},
		{BAR_DC_SONG, BarUiActLoveSong, "love song", '+',
				"act_songlove"},
		{BAR_DC_SONG, BarUiActBanSong, "ban song", '-', "act_songban"},
		{BAR_DC_STATION, BarUiActAddMusic, "add music to station", 'a',
				"act_stationaddmusic"},
		{BAR_DC_GLOBAL, BarUiActCreateStation, "create new station", 'c',
				"act_stationcreate"},
		{BAR_DC_STATION, BarUiActDeleteStation, "delete station", 'd',
				"act_stationdelete"},
		{BAR_DC_SONG, BarUiActExplain, "explain why this song is played", 'e',
				"act_songexplain"},
		{BAR_DC_GLOBAL, BarUiActStationFromGenre, "add genre station", 'g',
				"act_stationaddbygenre"},
		{BAR_DC_GLOBAL, BarUiActHistory, "song history", 'h', "act_history"},
		{BAR_DC_GLOBAL | BAR_DC_STATION | BAR_DC_SONG, BarUiActSongInfo,
				"print information about song/station", 'i',
				"act_songinfo"},
		{BAR_DC_GLOBAL, BarUiActAddSharedStation, "add shared station", 'j',
				"act_addshared"},
		{BAR_DC_SONG, BarUiActMoveSong, "move song to different station", 'm',
				"act_songmove"},
		{BAR_DC_GLOBAL | BAR_DC_STATION, BarUiActSkipSong, "next song", 'n', "act_songnext"},
		{BAR_DC_GLOBAL | BAR_DC_STATION, BarUiActPause, "pause/continue", 'p', "act_songpause"},
		{BAR_DC_GLOBAL, BarUiActQuit, "quit", 'q', "act_quit"},
		{BAR_DC_STATION, BarUiActRenameStation, "rename station", 'r',
				"act_stationrename"},
		{BAR_DC_GLOBAL, BarUiActSelectStation, "change station", 's',
				"act_stationchange"},
		{BAR_DC_SONG, BarUiActTempBanSong, "tired (ban song for 1 month)", 't',
				"act_songtired"},
		{BAR_DC_GLOBAL | BAR_DC_STATION, BarUiActPrintUpcoming, "upcoming songs", 'u',
				"act_upcoming"},
		{BAR_DC_STATION, BarUiActSelectQuickMix, "select quickmix stations",
				'x', "act_stationselectquickmix"},
		{BAR_DC_GLOBAL, BarUiActDebug, NULL, '$', "act_debug"},
		{BAR_DC_SONG, BarUiActBookmark, "bookmark song/artist", 'b',
				"act_bookmark"},
		{BAR_DC_GLOBAL, BarUiActVolDown, "decrease volume", '(',
				"act_voldown"},
		{BAR_DC_GLOBAL, BarUiActVolUp, "increase volume", ')',
				"act_volup"},
		{BAR_DC_STATION, BarUiActManageStation, "delete seeds/feedback", '=',
				"act_managestation"},
		};

#include <piano.h>
#include <stdbool.h>
#include <stdio.h>

BarKeyShortcutId_t BarUiDispatch (BarApp_t *, const char, PianoStation_t *, PianoSong_t *,
		const bool, BarUiDispatchContext_t);

#endif /* _UI_DISPATCH_H */

