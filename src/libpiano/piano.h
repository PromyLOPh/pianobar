/*
Copyright (c) 2008-2013
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

#pragma once

#include "../config.h"

#include <stdbool.h>
#ifdef __FreeBSD__
#define _GCRYPT_IN_LIBGCRYPT
#endif
#include <gcrypt.h>

/* this is our public API; don't expect this api to be stable as long as
 * pandora does not provide a stable api
 * all strings _must_ be utf-8 encoded. i won't care, but pandora does. so
 * be nice and check the encoding of your strings. thanks :) */

/* Pandora API documentation is available at
 * http://6xq.net/playground/pandora-apidoc/
 */

#define PIANO_RPC_HOST "tuner.pandora.com"
#define PIANO_RPC_PATH "/services/json/?"

typedef struct PianoListHead {
	struct PianoListHead *next;
} PianoListHead_t;

typedef struct PianoUserInfo {
	char *listenerId;
	char *authToken;
} PianoUserInfo_t;

typedef struct PianoStation {
	PianoListHead_t head;
	char isCreator;
	char isQuickMix;
	char useQuickMix; /* station will be included in quickmix */
	char *name;
	char *id;
	char *seedId;
} PianoStation_t;

typedef enum {
	PIANO_RATE_NONE = 0,
	PIANO_RATE_LOVE = 1,
	PIANO_RATE_BAN = 2,
	PIANO_RATE_TIRED = 3,
} PianoSongRating_t;

/* UNKNOWN should be 0, because memset sets audio format to 0 */
typedef enum {
	PIANO_AF_UNKNOWN = 0,
	PIANO_AF_AACPLUS = 1,
	PIANO_AF_MP3 = 2,
} PianoAudioFormat_t;

typedef enum {
	PIANO_AQ_UNKNOWN = 0,
	PIANO_AQ_LOW = 1,
	PIANO_AQ_MEDIUM = 2,
	PIANO_AQ_HIGH = 3,
} PianoAudioQuality_t;

typedef struct PianoSong {
	PianoListHead_t head;
	char *artist;
	char *stationId;
	char *album;
	char *audioUrl;
	char *coverArt;
	char *musicId;
	char *title;
	char *seedId;
	char *feedbackId;
	char *detailUrl;
	char *trackToken;
	float fileGain;
	unsigned int length; /* song length in seconds */
	PianoSongRating_t rating;
	PianoAudioFormat_t audioFormat;
} PianoSong_t;

/* currently only used for search results */
typedef struct PianoArtist {
	PianoListHead_t head;
	char *name;
	char *musicId;
	char *seedId;
	int score;
} PianoArtist_t;

typedef struct PianoGenre {
	PianoListHead_t head;
	char *name;
	char *musicId;
} PianoGenre_t;

typedef struct PianoGenreCategory {
	PianoListHead_t head;
	char *name;
	PianoGenre_t *genres;
} PianoGenreCategory_t;

typedef struct PianoPartner {
	gcry_cipher_hd_t in, out;
	char *authToken, *device, *user, *password;
	unsigned int id;
} PianoPartner_t;

