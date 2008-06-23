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

#include <unistd.h>
#include <string.h>

#include "player.h"
#include "config.h"

/*	LE to BE and reverse
 *	@param unsigned int
 *	@return byteswapped unsigned int
 */
unsigned int BarChangeByteorderUI32 (char buf[4]) {
	unsigned int ret = 0;

	ret = buf[0] << 24 & 0xffffffff;
	ret |= buf[1] << 16 & 0xffffff;
	ret |= buf[2] << 8 & 0xffff;
	ret |= buf[3] << 0 & 0xff;
	return ret;
}

/*	our heart: plays streamed music
 *	@param streamed data
 *	@param block size
 *	@param received blocks
 *	@param extra data (player data)
 *	@return received bytes or less on error
 */
size_t BarPlayerCurlCb (void *ptr, size_t size, size_t nmemb, void *stream) {
	char *data = ptr;
	struct aacPlayer *player = stream;

	if (player->doQuit) {
		return 0;
	}
	
	/* FIXME: not the best solution to poll every second, but the easiest
	 * one I know... (pthread's conditions could be another solution) */
	if (player->doPause) {
		curl_easy_pause (player->audioFd, CURLPAUSE_ALL);
		while (player->doPause && !player->doQuit) {
			sleep (1);
		}
		curl_easy_pause (player->audioFd, CURLPAUSE_CONT);
	}

	/* fill buffer */
	if (player->bufferFilled + size*nmemb > sizeof (player->buffer)) {
		printf ("Buffer overflow!\n");
		return 0;
	}
	memcpy (player->buffer+player->bufferFilled, data, size*nmemb);
	player->bufferFilled += size*nmemb;
	player->bufferRead = 0;

	if (player->mode == RECV_DATA) {
		char *aacDecoded;
		NeAACDecFrameInfo frameInfo;

		while ((player->bufferFilled - player->bufferRead) >
				player->sampleSize[player->sampleSizeCurr]) {
			/* decode frame */
			aacDecoded = NeAACDecDecode(player->aacHandle, &frameInfo,
					(unsigned char *) player->buffer + player->bufferRead,
					player->sampleSize[player->sampleSizeCurr]);
			if (frameInfo.error != 0) {
				printf ("error: %s\n\n",
						NeAACDecGetErrorMessage (frameInfo.error));
				break;
			}
			ao_play (player->audioOutDevice, aacDecoded,
					frameInfo.samples*frameInfo.channels);
			player->bufferRead += frameInfo.bytesconsumed;
			player->sampleSizeCurr++;
		}
	} else {
		if (player->mode == FIND_ESDS) {
			while (player->bufferRead+4 < player->bufferFilled) {
				if (memcmp (player->buffer + player->bufferRead, "esds",
						4) == 0) {
					player->mode = FOUND_ESDS;
					player->bufferRead += 4;
					break;
				}
				player->bufferRead++;
			}
		}
		if (player->mode == FOUND_ESDS) {
			/* FIXME: is this the correct way? */
			/* we're gonna read 10 bytes */
			while (player->bufferRead+1+4+5 < player->bufferFilled) {
				if (memcmp (player->buffer + player->bufferRead,
						"\x05\x80\x80\x80", 4) == 0) {
					ao_sample_format format;
					int audioOutDriver;

					/* +1+4 needs to be replaced by <something>! */
					player->bufferRead += 1+4;
					char err = NeAACDecInit2 (player->aacHandle,
							(unsigned char *) player->buffer +
							player->bufferRead, 5, &player->samplerate,
							&player->channels);
					player->bufferRead += 5;
					if (err != 0) {
						printf ("Error while initializing audio decoder"
								"(%i)\n", err);
						return 1;
					}
					audioOutDriver = ao_default_driver_id();
					format.bits = 16;
					format.channels = player->channels;
					format.rate = player->samplerate;
					format.byte_format = AO_FMT_LITTLE;
					player->audioOutDevice = ao_open_live (audioOutDriver,
							&format, NULL);
					player->mode = AUDIO_INITIALIZED;
					break;
				}
				player->bufferRead++;
			}
		}
		if (player->mode == AUDIO_INITIALIZED) {
			while (player->bufferRead+4+8 < player->bufferFilled) {
				if (memcmp (player->buffer + player->bufferRead, "stsz",
						4) == 0) {
					player->mode = FOUND_STSZ;
					player->bufferRead += 4;
					/* skip version and unknown */
					player->bufferRead += 8;
					break;
				}
				player->bufferRead++;
			}
		}
		/* get frame sizes */
		if (player->mode == FOUND_STSZ) {
			while (player->bufferRead+4 < player->bufferFilled) {
				/* how many frames do we have? */
				if (player->sampleSizeN == 0) {
					/* mp4 uses big endian, convert */
					player->sampleSizeN =
							BarChangeByteorderUI32 (player->buffer +
							player->bufferRead);
					player->sampleSize = calloc (player->sampleSizeN,
							sizeof (player->sampleSizeN));
					player->bufferRead += 4;
					player->sampleSizeCurr = 0;
					break;
				} else {
					player->sampleSize[player->sampleSizeCurr] =
							BarChangeByteorderUI32 (player->buffer +
							player->bufferRead);
					player->sampleSizeCurr++;
					player->bufferRead += 4;
				}
				/* all sizes read, nearly ready for data mode */
				if (player->sampleSizeCurr >= player->sampleSizeN) {
					player->mode = SAMPLESIZE_INITIALIZED;
					break;
				}
			}
		}
		/* search for data atom and let the show begin... */
		if (player->mode == SAMPLESIZE_INITIALIZED) {
			while (player->bufferRead+4 < player->bufferFilled) {
				if (memcmp (player->buffer + player->bufferRead, "mdat",
						4) == 0) {
					player->mode = RECV_DATA;
					player->sampleSizeCurr = 0;
					player->bufferRead += 4;
					break;
				}
				player->bufferRead++;
			}
		}
	}

	/* move remaining bytes to buffer beginning */
	memmove (player->buffer, player->buffer + player->bufferRead,
			(player->bufferFilled - player->bufferRead));
	player->bufferFilled = (player->bufferFilled - player->bufferRead);

	return size*nmemb;
}


/*	player thread; for every song a new thread is started
 *	@param aacPlayer structure
 *	@return NULL NULL NULL ...
 */
void *BarPlayerThread (void *data) {
	struct aacPlayer *player = data;
	NeAACDecConfigurationPtr conf;

	player->audioFd = curl_easy_init ();
	player->aacHandle = NeAACDecOpen();

	conf = NeAACDecGetCurrentConfiguration(player->aacHandle);
	conf->outputFormat = FAAD_FMT_16BIT;
    conf->downMatrix = 1;
	NeAACDecSetConfiguration(player->aacHandle, conf);

	curl_easy_setopt (player->audioFd, CURLOPT_URL, player->url);
	curl_easy_setopt (player->audioFd, CURLOPT_WRITEFUNCTION, BarPlayerCurlCb);
	curl_easy_setopt (player->audioFd, CURLOPT_WRITEDATA, player);
	curl_easy_setopt (player->audioFd, CURLOPT_USERAGENT, PACKAGE_STRING);
	curl_easy_perform (player->audioFd);

	NeAACDecClose(player->aacHandle);
	ao_close(player->audioOutDevice);
	curl_easy_cleanup (player->audioFd);
	if (player->sampleSize != NULL) {
		free (player->sampleSize);
	}

	player->finishedPlayback = 1;

	return NULL;
}
