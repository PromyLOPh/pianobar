/*
Copyright (c) 2008-2015
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

/* application settings */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <ctype.h>

#include <piano.h>

#include "settings.h"
#include "config.h"
#include "ui.h"
#include "ui_dispatch.h"

#define streq(a, b) (strcmp (a, b) == 0)

/*	Get current user’s home directory
 */
static char *BarSettingsGetHome () {
	char *home;

	/* try environment variable */
	if ((home = getenv ("HOME")) != NULL && strlen (home) > 0) {
		return strdup (home);
	}

	/* try passwd mechanism */
	struct passwd * const pw = getpwuid (getuid ());
	if (pw != NULL && pw->pw_dir != NULL && strlen (pw->pw_dir) > 0) {
		return strdup (pw->pw_dir);
	}

	return NULL;
}

/*	Get XDG config directory, which is set by BarSettingsRead (if not set)
 */
static char *BarGetXdgConfigDir (const char * const filename) {
	assert (filename != NULL);

	char *xdgConfigDir;

	if ((xdgConfigDir = getenv ("XDG_CONFIG_HOME")) != NULL &&
			strlen (xdgConfigDir) > 0) {
		const size_t len = (strlen (xdgConfigDir) + 1 +
				strlen (filename) + 1);
		char * const concat = malloc (len * sizeof (*concat));
		snprintf (concat, len, "%s/%s", xdgConfigDir, filename);
		return concat;
	}

	return NULL;
}

/*	Expand ~/ to user’s home directory
 */
char *BarSettingsExpandTilde (const char * const path, const char * const home) {
	assert (path != NULL);
	assert (home != NULL);

	if (strncmp (path, "~/", 2) == 0) {
		char * const expanded = malloc ((strlen (home) + 1 + strlen (path)-2 + 1) *
				sizeof (*expanded));
		sprintf (expanded, "%s/%s", home, &path[2]);
		return expanded;
	}

	return strdup (path);
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
	free (settings->passwordCmd);
	free (settings->autostartStation);
	free (settings->eventCmd);
	free (settings->loveIcon);
	free (settings->banIcon);
	free (settings->atIcon);
	free (settings->npSongFormat);
	free (settings->npStationFormat);
	free (settings->listSongFormat);
	free (settings->fifo);
	free (settings->rpcHost);
	free (settings->rpcTlsPort);
	free (settings->partnerUser);
	free (settings->partnerPassword);
	free (settings->device);
	free (settings->inkey);
	free (settings->outkey);
	for (size_t i = 0; i < MSG_COUNT; i++) {
		free (settings->msgFormat[i].prefix);
		free (settings->msgFormat[i].postfix);
	}
	memset (settings, 0, sizeof (*settings));
}

/*	read app settings from file; format is: key = value\n
 *	@param where to save these settings
 *	@return nothing yet
 */