typedef struct PianoHandle {
	PianoUserInfo_t user;
	/* linked lists */
	PianoStation_t *stations;
	PianoGenreCategory_t *genreStations;
	PianoPartner_t partner;
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

typedef struct {
	char *username;
	bool explicitContentFilter;
} PianoSettings_t;

typedef enum {
	/* 0 is reserved: memset (x, 0, sizeof (x)) */
	PIANO_REQUEST_LOGIN = 1,
	PIANO_REQUEST_GET_STATIONS = 2,
	PIANO_REQUEST_GET_PLAYLIST = 3,
	PIANO_REQUEST_RATE_SONG = 4,
	PIANO_REQUEST_ADD_FEEDBACK = 5,
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
	PIANO_REQUEST_BOOKMARK_SONG = 18,
	PIANO_REQUEST_BOOKMARK_ARTIST = 19,
	PIANO_REQUEST_GET_STATION_INFO = 20,
	PIANO_REQUEST_DELETE_FEEDBACK = 21,
	PIANO_REQUEST_DELETE_SEED = 22,
	PIANO_REQUEST_GET_SETTINGS = 23,
	PIANO_REQUEST_CHANGE_SETTINGS = 24,
} PianoRequestType_t;

typedef struct PianoRequest {
	PianoRequestType_t type;
	bool secure;
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
	PianoAudioQuality_t quality;
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
	PianoStation_t *station;
	char *newName;
} PianoRequestDataRenameStation_t;

typedef struct {
	char *searchStr;
	PianoSearchResult_t searchResult;
} PianoRequestDataSearch_t;

typedef struct {
	char *token;
	enum {
		PIANO_MUSICTYPE_INVALID = 0,
		PIANO_MUSICTYPE_SONG,
		PIANO_MUSICTYPE_ARTIST,
	} type;
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
	PianoStationInfo_t info;
} PianoRequestDataGetStationInfo_t;

typedef struct {
	PianoSong_t *song;
	PianoArtist_t *artist;
	PianoStation_t *station;
} PianoRequestDataDeleteSeed_t;

typedef enum {
	PIANO_UNDEFINED = 0,
	PIANO_FALSE = 1,
	PIANO_TRUE = 2,
} PianoTristate_t;

typedef struct {
	char *currentUsername, *newUsername;
	char *currentPassword, *newPassword;
	PianoTristate_t explicitContentFilter;
} PianoRequestDataChangeSettings_t;

/* pandora error code offset */
#define PIANO_RET_OFFSET 1024
typedef enum {
	PIANO_RET_ERR = 0,
	PIANO_RET_OK = 1,
	PIANO_RET_INVALID_RESPONSE = 2,
	PIANO_RET_CONTINUE_REQUEST = 3,
	PIANO_RET_OUT_OF_MEMORY = 4,
	PIANO_RET_INVALID_LOGIN = 5,
	PIANO_RET_QUALITY_UNAVAILABLE = 6,
	PIANO_RET_GCRY_ERR = 7,

	/* pandora error codes */
	PIANO_RET_P_INTERNAL = PIANO_RET_OFFSET+0,
	PIANO_RET_P_API_VERSION_NOT_SUPPORTED = PIANO_RET_OFFSET+11,
	PIANO_RET_P_BIRTH_YEAR_INVALID = PIANO_RET_OFFSET+1025,
	PIANO_RET_P_BIRTH_YEAR_TOO_YOUNG = PIANO_RET_OFFSET+1026,
	PIANO_RET_P_CALL_NOT_ALLOWED = PIANO_RET_OFFSET+1008,
	PIANO_RET_P_CERTIFICATE_REQUIRED = PIANO_RET_OFFSET+7,
	PIANO_RET_P_COMPLIMENTARY_PERIOD_ALREADY_IN_USE = PIANO_RET_OFFSET+1007,
	PIANO_RET_P_DAILY_TRIAL_LIMIT_REACHED = PIANO_RET_OFFSET+1035,
	PIANO_RET_P_DEVICE_ALREADY_ASSOCIATED_TO_ACCOUNT = PIANO_RET_OFFSET+1014,
	PIANO_RET_P_DEVICE_DISABLED = PIANO_RET_OFFSET+1034,
	PIANO_RET_P_DEVICE_MODEL_INVALID = PIANO_RET_OFFSET+1023,
	PIANO_RET_P_DEVICE_NOT_FOUND = PIANO_RET_OFFSET+1009,
	PIANO_RET_P_EXPLICIT_PIN_INCORRECT = PIANO_RET_OFFSET+1018,
	PIANO_RET_P_EXPLICIT_PIN_MALFORMED = PIANO_RET_OFFSET+1020,
	PIANO_RET_P_INSUFFICIENT_CONNECTIVITY = PIANO_RET_OFFSET+13,
	PIANO_RET_P_INVALID_AUTH_TOKEN = PIANO_RET_OFFSET+1001,
	PIANO_RET_P_INVALID_COUNTRY_CODE = PIANO_RET_OFFSET+1027,
	PIANO_RET_P_INVALID_GENDER = PIANO_RET_OFFSET+1027,
	PIANO_RET_P_INVALID_PARTNER_LOGIN = PIANO_RET_OFFSET+1002,
	PIANO_RET_P_INVALID_PASSWORD = PIANO_RET_OFFSET+1012,
	PIANO_RET_P_INVALID_SPONSOR = PIANO_RET_OFFSET+1036,
	PIANO_RET_P_INVALID_USERNAME = PIANO_RET_OFFSET+1011,
	PIANO_RET_P_LICENSING_RESTRICTIONS = PIANO_RET_OFFSET+12,
	PIANO_RET_P_MAINTENANCE_MODE = PIANO_RET_OFFSET+1,
	PIANO_RET_P_MAX_STATIONS_REACHED = PIANO_RET_OFFSET+1005,
	PIANO_RET_P_PARAMETER_MISSING = PIANO_RET_OFFSET+9,
	PIANO_RET_P_PARAMETER_TYPE_MISMATCH = PIANO_RET_OFFSET+8,
	PIANO_RET_P_PARAMETER_VALUE_INVALID = PIANO_RET_OFFSET+10,
	PIANO_RET_P_PARTNER_NOT_AUTHORIZED = PIANO_RET_OFFSET+1010,
	PIANO_RET_P_READ_ONLY_MODE = PIANO_RET_OFFSET+1000,
	PIANO_RET_P_SECURE_PROTOCOL_REQUIRED = PIANO_RET_OFFSET+6,
	PIANO_RET_P_STATION_DOES_NOT_EXIST = PIANO_RET_OFFSET+1006,
	PIANO_RET_P_UPGRADE_DEVICE_MODEL_INVALID = PIANO_RET_OFFSET+1015,
	PIANO_RET_P_URL_PARAM_MISSING_AUTH_TOKEN = PIANO_RET_OFFSET+3,
	PIANO_RET_P_URL_PARAM_MISSING_METHOD = PIANO_RET_OFFSET+2,
	PIANO_RET_P_URL_PARAM_MISSING_PARTNER_ID = PIANO_RET_OFFSET+4,
	PIANO_RET_P_URL_PARAM_MISSING_USER_ID = PIANO_RET_OFFSET+5,
	PIANO_RET_P_USERNAME_ALREADY_EXISTS = PIANO_RET_OFFSET+1013,
	PIANO_RET_P_USER_ALREADY_USED_TRIAL = PIANO_RET_OFFSET+1037,
	PIANO_RET_P_LISTENER_NOT_AUTHORIZED = PIANO_RET_OFFSET+1003,
	PIANO_RET_P_USER_NOT_AUTHORIZED = PIANO_RET_OFFSET+1004,
	PIANO_RET_P_ZIP_CODE_INVALID = PIANO_RET_OFFSET+1024,
	PIANO_RET_P_RATE_LIMIT = PIANO_RET_OFFSET+1039,
} PianoReturn_t;

/* list stuff */
#ifndef __GNUC__
#  define __attribute__(x)
#endif
size_t PianoListCount (const PianoListHead_t * const l);
#define PianoListCountP(l) PianoListCount(&(l)->head)
void *PianoListAppend (PianoListHead_t * const l, PianoListHead_t * const e)
		__attribute__ ((warn_unused_result));
#define PianoListAppendP(l,e) PianoListAppend(((l) == NULL) ? NULL : &(l)->head, \
		&(e)->head)
