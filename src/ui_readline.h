/*
Copyright (c) 2008-2011
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

#ifndef _UI_READLINE_H
#define _UI_READLINE_H

#include <stdbool.h>
#include <sys/select.h>

typedef enum {
	BAR_RL_DEFAULT = 0,
	BAR_RL_FULLRETURN = 1, /* return if buffer is full */
	BAR_RL_NOECHO = 2, /* don't echo to stdout */
} BarReadlineFlags_t;

typedef struct {
	fd_set set;
	int maxfd;
	int fds[2];
} BarReadlineFds_t;

size_t BarReadline (char *, const size_t, const char *,
		BarReadlineFds_t *, const BarReadlineFlags_t, int);
size_t BarReadlineStr (char *, const size_t,
		BarReadlineFds_t *, const BarReadlineFlags_t);
size_t BarReadlineInt (int *, BarReadlineFds_t *);
bool BarReadlineYesNo (bool, BarReadlineFds_t *);

#endif /* _UI_READLINE_H */

