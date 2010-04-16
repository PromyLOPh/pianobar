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

/* everything that interacts with the user */

#define _POSIX_C_SOURCE 1 /* fileno() */
#define _BSD_SOURCE /* strdup() */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <strings.h>

/* waitpid () */
#include <sys/types.h>
#include <sys/wait.h>

#include "ui.h"
#include "ui_readline.h"

/*	output message and flush stdout
 *	@param message
 */
inline void BarUiMsg (uiMsg_t type, const char *format, ...) {
	va_list fmtargs;

	switch (type) {
		case MSG_INFO:
			printf ("(i) ");
			break;

		case MSG_PLAYING:
			printf ("|>  ");
			break;

		case MSG_TIME:
			printf ("#   ");
			break;
		
		case MSG_ERR:
			printf ("/!\\ ");
			break;

		case MSG_QUESTION:
			printf ("[?] ");
			break;

		case MSG_LIST:
			printf ("       ");
			break;
	
		default:
			break;
	}
	va_start (fmtargs, format);
	vprintf (format, fmtargs);
	va_end (fmtargs);

	fflush (stdout);
}

/*	prints human readable status message based on return value
 *	@param piano return value
 */
inline PianoReturn_t BarUiPrintPianoStatus (PianoReturn_t ret) {
	if (ret != PIANO_RET_OK) {
		BarUiMsg (MSG_NONE, "Error: %s\n", PianoErrorToStr (ret));
	} else {
		BarUiMsg (MSG_NONE, "Ok.\n");
	}
	return ret;
}

/*	fetch http resource (post request)
 *	@param waitress handle
 *	@param piano request (initialized by PianoRequest())
 */
static WaitressReturn_t BarPianoHttpRequest (WaitressHandle_t *waith,
		PianoRequest_t *req) {
	waith->extraHeaders = "Content-Type: text/xml\r\n";
	waith->postData = req->postData;
	waith->method = WAITRESS_METHOD_POST;
	strncpy (waith->path, req->urlPath, sizeof (waith->path)-1);

	return WaitressFetchBuf (waith, &req->responseData);
}

/*	piano wrapper: prepare/execute http request and pass result back to
 *	libpiano (updates data structures)
 *	@param piano handle
 *	@param request type
 *	@param waitress handle
 *	@param data pointer (used as request data)
 *	@return 1 on success, 0 otherwise
 */
int BarUiPianoCall (PianoHandle_t *ph, PianoRequestType_t type,
		WaitressHandle_t *waith, void *data, PianoReturn_t *pRet,
		WaitressReturn_t *wRet) {
	PianoRequest_t req;

	memset (&req, 0, sizeof (req));

	/* repeat as long as there are http requests to do */
	do {
		req.data = data;

		*pRet = PianoRequest (ph, &req, type);
		if (*pRet != PIANO_RET_OK) {
			BarUiMsg (MSG_NONE, "Error: %s\n", PianoErrorToStr (*pRet));
			PianoDestroyRequest (&req);
			return 0;
		}

		*wRet = BarPianoHttpRequest (waith, &req);
		if (*wRet != WAITRESS_RET_OK) {
			BarUiMsg (MSG_NONE, "Network error: %s\n", WaitressErrorToStr (*wRet));
			PianoDestroyRequest (&req);
			if (req.responseData != NULL) {
				free (req.responseData);
			}
			return 0;
		}

		*pRet = PianoResponse (ph, &req);
		if (*pRet != PIANO_RET_CONTINUE_REQUEST) {
			if (*pRet != PIANO_RET_OK) {
				BarUiMsg (MSG_NONE, "Error: %s\n", PianoErrorToStr (*pRet));
				PianoDestroyRequest (&req);
				if (req.responseData != NULL) {
					free (req.responseData);
				}
				return 0;
			} else {
				BarUiMsg (MSG_NONE, "Ok.\n");
			}
		}
		/* we can destroy the request at this point, even when this call needs
		 * more than one http request. persistend data (step counter, e.g.) is
		 * stored in req.data */
		if (req.responseData != NULL) {
			free (req.responseData);
		}
		PianoDestroyRequest (&req);
	} while (*pRet == PIANO_RET_CONTINUE_REQUEST);

	return 1;
}

