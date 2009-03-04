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

/* application settings */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "settings.h"
#include "config.h"
#include "ui_act.h"

/*	tries to guess your config dir; somehow conforming to
 *	http://standards.freedesktop.org/basedir-spec/basedir-spec-0.6.html
 *	@param name of the config file (can contain subdirs too)
 *	@param store the whole path here
 *	@param but only up to this size
 *	@return nothing
 */
void BarGetXdgConfigDir (const char *filename, char *retDir,
		size_t retDirN) {
	char *xdgConfigDir = NULL;

	if ((xdgConfigDir = getenv ("XDG_CONFIG_HOME")) != NULL &&
			strlen (xdgConfigDir) > 0) {
		/* special dir: $xdg_config_home */
		snprintf (retDir, retDirN, "%s/%s", xdgConfigDir, filename);
	} else {
		if ((xdgConfigDir = getenv ("HOME")) != NULL &&
				strlen (xdgConfigDir) > 0) {
			/* standard config dir: $home/.config */
			snprintf (retDir, retDirN, "%s/.config/%s", xdgConfigDir,
					filename);
		} else {
			/* fallback: working dir */
			snprintf (retDir, retDirN, "%s", filename);
		}
	}
}

/*	initialize settings structure
 *	@param settings struct
 */
void BarSettingsInit (BarSettings_t *settings) {
	memset (settings, 0, sizeof (*settings));
}

/*	free settings structure, zero it afterwards
 *	@oaram pointer to struct
 */
void BarSettingsDestroy (BarSettings_t *settings) {
	BarKeyShortcut_t *curShortcut = settings->keys, *lastShortcut;

	while (curShortcut != NULL) {
		lastShortcut = curShortcut;
		curShortcut = curShortcut->next;
		if (lastShortcut->description != NULL) {
			free (lastShortcut->description);
		}
		if (lastShortcut->configKey != NULL) {
			free (lastShortcut->configKey);
		}
		free (lastShortcut);
	}
	free (settings->controlProxy);
	free (settings->username);
	free (settings->password);
	free (settings->lastfmUser);
	free (settings->lastfmPassword);
	free (settings->autostartStation);
	memset (settings, 0, sizeof (*settings));
}

/*	copy key shortcut into settings structure
 *	@param shortcut to be copied
 *	@param destination settings structure
 */
void BarSettingsAppendKey (BarKeyShortcut_t *shortcut,
		BarSettings_t *settings) {
	BarKeyShortcut_t *tmp = calloc (1, sizeof (*tmp));

	/* copy shortcut */
	memcpy (tmp, shortcut, sizeof (*tmp));
	if (shortcut->description != NULL) {
		tmp->description = strdup (shortcut->description);
	}
	if (shortcut->configKey != NULL) {
		tmp->configKey = strdup (shortcut->configKey);
	}

	/* insert into linked list */
	if (settings->keys == NULL) {
		settings->keys = tmp;
	} else {
		BarKeyShortcut_t *curShortcut = settings->keys;
		while (curShortcut->next != NULL) {
			curShortcut = curShortcut->next;
		}
		curShortcut->next = tmp;
	}
}

/*	read app settings from file; format is: key = value\n
 *	@param where to save these settings
 *	@return nothing yet
 */
