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

#include "../config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include "piano_private.h"
#include "piano.h"

/*	initialize piano handle
 *	@param piano handle
 *	@return nothing
 */
PianoReturn_t PianoInit (PianoHandle_t *ph, const char *partnerUser,
		const char *partnerPassword, const char *device, const char *inkey,
		const char *outkey) {
	memset (ph, 0, sizeof (*ph));
	ph->partner.user = strdup (partnerUser);
	ph->partner.password = strdup (partnerPassword);
	ph->partner.device = strdup (device);

	if (gcry_cipher_open (&ph->partner.in, GCRY_CIPHER_BLOWFISH,
			GCRY_CIPHER_MODE_ECB, 0) != GPG_ERR_NO_ERROR) {
		return PIANO_RET_GCRY_ERR;
	}
	if (gcry_cipher_setkey (ph->partner.in, (const unsigned char *) inkey,
			strlen (inkey)) != GPG_ERR_NO_ERROR) {
		return PIANO_RET_GCRY_ERR;
	}

	if (gcry_cipher_open (&ph->partner.out, GCRY_CIPHER_BLOWFISH,
			GCRY_CIPHER_MODE_ECB, 0) != GPG_ERR_NO_ERROR) {
		return PIANO_RET_GCRY_ERR;
	}
	if (gcry_cipher_setkey (ph->partner.out, (const unsigned char *) outkey,
			strlen (outkey)) != GPG_ERR_NO_ERROR) {
		return PIANO_RET_GCRY_ERR;
	}

	return PIANO_RET_OK;
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
		curArtist = (PianoArtist_t *) curArtist->head.next;
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
		curStation = (PianoStation_t *) curStation->head.next;
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
		curSong = (PianoSong_t *) curSong->head.next;
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
		curGenre = (PianoGenre_t *) curGenre->head.next;
		free (lastGenre);
	}
}

/*	destroy user information
 */
void PianoDestroyUserInfo (PianoUserInfo_t *user) {
	free (user->authToken);
	free (user->listenerId);
}

/*	destroy partner
 */
static void PianoDestroyPartner (PianoPartner_t *partner) {
	free (partner->user);
	free (partner->password);
	free (partner->device);
	free (partner->authToken);
	gcry_cipher_close (partner->in);
	gcry_cipher_close (partner->out);
	memset (partner, 0, sizeof (*partner));
}

/*	frees the whole piano handle structure
 *	@param piano handle
 *	@return nothing
 */
void PianoDestroy (PianoHandle_t *ph) {
	PianoDestroyUserInfo (&ph->user);
	PianoDestroyStations (ph->stations);
	PianoDestroyPartner (&ph->partner);
	/* destroy genre stations */
	PianoGenreCategory_t *curGenreCat = ph->genreStations, *lastGenreCat;
	while (curGenreCat != NULL) {
		PianoDestroyGenres (curGenreCat->genres);
		free (curGenreCat->name);
		lastGenreCat = curGenreCat;
		curGenreCat = (PianoGenreCategory_t *) curGenreCat->head.next;
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

/*	get station from list by id
 *	@param search here
 *	@param search for this
 *	@return the first station structure matching the given id
 */
PianoStation_t *PianoFindStationById (PianoStation_t * const stations,
		const char * const searchStation) {
	assert (stations != NULL);

	if (searchStation == NULL) {
		return NULL;
	}

	PianoStation_t *currStation = stations;
	PianoListForeachP (currStation) {
		if (strcmp (currStation->id, searchStation) == 0) {
			return currStation;
		}
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

		case PIANO_RET_INVALID_LOGIN:
			return "Wrong email address or password.";
			break;

		case PIANO_RET_QUALITY_UNAVAILABLE:
			return "Selected audio quality is not available.";
			break;

		case PIANO_RET_GCRY_ERR:
			return "libgcrypt initialization failed.";
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

		case PIANO_RET_P_LICENSING_RESTRICTIONS:
			return "Pandora is not available in your country. "
					"Set up a control proxy (see manpage).";
			break;

		case PIANO_RET_P_PARTNER_NOT_AUTHORIZED:
			return "Invalid partner credentials.";
			break;

		case PIANO_RET_P_LISTENER_NOT_AUTHORIZED:
			return "Listener not authorized.";
			break;

		case PIANO_RET_P_RATE_LIMIT:
			return "Access denied. Try again later.";
			break;

		default:
			return "No error message available.";
			break;
	}
}

