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

#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 1 /* fileno() */
#define _BSD_SOURCE /* strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

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
static const char *BarStrCaseStr (const char *haystack, const char *needle) {
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
void BarUiMsg (const BarSettings_t *settings, const BarUiMsg_t type,
		const char *format, ...) {
	va_list fmtargs;

	assert (settings != NULL);
	assert (type < MSG_COUNT);
	assert (format != NULL);

	switch (type) {
		case MSG_INFO:
		case MSG_PLAYING:
		case MSG_TIME:
		case MSG_ERR:
		case MSG_QUESTION:
		case MSG_LIST:
			/* print ANSI clear line */
			fputs ("\033[2K", stdout);
			break;

		default:
			break;
	}

	if (settings->msgFormat[type].prefix != NULL) {
		fputs (settings->msgFormat[type].prefix, stdout);
	}

	va_start (fmtargs, format);
	vprintf (format, fmtargs);
	va_end (fmtargs);

	if (settings->msgFormat[type].postfix != NULL) {
		fputs (settings->msgFormat[type].postfix, stdout);
	}

	fflush (stdout);
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
	waith->url.path = req->urlPath;

	return WaitressFetchBuf (waith, &req->responseData);
}

/*	piano wrapper: prepare/execute http request and pass result back to
 *	libpiano (updates data structures)
 *	@param app handle
 *	@param request type
 *	@param request data
 *	@param stores piano return code
 *	@param stores waitress return code
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
			BarUiMsg (&app->settings, MSG_NONE, "Error: %s\n", PianoErrorToStr (*pRet));
			PianoDestroyRequest (&req);
			return 0;
		}

		*wRet = BarPianoHttpRequest (&app->waith, &req);
		if (*wRet != WAITRESS_RET_OK) {
			BarUiMsg (&app->settings, MSG_NONE, "Network error: %s\n", WaitressErrorToStr (*wRet));
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

				BarUiMsg (&app->settings, MSG_NONE, "Reauthentication required... ");
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
					BarUiMsg (&app->settings, MSG_INFO, "Trying again... ");
				}
			} else if (*pRet != PIANO_RET_OK) {
				BarUiMsg (&app->settings, MSG_NONE, "Error: %s\n", PianoErrorToStr (*pRet));
				if (req.responseData != NULL) {
					free (req.responseData);
				}
				PianoDestroyRequest (&req);
				return 0;
			} else {
				BarUiMsg (&app->settings, MSG_NONE, "Ok.\n");
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
static PianoStation_t **BarSortedStations (PianoStation_t *unsortedStations,
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
 *	@param app handle
 *	@param prompt string
 *	@param called if input was not a number
 *	@return pointer to selected station or NULL
 */
PianoStation_t *BarUiSelectStation (BarApp_t *app, PianoStation_t *stations,
		const char *prompt, BarUiSelectStationCallback_t callback) {
	PianoStation_t **sortedStations = NULL, *retStation = NULL;
	size_t stationCount, i;
	char buf[100];

	if (stations == NULL) {
		BarUiMsg (&app->settings, MSG_ERR, "No station available.\n");
		return NULL;
	}

	memset (buf, 0, sizeof (buf));

	/* sort and print stations */
	sortedStations = BarSortedStations (stations, &stationCount,
			app->settings.sortOrder);

	do {
		for (i = 0; i < stationCount; i++) {
			const PianoStation_t *currStation = sortedStations[i];
			/* filter stations */
			if (BarStrCaseStr (currStation->name, buf) != NULL) {
				BarUiMsg (&app->settings, MSG_LIST, "%2i) %c%c%c %s\n", i,
						currStation->useQuickMix ? 'q' : ' ',
						currStation->isQuickMix ? 'Q' : ' ',
						!currStation->isCreator ? 'S' : ' ',
						currStation->name);
			}
		}

		BarUiMsg (&app->settings, MSG_QUESTION, prompt);
		if (BarReadlineStr (buf, sizeof (buf), &app->input,
				BAR_RL_DEFAULT) == 0) {
			free (sortedStations);
			return NULL;
		}

		if (isnumeric (buf)) {
			unsigned long selected = strtoul (buf, NULL, 0);
			if (selected < stationCount) {
				retStation = sortedStations[selected];
			}
		}

		/* hand over buffer to external function if it was not a station number */
		if (retStation == NULL && callback != NULL) {
			callback (app, buf);
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

		BarUiMsg (settings, MSG_QUESTION, "Select song: ");
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
 *	@param app handle
 *	@param artists (linked list)
 *	@return pointer to selected artist or NULL on abort
 */
PianoArtist_t *BarUiSelectArtist (BarApp_t *app, PianoArtist_t *startArtist) {
	PianoArtist_t *tmpArtist = NULL;
	char buf[100];
	unsigned long i;

	memset (buf, 0, sizeof (buf));

	do {
		/* print all artists */
		i = 0;
		tmpArtist = startArtist;
		while (tmpArtist != NULL) {
			if (BarStrCaseStr (tmpArtist->name, buf) != NULL) {
				BarUiMsg (&app->settings, MSG_LIST, "%2u) %s\n", i, tmpArtist->name);
			}
			i++;
			tmpArtist = tmpArtist->next;
		}

		BarUiMsg (&app->settings, MSG_QUESTION, "Select artist: ");
		if (BarReadlineStr (buf, sizeof (buf), &app->input,
				BAR_RL_DEFAULT) == 0) {
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
 *	@param seed suggestion station
 *	@param seed suggestion musicid
 *	@param prompt string
 *	@return musicId or NULL on abort/error
 */
char *BarUiSelectMusicId (BarApp_t *app, PianoStation_t *station,
		PianoSong_t *similarSong, const char *msg) {
	char *musicId = NULL;
	char lineBuf[100], selectBuf[2];
	PianoSearchResult_t searchResult;
	PianoArtist_t *tmpArtist;
	PianoSong_t *tmpSong;

	BarUiMsg (&app->settings, MSG_QUESTION, msg);
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), &app->input,
			BAR_RL_DEFAULT) > 0) {
		if (strcmp ("?", lineBuf) == 0 && station != NULL &&
				similarSong != NULL) {
			PianoReturn_t pRet;
			WaitressReturn_t wRet;
			PianoRequestDataGetSeedSuggestions_t reqData;

			reqData.station = station;
			reqData.musicId = similarSong->musicId;
			reqData.max = 20;

			BarUiMsg (&app->settings, MSG_INFO, "Receiving suggestions... ");
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

			BarUiMsg (&app->settings, MSG_INFO, "Searching... ");
			if (!BarUiPianoCall (app, PIANO_REQUEST_SEARCH, &reqData, &pRet,
					&wRet)) {
				return NULL;
			}
			memcpy (&searchResult, &reqData.searchResult, sizeof (searchResult));
		}
		BarUiMsg (&app->settings, MSG_NONE, "\r");
		if (searchResult.songs != NULL &&
				searchResult.artists != NULL) {
			/* songs and artists found */
			BarUiMsg (&app->settings, MSG_QUESTION, "Is this an [a]rtist or [t]rack name? ");
			BarReadline (selectBuf, sizeof (selectBuf), "at", &app->input,
					BAR_RL_FULLRETURN, -1);
			if (*selectBuf == 'a') {
				tmpArtist = BarUiSelectArtist (app, searchResult.artists);
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
			tmpArtist = BarUiSelectArtist (app, searchResult.artists);
			if (tmpArtist != NULL) {
				musicId = strdup (tmpArtist->musicId);
			}
		} else {
			BarUiMsg (&app->settings, MSG_INFO, "Nothing found...\n");
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

		BarUiMsg (&app->settings, MSG_INFO, "Receiving genre stations... ");
		if (!BarUiPianoCall (app, PIANO_REQUEST_GET_GENRE_STATIONS, NULL,
				&pRet, &wRet)) {
			return;
		}
	}

	/* print all available categories */
	curCat = app->ph.genreStations;
	i = 0;
	while (curCat != NULL) {
		BarUiMsg (&app->settings, MSG_LIST, "%2i) %s\n", i, curCat->name);
		i++;
		curCat = curCat->next;
	}

	do {
		/* select category or exit */
		BarUiMsg (&app->settings, MSG_QUESTION, "Select category: ");
		if (BarReadlineInt (&i, &app->input) == 0) {
			return;
		}
		curCat = app->ph.genreStations;
		while (curCat != NULL && i > 0) {
			curCat = curCat->next;
			i--;
		}
	} while (curCat == NULL);
	
	/* print all available stations */
	curGenre = curCat->genres;
	i = 0;
	while (curGenre != NULL) {
		BarUiMsg (&app->settings, MSG_LIST, "%2i) %s\n", i, curGenre->name);
		i++;
		curGenre = curGenre->next;
	}

	do {
		BarUiMsg (&app->settings, MSG_QUESTION, "Select genre: ");
		if (BarReadlineInt (&i, &app->input) == 0) {
			return;
		}
		curGenre = curCat->genres;
		while (curGenre != NULL && i > 0) {
			curGenre = curGenre->next;
			i--;
		}
	} while (curGenre == NULL);

	/* create station */
	BarUiMsg (&app->settings, MSG_INFO, "Adding shared station \"%s\"... ", curGenre->name);
	reqData.id = curGenre->musicId;
	reqData.type = "mi";
	BarUiPianoCall (app, PIANO_REQUEST_CREATE_STATION, &reqData, &pRet, &wRet);
}

/*	replaces format characters (%x) in format string with custom strings
 *	@param destination buffer
 *	@param dest buffer size
 *	@param format string
 *	@param format characters
 *	@param replacement for each given format character
 */
void BarUiCustomFormat (char *dest, size_t destSize, const char *format,
		const char *formatChars, const char **formatVals) {
	bool haveFormatChar = false;

	while (*format != '\0' && destSize > 1) {
		if (*format == '%' && !haveFormatChar) {
			haveFormatChar = true;
		} else if (haveFormatChar) {
			const char *testChar = formatChars;
			const char *val = NULL;

			/* search for format character */
			while (*testChar != '\0') {
				if (*testChar == *format) {
					val = formatVals[(testChar-formatChars)/sizeof (*testChar)];
					break;
				}
				++testChar;
			}

			if (val != NULL) {
				/* concat */
				while (*val != '\0' && destSize > 1) {
					*dest = *val;
					++val;
					++dest;
					--destSize;
				}
			} else {
				/* invalid format character */
				*dest = '%';
				++dest;
				--destSize;
				if (destSize > 1) {
					*dest = *format;
					++dest;
					--destSize;
				}
			}

			haveFormatChar = false;
		} else {
			/* copy */
			*dest = *format;
			++dest;
			--destSize;
		}
		++format;
	}
	*dest = '\0';
}

/*	append \n to string
 */
static void BarUiAppendNewline (char *s, size_t maxlen) {
	size_t len;

	/* append \n */
	if ((len = strlen (s)) == maxlen-1) {
		s[maxlen-2] = '\n';
	} else {
		s[len] = '\n';
		s[len+1] = '\0';
	}
}

/*	Print customizeable station infos
 *	@param pianobar settings
 *	@param the station
 */
inline void BarUiPrintStation (const BarSettings_t *settings,
		PianoStation_t *station) {
	char outstr[512];
	const char *vals[] = {station->name, station->id};

	BarUiCustomFormat (outstr, sizeof (outstr), settings->npStationFormat,
			"ni", vals);
	BarUiAppendNewline (outstr, sizeof (outstr));
	BarUiMsg (settings, MSG_PLAYING, outstr);
}

/*	Print song infos (artist, title, album, loved)
 *	@param pianobar settings
 *	@param the song
 *	@param alternative station info (show real station for quickmix, e.g.)
 */
inline void BarUiPrintSong (const BarSettings_t *settings,
		const PianoSong_t *song, const PianoStation_t *station) {
	char outstr[512];
	const char *vals[] = {song->title, song->artist, song->album,
			(song->rating == PIANO_RATE_LOVE) ? settings->loveIcon : "",
			station != NULL ? settings->atIcon : "",
			station != NULL ? station->name : "",
			song->detailUrl};

	BarUiCustomFormat (outstr, sizeof (outstr), settings->npSongFormat,
			"talr@su", vals);
	BarUiAppendNewline (outstr, sizeof (outstr));
	BarUiMsg (settings, MSG_PLAYING, outstr);
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
	char digits[4];

	while (song != NULL) {
		if (filter == NULL ||
				(filter != NULL && (BarStrCaseStr (song->artist, filter) != NULL ||
				BarStrCaseStr (song->title, filter) != NULL))) {
			char outstr[512];
			const char *vals[] = {digits, song->artist, song->title,
					(song->rating == PIANO_RATE_LOVE) ? settings->loveIcon :
					((song->rating == PIANO_RATE_BAN) ? settings->banIcon : "")};

			snprintf (digits, sizeof (digits) / sizeof (*digits), "%2zu", i);
			BarUiCustomFormat (outstr, sizeof (outstr), settings->listSongFormat,
					"iatr", vals);
			BarUiAppendNewline (outstr, sizeof (outstr));
			BarUiMsg (settings, MSG_LIST, outstr);
		}
		i++;
		song = song->next;
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
	int pipeFd[2];

	if (settings->eventCmd == NULL) {
		/* nothing to do... */
		return;
	}

	if (pipe (pipeFd) == -1) {
		BarUiMsg (settings, MSG_ERR, "Cannot create eventcmd pipe. (%s)\n", strerror (errno));
		return;
	}

	chld = fork ();
	if (chld == 0) {
		/* child */
		close (pipeFd[1]);
		dup2 (pipeFd[0], fileno (stdin));
		execl (settings->eventCmd, settings->eventCmd, type, (char *) NULL);
		BarUiMsg (settings, MSG_ERR, "Cannot start eventcmd. (%s)\n", strerror (errno));
		close (pipeFd[0]);
		exit (1);
	} else if (chld == -1) {
		BarUiMsg (settings, MSG_ERR, "Cannot fork eventcmd. (%s)\n", strerror (errno));
	} else {
		/* parent */
		int status;
		PianoStation_t *songStation = NULL;
		FILE *pipeWriteFd;

		close (pipeFd[0]);

		pipeWriteFd = fdopen (pipeFd[1], "w");

		if (curSong != NULL && stations != NULL && curStation->isQuickMix) {
			songStation = PianoFindStationById (stations, curSong->stationId);
		}

		fprintf (pipeWriteFd,
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
				"rating=%i\n"
				"detailUrl=%s\n",
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
				curSong == NULL ? PIANO_RATE_NONE : curSong->rating,
				curSong == NULL ? "" : curSong->detailUrl
				);

		if (stations != NULL) {
			/* send station list */
			PianoStation_t **sortedStations = NULL;
			size_t stationCount;
			sortedStations = BarSortedStations (stations, &stationCount,
					settings->sortOrder);
			assert (sortedStations != NULL);

			fprintf (pipeWriteFd, "stationCount=%zd\n", stationCount);

			for (size_t i = 0; i < stationCount; i++) {
				const PianoStation_t *currStation = sortedStations[i];
				fprintf (pipeWriteFd, "station%zd=%s\n", i,
						currStation->name);
			}
			free (sortedStations);
		} else {
			const char * const msg = "stationCount=0\n";
			fwrite (msg, sizeof (*msg), strlen (msg), pipeWriteFd);
		}
	
		/* closes pipeFd[1] as well */
		fclose (pipeWriteFd);
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

