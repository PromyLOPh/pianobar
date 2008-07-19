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

#ifndef _PIANO_H
#define _PIANO_H

/* this is our public API; don't expect this api to be stable as long as
 * pandora does not provide a stable api
 * all strings _must_ be utf-8 encoded. i won't care, but pandora does. so
 * be nice and check the encoding of your strings. thanks :) */

#include <curl/curl.h>

struct PianoUserInfo {
	//unsigned int zipcode;
	/* disabled: billingFrequency */
	//char hasExplicitContentFilter;
	char *webAuthToken;
	/* disabled: alertCode */
	/* disabled: seenQuickMixPanel */
	//unsigned short birthYear;
	//char *bookmarkUrl;
	//char *listenerState; /* FIXME: use enum? */
	/* disabled: adCookieValue */
	/* disabled: gender */
	/* disabled: emailOptIn */
	/* disabled: autoRenew */
	//char *username;
	char *listenerId;
	char *authToken;
	//char *webName;
};

typedef struct PianoUserInfo PianoUserInfo_t;

struct PianoStation {
	char isCreator;
	/* disabled: originalStationId */
	//char **genre;
	//unsigned int originalCreatorId;
	/* disabled: initialSeed */
	/* disabled: isNew */
	/* disabled: transformType */
	//char *idToken;
	char isQuickMix;
	char useQuickMix; /* station will be included in quickmix */
	char *name;
	char *id;
	struct PianoStation *next;
};
typedef struct PianoStation PianoStation_t;

enum PianoSongRating {PIANO_RATE_BAN, PIANO_RATE_LOVE, PIANO_RATE_NONE};
typedef enum PianoSongRating PianoSongRating_t;

struct PianoSong {
	char *artist;
	//char **genre;
	char *matchingSeed;
	/* disabled: composerName */
	/* disabled: isSeed */
	/* disabled: artistFansURL */
	/* disabled: songExplorerUrl */
	//float fileGain;
	/* disabled: songDetailURL */
	/* disabled: albumDetailURL */
	//char *webId;
	/* disabled: musicComUrl */
	/* disabled: fanExplorerUrl */
	PianoSongRating_t rating;
	/* disabled: artistExplorerUrl */
	/* disabled: artRadio */
	//char *audioEncoding; /* FIXME: should be enum: mp3 or aacplus */
	char *stationId;
	char *album;
	//char *artistMusicId;
	char *userSeed;
	/* disabled: albumExplorerUrl */
	/* disabled: amazonUrl */
	char *audioUrl;
	//char onTour;
	/* disabled: itunesUrl */
	char *musicId;
	char *title;
	char *focusTraitId;
	char *identity;
	//int score; /* only used for search results */
	struct PianoSong *next;
};

typedef struct PianoSong PianoSong_t;

/* currently only used for search results */
struct PianoArtist {
	/* disabled: iscomposer */
	/* disabled: likelymatch */
	char *name;
	char *musicId;
	int score;
	struct PianoArtist *next;
};

typedef struct PianoArtist PianoArtist_t;


struct PianoGenreCategory {
	char *name;
	PianoStation_t *stations;
	struct PianoGenreCategory *next;
};

typedef struct PianoGenreCategory PianoGenreCategory_t;

struct PianoHandle {
	CURL *curlHandle;
	char routeId[9];
	PianoUserInfo_t user;
	/* linked lists */
	PianoStation_t *stations;
	PianoSong_t *playlist;
	PianoGenreCategory_t *genreStations;
};

typedef struct PianoHandle PianoHandle_t;

struct PianoSearchResult {
	PianoSong_t *songs;
	PianoArtist_t *artists;
};

typedef struct PianoSearchResult PianoSearchResult_t;

/* FIXME: more error types (http failed, e.g.) later */
enum PianoReturn {PIANO_RET_OK, PIANO_RET_ERR, PIANO_RET_XML_INVALID,
		PIANO_RET_AUTH_TOKEN_INVALID, PIANO_RET_AUTH_USER_PASSWORD_INVALID,
		PIANO_RET_NET_ERROR};
typedef enum PianoReturn PianoReturn_t;

void PianoInit (PianoHandle_t *);
void PianoDestroy (PianoHandle_t *);
void PianoDestroyPlaylist (PianoHandle_t *ph);
void PianoDestroySearchResult (PianoSearchResult_t *searchResult);
PianoReturn_t PianoConnect (PianoHandle_t *ph, char *user, char *password,
		char secureLogin);

PianoReturn_t PianoGetStations (PianoHandle_t *ph);
PianoReturn_t PianoGetPlaylist (PianoHandle_t *ph, char *stationId);

PianoReturn_t PianoRateTrack (PianoHandle_t *ph, PianoSong_t *song,
		PianoSongRating_t rating);
PianoReturn_t PianoMoveSong (PianoHandle_t *ph, PianoStation_t *stationFrom,
		PianoStation_t *stationTo, PianoSong_t *song);
PianoReturn_t PianoRenameStation (PianoHandle_t *ph, PianoStation_t *station,
		char *newName);
PianoReturn_t PianoDeleteStation (PianoHandle_t *ph, PianoStation_t *station);
PianoReturn_t PianoSearchMusic (PianoHandle_t *ph, char *searchStr,
		PianoSearchResult_t *searchResult);
PianoReturn_t PianoCreateStation (PianoHandle_t *ph, char *type,
		char *id);
PianoReturn_t PianoStationAddMusic (PianoHandle_t *ph,
		PianoStation_t *station, char *musicId);
PianoReturn_t PianoSongTired (PianoHandle_t *ph, PianoSong_t *song);
PianoReturn_t PianoSetQuickmix (PianoHandle_t *ph);
PianoStation_t *PianoFindStationById (PianoStation_t *stations,
		char *searchStation);
PianoReturn_t PianoGetGenreStations (PianoHandle_t *ph);
PianoReturn_t PianoTransformShared (PianoHandle_t *ph,
		PianoStation_t *station);

#endif /* _PIANO_H */
