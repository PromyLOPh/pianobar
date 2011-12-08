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

/* needed for urlencode */
#include <waitress.h>

#include "piano_private.h"
#include "piano.h"
#include "xml.h"
#include "crypt.h"
#include "config.h"

#define PIANO_PROTOCOL_VERSION "33"
#define PIANO_RPC_HOST "www.pandora.com"
#define PIANO_RPC_PORT "80"
#define PIANO_RPC_PATH "/radio/xmlrpc/v" PIANO_PROTOCOL_VERSION "?"
#define PIANO_SEND_BUFFER_SIZE 10000

/*	initialize piano handle
 *	@param piano handle
 *	@return nothing
 */
void PianoInit (PianoHandle_t *ph) {
	memset (ph, 0, sizeof (*ph));

	/* route-id seems to be random. we're using time anyway... */
	snprintf (ph->routeId, sizeof (ph->routeId), "%07luP",
			(unsigned long) time (NULL) % 10000000);
}

/*	destroy artist linked list
 */
void PianoDestroyArtists (PianoArtist_t *artists) {
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
void PianoDestroyStations (PianoStation_t *stations) {
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
		free (curSong->userSeed);
		free (curSong->stationId);
		free (curSong->album);
		free (curSong->artistMusicId);
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
void PianoDestroyGenres (PianoGenre_t *genres) {
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
void PianoDestroyUserInfo (PianoUserInfo_t *user) {
	free (user->webAuthToken);
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
	char xmlSendBuf[PIANO_SEND_BUFFER_SIZE];
	/* corrected timestamp */
	time_t timestamp = time (NULL) - ph->timeOffset;

	assert (ph != NULL);
	assert (req != NULL);

	req->type = type;

	switch (req->type) {
		case PIANO_REQUEST_LOGIN: {
			/* authenticate user */
			PianoRequestDataLogin_t *logindata = req->data;

			assert (logindata != NULL);

			switch (logindata->step) {
				case 0:
					snprintf (xmlSendBuf, sizeof (xmlSendBuf), 
							"<?xml version=\"1.0\"?><methodCall>"
							"<methodName>misc.sync</methodName>"
							"<params></params></methodCall>");
					snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
							"rid=%s&method=sync", ph->routeId);
					break;

				case 1: {
					char *xmlencodedPassword = NULL;

					/* username == email address does not contain &,<,>," */
					if ((xmlencodedPassword =
							PianoXmlEncodeString (logindata->password)) ==
							NULL) {
						return PIANO_RET_OUT_OF_MEMORY;
					}

					snprintf (xmlSendBuf, sizeof (xmlSendBuf), 
							"<?xml version=\"1.0\"?><methodCall>"
							"<methodName>listener.authenticateListener</methodName>"
							"<params><param><value><int>%lu</int></value></param>"
							/* user */
							"<param><value><string>%s</string></value></param>"
							/* password */
							"<param><value><string>%s</string></value></param>"
							/* vendor */
							"<param><value><string>html5tuner</string></value></param>"
							"<param><value><string/></value></param>"
							"<param><value><string/></value></param>"
							"<param><value><string>HTML5</string></value></param>"
							"<param><value><boolean>1</boolean></value></param>"
							"</params></methodCall>", (unsigned long) timestamp,
							logindata->user, xmlencodedPassword);
					snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
							"rid=%s&method=authenticateListener", ph->routeId);

					free (xmlencodedPassword);
					break;
				}
			}
			break;
		}

		case PIANO_REQUEST_GET_STATIONS:
			/* get stations, user must be authenticated */
			assert (ph->user.listenerId != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.getStations</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=getStations", ph->routeId,
					ph->user.listenerId);
			break;

		case PIANO_REQUEST_GET_PLAYLIST: {
			/* get playlist for specified station */
			PianoRequestDataGetPlaylist_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->station->id != NULL);
			assert (reqData->format != PIANO_AF_UNKNOWN);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>playlist.getFragment</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					/* total listening time */
					"<param><value><string>0</string></value></param>"
					/* time since last session */
					"<param><value><string></string></value></param>"
					/* tracking code */
					"<param><value><string></string></value></param>"
					/* audio format */
					"<param><value><string>%s</string></value></param>"
					/* delta listening time */
					"<param><value><string>0</string></value></param>"
					/* listening timestamp */
					"<param><value><string>0</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->station->id,
					PianoAudioFormatToString (reqData->format));
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=getFragment&arg1=%s&arg2=0"
					"&arg3=&arg4=&arg5=%s&arg6=0&arg7=0", ph->routeId,
					ph->user.listenerId, reqData->station->id,
					PianoAudioFormatToString (reqData->format));
			break;
		}

		case PIANO_REQUEST_ADD_FEEDBACK: {
			/* low-level, don't use directly (see _RATE_SONG and _MOVE_SONG) */
			PianoRequestDataAddFeedback_t *reqData = req->data;
			
			assert (reqData != NULL);
			assert (reqData->stationId != NULL);
			assert (reqData->rating != PIANO_RATE_NONE);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.addFeedback</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					/* track token */
					"<param><value><string>%s</string></value></param>"
					/* positive */
					"<param><value><boolean>%i</boolean></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->stationId, reqData->trackToken,
					(reqData->rating == PIANO_RATE_LOVE) ? 1 : 0);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=addFeedback&arg1=%s&arg2=%s"
					"&arg3=%s",
					ph->routeId, ph->user.listenerId, reqData->stationId,
					reqData->trackToken,
					(reqData->rating == PIANO_RATE_LOVE) ? "true" : "false");
			break;
		}

		case PIANO_REQUEST_RENAME_STATION: {
			/* rename stations */
			PianoRequestDataRenameStation_t *reqData = req->data;
			char *urlencodedNewName, *xmlencodedNewName;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->newName != NULL);

			if ((xmlencodedNewName = PianoXmlEncodeString (reqData->newName)) == NULL) {
				return PIANO_RET_OUT_OF_MEMORY;
			}
			urlencodedNewName = WaitressUrlEncode (reqData->newName);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.setStationName</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					/* new name */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->station->id,
					xmlencodedNewName);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=setStationName&arg1=%s&arg2=%s",
					ph->routeId, ph->user.listenerId, reqData->station->id,
					urlencodedNewName);

			free (urlencodedNewName);
			free (xmlencodedNewName);
			break;
		}

		case PIANO_REQUEST_DELETE_STATION: {
			/* delete station */
			PianoStation_t *station = req->data;

			assert (station != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.removeStation</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, station->id);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=removeStation&arg1=%s", ph->routeId,
					ph->user.listenerId, station->id);
			break;
		}

		case PIANO_REQUEST_SEARCH: {
			/* search for artist/song title */
			PianoRequestDataSearch_t *reqData = req->data;
			char *xmlencodedSearchStr, *urlencodedSearchStr;

			assert (reqData != NULL);
			assert (reqData->searchStr != NULL);

			if ((xmlencodedSearchStr = PianoXmlEncodeString (reqData->searchStr)) == NULL) {
				return PIANO_RET_OUT_OF_MEMORY;
			}
			urlencodedSearchStr = WaitressUrlEncode (reqData->searchStr);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>music.search</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* search string */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, xmlencodedSearchStr);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=search&arg1=%s", ph->routeId,
					ph->user.listenerId, urlencodedSearchStr);

			free (urlencodedSearchStr);
			free (xmlencodedSearchStr);
			break;
		}

		case PIANO_REQUEST_CREATE_STATION: {
			/* create new station from specified musicid (type=mi, get one by
			 * performing a search) or shared station id (type=sh) */
			PianoRequestDataCreateStation_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->id != NULL);
			assert (reqData->type != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.createStation</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* music id */
					"<param><value><string>%s%s</string></value></param>"
					/* empty */
					"<param><value><string></string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->type, reqData->id);

			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=createStation&arg1=%s%s&arg2=", ph->routeId,
					ph->user.listenerId, reqData->type, reqData->id);
			break;
		}

		case PIANO_REQUEST_ADD_SEED: {
			/* add another seed to specified station */
			PianoRequestDataAddSeed_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->musicId != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.addSeed</methodName><params>"
					"<param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					/* music id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->station->id, reqData->musicId);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=addSeed&arg1=%s&arg2=%s", ph->routeId,
					ph->user.listenerId, reqData->station->id, reqData->musicId);
			break;
		}

		case PIANO_REQUEST_ADD_TIRED_SONG: {
			/* ban song for a month from all stations */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>listener.addTiredSong</methodName><params>"
					"<param><value><int>%lu</int></value></param>"
					"<param><value><string>%s</string></value></param>"
					/* key */
					"<param><value><string>%s</string></value></param>"
					/* user seed */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken,
					(song->musicId == NULL) ? "" : song->musicId,
					(song->userSeed == NULL) ? "" : song->userSeed,
					song->stationId);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=addTiredSong&arg1=%s&arg2=%s&arg3=%s",
					ph->routeId, ph->user.listenerId,
					(song->musicId == NULL) ? "" : song->musicId,
					(song->userSeed == NULL) ? "" : song->userSeed,
					song->stationId);
			break;
		}

		case PIANO_REQUEST_SET_QUICKMIX: {
			/* select stations included in quickmix (see useQuickMix flag of
			 * PianoStation_t) */
			char valueBuf[1000], urlArgBuf[1000];
			PianoStation_t *curStation = ph->stations;

			memset (urlArgBuf, 0, sizeof (urlArgBuf));
			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.setQuickMix</methodName><params>"
					"<param><value><int>%lu</int></value></param>"
					"<param><value><string>%s</string></value></param>"
					/* quick mix type */
					"<param><value><string>RANDOM</string></value></param>"
					"<param><value><array><data>", (unsigned long) timestamp,
					ph->user.authToken);
			while (curStation != NULL) {
				/* quick mix can't contain itself */
				if (!curStation->useQuickMix || curStation->isQuickMix) {
					curStation = curStation->next;
					continue;
				}
				/* append to xml doc */
				snprintf (valueBuf, sizeof (valueBuf),
						"<value><string>%s</string></value>", curStation->id);
				strncat (xmlSendBuf, valueBuf, sizeof (xmlSendBuf) -
						strlen (xmlSendBuf) - 1);
				/* append to url arg */
				strncat (urlArgBuf, curStation->id, sizeof (urlArgBuf) -
						strlen (urlArgBuf) - 1);
				curStation = curStation->next;
				/* if not last item: append "," */
				if (curStation != NULL) {
					strncat (urlArgBuf, "%2C", sizeof (urlArgBuf) -
							strlen (urlArgBuf) - 1);
				}
			}
			strncat (xmlSendBuf,
					"</data></array></value></param>"
					/* empty */
					"<param><value><string></string></value></param>"
					/* empty */
					"<param><value><string></string></value></param>"
					"</params></methodCall>",
					sizeof (xmlSendBuf) - strlen (xmlSendBuf) - 1);

			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=setQuickMix&arg1=RANDOM&arg2=%s&arg3=&arg4=",
					ph->routeId, ph->user.listenerId, urlArgBuf);
			break;
		}

		case PIANO_REQUEST_GET_GENRE_STATIONS:
			/* receive list of pandora's genre stations */
			xmlSendBuf[0] = '\0';
			snprintf (req->urlPath, sizeof (req->urlPath), "/xml/genre?r=%lu",
					(unsigned long) timestamp);
			break;

		case PIANO_REQUEST_TRANSFORM_STATION: {
			/* transform shared station into private */
			PianoStation_t *station = req->data;

			assert (station != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.transformShared</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, station->id);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=transformShared&arg1=%s", ph->routeId,
					ph->user.listenerId, station->id);
			break;
		}

		case PIANO_REQUEST_EXPLAIN: {
			/* explain why particular song was played */
			PianoRequestDataExplain_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->song != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>playlist.narrative</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					/* music id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->song->stationId,
					reqData->song->musicId);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=narrative&arg1=%s&arg2=%s",
					ph->routeId, ph->user.listenerId, reqData->song->stationId,
					reqData->song->musicId);
			break;
		}

		case PIANO_REQUEST_GET_SEED_SUGGESTIONS: {
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
			break;
		}

		case PIANO_REQUEST_BOOKMARK_SONG: {
			/* bookmark song */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.createBookmark</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					/* music id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, song->stationId, song->musicId);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=createBookmark&arg1=%s&arg2=%s",
					ph->routeId, ph->user.listenerId, song->stationId,
					song->musicId);
			break;
		}

		case PIANO_REQUEST_BOOKMARK_ARTIST: {
			/* bookmark artist */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.createArtistBookmark</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* music id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, song->artistMusicId);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=createArtistBookmark&arg1=%s",
					ph->routeId, ph->user.listenerId, song->artistMusicId);
			break;
		}

		case PIANO_REQUEST_GET_STATION_INFO: {
			/* get station information (seeds and feedback) */
			PianoRequestDataGetStationInfo_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.getStation</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->station->id);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=getStation&arg1=%s",
					ph->routeId, ph->user.listenerId, reqData->station->id);
			break;
		}

		case PIANO_REQUEST_DELETE_FEEDBACK: {
			PianoSong_t *song = req->data;

			assert (song != NULL);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.deleteFeedback</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* feedback id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, song->feedbackId);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=deleteFeedback&arg1=%s",
					ph->routeId, ph->user.listenerId, song->feedbackId);
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

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>station.deleteSeed</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* seed id */
					"<param><value><string>%s</string></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, seedId);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=deleteSeed&arg1=%s",
					ph->routeId, ph->user.listenerId, seedId);
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

	if ((req->postData = PianoEncryptString (xmlSendBuf)) == NULL) {
		return PIANO_RET_OUT_OF_MEMORY;
	}

	return PIANO_RET_OK;
}

