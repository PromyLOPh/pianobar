/*
Copyright (c) 2008-2009
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

#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <curl/curl.h>
#include <piano.h>

#include "player.h"

#define BAR_KS_ARGS PianoHandle_t *ph, struct aacPlayer *player, \
		struct BarSettings *settings, PianoSong_t **curSong, \
		PianoStation_t **curStation, char *doQuit

struct BarSettings {
	char *username;
	char *password;
	char *controlProxy; /* non-american listeners need this */
	curl_proxytype controlProxyType;
	char *lastfmUser;
	char *lastfmPassword;
	unsigned char lastfmScrobblePercent;
	char enableScrobbling;
	char disableSecureLogin;
	struct BarKeyShortcut {
		char key;
		void (*cmd) (BAR_KS_ARGS);
		char *description;
		char *configKey;
		struct BarKeyShortcut *next;
	} *keys;
};

typedef struct BarSettings BarSettings_t;
typedef struct BarKeyShortcut BarKeyShortcut_t;

void BarSettingsInit (BarSettings_t *settings);
void BarSettingsDestroy (BarSettings_t *settings);

void BarSettingsRead (BarSettings_t *settings);

#endif /* _SETTINGS_H */
