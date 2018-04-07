/*
Copyright (c) 2008-2018
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

#include "config.h"

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

typedef struct {
	char *data;
	size_t pos;
} buffer;

static size_t httpFetchCb (char *ptr, size_t size, size_t nmemb,
		void *userdata) {
	buffer * const buffer = userdata;
	size_t recvSize = size * nmemb;

	if (buffer->data == NULL) {
		if ((buffer->data = malloc (sizeof (*buffer->data) *
				(recvSize + 1))) == NULL) {
			return 0;
		}
	} else {
		char *newbuf;
		if ((newbuf = realloc (buffer->data, sizeof (*buffer->data) *
				(buffer->pos + recvSize + 1))) == NULL) {
			free (buffer->data);
			return 0;
		}
		buffer->data = newbuf;
	}
	memcpy (buffer->data + buffer->pos, ptr, recvSize);
	buffer->pos += recvSize;
	buffer->data[buffer->pos] = '\0';

	return recvSize;
}

/*	libcurl progress callback. aborts the current request if user pressed ^C
 */
int progressCb (void * const data, double dltotal, double dlnow,
		double ultotal, double ulnow) {
	const sig_atomic_t lint = *((sig_atomic_t *) data);
	if (lint) {
		return 1;
	} else {
		return 0;
	}
}

#define setAndCheck(k,v) \
	httpret = curl_easy_setopt (http, k, v); \
	assert (httpret == CURLE_OK);

static CURLcode BarPianoHttpRequest (CURL * const http,
		const BarSettings_t * const settings, PianoRequest_t * const req) {
	buffer buffer = {NULL, 0};
	sig_atomic_t lint = 0, *prevint;

	char url[2048];
	assert (settings->rpcHost != NULL);
	assert (settings->rpcTlsPort != NULL);
	assert (req->urlPath != NULL);
	int ret = snprintf (url, sizeof (url), "%s://%s:%s%s",
		req->secure ? "https" : "http",
		settings->rpcHost,
		req->secure ? settings->rpcTlsPort : "80",
		req->urlPath);
	assert (ret >= 0 && ret <= (int) sizeof (url));

	/* save the previous interrupt destination */
	prevint = interrupted;
	interrupted = &lint;

	curl_easy_reset (http);
	CURLcode httpret;
	setAndCheck (CURLOPT_URL, url);
	setAndCheck (CURLOPT_USERAGENT, PACKAGE "-" VERSION);
	setAndCheck (CURLOPT_POSTFIELDS, req->postData);
	setAndCheck (CURLOPT_WRITEFUNCTION, httpFetchCb);
	setAndCheck (CURLOPT_WRITEDATA, &buffer);
	setAndCheck (CURLOPT_PROGRESSFUNCTION, progressCb);
	setAndCheck (CURLOPT_PROGRESSDATA, &lint);
	setAndCheck (CURLOPT_NOPROGRESS, 0);
	setAndCheck (CURLOPT_POST, 1);
	setAndCheck (CURLOPT_TIMEOUT, settings->timeout);
	if (settings->caBundle != NULL) {
		setAndCheck (CURLOPT_CAINFO, settings->caBundle);
	}

	if (settings->bindTo!= NULL) {
		if (curl_easy_setopt (http, CURLOPT_INTERFACE,
				settings->bindTo) != CURLE_OK) {
			/* if binding fails, notice about that */
			BarUiMsg (settings, MSG_ERR, "bindTo (%s) is invalid!\n",
					settings->bindTo);
		}
	}

	/* set up proxy (control proxy for non-us citizen or global proxy for poor
	 * firewalled fellows) */
	if (settings->controlProxy != NULL) {
		/* control proxy overrides global proxy */
		if (curl_easy_setopt (http, CURLOPT_PROXY,
				settings->controlProxy) != CURLE_OK) {
			/* if setting proxy fails, url is invalid */
			BarUiMsg (settings, MSG_ERR, "Control proxy (%s) is invalid!\n",
					 settings->controlProxy);
		}
	} else if (settings->proxy != NULL && strlen (settings->proxy) > 0) {
		if (curl_easy_setopt (http, CURLOPT_PROXY,
				settings->proxy) != CURLE_OK) {
			/* if setting proxy fails, url is invalid */
			BarUiMsg (settings, MSG_ERR, "Proxy (%s) is invalid!\n",
					 settings->proxy);
		}
	}

	struct curl_slist *list = NULL;
	list = curl_slist_append (list, "Content-Type: text/plain");
	setAndCheck (CURLOPT_HTTPHEADER, list);

	unsigned int retry = 0;
	do {
		httpret = curl_easy_perform (http);
		++retry;
		if (httpret == CURLE_OPERATION_TIMEDOUT) {
			free (buffer.data);
			buffer.data = NULL;
			buffer.pos = 0;
			if (retry > settings->maxRetry) {
				break;
			}
		} else {
			break;
		}
	} while (true);

	curl_slist_free_all (list);

	req->responseData = buffer.data;

	interrupted = prevint;

	return httpret;
}

