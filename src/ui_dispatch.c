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
#include "debug_log.h"

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
	BarUiDispatchContext_t AllowedStationTypes = context & BAR_DC_ALL_STATION_TYPES;
	BarUiDispatchContext_t TempContext;
	BarKeyShortcutId_t Ret = BAR_KS_COUNT;

	if (selStation != NULL) {
		const BarUiDispatchContext_t StationTypes[] = {
			BAR_DC_UNDEFINED,
			BAR_DC_STATION_TYPE_STATION,
			BAR_DC_STATION_TYPE_PODCAST,
			BAR_DC_STATION_TYPE_PLAYLIST,
			BAR_DC_STATION_TYPE_ALBUM,
			BAR_DC_STATION_TYPE_TRACK
		};
		context |= BAR_DC_STATION;
		PianoStationType_t stationType = selStation->stationType;
		if(stationType >= PIANO_TYPE_NONE && stationType < PIANO_TYPE_LAST) {
			context |= StationTypes[stationType];
	}
		else {
			ELOG("Internal error: stationType 0x%x\n",stationType);
		}
	}
	if (selSong != NULL) {
		context |= BAR_DC_SONG;
	}

	for (size_t i = 0; i < BAR_KS_COUNT; i++) {
		if (app->settings.keys[i] != BAR_KS_DISABLED &&
			app->settings.keys[i] == key) 
		{
			const char *ErrMsg = NULL;
			if(!BarUiContextMatch(context,dispatchActions[i].context,&ErrMsg)) {
				if(verbose && ErrMsg != NULL) {
					BarUiMsg (&app->settings, MSG_ERR, "%s",ErrMsg);
				}
				break;
			}
			assert (dispatchActions[i].function != NULL);
			dispatchActions[i].function (app, selStation, selSong,context);
			Ret = i;
			break;
		}
	}
	return Ret;
}

bool BarUiContextMatch(
	BarUiDispatchContext_t Have,
	BarUiDispatchContext_t Need,
	const char **ErrMsg)
{
	BarUiDispatchContext_t NeedStation = Need & BAR_DC_ALL_STATION_TYPES;
	BarUiDispatchContext_t HaveStation = Have & BAR_DC_ALL_STATION_TYPES;
	bool Ret = false;

	Need &= ~BAR_DC_ALL_STATION_TYPES;
	Have &= ~BAR_DC_ALL_STATION_TYPES;

	do {
		if(NeedStation != 0 && (NeedStation & HaveStation) == 0) {
		// Station type reqirement not met
			break;
		}
		if(Need != BAR_DC_UNDEFINED && (Need & Have) !=  Need) {
			break;
			if(ErrMsg != NULL) {
				if((Need & BAR_DC_SONG) && !(Have & BAR_DC_SONG)) {
					*ErrMsg = "No song playing.\n";
				}
				else if((Need & BAR_DC_STATION) && !(Have & BAR_DC_STATION)) {
					*ErrMsg = "No station selected.\n";
				}
			}
		}
		Ret = true;
	} while(false);

	return Ret;
}

