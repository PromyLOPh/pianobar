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

#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <piano.h>
#include <waitress.h>

/* update structure in ui_dispatch.h if you add shortcuts here */
typedef enum {
	BAR_KS_HELP = 0,
	BAR_KS_LOVE = 1,
	BAR_KS_BAN = 2,
	BAR_KS_ADDMUSIC = 3,
	BAR_KS_CREATESTATION = 4,
	BAR_KS_DELETESTATION = 5,
	BAR_KS_EXPLAIN = 6,
	BAR_KS_GENRESTATION = 7,
	BAR_KS_HISTORY = 8,
	BAR_KS_INFO = 9,
	BAR_KS_ADDSHARED = 10,
	BAR_KS_MOVESONG = 11,
	BAR_KS_SKIP = 12,
	BAR_KS_PLAYPAUSE = 13,
	BAR_KS_QUIT = 14,
	BAR_KS_RENAMESTATION = 15,
	BAR_KS_SELECTSTATION = 16,
	BAR_KS_TIRED = 17,
	BAR_KS_UPCOMING = 18,
	BAR_KS_SELECTQUICKMIX = 19,
	BAR_KS_DEBUG = 20,
	BAR_KS_BOOKMARK = 21,
	BAR_KS_VOLDOWN = 22,
	BAR_KS_VOLUP = 23,
	BAR_KS_MANAGESTATION = 24,
	/* insert new shortcuts _before_ this element and increase its value */
	BAR_KS_COUNT = 25,
} BarKeyShortcutId_t;

#define BAR_KS_DISABLED '\x00'

typedef enum {
	BAR_SORT_NAME_AZ = 0,
	BAR_SORT_NAME_ZA = 1,
	BAR_SORT_QUICKMIX_01_NAME_AZ = 2,
	BAR_SORT_QUICKMIX_01_NAME_ZA = 3,
	BAR_SORT_QUICKMIX_10_NAME_AZ = 4,
	BAR_SORT_QUICKMIX_10_NAME_ZA = 5,
	BAR_SORT_COUNT = 6,
} BarStationSorting_t;

#include "ui_types.h"

typedef struct {
	char *prefix;
	char *postfix;
} BarMsgFormatStr_t;

typedef struct {
	unsigned int history;
	int volume;
	BarStationSorting_t sortOrder;
	PianoAudioFormat_t audioFormat;
	char *username;
	char *password;
	char *controlProxy; /* non-american listeners need this */
	char *proxy;
	char keys[BAR_KS_COUNT];
	char *autostartStation;
	char *eventCmd;
	char *loveIcon;
	char *banIcon;
	char *atIcon;
	char *npSongFormat;
	char *npStationFormat;
	char *listSongFormat;
	char *fifo;
	char tlsFingerprint[20];
	BarMsgFormatStr_t msgFormat[MSG_COUNT];
} BarSettings_t;

void BarSettingsInit (BarSettings_t *);
void BarSettingsDestroy (BarSettings_t *);
void BarSettingsRead (BarSettings_t *);
void BarGetXdgConfigDir (const char *, char *, size_t);

#endif /* _SETTINGS_H */
