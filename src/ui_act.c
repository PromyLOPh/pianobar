/*
Copyright (c) 2008-2009
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
static int BarTransformIfShared (PianoHandle_t *ph, PianoStation_t *station) {
	/* shared stations must be transformed */
	if (!station->isCreator) {
		BarUiMsg (MSG_INFO, "Transforming station... ");
		if (BarUiPrintPianoStatus (PianoTransformShared (ph, station)) !=
				PIANO_RET_OK) {
			return 0;
		}
	}
	return 1;
}

/*	print current shortcut configuration
 */
void BarUiActHelp (BAR_KS_ARGS) {
	BarKeyShortcut_t *curShortcut = settings->keys;

	BarUiMsg (MSG_NONE, "\r");
	while (curShortcut != NULL) {
		if (curShortcut->description != NULL) {
			BarUiMsg (MSG_LIST, "%c    %s\n", curShortcut->key,
					curShortcut->description);
		}
		curShortcut = curShortcut->next;
	}
}

/*	add more music to current station
 */
void BarUiActAddMusic (BAR_KS_ARGS) {
	char *musicId;
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_STATION;

	musicId = BarUiSelectMusicId (ph, curFd, (*curSong)->musicId);
	if (musicId != NULL) {
		if (!BarTransformIfShared (ph, *curStation)) {
			return;
		}
		BarUiMsg (MSG_INFO, "Adding music to station... ");
		pRet = BarUiPrintPianoStatus (PianoStationAddMusic (ph, *curStation, musicId));
		free (musicId);

		BarUiStartEventCmd (settings, "stationaddmusic", *curStation, *curSong, pRet);
	}
}

/*	ban song
 */
void BarUiActBanSong (BAR_KS_ARGS) {
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (ph, *curStation)) {
		return;
	}
	BarUiMsg (MSG_INFO, "Banning song... ");
	if ((pRet = BarUiPrintPianoStatus (PianoRateTrack (ph, *curSong,
			PIANO_RATE_BAN))) == PIANO_RET_OK) {
		BarUiDoSkipSong (player);
	}
	BarUiStartEventCmd (settings, "songban", *curStation, *curSong, pRet);
}

/*	create new station
 */
void BarUiActCreateStation (BAR_KS_ARGS) {
	char *musicId;
	PianoReturn_t pRet = PIANO_RET_ERR;

	musicId = BarUiSelectMusicId (ph, curFd, NULL);
	if (musicId != NULL) {
		BarUiMsg (MSG_INFO, "Creating station... ");
		pRet = BarUiPrintPianoStatus (PianoCreateStation (ph, "mi", musicId));
		free (musicId);
		BarUiStartEventCmd (settings, "stationcreate", *curStation, *curSong, pRet);
	}
}

/*	add shared station by id
 */
void BarUiActAddSharedStation (BAR_KS_ARGS) {
	char stationId[50];
	PianoReturn_t pRet = PIANO_RET_ERR;

	BarUiMsg (MSG_QUESTION, "Station id: ");
	if (BarReadline (stationId, sizeof (stationId), "0123456789", 0, 0,
			curFd) > 0) {
		BarUiMsg (MSG_INFO, "Adding shared station... ");
		pRet = BarUiPrintPianoStatus (PianoCreateStation (ph, "sh",
				(char *) stationId));
		BarUiStartEventCmd (settings, "stationaddshared", *curStation, *curSong, pRet);
	}
}

/*	delete current station
 */
void BarUiActDeleteStation (BAR_KS_ARGS) {
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_QUESTION, "Really delete \"%s\"? [yN] ",
			(*curStation)->name);
	if (BarReadlineYesNo (0, curFd)) {
		BarUiMsg (MSG_INFO, "Deleting station... ");
		if ((pRet = BarUiPrintPianoStatus (PianoDeleteStation (ph,
				*curStation))) == PIANO_RET_OK) {
			BarUiDoSkipSong (player);
			PianoDestroyPlaylist (*curSong);
			*curSong = NULL;
			*curStation = NULL;
		}
		BarUiStartEventCmd (settings, "stationdelete", *curStation, *curSong, pRet);
	}
}

/*	explain pandora's song choice
 */
void BarUiActExplain (BAR_KS_ARGS) {
	char *explanation;
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_INFO, "Receiving explanation... ");
	if ((pRet = BarUiPrintPianoStatus (PianoExplain (ph, *curSong,
			&explanation))) == PIANO_RET_OK) {
		BarUiMsg (MSG_INFO, "%s\n", explanation);
		free (explanation);
	}
	BarUiStartEventCmd (settings, "songexplain", *curStation, *curSong, pRet);
}

/*	choose genre station and add it as shared station
 */
void BarUiActStationFromGenre (BAR_KS_ARGS) {
	/* use genre station */
	BarStationFromGenre (ph, curFd);
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
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (ph, *curStation)) {
		return;
	}
	BarUiMsg (MSG_INFO, "Loving song... ");
	pRet = BarUiPrintPianoStatus (PianoRateTrack (ph, *curSong, PIANO_RATE_LOVE));
	BarUiStartEventCmd (settings, "songlove", *curStation, *curSong, pRet);
}

