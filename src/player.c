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

/* receive/play audio stream */

#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "player.h"
#include "config.h"
#include "ui.h"

#define byteswap32(x) (((x >> 24) & 0x000000ff) | ((x >> 8) & 0x0000ff00) | \
		((x << 8) & 0x00ff0000) | ((x << 24) & 0xff000000))

/* wait while locked, but don't slow down main thread by keeping
* locks too long */
#define QUIT_PAUSE_CHECK \
	pthread_mutex_lock (&player->pauseMutex); \
	pthread_mutex_unlock (&player->pauseMutex); \
	if (player->doQuit) { \
		return 0; \
	}

/* pandora uses float values with 2 digits precision. Scale them by 100 to get
 * a "nice" integer */
#define RG_SCALE_FACTOR 100

/* FIXME: Hardcoded bitrate (kbit/s) */
#define MP3_BITRATE 128

/*	compute replaygain scale factor
 *	algo taken from here: http://www.dsprelated.com/showmessage/29246/1.php
 *	mpd does the same
 *	@param apply this gain
 *	@return this * yourvalue = newgain value
 */
inline unsigned int computeReplayGainScale (float applyGain) {
	return pow (10.0, applyGain / 20.0) * RG_SCALE_FACTOR;
}

/*	apply replaygain to signed short value
 *	@param value
 *	@param replaygain scale (calculated by computeReplayGainScale)
 *	@return scaled value
 */
inline signed short int applyReplayGain (signed short int value,
		unsigned int scale) {
	int tmpReplayBuf = value * scale;
	/* avoid clipping */
	if (tmpReplayBuf > INT16_MAX*RG_SCALE_FACTOR) {
		return INT16_MAX;
	} else if (tmpReplayBuf < INT16_MIN*RG_SCALE_FACTOR) {
		return INT16_MIN;
	} else {
		return tmpReplayBuf / RG_SCALE_FACTOR;
	}
}

/*	Refill player's buffer with dataSize of data
 *	@param player structure
 *	@param new data
 *	@param data size
 *	@return 1 on success, 0 when buffer overflow occured
 */
inline int BarPlayerBufferFill (struct audioPlayer *player, char *data,
		size_t dataSize) {
	/* fill buffer */
	if (player->bufferFilled + dataSize > sizeof (player->buffer)) {
		BarUiMsg (MSG_ERR, PACKAGE ": Buffer overflow!\n");
		return 0;
	}
	memcpy (player->buffer+player->bufferFilled, data, dataSize);
	player->bufferFilled += dataSize;
	player->bufferRead = 0;
	player->bytesReceived += dataSize;
	return 1;
}

/*	move data beginning from read pointer to buffer beginning and
 *	overwrite data already read from buffer
 *	@param player structure
 *	@return nothing at all
 */
inline void BarPlayerBufferMove (struct audioPlayer *player) {
	/* move remaining bytes to buffer beginning */
	memmove (player->buffer, player->buffer + player->bufferRead,
			(player->bufferFilled - player->bufferRead));
	player->bufferFilled -= player->bufferRead;
}

#ifdef ENABLE_FAAD

/*	play aac stream
 *	@param streamed data
 *	@param block size
 *	@param received blocks
 *	@param extra data (player data)
 *	@return received bytes or less on error
 */