void BarSettingsRead (BarSettings_t *settings) {
	char * const configfiles[] = {PACKAGE "/state", PACKAGE "/config"};
	char * const userhome = BarSettingsGetHome ();
	assert (userhome != NULL);
	/* set xdg config path (if not set) */
	char * const defaultxdg = malloc (strlen (userhome) + strlen ("/.config") + 1);
	sprintf (defaultxdg, "%s/.config", userhome);
	setenv ("XDG_CONFIG_HOME", defaultxdg, 0);
	free (defaultxdg);

	assert (sizeof (settings->keys) / sizeof (*settings->keys) ==
			sizeof (dispatchActions) / sizeof (*dispatchActions));

	/* apply defaults */
	settings->audioQuality = PIANO_AQ_HIGH;
	settings->autoselect = true;
	settings->history = 5;
	settings->volume = 0;
	settings->maxPlayerErrors = 5;
	settings->sortOrder = BAR_SORT_NAME_AZ;
	settings->loveIcon = strdup (" <3");
	settings->banIcon = strdup (" </3");
	settings->atIcon = strdup (" @ ");
	settings->npSongFormat = strdup ("\"%t\" by \"%a\" on \"%l\"%r%@%s");
	settings->npStationFormat = strdup ("Station \"%n\" (%i)");
	settings->listSongFormat = strdup ("%i) %a - %t%r");
	settings->rpcHost = strdup (PIANO_RPC_HOST);
	settings->rpcTlsPort = NULL;
	settings->partnerUser = strdup ("android");
	settings->partnerPassword = strdup ("AC7IBG09A3DTSYM4R41UJWL07VLN8JI7");
	settings->device = strdup ("android-generic");
	settings->inkey = strdup ("R=U!LH$O2B#");
	settings->outkey = strdup ("6#26FRL$ZWD");
	settings->fifo = BarGetXdgConfigDir (PACKAGE "/ctl");
	assert (settings->fifo != NULL);

	settings->msgFormat[MSG_NONE].prefix = NULL;
	settings->msgFormat[MSG_NONE].postfix = NULL;
	settings->msgFormat[MSG_INFO].prefix = strdup ("(i) ");
	settings->msgFormat[MSG_INFO].postfix = NULL;
	settings->msgFormat[MSG_PLAYING].prefix = strdup ("|>  ");
	settings->msgFormat[MSG_PLAYING].postfix = NULL;
	settings->msgFormat[MSG_TIME].prefix = strdup ("#   ");
	settings->msgFormat[MSG_TIME].postfix = NULL;
	settings->msgFormat[MSG_ERR].prefix = strdup ("/!\\ ");
	settings->msgFormat[MSG_ERR].postfix = NULL;
	settings->msgFormat[MSG_QUESTION].prefix = strdup ("[?] ");
	settings->msgFormat[MSG_QUESTION].postfix = NULL;
	settings->msgFormat[MSG_LIST].prefix = strdup ("\t");
	settings->msgFormat[MSG_LIST].postfix = NULL;

	for (size_t i = 0; i < BAR_KS_COUNT; i++) {
		settings->keys[i] = dispatchActions[i].defaultKey;
	}

	/* read config files */
	for (size_t j = 0; j < sizeof (configfiles) / sizeof (*configfiles); j++) {
		static const char *formatMsgPrefix = "format_msg_";
		FILE *configfd;
		/* getline allocates these on the first run */
		char *line = NULL;
		size_t lineLen = 0, lineNum = 0;

		char * const path = BarGetXdgConfigDir (configfiles[j]);
		assert (path != NULL);
		if ((configfd = fopen (path, "r")) == NULL) {
			free (path);
			continue;
		}

		while (1) {
			++lineNum;
			ssize_t ret = getline (&line, &lineLen, configfd);
			if (ret == -1) {
				/* EOF or error */
				break;
			}
			/* parse lines that match "^\s*(.*?)\s?=\s?(.*)$". Windows and Unix
			 * line terminators are supported. */
			char *key = line;

			/* skip leading spaces */
			while (isspace ((unsigned char) key[0])) {
				++key;
			}

			/* skip comments */
			if (key[0] == '#') {
				continue;
			}

			/* search for delimiter and split key-value pair */
			char *val = strchr (line, '=');
			if (val == NULL) {
				/* no warning for empty lines */
				if (key[0] != '\0') {
					BarUiMsg (settings, MSG_INFO,
							"Invalid line at %s:%zu\n", path, lineNum);
				}
				/* invalid line */
				continue;
			}
			*val = '\0';
			++val;

			/* drop spaces at the end */
			char *keyend = &key[strlen (key)-1];
			while (keyend >= key && isspace ((unsigned char) *keyend)) {
				*keyend = '\0';
				--keyend;
			}

			/* strip at most one space, legacy cruft, required for values with
			 * leading spaces like love_icon */
			if (isspace ((unsigned char) val[0])) {
				++val;
			}
			/* drop trailing cr/lf */
			char *valend = &val[strlen (val)-1];
			while (valend >= val && (*valend == '\r' || *valend == '\n')) {
				*valend = '\0';
				--valend;
			}

			if (streq ("control_proxy", key)) {
				settings->controlProxy = strdup (val);
			} else if (streq ("proxy", key)) {
				settings->proxy = strdup (val);
			} else if (streq ("user", key)) {
				settings->username = strdup (val);
			} else if (streq ("password", key)) {
				settings->password = strdup (val);
			} else if (streq ("password_command", key)) {
				settings->passwordCmd = strdup (val);
			} else if (streq ("rpc_host", key)) {
				free (settings->rpcHost);
				settings->rpcHost = strdup (val);
			} else if (streq ("rpc_tls_port", key)) {
				free (settings->rpcTlsPort);
				settings->rpcTlsPort = strdup (val);
			} else if (streq ("partner_user", key)) {
				free (settings->partnerUser);
				settings->partnerUser = strdup (val);
			} else if (streq ("partner_password", key)) {
				free (settings->partnerPassword);
				settings->partnerPassword = strdup (val);
			} else if (streq ("device", key)) {
				free (settings->device);
				settings->device = strdup (val);
			} else if (streq ("encrypt_password", key)) {
				free (settings->outkey);
				settings->outkey = strdup (val);
			} else if (streq ("decrypt_password", key)) {
				free (settings->inkey);
				settings->inkey = strdup (val);
			} else if (streq ("ca_bundle", key)) {
				free (settings->caBundle);
				settings->caBundle = strdup (val);
			} else if (memcmp ("act_", key, 4) == 0) {
				size_t i;
				/* keyboard shortcuts */
				for (i = 0; i < BAR_KS_COUNT; i++) {
					if (streq (dispatchActions[i].configKey, key)) {
						if (streq (val, "disabled")) {
							settings->keys[i] = BAR_KS_DISABLED;
						} else {
							settings->keys[i] = val[0];
						}
						break;
					}
				}
			} else if (streq ("audio_quality", key)) {
				if (streq (val, "low")) {
					settings->audioQuality = PIANO_AQ_LOW;
				} else if (streq (val, "medium")) {
					settings->audioQuality = PIANO_AQ_MEDIUM;
				} else if (streq (val, "high")) {
					settings->audioQuality = PIANO_AQ_HIGH;
				}
			} else if (streq ("autostart_station", key)) {
				free (settings->autostartStation);
				settings->autostartStation = strdup (val);
			} else if (streq ("event_command", key)) {
				settings->eventCmd = BarSettingsExpandTilde (val, userhome);
			} else if (streq ("history", key)) {
				settings->history = atoi (val);
			} else if (streq ("max_player_errors", key)) {
				settings->maxPlayerErrors = atoi (val);
			} else if (streq ("sort", key)) {
				size_t i;
				static const char *mapping[] = {"name_az",
						"name_za",
						"quickmix_01_name_az",
						"quickmix_01_name_za",
						"quickmix_10_name_az",
						"quickmix_10_name_za",
						};
				for (i = 0; i < BAR_SORT_COUNT; i++) {
					if (streq (mapping[i], val)) {
						settings->sortOrder = i;
						break;
					}
				}
			} else if (streq ("love_icon", key)) {
				free (settings->loveIcon);
				settings->loveIcon = strdup (val);
			} else if (streq ("ban_icon", key)) {
				free (settings->banIcon);
				settings->banIcon = strdup (val);
			} else if (streq ("at_icon", key)) {
				free (settings->atIcon);
				settings->atIcon = strdup (val);
			} else if (streq ("volume", key)) {
				settings->volume = atoi (val);
			} else if (streq ("format_nowplaying_song", key)) {
				free (settings->npSongFormat);
				settings->npSongFormat = strdup (val);
			} else if (streq ("format_nowplaying_station", key)) {
				free (settings->npStationFormat);
				settings->npStationFormat = strdup (val);
			} else if (streq ("format_list_song", key)) {
				free (settings->listSongFormat);
				settings->listSongFormat = strdup (val);
			} else if (streq ("fifo", key)) {
				free (settings->fifo);
				settings->fifo = BarSettingsExpandTilde (val, userhome);
			} else if (streq ("autoselect", key)) {
				settings->autoselect = atoi (val);
			} else if (strncmp (formatMsgPrefix, key,
					strlen (formatMsgPrefix)) == 0) {
				static const char *mapping[] = {"none", "info", "nowplaying",
						"time", "err", "question", "list"};
				const char *typeStart = key + strlen (formatMsgPrefix);
				for (size_t i = 0; i < sizeof (mapping) / sizeof (*mapping); i++) {
					if (streq (typeStart, mapping[i])) {
						const char *formatPos = strstr (val, "%s");
						
						/* keep default if there is no format character */
						if (formatPos != NULL) {
							BarMsgFormatStr_t *format = &settings->msgFormat[i];

							free (format->prefix);
							free (format->postfix);

							const size_t prefixLen = formatPos - val;
							format->prefix = calloc (prefixLen + 1,
									sizeof (*format->prefix));
							memcpy (format->prefix, val, prefixLen);

							const size_t postfixLen = strlen (val) -
									(formatPos-val) - 2;
							format->postfix = calloc (postfixLen + 1,
									sizeof (*format->postfix));
							memcpy (format->postfix, formatPos+2, postfixLen);
						}
						break;
					}
				}
			} else {
				BarUiMsg (settings, MSG_INFO,
						"Unrecognized key %s at %s:%zu\n", key, path, lineNum);
			}
		}

		fclose (configfd);
		free (path);
		free (line);
	}

	/* check environment variable if proxy is not set explicitly */
	if (settings->proxy == NULL) {
		char *tmpProxy = getenv ("http_proxy");
		if (tmpProxy != NULL && strlen (tmpProxy) > 0) {
			settings->proxy = strdup (tmpProxy);
		}
	}

	/* ffmpeg does not support setting an http proxy explicitly */
	if (settings->proxy != NULL) {
		setenv ("http_proxy", settings->proxy, 1);
	}

	free (userhome);
}

/*	write statefile
 */
void BarSettingsWrite (PianoStation_t *station, BarSettings_t *settings) {
	FILE *fd;

	assert (settings != NULL);

	char * const path = BarGetXdgConfigDir (PACKAGE "/state");
	assert (path != NULL);
	if ((fd = fopen (path, "w")) == NULL) {
		free (path);
		return;
	}

	fputs ("# do not edit this file\n", fd);
	fprintf (fd, "volume = %i\n", settings->volume);
	if (station != NULL) {
		fprintf (fd, "autostart_station = %s\n", station->id);
	}

	fclose (fd);
	free (path);
}

