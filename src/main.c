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

/* system includes */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <ctype.h>
/* open () */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* last.fm scrobbling library */
#include <wardrobe.h>

#include <curl/curl.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <pthread.h>

#include <readline/readline.h>

/* pandora.com library */
#include <piano.h>

#include "player.h"
#include "settings.h"
#include "main.h"
#include "terminal.h"
#include "config.h"
#include "ui.h"

int main (int argc, char **argv) {
	/* handles */
	PianoHandle_t ph;
	static struct audioPlayer player;
	BarSettings_t settings;
	pthread_t playerThread;
	WardrobeHandle_t wh;
	/* currently playing */
	PianoSong_t *curSong = NULL;
	PianoStation_t *curStation = NULL;
	WardrobeSong_t scrobbleSong;
	char doQuit = 0;
	/* polls */
	/* FIXME: max path length? */
	char ctlPath[1024];
	struct pollfd polls[2];
	nfds_t pollsLen = 0;
	char buf = '\0';
	BarKeyShortcut_t *curShortcut = NULL;

	BarUiMsg (MSG_NONE, "Welcome to " PACKAGE "!\n");

	/* init some things */
	curl_global_init (CURL_GLOBAL_SSL);
	xmlInitParser ();
	ao_initialize ();
	PianoInit (&ph);
	WardrobeInit (&wh);
	BarSettingsInit (&settings);
	BarSettingsRead (&settings);

	/* init polls */
	polls[0].fd = fileno (stdin);
	polls[0].events = POLLIN;
	++pollsLen;
	BarGetXdgConfigDir (PACKAGE "/ctl", ctlPath, sizeof (ctlPath));
	/* FIXME: O_RDONLY blocks while opening :/ */
	polls[1].fd = open (ctlPath, O_RDWR);
	polls[1].events = POLLIN;
	if (polls[1].fd != -1) {
		++pollsLen;
		BarUiMsg (MSG_INFO, "Control fifo at %s opened\n", ctlPath);
	}

	if (settings.username == NULL) {
		BarUiMsg (MSG_QUESTION, "Username: ");
		settings.username = readline (NULL);
	}
	if (settings.password == NULL) {
		BarTermSetEcho (0);
		BarUiMsg (MSG_QUESTION, "Password: ");
		settings.password = readline (NULL);
		BarTermSetEcho (1);
		BarUiMsg (MSG_NONE, "\n");
	}

	if (settings.enableScrobbling) {
		wh.user = strdup (settings.lastfmUser);
		wh.password = strdup (settings.lastfmPassword);
	}

	/* setup control connection */
	if (settings.controlProxy != NULL &&
			settings.controlProxyType != -1) {
		curl_easy_setopt (ph.curlHandle, CURLOPT_PROXY,
				settings.controlProxy);
		curl_easy_setopt (ph.curlHandle, CURLOPT_PROXYTYPE,
				settings.controlProxyType);
	}
	curl_easy_setopt (ph.curlHandle, CURLOPT_CONNECTTIMEOUT, 60);

	BarTermSetBuffer (0);

	BarUiMsg (MSG_INFO, "Login... ");
	if (BarUiPrintPianoStatus (PianoConnect (&ph, settings.username,
			settings.password, !settings.disableSecureLogin)) !=
			PIANO_RET_OK) {
		return 0;
	}
	BarUiMsg (MSG_INFO, "Get stations... ");
	if (BarUiPrintPianoStatus (PianoGetStations (&ph)) != PIANO_RET_OK) {
		return 0;
	}

	/* try to get autostart station */
	if (settings.autostartStation != NULL) {
		curStation = PianoFindStationById (ph.stations,
				settings.autostartStation);
		if (curStation == NULL) {
			BarUiMsg (MSG_ERR, "Error: Autostart station not found.\n");
		}
	}
	/* no autostart? ask the user */
	if (curStation == NULL) {
		curStation = BarUiSelectStation (&ph, "Select station: ");
	}
	if (curStation != NULL) {
		BarUiPrintStation (curStation);
	}

	/* little hack, needed to signal: hey! we need a playlist, but don't
	 * free anything (there is nothing to be freed yet) */
	memset (&player, 0, sizeof (player));

	while (!doQuit) {
		/* song finished playing, clean up things/scrobble song */
		if (player.mode == PLAYER_FINISHED_PLAYBACK) {
			scrobbleSong.length = player.songDuration / BAR_PLAYER_MS_TO_S_FACTOR;
			/* scrobble when >= nn% are played; use seconds, not
			 * milliseconds */
			if (scrobbleSong.length > 0 && settings.enableScrobbling &&
					player.songPlayed / BAR_PLAYER_MS_TO_S_FACTOR * 100 /
					scrobbleSong.length >= settings.lastfmScrobblePercent) {
				WardrobeReturn_t wRet;

				BarUiMsg (MSG_INFO, "Scrobbling song... ");
				if ((wRet = WardrobeSubmit (&wh, &scrobbleSong)) ==
						WARDROBE_RET_OK) {
					BarUiMsg (MSG_NONE, "Ok.\n");
				} else {
					BarUiMsg (MSG_ERR, "Error: %s\n",
							WardrobeErrorToString (wRet));
				}
			}
			WardrobeSongDestroy (&scrobbleSong);
			free (player.url);
			/* FIXME: pthread_join blocks everything if network connection
			 * is hung up e.g. */
			pthread_join (playerThread, NULL);
			memset (&player, 0, sizeof (player));
		}

		/* check whether player finished playing and start playing new
		 * song */
		if (player.mode >= PLAYER_FINISHED_PLAYBACK ||
				player.mode == PLAYER_FREED) {
			if (curStation != NULL) {
				/* what's next? */
				if (curSong != NULL) {
					curSong = curSong->next;
				}
				if (curSong == NULL) {
					BarUiMsg (MSG_INFO, "Receiving new playlist... ");
					PianoDestroyPlaylist (&ph);
					if (BarUiPrintPianoStatus (PianoGetPlaylist (&ph,
							curStation->id, settings.audioFormat)) !=
							PIANO_RET_OK) {
						curStation = NULL;
					} else {
						curSong = ph.playlist;
						if (curSong == NULL) {
							BarUiMsg (MSG_INFO, "No tracks left.\n");
							curStation = NULL;
						}
					}
				}
				/* song ready to play */
				if (curSong != NULL) {
					BarUiPrintSong (curSong, curStation->isQuickMix ?
							PianoFindStationById (ph.stations,
							curSong->stationId) : NULL);
					/* setup artist and song name for scrobbling (curSong
					 * may be NULL later) */
					WardrobeSongInit (&scrobbleSong);
					scrobbleSong.artist = strdup (curSong->artist);
					scrobbleSong.title = strdup (curSong->title);
					scrobbleSong.album = strdup (curSong->album);
					scrobbleSong.started = time (NULL);

					/* setup player */
					memset (&player, 0, sizeof (player));
					player.url = strdup (curSong->audioUrl);
					player.gain = curSong->fileGain;
					player.audioFormat = curSong->audioFormat;
		
					/* throw event */
					BarUiStartEventCmd (&settings, "songstart", curStation,
							curSong);

					/* start player */
					pthread_create (&playerThread, NULL, BarPlayerThread,
							&player);
				}
			} /* end if curStation != NULL */
		}

		/* in the meantime: wait for user actions;
		 * 1000ms == 1s => refresh time display every second */
		if (poll (polls, pollsLen, 1000) > 0) {
			if (polls[0].revents & POLLIN) {
				read (fileno (stdin), &buf, sizeof (buf));
			} else if (polls[1].revents & POLLIN) {
				read (polls[1].fd, &buf, sizeof (buf));
			}
			curShortcut = settings.keys;
			/* don't show what the user enters here, we could disable
			 * echoing too... */
			BarUiMsg (MSG_NONE, "\r");
			while (curShortcut != NULL) {
				if (curShortcut->key == buf) {
					curShortcut->cmd (&ph, &player, &settings, &curSong,
							&curStation, &doQuit);
					break;
				}
				curShortcut = curShortcut->next;
			}
		}

		/* show time */
		if (player.mode >= PLAYER_SAMPLESIZE_INITIALIZED &&
				player.mode < PLAYER_FINISHED_PLAYBACK) {
			long int songRemaining = player.songDuration - player.songPlayed;
			BarUiMsg (MSG_TIME, "-%02i:%02i/%02i:%02i\r",
					(int) songRemaining / BAR_PLAYER_MS_TO_S_FACTOR / 60,
					(int) songRemaining / BAR_PLAYER_MS_TO_S_FACTOR % 60,
					(int) player.songDuration / BAR_PLAYER_MS_TO_S_FACTOR / 60,
					(int) player.songDuration / BAR_PLAYER_MS_TO_S_FACTOR % 60);
		}
	}

	/* destroy everything (including the world...) */
	if (player.url != NULL) {
		free (player.url);
		pthread_join (playerThread, NULL);
	}
	if (polls[1].fd != -1) {
		close (polls[1].fd);
	}
	PianoDestroy (&ph);
	WardrobeDestroy (&wh);
	curl_global_cleanup ();
	ao_shutdown();
	xmlCleanupParser ();
	BarSettingsDestroy (&settings);

	return 0;
}