/*	skip song
 */
void BarUiActSkipSong (BAR_KS_ARGS) {
	BarUiDoSkipSong (player);
}

/*	move song to different station
 */
void BarUiActMoveSong (BAR_KS_ARGS) {
	PianoStation_t *moveStation, *fromStation;
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_SONG;

	moveStation = BarUiSelectStation (ph, "Move song to station: ", curFd);
	if (moveStation != NULL) {
		if (!BarTransformIfShared (ph, *curStation) ||
				!BarTransformIfShared (ph, moveStation)) {
			return;
		}
		BarUiMsg (MSG_INFO, "Moving song to \"%s\"... ", moveStation->name);
		fromStation = PianoFindStationById (ph->stations, (*curSong)->stationId);
		if (fromStation == NULL) {
			BarUiMsg (MSG_ERR, "Station not found\n");
			return;
		}
		if ((pRet = BarUiPrintPianoStatus (PianoMoveSong (ph, fromStation,
				moveStation, *curSong))) == PIANO_RET_OK) {
			BarUiDoSkipSong (player);
		}
		BarUiStartEventCmd (settings, "songmove", *curStation, *curSong, pRet);
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
	char lineBuf[100];
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_QUESTION, "New name: ");
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), 0, curFd) > 0) {
		if (!BarTransformIfShared (ph, *curStation)) {
			return;
		}
		BarUiMsg (MSG_INFO, "Renaming station... ");
		pRet = BarUiPrintPianoStatus (PianoRenameStation (ph, *curStation,
				(char *) lineBuf));
		BarUiStartEventCmd (settings, "stationrename", *curStation, *curSong, pRet);
	}
}

/*	play another station
 */
void BarUiActSelectStation (BAR_KS_ARGS) {
	BarUiDoSkipSong (player);
	PianoDestroyPlaylist (*curSong);
	*curSong = NULL;
	*curStation = BarUiSelectStation (ph, "Select station: ", curFd);
	if (*curStation != NULL) {
		BarUiPrintStation ((*curStation));
	}
}

/*	ban song for 1 month
 */
void BarUiActTempBanSong (BAR_KS_ARGS) {
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_SONG;

	BarUiMsg (MSG_INFO, "Putting song on shelf... ");
	if ((pRet = BarUiPrintPianoStatus (PianoSongTired (ph, *curSong))) ==
			PIANO_RET_OK) {
		BarUiDoSkipSong (player);
	}
	BarUiStartEventCmd (settings, "songshelf", *curStation, *curSong, pRet);
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
	PianoReturn_t pRet = PIANO_RET_ERR;

	RETURN_IF_NO_STATION;

	if ((*curStation)->isQuickMix) {
		PianoStation_t *selStation;
		while ((selStation = BarUiSelectStation (ph,
				"Toggle quickmix for station: ", curFd)) != NULL) {
			selStation->useQuickMix = !selStation->useQuickMix;
		}
		BarUiMsg (MSG_INFO, "Setting quickmix stations... ");
		pRet = BarUiPrintPianoStatus (PianoSetQuickmix (ph));
		BarUiStartEventCmd (settings, "stationquickmixtoggle", *curStation,
				*curSong, pRet);
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
	char selectBuf[2];
	PianoSong_t *selectedSong;

	if (*songHistory != NULL) {
		selectedSong = BarUiSelectSong (*songHistory, curFd);
		if (selectedSong != NULL) {
			BarUiMsg (MSG_QUESTION, "%s - %s: [l]ove or [b]an? ",
					selectedSong->artist, selectedSong->title);
			BarReadline (selectBuf, sizeof (selectBuf), "lbs", 1, 0, curFd);
			if (selectBuf[0] == 'l' || selectBuf[0] == 'b') {
				PianoReturn_t pRet = PIANO_RET_ERR;
				/* make sure we're transforming the _original_ station (not
				 * curStation) */
				PianoStation_t *songStation =
						PianoFindStationById (ph->stations,
								selectedSong->stationId);

				if (songStation == NULL) {
					BarUiMsg (MSG_ERR, "Station does not exist any more.\n");
					return;
				}

				if (!BarTransformIfShared (ph, songStation)) {
					return;
				}

				switch (selectBuf[0]) {
					case 'l':
						/* love */
						/* FIXME: copy&waste */
						BarUiMsg (MSG_INFO, "Loving song... ");
						pRet = BarUiPrintPianoStatus (PianoRateTrack (ph,
								selectedSong, PIANO_RATE_LOVE));
						BarUiStartEventCmd (settings, "songlove", songStation,
								selectedSong, pRet);
						break;
					
					case 'b':
						/* ban */
						BarUiMsg (MSG_INFO, "Banning song... ");
						pRet = BarUiPrintPianoStatus (PianoRateTrack (ph,
								selectedSong, PIANO_RATE_BAN));
						BarUiStartEventCmd (settings, "songban", songStation,
								selectedSong, pRet);
						break;
				} /* end switch */
			} /* end if selectBuf[0] */
		} /* end if selectedSong != NULL */
	} else {
		BarUiMsg (MSG_INFO, (settings->history == 0) ? "History disabled.\n" :
				"No history yet.\n");
	}
}