void BarSettingsRead (BarSettings_t *settings) {
	/* FIXME: what is the max length of a path? */
	char configfile[1024], key[256], val[256];
	size_t i;
	FILE *configfd;
	BarKeyShortcut_t *curShortcut;
	BarKeyShortcut_t defaultKeys[] = {
			{'?', BarUiActHelp, NULL, "act_help", NULL},
			{'+', BarUiActLoveSong, "love current song", "act_songlove",
				NULL},
			{'-', BarUiActBanSong, "ban current song", "act_songban", NULL},
			{'a', BarUiActAddMusic, "add music to current station",
				"act_stationaddmusic", NULL},
			{'c', BarUiActCreateStation, "create new station",
				"act_stationcreate", NULL},
			{'d', BarUiActDeleteStation, "delete current station",
				"act_stationdelete", NULL},
			{'e', BarUiActExplain, "explain why this song is played",
				"act_songexplain", NULL},
			{'g', BarUiActStationFromGenre, "add genre station",
				"act_stationaddbygenre", NULL},
			{'i', BarUiActSongInfo,
				"print information about current song/station",
				"act_songinfo", NULL},
			{'j', BarUiActAddSharedStation, "add shared station",
				"act_addshared", NULL},
			{'m', BarUiActMoveSong, "move song to different station",
				"act_songmove", NULL},
			{'n', BarUiActSkipSong, "next song", "act_songnext", NULL},
			{'p', BarUiActPause, "pause/continue", "act_songpause", NULL},
			{'q', BarUiActQuit, "quit", "act_quit", NULL},
			{'r', BarUiActRenameStation, "rename current station",
				"act_stationrename", NULL},
			{'s', BarUiActSelectStation, "change station",
				"act_stationchange", NULL},
			{'t', BarUiActTempBanSong, "tired (ban song for 1 month)",
				"act_songtired", NULL},
			{'u', BarUiActPrintUpcoming, "upcoming songs", "act_upcoming",
				NULL},
			{'x', BarUiActSelectQuickMix, "select quickmix stations",
				"act_stationselectquickmix", NULL},
			{'$', BarUiActDebug, NULL,
				"act_debug", NULL},
			};

	/* apply defaults */
	#ifdef ENABLE_FAAD
	settings->audioFormat = PIANO_AF_AACPLUS;
	#else
		#ifdef ENABLE_MAD
		settings->audioFormat = PIANO_AF_MP3;
		#endif
	#endif

	BarGetXdgConfigDir (PACKAGE "/config", configfile, sizeof (configfile));
	if ((configfd = fopen (configfile, "r")) == NULL) {
		/* use default keyboard shortcuts */
		for (i = 0; i < sizeof (defaultKeys) / sizeof (*defaultKeys);
				i++) {
			BarSettingsAppendKey (&defaultKeys[i], settings);
		}
		return;
	}

	while (!feof (configfd)) {
		memset (val, 0, sizeof (*val));
		memset (key, 0, sizeof (*key));
		if (fscanf (configfd, "%255s = %255[^\n]", key, val) < 2) {
			/* invalid config line */
			continue;
		}
		if (strcmp ("control_proxy", key) == 0) {
			settings->controlProxy = strdup (val);
		} else if (strcmp ("control_proxy_type", key) == 0) {
			if (strcmp ("http", val) == 0) {
				settings->controlProxyType = CURLPROXY_HTTP;
			} else if (strcmp ("socks4", val) == 0) {
				settings->controlProxyType = CURLPROXY_SOCKS4;
			} else if (strcmp ("socks4a", val) == 0) {
				settings->controlProxyType = CURLPROXY_SOCKS4A;
			} else if (strcmp ("socks5", val) == 0) {
				settings->controlProxyType = CURLPROXY_SOCKS5;
			} else {
				/* error: don't use proxy at all */
				settings->controlProxyType = -1;
			}
		} else if (strcmp ("user", key) == 0) {
			settings->username = strdup (val);
		} else if (strcmp ("password", key) == 0) {
			settings->password = strdup (val);
		} else if (strcmp ("lastfm_user", key) == 0) {
			settings->lastfmUser = strdup (val);
		} else if (strcmp ("lastfm_password", key) == 0) {
			settings->lastfmPassword = strdup (val);
		} else if (strcmp ("lastfm_scrobble_percent", key) == 0) {
			settings->lastfmScrobblePercent = atoi (val);
		} else if (strcmp ("disable_secure_login", key) == 0) {
			if (strcmp (val, "1") == 0) {
				settings->disableSecureLogin = 1;
			} else {
				settings->disableSecureLogin = 0;
			}
		} else if (memcmp ("act_", key, 4) == 0) {
			/* keyboard shortcuts */
			for (i = 0; i < sizeof (defaultKeys) / sizeof (*defaultKeys);
					i++) {
				if (strcmp (defaultKeys[i].configKey, key) == 0) {
					defaultKeys[i].key = val[0];
					BarSettingsAppendKey (&defaultKeys[i], settings);
					break;
				}
			}
		} else if (strcmp ("audio_format", key) == 0) {
			if (strcmp (val, "aacplus") == 0) {
				settings->audioFormat = PIANO_AF_AACPLUS;
			} else if (strcmp (val, "mp3") == 0) {
				settings->audioFormat = PIANO_AF_MP3;
			}
		} else if (strcmp ("autostart_station", key) == 0) {
			settings->autostartStation = strdup (val);
		}
	}

	/* some checks */
	/* last.fm requests tracks to be played at least 50% */
	if (settings->lastfmScrobblePercent < 50 ||
			settings->lastfmScrobblePercent > 100) {
		settings->lastfmScrobblePercent = 50;
	}

	/* only scrobble tracks if username and password are set */
	if (settings->lastfmUser != NULL && settings->lastfmPassword != NULL) {
		settings->enableScrobbling = 1;
	}

	/* append missing keyboard shortcuts to ensure the functionality is
	 * available */
	for (i = 0; i < sizeof (defaultKeys) / sizeof (*defaultKeys); i++) {
		char shortcutAvailable = 0;
		curShortcut = settings->keys;
		while (curShortcut != NULL) {
			if (curShortcut->cmd == defaultKeys[i].cmd) {
				shortcutAvailable = 1;
				break;
			}
			curShortcut = curShortcut->next;
		}
		if (!shortcutAvailable) {
			BarSettingsAppendKey (&defaultKeys[i], settings);
		}
	}

	fclose (configfd);
}
