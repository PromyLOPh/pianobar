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

/* everything that interacts with the user */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>

#include "ui.h"

/*	output message and flush stdout
 *	@param message
 */
inline void BarUiMsg (uiMsg_t type, const char *format, ...) {
	va_list fmtargs;

	switch (type) {
		case MSG_INFO:
			printf ("(i) ");
			break;

		case MSG_PLAYING:
			printf ("|>  ");
			break;

		case MSG_TIME:
			printf ("#   ");
			break;
		
		case MSG_ERR:
			printf ("/!\\ ");
			break;

		case MSG_QUESTION:
			printf ("[?] ");
			break;

		case MSG_LIST:
			printf ("       ");
			break;
	
		default:
			break;
	}
	va_start (fmtargs, format);
	vprintf (format, fmtargs);
	va_end (fmtargs);

	fflush (stdout);
}

/*	prints human readable status message based on return value
 *	@param piano return value
 */
inline PianoReturn_t BarUiPrintPianoStatus (PianoReturn_t ret) {
	if (ret != PIANO_RET_OK) {
		BarUiMsg (MSG_NONE, "Error: %s\n", PianoErrorToStr (ret));
	} else {
		BarUiMsg (MSG_NONE, "Ok.\n");
	}
	return ret;
}

/*	check if all characters of string are numeric
 *	@param the string
 *	@return 1 = yes, 0 = not numeric
 */
char BarIsNumericStr (const char *str) {
	while (*str != '\0') {
		if (isdigit (*str) == 0) {
			return 0;
		}
		str++;
	}
	return 1;
}

/*	use readline to get integer value
 *	@param prompt or NULL
 *	@param returns integer
 *	@return 1 = success, 0 = failure (not an integer, ...)
 */
char BarReadlineInt (const char *prompt, int *retVal) {
	char *buf;
	char ret = 0;

	BarUiMsg (MSG_QUESTION, prompt);
	if ((buf = readline (NULL)) != NULL && strlen (buf) > 0 &&
			BarIsNumericStr (buf)) {
		*retVal = atoi (buf);
		ret = 1;
	}
	if (buf != NULL) {
		free (buf);
	}
	return ret;
}

/*	sort linked list (station)
 *	@param stations
 *	@return NULL-terminated array with sorted stations
 */
PianoStation_t **BarSortedStations (PianoStation_t *unsortedStations) {
	PianoStation_t *currStation, **sortedStations, **currSortedStation;
	PianoStation_t *oldStation, *veryOldStation;
	size_t unsortedStationsN = 0;
	char inserted;

	/* get size */
	currStation = unsortedStations;
	while (currStation != NULL) {
		unsortedStationsN++;
		currStation = currStation->next;
	}
	sortedStations = calloc (unsortedStationsN+1, sizeof (*sortedStations));

	currStation = unsortedStations;
	while (currStation != NULL) {
		currSortedStation = sortedStations;
		inserted = 0;
		while (*currSortedStation != NULL && !inserted) {
			/* item has to be inserted _before_ current item? */
			/* FIXME: this doesn't handle multibyte chars correctly */
			if (strcasecmp (currStation->name,
					(*currSortedStation)->name) < 0) {
				oldStation = *currSortedStation;
				*currSortedStation = currStation;
				currSortedStation++;
				/* move items */
				while (*currSortedStation != NULL) {
					veryOldStation = *currSortedStation;
					*currSortedStation = oldStation;
					oldStation = veryOldStation;
					currSortedStation++;
				}
				/* append last item */
				if (oldStation != NULL) {
					*currSortedStation = oldStation;
				}
				inserted = 1;
			}
			currSortedStation++;
		}
		/* item could not be inserted: append */
		if (!inserted) {
			*currSortedStation = currStation;
		}
		currStation = currStation->next;
	}
	return sortedStations;
}

/*	let user pick one station
 *	@param piano handle
 *	@return pointer to selected station or NULL
 */
PianoStation_t *BarUiSelectStation (PianoHandle_t *ph, const char *prompt) {
	PianoStation_t **ss = NULL, **ssCurr = NULL, *retStation;
	int i = 0;

	/* sort and print stations */
	ss = BarSortedStations (ph->stations);
	ssCurr = ss;
	while (*ssCurr != NULL) {
		BarUiMsg (MSG_LIST, "%2i) %c%c%c %s\n", i,
				(*ssCurr)->useQuickMix ? 'q' : ' ',
				(*ssCurr)->isQuickMix ? 'Q' : ' ',
				!(*ssCurr)->isCreator ? 'S' : ' ',
				(*ssCurr)->name);
		ssCurr++;
		i++;
	}

	if (!BarReadlineInt (prompt, &i)) {
		free (ss);
		return NULL;
	}
	ssCurr = ss;
	while (*ssCurr != NULL && i > 0) {
		ssCurr++;
		i--;
	}
	retStation = *ssCurr;
	free (ss);
	return retStation;
}

/*	let user pick one song
 *	@param song list
 *	@return pointer to selected item in song list or NULL
 */
PianoSong_t *BarUiSelectSong (PianoSong_t *startSong) {
	PianoSong_t *tmpSong = NULL;
	int i = 0;

	/* print all songs */
	tmpSong = startSong;
	while (tmpSong != NULL) {
		BarUiMsg (MSG_LIST, "%2u) %s - %s\n", i, tmpSong->artist,
				tmpSong->title);
		i++;
		tmpSong = tmpSong->next;
	}
	if (!BarReadlineInt ("Select song: ", &i)) {
		return NULL;
	}
	tmpSong = startSong;
	while (tmpSong != NULL && i > 0) {
		tmpSong = tmpSong->next;
		i--;
	}
	return tmpSong;
}

