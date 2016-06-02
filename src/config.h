#ifndef SRC_CONFIG_H_S6A1C09K
#define SRC_CONFIG_H_S6A1C09K

/* package name */
#define PACKAGE "pianobar"

#define VERSION "2016.06.02-dev"

/* glibc feature test macros, define _before_ including other files */
#define _POSIX_C_SOURCE 200809L

/* ffmpeg/libav quirks detection
 * ffmpeg’s micro versions always start at 100, that’s how we can distinguish
 * ffmpeg and libav */
#include <libavfilter/version.h>

/* does graph_send_command exist (ffmpeg >=2.2) */
#if LIBAVFILTER_VERSION_MAJOR >= 4 && \
		LIBAVFILTER_VERSION_MICRO >= 100
#define HAVE_AVFILTER_GRAPH_SEND_COMMAND
#endif

/* need avcodec.h (ffmpeg 1.2) */
#if LIBAVFILTER_VERSION_MAJOR == 3 && \
		LIBAVFILTER_VERSION_MINOR <= 42 && \
		LIBAVFILTER_VERSION_MINOR > 32 && \
		LIBAVFILTER_VERSION_MICRO >= 100
#define HAVE_AV_BUFFERSINK_GET_BUFFER_REF
#define HAVE_LIBAVFILTER_AVCODEC_H
#endif

#endif /* SRC_CONFIG_H_S6A1C09K */
