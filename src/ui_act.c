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
/* needed by readline */
#include <stdio.h>
#include <readline/readline.h>

#include "ui.h"
#include "ui_act.h"

#define RETURN_IF_NO_STATION if (*curStation == NULL) { \
		BarUiMsg ("No station selected.\n"); \
		return; }

#define RETURN_IF_NO_SONG if (*curStation == NULL || *curSong == NULL) { \
		BarUiMsg ("No song playing.\n"); \
		return; }

/*	transform station if necessary to allow changes like rename, rate, ...
 *	@param piano handle
 *	@param transform this station
 *	@return 0 = error, 1 = everything went well
 */
int BarTransformIfShared (PianoHandle_t *ph, PianoStation_t *station) {
	/* shared stations must be transformed */
	if (!station->isCreator) {
		BarUiMsg ("Transforming station... ");
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

	printf ("\r");
	while (curShortcut != NULL) {
		if (curShortcut->description != NULL) {
			printf ("%c\t%s\n", curShortcut->key, curShortcut->description);
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
		BarUiMsg ("Adding music to station... ");
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
	BarUiMsg ("Banning song... ");
	if (BarUiPrintPianoStatus (PianoRateTrack (ph, *curSong,
			PIANO_RATE_BAN)) == PIANO_RET_OK) {
		player->doQuit = 1;
	}
}

/*	create new station
 */
void BarUiActCreateStation (BAR_KS_ARGS) {
	char *musicId;
	musicId = BarUiSelectMusicId (ph);
	if (musicId != NULL) {
		BarUiMsg ("Creating station... ");
		BarUiPrintPianoStatus (PianoCreateStation (ph, "mi", musicId));
		free (musicId);
	}
}

/*	delete current station
 */
void BarUiActDeleteStation (BAR_KS_ARGS) {
	char yesNoBuf;

	RETURN_IF_NO_STATION;

	printf ("Really delete \"%s\"? [yn]\n", (*curStation)->name);
	read (fileno (stdin), &yesNoBuf, sizeof (yesNoBuf));
	if (yesNoBuf == 'y') {
		BarUiMsg ("Deleting station... ");
		if (BarUiPrintPianoStatus (PianoDeleteStation (ph,
				*curStation)) == PIANO_RET_OK) {
			player->doQuit = 1;
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

	BarUiMsg ("Receiving explanation... ");
	if (BarUiPrintPianoStatus (PianoExplain (ph, *curSong,
			&explanation)) == PIANO_RET_OK) {
		printf ("%s\n", explanation);
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

	/* print debug-alike infos */
	printf ("Song infos:\n"
			"album:\t%s\n"
			"artist:\t%s\n"
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
			(*curSong)->album, (*curSong)->artist, (*curSong)->audioUrl,
			(*curSong)->fileGain, (*curSong)->focusTraitId,
			(*curSong)->identity, (*curSong)->matchingSeed,
			(*curSong)->musicId, (*curSong)->rating,
			(*curSong)->stationId, (*curSong)->title,
			(*curSong)->userSeed);
}

/*	rate current song
 */
void BarUiActLoveSong (BAR_KS_ARGS) {
	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (ph, *curStation)) {
		return;
	}
	BarUiMsg ("Loving song... ");
	BarUiPrintPianoStatus (PianoRateTrack (ph, *curSong, PIANO_RATE_LOVE));
}

/*	skip song
 */
void BarUiActSkipSong (BAR_KS_ARGS) {
	player->doQuit = 1;
}

/*	move song to different station
 */
void BarUiActMoveSong (BAR_KS_ARGS) {
	PianoStation_t *moveStation;

	RETURN_IF_NO_SONG;

	moveStation = BarUiSelectStation (ph, "Move song to station: ");
	if (moveStation != NULL) {
		if (!BarTransformIfShared (ph, *curStation) ||
				!BarTransformIfShared (ph, moveStation)) {
			return;
		}
		printf ("Moving song to \"%s\"... ", moveStation->name);
		fflush (stdout);
		if (BarUiPrintPianoStatus (PianoMoveSong (ph, *curStation,
				moveStation, *curSong)) == PIANO_RET_OK) {
			player->doQuit = 1;
		}
	}
}

/*	pause
 */
void BarUiActPause (BAR_KS_ARGS) {
	player->doPause = !player->doPause;
}

/*	rename current station
 */
void BarUiActRenameStation (BAR_KS_ARGS) {
	char *lineBuf;

	RETURN_IF_NO_STATION;

	lineBuf = readline ("New name: ");
	if (lineBuf != NULL && strlen (lineBuf) > 0) {
		if (!BarTransformIfShared (ph, *curStation)) {
			return;
		}
		BarUiMsg ("Renaming station... ");
		BarUiPrintPianoStatus (PianoRenameStation (ph, *curStation, lineBuf));
	}
	if (lineBuf != NULL) {
		free (lineBuf);
	}
}

/*	play another station
 */
void BarUiActSelectStation (BAR_KS_ARGS) {
	player->doQuit = 1;
	PianoDestroyPlaylist (ph);
	*curSong = NULL;
	*curStation = BarUiSelectStation (ph, "Select station: ");
	if (*curStation != NULL) {
		printf ("Changed station to %s\n", (*curStation)->name);
	}
}

/*	ban song for 1 month
 */
void BarUiActTempBanSong (BAR_KS_ARGS) {
	RETURN_IF_NO_SONG;

	if (!BarTransformIfShared (ph, *curStation)) {
		return;
	}
	BarUiMsg ("Putting song on shelf... ");
	if (BarUiPrintPianoStatus (PianoSongTired (ph, *curSong)) ==
			PIANO_RET_OK) {
		player->doQuit = 1;
	}
}

/*	print upcoming songs
 */
void BarUiActPrintUpcoming (BAR_KS_ARGS) {
	RETURN_IF_NO_SONG;

	PianoSong_t *nextSong = (*curSong)->next;
	if (nextSong != NULL) {
		int i = 0;
		BarUiMsg ("Next songs:\n");
		while (nextSong != NULL) {
			printf ("%2i) \"%s\" by \"%s\"\n", i, nextSong->title,
					nextSong->artist);
			nextSong = nextSong->next;
			i++;
		}
	} else {
		BarUiMsg ("No songs in queue.\n");
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
		BarUiMsg ("Setting quickmix stations... ");
		BarUiPrintPianoStatus (PianoSetQuickmix (ph));
	} else {
		BarUiMsg ("Not a QuickMix station.\n");
	}
}

/*	quit
 */
void BarUiActQuit (BAR_KS_ARGS) {
	*doQuit = 1;
	player->doQuit = 1;
}
