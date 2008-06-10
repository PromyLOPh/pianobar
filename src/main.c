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

#include <piano.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ao/ao.h>
#include <neaacdec.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>

struct aacPlayer {
	/* buffer */
	char *buffer;
	size_t bufferFilled;
	char lastBytes[4];
	/* got mdat atom */
	char dataMode;
	char foundEsdsAtom;
	char audioInitialized;
	/* aac */
	NeAACDecHandle aacHandle;
	unsigned long samplerate;
	unsigned char channels;
	/* audio out */
	ao_device *audioOutDevice;
	char *url;
	char finishedPlayback;
	char doQuit;
};

void dumpBuffer (char *buf, size_t len) {
	int i;
	for (i = 0; i < len; i++) {
		printf ("%02x ", buf[i] & 0xff);
	}
	printf ("\n");
}

/* FIXME: this is a huge block of _bad_ and buggy code */
int playCurlCb (void *ptr, size_t size, size_t nmemb, void *stream) {
	char *data = ptr;
	struct aacPlayer *player = stream;

	if (player->doQuit) {
		return -1;
	}

	if (player->dataMode == 1) {
		size_t bufferOffset = 0, aacBufferN;
		char *aacDecoded, *aacBuffer;
		NeAACDecFrameInfo frameInfo;

		aacBufferN = player->bufferFilled + nmemb;
		aacBuffer = calloc (aacBufferN, sizeof (char));
		memcpy (aacBuffer, player->buffer, player->bufferFilled);
		memcpy (aacBuffer + player->bufferFilled, data, nmemb * size);

		free (player->buffer);

		/* keep some bytes in buffer */
		while (bufferOffset < aacBufferN-500) {
			//printf ("play buffer at %i/%i: ", bufferOffset, aacBufferN);
			//dumpBuffer (aacBuffer+bufferOffset, 50);
			/* FIXME: well, i think we need this block length table */
			aacDecoded = NeAACDecDecode(player->aacHandle, &frameInfo,
					(unsigned char *) aacBuffer+bufferOffset,
					aacBufferN-bufferOffset);
			//printf ("bytesconsumed: %li\nerror: %i\nsamples: %lu\nsamplerate: %lu\n\n", frameInfo.bytesconsumed, frameInfo.error, frameInfo.samples, frameInfo.samplerate);
			if (frameInfo.error != 0) {
				printf ("error: %s\n\n", NeAACDecGetErrorMessage (frameInfo.error));
				break;
			}
			ao_play (player->audioOutDevice, aacDecoded,
					frameInfo.samples*frameInfo.channels);
			bufferOffset += frameInfo.bytesconsumed;
		}
		/* copy remaining bytes */
		player->buffer = calloc (aacBufferN - bufferOffset, sizeof (char));
		memcpy (player->buffer, aacBuffer+bufferOffset,
				aacBufferN - bufferOffset);
		player->bufferFilled = aacBufferN - bufferOffset;
		//printf ("player->buffer (%i) = ", aacBufferN - bufferOffset);
		//dumpBuffer (player->buffer, 50);
		free (aacBuffer);
	} else {
		player->buffer = calloc (nmemb + sizeof (player->lastBytes), size);
		/* we are going to find a 4-byte string, but curl sends fragments and
		 * if the identifier is cut into two pieces, we will get into big
		 * trouble... */
		memcpy (player->buffer, player->lastBytes, sizeof (player->lastBytes));
		memcpy (player->buffer+sizeof (player->lastBytes), data, size*nmemb);

		char *searchBegin = player->buffer;

		if (!player->foundEsdsAtom) {
			while (searchBegin < player->buffer + nmemb) {
				if (memcmp (searchBegin, "esds", 4) == 0) {
					player->foundEsdsAtom = 1;
					break;
				}
				searchBegin++;
			}
		}
		if (player->foundEsdsAtom && !player->audioInitialized) {
			/* FIXME: is this the correct way? */
			while (searchBegin < player->buffer + nmemb) {
				if (memcmp (searchBegin, "\x05\x80\x80\x80", 4) == 0) {
					ao_sample_format format;
					int audioOutDriver;

					char err = NeAACDecInit2 (player->aacHandle,
							(unsigned char *) searchBegin+1+4, 5,
							&player->samplerate, &player->channels);
					if (err != 0) {
						printf ("whoops... %i\n", err);
						return 1;
					}
					//printf ("samplerate: %li\nchannels: %i\n\n",
					//		player->samplerate, player->channels);
					audioOutDriver = ao_default_driver_id();
					format.bits = 16;
					format.channels = player->channels;
					format.rate = player->samplerate;
					format.byte_format = AO_FMT_LITTLE;
					player->audioOutDevice = ao_open_live (audioOutDriver, &format, NULL);
					player->audioInitialized = 1;
					break;
				}
				searchBegin++;
			}
		}
		if (player->audioInitialized) {
			while (searchBegin < player->buffer + nmemb) {
				if (memcmp (searchBegin, "mdat", 4) == 0) {
					player->dataMode = 1;
					memmove (player->buffer, searchBegin+4, nmemb - (searchBegin-player->buffer));
					player->bufferFilled = nmemb - (searchBegin-player->buffer);
					//printf ("copied %i bytes: ", nmemb - (searchBegin-player->buffer));
					//dumpBuffer (player->buffer, 50);
					break;
				}
				searchBegin++;
			}
		}
		if (!player->dataMode) {
			memcpy (player->lastBytes, data + (size * nmemb - sizeof (player->lastBytes)),
					sizeof (player->lastBytes));
			//printf ("last bytes: ");
			//dumpBuffer (player->lastBytes, 4);
			free (player->buffer);
		}
	}

	return size*nmemb;
}