size_t BarPlayerAACCurlCb (void *ptr, size_t size, size_t nmemb, void *stream) {
	char *data = ptr;
	struct audioPlayer *player = stream;

	QUIT_PAUSE_CHECK;

	if (!BarPlayerBufferFill (player, data, size*nmemb)) {
		return 0;
	}

	if (player->mode == PLAYER_RECV_DATA) {
		short int *aacDecoded;
		NeAACDecFrameInfo frameInfo;
		size_t i;

		while ((player->bufferFilled - player->bufferRead) >
				player->sampleSize[player->sampleSizeCurr]) {
			/* decode frame */
			aacDecoded = NeAACDecDecode(player->aacHandle, &frameInfo,
					player->buffer + player->bufferRead,
					player->sampleSize[player->sampleSizeCurr]);
			if (frameInfo.error != 0) {
				BarUiMsg (MSG_ERR, PACKAGE ": Decoding error: %s\n",
						NeAACDecGetErrorMessage (frameInfo.error));
				break;
			}
			for (i = 0; i < frameInfo.samples; i++) {
				aacDecoded[i] = applyReplayGain (aacDecoded[i], player->scale);
			}
			/* ao_play needs bytes: 1 sample = 16 bits = 2 bytes */
			ao_play (player->audioOutDevice, (char *) aacDecoded,
					frameInfo.samples * 2);
			/* add played frame length to played time, explained below */
			player->songPlayed += (float) frameInfo.samples *
					(float) BAR_PLAYER_MS_TO_S_FACTOR /
					(float) player->samplerate / (float) player->channels;
			player->bufferRead += frameInfo.bytesconsumed;
			player->sampleSizeCurr++;
			/* going through this loop can take up to a few seconds =>
			 * allow earlier thread abort */
			QUIT_PAUSE_CHECK;
		}
	} else {
		if (player->mode == PLAYER_INITIALIZED) {
			while (player->bufferRead+4 < player->bufferFilled) {
				if (memcmp (player->buffer + player->bufferRead, "esds",
						4) == 0) {
					player->mode = PLAYER_FOUND_ESDS;
					player->bufferRead += 4;
					break;
				}
				player->bufferRead++;
			}
		}
		if (player->mode == PLAYER_FOUND_ESDS) {
			/* FIXME: is this the correct way? */
			/* we're gonna read 10 bytes */
			while (player->bufferRead+1+4+5 < player->bufferFilled) {
				if (memcmp (player->buffer + player->bufferRead,
						"\x05\x80\x80\x80", 4) == 0) {
					ao_sample_format format;
					int audioOutDriver;

					/* +1+4 needs to be replaced by <something>! */
					player->bufferRead += 1+4;
					char err = NeAACDecInit2 (player->aacHandle, player->buffer +
							player->bufferRead, 5, &player->samplerate,
							&player->channels);
					player->bufferRead += 5;
					if (err != 0) {
						BarUiMsg (MSG_ERR, PACKAGE ": Error while "
								"initializing audio decoder"
								"(%i)\n", err);
						return 0;
					}
					audioOutDriver = ao_default_driver_id();
					format.bits = 16;
					format.channels = player->channels;
					format.rate = player->samplerate;
					format.byte_format = AO_FMT_LITTLE;
					player->audioOutDevice = ao_open_live (audioOutDriver,
							&format, NULL);
					player->mode = PLAYER_AUDIO_INITIALIZED;
					break;
				}
				player->bufferRead++;
			}
		}
		if (player->mode == PLAYER_AUDIO_INITIALIZED) {
			while (player->bufferRead+4+8 < player->bufferFilled) {
				if (memcmp (player->buffer + player->bufferRead, "stsz",
						4) == 0) {
					player->mode = PLAYER_FOUND_STSZ;
					player->bufferRead += 4;
					/* skip version and unknown */
					player->bufferRead += 8;
					break;
				}
				player->bufferRead++;
			}
		}
		/* get frame sizes */
		if (player->mode == PLAYER_FOUND_STSZ) {
			while (player->bufferRead+4 < player->bufferFilled) {
				/* how many frames do we have? */
				if (player->sampleSizeN == 0) {
					/* mp4 uses big endian, convert */
					player->sampleSizeN =
							byteswap32 (*((int *) (player->buffer +
							player->bufferRead)));
					player->sampleSize = calloc (player->sampleSizeN,
							sizeof (player->sampleSizeN));
					player->bufferRead += 4;
					player->sampleSizeCurr = 0;
					/* set up song duration (assuming one frame always contains
					 * the same number of samples); numbers are too huge => use
					 * float
					 * calculation: channels * number of frames * samples per
					 * frame / samplerate */
					/* FIXME: Hard-coded number of samples per frame */
					player->songDuration = (float) player->sampleSizeN *
							4096.0 * (float) BAR_PLAYER_MS_TO_S_FACTOR /
							(float) player->samplerate /
							(float) player->channels;
					break;
				} else {
					player->sampleSize[player->sampleSizeCurr] =
							byteswap32 (*((int *) (player->buffer +
							player->bufferRead)));
					player->sampleSizeCurr++;
					player->bufferRead += 4;
				}
				/* all sizes read, nearly ready for data mode */
				if (player->sampleSizeCurr >= player->sampleSizeN) {
					player->mode = PLAYER_SAMPLESIZE_INITIALIZED;
					break;
				}
			}
		}
		/* search for data atom and let the show begin... */
		if (player->mode == PLAYER_SAMPLESIZE_INITIALIZED) {
			while (player->bufferRead+4 < player->bufferFilled) {
				if (memcmp (player->buffer + player->bufferRead, "mdat",
						4) == 0) {
					player->mode = PLAYER_RECV_DATA;
					player->sampleSizeCurr = 0;
					player->bufferRead += 4;
					break;
				}
				player->bufferRead++;
			}
		}
	}

	BarPlayerBufferMove (player);

	return size*nmemb;
}

