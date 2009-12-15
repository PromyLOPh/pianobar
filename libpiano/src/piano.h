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

#ifndef _PIANO_H
#define _PIANO_H

/* this is our public API; don't expect this api to be stable as long as
 * pandora does not provide a stable api
 * all strings _must_ be utf-8 encoded. i won't care, but pandora does. so
 * be nice and check the encoding of your strings. thanks :) */

#include <waitress.h>

typedef struct PianoUserInfo {
	char *webAuthToken;
	char *listenerId;
	char *authToken;
} PianoUserInfo_t;

typedef struct PianoStation {
	char isCreator;
	char isQuickMix;
	char useQuickMix; /* station will be included in quickmix */
	char *name;
	char *id;
	struct PianoStation *next;
} PianoStation_t;

typedef enum {PIANO_RATE_BAN, PIANO_RATE_LOVE, PIANO_RATE_NONE}
		PianoSongRating_t;

/* UNKNOWN should be 0, because memset sets audio format to 0 */
typedef enum {PIANO_AF_UNKNOWN = 0, PIANO_AF_AACPLUS, PIANO_AF_MP3,
		PIANO_AF_MP3_HI} PianoAudioFormat_t;

typedef struct PianoSong {
	char *artist;
	char *matchingSeed;
	float fileGain;
	PianoSongRating_t rating;
	char *stationId;
	char *album;
	char *userSeed;
	char *audioUrl;
	char *musicId;
	char *title;
	char *focusTraitId;
	char *identity;
	PianoAudioFormat_t audioFormat;
	struct PianoSong *next;
} PianoSong_t;

/* currently only used for search results */
typedef struct PianoArtist {
	char *name;
	char *musicId;
	int score;
	struct PianoArtist *next;
} PianoArtist_t;

typedef struct PianoGenreCategory {
	char *name;
	PianoStation_t *stations;
	struct PianoGenreCategory *next;
} PianoGenreCategory_t;

typedef struct PianoHandle {
	WaitressHandle_t waith;
	char routeId[9];
	PianoUserInfo_t user;
	/* linked lists */
	PianoStation_t *stations;
	PianoGenreCategory_t *genreStations;
} PianoHandle_t;

typedef struct PianoSearchResult {
	PianoSong_t *songs;
	PianoArtist_t *artists;
} PianoSearchResult_t;

typedef enum {PIANO_RET_OK, PIANO_RET_ERR, PIANO_RET_XML_INVALID,
		PIANO_RET_AUTH_TOKEN_INVALID, PIANO_RET_AUTH_USER_PASSWORD_INVALID,
		PIANO_RET_NET_ERROR, PIANO_RET_NOT_AUTHORIZED,
		PIANO_RET_PROTOCOL_INCOMPATIBLE, PIANO_RET_READONLY_MODE,
		PIANO_RET_STATION_CODE_INVALID, PIANO_RET_IP_REJECTED,
		PIANO_RET_STATION_NONEXISTENT, PIANO_RET_OUT_OF_MEMORY,
		PIANO_RET_OUT_OF_SYNC} PianoReturn_t;

void PianoInit (PianoHandle_t *);
void PianoDestroy (PianoHandle_t *);
void PianoDestroyPlaylist (PianoSong_t *);
void PianoDestroySearchResult (PianoSearchResult_t *);
PianoReturn_t PianoConnect (PianoHandle_t *, const char *, const char *);

PianoReturn_t PianoGetStations (PianoHandle_t *);
PianoReturn_t PianoGetPlaylist (PianoHandle_t *, const char *,
		PianoAudioFormat_t, PianoSong_t **);

PianoReturn_t PianoRateTrack (PianoHandle_t *, PianoSong_t *,
		PianoSongRating_t);
PianoReturn_t PianoMoveSong (PianoHandle_t *, const PianoStation_t *,
		const PianoStation_t *, const PianoSong_t *);
PianoReturn_t PianoRenameStation (PianoHandle_t *, PianoStation_t *,
		const char *);
PianoReturn_t PianoDeleteStation (PianoHandle_t *, PianoStation_t *);
PianoReturn_t PianoSearchMusic (PianoHandle_t *, const char *,
		PianoSearchResult_t *);
PianoReturn_t PianoCreateStation (PianoHandle_t *, const char *,
		const char *);
PianoReturn_t PianoStationAddMusic (PianoHandle_t *, PianoStation_t *,
		const char *);
PianoReturn_t PianoSongTired (PianoHandle_t *, const PianoSong_t *);
PianoReturn_t PianoSetQuickmix (PianoHandle_t *);
PianoStation_t *PianoFindStationById (PianoStation_t *, const char *);
PianoReturn_t PianoGetGenreStations (PianoHandle_t *);
PianoReturn_t PianoTransformShared (PianoHandle_t *, PianoStation_t *);
PianoReturn_t PianoExplain (PianoHandle_t *, const PianoSong_t *, char **);
const char *PianoErrorToStr (PianoReturn_t);
PianoReturn_t PianoSeedSuggestions (PianoHandle_t *, const char *,
		unsigned int, PianoSearchResult_t *);

#endif /* _PIANO_H */
