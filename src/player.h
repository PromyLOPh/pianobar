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

#ifndef _PLAYER_H
#define _PLAYER_H

#include "config.h"

#include <curl/curl.h>

#ifdef ENABLE_FAAD
#include <neaacdec.h>
#endif

#ifdef ENABLE_MAD
#include <mad.h>
#endif

#include <ao/ao.h>
#include <pthread.h>
#include <piano.h>

struct audioPlayer {
	/* buffer; should be large enough */
	unsigned char buffer[CURL_MAX_WRITE_SIZE*2];
	size_t bufferFilled;
	size_t bufferRead;
	size_t bytesReceived;

	enum {PLAYER_FREED = 0, PLAYER_INITIALIZED, PLAYER_FOUND_ESDS,
			PLAYER_AUDIO_INITIALIZED, PLAYER_FOUND_STSZ,
			PLAYER_SAMPLESIZE_INITIALIZED, PLAYER_RECV_DATA,
			PLAYER_FINISHED_PLAYBACK} mode;

	PianoAudioFormat_t audioFormat;

	size_t sampleSizeN;
	size_t sampleSizeCurr;

	/* aac */
	#ifdef ENABLE_FAAD
	NeAACDecHandle aacHandle;
	/* stsz atom: sample sizes */
	unsigned int *sampleSize;
	#endif

	/* mp3 */
	#ifdef ENABLE_MAD
	struct mad_stream mp3Stream;
	struct mad_frame mp3Frame;
	struct mad_synth mp3Synth;
	mad_timer_t mp3Timer;
	#endif

	unsigned long samplerate;
	unsigned char channels;

	float gain;
	unsigned int scale;

	/* audio out */
	ao_device *audioOutDevice;

	CURL *audioFd;
	char *url;

	char doQuit;
	pthread_mutex_t pauseMutex;
};

void *BarPlayerThread (void *data);

#endif /* _PLAYER_H */
