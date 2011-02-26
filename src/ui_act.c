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

/* functions responding to user's keystrokes */

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#include "ui.h"
#include "ui_readline.h"
#include "ui_dispatch.h"

/*	standard eventcmd call
 */
#define BarUiActDefaultEventcmd(name) BarUiStartEventCmd (&app->settings, \
		name, app->curStation, app->playlist, &app->player, app->ph.stations, \
		pRet, wRet)

/*	standard piano call
 */
#define BarUiActDefaultPianoCall(call, arg) BarUiPianoCall (app, \
		call, arg, &pRet, &wRet)

/*	helper to _really_ skip a song (unlock mutex, quit player)
 *	@param player handle
 */
static inline void BarUiDoSkipSong (struct audioPlayer *player) {
	assert (player != NULL);

	player->doQuit = 1;
	pthread_mutex_unlock (&player->pauseMutex);
}

/*	transform station if necessary to allow changes like rename, rate, ...
 *	@param piano handle
 *	@param transform this station
 *	@return 0 = error, 1 = everything went well
 */
static int BarTransformIfShared (BarApp_t *app, PianoStation_t *station) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	assert (station != NULL);

	/* shared stations must be transformed */
	if (!station->isCreator) {
		BarUiMsg (MSG_INFO, "Transforming station... ");
		if (!BarUiPianoCall (app, PIANO_REQUEST_TRANSFORM_STATION, station,
				&pRet, &wRet)) {
			return 0;
		}
	}
	return 1;
}

/*	print current shortcut configuration
 */
BarUiActCallback(BarUiActHelp) {
	BarUiMsg (MSG_NONE, "\r");
	for (size_t i = 0; i < BAR_KS_COUNT; i++) {
		if (dispatchActions[i].helpText != NULL &&
				(context & dispatchActions[i].context) == dispatchActions[i].context &&
				app->settings.keys[i] != BAR_KS_DISABLED) {
			BarUiMsg (MSG_LIST, "%c    %s\n", app->settings.keys[i],
					dispatchActions[i].helpText);
		}
	}
}

/*	add more music to current station
 */
BarUiActCallback(BarUiActAddMusic) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataAddSeed_t reqData;

	assert (selStation != NULL);

	reqData.musicId = BarUiSelectMusicId (app, app->playlist->musicId);
	if (reqData.musicId != NULL) {
		if (!BarTransformIfShared (app, selStation)) {
			return;
		}
		reqData.station = selStation;

		BarUiMsg (MSG_INFO, "Adding music to station... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_ADD_SEED, &reqData);

		free (reqData.musicId);

		BarUiActDefaultEventcmd ("stationaddmusic");
	}
}

/*	ban song
 */
BarUiActCallback(BarUiActBanSong) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	assert (selStation != NULL);
	assert (selSong != NULL);

	if (!BarTransformIfShared (app, selStation)) {
		return;
	}

	PianoRequestDataRateSong_t reqData;
	reqData.song = selSong;
	reqData.rating = PIANO_RATE_BAN;

	BarUiMsg (MSG_INFO, "Banning song... ");
	if (BarUiActDefaultPianoCall (PIANO_REQUEST_RATE_SONG, &reqData) &&
			selSong == app->playlist) {
		BarUiDoSkipSong (&app->player);
	}
	BarUiActDefaultEventcmd ("songban");
}

/*	create new station
 */
BarUiActCallback(BarUiActCreateStation) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataCreateStation_t reqData;

	reqData.id = BarUiSelectMusicId (app, NULL);
	if (reqData.id != NULL) {
		reqData.type = "mi";
		BarUiMsg (MSG_INFO, "Creating station... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_CREATE_STATION, &reqData);
		free (reqData.id);
		BarUiActDefaultEventcmd ("stationcreate");
	}
}

/*	add shared station by id
 */
BarUiActCallback(BarUiActAddSharedStation) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataCreateStation_t reqData;
	char stationId[50];

	BarUiMsg (MSG_QUESTION, "Station id: ");
	if (BarReadline (stationId, sizeof (stationId), "0123456789", &app->input,
			BAR_RL_DEFAULT, -1) > 0) {
		reqData.id = stationId;
		reqData.type = "sh";
		BarUiMsg (MSG_INFO, "Adding shared station... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_CREATE_STATION, &reqData);
		BarUiActDefaultEventcmd ("stationaddshared");
	}
}

/*	delete current station
 */
BarUiActCallback(BarUiActDeleteStation) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	assert (selStation != NULL);

	BarUiMsg (MSG_QUESTION, "Really delete \"%s\"? [yN] ",
			app->curStation->name);
	if (BarReadlineYesNo (false, &app->input)) {
		BarUiMsg (MSG_INFO, "Deleting station... ");
		if (BarUiActDefaultPianoCall (PIANO_REQUEST_DELETE_STATION,
				selStation) && selStation == app->curStation) {
			BarUiDoSkipSong (&app->player);
			PianoDestroyPlaylist (app->playlist->next);
			BarUiHistoryPrepend (app, app->playlist);
			app->playlist = NULL;
			app->curStation = NULL;
		}
		BarUiActDefaultEventcmd ("stationdelete");
	}
}