/*	compare stations by name (ignore case)
 *	@param station a
 *	@param station b
 *	@return -1, 0, 1
 */
static int BarStationCmp (const void *a, const void *b) {
	const PianoStation_t *stationA = *((PianoStation_t **) a),
			*stationB = *((PianoStation_t **) b);
	return strcasecmp (stationA->name, stationB->name);
}

/*	sort linked list (station)
 *	@param stations
 *	@return NULL-terminated array with sorted stations
 */
PianoStation_t **BarSortedStations (PianoStation_t *unsortedStations,
		size_t *retStationCount) {
	PianoStation_t **stationArray = NULL, *currStation = NULL;
	size_t stationCount = 0, i;

	/* get size */
	currStation = unsortedStations;
	while (currStation != NULL) {
		++stationCount;
		currStation = currStation->next;
	}
	stationArray = calloc (stationCount, sizeof (*stationArray));

	/* copy station pointers */
	currStation = unsortedStations;
	i = 0;
	while (currStation != NULL) {
		stationArray[i] = currStation;
		currStation = currStation->next;
		++i;
	}

	qsort (stationArray, stationCount, sizeof (*stationArray), BarStationCmp);

	*retStationCount = stationCount;
	return stationArray;
}

/*	let user pick one station
 *	@param piano handle
 *	@return pointer to selected station or NULL
 */
PianoStation_t *BarUiSelectStation (PianoHandle_t *ph, const char *prompt,
		FILE *curFd) {
	PianoStation_t **sortedStations = NULL, *retStation = NULL;
	size_t stationCount, i;
	int input;

	/* sort and print stations */
	sortedStations = BarSortedStations (ph->stations, &stationCount);
	for (i = 0; i < stationCount; i++) {
		const PianoStation_t *currStation = sortedStations[i];
		BarUiMsg (MSG_LIST, "%2i) %c%c%c %s\n", i,
				currStation->useQuickMix ? 'q' : ' ',
				currStation->isQuickMix ? 'Q' : ' ',
				!currStation->isCreator ? 'S' : ' ',
				currStation->name);
	}

	BarUiMsg (MSG_QUESTION, prompt);
	/* FIXME: using a _signed_ int is ugly */
	if (BarReadlineInt (&input, curFd) == 0) {
		free (sortedStations);
		return NULL;
	}
	if (input < stationCount) {
		retStation = sortedStations[input];
	}
	free (sortedStations);
	return retStation;
}

/*	let user pick one song
 *	@param song list
 *	@return pointer to selected item in song list or NULL
 */