#endif /* ENABLE_FAAD */

#ifdef ENABLE_MAD

/*	convert mad's internal fixed point format to short int
 *	@param mad fixed
 *	@return short int
 */
inline signed short int BarPlayerMadToShort (mad_fixed_t fixed) {
	/* Clipping */
	if (fixed >= MAD_F_ONE) {
		return SHRT_MAX;
	} else if (fixed <= -MAD_F_ONE) {
		return -SHRT_MAX;
	}

	/* Conversion */
	return (signed short int) (fixed >> (MAD_F_FRACBITS - 15));
}

size_t BarPlayerMp3CurlCb (void *ptr, size_t size, size_t nmemb, void *stream) {
	char *data = ptr;
	struct audioPlayer *player = stream;
	size_t i;

	QUIT_PAUSE_CHECK;

	if (!BarPlayerBufferFill (player, data, size*nmemb)) {
		return 0;
	}

	/* initialize song length */
	if (player->mode < PLAYER_SAMPLESIZE_INITIALIZED) {
		double contentLength = 0;
		/* FIXME: curl's documentation says we should call curl_easy_getinfo
		 * _after_ a transfer finished */
		curl_easy_getinfo (player->audioFd, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
				&contentLength);
		player->songDuration = contentLength / ((float) MP3_BITRATE * 1024.0 /
				(float) BAR_PLAYER_MS_TO_S_FACTOR / 8.0);
	}

	/* some "prebuffering" */
	if (player->mode < PLAYER_RECV_DATA &&
			player->bufferFilled < sizeof (player->buffer) / 10) {
		return size*nmemb;
	}

	mad_stream_buffer (&player->mp3Stream, player->buffer,
			player->bufferFilled);
	player->mp3Stream.error = 0;
	do {
		/* channels * max samples, found in mad.h */
		signed short int madDecoded[2*1152], *madPtr = madDecoded;

		if (mad_frame_decode (&player->mp3Frame, &player->mp3Stream) != 0) {
			if (player->mp3Stream.error != MAD_ERROR_BUFLEN) {
				BarUiMsg (MSG_ERR, PACKAGE ": mp3 decoding error: %s\n",
						mad_stream_errorstr (&player->mp3Stream));
				return 0;
			} else {
				/* rebuffering required => exit loop */
				break;
			}
		}
		mad_synth_frame (&player->mp3Synth, &player->mp3Frame);
		for (i = 0; i < player->mp3Synth.pcm.length; i++) {
			/* left channel */
			*(madPtr++) = applyReplayGain (BarPlayerMadToShort (
					player->mp3Synth.pcm.samples[0][i]), player->scale);

			/* right channel */
			*(madPtr++) = applyReplayGain (BarPlayerMadToShort (
					player->mp3Synth.pcm.samples[1][i]), player->scale);
		}
		if (player->mode < PLAYER_AUDIO_INITIALIZED) {
			ao_sample_format format;
			int audioOutDriver;

			player->channels = player->mp3Synth.pcm.channels;
			player->samplerate = player->mp3Synth.pcm.samplerate;
			audioOutDriver = ao_default_driver_id();
			format.bits = 16;
			format.channels = player->channels;
			format.rate = player->samplerate;
			format.byte_format = AO_FMT_LITTLE;
			player->audioOutDevice = ao_open_live (audioOutDriver,
					&format, NULL);
			/* must be > PLAYER_SAMPLESIZE_INITIALIZED, otherwise time won't
			 * be visible to user (ugly, but mp3 decoding != aac decoding) */
			player->mode = PLAYER_RECV_DATA;
		}
		/* samples * length * channels */
		ao_play (player->audioOutDevice, (char *) madDecoded,
				player->mp3Synth.pcm.length * 2 * 2);

		/* avoid division by 0 */
		if (player->mode == PLAYER_RECV_DATA) {
			/* same calculation as in aac player; don't need to divide by
			 * channels, length is number of samples for _one_ channel */
			player->songPlayed += (float) player->mp3Synth.pcm.length *
					(float) BAR_PLAYER_MS_TO_S_FACTOR /
					(float) player->samplerate;
		}

		QUIT_PAUSE_CHECK;
	} while (player->mp3Stream.error != MAD_ERROR_BUFLEN);

	player->bufferRead += player->mp3Stream.next_frame - player->buffer;

	BarPlayerBufferMove (player);

	return size*nmemb;
}
#endif /* ENABLE_MAD */