/*	let user pick one artist
 *	@param artists (linked list)
 *	@return pointer to selected artist or NULL on abort
 */
PianoArtist_t *BarUiSelectArtist (PianoArtist_t *startArtist) {
	PianoArtist_t *tmpArtist = NULL;
	int i = 0;

	/* print all artists */
	tmpArtist = startArtist;
	while (tmpArtist != NULL) {
		BarUiMsg (MSG_LIST, "%2u) %s\n", i, tmpArtist->name);
		i++;
		tmpArtist = tmpArtist->next;
	}
	if (!BarReadlineInt ("Select artist: ", &i)) {
		return NULL;
	}
	tmpArtist = startArtist;
	while (tmpArtist != NULL && i > 0) {
		tmpArtist = tmpArtist->next;
		i--;
	}
	return tmpArtist;
}

/*	search music: query, search request, return music id
 *	@param piano handle
 *	@return musicId or NULL on abort/error
 */
char *BarUiSelectMusicId (const PianoHandle_t *ph) {
	char *musicId = NULL, *lineBuf;
	char yesnoBuf;
	PianoSearchResult_t searchResult;
	PianoArtist_t *tmpArtist;
	PianoSong_t *tmpSong;

	BarUiMsg (MSG_QUESTION, "Search for artist/title: ");
	lineBuf = readline (NULL);
	if (lineBuf != NULL && strlen (lineBuf) > 0) {
		BarUiMsg (MSG_INFO, "Searching... ");
		if (BarUiPrintPianoStatus (PianoSearchMusic (ph, lineBuf,
				&searchResult)) != PIANO_RET_OK) {
			free (lineBuf);
			return NULL;
		}
		BarUiMsg (MSG_NONE, "\r");
		if (searchResult.songs != NULL && searchResult.artists != NULL) {
			/* songs and artists found */
			BarUiMsg (MSG_QUESTION, "Is this an [a]rtist or [t]rack name? "
					"Press c to abort. ");
			read (fileno (stdin), &yesnoBuf, sizeof (yesnoBuf));
			BarUiMsg (MSG_NONE, "\n");
			if (yesnoBuf == 'a') {
				tmpArtist = BarUiSelectArtist (searchResult.artists);
				if (tmpArtist != NULL) {
					musicId = strdup (tmpArtist->musicId);
				}
			} else if (yesnoBuf == 't') {
				tmpSong = BarUiSelectSong (searchResult.songs);
				if (tmpSong != NULL) {
					musicId = strdup (tmpSong->musicId);
				}
			}
		} else if (searchResult.songs != NULL) {
			/* songs found */
			tmpSong = BarUiSelectSong (searchResult.songs);
			if (tmpSong != NULL) {
				musicId = strdup (tmpSong->musicId);
			}
		} else if (searchResult.artists != NULL) {
			/* artists found */
			tmpArtist = BarUiSelectArtist (searchResult.artists);
			if (tmpArtist != NULL) {
				musicId = strdup (tmpArtist->musicId);
			}
		} else {
			BarUiMsg (MSG_INFO, "Nothing found...\n");
		}
		PianoDestroySearchResult (&searchResult);
	}
	if (lineBuf != NULL) {
		free (lineBuf);
	}

	return musicId;
}

/*	browse genre stations and create shared station
 *	@param piano handle
 */
void BarStationFromGenre (PianoHandle_t *ph) {
	int i;
	PianoGenreCategory_t *curCat;
	PianoStation_t *curStation;

	/* receive genre stations list if not yet available */
	if (ph->genreStations == NULL) {
		BarUiMsg (MSG_INFO, "Receiving genre stations... ");
		if (BarUiPrintPianoStatus (PianoGetGenreStations (ph)) !=
				PIANO_RET_OK) {
			return;
		}
	}

	/* print all available categories */
	curCat = ph->genreStations;
	i = 0;
	while (curCat != NULL) {
		BarUiMsg (MSG_LIST, "%2i) %s\n", i, curCat->name);
		i++;
		curCat = curCat->next;
	}
	/* select category or exit */
	if (!BarReadlineInt ("Select category: ", &i)) {
		return;
	}
	curCat = ph->genreStations;
	while (curCat != NULL && i > 0) {
		curCat = curCat->next;
		i--;
	}
	
	/* print all available stations */
	curStation = curCat->stations;
	i = 0;
	while (curStation != NULL) {
		BarUiMsg (MSG_LIST, "%2i) %s\n", i, curStation->name);
		i++;
		curStation = curStation->next;
	}
	if (!BarReadlineInt ("Select genre: ", &i)) {
		return;
	}
	curStation = curCat->stations;
	while (curStation != NULL && i > 0) {
		curStation = curStation->next;
		i--;
	}
	/* create station */
	BarUiMsg (MSG_INFO, "Adding shared station \"%s\"... ", curStation->name);
	BarUiPrintPianoStatus (PianoCreateStation (ph, "sh", curStation->id));
}

/*	Print station infos (including station id)
 *	@param the station
 */
inline void BarUiPrintStation (PianoStation_t *station) {
	BarUiMsg (MSG_PLAYING, "Station \"%s\" (%s)\n", station->name, station->id);
}

/*	Print song infos (artist, title, album, loved)
 *	@param the song
 *	@param alternative station info (show real station for quickmix, e.g.)
 */
inline void BarUiPrintSong (PianoSong_t *song, PianoStation_t *station) {
	BarUiMsg (MSG_PLAYING, "\"%s\" by \"%s\" on \"%s\"%s%s%s\n",
			song->title, song->artist, song->album,
			(song->rating == PIANO_RATE_LOVE) ? " <3" : "",
			station != NULL ? " @ " : "",
			station != NULL ? station->name : "");
}
