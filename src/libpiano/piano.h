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

#ifndef _PIANO_H
#define _PIANO_H

/* this is our public API; don't expect this api to be stable as long as
 * pandora does not provide a stable api
 * all strings _must_ be utf-8 encoded. i won't care, but pandora does. so
 * be nice and check the encoding of your strings. thanks :) */

#define PIANO_RPC_HOST "www.pandora.com"
#define PIANO_RPC_PORT "80"

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
	char *seedId;
	struct PianoStation *next;
} PianoStation_t;

typedef enum {
	PIANO_RATE_NONE = 0,
	PIANO_RATE_LOVE = 1,
	PIANO_RATE_BAN = 2
} PianoSongRating_t;

/* UNKNOWN should be 0, because memset sets audio format to 0 */
typedef enum {
	PIANO_AF_UNKNOWN = 0,
	PIANO_AF_AACPLUS = 1,
	PIANO_AF_MP3 = 2,
	PIANO_AF_MP3_HI = 3
} PianoAudioFormat_t;

typedef struct PianoSong {
	char *artist;
	char *artistMusicId;
	char *stationId;
	char *album;
	char *userSeed;
	char *audioUrl;
	char *coverArt;
	char *musicId;
	char *title;
	char *seedId;
	char *feedbackId;
	char *detailUrl;
	char *trackToken;
	float fileGain;
	PianoSongRating_t rating;
	PianoAudioFormat_t audioFormat;
	struct PianoSong *next;
} PianoSong_t;

/* currently only used for search results */
typedef struct PianoArtist {
	char *name;
	char *musicId;
	char *seedId;
	int score;
	struct PianoArtist *next;
} PianoArtist_t;

typedef struct PianoGenre {
	char *name;
	char *musicId;
	struct PianoGenre *next;
} PianoGenre_t;

typedef struct PianoGenreCategory {
	char *name;
	PianoGenre_t *genres;
	struct PianoGenreCategory *next;
} PianoGenreCategory_t;

typedef struct PianoHandle {
	char routeId[9];
	PianoUserInfo_t user;
	/* linked lists */
	PianoStation_t *stations;
	PianoGenreCategory_t *genreStations;
	int timeOffset;
} PianoHandle_t;

typedef struct PianoSearchResult {
	PianoSong_t *songs;
	PianoArtist_t *artists;
} PianoSearchResult_t;

typedef struct {
	PianoSong_t *songSeeds;
	PianoArtist_t *artistSeeds;
	PianoStation_t *stationSeeds;
	PianoSong_t *feedback;
} PianoStationInfo_t;

typedef enum {
	/* 0 is reserved: memset (x, 0, sizeof (x)) */
	PIANO_REQUEST_LOGIN = 1,
	PIANO_REQUEST_GET_STATIONS = 2,
	PIANO_REQUEST_GET_PLAYLIST = 3,
	PIANO_REQUEST_RATE_SONG = 4,
	PIANO_REQUEST_ADD_FEEDBACK = 5,
	PIANO_REQUEST_MOVE_SONG = 6,
	PIANO_REQUEST_RENAME_STATION = 7,
	PIANO_REQUEST_DELETE_STATION = 8,
	PIANO_REQUEST_SEARCH = 9,
	PIANO_REQUEST_CREATE_STATION = 10,
	PIANO_REQUEST_ADD_SEED = 11,
	PIANO_REQUEST_ADD_TIRED_SONG = 12,
	PIANO_REQUEST_SET_QUICKMIX = 13,
	PIANO_REQUEST_GET_GENRE_STATIONS = 14,
	PIANO_REQUEST_TRANSFORM_STATION = 15,
	PIANO_REQUEST_EXPLAIN = 16,
	PIANO_REQUEST_GET_SEED_SUGGESTIONS = 17,
	PIANO_REQUEST_BOOKMARK_SONG = 18,
	PIANO_REQUEST_BOOKMARK_ARTIST = 19,
	PIANO_REQUEST_GET_STATION_INFO = 20,
	PIANO_REQUEST_DELETE_FEEDBACK = 21,
	PIANO_REQUEST_DELETE_SEED = 22,
} PianoRequestType_t;

