/*
Copyright (c) 2008-2012
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

#ifndef __FreeBSD__
#define _BSD_SOURCE /* required by strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include <json.h>

/* needed for urlencode */
#include <waitress.h>

#include "piano_private.h"
#include "piano.h"
#include "crypt.h"
#include "config.h"

#define PIANO_RPC_PATH "/services/json/?"
#define PIANO_SEND_BUFFER_SIZE 10000

/*	initialize piano handle
 *	@param piano handle
 *	@return nothing
 */
void PianoInit (PianoHandle_t *ph) {
	memset (ph, 0, sizeof (*ph));
}

/*	destroy artist linked list
 */
static void PianoDestroyArtists (PianoArtist_t *artists) {
	PianoArtist_t *curArtist, *lastArtist;

	curArtist = artists;
	while (curArtist != NULL) {
		free (curArtist->name);
		free (curArtist->musicId);
		free (curArtist->seedId);
		lastArtist = curArtist;
		curArtist = curArtist->next;
		free (lastArtist);
	}
}

/*	free complete search result
 *	@public yes
 *	@param search result
 */
void PianoDestroySearchResult (PianoSearchResult_t *searchResult) {
	PianoDestroyArtists (searchResult->artists);
	PianoDestroyPlaylist (searchResult->songs);
}

/*	free single station
 *	@param station
 */
void PianoDestroyStation (PianoStation_t *station) {
	free (station->name);
	free (station->id);
	free (station->seedId);
	memset (station, 0, sizeof (*station));
}

/*	free complete station list
 *	@param piano handle
 */
static void PianoDestroyStations (PianoStation_t *stations) {
	PianoStation_t *curStation, *lastStation;

	curStation = stations;
	while (curStation != NULL) {
		lastStation = curStation;
		curStation = curStation->next;
		PianoDestroyStation (lastStation);
		free (lastStation);
	}
}

/* FIXME: copy & waste */
/*	free _all_ elements of playlist
 *	@param piano handle
 *	@return nothing
 */
void PianoDestroyPlaylist (PianoSong_t *playlist) {
	PianoSong_t *curSong, *lastSong;

	curSong = playlist;
	while (curSong != NULL) {
		free (curSong->audioUrl);
		free (curSong->coverArt);
		free (curSong->artist);
		free (curSong->musicId);
		free (curSong->title);
		free (curSong->stationId);
		free (curSong->album);
		free (curSong->feedbackId);
		free (curSong->seedId);
		free (curSong->detailUrl);
		free (curSong->trackToken);
		lastSong = curSong;
		curSong = curSong->next;
		free (lastSong);
	}
}

void PianoDestroyStationInfo (PianoStationInfo_t *info) {
	PianoDestroyPlaylist (info->feedback);
	PianoDestroyPlaylist (info->songSeeds);
	PianoDestroyArtists (info->artistSeeds);
	PianoDestroyStations (info->stationSeeds);
}

/*	destroy genre linked list
 */
static void PianoDestroyGenres (PianoGenre_t *genres) {
	PianoGenre_t *curGenre, *lastGenre;

	curGenre = genres;
	while (curGenre != NULL) {
		free (curGenre->name);
		free (curGenre->musicId);
		lastGenre = curGenre;
		curGenre = curGenre->next;
		free (lastGenre);
	}
}

/*	destroy user information
 */
static void PianoDestroyUserInfo (PianoUserInfo_t *user) {
	free (user->authToken);
	free (user->listenerId);
}

/*	frees the whole piano handle structure
 *	@param piano handle
 *	@return nothing
 */
void PianoDestroy (PianoHandle_t *ph) {
	PianoDestroyUserInfo (&ph->user);
	PianoDestroyStations (ph->stations);
	/* destroy genre stations */
	PianoGenreCategory_t *curGenreCat = ph->genreStations, *lastGenreCat;
	while (curGenreCat != NULL) {
		PianoDestroyGenres (curGenreCat->genres);
		free (curGenreCat->name);
		lastGenreCat = curGenreCat;
		curGenreCat = curGenreCat->next;
		free (lastGenreCat);
	}
	free (ph->partnerAuthToken);
	memset (ph, 0, sizeof (*ph));
}

/*	destroy request, free post data. req->responseData is *not* freed here, as
 *	it might be allocated by something else than malloc!
 *	@param piano request
 */
void PianoDestroyRequest (PianoRequest_t *req) {
	free (req->postData);
	memset (req, 0, sizeof (*req));
}

/*	convert audio format id to string that can be used in xml requests
 *	@param format id
 *	@return constant string
 */
static const char *PianoAudioFormatToString (PianoAudioFormat_t format) {
	switch (format) {
		case PIANO_AF_AACPLUS:
			return "aacplus";
			break;

		case PIANO_AF_MP3:
			return "mp3";
			break;

		case PIANO_AF_MP3_HI:
			return "mp3-hifi";
			break;

		default:
			return NULL;
			break;
	}
}

