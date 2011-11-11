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

#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 1 /* fileno() */
#define _BSD_SOURCE /* strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

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
#include <stdbool.h>
#include <limits.h>

/* pandora.com library */
#include <piano.h>

#include "main.h"
#include "terminal.h"
#include "config.h"
#include "ui.h"
#include "ui_dispatch.h"
#include "ui_readline.h"

/*	copy proxy settings to waitress handle
 */
static void BarMainLoadProxy (const BarSettings_t *settings,
		WaitressHandle_t *waith) {
	/* set up proxy (control proxy for non-us citizen or global proxy for poor
	 * firewalled fellows) */
	if (settings->controlProxy != NULL) {
		/* control proxy overrides global proxy */
		WaitressSetProxy (waith, settings->controlProxy);
	} else if (settings->proxy != NULL && strlen (settings->proxy) > 0) {
		WaitressSetProxy (waith, settings->proxy);
	}
}

/*	authenticate user
 */
static bool BarMainLoginUser (BarApp_t *app) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataLogin_t reqData;
	bool ret;

	reqData.user = app->settings.username;
	reqData.password = app->settings.password;
	reqData.step = 0;

	BarUiMsg (&app->settings, MSG_INFO, "Login... ");
	ret = BarUiPianoCall (app, PIANO_REQUEST_LOGIN, &reqData, &pRet, &wRet);
	BarUiStartEventCmd (&app->settings, "userlogin", NULL, NULL, &app->player,
			NULL, pRet, wRet);
	return ret;
}

/*	ask for username/password if none were provided in settings
 */
static void BarMainGetLoginCredentials (BarSettings_t *settings,
		BarReadlineFds_t *input) {
	if (settings->username == NULL) {
		char nameBuf[100];
		BarUiMsg (settings, MSG_QUESTION, "Email: ");
		BarReadlineStr (nameBuf, sizeof (nameBuf), input, BAR_RL_DEFAULT);
		settings->username = strdup (nameBuf);
	}
	if (settings->password == NULL) {
		char passBuf[100];
		BarUiMsg (settings, MSG_QUESTION, "Password: ");
		BarReadlineStr (passBuf, sizeof (passBuf), input, BAR_RL_NOECHO);
		write (STDIN_FILENO, "\n", 1);
		settings->password = strdup (passBuf);
	}
}

/*	get station list
 */
static bool BarMainGetStations (BarApp_t *app) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	bool ret;

	BarUiMsg (&app->settings, MSG_INFO, "Get stations... ");
	ret = BarUiPianoCall (app, PIANO_REQUEST_GET_STATIONS, NULL, &pRet, &wRet);
	BarUiStartEventCmd (&app->settings, "usergetstations", NULL, NULL, &app->player,
			app->ph.stations, pRet, wRet);
	return ret;
}

/*	get initial station from autostart setting or user input
 */
static void BarMainGetInitialStation (BarApp_t *app) {
	/* try to get autostart station */
	if (app->settings.autostartStation != NULL) {
		app->curStation = PianoFindStationById (app->ph.stations,
				app->settings.autostartStation);
		if (app->curStation == NULL) {
			BarUiMsg (&app->settings, MSG_ERR,
					"Error: Autostart station not found.\n");
		}
	}
	/* no autostart? ask the user */
	if (app->curStation == NULL) {
		app->curStation = BarUiSelectStation (app, app->ph.stations, "Select station: ", NULL);
	}
	if (app->curStation != NULL) {
		BarUiPrintStation (&app->settings, app->curStation);
	}
}

/*	wait for user input
 */
static void BarMainHandleUserInput (BarApp_t *app) {
	char buf[2];
	if (BarReadline (buf, sizeof (buf), NULL, &app->input,
			BAR_RL_FULLRETURN | BAR_RL_NOECHO, 1) > 0) {
		BarUiDispatch (app, buf[0], app->curStation, app->playlist, true,
				BAR_DC_GLOBAL);
	}
}

/*	fetch new playlist
 */