PianoSong_t *BarUiSelectSong (PianoSong_t *startSong, FILE *curFd) {
	PianoSong_t *tmpSong = NULL;
	int i = 0;

	/* print all songs */
	tmpSong = startSong;
	while (tmpSong != NULL) {
		BarUiMsg (MSG_LIST, "%2u) %s - %s\n", i, tmpSong->artist,
				tmpSong->title);
		i++;
		tmpSong = tmpSong->next;
	}
	BarUiMsg (MSG_QUESTION, "Select song: ");
	if (BarReadlineInt (&i, curFd) == 0) {
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
PianoArtist_t *BarUiSelectArtist (PianoArtist_t *startArtist, FILE *curFd) {
	PianoArtist_t *tmpArtist = NULL;
	int i = 0;

	/* print all artists */
	tmpArtist = startArtist;
	while (tmpArtist != NULL) {
		BarUiMsg (MSG_LIST, "%2u) %s\n", i, tmpArtist->name);
		i++;
		tmpArtist = tmpArtist->next;
	}
	BarUiMsg (MSG_QUESTION, "Select artist: ");
	if (BarReadlineInt (&i, curFd) == 0) {
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
 *	@param read data from fd
 *	@param allow seed suggestions if != NULL
 *	@return musicId or NULL on abort/error
 */
char *BarUiSelectMusicId (PianoHandle_t *ph, WaitressHandle_t *waith,
		FILE *curFd, char *similarToId) {
	char *musicId = NULL;
	char lineBuf[100], selectBuf[2];
	PianoSearchResult_t searchResult;
	PianoArtist_t *tmpArtist;
	PianoSong_t *tmpSong;

	BarUiMsg (MSG_QUESTION, "Search for artist/title: ");
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), 0, curFd) > 0) {
		if (strcmp ("?", lineBuf) == 0 && similarToId != NULL) {
			PianoReturn_t pRet;
			WaitressReturn_t wRet;
			PianoRequestDataGetSeedSuggestions_t reqData;

			reqData.musicId = similarToId;
			reqData.max = 20;

			BarUiMsg (MSG_INFO, "Receiving suggestions... ");
			if (!BarUiPianoCall (ph, PIANO_REQUEST_GET_SEED_SUGGESTIONS,
					waith, &reqData, &pRet, &wRet)) {
				return NULL;
			}
			memcpy (&searchResult, &reqData.searchResult, sizeof (searchResult));
		} else {
			PianoReturn_t pRet;
			WaitressReturn_t wRet;
			PianoRequestDataSearch_t reqData;

			reqData.searchStr = lineBuf;

			BarUiMsg (MSG_INFO, "Searching... ");
			if (!BarUiPianoCall (ph, PIANO_REQUEST_SEARCH, waith, &reqData,
					&pRet, &wRet)) {
				return NULL;
			}
			memcpy (&searchResult, &reqData.searchResult, sizeof (searchResult));
		}
		BarUiMsg (MSG_NONE, "\r");
		if (searchResult.songs != NULL &&
				searchResult.artists != NULL) {
			/* songs and artists found */
			BarUiMsg (MSG_QUESTION, "Is this an [a]rtist or [t]rack name? ");
			BarReadline (selectBuf, sizeof (selectBuf), "at", 1, 0, curFd);
			if (*selectBuf == 'a') {
				tmpArtist = BarUiSelectArtist (searchResult.artists, curFd);
				if (tmpArtist != NULL) {
					musicId = strdup (tmpArtist->musicId);
				}
			} else if (*selectBuf == 't') {
				tmpSong = BarUiSelectSong (searchResult.songs, curFd);
				if (tmpSong != NULL) {
					musicId = strdup (tmpSong->musicId);
				}
			}
		} else if (searchResult.songs != NULL) {
			/* songs found */
			tmpSong = BarUiSelectSong (searchResult.songs, curFd);
			if (tmpSong != NULL) {
				musicId = strdup (tmpSong->musicId);
			}
		} else if (searchResult.artists != NULL) {
			/* artists found */
			tmpArtist = BarUiSelectArtist (searchResult.artists, curFd);
			if (tmpArtist != NULL) {
				musicId = strdup (tmpArtist->musicId);
			}
		} else {
			BarUiMsg (MSG_INFO, "Nothing found...\n");
		}
		PianoDestroySearchResult (&searchResult);
	}

	return musicId;
}

/*	browse genre stations and create shared station
 *	@param piano handle
 */
void BarStationFromGenre (PianoHandle_t *ph, WaitressHandle_t *waith, FILE *curFd) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoGenreCategory_t *curCat;
	PianoStation_t *curStation;
	PianoRequestDataCreateStation_t reqData;
	int i;

	/* receive genre stations list if not yet available */
	if (ph->genreStations == NULL) {
		PianoReturn_t pRet;
		WaitressReturn_t wRet;

		BarUiMsg (MSG_INFO, "Receiving genre stations... ");
		if (!BarUiPianoCall (ph, PIANO_REQUEST_GET_GENRE_STATIONS, waith, NULL,
				&pRet, &wRet)) {
			return;
		}
	}

	/* print all available categories */
	curCat = ph->genreStations;
	i = 0;
	while (curCat != NULL) {
		BarUiMsg (MSG_LIST, "%2i) %s\n", i, curCat->name);
		i++;
		curCat = curCat->next;
	}
	/* select category or exit */
	BarUiMsg (MSG_QUESTION, "Select category: ");
	if (BarReadlineInt (&i, curFd) == 0) {
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
		BarUiMsg (MSG_LIST, "%2i) %s\n", i, curStation->name);
		i++;
		curStation = curStation->next;
	}
	BarUiMsg (MSG_QUESTION, "Select genre: ");
	if (BarReadlineInt (&i, curFd) == 0) {
		return;
	}
	curStation = curCat->stations;
	while (curStation != NULL && i > 0) {
		curStation = curStation->next;
		i--;
	}
	/* create station */
	BarUiMsg (MSG_INFO, "Adding shared station \"%s\"... ", curStation->name);
	reqData.id = curStation->id;
	reqData.type = "sh";
	BarUiPianoCall (ph, PIANO_REQUEST_CREATE_STATION, waith, &reqData, &pRet,
			&wRet);
}