/*	prepare piano request (initializes request type, urlpath and postData)
 *	@param piano handle
 *	@param request structure
 *	@param request type
 */
PianoReturn_t PianoRequest (PianoHandle_t *ph, PianoRequest_t *req,
		PianoRequestType_t type) {
	const char *jsonSendBuf;
	const char *method = NULL;
	json_object *j = json_object_new_object ();
	/* corrected timestamp */
	time_t timestamp = time (NULL) - ph->timeOffset;
	bool encrypted = true;

	assert (ph != NULL);
	assert (req != NULL);

	req->type = type;
	/* no tls by default */
	req->secure = false;

	switch (req->type) {
		case PIANO_REQUEST_LOGIN: {
			/* authenticate user */
			PianoRequestDataLogin_t *logindata = req->data;

			assert (logindata != NULL);

			switch (logindata->step) {
				case 0:
					encrypted = false;
					req->secure = true;

					json_object_object_add (j, "username",
							json_object_new_string ("android"));
					json_object_object_add (j, "password",
							json_object_new_string ("AC7IBG09A3DTSYM4R41UJWL07VLN8JI7"));
					json_object_object_add (j, "deviceModel",
							json_object_new_string ("android-generic"));
					json_object_object_add (j, "version",
							json_object_new_string ("5"));
					json_object_object_add (j, "includeUrls",
							json_object_new_boolean (true));
					snprintf (req->urlPath, sizeof (req->urlPath),
							PIANO_RPC_PATH "method=auth.partnerLogin");
					break;

				case 1: {
					char *urlencAuthToken;

					req->secure = true;

					json_object_object_add (j, "loginType",
							json_object_new_string ("user"));
					json_object_object_add (j, "username",
							json_object_new_string (logindata->user));
					json_object_object_add (j, "password",
							json_object_new_string (logindata->password));
					json_object_object_add (j, "partnerAuthToken",
							json_object_new_string (ph->partnerAuthToken));
					json_object_object_add (j, "syncTime",
							json_object_new_int (timestamp));

					urlencAuthToken = WaitressUrlEncode (ph->partnerAuthToken);
					assert (urlencAuthToken != NULL);
					snprintf (req->urlPath, sizeof (req->urlPath),
							PIANO_RPC_PATH "method=auth.userLogin&"
							"auth_token=%s&partner_id=%i", urlencAuthToken,
							ph->partnerId);
					free (urlencAuthToken);

					break;
				}
			}
			break;
		}

		case PIANO_REQUEST_GET_STATIONS: {
			/* get stations, user must be authenticated */
			assert (ph->user.listenerId != NULL);
			method = "user.getStationList";
			break;
		}

		case PIANO_REQUEST_GET_PLAYLIST: {
			/* get playlist for specified station */
			PianoRequestDataGetPlaylist_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->station->id != NULL);
			assert (reqData->format != PIANO_AF_UNKNOWN);

			req->secure = true;

			json_object_object_add (j, "stationToken",
					json_object_new_string (reqData->station->id));

			method = "station.getPlaylist";
			break;
		}

		case PIANO_REQUEST_ADD_FEEDBACK: {
			/* low-level, don't use directly (see _RATE_SONG and _MOVE_SONG) */
			PianoRequestDataAddFeedback_t *reqData = req->data;
			
			assert (reqData != NULL);
			assert (reqData->trackToken != NULL);
			assert (reqData->rating != PIANO_RATE_NONE);

			json_object_object_add (j, "trackToken",
					json_object_new_string (reqData->trackToken));
			json_object_object_add (j, "isPositive",
					json_object_new_boolean (reqData->rating == PIANO_RATE_LOVE));

			method = "station.addFeedback";
			break;
		}

		case PIANO_REQUEST_RENAME_STATION: {
			PianoRequestDataRenameStation_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->newName != NULL);

			json_object_object_add (j, "stationToken",
					json_object_new_string (reqData->station->id));
			json_object_object_add (j, "stationName",
					json_object_new_string (reqData->newName));

			method = "station.renameStation";
			break;
		}

		case PIANO_REQUEST_DELETE_STATION: {
			/* delete station */
			PianoStation_t *station = req->data;

			assert (station != NULL);
			assert (station->id != NULL);

			json_object_object_add (j, "stationToken",
					json_object_new_string (station->id));

			method = "station.deleteStation";
			break;
		}

		case PIANO_REQUEST_SEARCH: {
			/* search for artist/song title */
			PianoRequestDataSearch_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->searchStr != NULL);

			json_object_object_add (j, "searchText",
					json_object_new_string (reqData->searchStr));

			method = "music.search";
			break;
		}

		case PIANO_REQUEST_CREATE_STATION: {
			/* create new station from specified musicid (type=mi, get one by
			 * performing a search) or shared station id (type=sh) */
			PianoRequestDataCreateStation_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->id != NULL);

			json_object_object_add (j, "musicToken",
					json_object_new_string (reqData->id));

			method = "station.createStation";
			break;
		}

		case PIANO_REQUEST_ADD_SEED: {
			/* add another seed to specified station */
			PianoRequestDataAddSeed_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->musicId != NULL);

			json_object_object_add (j, "musicToken",
					json_object_new_string (reqData->musicId));
			json_object_object_add (j, "stationToken",
					json_object_new_string (reqData->station->id));

			method = "station.addMusic";
			break;
		}

		case PIANO_REQUEST_ADD_TIRED_SONG: {
			/* ban song for a month from all stations */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			json_object_object_add (j, "trackToken",
					json_object_new_string (song->trackToken));

			method = "user.sleepSong";
			break;
		}

		case PIANO_REQUEST_SET_QUICKMIX: {
			/* select stations included in quickmix (see useQuickMix flag of
			 * PianoStation_t) */
			PianoStation_t *curStation = ph->stations;
			json_object *a = json_object_new_array ();

			while (curStation != NULL) {
				/* quick mix can't contain itself */
				if (curStation->useQuickMix && !curStation->isQuickMix) {
					json_object_array_add (a,
							json_object_new_string (curStation->id));
				}

				curStation = curStation->next;
			}

			json_object_object_add (j, "quickMixStationIds", a);

			method = "user.setQuickMix";
			break;
		}

		case PIANO_REQUEST_GET_GENRE_STATIONS: {
			/* receive list of pandora's genre stations */
			method = "station.getGenreStations";
			break;
		}

		case PIANO_REQUEST_TRANSFORM_STATION: {
			/* transform shared station into private */
			PianoStation_t *station = req->data;

			assert (station != NULL);

			json_object_object_add (j, "stationToken",
					json_object_new_string (station->id));

			method = "station.transformSharedStation";
			break;
		}

		case PIANO_REQUEST_EXPLAIN: {
			/* explain why particular song was played */
			PianoRequestDataExplain_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->song != NULL);

			json_object_object_add (j, "trackToken",
					json_object_new_string (reqData->song->trackToken));

			method = "track.explainTrack";
			break;
		}

		case PIANO_REQUEST_GET_SEED_SUGGESTIONS: {
#if 0
			/* find similar artists */
			PianoRequestDataGetSeedSuggestions_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->musicId != NULL);
			assert (reqData->max != 0);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>music.getSeedSuggestions</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					/* seed music id */
					"<param><value><string>%s</string></value></param>"
					/* max */
					"<param><value><int>%u</int></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->station->id, reqData->musicId,
					reqData->max);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=getSeedSuggestions&arg1=%s&arg2=%u",
					ph->routeId, ph->user.listenerId, reqData->musicId, reqData->max);
