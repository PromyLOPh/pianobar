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

#pragma once

#include "config.h"
#include <stdbool.h>

#ifdef HAVE_DEBUGLOG
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/* bitfield */
typedef enum {
	DEBUG_NETWORK = 1,
	DEBUG_AUDIO = 2,
	DEBUG_UI = 4,
} debugKind;

extern unsigned int debug;

inline static bool debugEnable () {
	const char * const debugStr = getenv("PIANOBAR_DEBUG");
	if (debugStr != NULL) {
		debug = atoi (debugStr);
	}
	return debug;
}

__attribute__((format(printf, 2, 3)))
inline static void debugPrint(debugKind kind, const char * const format, ...) {
	if (debug & kind) {
		va_list fmtargs;
		va_start (fmtargs, format);
		vfprintf (stderr, format, fmtargs);
		va_end (fmtargs);
	}
}
#else
inline static bool debugEnable () {}
#define debugPrint(...)
#endif

