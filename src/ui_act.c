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
/* needed by readline */
#include <stdio.h>
#include <readline/readline.h>

#include "ui.h"
#include "ui_act.h"

#define RETURN_IF_NO_STATION if (*curStation == NULL) { \
		BarUiMsg (MSG_ERR, "No station selected.\n"); \
		return; }

#define RETURN_IF_NO_SONG if (*curStation == NULL || *curSong == NULL) { \
		BarUiMsg (MSG_ERR, "No song playing.\n"); \
		return; }

/*	helper to _really_ skip a song (unlock mutex, quit player)
 *	@param player handle
 */
inline void BarUiDoSkipSong (struct audioPlayer *player) {
	player->doQuit = 1;
	pthread_mutex_unlock (&player->pauseMutex);
}

/*	transform station if necessary to allow changes like rename, rate, ...
 *	@param piano handle
 *	@param transform this station
 *	@return 0 = error, 1 = everything went well
 */
int BarTransformIfShared (PianoHandle_t *ph, PianoStation_t *station) {
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

	RETURN_IF_NO_STATION;

	musicId = BarUiSelectMusicId (ph);
	if (musicId != NULL) {
		if (!BarTransformIfShared (ph, *curStation)) {
			return;
		}
		BarUiMsg (MSG_INFO, "Adding music to station... ");
		BarUiPrintPianoStatus (PianoStationAddMusic (ph,
				*curStation, musicId));
		free (musicId);
	}
}

/*	ban song
 */
void BarUiActBanSong (BAR_KS_ARGS) {
	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (ph, *curStation)) {
		return;
	}
	BarUiMsg (MSG_INFO, "Banning song... ");
	if (BarUiPrintPianoStatus (PianoRateTrack (ph, *curSong,
			PIANO_RATE_BAN)) == PIANO_RET_OK) {
		BarUiDoSkipSong (player);
	}
}

/*	create new station
 */
void BarUiActCreateStation (BAR_KS_ARGS) {
	char *musicId;
	musicId = BarUiSelectMusicId (ph);
	if (musicId != NULL) {
		BarUiMsg (MSG_INFO, "Creating station... ");
		BarUiPrintPianoStatus (PianoCreateStation (ph, "mi", musicId));
		free (musicId);
	}
}

/*	add shared station by id
 */
void BarUiActAddSharedStation (BAR_KS_ARGS) {
	char *stationId = NULL;

	BarUiMsg (MSG_QUESTION, "Station id: ");
	if ((stationId = readline (NULL)) != NULL &&
			strlen (stationId) > 0) {
		BarUiMsg (MSG_INFO, "Adding shared station... ");
		BarUiPrintPianoStatus (PianoCreateStation (ph, "sh", stationId));
		free (stationId);
	}
}

/*	delete current station
 */
void BarUiActDeleteStation (BAR_KS_ARGS) {
	char yesNoBuf;

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_QUESTION, "Really delete \"%s\"? [yN] ",
			(*curStation)->name);
	read (fileno (stdin), &yesNoBuf, sizeof (yesNoBuf));
	BarUiMsg (MSG_NONE, "\n");
	if (yesNoBuf == 'y') {
		BarUiMsg (MSG_INFO, "Deleting station... ");
		if (BarUiPrintPianoStatus (PianoDeleteStation (ph,
				*curStation)) == PIANO_RET_OK) {
			BarUiDoSkipSong (player);
			PianoDestroyPlaylist (ph);
			*curSong = NULL;
			*curStation = NULL;
		}
	}
}

/*	explain pandora's song choice
 */
void BarUiActExplain (BAR_KS_ARGS) {
	char *explanation;

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_INFO, "Receiving explanation... ");
	if (BarUiPrintPianoStatus (PianoExplain (ph, *curSong,
			&explanation)) == PIANO_RET_OK) {
		BarUiMsg (MSG_INFO, "%s\n", explanation);
		free (explanation);
	}
}

/*	choose genre station and add it as shared station
 */
void BarUiActStationFromGenre (BAR_KS_ARGS) {
	/* use genre station */
	BarStationFromGenre (ph);
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
	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (ph, *curStation)) {
		return;
	}
	BarUiMsg (MSG_INFO, "Loving song... ");
	BarUiPrintPianoStatus (PianoRateTrack (ph, *curSong, PIANO_RATE_LOVE));
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

	RETURN_IF_NO_SONG;

	moveStation = BarUiSelectStation (ph, "Move song to station: ");
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
		if (BarUiPrintPianoStatus (PianoMoveSong (ph, fromStation,
				moveStation, *curSong)) == PIANO_RET_OK) {
			BarUiDoSkipSong (player);
		}
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
	char *lineBuf;

	RETURN_IF_NO_STATION;

	BarUiMsg (MSG_QUESTION, "New name: ");
	lineBuf = readline (NULL);
	if (lineBuf != NULL && strlen (lineBuf) > 0) {
		if (!BarTransformIfShared (ph, *curStation)) {
			return;
		}
		BarUiMsg (MSG_INFO, "Renaming station... ");
		BarUiPrintPianoStatus (PianoRenameStation (ph, *curStation, lineBuf));
	}
	if (lineBuf != NULL) {
		free (lineBuf);
	}
}

/*	play another station
 */
void BarUiActSelectStation (BAR_KS_ARGS) {
	BarUiDoSkipSong (player);
	PianoDestroyPlaylist (ph);
	*curSong = NULL;
	*curStation = BarUiSelectStation (ph, "Select station: ");
	if (*curStation != NULL) {
		BarUiPrintStation ((*curStation));
	}
}

/*	ban song for 1 month
 */
void BarUiActTempBanSong (BAR_KS_ARGS) {
	RETURN_IF_NO_SONG;

	BarUiMsg (MSG_INFO, "Putting song on shelf... ");
	if (BarUiPrintPianoStatus (PianoSongTired (ph, *curSong)) ==
			PIANO_RET_OK) {
		BarUiDoSkipSong (player);
	}
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
	RETURN_IF_NO_STATION;

	if ((*curStation)->isQuickMix) {
		PianoStation_t *selStation;
		while ((selStation = BarUiSelectStation (ph,
				"Toggle quickmix for station: ")) != NULL) {
			selStation->useQuickMix = !selStation->useQuickMix;
		}
		BarUiMsg (MSG_INFO, "Setting quickmix stations... ");
		BarUiPrintPianoStatus (PianoSetQuickmix (ph));
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
