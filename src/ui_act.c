/*
Copyright (c) 2008-2010
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

#include "ui.h"
#include "ui_act.h"
#include "ui_readline.h"

#define RETURN_IF_NO_STATION if (app->curStation == NULL) { \
		BarUiMsg (MSG_ERR, "No station selected.\n"); \
		return; }

#define RETURN_IF_NO_SONG if (app->curStation == NULL || app->playlist == NULL) { \
		BarUiMsg (MSG_ERR, "No song playing.\n"); \
		return; }

/*	standard eventcmd call
 */
#define BarUiActDefaultEventcmd(name) BarUiStartEventCmd (&app->settings, \
		name, app->curStation, app->playlist, &app->player, &app->ph, pRet, wRet)

/*	standard piano call
 */
#define BarUiActDefaultPianoCall(call, arg) BarUiPianoCall (app, \
		call, arg, &pRet, &wRet)

/*	helper to _really_ skip a song (unlock mutex, quit player)
 *	@param player handle
 */
static inline void BarUiDoSkipSong (struct audioPlayer *player) {
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
	static const char *idToDesc[] = {
			NULL,
			"love current song",
			"ban current song",
			"add music to current station",
			"create new station",
			"delete current station",
			"explain why this song is played",
			"add genre station",
			"song history",
			"print information about current song/station",
			"add shared station",
			"move song to different station",
			"next song",
			"pause/continue",
			"quit",
			"rename current station",
			"change station",
			"tired (ban song for 1 month)",
			"upcoming songs",
			"select quickmix stations",
			NULL,
			"bookmark song/artist",
			"decrease volume",
			"increase volume",
			};
	size_t i;

	BarUiMsg (MSG_NONE, "\r");
	for (i = 0; i < BAR_KS_COUNT; i++) {
		if (idToDesc[i] != NULL) {
			BarUiMsg (MSG_LIST, "%c    %s\n", app->settings.keys[i], idToDesc[i]);
		}
	}
}

/*	add more music to current station
 */
BarUiActCallback(BarUiActAddMusic) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataAddSeed_t reqData;

	RETURN_IF_NO_STATION;

	reqData.musicId = BarUiSelectMusicId (app, curFd, app->playlist->musicId);
	if (reqData.musicId != NULL) {
		if (!BarTransformIfShared (app, app->curStation)) {
			return;
		}
		reqData.station = app->curStation;

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

	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (app, app->curStation)) {
		return;
	}

	PianoRequestDataRateSong_t reqData;
	reqData.song = app->playlist;
	reqData.rating = PIANO_RATE_BAN;

	BarUiMsg (MSG_INFO, "Banning song... ");
	if (BarUiActDefaultPianoCall (PIANO_REQUEST_RATE_SONG, &reqData)) {
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

	reqData.id = BarUiSelectMusicId (app, curFd, NULL);
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
	if (BarReadline (stationId, sizeof (stationId), "0123456789", 0, 0,
			curFd) > 0) {
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

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_QUESTION, "Really delete \"%s\"? [yN] ",
			app->curStation->name);
	if (BarReadlineYesNo (0, curFd)) {
		BarUiMsg (MSG_INFO, "Deleting station... ");
		if (BarUiActDefaultPianoCall (PIANO_REQUEST_DELETE_STATION,
				app->curStation)) {
			BarUiDoSkipSong (&app->player);
			PianoDestroyPlaylist (app->playlist);
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

	RETURN_IF_NO_STATION;

	reqData.song = app->playlist;

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
	BarStationFromGenre (app, curFd);
}

/*	print verbose song information
 */
BarUiActCallback(BarUiActSongInfo) {
	RETURN_IF_NO_SONG;

	BarUiPrintStation (app->curStation);
	/* print real station if quickmix */
	BarUiPrintSong (&app->settings, app->playlist,
			app->curStation->isQuickMix ?
			PianoFindStationById (app->ph.stations, app->playlist->stationId) :
			NULL);
}

/*	print some debugging information
 */
BarUiActCallback(BarUiActDebug) {
	RETURN_IF_NO_SONG;

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
			app->playlist->album, app->playlist->artist,
			app->playlist->audioFormat,
			app->playlist->audioUrl,
			app->playlist->coverArt,
			app->playlist->fileGain,
			app->playlist->musicId, app->playlist->rating,
			app->playlist->songType, app->playlist->stationId,
			app->playlist->testStrategy, app->playlist->title,
			app->playlist->userSeed);
}

/*	rate current song
 */
BarUiActCallback(BarUiActLoveSong) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (app, app->curStation)) {
		return;
	}

	PianoRequestDataRateSong_t reqData;
	reqData.song = app->playlist;
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

	reqData.step = 0;

	RETURN_IF_NO_SONG;

	reqData.to = BarUiSelectStation (&app->ph, "Move song to station: ",
			app->settings.sortOrder, curFd);
	if (reqData.to != NULL) {
		/* find original station (just is case we're playing a quickmix
		 * station) */
		reqData.from = PianoFindStationById (app->ph.stations,
				app->playlist->stationId);
		if (reqData.from == NULL) {
			BarUiMsg (MSG_ERR, "Station not found\n");
			return;
		}

		if (!BarTransformIfShared (app, reqData.from) ||
				!BarTransformIfShared (app, reqData.to)) {
			return;
		}
		BarUiMsg (MSG_INFO, "Moving song to \"%s\"... ", reqData.to->name);
		reqData.song = app->playlist;
		if (BarUiActDefaultPianoCall (PIANO_REQUEST_MOVE_SONG, &reqData)) {
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

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_QUESTION, "New name: ");
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), 0, curFd) > 0) {
		PianoRequestDataRenameStation_t reqData;
		if (!BarTransformIfShared (app, app->curStation)) {
			return;
		}

		reqData.station = app->curStation;
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
			app->settings.sortOrder, curFd);
	if (newStation != NULL) {
		app->curStation = newStation;
		BarUiPrintStation (app->curStation);
		BarUiDoSkipSong (&app->player);
		PianoDestroyPlaylist (app->playlist);
		app->playlist = NULL;
	}
}