/*	piano wrapper: prepare/execute http request and pass result back to
 *	libpiano
 */
bool BarUiPianoCall (BarApp_t * const app, const PianoRequestType_t type,
		void * const data, PianoReturn_t * const pRet, CURLcode * const wRet) {
	PianoReturn_t pRetLocal = PIANO_RET_OK;
	CURLcode wRetLocal = CURLE_OK;
	bool ret = false;

	/* repeat as long as there are http requests to do */
	do {
		PianoRequest_t req = { .data = data, .responseData = NULL };

		pRetLocal = PianoRequest (&app->ph, &req, type);
		if (pRetLocal != PIANO_RET_OK) {
			BarUiMsg (&app->settings, MSG_NONE, "Error: %s\n",
					PianoErrorToStr (pRetLocal));
			goto cleanup;
		}

		wRetLocal = BarPianoHttpRequest (app->http, &app->settings, &req);
		if (wRetLocal == CURLE_ABORTED_BY_CALLBACK) {
			BarUiMsg (&app->settings, MSG_NONE, "Interrupted.\n");
			goto cleanup;
		} else if (wRetLocal != CURLE_OK) {
			BarUiMsg (&app->settings, MSG_NONE, "Network error: %s\n",
					curl_easy_strerror (wRetLocal));
			goto cleanup;
		}

		pRetLocal = PianoResponse (&app->ph, &req);
		if (pRetLocal != PIANO_RET_CONTINUE_REQUEST) {
			/* checking for request type avoids infinite loops */
			if (pRetLocal == PIANO_RET_P_INVALID_AUTH_TOKEN &&
					type != PIANO_REQUEST_LOGIN) {
				/* reauthenticate */
				PianoRequestDataLogin_t reqData;
				reqData.user = app->settings.username;
				reqData.password = app->settings.password;
				reqData.step = 0;

				BarUiMsg (&app->settings, MSG_NONE,
						"Reauthentication required... ");
				if (!BarUiPianoCall (app, PIANO_REQUEST_LOGIN, &reqData,
						&pRetLocal, &wRetLocal)) {
					goto cleanup;
				} else {
					/* try again */
					pRetLocal = PIANO_RET_CONTINUE_REQUEST;
					BarUiMsg (&app->settings, MSG_INFO, "Trying again... ");
				}
			} else if (pRetLocal != PIANO_RET_OK) {
				BarUiMsg (&app->settings, MSG_NONE, "Error: %s\n",
						PianoErrorToStr (pRetLocal));
				goto cleanup;
			} else {
				BarUiMsg (&app->settings, MSG_NONE, "Ok.\n");
				ret = true;
			}
		}

cleanup:
		/* persistent data is stored in req.data */
		free (req.responseData);
		PianoDestroyRequest (&req);
	} while (pRetLocal == PIANO_RET_CONTINUE_REQUEST);

	*pRet = pRetLocal;
	*wRet = wRetLocal;

	return ret;
}

/*	Station sorting functions */

static inline int BarStationQuickmix01Cmp (const void *a, const void *b) {
	const PianoStation_t *stationA = *((PianoStation_t * const *) a),
			*stationB = *((PianoStation_t * const *) b);
	return stationA->isQuickMix - stationB->isQuickMix;
}

/*	sort by station name from a to z, case insensitive
 */
static inline int BarStationNameAZCmp (const void *a, const void *b) {
	const PianoStation_t *stationA = *((PianoStation_t * const *) a),
			*stationB = *((PianoStation_t * const *) b);
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

	stationCount = PianoListCountP (unsortedStations);
	stationArray = calloc (stationCount, sizeof (*stationArray));

	/* copy station pointers */
	i = 0;
	currStation = unsortedStations;
	PianoListForeachP (currStation) {
		stationArray[i] = currStation;
		++i;
	}

	qsort (stationArray, stationCount, sizeof (*stationArray), orderMapping[order]);

	*retStationCount = stationCount;
	return stationArray;
}

/*	let user pick one station
 *	@param app handle
 *	@param stations that should be listed
 *	@param prompt string
 *	@param called if input was not a number
 *	@param auto-select if only one station remains after filtering
 *	@return pointer to selected station or NULL
 */
