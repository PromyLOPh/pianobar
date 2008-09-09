/*
Copyright (c) 2008 Lars-Dominik Braun

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

/* main loop, helper functions */

#include <wardrobe.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <readline/readline.h>
#include <time.h>
#include <ctype.h>
#include <piano.h>

#include "player.h"
#include "settings.h"
#include "main.h"
#include "terminal.h"
#include "config.h"
#include "ui.h"

inline float BarSamplesToSeconds (float samplerate, float channels,
		float samples) {
	return channels * 1000.0 * samples / samplerate;
}

int main (int argc, char **argv) {
	PianoHandle_t ph;
	struct aacPlayer player;
	BarSettings_t settings;
	PianoSong_t *curSong = NULL;
	PianoStation_t *curStation = NULL;
	char doQuit = 0;
	pthread_t playerThread;
	WardrobeHandle_t wh;
	WardrobeSong_t scrobbleSong;
	/* needed in main loop */
	struct pollfd polls = {fileno (stdin), POLLIN, POLLIN};
	char buf = '\0';
	BarKeyShortcut_t *curShortcut;

	BarUiMsg ("Welcome to " PACKAGE_STRING " (built on " __DATE__ ")\n");

	/* init some things */
	curl_global_init (CURL_GLOBAL_SSL);
	xmlInitParser ();
	ao_initialize ();
	PianoInit (&ph);
	WardrobeInit (&wh);
	BarSettingsInit (&settings);

	BarSettingsRead (&settings);

	if (settings.username == NULL) {
		settings.username = readline ("Username: ");
	}
	if (settings.password == NULL) {
		BarTermSetEcho (0);
		settings.password = readline ("Password: ");
		BarTermSetEcho (1);
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

	BarUiMsg ("Login... ");
	if (BarUiPrintPianoStatus (PianoConnect (&ph, settings.username,
			settings.password, !settings.disableSecureLogin)) !=
			PIANO_RET_OK) {
		return 0;
	}
	BarUiMsg ("Get stations... ");
	if (BarUiPrintPianoStatus (PianoGetStations (&ph)) != PIANO_RET_OK) {
		return 0;
	}

	/* select station */
	curStation = BarUiSelectStation (&ph, "Select station: ");
	if (curStation != NULL) {
		printf ("Playing station \"%s\"\n", curStation->name);
	}

	/* little hack, needed to signal: hey! we need a playlist, but don't
	 * free anything (there is nothing to be freed yet) */
	memset (&player, 0, sizeof (player));

	while (!doQuit) {
		/* already played a song, clean up things/scrobble song */
		if (player.mode == PLAYER_FINISHED_PLAYBACK) {
			scrobbleSong.length = BarSamplesToSeconds (player.samplerate,
					player.channels, player.sampleSizeN);
			/* scrobble when >= nn% are played */
			if (BarSamplesToSeconds (player.samplerate,
					player.channels, player.sampleSizeCurr) * 100 /
					scrobbleSong.length >=
					settings.lastfmScrobblePercent &&
					settings.enableScrobbling) {
				WardrobeReturn_t wRet;

				BarUiMsg ("Scrobbling song... ");
				if ((wRet = WardrobeSubmit (&wh, &scrobbleSong)) ==
						WARDROBE_RET_OK) {
					BarUiMsg ("Ok.\n");
				} else {
					printf ("Error: %s\n", WardrobeErrorToString (wRet));
				}
			}
			WardrobeSongDestroy (&scrobbleSong);
			free (player.url);
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
					BarUiMsg ("Receiving new playlist... ");
					PianoDestroyPlaylist (&ph);
					if (BarUiPrintPianoStatus (PianoGetPlaylist (&ph,
							curStation->id)) != PIANO_RET_OK) {
						curStation = NULL;
					} else {
						curSong = ph.playlist;
						if (curSong == NULL) {
							BarUiMsg ("No tracks left.\n");
							curStation = NULL;
						}
					}
				}
				if (curSong != NULL) {
					PianoStation_t *realStation =
							PianoFindStationById (ph.stations,
							curSong->stationId);
					printf ("\"%s\" by \"%s\" on \"%s\"%s%s%s\n",
							curSong->title, curSong->artist,
							curSong->album, (curSong->rating ==
							PIANO_RATE_LOVE) ? " (Loved)" : "",
							curStation->isQuickMix ? " @ ": "",
							curStation->isQuickMix ? realStation->name :
							"");
					/* setup artist and song name for scrobbling (curSong
					 * may be NULL later) */
					WardrobeSongInit (&scrobbleSong);
					scrobbleSong.artist = strdup (curSong->artist);
					scrobbleSong.title = strdup (curSong->title);
					scrobbleSong.album = strdup (curSong->album);
					scrobbleSong.started = time (NULL);

					memset (&player, 0, sizeof (player));
					player.url = strdup (curSong->audioUrl);
					player.gain = curSong->fileGain;
		
					/* start player */
					pthread_create (&playerThread, NULL, BarPlayerThread,
							&player);
				}
			} /* end if curStation != NULL */
		}

		/* in the meantime: wait for user actions */
		/* FIXME: uhm, this may work, but I guess there's a better solution. */
		if (poll (&polls, 1, (player.mode >= PLAYER_INITIALIZED &&
				player.mode < PLAYER_FINISHED_PLAYBACK &&
				player.doPause == 0 ? 1000 : -1)) > 0) {
			read (fileno (stdin), &buf, sizeof (buf));
			curShortcut = settings.keys;
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
			float songLength = BarSamplesToSeconds (player.samplerate,
					player.channels, player.sampleSizeN);
			float songRemaining = songLength -
					BarSamplesToSeconds (player.samplerate,
							player.channels, player.sampleSizeCurr);
			printf ("-%02i:%02i/%02i:%02i\r", (int) songRemaining/60,
					(int) songRemaining%60, (int) songLength/60,
					(int) songLength%60);
			fflush (stdout);
		}
	}

	if (player.url != NULL) {
		free (player.url);
		pthread_join (playerThread, NULL);
	}
	/* destroy everything (including the world...) */
	PianoDestroy (&ph);
	WardrobeDestroy (&wh);
	curl_global_cleanup ();
	ao_shutdown();
	xmlCleanupParser ();
	BarSettingsDestroy (&settings);

	return 0;
}