void *PianoListDelete (PianoListHead_t * const l, PianoListHead_t * const e)
		__attribute__ ((warn_unused_result));
#define PianoListDeleteP(l,e) PianoListDelete(((l) == NULL) ? NULL : &(l)->head, \
		&(e)->head)
#define PianoListNextP(e) ((e) == NULL ? NULL : (void *) (e)->head.next)
void *PianoListPrepend (PianoListHead_t * const l, PianoListHead_t * const e)
		__attribute__ ((warn_unused_result));
#define PianoListPrependP(l,e) PianoListPrepend (((l) == NULL) ? NULL : &(l)->head, \
		&(e)->head)
void *PianoListGet (PianoListHead_t * const l, const size_t n);
#define PianoListGetP(l,n) PianoListGet (&(l)->head, n)
#define PianoListForeachP(l) for (; (l) != NULL; (l) = (void *) (l)->head.next)

/* memory management */
PianoReturn_t PianoInit (PianoHandle_t *, const char *,
		const char *, const char *, const char *,
		const char *);
void PianoDestroy (PianoHandle_t *);
void PianoDestroyPlaylist (PianoSong_t *);
void PianoDestroySearchResult (PianoSearchResult_t *);
void PianoDestroyStationInfo (PianoStationInfo_t *);

/* pandora rpc */
PianoReturn_t PianoRequest (PianoHandle_t *, PianoRequest_t *,
		PianoRequestType_t);
PianoReturn_t PianoResponse (PianoHandle_t *, PianoRequest_t *);
void PianoDestroyRequest (PianoRequest_t *);

/* misc */
PianoStation_t *PianoFindStationById (PianoStation_t * const,
		const char * const);
const char *PianoErrorToStr (PianoReturn_t);