PianoStation_t *BarUiSelectStation (BarApp_t *app, PianoStation_t *stations,
		const char *prompt, BarUiSelectStationCallback_t callback,
		bool autoselect) {
	PianoStation_t **sortedStations = NULL, *retStation = NULL;
	size_t stationCount, i, lastDisplayed, displayCount;
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
		displayCount = 0;
		for (i = 0; i < stationCount; i++) {
			const PianoStation_t *currStation = sortedStations[i];
			/* filter stations */
			if (BarStrCaseStr (currStation->name, buf) != NULL) {
				BarUiMsg (&app->settings, MSG_LIST, "%2zi) %c%c%c %s\n", i,
						currStation->useQuickMix ? 'q' : ' ',
						currStation->isQuickMix ? 'Q' : ' ',
						!currStation->isCreator ? 'S' : ' ',
						currStation->name);
				++displayCount;
				lastDisplayed = i;
			}
		}

		BarUiMsg (&app->settings, MSG_QUESTION, "%s", prompt);
		if (autoselect && displayCount == 1 && stationCount != 1) {
			/* auto-select last remaining station */
			BarUiMsg (&app->settings, MSG_NONE, "%zi\n", lastDisplayed);
			retStation = sortedStations[lastDisplayed];
		} else {
			if (BarReadlineStr (buf, sizeof (buf), &app->input,
					BAR_RL_DEFAULT) == 0) {
				break;
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
		}
	} while (retStation == NULL);

	free (sortedStations);
	return retStation;
}

/*	let user pick one song
 *	@param app
 *	@param song list
 *	@param input fds
 *	@return pointer to selected item in song list or NULL
 */