/*	explain pandora's song choice
 */
BarUiActCallback(BarUiActExplain) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataExplain_t reqData;

	assert (selSong != NULL);

	reqData.song = selSong;

	BarUiMsg (MSG_INFO, "Receiving explanation... ");
	if (BarUiActDefaultPianoCall (PIANO_REQUEST_EXPLAIN, &reqData)) {
		BarUiMsg (MSG_INFO, "%s\n", reqData.retExplain);
		free (reqData.retExplain);
	}
	BarUiActDefaultEventcmd ("songexplain");
}

/*	choose genre station and add it as shared station
 */
BarUiActCallback(BarUiActStationFromGenre) {
	/* use genre station */
	BarStationFromGenre (app);
}

/*	print verbose song information
 */
BarUiActCallback(BarUiActSongInfo) {
	assert (selStation != NULL);
	assert (selSong != NULL);

	BarUiPrintStation (selStation);
	/* print real station if quickmix */
	BarUiPrintSong (&app->settings, selSong,
			selStation->isQuickMix ?
			PianoFindStationById (app->ph.stations, selSong->stationId) :
			NULL);
}

/*	print some debugging information
 */
BarUiActCallback(BarUiActDebug) {
	assert (selSong != NULL);

	/* print debug-alike infos */
	BarUiMsg (MSG_NONE,
			"album:\t%s\n"
			"artist:\t%s\n"
			"audioFormat:\t%i\n"
			"audioUrl:\t%s\n"
			"coverArt:\t%s\n"
			"fileGain:\t%f\n"
			"musicId:\t%s\n"
			"rating:\t%i\n"
			"songType:\t%i\n"
			"stationId:\t%s\n"
			"testStrategy:\t%i\n"
			"title:\t%s\n"
			"userSeed:\t%s\n",
			selSong->album,
			selSong->artist,
			selSong->audioFormat,
			selSong->audioUrl,
			selSong->coverArt,
			selSong->fileGain,
			selSong->musicId,
			selSong->rating,
			selSong->songType,
			selSong->stationId,
			selSong->testStrategy,
			selSong->title,
			selSong->userSeed);
}

/*	rate current song
 */
BarUiActCallback(BarUiActLoveSong) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	assert (selStation != NULL);
	assert (selSong != NULL);

	if (!BarTransformIfShared (app, selStation)) {
		return;
	}

	PianoRequestDataRateSong_t reqData;
	reqData.song = selSong;
	reqData.rating = PIANO_RATE_LOVE;

	BarUiMsg (MSG_INFO, "Loving song... ");
	BarUiActDefaultPianoCall (PIANO_REQUEST_RATE_SONG, &reqData);
	BarUiActDefaultEventcmd ("songlove");
}

/*	skip song
 */
BarUiActCallback(BarUiActSkipSong) {
	BarUiDoSkipSong (&app->player);
}

/*	move song to different station
 */
BarUiActCallback(BarUiActMoveSong) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataMoveSong_t reqData;

	assert (selSong != NULL);

	reqData.step = 0;

	reqData.to = BarUiSelectStation (&app->ph, "Move song to station: ",
			app->settings.sortOrder, &app->input);
	if (reqData.to != NULL) {
		/* find original station (just is case we're playing a quickmix
		 * station) */
		reqData.from = PianoFindStationById (app->ph.stations,
				selSong->stationId);
		if (reqData.from == NULL) {
			BarUiMsg (MSG_ERR, "Station not found\n");
			return;
		}

		if (!BarTransformIfShared (app, reqData.from) ||
				!BarTransformIfShared (app, reqData.to)) {
			return;
		}
		BarUiMsg (MSG_INFO, "Moving song to \"%s\"... ", reqData.to->name);
		reqData.song = selSong;
		if (BarUiActDefaultPianoCall (PIANO_REQUEST_MOVE_SONG, &reqData) &&
				selSong == app->playlist) {
			BarUiDoSkipSong (&app->player);
		}
		BarUiActDefaultEventcmd ("songmove");
	}
}

/*	pause
 */
BarUiActCallback(BarUiActPause) {
	/* already locked => unlock/unpause */
	if (pthread_mutex_trylock (&app->player.pauseMutex) == EBUSY) {
		pthread_mutex_unlock (&app->player.pauseMutex);
	}
}

/*	rename current station
 */
BarUiActCallback(BarUiActRenameStation) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	char lineBuf[100];

	assert (selStation != NULL);

	BarUiMsg (MSG_QUESTION, "New name: ");
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), &app->input, BAR_RL_DEFAULT) > 0) {
		PianoRequestDataRenameStation_t reqData;
		if (!BarTransformIfShared (app, selStation)) {
			return;
		}

		reqData.station = selStation;
		reqData.newName = lineBuf;

		BarUiMsg (MSG_INFO, "Renaming station... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_RENAME_STATION, &reqData);
		BarUiActDefaultEventcmd ("stationrename");
	}
}