void *threadPlayUrl (void *data) {
	struct aacPlayer *player = data;
	NeAACDecConfigurationPtr conf;
	CURL *audioFd;

	audioFd = curl_easy_init ();
	player->aacHandle = NeAACDecOpen();

	conf = NeAACDecGetCurrentConfiguration(player->aacHandle);
	conf->outputFormat = FAAD_FMT_16BIT;
    conf->downMatrix = 1;
	NeAACDecSetConfiguration(player->aacHandle, conf);

	curl_easy_setopt (audioFd, CURLOPT_URL, player->url);
	curl_easy_setopt (audioFd, CURLOPT_WRITEFUNCTION, playCurlCb);
	curl_easy_setopt (audioFd, CURLOPT_WRITEDATA, player);
	/* at the moment we don't need publicity */
	curl_easy_setopt (audioFd, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9pre) Gecko/2008051115 Firefox/3.0pre");
	curl_easy_perform (audioFd);

	free (player->buffer);
	NeAACDecClose(player->aacHandle);
	ao_close(player->audioOutDevice);
	curl_easy_cleanup (audioFd);

	player->finishedPlayback = 1;

	return NULL;
}

PianoStation_t *selectStation (PianoHandle_t *ph) {
	PianoStation_t *curStation;
	size_t i;

	printf ("which station do you want to listen to?\n");
	i = 0;
	curStation = ph->stations;
	while (curStation != NULL) {
		printf ("%2i) %s\n", i, curStation->name);
		curStation = curStation->next;
		i++;
	}
	scanf ("%i", &i);
	curStation = ph->stations;
	while (curStation != NULL && i > 0) {
		curStation = curStation->next;
		i--;
	}
	return curStation;
}

int main (int argc, char **argv) {
	PianoHandle_t ph;
	struct aacPlayer player;
	char doQuit = 0;
	PianoSong_t *curSong = NULL;
	PianoStation_t *curStation;
	struct termios termopts;

	/* init some things */
	curl_global_init (CURL_GLOBAL_SSL);
	ao_initialize();
	PianoInit (&ph);

	/* no buffering for stdin */
	tcgetattr (fileno (stdin), &termopts);
	termopts.c_lflag &= ~ICANON;
	tcsetattr(fileno (stdin), TCSANOW, &termopts);
	setvbuf (stdin, NULL, _IONBF, 1);

	PianoConnect (&ph, argv[1], argv[2]);
	PianoGetStations (&ph);
	printf ("webAuthToken: %s\nauthToken: %s\nlistenerId: %s\n", ph.user.webAuthToken, ph.user.authToken, ph.user.listenerId);

	/* select station */
	curStation = selectStation (&ph);
	printf ("playing station %s\n", curStation->name);
	PianoGetPlaylist (&ph, curStation->id);

	curSong = ph.playlist;
	/* play first track */
	while (!doQuit) {
		PianoSong_t *lastSong = NULL;
		pthread_t playerThread;
		printf ("%s by %s\n", curSong->title, curSong->artist);
		memset (&player, 0, sizeof (player));
		player.url = strdup (curSong->audioUrl);

		/* start player */
		pthread_create (&playerThread, NULL, threadPlayUrl, &player);

		/* in the meantime: wait for user actions */
		while (!player.finishedPlayback) {
			struct pollfd polls = {fileno (stdin), POLLIN, POLLIN};
			char buf;

			if (poll (&polls, 1, 1000) > 0) {
				read (fileno (stdin), &buf, sizeof (buf));
				switch (buf) {
					case '?':
						printf ("n\tnext track\nq\tquit\ns\tchange station\n");
						break;

					case 'b':
						player.doQuit = 1;
						PianoRateTrack (&ph, curStation, curSong,
								PIANO_RATE_BAN);
						printf ("Banned.\n");
						/* pandora does this too, I think */
						PianoDestroyPlaylist (&ph);
						break;

					case 'l':
						PianoRateTrack (&ph, curStation, curSong,
								PIANO_RATE_LOVE);
						printf ("Loved.\n");
						break;

					case 'n':
						player.doQuit = 1;
						break;

					case 'q':
						doQuit = 1;
						player.doQuit = 1;
						break;

					case 's':
						player.doQuit = 1;
						PianoDestroyPlaylist (&ph);
						curStation = selectStation (&ph);
						printf ("changed station to %s\n", curStation->name);
						break;
				}
			}
		}
		pthread_join (playerThread, NULL);

		free (player.url);
		/* what's next? */
		lastSong = curSong;
		curSong = lastSong->next;
		if (curSong == NULL && !doQuit) {
			printf ("receiving new playlist\n");
			PianoDestroyPlaylist (&ph);
			PianoGetPlaylist (&ph, curStation->id);
			curSong = ph.playlist;
			if (curSong == NULL) {
				printf ("no tracks left\n");
				doQuit = 1;
			}
		}
	}

	/* destroy everything (including the world...) */
	PianoDestroy (&ph);
	curl_global_cleanup ();
	ao_shutdown();

	return 0;
}
