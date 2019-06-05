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

/* receive/play audio stream.
 *
 * There are two threads involved here:
 * BarPlayerThread
 * 		Sets up the stream and fetches the data into a ffmpeg buffersrc
 * BarAoPlayThread
 * 		Reads data from the filter chain’s sink and hands it over to libao for
 * 		playback.
 * 
 */

#include "config.h"

#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <arpa/inet.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#ifdef HAVE_LIBAVFILTER_AVCODEC_H
/* required by ffmpeg1.2 for avfilter_copy_buf_props */
#include <libavfilter/avcodec.h>
#endif
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/frame.h>

#include "player.h"
#include "ui.h"
#include "ui_types.h"

/* default sample format */
const enum AVSampleFormat avformat = AV_SAMPLE_FMT_S16;

static void printError (const BarSettings_t * const settings,
		const char * const msg, int ret) {
	char avmsg[128];
	av_strerror (ret, avmsg, sizeof (avmsg));
	BarUiMsg (settings, MSG_ERR, "%s (%s)\n", msg, avmsg);
}

/*	global initialization
 */
void BarPlayerInit (player_t * const p, const BarSettings_t * const settings) {
	ao_initialize ();
	av_log_set_level (AV_LOG_FATAL);
#ifdef HAVE_AV_REGISTER_ALL
	av_register_all ();
#endif
#ifdef HAVE_AVFILTER_REGISTER_ALL
	avfilter_register_all ();
#endif
#ifdef HAVE_AVFORMAT_NETWORK_INIT
	avformat_network_init ();
#endif

	pthread_mutex_init (&p->lock, NULL);
	pthread_cond_init (&p->cond, NULL);
	pthread_mutex_init (&p->aoplayLock, NULL);
	pthread_cond_init (&p->aoplayCond, NULL);
	BarPlayerReset (p);
	p->settings = settings;
}

void BarPlayerDestroy (player_t * const p) {
	pthread_cond_destroy (&p->cond);
	pthread_mutex_destroy (&p->lock);
	pthread_cond_destroy (&p->aoplayCond);
	pthread_mutex_destroy (&p->aoplayLock);

#ifdef HAVE_AVFORMAT_NETWORK_INIT
	avformat_network_deinit ();
#endif
	ao_shutdown ();
}

void BarPlayerReset (player_t * const p) {
	p->doQuit = false;
	p->doPause = false;
	p->songDuration = 0;
	p->songPlayed = 0;
	p->mode = PLAYER_DEAD;
	p->fvolume = NULL;
	p->fgraph = NULL;
	p->fctx = NULL;
	p->st = NULL;
	p->cctx = NULL;
	p->fbufsink = NULL;
	p->fabuf = NULL;
	p->streamIdx = -1;
	p->lastTimestamp = 0;
	p->interrupted = 0;
	p->aoDev = NULL;
}

/*	Update volume filter
 */
