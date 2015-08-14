/*
Copyright (c) 2008-2014
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

/* receive/play audio stream */

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
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#ifdef HAVE_LIBAVFILTER_AVCODEC_H
/* required by ffmpeg1.2 for avfilter_copy_buf_props */
#include <libavfilter/avcodec.h>
#endif
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/frame.h>
#ifndef HAVE_AV_TIMEOUT
#include <libavutil/time.h>
#endif

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
 *
 *	XXX: in theory we can select the filters/formats we want to support, but
 *	this does not work in practise.
 */
void BarPlayerInit () {
	ao_initialize ();
	av_register_all ();
	avfilter_register_all ();
	avformat_network_init ();
}

void BarPlayerDestroy () {
	avformat_network_deinit ();
	ao_shutdown ();
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
			player->settings->volume + player->gain);
	assert (player->fgraph != NULL);
	if ((ret = avfilter_graph_send_command (player->fgraph, "volume", "volume",
					strbuf, NULL, 0, 0)) < 0) {
#else
	/* convert from decibel */
	const double volume = pow (10, (player->settings->volume + player->gain) / 20);
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

#ifndef HAVE_AV_TIMEOUT
/*	interrupt callback for libav, which lacks a timeout option
 *
 *	obviously calling ping() a lot of times and then calling av_gettime here
 *	again is rather inefficient.
 */
static int intCb (void * const data) {
	player_t * const player = data;
	assert (player != NULL);
	/* 10 seconds timeout (usec) */
	return (av_gettime () - player->ping) > 10*1000000;
}

#define ping() player->ping = av_gettime ()
#else
#define ping()
#endif

static bool openStream (player_t * const player) {
	assert (player != NULL);
	/* no leak? */
	assert (player->fctx == NULL);

	int ret;

	/* stream setup */
	AVDictionary *options = NULL;
#ifdef HAVE_AV_TIMEOUT
	/* 10 seconds timeout on TCP r/w */
	av_dict_set (&options, "timeout", "10000000", 0);
#else
	/* libav does not support the timeout option above. the workaround stores
	 * the current time with ping() now and then, registers an interrupt
	 * callback (below) and compares saved/current time in this callback. it’s
	 * not bullet-proof, but seems to work fine for av_read_frame. */
	player->fctx = avformat_alloc_context ();
	player->fctx->interrupt_callback.callback = intCb;
	player->fctx->interrupt_callback.opaque = player;
#endif

	assert (player->url != NULL);
	ping ();
	if ((ret = avformat_open_input (&player->fctx, player->url, NULL, &options)) < 0) {
		softfail ("Unable to open audio file");
	}

	ping ();
	if ((ret = avformat_find_stream_info (player->fctx, NULL)) < 0) {
		softfail ("find_stream_info");
	}

	/* ignore all streams, undone for audio stream below */
	for (size_t i = 0; i < player->fctx->nb_streams; i++) {
		player->fctx->streams[i]->discard = AVDISCARD_ALL;
	}

	ping ();
	player->streamIdx = av_find_best_stream (player->fctx, AVMEDIA_TYPE_AUDIO,
			-1, -1, NULL, 0);
	if (player->streamIdx < 0) {
		softfail ("find_best_stream");
	}

	player->st = player->fctx->streams[player->streamIdx];
	AVCodecContext * const cctx = player->st->codec;
	player->st->discard = AVDISCARD_DEFAULT;

	/* decoder setup */
	AVCodec * const decoder = avcodec_find_decoder (cctx->codec_id);
	if (decoder == NULL) {
		softfail ("find_decoder");
	}

	if ((ret = avcodec_open2 (cctx, decoder, NULL)) < 0) {
		softfail ("codec_open2");
	}

	if (player->lastTimestamp > 0) {
		ping ();
		av_seek_frame (player->fctx, player->streamIdx, player->lastTimestamp, 0);
	}

	player->songPlayed = 0;
	player->songDuration = av_q2d (player->st->time_base) *
			(double) player->st->duration;

	return true;
}

/*	setup filter chain
 */
static bool openFilter (player_t * const player) {
	/* filter setup */
	char strbuf[256];
	int ret = 0;
	AVCodecContext * const cctx = player->st->codec;

	if ((player->fgraph = avfilter_graph_alloc ()) == NULL) {
		softfail ("graph_alloc");
	}

	/* abuffer */
	AVRational time_base = player->st->time_base;

	/* Workaround for a bug in libav-11, which reports an invalid channel
	 * layout mp3 files */
	if (cctx->channel_layout == 0) {
		cctx->channel_layout = av_get_default_channel_layout (cctx->channels);
	}

	snprintf (strbuf, sizeof (strbuf),
			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64, 
			time_base.num, time_base.den, cctx->sample_rate,
			av_get_sample_fmt_name (cctx->sample_fmt),
			cctx->channel_layout);
	if ((ret = avfilter_graph_create_filter (&player->fabuf,
			avfilter_get_by_name ("abuffer"), NULL, strbuf, NULL,
			player->fgraph)) < 0) {
		softfail ("create_filter abuffer");
	}

	/* volume */
	if ((ret = avfilter_graph_create_filter (&player->fvolume,
			avfilter_get_by_name ("volume"), NULL, "0dB", NULL,
			player->fgraph)) < 0) {
		softfail ("create_filter volume");
	}

	/* aformat: convert float samples into something more usable */
	AVFilterContext *fafmt = NULL;
	snprintf (strbuf, sizeof (strbuf), "sample_fmts=%s",
			av_get_sample_fmt_name (avformat));
	if ((ret = avfilter_graph_create_filter (&fafmt,
					avfilter_get_by_name ("aformat"), NULL, strbuf, NULL,
					player->fgraph)) < 0) {
		softfail ("create_filter aformat");
	}

	/* abuffersink */
	if ((ret = avfilter_graph_create_filter (&player->fbufsink,
			avfilter_get_by_name ("abuffersink"), NULL, NULL, NULL,
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
	AVCodecContext * const cctx = player->st->codec;

	ao_sample_format aoFmt;
	memset (&aoFmt, 0, sizeof (aoFmt));
	aoFmt.bits = av_get_bytes_per_sample (avformat) * 8;
	assert (aoFmt.bits > 0);
	aoFmt.channels = cctx->channels;
	aoFmt.rate = cctx->sample_rate;
	aoFmt.byte_format = AO_FMT_NATIVE;

	int driver = ao_default_driver_id ();
	if ((player->aoDev = ao_open_live (driver, &aoFmt, NULL)) == NULL) {
		BarUiMsg (player->settings, MSG_ERR, "Cannot open audio device.\n");
		return false;
	}

	return true;
}

/*	decode and play stream. returns 0 or av error code.
 */
static int play (player_t * const player) {
	assert (player != NULL);

	AVPacket pkt;
	av_init_packet (&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	AVFrame *frame = NULL, *filteredFrame = NULL;
	frame = av_frame_alloc ();
	assert (frame != NULL);
	filteredFrame = av_frame_alloc ();
	assert (filteredFrame != NULL);

	while (!player->doQuit) {
		ping ();
		int ret = av_read_frame (player->fctx, &pkt);
		if (ret < 0) {
			av_free_packet (&pkt);
			return ret;
		} else if (pkt.stream_index != player->streamIdx) {
			av_free_packet (&pkt);
			continue;
		}

		AVPacket pkt_orig = pkt;

		/* pausing */
		pthread_mutex_lock (&player->pauseMutex);
		if (player->doPause) {
			av_read_pause (player->fctx);
			do {
				pthread_cond_wait (&player->pauseCond, &player->pauseMutex);
			} while (player->doPause);
			av_read_play (player->fctx);
		}
		pthread_mutex_unlock (&player->pauseMutex);

		while (pkt.size > 0 && !player->doQuit) {
			int got_frame = 0;

			const int decoded = avcodec_decode_audio4 (player->st->codec,
					frame, &got_frame, &pkt);
			if (decoded < 0) {
				/* skip this one */
				break;
			}

			if (got_frame != 0) {
				/* XXX: suppresses warning from resample filter */
				if (frame->pts == (int64_t) AV_NOPTS_VALUE) {
					frame->pts = 0;
				}
				ret = av_buffersrc_write_frame (player->fabuf, frame);
				assert (ret >= 0);

				while (true) {
					if (av_buffersink_get_frame (player->fbufsink, filteredFrame) < 0) {
						/* try again next frame */
						break;
					}

					const int numChannels = av_get_channel_layout_nb_channels (
							filteredFrame->channel_layout);
					const int bps = av_get_bytes_per_sample(filteredFrame->format);
					ao_play (player->aoDev, (char *) filteredFrame->data[0],
							filteredFrame->nb_samples * numChannels * bps);

					av_frame_unref (filteredFrame);
				}
			}

			pkt.data += decoded;
			pkt.size -= decoded;
		};

		av_free_packet (&pkt_orig);

		player->songPlayed = av_q2d (player->st->time_base) * (double) pkt.pts;
		player->lastTimestamp = pkt.pts;
	}

	av_frame_free (&filteredFrame);
	av_frame_free (&frame);

	return 0;
}

static void finish (player_t * const player) {
	ao_close (player->aoDev);
	player->aoDev = NULL;
	if (player->fgraph != NULL) {
		avfilter_graph_free (&player->fgraph);
		player->fgraph = NULL;
	}
	if (player->st != NULL && player->st->codec != NULL) {
		avcodec_close (player->st->codec);
		player->st = NULL;
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
	intptr_t pret = PLAYER_RET_OK;

	bool retry;
	do {
		retry = false;
		if (openStream (player)) {
			if (openFilter (player) && openDevice (player)) {
				player->mode = PLAYER_PLAYING;
				BarPlayerSetVolume (player);
				retry = play (player) == AVERROR_INVALIDDATA;
			} else {
				/* filter missing or audio device busy */
				pret = PLAYER_RET_HARDFAIL;
			}
		} else {
			/* stream not found */
			pret = PLAYER_RET_SOFTFAIL;
		}
		player->mode = PLAYER_WAITING;
		finish (player);
	} while (retry);

	player->mode = PLAYER_FINISHED;

	return (void *) pret;
}