/*	player thread; for every song a new thread is started
 *	@param aacPlayer structure
 *	@return NULL NULL NULL ...
 */
void *BarPlayerThread (void *data) {
	struct audioPlayer *player = data;
	#ifdef ENABLE_FAAD
	NeAACDecConfigurationPtr conf;
	#endif
	CURLcode curlRet = 0;

	/* init handles */
	pthread_mutex_init (&player->pauseMutex, NULL);
	player->audioFd = curl_easy_init ();

	switch (player->audioFormat) {
		#ifdef ENABLE_FAAD
		case PIANO_AF_AACPLUS:
			player->aacHandle = NeAACDecOpen();
			/* set aac conf */
			conf = NeAACDecGetCurrentConfiguration(player->aacHandle);
			conf->outputFormat = FAAD_FMT_16BIT;
		    conf->downMatrix = 1;
			NeAACDecSetConfiguration(player->aacHandle, conf);

			curl_easy_setopt (player->audioFd, CURLOPT_WRITEFUNCTION,
					BarPlayerAACCurlCb);
			break;
		#endif /* ENABLE_FAAD */

		#ifdef ENABLE_MAD
		case PIANO_AF_MP3:
			mad_stream_init (&player->mp3Stream);
			mad_frame_init (&player->mp3Frame);
			mad_synth_init (&player->mp3Synth);

			curl_easy_setopt (player->audioFd, CURLOPT_WRITEFUNCTION,
					BarPlayerMp3CurlCb);
			break;
		#endif /* ENABLE_MAD */

		default:
			BarUiMsg (MSG_ERR, PACKAGE ": Unsupported audio format!\n");
			return NULL;
			break;
	}
	
	/* init replaygain */
	player->scale = computeReplayGainScale (player->gain);

	/* init curl */
	curl_easy_setopt (player->audioFd, CURLOPT_URL, player->url);
	curl_easy_setopt (player->audioFd, CURLOPT_WRITEDATA, (void *) player);
	curl_easy_setopt (player->audioFd, CURLOPT_USERAGENT, PACKAGE);
	curl_easy_setopt (player->audioFd, CURLOPT_CONNECTTIMEOUT, 60);
	/* start downloading from beginning of file */
	curl_easy_setopt (player->audioFd, CURLOPT_RESUME_FROM, 0);

	player->mode = PLAYER_INITIALIZED;

	/* This loop should work around song abortions by requesting the
	 * missing part of the song */
	do {
		curl_easy_setopt (player->audioFd, CURLOPT_RESUME_FROM,
				player->bytesReceived);
		curlRet = curl_easy_perform (player->audioFd);
	} while (curlRet == CURLE_PARTIAL_FILE || curlRet == CURLE_RECV_ERROR);

	switch (player->audioFormat) {
		#ifdef ENABLE_FAAD
		case PIANO_AF_AACPLUS:
			NeAACDecClose(player->aacHandle);
			break;
		#endif /* ENABLE_FAAD */

		#ifdef ENABLE_MAD
		case PIANO_AF_MP3:
			mad_synth_finish (&player->mp3Synth);
			mad_frame_finish (&player->mp3Frame);
			mad_stream_finish (&player->mp3Stream);
			break;
		#endif /* ENABLE_MAD */

		default:
			/* this should never happen: thread is aborted above */
			break;
	}
	ao_close(player->audioOutDevice);
	curl_easy_cleanup (player->audioFd);
	#ifdef ENABLE_FAAD
	if (player->sampleSize != NULL) {
		free (player->sampleSize);
	}
	#endif /* ENABLE_FAAD */
	pthread_mutex_destroy (&player->pauseMutex);

	player->mode = PLAYER_FINISHED_PLAYBACK;

	return NULL;
}
