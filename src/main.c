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

#define _POSIX_C_SOURCE 1 /* fileno() */
#define _BSD_SOURCE /* strdup() */

/* system includes */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <ctype.h>
/* open () */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* tcset/getattr () */
#include <termios.h>
#include <pthread.h>
#include <assert.h>

/* pandora.com library */
#include <piano.h>

#include "player.h"
#include "settings.h"
#include "terminal.h"
#include "config.h"
#include "ui.h"
#include "ui_act.h"
#include "ui_readline.h"

int main (int argc, char **argv) {
	/* handles */
	PianoHandle_t ph;
	WaitressHandle_t waith;
	static struct audioPlayer player;
	BarSettings_t settings;
	pthread_t playerThread;
	/* playlist; first item is current song */
	PianoSong_t *playlist = NULL;
	PianoSong_t *songHistory = NULL;
	PianoStation_t *curStation = NULL;
	char doQuit = 0;
	/* FIXME: max path length? */
	char ctlPath[1024];
	FILE *ctlFd = NULL;
	struct timeval selectTimeout;
	int maxFd, selectFds[2];
	fd_set readSet, readSetCopy;
	char buf = '\0';
	/* terminal attributes _before_ we started messing around with ~ECHO */
	struct termios termOrig;

	/* save terminal attributes, before disabling echoing */
	BarTermSave (&termOrig);

	BarTermSetEcho (0);
	BarTermSetBuffer (0);
	/* init some things */
	ao_initialize ();
	PianoInit (&ph);

	WaitressInit (&waith);
	strncpy (waith.host, PIANO_RPC_HOST, sizeof (waith.host)-1);
	strncpy (waith.port, PIANO_RPC_PORT, sizeof (waith.port)-1);

	BarSettingsInit (&settings);
	BarSettingsRead (&settings);

	BarUiMsg (MSG_NONE, "Welcome to " PACKAGE "! Press %c for a list of commands.\n",
			settings.keys[BAR_KS_HELP]);

	/* init fds */
	FD_ZERO(&readSet);
	selectFds[0] = fileno (stdin);
	FD_SET(selectFds[0], &readSet);
	maxFd = selectFds[0] + 1;

	BarGetXdgConfigDir (PACKAGE "/ctl", ctlPath, sizeof (ctlPath));
	/* FIXME: why is r_+_ required? */
	ctlFd = fopen (ctlPath, "r+");
	if (ctlFd != NULL) {
		selectFds[1] = fileno (ctlFd);
		FD_SET(selectFds[1], &readSet);
		/* assuming ctlFd is always > stdin */
		maxFd = selectFds[1] + 1;
		BarUiMsg (MSG_INFO, "Control fifo at %s opened\n", ctlPath);
	}

	if (settings.username == NULL) {
		char nameBuf[100];
		BarUiMsg (MSG_QUESTION, "Username: ");
		BarReadlineStr (nameBuf, sizeof (nameBuf), 0, stdin);
		settings.username = strdup (nameBuf);
	}
	if (settings.password == NULL) {
		char passBuf[100];
		BarUiMsg (MSG_QUESTION, "Password: ");
		BarReadlineStr (passBuf, sizeof (passBuf), 1, stdin);
		settings.password = strdup (passBuf);
	}

	/* set up proxy (control proxy for non-us citizen or global proxy for poor
	 * firewalled fellows) */
	if (settings.controlProxy != NULL) {
		/* control proxy overrides global proxy */
		char tmpPath[2];
		WaitressSplitUrl (settings.controlProxy, waith.proxyHost,
				sizeof (waith.proxyHost), waith.proxyPort,
				sizeof (waith.proxyPort), tmpPath, sizeof (tmpPath));
	} else if (settings.proxy != NULL && strlen (settings.proxy) > 0) {
		char tmpPath[2];
		WaitressSplitUrl (settings.proxy, waith.proxyHost,
				sizeof (waith.proxyHost), waith.proxyPort,
				sizeof (waith.proxyPort), tmpPath, sizeof (tmpPath));
	}

	{
		PianoReturn_t pRet;
		WaitressReturn_t wRet;
		PianoRequestDataLogin_t reqData;
		reqData.user = settings.username;
		reqData.password = settings.password;
		
		BarUiMsg (MSG_INFO, "Login... ");
		if (!BarUiPianoCall (&ph, PIANO_REQUEST_LOGIN, &waith, &reqData, &pRet,
				&wRet)) {
			BarTermRestore (&termOrig);
			return 0;
		}
	}

	{
		PianoReturn_t pRet;
		WaitressReturn_t wRet;

		BarUiMsg (MSG_INFO, "Get stations... ");
		if (!BarUiPianoCall (&ph, PIANO_REQUEST_GET_STATIONS, &waith, NULL,
				&pRet, &wRet)) {
			BarTermRestore (&termOrig);
			return 0;
		}
	}

	/* try to get autostart station */
	if (settings.autostartStation != NULL) {
		curStation = PianoFindStationById (ph.stations,
				settings.autostartStation);
		if (curStation == NULL) {
			BarUiMsg (MSG_ERR, "Error: Autostart station not found.\n");
		}
	}
	/* no autostart? ask the user */
	if (curStation == NULL) {
		curStation = BarUiSelectStation (&ph, "Select station: ",
				settings.sortOrder, stdin);
	}
	if (curStation != NULL) {
		BarUiPrintStation (curStation);
	}

	/* little hack, needed to signal: hey! we need a playlist, but don't
	 * free anything (there is nothing to be freed yet) */
	memset (&player, 0, sizeof (player));

	while (!doQuit) {
		/* song finished playing, clean up things/scrobble song */
		if (player.mode == PLAYER_FINISHED_PLAYBACK) {
			BarUiStartEventCmd (&settings, "songfinish", curStation, playlist,
					&player, PIANO_RET_OK, WAITRESS_RET_OK);
			/* FIXME: pthread_join blocks everything if network connection
			 * is hung up e.g. */
			void *threadRet;
			pthread_join (playerThread, &threadRet);
			/* don't continue playback if thread reports error */
			if (threadRet != (void *) PLAYER_RET_OK) {
				curStation = NULL;
			}
			memset (&player, 0, sizeof (player));
		}

		/* check whether player finished playing and start playing new
		 * song */
		if (player.mode >= PLAYER_FINISHED_PLAYBACK ||
				player.mode == PLAYER_FREED) {
			if (curStation != NULL) {
				/* what's next? */
				if (playlist != NULL) {
					if (settings.history != 0) {
						/* prepend song to history list */
						PianoSong_t *tmpSong = songHistory;
						songHistory = playlist;
						/* select next song */
						playlist = playlist->next;
						songHistory->next = tmpSong;

						/* limit history's length */
						/* start with 1, so we're stopping at n-1 and have the
						 * chance to set ->next = NULL */
						unsigned int i = 1;
						tmpSong = songHistory;
						while (i < settings.history && tmpSong != NULL) {
							tmpSong = tmpSong->next;
							++i;
						}
						/* if too many songs in history... */
						if (tmpSong != NULL) {
							PianoSong_t *delSong = tmpSong->next;
							tmpSong->next = NULL;
							if (delSong != NULL) {
								PianoDestroyPlaylist (delSong);
							}
						}
					} else {
						/* don't keep history */
						playlist = playlist->next;
					}
				}
				if (playlist == NULL) {
					PianoReturn_t pRet;
					WaitressReturn_t wRet;
					PianoRequestDataGetPlaylist_t reqData;
					reqData.station = curStation;
					reqData.format = settings.audioFormat;

					BarUiMsg (MSG_INFO, "Receiving new playlist... ");
					if (!BarUiPianoCall (&ph, PIANO_REQUEST_GET_PLAYLIST,
							&waith, &reqData, &pRet, &wRet)) {
						curStation = NULL;
					} else {
						playlist = reqData.retPlaylist;
						if (playlist == NULL) {
							BarUiMsg (MSG_INFO, "No tracks left.\n");
							curStation = NULL;
						}
					}
					BarUiStartEventCmd (&settings, "stationfetchplaylist",
							curStation, playlist, &player, pRet, wRet);
				}
				/* song ready to play */
				if (playlist != NULL) {
					BarUiPrintSong (playlist, curStation->isQuickMix ?
							PianoFindStationById (ph.stations,
							playlist->stationId) : NULL);

					if (playlist->audioUrl == NULL) {
						BarUiMsg (MSG_ERR, "Invalid song url.\n");
					} else {
						/* setup player */
						memset (&player, 0, sizeof (player));

						WaitressInit (&player.waith);
						WaitressSetUrl (&player.waith, playlist->audioUrl);

						/* set up global proxy, player is NULLed on songfinish */
						if (settings.proxy != NULL) {
							char tmpPath[2];
							WaitressSplitUrl (settings.proxy,
									player.waith.proxyHost,
									sizeof (player.waith.proxyHost),
									player.waith.proxyPort,
									sizeof (player.waith.proxyPort), tmpPath,
									sizeof (tmpPath));
						}

						player.gain = playlist->fileGain;
						player.audioFormat = playlist->audioFormat;
			
						/* throw event */
						BarUiStartEventCmd (&settings, "songstart", curStation,
								playlist, &player, PIANO_RET_OK,
								WAITRESS_RET_OK);

						/* prevent race condition, mode must _not_ be FREED if
						 * thread has been started */
						player.mode = PLAYER_STARTING;
						/* start player */
						pthread_create (&playerThread, NULL, BarPlayerThread,
								&player);
					} /* end if audioUrl == NULL */
				} /* end if playlist != NULL */
			} /* end if curStation != NULL */
		}

		/* select modifies its arguments => copy the set */
		memcpy (&readSetCopy, &readSet, sizeof (readSet));
		selectTimeout.tv_sec = 1;
		selectTimeout.tv_usec = 0;

		/* in the meantime: wait for user actions */
		if (select (maxFd, &readSetCopy, NULL, NULL, &selectTimeout) > 0) {
			FILE *curFd = NULL;

			if (FD_ISSET(selectFds[0], &readSetCopy)) {
				curFd = stdin;
			} else if (FD_ISSET(selectFds[1], &readSetCopy)) {
				curFd = ctlFd;
			}
			buf = fgetc (curFd);

			size_t i;
			for (i = 0; i < BAR_KS_COUNT; i++) {
				if (settings.keys[i] == buf) {
					BarKeyShortcutFunc_t idToF[] = {BarUiActHelp,
							BarUiActLoveSong, BarUiActBanSong,
							BarUiActAddMusic, BarUiActCreateStation,
							BarUiActDeleteStation, BarUiActExplain,
							BarUiActStationFromGenre, BarUiActHistory,
							BarUiActSongInfo, BarUiActAddSharedStation,
							BarUiActMoveSong, BarUiActSkipSong, BarUiActPause,
							BarUiActQuit, BarUiActRenameStation,
							BarUiActSelectStation, BarUiActTempBanSong,
							BarUiActPrintUpcoming, BarUiActSelectQuickMix,
							BarUiActDebug, BarUiActBookmark};
					idToF[i] (&ph, &waith, &player, &settings, &playlist,
							&curStation, &songHistory, &doQuit, curFd);
					break;
				}
			}
		}

		/* show time */
		if (player.mode >= PLAYER_SAMPLESIZE_INITIALIZED &&
				player.mode < PLAYER_FINISHED_PLAYBACK) {
			/* Ugly: songDuration is unsigned _long_ int! Lets hope this won't
			 * overflow */
			int songRemaining = (signed long int) (player.songDuration - player.songPlayed)
					/ BAR_PLAYER_MS_TO_S_FACTOR;
			char pos = 0;
			if (songRemaining < 0) {
				/* Use plus sign if song is longer than expected */
				pos = 1;
				songRemaining = -songRemaining;
			}
			BarUiMsg (MSG_TIME, "%c%02i:%02i/%02i:%02i\r", (pos ? '+' : '-'),
					songRemaining / 60, songRemaining % 60,
					player.songDuration / BAR_PLAYER_MS_TO_S_FACTOR / 60,
					player.songDuration / BAR_PLAYER_MS_TO_S_FACTOR % 60);
		}
	}

	/* destroy everything (including the world...) */
	if (player.mode != PLAYER_FREED) {
		pthread_join (playerThread, NULL);
	}
	if (ctlFd != NULL) {
		fclose (ctlFd);
	}
	PianoDestroy (&ph);
	PianoDestroyPlaylist (songHistory);
	PianoDestroyPlaylist (playlist);
	ao_shutdown();
	BarSettingsDestroy (&settings);

	/* restore terminal attributes, zsh doesn't need this, bash does... */
	BarTermRestore (&termOrig);

	return 0;
}
