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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "ui_readline.h"

/*	return size of previous UTF-8 character
 */
static size_t BarReadlinePrevUtf8 (char *ptr) {
	size_t i = 0;

	do {
		++i;
		--ptr;
	} while ((*ptr & (1 << 7)) && !(*ptr & (1 << 6)));

	return i;
}

/*	readline replacement
 *	@param buffer
 *	@param buffer size
 *	@param accept these characters
 *	@param input fds
 *	@param flags
 *	@param timeout (seconds) or -1 (no timeout)
 *	@return number of bytes read from stdin
 */
size_t BarReadline (char *buf, const size_t bufSize, const char *mask,
		BarReadlineFds_t *input, const BarReadlineFlags_t flags, int timeout) {
	size_t bufPos = 0;
	size_t bufLen = 0;
	unsigned char escapeState = 0;
	fd_set set;
	const bool echo = !(flags & BAR_RL_NOECHO);

	assert (buf != NULL);
	assert (bufSize > 0);
	assert (input != NULL);

	memset (buf, 0, bufSize);

	/* if fd is a fifo fgetc will always return EOF if nobody writes to
	 * it, stdin will block */
	while (1) {
		int curFd = -1;
		unsigned char chr;
		struct timeval timeoutstruct;

		/* select modifies set and timeout */
		memcpy (&set, &input->set, sizeof (set));
		timeoutstruct.tv_sec = timeout;
		timeoutstruct.tv_usec = 0;

		if (select (input->maxfd, &set, NULL, NULL,
				(timeout == -1) ? NULL : &timeoutstruct) <= 0) {
			/* fail or timeout */
			break;
		}

		assert (sizeof (input->fds) / sizeof (*input->fds) == 2);
		if (FD_ISSET(input->fds[0], &set)) {
			curFd = input->fds[0];
		} else if (input->fds[1] != -1 && FD_ISSET(input->fds[1], &set)) {
			curFd = input->fds[1];
		}
		if (read (curFd, &chr, sizeof (chr)) <= 0) {
			/* select() is going wild if fdset contains EOFed stdin, only check
			 * for stdin, fifo is "reopened" as soon as another writer is
			 * available
			 * FIXME: ugly */
			if (curFd == STDIN_FILENO) {
				FD_CLR (curFd, &input->set);
			}
			continue;
		}
		switch (chr) {
			/* EOT */
			case 4:
			/* return */
			case 10:
				if (echo) {
					fputs ("\n", stdout);
				}
				return bufLen;
				break;

			/* escape */
			case 27:
				escapeState = 1;
				break;

			/* del */
			case 126:
				break;

			/* backspace */
			case 8: /* ASCII BS */
			case 127: /* ASCII DEL */
				if (bufPos > 0) {
					size_t moveSize = BarReadlinePrevUtf8 (&buf[bufPos]);
					assert ((signed int) bufPos - (signed int) moveSize >= 0);
					memmove (&buf[bufPos-moveSize], &buf[bufPos], moveSize);

					bufPos -= moveSize;
					bufLen -= moveSize;

					buf[bufLen] = '\0';

					/* move caret back and delete last character */
					if (echo) {
						fputs ("\033[D\033[K", stdout);
						fflush (stdout);
					}
				} else if (bufPos == 0 && buf[bufPos] != '\0') {
					/* delete char at position 0 but don't move cursor any further */
					buf[bufPos] = '\0';
					if (echo) {
						fputs ("\033[K", stdout);
						fflush (stdout);
					}
				}
				break;

			default:
				/* ignore control/escape characters */
				if (chr <= 0x1F) {
					break;
				}
				if (escapeState == 2) {
					escapeState = 0;
					break;
				}
				if (escapeState == 1 && chr == '[') {
					escapeState = 2;
					break;
				}
				/* don't accept chars not in mask */
				if (mask != NULL && !strchr (mask, chr)) {
					break;
				}
				/* don't write beyond buffer's limits */
				if (bufPos < bufSize-1) {
					buf[bufPos] = chr;
					++bufPos;
					++bufLen;
					if (echo) {
						putchar (chr);
						fflush (stdout);
					}
					/* buffer full => return if requested */
					if (bufPos >= bufSize-1 && (flags & BAR_RL_FULLRETURN)) {
						if (echo) {
							fputs ("\n", stdout);
						}
						return bufLen;
					}
				}
				break;
		} /* end switch */
	} /* end while */
	return 0;
}

/*	Read string from stdin
 *	@param buffer
 *	@param buffer size
 *	@return number of bytes read from stdin
 */
size_t BarReadlineStr (char *buf, const size_t bufSize,
		BarReadlineFds_t *input, const BarReadlineFlags_t flags) {
	return BarReadline (buf, bufSize, NULL, input, flags, -1);
}

/*	Read int from stdin
 *	@param write result into this variable
 *	@return number of bytes read from stdin
 */
size_t BarReadlineInt (int *ret, BarReadlineFds_t *input) {
	int rlRet = 0;
	char buf[16];

	rlRet = BarReadline (buf, sizeof (buf), "0123456789", input,
			BAR_RL_DEFAULT, -1);
	*ret = atoi ((char *) buf);

	return rlRet;
}

/*	Yes/No?
 *	@param default (user presses enter)
 */
bool BarReadlineYesNo (bool def, BarReadlineFds_t *input) {
	char buf[2];
	BarReadline (buf, sizeof (buf), "yYnN", input, BAR_RL_FULLRETURN, -1);
	if (*buf == 'y' || *buf == 'Y' || (def == true && *buf == '\0')) {
		return true;
	} else {
		return false;
	}
}