typedef struct PianoRequest {
	PianoRequestType_t type;
	void *data;
	char urlPath[1024];
	char *postData;
	char *responseData;
} PianoRequest_t;

/* request data structures */
typedef struct {
	char *user;
	char *password;
	unsigned char step;
} PianoRequestDataLogin_t;

typedef struct {
	PianoStation_t *station;
	PianoAudioFormat_t format;
	PianoSong_t *retPlaylist;
} PianoRequestDataGetPlaylist_t;

typedef struct {
	PianoSong_t *song;
	PianoSongRating_t rating;
} PianoRequestDataRateSong_t;

typedef struct {
	char *stationId;
	char *trackToken;
	PianoSongRating_t rating;
} PianoRequestDataAddFeedback_t;

typedef struct {
	PianoSong_t *song;
	PianoStation_t *from;
	PianoStation_t *to;
	unsigned short step;
} PianoRequestDataMoveSong_t;

typedef struct {
	PianoStation_t *station;
	char *newName;
} PianoRequestDataRenameStation_t;

typedef struct {
	char *searchStr;
	PianoSearchResult_t searchResult;
} PianoRequestDataSearch_t;

typedef struct {
	char *type;
	char *id;
} PianoRequestDataCreateStation_t;

typedef struct {
	PianoStation_t *station;
	char *musicId;
} PianoRequestDataAddSeed_t;

typedef struct {
	PianoSong_t *song;
	char *retExplain;
} PianoRequestDataExplain_t;

typedef struct {
	PianoStation_t *station;
	char *musicId;
	unsigned short max;
	PianoSearchResult_t searchResult;
} PianoRequestDataGetSeedSuggestions_t;

typedef struct {
	PianoStation_t *station;
	PianoStationInfo_t info;
} PianoRequestDataGetStationInfo_t;

typedef struct {
	PianoSong_t *song;
	PianoArtist_t *artist;
	PianoStation_t *station;
} PianoRequestDataDeleteSeed_t;

typedef enum {
	PIANO_RET_ERR = 0,
	PIANO_RET_OK = 1,
	PIANO_RET_XML_INVALID = 2,
	PIANO_RET_AUTH_TOKEN_INVALID = 3,
	PIANO_RET_AUTH_USER_PASSWORD_INVALID = 4,
	PIANO_RET_CONTINUE_REQUEST = 5,
	PIANO_RET_NOT_AUTHORIZED = 6,
	PIANO_RET_PROTOCOL_INCOMPATIBLE = 7,
	PIANO_RET_READONLY_MODE = 8,
	PIANO_RET_STATION_CODE_INVALID = 9,
	PIANO_RET_IP_REJECTED = 10,
	PIANO_RET_STATION_NONEXISTENT = 11,
	PIANO_RET_OUT_OF_MEMORY = 12,
	PIANO_RET_OUT_OF_SYNC = 13,
	PIANO_RET_PLAYLIST_END = 14,
	PIANO_RET_QUICKMIX_NOT_PLAYABLE = 15,
	PIANO_RET_REMOVING_TOO_MANY_SEEDS = 16,
} PianoReturn_t;

void PianoInit (PianoHandle_t *);
void PianoDestroy (PianoHandle_t *);
void PianoDestroyPlaylist (PianoSong_t *);
void PianoDestroySearchResult (PianoSearchResult_t *);
void PianoDestroyStationInfo (PianoStationInfo_t *);

PianoReturn_t PianoRequest (PianoHandle_t *, PianoRequest_t *,
		PianoRequestType_t);
PianoReturn_t PianoResponse (PianoHandle_t *, PianoRequest_t *);
void PianoDestroyRequest (PianoRequest_t *);

PianoStation_t *PianoFindStationById (PianoStation_t *, const char *);
const char *PianoErrorToStr (PianoReturn_t);

#endif /* _PIANO_H */
