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

/* application settings */

#define _BSD_SOURCE /* strdup() */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "settings.h"
#include "config.h"

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
	free (settings->controlProxy);
	free (settings->proxy);
	free (settings->username);
	free (settings->password);
	free (settings->autostartStation);
	free (settings->eventCmd);
	free (settings->loveIcon);
	free (settings->banIcon);
	memset (settings, 0, sizeof (*settings));
}

/*	read app settings from file; format is: key = value\n
 *	@param where to save these settings
 *	@return nothing yet
 */
void BarSettingsRead (BarSettings_t *settings) {
	/* FIXME: what is the max length of a path? */
	char configfile[1024], key[256], val[256];
	FILE *configfd;
	/* _must_ have same order as in BarKeyShortcutId_t */
	static const char defaultKeys[] = {'?', '+', '-', 'a', 'c', 'd', 'e', 'g',
			'h', 'i', 'j', 'm', 'n', 'p', 'q', 'r', 's', 't', 'u', 'x', '$',
			'b',
			};
	static const char *shortcutFileKeys[] = {
			"act_help", "act_songlove", "act_songban", "act_stationaddmusic",
			"act_stationcreate", "act_stationdelete", "act_songexplain",
			"act_stationaddbygenre", "act_history", "act_songinfo",
			"act_addshared", "act_songmove", "act_songnext", "act_songpause",
			"act_quit", "act_stationrename", "act_stationchange",
			"act_songtired", "act_upcoming", "act_stationselectquickmix",
			"act_debug", "act_bookmark",
			};

	/* apply defaults */
	#ifdef ENABLE_FAAD
	settings->audioFormat = PIANO_AF_AACPLUS;
	#else
		#ifdef ENABLE_MAD
		settings->audioFormat = PIANO_AF_MP3;
		#endif
	#endif
	settings->history = 5;
	settings->sortOrder = BAR_SORT_NAME_AZ;
	memcpy (settings->keys, defaultKeys, sizeof (defaultKeys));

	BarGetXdgConfigDir (PACKAGE "/config", configfile, sizeof (configfile));
	if ((configfd = fopen (configfile, "r")) == NULL) {
		return;
	}

	/* read config file */
	while (1) {
		int scanRet = fscanf (configfd, "%255s = %255[^\n]", key, val);
		if (scanRet == EOF) {
			break;
		} else if (scanRet != 2) {
			/* invalid config line */
			continue;
		}
		if (strcmp ("control_proxy", key) == 0) {
			settings->controlProxy = strdup (val);
		} else if (strcmp ("proxy", key) == 0) {
			settings->proxy = strdup (val);
		} else if (strcmp ("user", key) == 0) {
			settings->username = strdup (val);
		} else if (strcmp ("password", key) == 0) {
			settings->password = strdup (val);
		} else if (memcmp ("act_", key, 4) == 0) {
			size_t i;
			/* keyboard shortcuts */
			for (i = 0; i < BAR_KS_COUNT; i++) {
				if (strcmp (shortcutFileKeys[i], key) == 0) {
					settings->keys[i] = val[0];
					break;
				}
			}
		} else if (strcmp ("audio_format", key) == 0) {
			if (strcmp (val, "aacplus") == 0) {
				settings->audioFormat = PIANO_AF_AACPLUS;
			} else if (strcmp (val, "mp3") == 0) {
				settings->audioFormat = PIANO_AF_MP3;
			} else if (strcmp (val, "mp3-hifi") == 0) {
				settings->audioFormat = PIANO_AF_MP3_HI;
			}
		} else if (strcmp ("autostart_station", key) == 0) {
			settings->autostartStation = strdup (val);
		} else if (strcmp ("event_command", key) == 0) {
			settings->eventCmd = strdup (val);
		} else if (strcmp ("history", key) == 0) {
			settings->history = atoi (val);
		} else if (strcmp ("sort", key) == 0) {
			size_t i;
			static const char *mapping[] = {"name_az",
					"name_za",
					"quickmix_01_name_az",
					"quickmix_01_name_za",
					"quickmix_10_name_az",
					"quickmix_10_name_za",
					};
			for (i = 0; i < BAR_SORT_COUNT; i++) {
				if (strcmp (mapping[i], val) == 0) {
					settings->sortOrder = i;
					break;
				}
			}
		} else if (strcmp ("love_icon", key) == 0) {
			settings->loveIcon = strdup (val);
		} else if (strcmp ("ban_icon", key) == 0) {
			settings->banIcon = strdup (val);
		}
	}

	/* check environment variable if proxy is not set explicitly */
	if (settings->proxy == NULL) {
		char *tmpProxy = getenv ("http_proxy");
		if (tmpProxy != NULL && strlen (tmpProxy) > 0) {
			settings->proxy = strdup (tmpProxy);
		}
	}

	/* use default strings */
	if (settings->loveIcon == NULL) {
		settings->loveIcon = strdup ("<3");
	}
	if (settings->banIcon == NULL) {
		settings->banIcon = strdup ("</3");
	}

	fclose (configfd);
}