/*	Print station infos (including station id)
 *	@param the station
 */
inline void BarUiPrintStation (PianoStation_t *station) {
	BarUiMsg (MSG_PLAYING, "Station \"%s\" (%s)\n", station->name, station->id);
}

/*	Print song infos (artist, title, album, loved)
 *	@param the song
 *	@param alternative station info (show real station for quickmix, e.g.)
 */
inline void BarUiPrintSong (PianoSong_t *song, PianoStation_t *station) {
	BarUiMsg (MSG_PLAYING, "\"%s\" by \"%s\" on \"%s\"%s%s%s\n",
			song->title, song->artist, song->album,
			(song->rating == PIANO_RATE_LOVE) ? " <3" : "",
			station != NULL ? " @ " : "",
			station != NULL ? station->name : "");
}

/*	Excute external event handler
 *	@param settings containing the cmdline
 *	@param event type
 *	@param current station
 *	@param current song
 *	@param piano error-code (PIANO_RET_OK if not applicable)
 *	@param waitress error-code (WAITRESS_RET_OK if not applicable)
 */
void BarUiStartEventCmd (const BarSettings_t *settings, const char *type,
		const PianoStation_t *curStation, const PianoSong_t *curSong,
		const struct audioPlayer *player, PianoReturn_t pRet,
		WaitressReturn_t wRet) {
	pid_t chld;
	char pipeBuf[1024];
	int pipeFd[2];

	if (settings->eventCmd == NULL) {
		/* nothing to do... */
		return;
	}

	/* prepare stdin content */
	memset (pipeBuf, 0, sizeof (pipeBuf));
	snprintf (pipeBuf, sizeof (pipeBuf),
			"artist=%s\n"
			"title=%s\n"
			"album=%s\n"
			"stationName=%s\n"
			"pRet=%i\n"
			"pRetStr=%s\n"
			"wRet=%i\n"
			"wRetStr=%s\n"
			"songDuration=%lu\n"
			"songPlayed=%lu\n"
			"rating=%i\n",
			curSong == NULL ? "" : curSong->artist,
			curSong == NULL ? "" : curSong->title,
			curSong == NULL ? "" : curSong->album,
			curStation == NULL ? "" : curStation->name,
			pRet,
			PianoErrorToStr (pRet),
			wRet,
			WaitressErrorToStr (wRet),
			player->songDuration,
			player->songPlayed,
			curSong == NULL ? PIANO_RATE_NONE : curSong->rating
			);
	
	if (pipe (pipeFd) == -1) {
		BarUiMsg (MSG_ERR, "Cannot create eventcmd pipe. (%s)\n", strerror (errno));
		return;
	}

	chld = fork ();
	if (chld == 0) {
		/* child */
		close (pipeFd[1]);
		dup2 (pipeFd[0], fileno (stdin));
		execl (settings->eventCmd, settings->eventCmd, type, (char *) NULL);
		BarUiMsg (MSG_ERR, "Cannot start eventcmd. (%s)\n", strerror (errno));
		close (pipeFd[0]);
		exit (1);
	} else if (chld == -1) {
		BarUiMsg (MSG_ERR, "Cannot fork eventcmd. (%s)\n", strerror (errno));
	} else {
		/* parent */
		int status;
		close (pipeFd[0]);
		write (pipeFd[1], pipeBuf, strlen (pipeBuf));
		close (pipeFd[1]);
		/* wait to get rid of the zombie */
		waitpid (chld, &status, 0);
	}
}