/*	ban song for 1 month
 */
BarUiActCallback(BarUiActTempBanSong) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	RETURN_IF_NO_SONG;

	BarUiMsg (MSG_INFO, "Putting song on shelf... ");
	if (BarUiActDefaultPianoCall (PIANO_REQUEST_ADD_TIRED_SONG, app->playlist)) {
		BarUiDoSkipSong (&app->player);
	}
	BarUiActDefaultEventcmd ("songshelf");
}

/*	print upcoming songs
 */
BarUiActCallback(BarUiActPrintUpcoming) {
	RETURN_IF_NO_SONG;

	PianoSong_t *nextSong = app->playlist->next;
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

	RETURN_IF_NO_STATION;

	if (app->curStation->isQuickMix) {
		PianoStation_t *selStation;
		while ((selStation = BarUiSelectStation (&app->ph,
				"Toggle quickmix for station: ", app->settings.sortOrder,
				curFd)) != NULL) {
			selStation->useQuickMix = !selStation->useQuickMix;
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
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	char selectBuf[2], allowedBuf[4];
	PianoSong_t *selectedSong;

	if (app->songHistory != NULL) {
		selectedSong = BarUiSelectSong (&app->settings, app->songHistory,
				curFd);
		if (selectedSong != NULL) {
			/* use user-defined keybindings */
			allowedBuf[0] = app->settings.keys[BAR_KS_LOVE];
			allowedBuf[1] = app->settings.keys[BAR_KS_BAN];
			allowedBuf[2] = app->settings.keys[BAR_KS_TIRED];
			allowedBuf[3] = '\0';

			BarUiMsg (MSG_QUESTION, "%s - %s: love[%c], ban[%c] or tired[%c]? ",
					selectedSong->artist, selectedSong->title,
					app->settings.keys[BAR_KS_LOVE],
					app->settings.keys[BAR_KS_BAN],
					app->settings.keys[BAR_KS_TIRED]);
			BarReadline (selectBuf, sizeof (selectBuf), allowedBuf, 1, 0, curFd);

			if (selectBuf[0] == app->settings.keys[BAR_KS_LOVE] ||
					selectBuf[0] == app->settings.keys[BAR_KS_BAN] ||
					selectBuf[0] == app->settings.keys[BAR_KS_TIRED]) {
				/* make sure we're transforming the _original_ station (not
				 * curStation) */
				PianoStation_t *songStation =
						PianoFindStationById (app->ph.stations,
								selectedSong->stationId);

				if (songStation == NULL) {
					BarUiMsg (MSG_ERR, "Station does not exist any more.\n");
					return;
				}

				if (!BarTransformIfShared (app, songStation)) {
					return;
				}

				if (selectBuf[0] == app->settings.keys[BAR_KS_LOVE]) {
					/* FIXME: copy&waste */
					PianoRequestDataRateSong_t reqData;
					reqData.song = selectedSong;
					reqData.rating = PIANO_RATE_LOVE;

					BarUiMsg (MSG_INFO, "Loving song... ");
					BarUiActDefaultPianoCall (PIANO_REQUEST_RATE_SONG,
							&reqData);

					BarUiStartEventCmd (&app->settings, "songlove", songStation,
							selectedSong, &app->player, &app->ph, pRet, wRet);
				} else if (selectBuf[0] == app->settings.keys[BAR_KS_BAN]) {
					PianoRequestDataRateSong_t reqData;
					reqData.song = selectedSong;
					reqData.rating = PIANO_RATE_BAN;

					BarUiMsg (MSG_INFO, "Banning song... ");
					BarUiActDefaultPianoCall (PIANO_REQUEST_RATE_SONG,
							&reqData);
					BarUiStartEventCmd (&app->settings, "songban", songStation,
							selectedSong, &app->player, &app->ph, pRet, wRet);
				} else if (selectBuf[0] == app->settings.keys[BAR_KS_TIRED]) {
					BarUiMsg (MSG_INFO, "Putting song on shelf... ");
					BarUiActDefaultPianoCall (PIANO_REQUEST_ADD_TIRED_SONG, selectedSong);

					BarUiStartEventCmd (&app->settings, "songshelf", songStation,
							selectedSong, &app->player, &app->ph, pRet, wRet);
				} /* end if */
			} /* end if selectBuf[0] */
		} /* end if selectedSong != NULL */
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

	RETURN_IF_NO_SONG;

	BarUiMsg (MSG_QUESTION, "Bookmark [s]ong or [a]rtist? ");
	BarReadline (selectBuf, sizeof (selectBuf), "sa", 1, 0, curFd);
	if (selectBuf[0] == 's') {
		BarUiMsg (MSG_INFO, "Bookmarking song... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_BOOKMARK_SONG, app->playlist);
		BarUiActDefaultEventcmd ("songbookmark");
	} else if (selectBuf[0] == 'a') {
		BarUiMsg (MSG_INFO, "Bookmarking artist... ");
		BarUiActDefaultPianoCall (PIANO_REQUEST_BOOKMARK_ARTIST,
				app->playlist);
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

