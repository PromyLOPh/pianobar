/*
Copyright (c) 2008-2010
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

/* functions responding to user's keystrokes */

#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "ui.h"
#include "ui_act.h"
#include "ui_readline.h"

#define RETURN_IF_NO_STATION if (*curStation == NULL) { \
		BarUiMsg (MSG_ERR, "No station selected.\n"); \
		return; }

#define RETURN_IF_NO_SONG if (*curStation == NULL || *curSong == NULL) { \
		BarUiMsg (MSG_ERR, "No song playing.\n"); \
		return; }

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
static int BarTransformIfShared (PianoHandle_t *ph, WaitressHandle_t *waith,
		PianoStation_t *station) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	/* shared stations must be transformed */
	if (!station->isCreator) {
		BarUiMsg (MSG_INFO, "Transforming station... ");
		if (!BarUiPianoCall (ph, PIANO_REQUEST_TRANSFORM_STATION, waith,
				station, &pRet, &wRet)) {
			return 0;
		}
	}
	return 1;
}

/*	print current shortcut configuration
 */
void BarUiActHelp (BAR_KS_ARGS) {
	const char *idToDesc[] = {
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
			};
	size_t i;

	BarUiMsg (MSG_NONE, "\r");
	for (i = 0; i < BAR_KS_COUNT; i++) {
		if (idToDesc[i] != NULL) {
			BarUiMsg (MSG_LIST, "%c    %s\n", settings->keys[i], idToDesc[i]);
		}
	}
}

/*	add more music to current station
 */
void BarUiActAddMusic (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataAddSeed_t reqData;

	RETURN_IF_NO_STATION;

	reqData.musicId = BarUiSelectMusicId (ph, waith, curFd, (*curSong)->musicId);
	if (reqData.musicId != NULL) {
		if (!BarTransformIfShared (ph, waith, *curStation)) {
			return;
		}
		reqData.station = *curStation;

		BarUiMsg (MSG_INFO, "Adding music to station... ");
		BarUiPianoCall (ph, PIANO_REQUEST_ADD_SEED, waith, &reqData, &pRet,
				&wRet);

		free (reqData.musicId);

		BarUiStartEventCmd (settings, "stationaddmusic", *curStation, *curSong,
				player, pRet, wRet);
	}
}

/*	ban song
 */
void BarUiActBanSong (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (ph, waith, *curStation)) {
		return;
	}

	PianoRequestDataRateSong_t reqData;
	reqData.song = *curSong;
	reqData.rating = PIANO_RATE_BAN;

	BarUiMsg (MSG_INFO, "Banning song... ");
	if (BarUiPianoCall (ph, PIANO_REQUEST_RATE_SONG, waith, &reqData, &pRet,
			&wRet)) {
		BarUiDoSkipSong (player);
	}
	BarUiStartEventCmd (settings, "songban", *curStation, *curSong, player,
			pRet, wRet);
}

/*	create new station
 */
void BarUiActCreateStation (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataCreateStation_t reqData;

	reqData.id = BarUiSelectMusicId (ph, waith, curFd, NULL);
	if (reqData.id != NULL) {
		reqData.type = "mi";
		BarUiMsg (MSG_INFO, "Creating station... ");
		BarUiPianoCall (ph, PIANO_REQUEST_CREATE_STATION, waith, &reqData,
				&pRet, &wRet);
		free (reqData.id);
		BarUiStartEventCmd (settings, "stationcreate", *curStation, *curSong,
				player, pRet, wRet);
	}
}

/*	add shared station by id
 */
void BarUiActAddSharedStation (BAR_KS_ARGS) {
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
		BarUiPianoCall (ph, PIANO_REQUEST_CREATE_STATION, waith, &reqData,
				&pRet, &wRet);
		BarUiStartEventCmd (settings, "stationaddshared", *curStation,
				*curSong, player, pRet, wRet);
	}
}

/*	delete current station
 */
