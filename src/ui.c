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
#include <assert.h>
#include <ctype.h> /* tolower() */

/* waitpid () */
#include <sys/types.h>
#include <sys/wait.h>

#include "ui.h"
#include "ui_readline.h"

typedef int (*BarSortFunc_t) (const void *, const void *);

/*	is string a number?
 */
static bool isnumeric (const char *s) {
	if (*s == '\0') {
		return false;
	}
	while (*s != '\0') {
		if (!isdigit ((unsigned char) *s)) {
			return false;
		}
		++s;
	}
	return true;
}

/*	find needle in haystack, ignoring case, and return first position
 */
const char *strcasestr (const char *haystack, const char *needle) {
	const char *needlePos = needle;

	assert (haystack != NULL);
	assert (needle != NULL);

	if (*needle == '\0') {
		return haystack;
	}

	while (*haystack != '\0') {
		if (tolower ((unsigned char) *haystack) == tolower ((unsigned char) *needlePos)) {
			++needlePos;
		} else {
			needlePos = needle;
		}
		++haystack;
		if (*needlePos == '\0') {
			return haystack - strlen (needle);
		}
	}

	return NULL;
}

/*	output message and flush stdout
 *	@param message
 */
inline void BarUiMsg (uiMsg_t type, const char *format, ...) {
	#define ANSI_CLEAR_LINE "\033[2K"
	va_list fmtargs;

	switch (type) {
		case MSG_INFO:
			printf (ANSI_CLEAR_LINE "(i) ");
			break;

		case MSG_PLAYING:
			printf (ANSI_CLEAR_LINE "|>  ");
			break;

		case MSG_TIME:
			printf (ANSI_CLEAR_LINE "#   ");
			break;
		
		case MSG_ERR:
			printf (ANSI_CLEAR_LINE "/!\\ ");
			break;

		case MSG_QUESTION:
			printf (ANSI_CLEAR_LINE "[?] ");
			break;

		case MSG_LIST:
			printf (ANSI_CLEAR_LINE "\t");
			break;
	
		default:
			break;
	}
	va_start (fmtargs, format);
	vprintf (format, fmtargs);
	va_end (fmtargs);

	fflush (stdout);

	#undef ANSI_CLEAR_LINE
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
int BarUiPianoCall (BarApp_t * const app, PianoRequestType_t type,
		void *data, PianoReturn_t *pRet, WaitressReturn_t *wRet) {
	PianoRequest_t req;

	memset (&req, 0, sizeof (req));

	/* repeat as long as there are http requests to do */
	do {
		req.data = data;

		*pRet = PianoRequest (&app->ph, &req, type);
		if (*pRet != PIANO_RET_OK) {
			BarUiMsg (MSG_NONE, "Error: %s\n", PianoErrorToStr (*pRet));
			PianoDestroyRequest (&req);
			return 0;
		}

		*wRet = BarPianoHttpRequest (&app->waith, &req);
		if (*wRet != WAITRESS_RET_OK) {
			BarUiMsg (MSG_NONE, "Network error: %s\n", WaitressErrorToStr (*wRet));
			if (req.responseData != NULL) {
				free (req.responseData);
			}
			PianoDestroyRequest (&req);
			return 0;
		}

		*pRet = PianoResponse (&app->ph, &req);
		if (*pRet != PIANO_RET_CONTINUE_REQUEST) {
			/* checking for request type avoids infinite loops */
			if (*pRet == PIANO_RET_AUTH_TOKEN_INVALID &&
					type != PIANO_REQUEST_LOGIN) {
				/* reauthenticate */
				PianoReturn_t authpRet;
				WaitressReturn_t authwRet;
				PianoRequestDataLogin_t reqData;
				reqData.user = app->settings.username;
				reqData.password = app->settings.password;
				reqData.step = 0;

				BarUiMsg (MSG_NONE, "Reauthentication required... ");
				if (!BarUiPianoCall (app, PIANO_REQUEST_LOGIN, &reqData, &authpRet,
						&authwRet)) {
					*pRet = authpRet;
					*wRet = authwRet;
					if (req.responseData != NULL) {
						free (req.responseData);
					}
					PianoDestroyRequest (&req);
					return 0;
				} else {
					/* try again */
					*pRet = PIANO_RET_CONTINUE_REQUEST;
					BarUiMsg (MSG_INFO, "Trying again... ");
				}
			} else if (*pRet != PIANO_RET_OK) {
				BarUiMsg (MSG_NONE, "Error: %s\n", PianoErrorToStr (*pRet));
				if (req.responseData != NULL) {
					free (req.responseData);
				}
				PianoDestroyRequest (&req);
				return 0;
			} else {
				BarUiMsg (MSG_NONE, "Ok.\n");
			}
		}
		/* we can destroy the request at this point, even when this call needs
		 * more than one http request. persistent data (step counter, e.g.) is
		 * stored in req.data */
		if (req.responseData != NULL) {
			free (req.responseData);
		}
		PianoDestroyRequest (&req);
	} while (*pRet == PIANO_RET_CONTINUE_REQUEST);

	return 1;
}

/*	Station sorting functions */

static inline int BarStationQuickmix01Cmp (const void *a, const void *b) {
	const PianoStation_t *stationA = *((PianoStation_t **) a),
			*stationB = *((PianoStation_t **) b);
	return stationA->isQuickMix - stationB->isQuickMix;
}

/*	sort by station name from a to z, case insensitive
 */
static inline int BarStationNameAZCmp (const void *a, const void *b) {
	const PianoStation_t *stationA = *((PianoStation_t **) a),
			*stationB = *((PianoStation_t **) b);
	return strcasecmp (stationA->name, stationB->name);
}

/*	sort by station name from z to a, case insensitive
 */
static int BarStationNameZACmp (const void *a, const void *b) {
	return BarStationNameAZCmp (b, a);
}

/*	helper for quickmix/name sorting
 */
static inline int BarStationQuickmixNameCmp (const void *a, const void *b,
		const void *c, const void *d) {
	int qmc = BarStationQuickmix01Cmp (a, b);
	return qmc == 0 ? BarStationNameAZCmp (c, d) : qmc;
}

/*	sort by quickmix (no to yes) and name (a to z)
 */
static int BarStationCmpQuickmix01NameAZ (const void *a, const void *b) {
	return BarStationQuickmixNameCmp (a, b, a, b);
}

/*	sort by quickmix (no to yes) and name (z to a)
 */
static int BarStationCmpQuickmix01NameZA (const void *a, const void *b) {
	return BarStationQuickmixNameCmp (a, b, b, a);
}

/*	sort by quickmix (yes to no) and name (a to z)
 */
static int BarStationCmpQuickmix10NameAZ (const void *a, const void *b) {
	return BarStationQuickmixNameCmp (b, a, a, b);
}

/*	sort by quickmix (yes to no) and name (z to a)
 */
static int BarStationCmpQuickmix10NameZA (const void *a, const void *b) {
	return BarStationQuickmixNameCmp (b, a, b, a);
}

/*	sort linked list (station)
 *	@param stations
 *	@return NULL-terminated array with sorted stations
 */
PianoStation_t **BarSortedStations (PianoStation_t *unsortedStations,
		size_t *retStationCount, BarStationSorting_t order) {
	static const BarSortFunc_t orderMapping[] = {BarStationNameAZCmp,
			BarStationNameZACmp,
			BarStationCmpQuickmix01NameAZ,
			BarStationCmpQuickmix01NameZA,
			BarStationCmpQuickmix10NameAZ,
			BarStationCmpQuickmix10NameZA,
			};
	PianoStation_t **stationArray = NULL, *currStation = NULL;
	size_t stationCount = 0, i;

	assert (order < sizeof (orderMapping)/sizeof(*orderMapping));

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

	qsort (stationArray, stationCount, sizeof (*stationArray), orderMapping[order]);

	*retStationCount = stationCount;
	return stationArray;
}

/*	let user pick one station
 *	@param piano handle
 *	@param prompt string
 *	@param station list sort order
 *	@param input fds
 *	@return pointer to selected station or NULL
 */
PianoStation_t *BarUiSelectStation (PianoHandle_t *ph, const char *prompt,
		BarStationSorting_t order, BarReadlineFds_t *input) {
	PianoStation_t **sortedStations = NULL, *retStation = NULL;
	size_t stationCount, i;
	char buf[100];

	if (ph->stations == NULL) {
		BarUiMsg (MSG_ERR, "No station available.\n");
		return NULL;
	}

	memset (buf, 0, sizeof (buf));

	/* sort and print stations */
	sortedStations = BarSortedStations (ph->stations, &stationCount, order);

	do {
		for (i = 0; i < stationCount; i++) {
			const PianoStation_t *currStation = sortedStations[i];
			if (strcasestr (currStation->name, buf) != NULL) {
				BarUiMsg (MSG_LIST, "%2i) %c%c%c %s\n", i,
						currStation->useQuickMix ? 'q' : ' ',
						currStation->isQuickMix ? 'Q' : ' ',
						!currStation->isCreator ? 'S' : ' ',
						currStation->name);
			}
		}

		BarUiMsg (MSG_QUESTION, prompt);
		if (BarReadlineStr (buf, sizeof (buf), input, BAR_RL_DEFAULT) == 0) {
			free (sortedStations);
			return NULL;
		}

		if (isnumeric (buf)) {
			unsigned long selected = strtoul (buf, NULL, 0);
			if (selected < stationCount) {
				retStation = sortedStations[selected];
			}
		}
	} while (retStation == NULL);

	free (sortedStations);
	return retStation;
}

/*	let user pick one song
 *	@param pianobar settings
 *	@param song list
 *	@param input fds
 *	@return pointer to selected item in song list or NULL
 */
PianoSong_t *BarUiSelectSong (const BarSettings_t *settings,
		PianoSong_t *startSong, BarReadlineFds_t *input) {
	PianoSong_t *tmpSong = NULL;
	char buf[100];

	memset (buf, 0, sizeof (buf));

	do {
		BarUiListSongs (settings, startSong, buf);

		BarUiMsg (MSG_QUESTION, "Select song: ");
		if (BarReadlineStr (buf, sizeof (buf), input, BAR_RL_DEFAULT) == 0) {
			return NULL;
		}

		if (isnumeric (buf)) {
			unsigned long i = strtoul (buf, NULL, 0);
			tmpSong = startSong;
			while (tmpSong != NULL && i > 0) {
				tmpSong = tmpSong->next;
				i--;
			}
		}
	} while (tmpSong == NULL);

	return tmpSong;
}

/*	let user pick one artist
 *	@param artists (linked list)
 *	@param input fds
 *	@return pointer to selected artist or NULL on abort
 */
PianoArtist_t *BarUiSelectArtist (PianoArtist_t *startArtist,
		BarReadlineFds_t *input) {
	PianoArtist_t *tmpArtist = NULL;
	char buf[100];
	unsigned long i;

	memset (buf, 0, sizeof (buf));

	do {
		/* print all artists */
		i = 0;
		tmpArtist = startArtist;
		while (tmpArtist != NULL) {
			if (strcasestr (tmpArtist->name, buf) != NULL) {
				BarUiMsg (MSG_LIST, "%2u) %s\n", i, tmpArtist->name);
			}
			i++;
			tmpArtist = tmpArtist->next;
		}

		BarUiMsg (MSG_QUESTION, "Select artist: ");
		if (BarReadlineStr (buf, sizeof (buf), input, BAR_RL_DEFAULT) == 0) {
			return NULL;
		}

		if (isnumeric (buf)) {
			i = strtoul (buf, NULL, 0);
			tmpArtist = startArtist;
			while (tmpArtist != NULL && i > 0) {
				tmpArtist = tmpArtist->next;
				i--;
			}
		}
	} while (tmpArtist == NULL);

	return tmpArtist;
}

/*	search music: query, search request, return music id
 *	@param app handle
 *	@param allow seed suggestions if != NULL
 *	@return musicId or NULL on abort/error
 */
char *BarUiSelectMusicId (BarApp_t *app, char *similarToId) {
	char *musicId = NULL;
	char lineBuf[100], selectBuf[2];
	PianoSearchResult_t searchResult;
	PianoArtist_t *tmpArtist;
	PianoSong_t *tmpSong;

	BarUiMsg (MSG_QUESTION, "Search for artist/title: ");
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), &app->input,
			BAR_RL_DEFAULT) > 0) {
		if (strcmp ("?", lineBuf) == 0 && similarToId != NULL) {
			PianoReturn_t pRet;
			WaitressReturn_t wRet;
			PianoRequestDataGetSeedSuggestions_t reqData;

			reqData.musicId = similarToId;
			reqData.max = 20;

			BarUiMsg (MSG_INFO, "Receiving suggestions... ");
			if (!BarUiPianoCall (app, PIANO_REQUEST_GET_SEED_SUGGESTIONS,
					&reqData, &pRet, &wRet)) {
				return NULL;
			}
			memcpy (&searchResult, &reqData.searchResult, sizeof (searchResult));
		} else {
			PianoReturn_t pRet;
			WaitressReturn_t wRet;
			PianoRequestDataSearch_t reqData;

			reqData.searchStr = lineBuf;

			BarUiMsg (MSG_INFO, "Searching... ");
			if (!BarUiPianoCall (app, PIANO_REQUEST_SEARCH, &reqData, &pRet,
					&wRet)) {
				return NULL;
			}
			memcpy (&searchResult, &reqData.searchResult, sizeof (searchResult));
		}
		BarUiMsg (MSG_NONE, "\r");
		if (searchResult.songs != NULL &&
				searchResult.artists != NULL) {
			/* songs and artists found */
			BarUiMsg (MSG_QUESTION, "Is this an [a]rtist or [t]rack name? ");
			BarReadline (selectBuf, sizeof (selectBuf), "at", &app->input,
					BAR_RL_FULLRETURN, -1);
			if (*selectBuf == 'a') {
				tmpArtist = BarUiSelectArtist (searchResult.artists,
						&app->input);
				if (tmpArtist != NULL) {
					musicId = strdup (tmpArtist->musicId);
				}
			} else if (*selectBuf == 't') {
				tmpSong = BarUiSelectSong (&app->settings, searchResult.songs,
						&app->input);
				if (tmpSong != NULL) {
					musicId = strdup (tmpSong->musicId);
				}
			}
		} else if (searchResult.songs != NULL) {
			/* songs found */
			tmpSong = BarUiSelectSong (&app->settings, searchResult.songs,
					&app->input);
			if (tmpSong != NULL) {
				musicId = strdup (tmpSong->musicId);
			}
		} else if (searchResult.artists != NULL) {
			/* artists found */
			tmpArtist = BarUiSelectArtist (searchResult.artists, &app->input);
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
 *	@param app handle
 */
void BarStationFromGenre (BarApp_t *app) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoGenreCategory_t *curCat;
	PianoGenre_t *curGenre;
	PianoRequestDataCreateStation_t reqData;
	int i;

	/* receive genre stations list if not yet available */
	if (app->ph.genreStations == NULL) {
		PianoReturn_t pRet;
		WaitressReturn_t wRet;

		BarUiMsg (MSG_INFO, "Receiving genre stations... ");
		if (!BarUiPianoCall (app, PIANO_REQUEST_GET_GENRE_STATIONS, NULL,
				&pRet, &wRet)) {
			return;
		}
	}

	/* print all available categories */
	curCat = app->ph.genreStations;
	i = 0;
	while (curCat != NULL) {
		BarUiMsg (MSG_LIST, "%2i) %s\n", i, curCat->name);
		i++;
		curCat = curCat->next;
	}
	/* select category or exit */
	BarUiMsg (MSG_QUESTION, "Select category: ");
	if (BarReadlineInt (&i, &app->input) == 0) {
		return;
	}
	curCat = app->ph.genreStations;
	while (curCat != NULL && i > 0) {
		curCat = curCat->next;
		i--;
	}
	
	/* print all available stations */
	curGenre = curCat->genres;
	i = 0;
	while (curGenre != NULL) {
		BarUiMsg (MSG_LIST, "%2i) %s\n", i, curGenre->name);
		i++;
		curGenre = curGenre->next;
	}
	BarUiMsg (MSG_QUESTION, "Select genre: ");
	if (BarReadlineInt (&i, &app->input) == 0) {
		return;
	}
	curGenre = curCat->genres;
	while (curGenre != NULL && i > 0) {
		curGenre = curGenre->next;
		i--;
	}
	/* create station */
	BarUiMsg (MSG_INFO, "Adding shared station \"%s\"... ", curGenre->name);
	reqData.id = curGenre->musicId;
	reqData.type = "mi";
	BarUiPianoCall (app, PIANO_REQUEST_CREATE_STATION, &reqData, &pRet, &wRet);
}

/*	Print station infos (including station id)
 *	@param the station
 */
inline void BarUiPrintStation (PianoStation_t *station) {
	BarUiMsg (MSG_PLAYING, "Station \"%s\" (%s)\n", station->name, station->id);
}

/*	Print song infos (artist, title, album, loved)
 *	@param pianobar settings
 *	@param the song
 *	@param alternative station info (show real station for quickmix, e.g.)
 */
inline void BarUiPrintSong (const BarSettings_t *settings,
		const PianoSong_t *song, const PianoStation_t *station) {
	BarUiMsg (MSG_PLAYING, "\"%s\" by \"%s\" on \"%s\"%s%s%s%s\n",
			song->title, song->artist, song->album,
			(song->rating == PIANO_RATE_LOVE) ? " " : "",
			(song->rating == PIANO_RATE_LOVE) ? settings->loveIcon : "",
			station != NULL ? " @ " : "",
			station != NULL ? station->name : "");
}

/*	Print list of songs
 *	@param pianobar settings
 *	@param linked list of songs
 *	@param artist/song filter string
 *	@return # of songs
 */
size_t BarUiListSongs (const BarSettings_t *settings,
		const PianoSong_t *song, const char *filter) {
	size_t i = 0;

	while (song != NULL) {
		if (filter == NULL ||
				(filter != NULL && (strcasestr (song->artist, filter) != NULL ||
				strcasestr (song->title, filter) != NULL))) {
			BarUiMsg (MSG_LIST, "%2lu) %s - %s %s%s\n", i, song->artist,
					song->title,
					(song->rating == PIANO_RATE_LOVE) ? settings->loveIcon : "",
					(song->rating == PIANO_RATE_BAN) ? settings->banIcon : "");
		}
		song = song->next;
		i++;
	}

	return i;
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
		const struct audioPlayer *player, PianoStation_t *stations,
                PianoReturn_t pRet, WaitressReturn_t wRet) {
	pid_t chld;
	char pipeBuf[1024];
	int pipeFd[2];
	PianoStation_t *songStation = NULL;

	if (settings->eventCmd == NULL) {
		/* nothing to do... */
		return;
	}

	if (curSong != NULL && stations != NULL && curStation->isQuickMix) {
		songStation = PianoFindStationById (stations, curSong->stationId);
	}

	/* prepare stdin content */
	memset (pipeBuf, 0, sizeof (pipeBuf));
	snprintf (pipeBuf, sizeof (pipeBuf),
			"artist=%s\n"
			"title=%s\n"
			"album=%s\n"
			"coverArt=%s\n"
			"stationName=%s\n"
			"songStationName=%s\n"
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
			curSong == NULL ? "" : curSong->coverArt,
			curStation == NULL ? "" : curStation->name,
			songStation == NULL ? "" : songStation->name,
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

		if (stations != NULL) {
			/* send station list */
			PianoStation_t **sortedStations = NULL;
			size_t stationCount;
			sortedStations = BarSortedStations (stations, &stationCount,
					settings->sortOrder);
			assert (sortedStations != NULL);

			snprintf (pipeBuf, sizeof (pipeBuf), "stationCount=%zd\n", stationCount);
			write (pipeFd[1], pipeBuf, strlen (pipeBuf));

			for (size_t i = 0; i < stationCount; i++) {
				const PianoStation_t *currStation = sortedStations[i];
				snprintf (pipeBuf, sizeof (pipeBuf), "station%zd=%s\n", i,
						currStation->name);
				write (pipeFd[1], pipeBuf, strlen (pipeBuf));
			}
			free (sortedStations);
		} else {
			const char *msg = "stationCount=0\n";
			write (pipeFd[1], msg, strlen (msg));
		}
	
		close (pipeFd[1]);
		/* wait to get rid of the zombie */
		waitpid (chld, &status, 0);
	}
}

/*	prepend song to history, must not be a list of songs as ->next is modified!
 */
void BarUiHistoryPrepend (BarApp_t *app, PianoSong_t *song) {
	if (app->settings.history != 0) {
		PianoSong_t *tmpSong;

		song->next = app->songHistory;
		app->songHistory = song;

		/* limit history's length */
		/* start with 1, so we're stopping at n-1 and have the
		 * chance to set ->next = NULL */
		unsigned int i = 1;
		tmpSong = app->songHistory;
		while (i < app->settings.history && tmpSong != NULL) {
			tmpSong = tmpSong->next;
			++i;
		}
		/* if too many songs in history... */
		if (tmpSong != NULL) {
			PianoSong_t *delSong = tmpSong->next;
			tmpSong->next = NULL;
			if (delSong != NULL) {
				PianoDestroyPlaylist (delSong);
			}
		}
	}
}

