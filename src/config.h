#pragma once

/* package name */
#define PACKAGE "pianobar"

#define VERSION "2018.06.22"

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