#endif
			break;
		}

		case PIANO_REQUEST_BOOKMARK_SONG: {
			/* bookmark song */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			json_object_object_add (j, "trackToken",
					json_object_new_string (song->trackToken));

			method = "bookmark.addSongBookmark";
			break;
		}

		case PIANO_REQUEST_BOOKMARK_ARTIST: {
			/* bookmark artist */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			json_object_object_add (j, "trackToken",
					json_object_new_string (song->trackToken));

			method = "bookmark.addArtistBookmark";
			break;
		}

		case PIANO_REQUEST_GET_STATION_INFO: {
			/* get station information (seeds and feedback) */
			PianoRequestDataGetStationInfo_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);

			json_object_object_add (j, "stationToken",
					json_object_new_string (reqData->station->id));
			json_object_object_add (j, "includeExtendedAttributes",
					json_object_new_boolean (true));

			method = "station.getStation";
			break;
		}

		case PIANO_REQUEST_DELETE_FEEDBACK: {
			PianoSong_t *song = req->data;

			assert (song != NULL);

			json_object_object_add (j, "feedbackId",
					json_object_new_string (song->feedbackId));

			method = "station.deleteFeedback";
			break;
		}

		case PIANO_REQUEST_DELETE_SEED: {
			PianoRequestDataDeleteSeed_t *reqData = req->data;
			char *seedId = NULL;

			assert (reqData != NULL);
			assert (reqData->song != NULL || reqData->artist != NULL ||
					reqData->station != NULL);

			if (reqData->song != NULL) {
				seedId = reqData->song->seedId;
			} else if (reqData->artist != NULL) {
				seedId = reqData->artist->seedId;
			} else if (reqData->station != NULL) {
				seedId = reqData->station->seedId;
			}

			assert (seedId != NULL);

			json_object_object_add (j, "seedId",
					json_object_new_string (seedId));

			method = "station.deleteMusic";
			break;
		}

		/* "high-level" wrapper */
		case PIANO_REQUEST_RATE_SONG: {
			/* love/ban song */
			PianoRequestDataRateSong_t *reqData = req->data;
			PianoReturn_t pRet;

			assert (reqData != NULL);
			assert (reqData->song != NULL);
			assert (reqData->rating != PIANO_RATE_NONE);

			PianoRequestDataAddFeedback_t transformedReqData;
			transformedReqData.stationId = reqData->song->stationId;
			transformedReqData.trackToken = reqData->song->trackToken;
			transformedReqData.rating = reqData->rating;
			req->data = &transformedReqData;

			/* create request data (url, post data) */
			pRet = PianoRequest (ph, req, PIANO_REQUEST_ADD_FEEDBACK);
			/* and reset request type/data */
			req->type = PIANO_REQUEST_RATE_SONG;
			req->data = reqData;

			return pRet;
			break;
		}

		case PIANO_REQUEST_MOVE_SONG: {
			/* move song to a different station, needs two requests */
			PianoRequestDataMoveSong_t *reqData = req->data;
			PianoRequestDataAddFeedback_t transformedReqData;
			PianoReturn_t pRet;

			assert (reqData != NULL);
			assert (reqData->song != NULL);
			assert (reqData->from != NULL);
			assert (reqData->to != NULL);
			assert (reqData->step < 2);

			transformedReqData.trackToken = reqData->song->trackToken;
			req->data = &transformedReqData;

			switch (reqData->step) {
				case 0:
					transformedReqData.stationId = reqData->from->id;
					transformedReqData.rating = PIANO_RATE_BAN;
					break;

				case 1:
					transformedReqData.stationId = reqData->to->id;
					transformedReqData.rating = PIANO_RATE_LOVE;
					break;
			}

			/* create request data (url, post data) */
			pRet = PianoRequest (ph, req, PIANO_REQUEST_ADD_FEEDBACK);
			/* and reset request type/data */
			req->type = PIANO_REQUEST_MOVE_SONG;
			req->data = reqData;

			return pRet;
			break;
		}
	}

	/* standard parameter */
	if (method != NULL) {
		char *urlencAuthToken;

		assert (ph->user.authToken != NULL);

		urlencAuthToken = WaitressUrlEncode (ph->user.authToken);
		assert (urlencAuthToken != NULL);

		snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
				"method=%s&auth_token=%s&partner_id=%i&user_id=%s", method,
				urlencAuthToken, ph->partnerId, ph->user.listenerId);

		free (urlencAuthToken);

		json_object_object_add (j, "userAuthToken",
				json_object_new_string (ph->user.authToken));
		json_object_object_add (j, "syncTime",
				json_object_new_int (timestamp));
	}

	/* json to string */
	jsonSendBuf = json_object_to_json_string (j);
	if (encrypted) {
		if ((req->postData = PianoEncryptString (jsonSendBuf)) == NULL) {
			return PIANO_RET_OUT_OF_MEMORY;
		}
	} else {
		req->postData = strdup (jsonSendBuf);
	}
	json_object_put (j);

	return PIANO_RET_OK;
}

