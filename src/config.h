#pragma once

/* package name */
#define PACKAGE "pianobar"

#define VERSION "2022.04.01"

/* glibc feature test macros, define _before_ including other files */
#define _POSIX_C_SOURCE 200809L

/* ffmpeg/libav quirks detection
 * ffmpeg’s micro versions always start at 100, that’s how we can distinguish
 * ffmpeg and libav */
#include <libavfilter/version.h>
#include <libavformat/version.h>

/* does graph_send_command exist (ffmpeg >=2.2) */
#if !defined(HAVE_AVFILTER_GRAPH_SEND_COMMAND) && \
		LIBAVFILTER_VERSION_MAJOR >= 4 && \
		LIBAVFILTER_VERSION_MICRO >= 100
#define HAVE_AVFILTER_GRAPH_SEND_COMMAND
#endif

/* explicit init is optional for ffmpeg>=4.0 */
#if !defined(HAVE_AVFORMAT_NETWORK_INIT) && \
		LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 5, 100) && \
		LIBAVFORMAT_VERSION_MICRO >= 100
#define HAVE_AVFORMAT_NETWORK_INIT
#endif

/* dito */
#if !defined(HAVE_AVFILTER_REGISTER_ALL) && \
		LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100) && \
		LIBAVFILTER_VERSION_MICRO >= 100
#define HAVE_AVFILTER_REGISTER_ALL
#endif

/* dito */
#if !defined(HAVE_AV_REGISTER_ALL) && \
		LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100) && \
		LIBAVFORMAT_VERSION_MICRO >= 100
#define HAVE_AV_REGISTER_ALL
#endif

#ifndef NDEBUG
#define HAVE_DEBUGLOG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug(...)
#endif