PianoSong_t *BarUiSelectSong (const BarApp_t * const app,
		PianoSong_t *startSong, BarReadlineFds_t *input) {
	const BarSettings_t * const settings = &app->settings;
	PianoSong_t *tmpSong = NULL;
	char buf[100];

	memset (buf, 0, sizeof (buf));

	do {
		BarUiListSongs (app, startSong, buf);

		BarUiMsg (settings, MSG_QUESTION, "Select song: ");
		if (BarReadlineStr (buf, sizeof (buf), input, BAR_RL_DEFAULT) == 0) {
			return NULL;
		}

		if (isnumeric (buf)) {
			unsigned long i = strtoul (buf, NULL, 0);
			tmpSong = PianoListGetP (startSong, i);
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
		PianoListForeachP (tmpArtist) {
			if (BarStrCaseStr (tmpArtist->name, buf) != NULL) {
				BarUiMsg (&app->settings, MSG_LIST, "%2lu) %s\n", i,
						tmpArtist->name);
			}
			i++;
		}

		BarUiMsg (&app->settings, MSG_QUESTION, "Select artist: ");
		if (BarReadlineStr (buf, sizeof (buf), &app->input,
				BAR_RL_DEFAULT) == 0) {
			return NULL;
		}

		if (isnumeric (buf)) {
			i = strtoul (buf, NULL, 0);
			tmpArtist = PianoListGetP (startArtist, i);
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
		const char *msg) {
	char *musicId = NULL;
	char lineBuf[100], selectBuf[2];
	PianoSearchResult_t searchResult;
	PianoArtist_t *tmpArtist;
	PianoSong_t *tmpSong;

	BarUiMsg (&app->settings, MSG_QUESTION, "%s", msg);
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), &app->input,
			BAR_RL_DEFAULT) > 0) {
		PianoReturn_t pRet;
		CURLcode wRet;
		PianoRequestDataSearch_t reqData;

		reqData.searchStr = lineBuf;

		BarUiMsg (&app->settings, MSG_INFO, "Searching... ");
		if (!BarUiPianoCall (app, PIANO_REQUEST_SEARCH, &reqData, &pRet,
				&wRet)) {
			return NULL;
		}
		memcpy (&searchResult, &reqData.searchResult, sizeof (searchResult));

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
				tmpSong = BarUiSelectSong (app, searchResult.songs,
						&app->input);
				if (tmpSong != NULL) {
					musicId = strdup (tmpSong->musicId);
				}
			}
		} else if (searchResult.songs != NULL) {
			/* songs found */
			tmpSong = BarUiSelectSong (app, searchResult.songs,
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

/*	replaces format characters (%x) in format string with custom strings
 *	@param destination buffer
 *	@param dest buffer size
 *	@param format string
 *	@param format characters
 *	@param replacement for each given format character
 */
static void BarUiCustomFormat (char *dest, size_t destSize, const char *format,
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
void BarUiPrintStation (const BarSettings_t *settings,
		PianoStation_t *station) {
	char outstr[512];
	const char *vals[] = {station->name, station->id};

	BarUiCustomFormat (outstr, sizeof (outstr), settings->npStationFormat,
			"ni", vals);
	BarUiAppendNewline (outstr, sizeof (outstr));
	BarUiMsg (settings, MSG_PLAYING, "%s", outstr);
}

static const char *ratingToIcon (const BarSettings_t * const settings,
		const PianoSong_t * const song) {
	switch (song->rating) {
		case PIANO_RATE_LOVE:
			return settings->loveIcon;

		case PIANO_RATE_BAN:
			return settings->banIcon;

		case PIANO_RATE_TIRED:
			return settings->tiredIcon;

		default:
			return "";
	}
}

/*	Print song infos (artist, title, album, loved)
 *	@param pianobar settings
 *	@param the song
 *	@param alternative station info (show real station for quickmix, e.g.)
 */
void BarUiPrintSong (const BarSettings_t *settings,
		const PianoSong_t *song, const PianoStation_t *station) {
	char outstr[512];
	const char *vals[] = {song->title, song->artist, song->album,
			ratingToIcon (settings, song),
			station != NULL ? settings->atIcon : "",
			station != NULL ? station->name : "",
			song->detailUrl};

	BarUiCustomFormat (outstr, sizeof (outstr), settings->npSongFormat,
			"talr@su", vals);
	BarUiAppendNewline (outstr, sizeof (outstr));
	BarUiMsg (settings, MSG_PLAYING, "%s", outstr);
}

/*	Print list of songs
 *	@param pianobar settings
 *	@param linked list of songs
 *	@param artist/song filter string
 *	@return # of songs
 */
size_t BarUiListSongs (const BarApp_t * const app,
		const PianoSong_t *song, const char *filter) {
	const BarSettings_t * const settings = &app->settings;
	size_t i = 0;

	PianoListForeachP (song) {
		if (filter == NULL ||
				(filter != NULL && (BarStrCaseStr (song->artist, filter) != NULL ||
				BarStrCaseStr (song->title, filter) != NULL))) {
			const char * const deleted = "(deleted)", * const empty = "";
			const char *stationName = empty;

			const PianoStation_t * const station =
					PianoFindStationById (app->ph.stations, song->stationId);
			if (station != NULL && station != app->curStation) {
				stationName = station->name;
			} else if (station == NULL && song->stationId != NULL) {
				stationName = deleted;
			}

			char outstr[512], digits[8], duration[8] = "??:??";
			const char *vals[] = {digits, song->artist, song->title,
					ratingToIcon (settings, song),
					duration,
					stationName != empty ? settings->atIcon : "",
					stationName,
					};

			/* pre-format a few strings */
			snprintf (digits, sizeof (digits) / sizeof (*digits), "%2zu", i);
			const unsigned int length = song->length;
			if (length > 0) {
				snprintf (duration, sizeof (duration), "%02u:%02u",
						length / 60, length % 60);
			}

			BarUiCustomFormat (outstr, sizeof (outstr), settings->listSongFormat,
					"iatrd@s", vals);
			BarUiAppendNewline (outstr, sizeof (outstr));
			BarUiMsg (settings, MSG_LIST, "%s", outstr);
		}
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
 */
void BarUiStartEventCmd (const BarSettings_t *settings, const char *type,
		const PianoStation_t *curStation, const PianoSong_t *curSong,
		player_t * const player, PianoStation_t *stations,
		PianoReturn_t pRet, CURLcode wRet) {
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

		if (curSong != NULL && stations != NULL && curStation != NULL &&
				curStation->isQuickMix) {
			songStation = PianoFindStationById (stations, curSong->stationId);
		}

		pthread_mutex_lock (&player->lock);
		const unsigned int songDuration = player->songDuration;
		const unsigned int songPlayed = player->songPlayed;
		pthread_mutex_unlock (&player->lock);

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
				"songDuration=%u\n"
				"songPlayed=%u\n"
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
				curl_easy_strerror (wRet),
				songDuration,
				songPlayed,
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

/*	prepend song to history
 */
void BarUiHistoryPrepend (BarApp_t *app, PianoSong_t *song) {
	assert (app != NULL);
	assert (song != NULL);
	/* make sure it's a single song */
	assert (PianoListNextP (song) == NULL);

	if (app->settings.history != 0) {
		app->songHistory = PianoListPrependP (app->songHistory, song);
		PianoSong_t *del;
		do {
			del = PianoListGetP (app->songHistory, app->settings.history);
			if (del != NULL) {
				app->songHistory = PianoListDeleteP (app->songHistory, del);
				PianoDestroyPlaylist (del);
			} else {
				break;
			}
		} while (true);
	} else {
		PianoDestroyPlaylist (song);
	}
}

