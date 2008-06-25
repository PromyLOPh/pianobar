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

#include <piano.h>
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

#include "terminal.h"
#include "settings.h"
#include "config.h"
#include "player.h"

/*	check whether complete string is numeric
 *	@param the string
 *	@return 1 = yes, 0 = not numeric
 */
char BarIsNumericStr (char *str) {
	while (*str != '\0') {
		if (isdigit (*str) == 0) {
			return 0;
		}
		str++;
	}
	return 1;
}

/*	use readline to get integer value
 *	@param prompt or NULL
 *	@param returns integer
 *	@return 1 = success, 0 = failure (not an integer, ...)
 */
char BarReadlineInt (char *prompt, int *retVal) {
	char *buf;
	char ret = 0;

	if ((buf = readline (prompt)) != NULL && strlen (buf) > 0 &&
			BarIsNumericStr (buf)) {
		*retVal = atoi (buf);
		ret = 1;
	}
	if (buf != NULL) {
		free (buf);
	}
	return ret;
}

/*	let user pick one station
 *	@param piano handle
 *	@return pointer to selected station or NULL
 */
PianoStation_t *BarUiSelectStation (PianoHandle_t *ph) {
	PianoStation_t *curStation = NULL;
	int i = 0;

	printf ("which station do you want to listen to?\n");
	curStation = ph->stations;
	while (curStation != NULL) {
		printf ("%2i) %s\n", i, curStation->name);
		curStation = curStation->next;
		i++;
	}
	if (!BarReadlineInt (NULL, &i)) {
		return NULL;
	}
	curStation = ph->stations;
	while (curStation != NULL && i > 0) {
		curStation = curStation->next;
		i--;
	}
	return curStation;
}

/*	let user pick one song
 *	@param song list
 *	@return pointer to selected item in song list or NULL
 */
PianoSong_t *BarUiSelectSong (PianoSong_t *startSong) {
	PianoSong_t *tmpSong = NULL;
	int i = 0;

	tmpSong = startSong;
	while (tmpSong != NULL) {
		printf ("%2u) %s - %s\n", i, tmpSong->artist, tmpSong->title);
		i++;
		tmpSong = tmpSong->next;
	}
	if (!BarReadlineInt (NULL, &i)) {
		return NULL;
	}
	tmpSong = startSong;
	while (tmpSong != NULL && i > 0) {
		tmpSong = tmpSong->next;
		i--;
	}
	return tmpSong;
}

/*	let user pick one artist
 *	@param artists (linked list)
 *	@return pointer to selected artist or NULL on abort
 */
PianoArtist_t *BarUiSelectArtist (PianoArtist_t *startArtist) {
	PianoArtist_t *tmpArtist = NULL;
	int i = 0;

	tmpArtist = startArtist;
	while (tmpArtist != NULL) {
		printf ("%2u) %s\n", i, tmpArtist->name);
		i++;
		tmpArtist = tmpArtist->next;
	}
	if (!BarReadlineInt (NULL, &i)) {
		return NULL;
	}
	tmpArtist = startArtist;
	while (tmpArtist != NULL && i > 0) {
		tmpArtist = tmpArtist->next;
		i--;
	}
	return tmpArtist;
}

/*	search music: query, search request, return music id
 *	@param piano handle
 *	@return musicId or NULL on abort/error
 */
char *BarUiSelectMusicId (PianoHandle_t *ph) {
	char *musicId = NULL, *lineBuf;
	char yesnoBuf;
	PianoSearchResult_t searchResult;
	PianoArtist_t *tmpArtist;
	PianoSong_t *tmpSong;

	lineBuf = readline ("Search for artist/title\n");
	if (lineBuf != NULL && strlen (lineBuf) > 0) {
		PianoSearchMusic (ph, lineBuf, &searchResult);
		if (searchResult.songs != NULL && searchResult.artists != NULL) {
			printf ("Is this an [a]rtist or [t]rack name? Press c to abort.\n");
			read (fileno (stdin), &yesnoBuf, sizeof (yesnoBuf));
			if (yesnoBuf == 'a') {
				tmpArtist = BarUiSelectArtist (searchResult.artists);
				if (tmpArtist != NULL) {
					musicId = strdup (tmpArtist->musicId);
				}
			} else if (yesnoBuf == 't') {
				tmpSong = BarUiSelectSong (searchResult.songs);
				if (tmpSong != NULL) {
					musicId = strdup (tmpSong->musicId);
				}
			}
		} else if (searchResult.songs != NULL) {
			printf ("Select song\n");
			tmpSong = BarUiSelectSong (searchResult.songs);
			if (tmpSong != NULL) {
				musicId = strdup (tmpSong->musicId);
			}
		} else if (searchResult.artists != NULL) {
			printf ("Select artist\n");
			tmpArtist = BarUiSelectArtist (searchResult.artists);
			if (tmpArtist != NULL) {
				musicId = strdup (tmpArtist->musicId);
			}
		} else {
			printf ("Nothing found...\n");
		}
		PianoDestroySearchResult (&searchResult);
	}
	if (lineBuf != NULL) {
		free (lineBuf);
	}

	return musicId;
}