void BarUiActDeleteStation (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_QUESTION, "Really delete \"%s\"? [yN] ",
			(*curStation)->name);
	if (BarReadlineYesNo (0, curFd)) {
		BarUiMsg (MSG_INFO, "Deleting station... ");
		if (BarUiPianoCall (ph, PIANO_REQUEST_DELETE_STATION, waith,
				*curStation, &pRet, &wRet)) {
			BarUiDoSkipSong (player);
			PianoDestroyPlaylist (*curSong);
			*curSong = NULL;
			*curStation = NULL;
		}
		BarUiStartEventCmd (settings, "stationdelete", *curStation, *curSong,
				player, pRet, wRet);
	}
}

/*	explain pandora's song choice
 */
void BarUiActExplain (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataExplain_t reqData;

	RETURN_IF_NO_STATION;

	reqData.song = *curSong;

	BarUiMsg (MSG_INFO, "Receiving explanation... ");
	if (BarUiPianoCall (ph, PIANO_REQUEST_EXPLAIN, waith, &reqData, &pRet,
			&wRet)) {
		BarUiMsg (MSG_INFO, "%s\n", reqData.retExplain);
		free (reqData.retExplain);
	}
	BarUiStartEventCmd (settings, "songexplain", *curStation, *curSong, player,
			pRet, wRet);
}

/*	choose genre station and add it as shared station
 */
void BarUiActStationFromGenre (BAR_KS_ARGS) {
	/* use genre station */
	BarStationFromGenre (ph, waith, curFd);
}

/*	print verbose song information
 */
void BarUiActSongInfo (BAR_KS_ARGS) {
	RETURN_IF_NO_SONG;

	BarUiPrintStation ((*curStation));
	/* print real station if quickmix */
	BarUiPrintSong ((*curSong), (*curStation)->isQuickMix ?
			PianoFindStationById (ph->stations, (*curSong)->stationId) : NULL);
}

/*	print some debugging information
 */
void BarUiActDebug (BAR_KS_ARGS) {
	RETURN_IF_NO_SONG;

	/* print debug-alike infos */
	BarUiMsg (MSG_NONE,
			"album:\t%s\n"
			"artist:\t%s\n"
			"audioFormat:\t%i\n"
			"audioUrl:\t%s\n"
			"fileGain:\t%f\n"
			"focusTraitId:\t%s\n"
			"identity:\t%s\n"
			"matchingSeed:\t%s\n"
			"musicId:\t%s\n"
			"rating:\t%i\n"
			"stationId:\t%s\n"
			"title:\t%s\n"
			"userSeed:\t%s\n",
			(*curSong)->album, (*curSong)->artist, (*curSong)->audioFormat,
			(*curSong)->audioUrl, (*curSong)->fileGain,
			(*curSong)->focusTraitId, (*curSong)->identity,
			(*curSong)->matchingSeed, (*curSong)->musicId, (*curSong)->rating,
			(*curSong)->stationId, (*curSong)->title, (*curSong)->userSeed);
}

/*	rate current song
 */
void BarUiActLoveSong (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (ph, waith, *curStation)) {
		return;
	}

	PianoRequestDataRateSong_t reqData;
	reqData.song = *curSong;
	reqData.rating = PIANO_RATE_LOVE;

	BarUiMsg (MSG_INFO, "Loving song... ");
	BarUiPianoCall (ph, PIANO_REQUEST_RATE_SONG, waith, &reqData, &pRet,
			&wRet);
	BarUiStartEventCmd (settings, "songlove", *curStation, *curSong, player,
			pRet, wRet);
}

/*	skip song
 */
void BarUiActSkipSong (BAR_KS_ARGS) {
	BarUiDoSkipSong (player);
}

/*	move song to different station
 */