/*	play another station
 */
BarUiActCallback(BarUiActSelectStation) {
	PianoStation_t *newStation = BarUiSelectStation (&app->ph, "Select station: ",
			app->settings.sortOrder, &app->input);
	if (newStation != NULL) {
		app->curStation = newStation;
		BarUiPrintStation (app->curStation);
		BarUiDoSkipSong (&app->player);
		if (app->playlist != NULL) {
			PianoDestroyPlaylist (app->playlist->next);
			BarUiHistoryPrepend (app, app->playlist);
			app->playlist = NULL;
		}
	}
}

/*	ban song for 1 month
 */
BarUiActCallback(BarUiActTempBanSong) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	assert (selSong != NULL);

	BarUiMsg (MSG_INFO, "Putting song on shelf... ");
	if (BarUiActDefaultPianoCall (PIANO_REQUEST_ADD_TIRED_SONG, selSong) &&
			selSong == app->playlist) {
		BarUiDoSkipSong (&app->player);
	}
	BarUiActDefaultEventcmd ("songshelf");
}

/*	print upcoming songs
 */
BarUiActCallback(BarUiActPrintUpcoming) {
	assert (selSong != NULL);

	PianoSong_t *nextSong = selSong->next;
	if (nextSong != NULL) {
		BarUiListSongs (&app->settings, nextSong);
	} else {
		BarUiMsg (MSG_INFO, "No songs in queue.\n");
	}
}

/*	if current station is a quickmix: select stations that are played in
 *	quickmix
 */
BarUiActCallback(BarUiActSelectQuickMix) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	assert (selStation != NULL);

	if (selStation->isQuickMix) {
		PianoStation_t *toggleStation;
		while ((toggleStation = BarUiSelectStation (&app->ph,
				"Toggle quickmix for station: ", app->settings.sortOrder,
				&app->input)) != NULL) {
			toggleStation->useQuickMix = !toggleStation->useQuickMix;
		}
		BarUiMsg (MSG_INFO, "Setting quickmix stations... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_SET_QUICKMIX, NULL);
		BarUiActDefaultEventcmd ("stationquickmixtoggle");
	} else {
		BarUiMsg (MSG_ERR, "Not a QuickMix station.\n");
	}
}

/*	quit
 */
BarUiActCallback(BarUiActQuit) {
	app->doQuit = 1;
	BarUiDoSkipSong (&app->player);
}

/*	song history
 */
BarUiActCallback(BarUiActHistory) {
	char buf[2];
	PianoSong_t *histSong;

	if (app->songHistory != NULL) {
		histSong = BarUiSelectSong (&app->settings, app->songHistory,
				&app->input);
		if (histSong != NULL) {
			BarKeyShortcutId_t action;
			PianoStation_t *songStation = PianoFindStationById (app->ph.stations,
					histSong->stationId);

			if (songStation == NULL) {
				BarUiMsg (MSG_ERR, "Station does not exist any more.\n");
				return;
			}

			do {
				action = BAR_KS_COUNT;

				BarUiMsg (MSG_QUESTION, "What to do with this song? ");

				if (BarReadline (buf, sizeof (buf), NULL, &app->input,
						BAR_RL_FULLRETURN, -1) > 0) {
					/* actions assume that selStation is the song's original
					 * station */
					action = BarUiDispatch (app, buf[0], songStation, histSong,
							false, BAR_DC_UNDEFINED);
				}
			} while (action == BAR_KS_HELP);
		} /* end if histSong != NULL */
	} else {
		BarUiMsg (MSG_INFO, (app->settings.history == 0) ? "History disabled.\n" :
				"No history yet.\n");
	}
}

/*	create song bookmark
 */
BarUiActCallback(BarUiActBookmark) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	char selectBuf[2];

	assert (selSong != NULL);

	BarUiMsg (MSG_QUESTION, "Bookmark [s]ong or [a]rtist? ");
	BarReadline (selectBuf, sizeof (selectBuf), "sa", &app->input,
			BAR_RL_FULLRETURN, -1);
	if (selectBuf[0] == 's') {
		BarUiMsg (MSG_INFO, "Bookmarking song... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_BOOKMARK_SONG, selSong);
		BarUiActDefaultEventcmd ("songbookmark");
	} else if (selectBuf[0] == 'a') {
		BarUiMsg (MSG_INFO, "Bookmarking artist... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_BOOKMARK_ARTIST, selSong);
		BarUiActDefaultEventcmd ("artistbookmark");
	}
}

/*	decrease volume
 */
BarUiActCallback(BarUiActVolDown) {
	--app->settings.volume;
	/* FIXME: assuming unsigned integer store is atomic operation */
	app->player.scale = BarPlayerCalcScale (app->player.gain + app->settings.volume);
}

/*	increase volume
 */
BarUiActCallback(BarUiActVolUp) {
	++app->settings.volume;
	/* FIXME: assuming unsigned integer store is atomic operation */
	app->player.scale = BarPlayerCalcScale (app->player.gain + app->settings.volume);
}

