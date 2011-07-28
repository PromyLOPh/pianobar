/*
Copyright (c) 2010-2011
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

#include <assert.h>

#include "ui_dispatch.h"
#include "settings.h"
#include "ui.h"

/*	handle global keyboard shortcuts
 *	@return BAR_KS_* if action was performed or BAR_KS_COUNT on error/if no
 *			action was performed
 */
BarKeyShortcutId_t BarUiDispatch (BarApp_t *app, const char key, PianoStation_t *selStation,
		PianoSong_t *selSong, const bool verbose,
		BarUiDispatchContext_t context) {
	assert (app != NULL);
	assert (sizeof (app->settings.keys) / sizeof (*app->settings.keys) ==
			sizeof (dispatchActions) / sizeof (*dispatchActions));

	if (selStation != NULL) {
		context |= BAR_DC_STATION;
	}
	if (selSong != NULL) {
		context |= BAR_DC_SONG;
	}

	for (size_t i = 0; i < BAR_KS_COUNT; i++) {
		if (app->settings.keys[i] != BAR_KS_DISABLED &&
				app->settings.keys[i] == key) {
			if ((dispatchActions[i].context & context) == dispatchActions[i].context) {
				assert (dispatchActions[i].function != NULL);

				dispatchActions[i].function (app, selStation, selSong,
						context);
				return i;
			} else if (verbose) {
				if (dispatchActions[i].context & BAR_DC_SONG) {
					BarUiMsg (&app->settings, MSG_ERR, "No song playing.\n");
				} else if (dispatchActions[i].context & BAR_DC_STATION) {
					BarUiMsg (&app->settings, MSG_ERR, "No station selected.\n");
				} else {
					assert (0);
				}
				return BAR_KS_COUNT;
			}
		}
	}
	return BAR_KS_COUNT;
}

