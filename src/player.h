/*
Copyright (c) 2008-2018
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

#pragma once

#include "config.h"

/* required for freebsd */
#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>

#include <ao/ao.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <piano.h>

#include "settings.h"

typedef enum {
	/* not running */
	PLAYER_DEAD = 0,
	/* running, but not ready to play music yet */
	PLAYER_WAITING,
	/* currently playing a song */
	PLAYER_PLAYING,
	/* finished playing a song */
	PLAYER_FINISHED,
} BarPlayerMode;

typedef struct {
	/* public attributes protected by mutex */
	pthread_mutex_t lock, aoplayLock;
	pthread_cond_t cond, aoplayCond; /* broadcast changes to doPause */
	bool doQuit, doPause;

	/* measured in seconds */
	unsigned int songDuration;
	unsigned int songPlayed;

	BarPlayerMode mode;

	/* private attributes _not_ protected by mutex */

	/* libav */
	AVFilterContext *fvolume;
	AVFilterGraph *fgraph;
	AVFormatContext *fctx;
	AVStream *st;
	AVCodecContext *cctx;
	AVFilterContext *fbufsink, *fabuf;
	int streamIdx;
	int64_t lastTimestamp;
	sig_atomic_t interrupted;

	ao_device *aoDev;

	/* settings (must be set before starting the thread) */
	double gain;
	char *url;
	const BarSettings_t *settings;
} player_t;

enum {PLAYER_RET_OK = 0, PLAYER_RET_HARDFAIL = 1, PLAYER_RET_SOFTFAIL = 2};

void *BarPlayerThread (void *data);
void *BarAoPlayThread (void *data);
void BarPlayerSetVolume (player_t * const player);
void BarPlayerInit (player_t * const p, const BarSettings_t * const settings);
void BarPlayerReset (player_t * const p);
void BarPlayerDestroy (player_t * const p);
BarPlayerMode BarPlayerGetMode (player_t * const player);

