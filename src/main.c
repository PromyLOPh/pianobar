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

/*	output message and flush stdout
 *	@param message
 */
inline void BarUiMsg (const char *msg) {
	printf ("%s", msg);
	fflush (stdout);
}

inline PianoReturn_t BarUiPrintPianoStatus (PianoReturn_t ret) {
	if (ret != PIANO_RET_OK) {
		printf ("Error: %s\n", PianoErrorToStr (ret));
	} else {
		printf ("Ok.\n");
	}
	return ret;
}

/*	check whether complete string is numeric
 *	@param the string
 *	@return 1 = yes, 0 = not numeric
 */
char BarIsNumericStr (const char *str) {
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
char BarReadlineInt (const char *prompt, int *retVal) {
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

/* sort linked list (station); attention: this is a
 * "i-had-no-clue-what-to-do-algo", but it works.
 * @param stations
 * @return NULL-terminated array with sorted stations
 */
PianoStation_t **BarSortedStations (PianoStation_t *unsortedStations) {
	PianoStation_t *currStation, **sortedStations, **currSortedStation;
	PianoStation_t *oldStation, *veryOldStation;
	size_t unsortedStationsN = 0;
	char inserted;

	/* get size */
	currStation = unsortedStations;
	while (currStation != NULL) {
		unsortedStationsN++;
		currStation = currStation->next;
	}
	sortedStations = calloc (unsortedStationsN+1, sizeof (*sortedStations));

	currStation = unsortedStations;
	while (currStation != NULL) {
		currSortedStation = sortedStations;
		inserted = 0;
		while (*currSortedStation != NULL && !inserted) {
			/* item has to be inserted _before_ current item? */
			/* FIXME: this doesn't handle multibyte chars correctly */
			if (strcasecmp (currStation->name,
					(*currSortedStation)->name) < 0) {
				oldStation = *currSortedStation;
				*currSortedStation = currStation;
				currSortedStation++;
				/* move items */
				while (*currSortedStation != NULL) {
					veryOldStation = *currSortedStation;
					*currSortedStation = oldStation;
					oldStation = veryOldStation;
					currSortedStation++;
				}
				/* append last item */
				if (oldStation != NULL) {
					*currSortedStation = oldStation;
				}
				inserted = 1;
			}
			currSortedStation++;
		}
		/* item could not be inserted: append */
		if (!inserted) {
			*currSortedStation = currStation;
		}
		currStation = currStation->next;
	}
	return sortedStations;
}

/*	let user pick one station
 *	@param piano handle
 *	@return pointer to selected station or NULL
 */
PianoStation_t *BarUiSelectStation (PianoHandle_t *ph,
		const char *prompt) {
	PianoStation_t **ss = NULL, **ssCurr = NULL, *retStation;
	int i = 0;

	ss = BarSortedStations (ph->stations);
	ssCurr = ss;
	while (*ssCurr != NULL) {
		printf ("%2i) %c%c%c %s\n", i,
				(*ssCurr)->useQuickMix ? 'q' : ' ',
				(*ssCurr)->isQuickMix ? 'Q' : ' ',
				!(*ssCurr)->isCreator ? 'S' : ' ',
				(*ssCurr)->name);
		ssCurr++;
		i++;
	}

	if (!BarReadlineInt (prompt, &i)) {
		return NULL;
	}
	ssCurr = ss;
	while (*ssCurr != NULL && i > 0) {
		ssCurr++;
		i--;
	}
	retStation = *ssCurr;
	free (ss);
	return retStation;
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
	if (!BarReadlineInt ("Select song: ", &i)) {
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
	if (!BarReadlineInt ("Select artist: ", &i)) {
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
char *BarUiSelectMusicId (const PianoHandle_t *ph) {
	char *musicId = NULL, *lineBuf;
	char yesnoBuf;
	PianoSearchResult_t searchResult;
	PianoArtist_t *tmpArtist;
	PianoSong_t *tmpSong;

	lineBuf = readline ("Search for artist/title: ");
	if (lineBuf != NULL && strlen (lineBuf) > 0) {
		BarUiMsg ("Searching... ");
		if (BarUiPrintPianoStatus (PianoSearchMusic (ph, lineBuf,
				&searchResult)) != PIANO_RET_OK) {
			free (lineBuf);
			return NULL;
		}
		BarUiMsg ("\r");
		if (searchResult.songs != NULL && searchResult.artists != NULL) {
			BarUiMsg ("Is this an [a]rtist or [t]rack name? Press c to abort.\n");
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
			} else {
				BarUiMsg ("Aborted.\n");
			}
		} else if (searchResult.songs != NULL) {
			tmpSong = BarUiSelectSong (searchResult.songs);
			if (tmpSong != NULL) {
				musicId = strdup (tmpSong->musicId);
			} else {
				BarUiMsg ("Aborted.\n");
			}
		} else if (searchResult.artists != NULL) {
			tmpArtist = BarUiSelectArtist (searchResult.artists);
			if (tmpArtist != NULL) {
				musicId = strdup (tmpArtist->musicId);
			} else {
				BarUiMsg ("Aborted.\n");
			}
		} else {
			BarUiMsg ("Nothing found...\n");
		}
		PianoDestroySearchResult (&searchResult);
	} else {
		BarUiMsg ("Aborted.\n");
	}
	if (lineBuf != NULL) {
		free (lineBuf);
	}

	return musicId;
}

inline float BarSamplesToSeconds (float samplerate, float channels,
		float samples) {
	return channels * 1000.0 * samples / samplerate;
}

/*	browse genre stations and create shared station
 *	@param piano handle
 */
void BarStationFromGenre (PianoHandle_t *ph) {
	int i;
	PianoGenreCategory_t *curCat;
	PianoStation_t *curStation;

	/* receive genre stations list if not yet available */
	if (ph->genreStations == NULL) {
		BarUiMsg ("Receiving genre stations... ");
		if (BarUiPrintPianoStatus (PianoGetGenreStations (ph)) !=
				PIANO_RET_OK) {
			return;
		}
	}

	/* print all available categories */
	curCat = ph->genreStations;
	i = 0;
	while (curCat != NULL) {
		printf ("%2i) %s\n", i, curCat->name);
		i++;
		curCat = curCat->next;
	}
	/* select category or exit */
	if (!BarReadlineInt (NULL, &i)) {
		BarUiMsg ("Aborted.\n");
		return;
	}
	curCat = ph->genreStations;
	while (curCat != NULL && i > 0) {
		curCat = curCat->next;
		i--;
	}
	
	/* print all available stations */
	curStation = curCat->stations;
	i = 0;
	while (curStation != NULL) {
		printf ("%2i) %s\n", i, curStation->name);
		i++;
		curStation = curStation->next;
	}
	if (!BarReadlineInt (NULL, &i)) {
		BarUiMsg ("Aborted.\n");
		return;
	}
	curStation = curCat->stations;
	while (curStation != NULL && i > 0) {
		curStation = curStation->next;
		i--;
	}
	/* create station */
	printf ("Adding shared station \"%s\"... ", curStation->name);
	fflush (stdout);
	BarUiPrintPianoStatus (PianoCreateStation (ph, "sh", curStation->id));
}

/*	transform station if necessary to allow changes like rename, rate, ...
 *	@param piano handle
 *	@param transform this station
 *	@return 0 = error, 1 = everything went well
 */
int BarTransformIfShared (PianoHandle_t *ph, PianoStation_t *station) {
	/* shared stations must be transformed */
	if (!station->isCreator) {
		BarUiMsg ("Transforming station... ");
		if (BarUiPrintPianoStatus (PianoTransformShared (ph, station)) !=
				PIANO_RET_OK) {
			return 0;
		}
	}
	return 1;
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

	BarUiMsg ("Welcome to " PACKAGE_STRING "! Press ? for help.\n");

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

	BarUiMsg ("Login... ");
	if (BarUiPrintPianoStatus (PianoConnect (&ph, bsettings.username,
			bsettings.password, !bsettings.disableSecureLogin)) !=
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
					bsettings.lastfmScrobblePercent &&
					bsettings.enableScrobbling) {
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
							curSong->title, curSong->artist, curSong->album,
							(curSong->rating ==
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
			}
		}

		/* in the meantime: wait for user actions */
		struct pollfd polls = {fileno (stdin), POLLIN, POLLIN};
		char buf, yesnoBuf;
		char *lineBuf, *musicId, *explanation;
		PianoStation_t *moveStation;

		if (poll (&polls, 1, 1000) > 0) {
			read (fileno (stdin), &buf, sizeof (buf));
			switch (buf) {
				case '?':
					printf ("\na\tadd music to current station\n"
							"b\tban current song\n"
							"c\tcreate new station\n"
							"d\tdelete current station\n"
							"e\texplain why this song is played\n"
							"g\tadd genre station\n"
							"l\tlove current song\n"
							"n\tnext song\n"
							"p\tpause/continue\n"
							"q\tquit\n"
							"r\trename current station\n"
							"s\tchange station\n"
							"t\ttired (ban song for 1 month)\n"
							"u\tupcoming songs\n"
							"x\tselect quickmix stations\n");
					break;

				case 'a':
					if (curStation == NULL) {
						BarUiMsg ("No station selected.\n");
						break;
					}
					musicId = BarUiSelectMusicId (&ph);
					if (musicId == NULL) {
						if (!BarTransformIfShared (&ph, curStation)) {
							break;
						}
						BarUiMsg ("Adding music to station... ");
						BarUiPrintPianoStatus (PianoStationAddMusic (&ph,
								curStation, musicId));
						free (musicId);
					}
					break;

				case 'b':
					if (curStation == NULL || curSong == NULL) {
						BarUiMsg ("No song playing.\n");
						break;
					}
					if (!BarTransformIfShared (&ph, curStation)) {
						break;
					}
					BarUiMsg ("Banning song... ");
					if (BarUiPrintPianoStatus (PianoRateTrack (&ph, curSong,
							PIANO_RATE_BAN)) == PIANO_RET_OK) {
						player.doQuit = 1;
					}
					break;

				case 'c':
					musicId = BarUiSelectMusicId (&ph);
					if (musicId != NULL) {
						BarUiMsg ("Creating station... ");
						BarUiPrintPianoStatus (PianoCreateStation (&ph,
								"mi", musicId));
						free (musicId);
					}
					break;

				case 'd':
					if (curStation == NULL) {
						BarUiMsg ("No station selected.\n");
						break;
					}
					printf ("Really delete \"%s\"? [yn]\n",
							curStation->name);
					read (fileno (stdin), &yesnoBuf, sizeof (yesnoBuf));
					if (yesnoBuf == 'y') {
						BarUiMsg ("Deleting station... ");
						if (BarUiPrintPianoStatus (PianoDeleteStation (&ph,
								curStation)) == PIANO_RET_OK) {
							player.doQuit = 1;
							PianoDestroyPlaylist (&ph);
							curSong = NULL;
							curStation = NULL;
						}
					}
					break;

				case 'e':
					if (curSong == NULL) {
						BarUiMsg ("No song playing.\n");
						break;
					}
					BarUiMsg ("Receiving explanation... ");
					if (BarUiPrintPianoStatus (PianoExplain (&ph, curSong,
							&explanation)) == PIANO_RET_OK) {
						printf ("%s\n", explanation);
						free (explanation);
					}
					break;

				case 'g':
					/* use genre station */
					BarStationFromGenre (&ph);
					break;

				case 'i':
					if (curStation == NULL || curSong == NULL) {
						BarUiMsg ("No song playing.\n");
						break;
					}
					/* print debug-alike infos */
					printf ("Song infos:\n"
							"album:\t%s\n"
							"artist:\t%s\n"
							"audioUrl:\t%s\n"
							"fileGain:\t%f\n"
							"focusTraitId:\t%s\n"
							"identity:\t%s\n"
							"matchingSeed:\t%s\n"
							"musicId:\t%s\n"
							"rating:\t%i\n"
							"stationId:\t%s\n"
							"title:\t%s\n"
							"userSeed:\t%s\n",
							curSong->album, curSong->artist, curSong->audioUrl,
							curSong->fileGain, curSong->focusTraitId,
							curSong->identity, curSong->matchingSeed,
							curSong->musicId, curSong->rating,
							curSong->stationId, curSong->title,
							curSong->userSeed);
					break;

				case 'l':
					if (curStation == NULL || curSong == NULL) {
						BarUiMsg ("No song playing.\n");
						break;
					}
					if (curSong->rating == PIANO_RATE_LOVE) {
						BarUiMsg ("Already loved. No need to do this twice.\n");
						break;
					}
					if (!BarTransformIfShared (&ph, curStation)) {
						break;
					}
					BarUiMsg ("Loving song... ");
					BarUiPrintPianoStatus (PianoRateTrack (&ph, curSong,
							PIANO_RATE_LOVE));
					break;

				case 'n':
					player.doQuit = 1;
					break;

				case 'm':
					if (curStation == NULL || curSong == NULL) {
						BarUiMsg ("No song playing.\n");
						break;
					}
					moveStation = BarUiSelectStation (&ph, "Move song to station: ");
					if (moveStation != NULL) {
						if (!BarTransformIfShared (&ph, curStation) ||
								!BarTransformIfShared (&ph, moveStation)) {
							break;
						}
						printf ("Moving song to \"%s\"... ", moveStation->name);
						fflush (stdout);
						if (BarUiPrintPianoStatus (PianoMoveSong (&ph,
								curStation, moveStation, curSong)) ==
								PIANO_RET_OK) {
							player.doQuit = 1;
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
					if (curStation == NULL) {
						BarUiMsg ("No station selected.\n");
						break;
					}
					lineBuf = readline ("New name?\n");
					if (lineBuf != NULL && strlen (lineBuf) > 0) {
						if (!BarTransformIfShared (&ph, curStation)) {
							break;
						}
						BarUiMsg ("Renaming station... ");
						BarUiPrintPianoStatus (PianoRenameStation (&ph,
								curStation, lineBuf));
					}
					if (lineBuf != NULL) {
						free (lineBuf);
					}
					break;

				case 's':
					player.doQuit = 1;
					PianoDestroyPlaylist (&ph);
					curSong = NULL;
					curStation = BarUiSelectStation (&ph, "Select station: ");
					if (curStation != NULL) {
						printf ("Changed station to %s\n", curStation->name);
					}
					break;

				case 't':
					if (curStation == NULL || curSong == NULL) {
						BarUiMsg ("No song playing.\n");
						break;
					}
					if (!BarTransformIfShared (&ph, curStation)) {
						break;
					}
					BarUiMsg ("Putting song on shelf... ");
					if (BarUiPrintPianoStatus (PianoSongTired (&ph,
							curSong)) == PIANO_RET_OK) {
						player.doQuit = 1;
					}
					break;

				case 'u':
					if (curStation == NULL || curSong == NULL) {
						BarUiMsg ("No song playing.\n");
						break;
					}
					PianoSong_t *nextSong = curSong->next;
					if (nextSong != NULL) {
						int i = 0;
						BarUiMsg ("Next songs:\n");
						while (nextSong != NULL) {
							printf ("%2i) \"%s\" by \"%s\"\n", i,
									nextSong->title, nextSong->artist);
							nextSong = nextSong->next;
							i++;
						}
					} else {
						BarUiMsg ("No songs in queue.\n");
					}
					break;
				
				case 'x':
					if (curStation == NULL) {
						BarUiMsg ("No station selected.\n");
						break;
					}
					if (curStation->isQuickMix) {
						PianoStation_t *selStation;
						while ((selStation = BarUiSelectStation (&ph,
								"Toggle quickmix for station: ")) != NULL) {
							selStation->useQuickMix = !selStation->useQuickMix;
						}
						BarUiMsg ("Setting quickmix stations... ");
						BarUiPrintPianoStatus (PianoSetQuickmix (&ph));
					} else {
						BarUiMsg ("Not a QuickMix station.\n");
					}
					break;

			} /* end case */
		} /* end poll */

		/* show time */
		if (player.mode >= PLAYER_SAMPLESIZE_INITIALIZED &&
				player.mode < PLAYER_FINISHED_PLAYBACK) {
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
