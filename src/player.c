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

#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <arpa/inet.h>

#include <ao/ao.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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

#include "player.h"
#include "config.h"
#include "ui.h"
#include "ui_types.h"

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
	avfilter_uninit ();
	ao_shutdown ();
}

/*	Update volume filter
 */
void BarPlayerSetVolume (struct audioPlayer * const player) {
	assert (player != NULL);
	assert (player->fvolume != NULL);

	int ret;
#ifdef HAVE_AVFILTER_GRAPH_SEND_COMMAND
	/* ffmpeg and libav disagree on the type of this option (string vs. double)
	 * -> print to string and let them parse it again */
	char strbuf[16];
	snprintf (strbuf, sizeof (strbuf), "%fdB",
			player->settings->volume + player->gain);
	if ((ret = avfilter_graph_send_command (player->fgraph, "volume", "volume",
					strbuf, NULL, 0, 0)) < 0) {
#else
	/* convert from decibel */
	const double volume = pow (10, (player->settings->volume + player->gain) / 20);
	/* libav does not provide other means to set this right now. it might not
	 * even work everywhere. */
	if ((ret = av_opt_set_double (player->fvolume->priv, "volume", volume,
			0)) != 0) {
#endif
		printError (player->settings, "Cannot set volume", ret);
	}
}

#define softfail(msg) \
	printError (player->settings, msg, ret); \
	pret = PLAYER_RET_SOFTFAIL; \
	goto finish;

/*	player thread; for every song a new thread is started
 *	@param audioPlayer structure
 *	@return PLAYER_RET_*
 */
void *BarPlayerThread (void *data) {
	assert (data != NULL);

	struct audioPlayer * const player = data;
	int ret;
	intptr_t pret = PLAYER_RET_OK;
	const enum AVSampleFormat avformat = AV_SAMPLE_FMT_S16;

	ao_device *aoDev = NULL;
	ao_sample_format aoFmt;

	AVFrame *frame = NULL, *filteredFrame = NULL;
	AVFormatContext *fctx = NULL;
	AVCodecContext *cctx = NULL;

	/* stream setup */
	if ((ret = avformat_open_input (&fctx, player->url, NULL, NULL)) < 0) {
		softfail ("Unable to open audio file");
	}

	if ((ret = avformat_find_stream_info (fctx, NULL)) < 0) {
		softfail ("find_stream_info");
	}

	/* ignore all streams, undone for audio stream below */
	for (size_t i = 0; i < fctx->nb_streams; i++) {
		fctx->streams[i]->discard = AVDISCARD_ALL;
	}

	const int streamIdx = av_find_best_stream (fctx, AVMEDIA_TYPE_AUDIO, -1,
			-1, NULL, 0);
	if (streamIdx < 0) {
		softfail ("find_best_stream");
	}

	AVStream * const st = fctx->streams[streamIdx];
	cctx = st->codec;
	st->discard = AVDISCARD_DEFAULT;

	/* decoder setup */
	AVCodec * const decoder = avcodec_find_decoder (cctx->codec_id);
	if (decoder == NULL) {
		softfail ("find_decoder");
	}

	if ((ret = avcodec_open2 (cctx, decoder, NULL)) < 0) {
		softfail ("codec_open2");
	}

	frame = avcodec_alloc_frame ();
	assert (frame != NULL);
	filteredFrame = avcodec_alloc_frame ();
	assert (filteredFrame != NULL);

	AVPacket pkt;
	av_init_packet (&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	/* filter setup */
	char strbuf[256];

	if ((player->fgraph = avfilter_graph_alloc ()) == NULL) {
		softfail ("graph_alloc");
	}

	/* abuffer */
	AVFilterContext *fabuf = NULL;
	AVRational time_base = st->time_base;
	snprintf (strbuf, sizeof (strbuf),
			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64, 
			time_base.num, time_base.den, cctx->sample_rate,
			av_get_sample_fmt_name (cctx->sample_fmt),
			cctx->channel_layout);
	if ((ret = avfilter_graph_create_filter (&fabuf,
			avfilter_get_by_name ("abuffer"), NULL, strbuf, NULL,
			player->fgraph)) < 0) {
		softfail ("create_filter abuffer");
	}

	/* volume */
	if ((ret = avfilter_graph_create_filter (&player->fvolume,
			avfilter_get_by_name ("volume"), NULL, NULL, NULL,
			player->fgraph)) < 0) {
		softfail ("create_filter volume");
	}
	BarPlayerSetVolume (player);

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
	AVFilterContext *fbufsink = NULL;
	if ((ret = avfilter_graph_create_filter (&fbufsink,
			avfilter_get_by_name ("abuffersink"), NULL, NULL, NULL,
			player->fgraph)) < 0) {
		softfail ("create_filter abuffersink");
	}

	/* connect filter: abuffer -> volume -> aformat -> abuffersink */
	if (avfilter_link (fabuf, 0, player->fvolume, 0) != 0 ||
			avfilter_link (player->fvolume, 0, fafmt, 0) != 0 ||
			avfilter_link (fafmt, 0, fbufsink, 0) != 0) {
		softfail ("filter_link");
	}

	if ((ret = avfilter_graph_config (player->fgraph, NULL)) < 0) {
		softfail ("graph_config");
	}

	/* setup libao */
	memset (&aoFmt, 0, sizeof (aoFmt));
	aoFmt.bits = av_get_bytes_per_sample (avformat) * 8;
	assert (aoFmt.bits > 0);
	aoFmt.channels = cctx->channels;
	aoFmt.rate = cctx->sample_rate;
	aoFmt.byte_format = AO_FMT_NATIVE;

	int driver = ao_default_driver_id ();
	if ((aoDev = ao_open_live (driver, &aoFmt, NULL)) == NULL) {
		BarUiMsg (player->settings, MSG_ERR, "Cannot open audio device.\n");
		pret = PLAYER_RET_HARDFAIL;
		goto finish;
	}

	player->songPlayed = 0;
	player->songDuration = av_q2d (st->time_base) * (double) st->duration;
	player->mode = PLAYER_PLAYING;

	while (av_read_frame (fctx, &pkt) >= 0) {
		AVPacket pkt_orig = pkt;

		/* pausing */
		pthread_mutex_lock (&player->pauseMutex);
		while (true) {
			if (!player->doPause) {
				av_read_play (fctx);
				break;
			} else {
				av_read_pause (fctx);
			}
			pthread_cond_wait (&player->pauseCond, &player->pauseMutex);
		}
		pthread_mutex_unlock (&player->pauseMutex);

		if (player->doQuit) {
			av_free_packet (&pkt_orig);
			break;
		}

		if (pkt.stream_index != streamIdx) {
			av_free_packet (&pkt_orig);
			continue;
		}

		do {
			int got_frame = 0;

			const int decoded = avcodec_decode_audio4 (cctx, frame, &got_frame,
					&pkt);
			if (decoded < 0) {
				softfail ("decode_audio4");
			}

			if (got_frame != 0) {
				/* XXX: suppresses warning from resample filter */
				if (frame->pts == (int64_t) AV_NOPTS_VALUE) {
					frame->pts = 0;
				}
				ret = av_buffersrc_write_frame (fabuf, frame);
				assert (ret >= 0);

				while (true) {
					AVFilterBufferRef *audioref = NULL;
#ifdef HAVE_AV_BUFFERSINK_GET_BUFFER_REF
					/* ffmpeg’s compatibility layer is broken in some releases */
					if (av_buffersink_get_buffer_ref (fbufsink, &audioref, 0) < 0) {
#else
					if (av_buffersink_read (fbufsink, &audioref) < 0) {
#endif
						/* try again next frame */
						break;
					}

					ret = avfilter_copy_buf_props (filteredFrame, audioref);
					assert (ret >= 0);

					const int numChannels = av_get_channel_layout_nb_channels (
							filteredFrame->channel_layout);
					const int bps = av_get_bytes_per_sample(filteredFrame->format);
					ao_play (aoDev, (char *) filteredFrame->data[0],
							filteredFrame->nb_samples * numChannels * bps);

					avfilter_unref_bufferp (&audioref);
				}
			}

			pkt.data += decoded;
			pkt.size -= decoded;
		} while (pkt.size > 0);

		av_free_packet (&pkt_orig);

		player->songPlayed = av_q2d (st->time_base) * (double) pkt.pts;
	}

finish:
	ao_close (aoDev);
	if (player->fgraph != NULL) {
		avfilter_graph_free (&player->fgraph);
	}
	if (cctx != NULL) {
		avcodec_close (cctx);
	}
	if (fctx != NULL) {
		avformat_close_input (&fctx);
	}
	if (frame != NULL) {
		avcodec_free_frame (&frame);
	}
	if (filteredFrame != NULL) {
		avcodec_free_frame (&filteredFrame);
	}

	player->mode = PLAYER_FINISHED;

	return (void *) pret;
}

