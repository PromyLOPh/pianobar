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

#ifndef _UI_ACT_H
#define _UI_ACT_H

#include <piano.h>

#include "main.h"
#include "ui_dispatch.h"

#define BarUiActCallback(name) void name (BarApp_t *app, \
		PianoStation_t *selStation, PianoSong_t *selSong, \
		BarUiDispatchContext_t context)

BarUiActCallback(BarUiActHelp);
BarUiActCallback(BarUiActAddMusic);
BarUiActCallback(BarUiActBanSong);
BarUiActCallback(BarUiActCreateStation);
BarUiActCallback(BarUiActAddSharedStation);
BarUiActCallback(BarUiActDeleteStation);
BarUiActCallback(BarUiActExplain);
BarUiActCallback(BarUiActStationFromGenre);
BarUiActCallback(BarUiActSongInfo);
BarUiActCallback(BarUiActLoveSong);
BarUiActCallback(BarUiActSkipSong);
BarUiActCallback(BarUiActMoveSong);
BarUiActCallback(BarUiActPause);
BarUiActCallback(BarUiActRenameStation);
BarUiActCallback(BarUiActSelectStation);
BarUiActCallback(BarUiActTempBanSong);
BarUiActCallback(BarUiActPrintUpcoming);
BarUiActCallback(BarUiActSelectQuickMix);
BarUiActCallback(BarUiActQuit);
BarUiActCallback(BarUiActDebug);
BarUiActCallback(BarUiActHistory);
BarUiActCallback(BarUiActBookmark);
BarUiActCallback(BarUiActVolDown);
BarUiActCallback(BarUiActVolUp);
BarUiActCallback(BarUiActManageStation);

#endif /* _UI_ACT_H */