void BarUiActMoveSong (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataMoveSong_t reqData;

	reqData.step = 0;

	RETURN_IF_NO_SONG;

	reqData.to = BarUiSelectStation (ph, "Move song to station: ",
			settings->sortOrder, curFd);
	if (reqData.to != NULL) {
		/* find original station (just is case we're playing a quickmix
		 * station) */
		reqData.from = PianoFindStationById (ph->stations, (*curSong)->stationId);
		if (reqData.from == NULL) {
			BarUiMsg (MSG_ERR, "Station not found\n");
			return;
		}

		if (!BarTransformIfShared (ph, waith, reqData.from) ||
				!BarTransformIfShared (ph, waith, reqData.to)) {
			return;
		}
		BarUiMsg (MSG_INFO, "Moving song to \"%s\"... ", reqData.to->name);
		reqData.song = *curSong;
		if (BarUiPianoCall (ph, PIANO_REQUEST_MOVE_SONG, waith, &reqData,
				&pRet, &wRet)) {
			BarUiDoSkipSong (player);
		}
		BarUiStartEventCmd (settings, "songmove", *curStation, *curSong,
				player, pRet, wRet);
	}
}

/*	pause
 */
void BarUiActPause (BAR_KS_ARGS) {
	/* already locked => unlock/unpause */
	if (pthread_mutex_trylock (&player->pauseMutex) == EBUSY) {
		pthread_mutex_unlock (&player->pauseMutex);
	}
}

/*	rename current station
 */
void BarUiActRenameStation (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	char lineBuf[100];

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_QUESTION, "New name: ");
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), 0, curFd) > 0) {
		PianoRequestDataRenameStation_t reqData;
		if (!BarTransformIfShared (ph, waith, *curStation)) {
			return;
		}

		reqData.station = *curStation;
		reqData.newName = lineBuf;

		BarUiMsg (MSG_INFO, "Renaming station... ");
		BarUiPianoCall (ph, PIANO_REQUEST_RENAME_STATION, waith, &reqData,
				&pRet, &wRet);
		BarUiStartEventCmd (settings, "stationrename", *curStation, *curSong,
				player, pRet, wRet);
	}
}

/*	play another station
 */
void BarUiActSelectStation (BAR_KS_ARGS) {
	PianoStation_t *newStation = BarUiSelectStation (ph, "Select station: ",
			settings->sortOrder, curFd);
	if (newStation != NULL) {
		*curStation = newStation;
		BarUiPrintStation ((*curStation));
		BarUiDoSkipSong (player);
		PianoDestroyPlaylist (*curSong);
		*curSong = NULL;
	}
}

/*	ban song for 1 month
 */
void BarUiActTempBanSong (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	RETURN_IF_NO_SONG;

	BarUiMsg (MSG_INFO, "Putting song on shelf... ");
	if (BarUiPianoCall (ph, PIANO_REQUEST_ADD_TIRED_SONG, waith, *curSong,
			&pRet, &wRet)) {
		BarUiDoSkipSong (player);
	}
	BarUiStartEventCmd (settings, "songshelf", *curStation, *curSong, player,
			pRet, wRet);
}

/*	print upcoming songs
 */
void BarUiActPrintUpcoming (BAR_KS_ARGS) {
	RETURN_IF_NO_SONG;

	PianoSong_t *nextSong = (*curSong)->next;
	if (nextSong != NULL) {
		int i = 0;
		while (nextSong != NULL) {
			BarUiMsg (MSG_LIST, "%2i) \"%s\" by \"%s\"\n", i, nextSong->title,
					nextSong->artist);
			nextSong = nextSong->next;
			i++;
		}
	} else {
		BarUiMsg (MSG_INFO, "No songs in queue.\n");
	}
}

/*	if current station is a quickmix: select stations that are played in
 *	quickmix
 */
void BarUiActSelectQuickMix (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;

	RETURN_IF_NO_STATION;

	if ((*curStation)->isQuickMix) {
		PianoStation_t *selStation;
		while ((selStation = BarUiSelectStation (ph,
				"Toggle quickmix for station: ", settings->sortOrder,
				curFd)) != NULL) {
			selStation->useQuickMix = !selStation->useQuickMix;
		}
		BarUiMsg (MSG_INFO, "Setting quickmix stations... ");
		BarUiPianoCall (ph, PIANO_REQUEST_SET_QUICKMIX, waith, NULL, &pRet,
				&wRet);
		BarUiStartEventCmd (settings, "stationquickmixtoggle", *curStation,
				*curSong, player, pRet, wRet);
	} else {
		BarUiMsg (MSG_ERR, "Not a QuickMix station.\n");
	}
}

