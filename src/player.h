/*
Copyright (c) 2008 Lars-Dominik Braun

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

#include <curl/curl.h>
#include <neaacdec.h>
#include <ao/ao.h>

struct aacPlayer {
	/* buffer; should be large enough */
	char buffer[CURL_MAX_WRITE_SIZE*2];
	size_t bufferFilled;
	size_t bufferRead;
	enum {PLAYER_FREED = 0, PLAYER_INITIALIZED, PLAYER_FOUND_ESDS,
			PLAYER_AUDIO_INITIALIZED, PLAYER_FOUND_STSZ,
			PLAYER_SAMPLESIZE_INITIALIZED, PLAYER_RECV_DATA,
			PLAYER_FINISHED_PLAYBACK} mode;
	/* stsz atom: sample sizes */
	unsigned int *sampleSize;
	size_t sampleSizeN;
	size_t sampleSizeCurr;
	/* aac */
	NeAACDecHandle aacHandle;
	unsigned long samplerate;
	unsigned char channels;
	float gain;
	float scale;
	/* audio out */
	ao_device *audioOutDevice;
	char *url;
	char doQuit;
	char doPause;
	CURL *audioFd;
};

void *BarPlayerThread (void *data);

#endif /* _PLAYER_H */