static void BarMainGetPlaylist (BarApp_t *app) {
	PianoReturn_t pRet;
	WaitressReturn_t wRet;
	PianoRequestDataGetPlaylist_t reqData;
	reqData.station = app->curStation;
	reqData.format = app->settings.audioFormat;

	BarUiMsg (&app->settings, MSG_INFO, "Receiving new playlist... ");
	if (!BarUiPianoCall (app, PIANO_REQUEST_GET_PLAYLIST,
			&reqData, &pRet, &wRet)) {
		app->curStation = NULL;
	} else {
		app->playlist = reqData.retPlaylist;
		if (app->playlist == NULL) {
			BarUiMsg (&app->settings, MSG_INFO, "No tracks left.\n");
			app->curStation = NULL;
		}
	}
	BarUiStartEventCmd (&app->settings, "stationfetchplaylist",
			app->curStation, app->playlist, &app->player, app->ph.stations,
			pRet, wRet);
}

/*	start new player thread
 */
static void BarMainStartPlayback (BarApp_t *app, pthread_t *playerThread) {
	BarUiPrintSong (&app->settings, app->playlist, app->curStation->isQuickMix ?
			PianoFindStationById (app->ph.stations,
			app->playlist->stationId) : NULL);

	if (app->playlist->audioUrl == NULL) {
		BarUiMsg (&app->settings, MSG_ERR, "Invalid song url.\n");
	} else {
		/* setup player */
		memset (&app->player, 0, sizeof (app->player));

		WaitressInit (&app->player.waith);
		WaitressSetUrl (&app->player.waith, app->playlist->audioUrl);

		/* set up global proxy, player is NULLed on songfinish */
		if (app->settings.proxy != NULL) {
			WaitressSetProxy (&app->player.waith, app->settings.proxy);
		}

		app->player.gain = app->playlist->fileGain;
		app->player.scale = BarPlayerCalcScale (app->player.gain + app->settings.volume);
		app->player.audioFormat = app->playlist->audioFormat;
		app->player.settings = &app->settings;

		/* throw event */
		BarUiStartEventCmd (&app->settings, "songstart",
				app->curStation, app->playlist, &app->player, app->ph.stations,
				PIANO_RET_OK, WAITRESS_RET_OK);

		/* prevent race condition, mode must _not_ be FREED if
		 * thread has been started */
		app->player.mode = PLAYER_STARTING;
		/* start player */
		pthread_create (playerThread, NULL, BarPlayerThread,
				&app->player);
	}
}

/*	player is done, clean up
 */
static void BarMainPlayerCleanup (BarApp_t *app, pthread_t *playerThread) {
	void *threadRet;

	BarUiStartEventCmd (&app->settings, "songfinish", app->curStation,
			app->playlist, &app->player, app->ph.stations, PIANO_RET_OK,
			WAITRESS_RET_OK);

	/* FIXME: pthread_join blocks everything if network connection
	 * is hung up e.g. */
	pthread_join (*playerThread, &threadRet);

	/* don't continue playback if thread reports error */
	if (threadRet != (void *) PLAYER_RET_OK) {
		app->curStation = NULL;
	}

	memset (&app->player, 0, sizeof (app->player));
}

/*	print song duration
 */
static void BarMainPrintTime (BarApp_t *app) {
	/* Ugly: songDuration is unsigned _long_ int! Lets hope this won't
	 * overflow */
	int songRemaining = (signed long int) (app->player.songDuration -
			app->player.songPlayed) / BAR_PLAYER_MS_TO_S_FACTOR;
	enum {POSITIVE, NEGATIVE} sign = NEGATIVE;
	if (songRemaining < 0) {
		/* song is longer than expected */
		sign = POSITIVE;
		songRemaining = -songRemaining;
	}
	BarUiMsg (&app->settings, MSG_TIME, "%c%02i:%02i/%02i:%02i\r",
			(sign == POSITIVE ? '+' : '-'),
			songRemaining / 60, songRemaining % 60,
			app->player.songDuration / BAR_PLAYER_MS_TO_S_FACTOR / 60,
			app->player.songDuration / BAR_PLAYER_MS_TO_S_FACTOR % 60);
}

/*	main loop
 */