static char *PianoJsonStrdup (json_object *j, const char *key) {
	return strdup (json_object_get_string (json_object_object_get (j, key)));
}

static void PianoJsonParseStation (json_object *j, PianoStation_t *s) {
	s->name = PianoJsonStrdup (j, "stationName");
	s->id = PianoJsonStrdup (j, "stationToken");
	s->isCreator = !json_object_get_boolean (json_object_object_get (j,
			"isShared"));
	s->isQuickMix = json_object_get_boolean (json_object_object_get (j,
			"isQuickMix"));
}

/*	parse xml response and update data structures/return new data structure
 *	@param piano handle
 *	@param initialized request (expects responseData to be a NUL-terminated
 *			string)
 */
PianoReturn_t PianoResponse (PianoHandle_t *ph, PianoRequest_t *req) {
	PianoReturn_t ret = PIANO_RET_OK;
	json_object *j, *result, *status;

	assert (ph != NULL);
	assert (req != NULL);

	j = json_tokener_parse (req->responseData);

	status = json_object_object_get (j, "stat");
	if (status == NULL) {
		json_object_put (j);
		return PIANO_RET_INVALID_RESPONSE;
	}

	/* error handling */
	if (strcmp (json_object_get_string (status), "ok") != 0) {
		json_object *code = json_object_object_get (j, "code");
		if (code == NULL) {
			ret = PIANO_RET_INVALID_RESPONSE;
		} else {
			ret = json_object_get_int (code)+PIANO_RET_OFFSET;
		}

		json_object_put (j);
		return ret;
	}

	result = json_object_object_get (j, "result");

	switch (req->type) {
		case PIANO_REQUEST_LOGIN: {
			/* authenticate user */
			PianoRequestDataLogin_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			switch (reqData->step) {
				case 0: {
					/* decrypt timestamp */
					const char *cryptedTimestamp = json_object_get_string (
							json_object_object_get (result, "syncTime"));
					unsigned long timestamp = 0;
					const time_t realTimestamp = time (NULL);
					char *decryptedTimestamp = NULL;
					size_t decryptedSize;

					ret = PIANO_RET_ERR;
					if ((decryptedTimestamp = PianoDecryptString (cryptedTimestamp,
							&decryptedSize)) != NULL && decryptedSize > 4) {
						/* skip four bytes garbage(?) at beginning */
						timestamp = strtoul (decryptedTimestamp+4, NULL, 0);
						ph->timeOffset = realTimestamp - timestamp;
						ret = PIANO_RET_CONTINUE_REQUEST;
					}
					free (decryptedTimestamp);
					/* get auth token */
					ph->partnerAuthToken = PianoJsonStrdup (result,
							"partnerAuthToken");
					ph->partnerId = json_object_get_int (
							json_object_object_get (result, "partnerId"));
					++reqData->step;
					break;
				}

				case 1:
					/* information exists when reauthenticating, destroy to
					 * avoid memleak */
					if (ph->user.listenerId != NULL) {
						PianoDestroyUserInfo (&ph->user);
					}
					ph->user.listenerId = PianoJsonStrdup (result, "userId");
					ph->user.authToken = PianoJsonStrdup (result,
							"userAuthToken");
					break;
			}
			break;
		}

		case PIANO_REQUEST_GET_STATIONS: {
			/* get stations */
			assert (req->responseData != NULL);

			json_object *stations = json_object_object_get (result,
					"stations"), *mix = NULL;

			for (size_t i=0; i < json_object_array_length (stations); i++) {
				PianoStation_t *tmpStation;
				json_object *s = json_object_array_get_idx (stations, i);

				if ((tmpStation = calloc (1, sizeof (*tmpStation))) == NULL) {
					return PIANO_RET_OUT_OF_MEMORY;
				}

				PianoJsonParseStation (s, tmpStation);

				if (tmpStation->isQuickMix) {
					/* fix flags on other stations later */
					mix = json_object_object_get (s, "quickMixStationIds");
				}

				/* start new linked list or append */
				if (ph->stations == NULL) {
					ph->stations = tmpStation;
				} else {
					PianoStation_t *curStation = ph->stations;
					while (curStation->next != NULL) {
						curStation = curStation->next;
					}
					curStation->next = tmpStation;
				}
			}

			/* fix quickmix flags */
			if (mix != NULL) {
				PianoStation_t *curStation = ph->stations;
				while (curStation != NULL) {
					for (size_t i = 0; i < json_object_array_length (mix); i++) {
						json_object *id = json_object_array_get_idx (mix, i);
						if (strcmp (json_object_get_string (id),
								curStation->id) == 0) {
							curStation->useQuickMix = true;
						}
					}
					curStation = curStation->next;
				}
			}
			break;
		}

		case PIANO_REQUEST_GET_PLAYLIST: {
			/* get playlist, usually four songs */
			PianoRequestDataGetPlaylist_t *reqData = req->data;
			PianoSong_t *playlist = NULL;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			json_object *items = json_object_object_get (result, "items");
			assert (items != NULL);

			for (size_t i=0; i < json_object_array_length (items); i++) {
				json_object *s = json_object_array_get_idx (items, i);
				PianoSong_t *song;

				if ((song = calloc (1, sizeof (*song))) == NULL) {
					return PIANO_RET_OUT_OF_MEMORY;
				}

				if (json_object_object_get (s, "artistName") == NULL) {
					free (song);
					continue;
				}
				song->audioUrl = strdup (json_object_get_string (json_object_object_get (json_object_object_get (json_object_object_get (s, "audioUrlMap"), "highQuality"), "audioUrl")));
				song->artist = PianoJsonStrdup (s, "artistName");
				song->album = PianoJsonStrdup (s, "albumName");
				song->title = PianoJsonStrdup (s, "songName");
				song->trackToken = PianoJsonStrdup (s, "trackToken");
				song->stationId = PianoJsonStrdup (s, "stationId");
				song->fileGain = json_object_get_double (
						json_object_object_get (s, "trackGain"));
				song->audioFormat = PIANO_AF_AACPLUS;
				switch (json_object_get_int (json_object_object_get (s,
						"songRating"))) {
					case 1:
						song->rating = PIANO_RATE_LOVE;
						break;
				}

				/* begin linked list or append */
				if (playlist == NULL) {
					playlist = song;
				} else {
					PianoSong_t *curSong = playlist;
					while (curSong->next != NULL) {
						curSong = curSong->next;
					}
					curSong->next = song;
				}
			}

			reqData->retPlaylist = playlist;
			break;
		}

		case PIANO_REQUEST_RATE_SONG: {
			/* love/ban song */
			PianoRequestDataRateSong_t *reqData = req->data;
			reqData->song->rating = reqData->rating;
			break;
		}

		case PIANO_REQUEST_ADD_FEEDBACK:
			/* never ever use this directly, low-level call */
			assert (0);
			break;

		case PIANO_REQUEST_RENAME_STATION: {
			/* rename station and update PianoStation_t structure */
			PianoRequestDataRenameStation_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->newName != NULL);

			free (reqData->station->name);
			reqData->station->name = strdup (reqData->newName);
			break;
		}

		case PIANO_REQUEST_MOVE_SONG: {
			/* move song to different station */
			PianoRequestDataMoveSong_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);
			assert (reqData->step < 2);

			if (reqData->step == 0) {
				ret = PIANO_RET_CONTINUE_REQUEST;
				++reqData->step;
			}
			break;
		}

		case PIANO_REQUEST_DELETE_STATION: {
			/* delete station from server and station list */
			PianoStation_t *station = req->data;

			assert (station != NULL);

			/* delete station from local station list */
			PianoStation_t *curStation = ph->stations, *lastStation = NULL;
			while (curStation != NULL) {
				if (curStation == station) {
					if (lastStation != NULL) {
						lastStation->next = curStation->next;
					} else {
						/* first station in list */
						ph->stations = curStation->next;
					}
					PianoDestroyStation (curStation);
					free (curStation);
					break;
				}
				lastStation = curStation;
				curStation = curStation->next;
			}
			break;
		}

		case PIANO_REQUEST_SEARCH: {
			/* search artist/song */
			PianoRequestDataSearch_t *reqData = req->data;
			PianoSearchResult_t *searchResult;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			searchResult = &reqData->searchResult;
			memset (searchResult, 0, sizeof (*searchResult));

			/* get artists */
			json_object *artists = json_object_object_get (result, "artists");
			if (artists != NULL) {
				for (size_t i=0; i < json_object_array_length (artists); i++) {
					json_object *a = json_object_array_get_idx (artists, i);
					PianoArtist_t *artist;

					if ((artist = calloc (1, sizeof (*artist))) == NULL) {
						return PIANO_RET_OUT_OF_MEMORY;
					}

					artist->name = PianoJsonStrdup (a, "artistName");
					artist->musicId = PianoJsonStrdup (a, "musicToken");

					/* add result to linked list */
					if (searchResult->artists == NULL) {
						searchResult->artists = artist;
					} else {
						PianoArtist_t *curArtist = searchResult->artists;
						while (curArtist->next != NULL) {
							curArtist = curArtist->next;
						}
						curArtist->next = artist;
					}
				}
			}

			/* get songs */
			json_object *songs = json_object_object_get (result, "songs");
			if (songs != NULL) {
				for (size_t i=0; i < json_object_array_length (songs); i++) {
					json_object *s = json_object_array_get_idx (songs, i);
					PianoSong_t *song;

					if ((song = calloc (1, sizeof (*song))) == NULL) {
						return PIANO_RET_OUT_OF_MEMORY;
					}

					song->title = PianoJsonStrdup (s, "songName");
					song->artist = PianoJsonStrdup (s, "artistName");
					song->musicId = PianoJsonStrdup (s, "musicToken");

					/* add result to linked list */
					if (searchResult->songs == NULL) {
						searchResult->songs = song;
					} else {
						PianoSong_t *curSong = searchResult->songs;
						while (curSong->next != NULL) {
							curSong = curSong->next;
						}
						curSong->next = song;
					}
				}
			}
			break;
		}

		case PIANO_REQUEST_CREATE_STATION: {
			/* create station, insert new station into station list on success */
			PianoStation_t *tmpStation;

			if ((tmpStation = calloc (1, sizeof (*tmpStation))) == NULL) {
				return PIANO_RET_OUT_OF_MEMORY;
			}

			PianoJsonParseStation (result, tmpStation);

			/* start new linked list or append */
			if (ph->stations == NULL) {
				ph->stations = tmpStation;
			} else {
				PianoStation_t *curStation = ph->stations;
				while (curStation->next != NULL) {
					curStation = curStation->next;
				}
				curStation->next = tmpStation;
			}
			break;
		}

		case PIANO_REQUEST_ADD_SEED:
		case PIANO_REQUEST_ADD_TIRED_SONG:
		case PIANO_REQUEST_SET_QUICKMIX:
		case PIANO_REQUEST_BOOKMARK_SONG:
		case PIANO_REQUEST_BOOKMARK_ARTIST:
		case PIANO_REQUEST_DELETE_FEEDBACK:
		case PIANO_REQUEST_DELETE_SEED:
			/* response unused */
			break;

		case PIANO_REQUEST_GET_GENRE_STATIONS: {
			/* get genre stations */
			json_object *categories = json_object_object_get (result, "categories");
			if (categories != NULL) {
				for (size_t i = 0; i < json_object_array_length (categories); i++) {
					json_object *c = json_object_array_get_idx (categories, i);
					PianoGenreCategory_t *tmpGenreCategory;

					if ((tmpGenreCategory = calloc (1,
							sizeof (*tmpGenreCategory))) == NULL) {
						return PIANO_RET_OUT_OF_MEMORY;
					}

					tmpGenreCategory->name = PianoJsonStrdup (c,
							"categoryName");

					/* get genre subnodes */
					json_object *stations = json_object_object_get (c,
							"stations");
					if (stations != NULL) {
						for (size_t k = 0;
								k < json_object_array_length (stations); k++) {
							json_object *s =
									json_object_array_get_idx (stations, k);
							PianoGenre_t *tmpGenre;

							if ((tmpGenre = calloc (1,
									sizeof (*tmpGenre))) == NULL) {
								return PIANO_RET_OUT_OF_MEMORY;
							}

							/* get genre attributes */
							tmpGenre->name = PianoJsonStrdup (s,
									"stationName");
							tmpGenre->musicId = PianoJsonStrdup (s,
									"stationToken");

							/* append station */
							if (tmpGenreCategory->genres == NULL) {
								tmpGenreCategory->genres = tmpGenre;
							} else {
								PianoGenre_t *curGenre =
										tmpGenreCategory->genres;
								while (curGenre->next != NULL) {
									curGenre = curGenre->next;
								}
								curGenre->next = tmpGenre;
							}
						}
					}
					/* append category */
					if (ph->genreStations == NULL) {
						ph->genreStations = tmpGenreCategory;
					} else {
						PianoGenreCategory_t *curCat = ph->genreStations;
						while (curCat->next != NULL) {
							curCat = curCat->next;
						}
						curCat->next = tmpGenreCategory;
					}
				}
			}
			break;
		}

		case PIANO_REQUEST_TRANSFORM_STATION: {
			/* transform shared station into private and update isCreator flag */
			PianoStation_t *station = req->data;

			assert (req->responseData != NULL);
			assert (station != NULL);

			station->isCreator = 1;
			break;
		}

		case PIANO_REQUEST_EXPLAIN: {
			/* explain why song was selected */
			PianoRequestDataExplain_t *reqData = req->data;
			const size_t strSize = 1024;
			size_t pos = 0;

			assert (reqData != NULL);

			json_object *explanations = json_object_object_get (result,
					"explanations");
			if (explanations != NULL) {
				reqData->retExplain = malloc (strSize *
						sizeof (*reqData->retExplain));
				strncpy (reqData->retExplain, "We're playing this track "
						"because it features ", strSize);
				pos = strlen (reqData->retExplain);
				for (size_t i=0; i < json_object_array_length (explanations); i++) {
					json_object *e = json_object_array_get_idx (explanations,
							i);
					const char *s = json_object_get_string (
							json_object_object_get (e, "focusTraitName"));

					strncpy (&reqData->retExplain[pos], s, strSize-pos-1);
					pos += strlen (s);
					if (i < json_object_array_length (explanations)-2) {
						strncpy (&reqData->retExplain[pos], ", ", strSize-pos-1);
						pos += 2;
					} else if (i == json_object_array_length (explanations)-2) {
						strncpy (&reqData->retExplain[pos], " and ", strSize-pos-1);
						pos += 5;
					} else {
						strncpy (&reqData->retExplain[pos], ".", strSize-pos-1);
						pos += 1;
					}
				}
			}
			break;
		}

		case PIANO_REQUEST_GET_SEED_SUGGESTIONS: {
#if 0
			/* find similar artists */
			PianoRequestDataGetSeedSuggestions_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			ret = PianoXmlParseSeedSuggestions (req->responseData,
					&reqData->searchResult);
#endif
			break;
		}

		case PIANO_REQUEST_GET_STATION_INFO: {
			/* get station information (seeds and feedback) */
			PianoRequestDataGetStationInfo_t *reqData = req->data;
			PianoStationInfo_t *info;

			assert (reqData != NULL);

			info = &reqData->info;
			assert (info != NULL);

			/* parse music seeds */
			json_object *music = json_object_object_get (result, "music");
			if (music != NULL) {
				/* songs */
				json_object *songs = json_object_object_get (music, "songs");
				if (songs != NULL) {
					for (size_t i = 0; i < json_object_array_length (songs); i++) {
						json_object *s = json_object_array_get_idx (songs, i);
						PianoSong_t *seedSong;

						seedSong = calloc (1, sizeof (*seedSong));
						if (seedSong == NULL) {
							return PIANO_RET_OUT_OF_MEMORY;
						}

						seedSong->title = PianoJsonStrdup (s, "songName");
						seedSong->artist = PianoJsonStrdup (s, "artistName");
						seedSong->seedId = PianoJsonStrdup (s, "seedId");

						if (info->songSeeds == NULL) {
							info->songSeeds = seedSong;
						} else {
							PianoSong_t *curSong = info->songSeeds;
							while (curSong->next != NULL) {
								curSong = curSong->next;
							}
							curSong->next = seedSong;
						}
					}
				}

				/* artists */
				json_object *artists = json_object_object_get (music,
						"artists");
				if (artists != NULL) {
					for (size_t i = 0; i < json_object_array_length (artists); i++) {
						json_object *a = json_object_array_get_idx (artists, i);
						PianoArtist_t *seedArtist;

						seedArtist = calloc (1, sizeof (*seedArtist));
						if (seedArtist == NULL) {
							return PIANO_RET_OUT_OF_MEMORY;
						}

						seedArtist->name = PianoJsonStrdup (a, "artistName");
						seedArtist->seedId = PianoJsonStrdup (a, "seedId");

						if (info->artistSeeds == NULL) {
							info->artistSeeds = seedArtist;
						} else {
							PianoArtist_t *curArtist = info->artistSeeds;
							while (curArtist->next != NULL) {
								curArtist = curArtist->next;
							}
							curArtist->next = seedArtist;
						}
					}
				}
			}

			/* parse feedback */
			json_object *feedback = json_object_object_get (result,
					"feedback");
			if (feedback != NULL) {
				json_object_object_foreach (feedback, key, val) {
					for (size_t i = 0; i < json_object_array_length (val); i++) {
						json_object *s = json_object_array_get_idx (val, i);
						PianoSong_t *feedbackSong;

						feedbackSong = calloc (1, sizeof (*feedbackSong));
						if (feedbackSong == NULL) {
							return PIANO_RET_OUT_OF_MEMORY;
						}

						feedbackSong->title = PianoJsonStrdup (s, "songName");
						feedbackSong->artist = PianoJsonStrdup (s,
								"artistName");
						feedbackSong->feedbackId = PianoJsonStrdup (s,
								"feedbackId");
						feedbackSong->rating = json_object_get_boolean (
								json_object_object_get (s, "isPositive")) ?
								PIANO_RATE_LOVE : PIANO_RATE_BAN;


						if (info->feedback == NULL) {
							info->feedback = feedbackSong;
						} else {
							PianoSong_t *curSong = info->feedback;
							while (curSong->next != NULL) {
								curSong = curSong->next;
							}
							curSong->next = feedbackSong;
						}
					}
				}
			}
			break;
		}
	}

	json_object_put (j);

	return ret;
}

