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

#ifndef __FreeBSD__
#define _BSD_SOURCE /* required by strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

#include <json.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>

#include "piano.h"
#include "piano_private.h"
#include "crypt.h"

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

/*	concat strings
 *	@param destination
 *	@param source string
 *	@param destination size
 */
static void PianoStrpcat (char * restrict dest, const char * restrict src,
		size_t len) {
	/* skip to end of string */
	while (*dest != '\0' && len > 1) {
		++dest;
		--len;
	}

	/* append until source exhaused or destination full */
	while (*src != '\0' && len > 1) {
		*dest = *src;
		++dest;
		++src;
		--len;
	}

	*dest = '\0';
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

			if (ret == PIANO_RET_P_INVALID_PARTNER_LOGIN &&
					req->type == PIANO_REQUEST_LOGIN) {
				PianoRequestDataLogin_t *reqData = req->data;
				if (reqData->step == 1) {
					/* return value is ambiguous, as both, partnerLogin and
					 * userLogin return INVALID_PARTNER_LOGIN. Fix that to provide
					 * better error messages. */
					ret = PIANO_RET_INVALID_LOGIN;
				}
			}
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
					const char * const cryptedTimestamp = json_object_get_string (
							json_object_object_get (result, "syncTime"));
					const time_t realTimestamp = time (NULL);
					char *decryptedTimestamp = NULL;
					size_t decryptedSize;

					ret = PIANO_RET_ERR;
					if ((decryptedTimestamp = PianoDecryptString (ph->partner.in,
							cryptedTimestamp, &decryptedSize)) != NULL &&
							decryptedSize > 4) {
						/* skip four bytes garbage(?) at beginning */
						const unsigned long timestamp = strtoul (
								decryptedTimestamp+4, NULL, 0);
						ph->timeOffset = (long int) realTimestamp -
								(long int) timestamp;
						ret = PIANO_RET_CONTINUE_REQUEST;
					}
					free (decryptedTimestamp);
					/* get auth token */
					ph->partner.authToken = PianoJsonStrdup (result,
							"partnerAuthToken");
					ph->partner.id = json_object_get_int (
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

			for (int i = 0; i < json_object_array_length (stations); i++) {
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
				ph->stations = PianoListAppendP (ph->stations, tmpStation);
			}

			/* fix quickmix flags */
			if (mix != NULL) {
				PianoStation_t *curStation = ph->stations;
				PianoListForeachP (curStation) {
					for (int i = 0; i < json_object_array_length (mix); i++) {
						json_object *id = json_object_array_get_idx (mix, i);
						if (strcmp (json_object_get_string (id),
								curStation->id) == 0) {
							curStation->useQuickMix = true;
						}
					}
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
			assert (reqData->quality != PIANO_AQ_UNKNOWN);

			json_object *items = json_object_object_get (result, "items");
			assert (items != NULL);

			for (int i = 0; i < json_object_array_length (items); i++) {
				json_object *s = json_object_array_get_idx (items, i);
				PianoSong_t *song;

				if ((song = calloc (1, sizeof (*song))) == NULL) {
					return PIANO_RET_OUT_OF_MEMORY;
				}

				if (json_object_object_get (s, "artistName") == NULL) {
					free (song);
					continue;
				}

				/* get audio url based on selected quality */
				static const char *qualityMap[] = {"", "lowQuality", "mediumQuality",
						"highQuality"};
				assert (reqData->quality < sizeof (qualityMap)/sizeof (*qualityMap));
				static const char *formatMap[] = {"", "aacplus", "mp3"};
				json_object *map = json_object_object_get (s, "audioUrlMap");
				assert (map != NULL);

				if (map != NULL) {
					map = json_object_object_get (map, qualityMap[reqData->quality]);

					if (map != NULL) {
						const char *encoding = json_object_get_string (
								json_object_object_get (map, "encoding"));
						assert (encoding != NULL);
						for (size_t k = 0; k < sizeof (formatMap)/sizeof (*formatMap); k++) {
							if (strcmp (formatMap[k], encoding) == 0) {
								song->audioFormat = k;
								break;
							}
						}
						song->audioUrl = PianoJsonStrdup (map, "audioUrl");
					} else {
						/* requested quality is not available */
						ret = PIANO_RET_QUALITY_UNAVAILABLE;
						free (song);
						PianoDestroyPlaylist (playlist);
						goto cleanup;
					}
				}

				song->artist = PianoJsonStrdup (s, "artistName");
				song->album = PianoJsonStrdup (s, "albumName");
				song->title = PianoJsonStrdup (s, "songName");
				song->trackToken = PianoJsonStrdup (s, "trackToken");
				song->stationId = PianoJsonStrdup (s, "stationId");
				song->coverArt = PianoJsonStrdup (s, "albumArtUrl");
				song->detailUrl = PianoJsonStrdup (s, "songDetailUrl");
				song->fileGain = json_object_get_double (
						json_object_object_get (s, "trackGain"));
				song->length = json_object_get_int (
						json_object_object_get (s, "trackLength"));
				switch (json_object_get_int (json_object_object_get (s,
						"songRating"))) {
					case 1:
						song->rating = PIANO_RATE_LOVE;
						break;
				}

				playlist = PianoListAppendP (playlist, song);
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

		case PIANO_REQUEST_DELETE_STATION: {
			/* delete station from server and station list */
			PianoStation_t *station = req->data;

			assert (station != NULL);

			ph->stations = PianoListDeleteP (ph->stations, station);
			PianoDestroyStation (station);
			free (station);
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
				for (int i = 0; i < json_object_array_length (artists); i++) {
					json_object *a = json_object_array_get_idx (artists, i);
					PianoArtist_t *artist;

					if ((artist = calloc (1, sizeof (*artist))) == NULL) {
						return PIANO_RET_OUT_OF_MEMORY;
					}

					artist->name = PianoJsonStrdup (a, "artistName");
					artist->musicId = PianoJsonStrdup (a, "musicToken");

					searchResult->artists =
							PianoListAppendP (searchResult->artists, artist);
				}
			}

			/* get songs */
			json_object *songs = json_object_object_get (result, "songs");
			if (songs != NULL) {
				for (int i = 0; i < json_object_array_length (songs); i++) {
					json_object *s = json_object_array_get_idx (songs, i);
					PianoSong_t *song;

					if ((song = calloc (1, sizeof (*song))) == NULL) {
						return PIANO_RET_OUT_OF_MEMORY;
					}

					song->title = PianoJsonStrdup (s, "songName");
					song->artist = PianoJsonStrdup (s, "artistName");
					song->musicId = PianoJsonStrdup (s, "musicToken");

					searchResult->songs =
							PianoListAppendP (searchResult->songs, song);
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

			PianoStation_t *search = PianoFindStationById (ph->stations,
					tmpStation->id);
			if (search != NULL) {
				ph->stations = PianoListDeleteP (ph->stations, search);
				PianoDestroyStation (search);
				free (search);
			}
			ph->stations = PianoListAppendP (ph->stations, tmpStation);
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
				for (int i = 0; i < json_object_array_length (categories); i++) {
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
						for (int k = 0;
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

							tmpGenreCategory->genres =
									PianoListAppendP (tmpGenreCategory->genres,
									tmpGenre);
						}
					}

					ph->genreStations = PianoListAppendP (ph->genreStations,
							tmpGenreCategory);
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
			const size_t strSize = 768;

			assert (reqData != NULL);

			json_object *explanations = json_object_object_get (result,
					"explanations");
			if (explanations != NULL) {
				reqData->retExplain = malloc (strSize *
						sizeof (*reqData->retExplain));
				strncpy (reqData->retExplain, "We're playing this track "
						"because it features ", strSize);
				for (int i = 0; i < json_object_array_length (explanations); i++) {
					json_object *e = json_object_array_get_idx (explanations,
							i);
					const char *s = json_object_get_string (
							json_object_object_get (e, "focusTraitName"));

					PianoStrpcat (reqData->retExplain, s, strSize);
					if (i < json_object_array_length (explanations)-2) {
						PianoStrpcat (reqData->retExplain, ", ", strSize);
					} else if (i == json_object_array_length (explanations)-2) {
						PianoStrpcat (reqData->retExplain, " and ", strSize);
					} else {
						PianoStrpcat (reqData->retExplain, ".", strSize);
					}
				}
			}
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
					for (int i = 0; i < json_object_array_length (songs); i++) {
						json_object *s = json_object_array_get_idx (songs, i);
						PianoSong_t *seedSong;

						seedSong = calloc (1, sizeof (*seedSong));
						if (seedSong == NULL) {
							return PIANO_RET_OUT_OF_MEMORY;
						}

						seedSong->title = PianoJsonStrdup (s, "songName");
						seedSong->artist = PianoJsonStrdup (s, "artistName");
						seedSong->seedId = PianoJsonStrdup (s, "seedId");

						info->songSeeds = PianoListAppendP (info->songSeeds,
								seedSong);
					}
				}

				/* artists */
				json_object *artists = json_object_object_get (music,
						"artists");
				if (artists != NULL) {
					for (int i = 0; i < json_object_array_length (artists); i++) {
						json_object *a = json_object_array_get_idx (artists, i);
						PianoArtist_t *seedArtist;

						seedArtist = calloc (1, sizeof (*seedArtist));
						if (seedArtist == NULL) {
							return PIANO_RET_OUT_OF_MEMORY;
						}

						seedArtist->name = PianoJsonStrdup (a, "artistName");
						seedArtist->seedId = PianoJsonStrdup (a, "seedId");

						info->artistSeeds =
								PianoListAppendP (info->artistSeeds, seedArtist);
					}
				}
			}

			/* parse feedback */
			json_object *feedback = json_object_object_get (result,
					"feedback");
			if (feedback != NULL) {
				json_object_object_foreach (feedback, key, val) {
					for (int i = 0; i < json_object_array_length (val); i++) {
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

						info->feedback = PianoListAppendP (info->feedback,
								feedbackSong);
					}
				}
			}
			break;
		}
	}

cleanup:
	json_object_put (j);

	return ret;
}

