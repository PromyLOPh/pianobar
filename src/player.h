/*
Copyright (c) 2008-2010
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

#ifndef _PLAYER_H
#define _PLAYER_H

#include "config.h"

#ifdef ENABLE_FAAD
#include <neaacdec.h>
#endif

#ifdef ENABLE_MAD
#include <mad.h>
#endif

#include <ao/ao.h>
/* required for freebsd */
#include <sys/types.h>
#include <pthread.h>

#include <piano.h>
#include <waitress.h>

#include "settings.h"

#define BAR_PLAYER_MS_TO_S_FACTOR 1000

struct audioPlayer {
	char doQuit;
	unsigned char channels;
	unsigned char aoError;

	enum {
		PLAYER_FREED = 0, /* thread is not running */
		PLAYER_STARTING, /* thread is starting */
		PLAYER_INITIALIZED, /* decoder/waitress initialized */
		PLAYER_FOUND_ESDS,
		PLAYER_AUDIO_INITIALIZED, /* audio device opened */
		PLAYER_FOUND_STSZ,
		PLAYER_SAMPLESIZE_INITIALIZED,
		PLAYER_RECV_DATA, /* playing track */
		PLAYER_FINISHED_PLAYBACK
	} mode;
	PianoAudioFormat_t audioFormat;

	unsigned int scale;
	float gain;

	/* duration and already played time; measured in milliseconds */
	unsigned long int songDuration;
	unsigned long int songPlayed;

	unsigned long samplerate;

	size_t bufferFilled;
	size_t bufferRead;
	size_t bytesReceived;

	/* aac */
	#ifdef ENABLE_FAAD
	/* stsz atom: sample sizes */
	size_t sampleSizeN;
	size_t sampleSizeCurr;
	unsigned int *sampleSize;
	NeAACDecHandle aacHandle;
	#endif

	/* mp3 */
	#ifdef ENABLE_MAD
	struct mad_stream mp3Stream;
	struct mad_frame mp3Frame;
	struct mad_synth mp3Synth;
	#endif

	/* audio out */
	ao_device *audioOutDevice;
	const BarSettings_t *settings;

	pthread_mutex_t pauseMutex;
	WaitressHandle_t waith;
	/* buffer; should be large enough */
	unsigned char buffer[WAITRESS_BUFFER_SIZE*2];
};

enum {PLAYER_RET_OK = 0, PLAYER_RET_ERR = 1};

void *BarPlayerThread (void *data);
unsigned int BarPlayerCalcScale (float);

#endif /* _PLAYER_H */