/*	quit
 */
void BarUiActQuit (BAR_KS_ARGS) {
	*doQuit = 1;
	BarUiDoSkipSong (player);
}

/*	song history
 */
void BarUiActHistory (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	char selectBuf[2], allowedBuf[3];
	PianoSong_t *selectedSong;

	if (*songHistory != NULL) {
		selectedSong = BarUiSelectSong (*songHistory, curFd);
		if (selectedSong != NULL) {
			/* use user-defined keybindings */
			allowedBuf[0] = settings->keys[BAR_KS_LOVE];
			allowedBuf[1] = settings->keys[BAR_KS_BAN];
			allowedBuf[2] = '\0';

			BarUiMsg (MSG_QUESTION, "%s - %s: love[%c] or ban[%c]? ",
					selectedSong->artist, selectedSong->title,
					settings->keys[BAR_KS_LOVE], settings->keys[BAR_KS_BAN]);
			BarReadline (selectBuf, sizeof (selectBuf), allowedBuf, 1, 0, curFd);

			if (selectBuf[0] == settings->keys[BAR_KS_LOVE] ||
					selectBuf[0] == settings->keys[BAR_KS_BAN]) {
				/* make sure we're transforming the _original_ station (not
				 * curStation) */
				PianoStation_t *songStation =
						PianoFindStationById (ph->stations,
								selectedSong->stationId);

				if (songStation == NULL) {
					BarUiMsg (MSG_ERR, "Station does not exist any more.\n");
					return;
				}

				if (!BarTransformIfShared (ph, waith, songStation)) {
					return;
				}

				if (selectBuf[0] == settings->keys[BAR_KS_LOVE]) {
					/* FIXME: copy&waste */
					PianoRequestDataRateSong_t reqData;
					reqData.song = selectedSong;
					reqData.rating = PIANO_RATE_LOVE;

					BarUiMsg (MSG_INFO, "Loving song... ");
					BarUiPianoCall (ph, PIANO_REQUEST_RATE_SONG, waith,
							&reqData, &pRet, &wRet);

					BarUiStartEventCmd (settings, "songlove", songStation,
							selectedSong, player, pRet, wRet);
				} else if (selectBuf[0] == settings->keys[BAR_KS_BAN]) {
					PianoRequestDataRateSong_t reqData;
					reqData.song = selectedSong;
					reqData.rating = PIANO_RATE_BAN;

					BarUiMsg (MSG_INFO, "Banning song... ");
					BarUiPianoCall (ph, PIANO_REQUEST_RATE_SONG, waith,
							&reqData, &pRet, &wRet);
					BarUiStartEventCmd (settings, "songban", songStation,
							selectedSong, player, pRet, wRet);
				} /* end if */
			} /* end if selectBuf[0] */
		} /* end if selectedSong != NULL */
	} else {
		BarUiMsg (MSG_INFO, (settings->history == 0) ? "History disabled.\n" :
				"No history yet.\n");
	}
}

/*	create song bookmark
 */
void BarUiActBookmark (BAR_KS_ARGS) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	char selectBuf[2];

	RETURN_IF_NO_SONG;

	BarUiMsg (MSG_QUESTION, "Bookmark [s]ong or [a]rtist? ");
	BarReadline (selectBuf, sizeof (selectBuf), "sa", 1, 0, curFd);
	if (selectBuf[0] == 's') {
		BarUiMsg (MSG_INFO, "Bookmarking song... ");
		BarUiPianoCall (ph, PIANO_REQUEST_BOOKMARK_SONG, waith, *curSong,
				&pRet, &wRet);
		BarUiStartEventCmd (settings, "songbookmark", *curStation, *curSong,
				player, pRet, wRet);
	} else if (selectBuf[0] == 'a') {
		BarUiMsg (MSG_INFO, "Bookmarking artist... ");
		BarUiPianoCall (ph, PIANO_REQUEST_BOOKMARK_ARTIST, waith, *curSong,
				&pRet, &wRet);
		BarUiStartEventCmd (settings, "artistbookmark", *curStation, *curSong,
				player, pRet, wRet);
	}
}