/* FIXME: this is now correct */
inline float BarSamplesToSeconds (float samplerate, float channels,
		float samples) {
	return channels * 1000.0 * samples / samplerate;
}

int main (int argc, char **argv) {
	PianoHandle_t ph;
	struct aacPlayer player;
	char doQuit = 0;
	PianoSong_t *curSong = NULL;
	PianoStation_t *curStation = NULL;
	BarSettings_t bsettings;
	pthread_t playerThread;
	WardrobeSong_t scrobbleSong;
	WardrobeHandle_t wh;

	printf ("Welcome to " PACKAGE_STRING "! Press ? for help.\n");

	/* init some things */
	curl_global_init (CURL_GLOBAL_SSL);
	xmlInitParser ();
	ao_initialize();

	BarSettingsInit (&bsettings);
	BarSettingsRead (&bsettings);

	if (bsettings.username == NULL) {
		bsettings.username = readline ("Username: ");
	}
	if (bsettings.password == NULL) {
		BarTermSetEcho (0);
		bsettings.password = readline ("Password: ");
		BarTermSetEcho (1);
	}

	PianoInit (&ph);
	WardrobeInit (&wh);

	if (bsettings.enableScrobbling) {
		wh.user = strdup (bsettings.lastfmUser);
		wh.password = strdup (bsettings.lastfmPassword);
	}

	/* setup control connection */
	if (bsettings.controlProxy != NULL &&
			bsettings.controlProxyType != -1) {
		curl_easy_setopt (ph.curlHandle, CURLOPT_PROXY,
				bsettings.controlProxy);
		curl_easy_setopt (ph.curlHandle, CURLOPT_PROXYTYPE,
				bsettings.controlProxyType);
	}
	curl_easy_setopt (ph.curlHandle, CURLOPT_CONNECTTIMEOUT, 60);

	BarTermSetBuffer (0);

	printf ("Login...\n");
	if (PianoConnect (&ph, bsettings.username, bsettings.password) !=
			PIANO_RET_OK) {
		printf ("Login failed. Check your username and password\n");
		return 0;
	}
	printf ("Get stations...\n");
	if (PianoGetStations (&ph) != PIANO_RET_OK) {
		printf ("Error while fetching your stations.\n");
		return 0;
	}

	/* select station */
	curStation = BarUiSelectStation (&ph);
	if (curStation != NULL) {
		printf ("Playing station \"%s\"\n", curStation->name);
	}

	/* little hack, needed to signal: hey! we need a playlist, but don't
	 * free anything (there is nothing to be freed yet) */
	memset (&player, 0, sizeof (player));

	while (!doQuit) {
		/* check whether player finished playing and start playing new
		 * song */
		if (player.finishedPlayback == 1 || curSong == NULL) {
			/* already played a song, clean up things */
			if (player.url != NULL) {
				scrobbleSong.length = BarSamplesToSeconds (player.samplerate,
						player.channels, player.sampleSizeN);
				/* scrobble when >= nn% are played */
				if (BarSamplesToSeconds (player.samplerate,
						player.channels, player.sampleSizeCurr) * 100 /
						scrobbleSong.length >=
						bsettings.lastfmScrobblePercent &&
						bsettings.enableScrobbling) {
					if (WardrobeSubmit (&wh, &scrobbleSong) ==
							WARDROBE_RET_OK) {
						printf ("Scrobbled. \n");
					} else {
						printf ("Errror while scrobbling. \n");
					}
				}
				WardrobeSongDestroy (&scrobbleSong);
				free (player.url);
				memset (&player, 0, sizeof (player));
				pthread_join (playerThread, NULL);
			}

			if (curStation != NULL) {
				/* what's next? */
				if (curSong != NULL) {
					curSong = curSong->next;
				}
				if (curSong == NULL && curStation != NULL) {
					printf ("Receiving new playlist\n");
					PianoDestroyPlaylist (&ph);
					PianoGetPlaylist (&ph, curStation->id);
					curSong = ph.playlist;
					if (curSong == NULL) {
						printf ("No tracks left\n");
						curStation = NULL;
					}
				}
				if (curSong != NULL) {
					printf ("\"%s\" by \"%s\"%s\n", curSong->title,
							curSong->artist, (curSong->rating ==
							PIANO_RATE_LOVE) ? " (Loved)" : "");
					/* setup artist and song name for scrobbling (curSong
					 * may be NULL later) */
					WardrobeSongInit (&scrobbleSong);
					scrobbleSong.artist = strdup (curSong->artist);
					scrobbleSong.title = strdup (curSong->title);
					scrobbleSong.started = time (NULL);

					/* FIXME: why do we need to zero everything again? */
					memset (&player, 0, sizeof (player));
					player.url = strdup (curSong->audioUrl);
		
					/* start player */
					pthread_create (&playerThread, NULL, BarPlayerThread,
							&player);
				}
			}
		}

		/* in the meantime: wait for user actions */
		struct pollfd polls = {fileno (stdin), POLLIN, POLLIN};
		char buf, yesnoBuf;
		char *lineBuf, *musicId;
		PianoStation_t *moveStation;

		if (poll (&polls, 1, 1000) > 0) {
			read (fileno (stdin), &buf, sizeof (buf));
			switch (buf) {
				case '?':
					printf ("a\tadd music to current station\n"
							"b\tban current song\n"
							"c\tcreate new station\n"
							"d\tdelete current station\n"
							"l\tlove current song\n"
							"n\tnext song\n"
							"p\tpause/continue\n"
							"q\tquit\n"
							"r\trename current station\n"
							"s\tchange station\n"
							"t\ttired (ban song for 1 month)\n");
					break;

				case 'a':
					musicId = BarUiSelectMusicId (&ph);
					if (musicId == NULL) {
						printf ("Aborted.\n");
					} else {
						printf ("Adding music to station... ");
						fflush (stdout);
						if (PianoStationAddMusic (&ph, curStation, musicId) ==
								PIANO_RET_OK) {
							printf ("Ok.\n");
						} else {
							printf ("Error.\n");
						}
						free (musicId);
					}
					break;

				case 'b':
					player.doQuit = 1;
					if (PianoRateTrack (&ph, curStation, curSong,
							PIANO_RATE_BAN) == PIANO_RET_OK) {
						printf ("Banned.\n");
					} else {
						printf ("Error while banning track.\n");
					}
					/* pandora does this too, I think */
					PianoDestroyPlaylist (&ph);
					curSong = NULL;
					break;

				case 'c':
					musicId = BarUiSelectMusicId (&ph);
					if (musicId != NULL) {
						printf ("Creating station... ");
						fflush (stdout);
						if (PianoCreateStation (&ph, musicId) == PIANO_RET_OK) {
							printf ("Ok.\n");
						} else {
							printf ("Error.\n");
						}
						free (musicId);
					} else {
						printf ("Aborted.\n");
					}
					break;

				case 'd':
					printf ("Really delete \"%s\"? [yn]\n",
							curStation->name);
					read (fileno (stdin), &yesnoBuf, sizeof (yesnoBuf));
					if (yesnoBuf == 'y') {
						if (PianoDeleteStation (&ph, curStation) ==
								PIANO_RET_OK) {
							player.doQuit = 1;
							printf ("Deleted.\n");
							PianoDestroyPlaylist (&ph);
							curSong = NULL;
							curStation = NULL;
						} else {
							printf ("Error while deleting station.\n");
						}
					}
					break;

				case 'l':
					if (curSong->rating == PIANO_RATE_LOVE) {
						printf ("Already loved. No need to do this twice.\n");
						break;
					}
					if (PianoRateTrack (&ph, curStation, curSong,
							PIANO_RATE_LOVE) == PIANO_RET_OK) {
						printf ("Loved.\n");
					} else {
						printf ("Error while loving track.\n");
					}
					break;

				case 'n':
					player.doQuit = 1;
					break;

				case 'm':
					moveStation = BarUiSelectStation (&ph);
					if (moveStation != NULL) {
						printf ("Moving song to \"%s\"...", moveStation->name);
						fflush (stdout);
						if (PianoMoveSong (&ph, curStation, moveStation,
								curSong) == PIANO_RET_OK) {
							printf ("Ok.\n");
							player.doQuit = 1;
						} else {
							printf ("Error.\n");
						}
					}
					break;
				
				case 'p':
					player.doPause = !player.doPause;
					break;

				case 'q':
					doQuit = 1;
					player.doQuit = 1;
					break;

				case 'r':
					lineBuf = readline ("New name?\n");
					if (lineBuf != NULL && strlen (lineBuf) > 0) {
						if (PianoRenameStation (&ph, curStation, lineBuf) ==
								PIANO_RET_OK) {
							printf ("Renamed.\n");
						} else {
							printf ("Error while renaming station.\n");
						}
					}
					if (lineBuf != NULL) {
						free (lineBuf);
					}
					break;

				case 's':
					player.doQuit = 1;
					PianoDestroyPlaylist (&ph);
					curSong = NULL;
					curStation = BarUiSelectStation (&ph);
					if (curStation != NULL) {
						printf ("Changed station to %s\n", curStation->name);
					}
					break;

				case 't':
					printf ("Putting song on shelf... ");
					fflush (stdout);
					if (PianoSongTired (&ph, curSong) == PIANO_RET_OK) {
						printf ("Ok.\n");
						player.doQuit = 1;
					} else {
						printf ("Error.\n");
					}
					break;

			} /* end case */
		} /* end poll */

		/* show time */
		if (player.finishedPlayback == 0 &&
				player.mode >= SAMPLESIZE_INITIALIZED) {
			float songLength = BarSamplesToSeconds (player.samplerate,
					player.channels, player.sampleSizeN);
			float songRemaining = songLength -
					BarSamplesToSeconds (player.samplerate, player.channels,
							player.sampleSizeCurr);
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
	BarSettingsDestroy (&bsettings);

	return 0;
}