void BarPlayerSetVolume (player_t * const player) {
	assert (player != NULL);

	if (player->mode != PLAYER_PLAYING) {
		return;
	}

	int ret;
#ifdef HAVE_AVFILTER_GRAPH_SEND_COMMAND
	/* ffmpeg and libav disagree on the type of this option (string vs. double)
	 * -> print to string and let them parse it again */
	char strbuf[16];
	snprintf (strbuf, sizeof (strbuf), "%fdB",
			player->settings->volume + (player->gain * player->settings->gainMul));
	assert (player->fgraph != NULL);
	if ((ret = avfilter_graph_send_command (player->fgraph, "volume", "volume",
					strbuf, NULL, 0, 0)) < 0) {
#else
	/* convert from decibel */
	const double volume = pow (10, (player->settings->volume + (player->gain * player->settings->gainMul)) / 20);
	/* libav does not provide other means to set this right now. it might not
	 * even work everywhere. */
	assert (player->fvolume != NULL);
	if ((ret = av_opt_set_double (player->fvolume->priv, "volume", volume,
			0)) != 0) {
#endif
		printError (player->settings, "Cannot set volume", ret);
	}
}

#define softfail(msg) \
	printError (player->settings, msg, ret); \
	return false;

/*	ffmpeg callback for blocking functions, returns 1 to abort function
 */
static int intCb (void * const data) {
	player_t * const player = data;
	assert (player != NULL);
	if (player->interrupted > 1) {
		/* got a sigint multiple times, quit pianobar (handled by main.c). */
		pthread_mutex_lock (&player->lock);
		player->doQuit = true;
		pthread_mutex_unlock (&player->lock);
		return 1;
	} else if (player->interrupted != 0) {
		/* the request is retried with the same player context */
		player->interrupted = 0;
		return 1;
	} else {
		return 0;
	}
}

static bool openStream (player_t * const player) {
	assert (player != NULL);
	/* no leak? */
	assert (player->fctx == NULL);

	int ret;

	/* stream setup */
	player->fctx = avformat_alloc_context ();
	player->fctx->interrupt_callback.callback = intCb;
	player->fctx->interrupt_callback.opaque = player;

	/* in microseconds */
	unsigned long int timeout = player->settings->timeout*1000000;
	char timeoutStr[16];
	ret = snprintf (timeoutStr, sizeof (timeoutStr), "%lu", timeout);
	assert (ret < sizeof (timeoutStr));
	AVDictionary *options = NULL;
	av_dict_set (&options, "timeout", timeoutStr, 0);

	assert (player->url != NULL);
	if ((ret = avformat_open_input (&player->fctx, player->url, NULL, &options)) < 0) {
		softfail ("Unable to open audio file");
	}

	if ((ret = avformat_find_stream_info (player->fctx, NULL)) < 0) {
		softfail ("find_stream_info");
	}

	/* ignore all streams, undone for audio stream below */
	for (size_t i = 0; i < player->fctx->nb_streams; i++) {
		player->fctx->streams[i]->discard = AVDISCARD_ALL;
	}

	player->streamIdx = av_find_best_stream (player->fctx, AVMEDIA_TYPE_AUDIO,
			-1, -1, NULL, 0);
	if (player->streamIdx < 0) {
		softfail ("find_best_stream");
	}

	player->st = player->fctx->streams[player->streamIdx];
	player->st->discard = AVDISCARD_DEFAULT;

	/* decoder setup */
	if ((player->cctx = avcodec_alloc_context3 (NULL)) == NULL) {
		softfail ("avcodec_alloc_context3");
	}
	const AVCodecParameters * const cp = player->st->codecpar;
	if ((ret = avcodec_parameters_to_context (player->cctx, cp)) < 0) {
		softfail ("avcodec_parameters_to_context");
	}

	AVCodec * const decoder = avcodec_find_decoder (cp->codec_id);
	if (decoder == NULL) {
		softfail ("find_decoder");
	}

	if ((ret = avcodec_open2 (player->cctx, decoder, NULL)) < 0) {
		softfail ("codec_open2");
	}

	if (player->lastTimestamp > 0) {
		av_seek_frame (player->fctx, player->streamIdx, player->lastTimestamp, 0);
	}

	const unsigned int songDuration = av_q2d (player->st->time_base) *
			(double) player->st->duration;
	pthread_mutex_lock (&player->lock);
	player->songPlayed = 0;
	player->songDuration = songDuration;
	pthread_mutex_unlock (&player->lock);

	return true;
}

/*	Get output sample rate. Default to stream sample rate
 */
static int getSampleRate (const player_t * const player) {
	return player->settings->sampleRate == 0 ?
			cp->sample_rate :
			player->settings->sampleRate;
}

/*	setup filter chain
 */
static bool openFilter (player_t * const player) {
	/* filter setup */
	char strbuf[256];
	int ret = 0;
	AVCodecParameters * const cp = player->st->codecpar;

	if ((player->fgraph = avfilter_graph_alloc ()) == NULL) {
		softfail ("graph_alloc");
	}

	/* abuffer */
	AVRational time_base = player->st->time_base;

	snprintf (strbuf, sizeof (strbuf),
			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64, 
			time_base.num, time_base.den, cp->sample_rate,
			av_get_sample_fmt_name (player->cctx->sample_fmt),
			cp->channel_layout);
	if ((ret = avfilter_graph_create_filter (&player->fabuf,
			avfilter_get_by_name ("abuffer"), "source", strbuf, NULL,
			player->fgraph)) < 0) {
		softfail ("create_filter abuffer");
	}

	/* volume */
	if ((ret = avfilter_graph_create_filter (&player->fvolume,
			avfilter_get_by_name ("volume"), "volume", "0dB", NULL,
			player->fgraph)) < 0) {
		softfail ("create_filter volume");
	}

	/* aformat: convert float samples into something more usable */
	AVFilterContext *fafmt = NULL;
	snprintf (strbuf, sizeof (strbuf), "sample_fmts=%s:sample_rates=%d",
			av_get_sample_fmt_name (avformat), getSampleRate (player));
	if ((ret = avfilter_graph_create_filter (&fafmt,
					avfilter_get_by_name ("aformat"), "format", strbuf, NULL,
					player->fgraph)) < 0) {
		softfail ("create_filter aformat");
	}

	/* abuffersink */
	if ((ret = avfilter_graph_create_filter (&player->fbufsink,
			avfilter_get_by_name ("abuffersink"), "sink", NULL, NULL,
			player->fgraph)) < 0) {
		softfail ("create_filter abuffersink");
	}

	/* connect filter: abuffer -> volume -> aformat -> abuffersink */
	if (avfilter_link (player->fabuf, 0, player->fvolume, 0) != 0 ||
			avfilter_link (player->fvolume, 0, fafmt, 0) != 0 ||
			avfilter_link (fafmt, 0, player->fbufsink, 0) != 0) {
		softfail ("filter_link");
	}

	if ((ret = avfilter_graph_config (player->fgraph, NULL)) < 0) {
		softfail ("graph_config");
	}

	return true;
}

/*	setup libao
 */
static bool openDevice (player_t * const player) {
	const AVCodecParameters * const cp = player->st->codecpar;

	ao_sample_format aoFmt;
	memset (&aoFmt, 0, sizeof (aoFmt));
	aoFmt.bits = av_get_bytes_per_sample (avformat) * 8;
	assert (aoFmt.bits > 0);
	aoFmt.channels = cp->channels;
	aoFmt.rate = getSampleRate (player);
	aoFmt.byte_format = AO_FMT_NATIVE;

	int driver = ao_default_driver_id ();
	if ((player->aoDev = ao_open_live (driver, &aoFmt, NULL)) == NULL) {
		BarUiMsg (player->settings, MSG_ERR, "Cannot open audio device.\n");
		return false;
	}

	return true;
}

/*	Operating on shared variables and must be protected by mutex
 */

static bool shouldQuit (player_t * const player) {
	pthread_mutex_lock (&player->lock);
	const bool ret = player->doQuit;
	pthread_mutex_unlock (&player->lock);
	return ret;
}

static void changeMode (player_t * const player, unsigned int mode) {
	pthread_mutex_lock (&player->lock);
	player->mode = mode;
	pthread_mutex_unlock (&player->lock);
}

BarPlayerMode BarPlayerGetMode (player_t * const player) {
	pthread_mutex_lock (&player->lock);
	const BarPlayerMode ret = player->mode;
	pthread_mutex_unlock (&player->lock);
	return ret;
}

/*	decode and play stream. returns 0 or av error code.
 */
static int play (player_t * const player) {
	assert (player != NULL);
	const int64_t minBufferHealth = player->settings->bufferSecs;

	AVPacket pkt;
	AVCodecContext * const cctx = player->cctx;
	av_init_packet (&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	AVFrame *frame = NULL;
	frame = av_frame_alloc ();
	assert (frame != NULL);
	pthread_t aoplaythread;
	pthread_create (&aoplaythread, NULL, BarAoPlayThread, player);
	enum { FILL, DRAIN, DONE } drainMode = FILL;
	int ret = 0;
	const double timeBase = av_q2d (player->st->time_base);
	while (!shouldQuit (player) && drainMode != DONE) {
		if (drainMode == FILL) {
			ret = av_read_frame (player->fctx, &pkt);
			if (ret == AVERROR_EOF) {
				/* enter drain mode */
				drainMode = DRAIN;
				avcodec_send_packet (cctx, NULL);
			} else if (pkt.stream_index != player->streamIdx) {
				/* unused packet */
				av_packet_unref (&pkt);
				continue;
			} else if (ret < 0) {
				/* error, abort */
				/* mark the EOF, so that BarAoPlayThread can quit*/
				pthread_mutex_lock (&player->aoplayLock);
				const int rt = av_buffersrc_add_frame (player->fabuf, NULL);
				assert (rt == 0);
				pthread_cond_broadcast (&player->aoplayCond);
				pthread_mutex_unlock (&player->aoplayLock);
				break;
			} else {
				/* fill buffer */
				avcodec_send_packet (cctx, &pkt);
			}
		}

		while (!shouldQuit (player)) {
			ret = avcodec_receive_frame (cctx, frame);
			if (ret == AVERROR_EOF) {
				/* done draining */
				drainMode = DONE;
				/* mark the EOF*/
				pthread_mutex_lock (&player->aoplayLock);
				const int rt = av_buffersrc_add_frame (player->fabuf, NULL);
				assert (rt == 0);
				pthread_cond_broadcast (&player->aoplayCond);
				pthread_mutex_unlock (&player->aoplayLock);
				break;
			} else if (ret != 0) {
				/* no more output */
				break;
			}

			/* XXX: suppresses warning from resample filter */
			if (frame->pts == (int64_t) AV_NOPTS_VALUE) {
				frame->pts = 0;
			}
			pthread_mutex_lock (&player->aoplayLock);
			ret = av_buffersrc_write_frame (player->fabuf, frame);
			assert (ret >= 0);
			pthread_mutex_unlock (&player->aoplayLock);
			
			int64_t bufferHealth = 0;
			do {
				pthread_mutex_lock (&player->aoplayLock);
				bufferHealth = timeBase * (double) (frame->pts - player->lastTimestamp);
				if (bufferHealth > minBufferHealth) {
					/* Buffer get healthy, resume */
					pthread_cond_broadcast (&player->aoplayCond);
					/* Buffer is healthy enough, wait */
					pthread_cond_wait (&player->aoplayCond, &player->aoplayLock);
				}
				pthread_mutex_unlock (&player->aoplayLock);
			} while (bufferHealth > minBufferHealth);
		}

		av_packet_unref (&pkt);
	}
	av_frame_free (&frame);
	pthread_join (aoplaythread, NULL);

	return ret;
}

static void finish (player_t * const player) {
	ao_close (player->aoDev);
	player->aoDev = NULL;
	if (player->fgraph != NULL) {
		avfilter_graph_free (&player->fgraph);
		player->fgraph = NULL;
	}
	if (player->cctx != NULL) {
		avcodec_close (player->cctx);
		player->cctx = NULL;
	}
	if (player->fctx != NULL) {
		avformat_close_input (&player->fctx);
	}
}

/*	player thread; for every song a new thread is started
 *	@param audioPlayer structure
 *	@return PLAYER_RET_*
 */
void *BarPlayerThread (void *data) {
	assert (data != NULL);

	player_t * const player = data;
	uintptr_t pret = PLAYER_RET_OK;

	bool retry;
	do {
		retry = false;
		if (openStream (player)) {
			if (openFilter (player) && openDevice (player)) {
				changeMode (player, PLAYER_PLAYING);
				BarPlayerSetVolume (player);
				retry = play (player) == AVERROR_INVALIDDATA &&
						!player->interrupted;
			} else {
				/* filter missing or audio device busy */
				pret = PLAYER_RET_HARDFAIL;
			}
		} else {
			/* stream not found */
			pret = PLAYER_RET_SOFTFAIL;
		}
		changeMode (player, PLAYER_WAITING);
		finish (player);
	} while (retry);

	changeMode (player, PLAYER_FINISHED);

	return (void *) pret;
}

void *BarAoPlayThread (void *data) {
	assert (data != NULL);

	player_t * const player = data;

	AVFrame *filteredFrame = NULL;
	filteredFrame = av_frame_alloc ();
	assert (filteredFrame != NULL);

	int ret;
	const double timeBase = av_q2d (av_buffersink_get_time_base (player->fbufsink)),
			timeBaseSt = av_q2d (player->st->time_base);
	while (!shouldQuit(player)) {
		pthread_mutex_lock (&player->aoplayLock);
		ret = av_buffersink_get_frame (player->fbufsink, filteredFrame);
		if (ret == AVERROR_EOF || shouldQuit (player)) {
			/* we are done here */
			pthread_mutex_unlock (&player->aoplayLock);
			break;
		} else if (ret < 0) {
			/* wait for more frames */
			pthread_cond_broadcast (&player->aoplayCond);
			pthread_cond_wait (&player->aoplayCond, &player->aoplayLock);
			pthread_mutex_unlock (&player->aoplayLock);
			continue;
		}
		pthread_mutex_unlock (&player->aoplayLock);

		const int numChannels = av_get_channel_layout_nb_channels (
				filteredFrame->channel_layout);
		const int bps = av_get_bytes_per_sample (filteredFrame->format);
		ao_play (player->aoDev, (char *) filteredFrame->data[0],
				filteredFrame->nb_samples * numChannels * bps);

		const double timestamp = (double) filteredFrame->pts * timeBase;
		const unsigned int songPlayed = timestamp;

		pthread_mutex_lock (&player->lock);
		player->songPlayed = songPlayed;
		/* pausing */
		if (player->doPause) {
			do {
				pthread_cond_wait (&player->cond, &player->lock);
			} while (player->doPause);
		}
		pthread_mutex_unlock (&player->lock);

		/* lastTimestamp must be the last pts, but expressed in terms of
		 * st->time_base, not the sink’s time_base. */
		const int64_t lastTimestamp = timestamp/timeBaseSt;
		/* notify download thread, we might need more data */
		pthread_mutex_lock (&player->aoplayLock);
		player->lastTimestamp = lastTimestamp;
		pthread_cond_broadcast (&player->aoplayCond);
		pthread_mutex_unlock (&player->aoplayLock);

		av_frame_unref (filteredFrame);
	}
	av_frame_free (&filteredFrame);

	return (void *) 0;
}