/*	parse xml response and update data structures/return new data structure
 *	@param piano handle
 *	@param initialized request (expects responseData to be a NUL-terminated
 *			string)
 */
PianoReturn_t PianoResponse (PianoHandle_t *ph, PianoRequest_t *req) {
	PianoReturn_t ret = PIANO_RET_ERR;

	assert (ph != NULL);
	assert (req != NULL);

	switch (req->type) {
		case PIANO_REQUEST_LOGIN: {
			/* authenticate user */
			PianoRequestDataLogin_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			switch (reqData->step) {
				case 0: {
					char *cryptedTimestamp = NULL;

					assert (req->responseData != NULL);

					/* abusing parseNarrative; has same xml structure */
					ret = PianoXmlParseNarrative (req->responseData, &cryptedTimestamp);
					if (cryptedTimestamp != NULL) {
						unsigned long timestamp = 0;
						time_t realTimestamp = time (NULL);
						char *decryptedTimestamp = NULL, *decryptedPos = NULL;
						unsigned char i = 4;

						if ((decryptedTimestamp = PianoDecryptString (cryptedTimestamp)) != NULL) {
							decryptedPos = decryptedTimestamp;
							/* skip four bytes garbage? at beginning */
							while (i-- > 0 && *decryptedPos++ != '\0');
							timestamp = strtoul (decryptedPos, NULL, 0);
							ph->timeOffset = realTimestamp - timestamp;

							free (decryptedTimestamp);
						}
						free (cryptedTimestamp);
					}
					ret = PIANO_RET_CONTINUE_REQUEST;
					++reqData->step;
					break;
				}

				case 1:
					/* information exists when reauthenticating, destroy to
					 * avoid memleak */
					if (ph->user.listenerId != NULL) {
						PianoDestroyUserInfo (&ph->user);
					}
					ret = PianoXmlParseUserinfo (ph, req->responseData);
					break;
			}
			break;
		}

		case PIANO_REQUEST_GET_STATIONS:
			/* get stations */
			assert (req->responseData != NULL);
			
			ret = PianoXmlParseStations (ph, req->responseData);
			break;

		case PIANO_REQUEST_GET_PLAYLIST: {
			/* get playlist, usually four songs */
			PianoRequestDataGetPlaylist_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			reqData->retPlaylist = NULL;
			ret = PianoXmlParsePlaylist (ph, req->responseData,
					&reqData->retPlaylist);
			break;
		}

		case PIANO_REQUEST_RATE_SONG:
			/* love/ban song */
			assert (req->responseData != NULL);

			ret = PianoXmlParseSimple (req->responseData);
			if (ret == PIANO_RET_OK) {
				PianoRequestDataRateSong_t *reqData = req->data;
				reqData->song->rating = reqData->rating;
			}
			break;

		case PIANO_REQUEST_ADD_FEEDBACK:
			/* never ever use this directly, low-level call */
			assert (0);
			break;

		case PIANO_REQUEST_MOVE_SONG: {
			/* move song to different station */
			PianoRequestDataMoveSong_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);
			assert (reqData->step < 2);

			ret = PianoXmlParseSimple (req->responseData);
			if (ret == PIANO_RET_OK && reqData->step == 0) {
				ret = PIANO_RET_CONTINUE_REQUEST;
				++reqData->step;
			}
			break;
		}

		case PIANO_REQUEST_RENAME_STATION:
			/* rename station and update PianoStation_t structure */
			assert (req->responseData != NULL);

			if ((ret = PianoXmlParseSimple (req->responseData)) == PIANO_RET_OK) {
				PianoRequestDataRenameStation_t *reqData = req->data;

				assert (reqData != NULL);
				assert (reqData->station != NULL);
				assert (reqData->newName != NULL);

				free (reqData->station->name);
				reqData->station->name = strdup (reqData->newName);
			}
			break;

		case PIANO_REQUEST_DELETE_STATION:
			/* delete station from server and station list */
			assert (req->responseData != NULL);

			if ((ret = PianoXmlParseSimple (req->responseData)) == PIANO_RET_OK) {
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
			}
			break;

		case PIANO_REQUEST_SEARCH: {
			/* search artist/song */
			PianoRequestDataSearch_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			ret = PianoXmlParseSearch (req->responseData, &reqData->searchResult);
			break;
		}

		case PIANO_REQUEST_CREATE_STATION: {
			/* create station, insert new station into station list on success */
			assert (req->responseData != NULL);

			ret = PianoXmlParseCreateStation (ph, req->responseData);
			break;
		}

		case PIANO_REQUEST_ADD_SEED: {
			/* add seed to station, updates station structure */
			PianoRequestDataAddSeed_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);
			assert (reqData->station != NULL);

			/* FIXME: update station data instead of replacing them */
			ret = PianoXmlParseAddSeed (ph, req->responseData, reqData->station);
			break;
		}

		case PIANO_REQUEST_ADD_TIRED_SONG:
		case PIANO_REQUEST_SET_QUICKMIX:
		case PIANO_REQUEST_BOOKMARK_SONG:
		case PIANO_REQUEST_BOOKMARK_ARTIST:
		case PIANO_REQUEST_DELETE_FEEDBACK:
			assert (req->responseData != NULL);

			ret = PianoXmlParseSimple (req->responseData);
			break;

		case PIANO_REQUEST_GET_GENRE_STATIONS:
			/* get genre stations */
			assert (req->responseData != NULL);

			ret = PianoXmlParseGenreExplorer (ph, req->responseData);
			break;

		case PIANO_REQUEST_TRANSFORM_STATION: {
			/* transform shared station into private and update isCreator flag */
			PianoStation_t *station = req->data;

			assert (req->responseData != NULL);
			assert (station != NULL);

			/* though this call returns a bunch of "new" data only this one is
			 * changed and important (at the moment) */
			if ((ret = PianoXmlParseTranformStation (req->responseData)) ==
					PIANO_RET_OK) {
				station->isCreator = 1;
			}
			break;
		}

		case PIANO_REQUEST_EXPLAIN: {
			/* explain why song was selected */
			PianoRequestDataExplain_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			ret = PianoXmlParseNarrative (req->responseData, &reqData->retExplain);
			break;
		}

		case PIANO_REQUEST_GET_SEED_SUGGESTIONS: {
			/* find similar artists */
			PianoRequestDataGetSeedSuggestions_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			ret = PianoXmlParseSeedSuggestions (req->responseData,
					&reqData->searchResult);
			break;
		}

		case PIANO_REQUEST_GET_STATION_INFO: {
			/* get station information (seeds and feedback) */
			PianoRequestDataGetStationInfo_t *reqData = req->data;

			assert (req->responseData != NULL);
			assert (reqData != NULL);

			ret = PianoXmlParseGetStationInfo (req->responseData,
					&reqData->info);
			break;
		}

		case PIANO_REQUEST_DELETE_SEED: {
			assert (req->responseData != NULL);

			/* dummy function, checks for errors only */
			ret = PianoXmlParseTranformStation (req->responseData);
		}
	}

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

		case PIANO_RET_XML_INVALID:
			return "Invalid XML.";
			break;

		case PIANO_RET_AUTH_TOKEN_INVALID:
			return "Invalid auth token.";
			break;
		
		case PIANO_RET_AUTH_USER_PASSWORD_INVALID:
			return "Username and/or password not correct.";
			break;

		case PIANO_RET_NOT_AUTHORIZED:
			return "Not authorized.";
			break;

		case PIANO_RET_PROTOCOL_INCOMPATIBLE:
			return "Protocol incompatible. Please upgrade " PACKAGE ".";
			break;

		case PIANO_RET_READONLY_MODE:
			return "Request cannot be completed at this time, please try "
					"again later.";
			break;

		case PIANO_RET_STATION_CODE_INVALID:
			return "Station id is invalid.";
			break;

		case PIANO_RET_IP_REJECTED:
			return "Your ip address was rejected. Please setup a control "
					"proxy (see manpage).";
			break;

		case PIANO_RET_STATION_NONEXISTENT:
			return "Station does not exist.";
			break;

		case PIANO_RET_OUT_OF_MEMORY:
			return "Out of memory.";
			break;

		case PIANO_RET_OUT_OF_SYNC:
			return "Out of sync. Please correct your system's time.";
			break;

		case PIANO_RET_PLAYLIST_END:
			return "Playlist end.";
			break;

		case PIANO_RET_QUICKMIX_NOT_PLAYABLE:
			return "Quickmix not playable.";
			break;

		case PIANO_RET_REMOVING_TOO_MANY_SEEDS:
			return "Last seed cannot be removed.";
			break;

		default:
			return "No error message available.";
			break;
	}
}