static void BarMainLoop (BarApp_t *app) {
	pthread_t playerThread;

	BarMainGetLoginCredentials (&app->settings, &app->input);

	BarMainLoadProxy (&app->settings, &app->waith);

	if (!BarMainLoginUser (app)) {
		return;
	}

	if (!BarMainGetStations (app)) {
		return;
	}

	BarMainGetInitialStation (app);

	/* little hack, needed to signal: hey! we need a playlist, but don't
	 * free anything (there is nothing to be freed yet) */
	memset (&app->player, 0, sizeof (app->player));

	while (!app->doQuit) {
		/* song finished playing, clean up things/scrobble song */
		if (app->player.mode == PLAYER_FINISHED_PLAYBACK) {
			BarMainPlayerCleanup (app, &playerThread);
		}

		/* check whether player finished playing and start playing new
		 * song */
		if (app->player.mode >= PLAYER_FINISHED_PLAYBACK ||
				app->player.mode == PLAYER_FREED) {
			if (app->curStation != NULL) {
				/* what's next? */
				if (app->playlist != NULL) {
					PianoSong_t *histsong = app->playlist;
					app->playlist = app->playlist->next;
					BarUiHistoryPrepend (app, histsong);
				}
				if (app->playlist == NULL) {
					BarMainGetPlaylist (app);
				}
				/* song ready to play */
				if (app->playlist != NULL) {
					BarMainStartPlayback (app, &playerThread);
				}
			}
		}

		BarMainHandleUserInput (app);

		/* show time */
		if (app->player.mode >= PLAYER_SAMPLESIZE_INITIALIZED &&
				app->player.mode < PLAYER_FINISHED_PLAYBACK) {
			BarMainPrintTime (app);
		}
	}

	if (app->player.mode != PLAYER_FREED) {
		pthread_join (playerThread, NULL);
	}
}

int main (int argc, char **argv) {
	static BarApp_t app;
	/* terminal attributes _before_ we started messing around with ~ECHO */
	struct termios termOrig;

	memset (&app, 0, sizeof (app));

	/* save terminal attributes, before disabling echoing */
	BarTermSave (&termOrig);
	BarTermSetEcho (0);
	BarTermSetBuffer (0);

	/* init some things */
	ao_initialize ();
	gnutls_global_init ();
	PianoInit (&app.ph);

	BarSettingsInit (&app.settings);
	BarSettingsRead (&app.settings);

	BarUiMsg (&app.settings, MSG_NONE,
			"Welcome to " PACKAGE " (" VERSION ")! ");
	if (app.settings.keys[BAR_KS_HELP] == BAR_KS_DISABLED) {
		BarUiMsg (&app.settings, MSG_NONE, "\n");
	} else {
		BarUiMsg (&app.settings, MSG_NONE,
				"Press %c for a list of commands.\n",
				app.settings.keys[BAR_KS_HELP]);
	}

	WaitressInit (&app.waith);
	app.waith.url.host = strdup (PIANO_RPC_HOST);
	app.waith.url.tls = true;
	app.waith.tlsFingerprint = app.settings.tlsFingerprint;

	/* init fds */
	FD_ZERO(&app.input.set);
	app.input.fds[0] = STDIN_FILENO;
	FD_SET(app.input.fds[0], &app.input.set);

	/* open fifo read/write so it won't EOF if nobody writes to it */
	assert (sizeof (app.input.fds) / sizeof (*app.input.fds) >= 2);
	app.input.fds[1] = open (app.settings.fifo, O_RDWR);
	if (app.input.fds[1] != -1) {
		FD_SET(app.input.fds[1], &app.input.set);
		BarUiMsg (&app.settings, MSG_INFO, "Control fifo at %s opened\n",
				app.settings.fifo);
	}
	app.input.maxfd = app.input.fds[0] > app.input.fds[1] ? app.input.fds[0] :
			app.input.fds[1];
	++app.input.maxfd;

	BarMainLoop (&app);

	if (app.input.fds[1] != -1) {
		close (app.input.fds[1]);
	}

	PianoDestroy (&app.ph);
	PianoDestroyPlaylist (app.songHistory);
	PianoDestroyPlaylist (app.playlist);
	WaitressFree (&app.waith);
	ao_shutdown();
	gnutls_global_deinit ();
	BarSettingsDestroy (&app.settings);

	/* restore terminal attributes, zsh doesn't need this, bash does... */
	BarTermRestore (&termOrig);

	return 0;
}