/*	get station from list by id
 *	@param search here
 *	@param search for this
 *	@return the first station structure matching the given id
 */
PianoStation_t *PianoFindStationById (PianoStation_t *stations,
		const char *searchStation) {
	while (stations != NULL) {
		if (strcmp (stations->id, searchStation) == 0) {
			return stations;
		}
		stations = stations->next;
	}
	return NULL;
}

/*	convert return value to human-readable string
 *	@param enum
 *	@return error string
 */
const char *PianoErrorToStr (PianoReturn_t ret) {
	switch (ret) {
		case PIANO_RET_OK:
			return "Everything is fine :)";
			break;

		case PIANO_RET_ERR:
			return "Unknown.";
			break;

		case PIANO_RET_INVALID_RESPONSE:
			return "Invalid response.";
			break;

		case PIANO_RET_CONTINUE_REQUEST:
			/* never shown to the user */
			assert (0);
			return "Fix your program.";
			break;

		case PIANO_RET_OUT_OF_MEMORY:
			return "Out of memory.";
			break;

		/* pandora error messages */
		case PIANO_RET_P_INTERNAL:
			return "Internal error.";
			break;

		case PIANO_RET_P_CALL_NOT_ALLOWED:
			return "Call not allowed.";
			break;

		case PIANO_RET_P_INVALID_AUTH_TOKEN:
			return "Invalid auth token.";
			break;

		case PIANO_RET_P_MAINTENANCE_MODE:
			return "Maintenance mode.";
			break;

		case PIANO_RET_P_MAX_STATIONS_REACHED:
			return "Max number of stations reached.";
			break;

		case PIANO_RET_P_READ_ONLY_MODE:
			return "Read only mode. Try again later.";
			break;

		case PIANO_RET_P_STATION_DOES_NOT_EXIST:
			return "Station does not exist.";
			break;

		case PIANO_RET_P_INVALID_PARTNER_LOGIN:
			return "Invalid partner login.";
			break;

		default:
			return "No error message available.";
			break;
	}
}

