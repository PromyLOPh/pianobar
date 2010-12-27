/*
Copyright (c) 2008-2010
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

static inline void BarReadlineMoveLeft (char *buf, size_t *bufPos,
		size_t *bufLen) {
	char *tmpBuf = &buf[*bufPos-1];
	while (tmpBuf < &buf[*bufLen]) {
		*tmpBuf = *(tmpBuf+1);
		++tmpBuf;
	}
	--(*bufPos);
	--(*bufLen);
}

static inline char BarReadlineIsAscii (char b) {
	return !(b & (1 << 7));
}

static inline char BarReadlineIsUtf8Start (char b) {
	return (b & (1 << 7)) && (b & (1 << 6));
}

static inline char BarReadlineIsUtf8Content (char b) {
	return (b & (1 << 7)) && !(b & (1 << 6));
}

/*	readline replacement
 *	@param buffer
 *	@param buffer size
 *	@param accept these characters
 *	@param return if buffer full (otherwise more characters are not accepted)
 *	@param don't echo anything (for passwords)
 *	@param read from this fd
 *	@return number of bytes read from stdin
 */
size_t BarReadline (char *buf, size_t bufSize, const char *mask,
		char fullReturn, char noEcho, FILE *fd) {
	int chr = 0;
	size_t bufPos = 0;
	size_t bufLen = 0;
	unsigned char escapeState = 0;

	memset (buf, 0, bufSize);

	/* if fd is a fifo fgetc will always return EOF if nobody writes to
	 * it, stdin will block */
	while ((chr = fgetc (fd)) != EOF) {
		switch (chr) {
			/* EOT */
			case 4:
				printf ("\n");
				return bufLen;
				break;

			/* return */
			case 10:
				printf ("\n");
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
					if (BarReadlineIsAscii (buf[bufPos-1])) {
						BarReadlineMoveLeft (buf, &bufPos, &bufLen);
					} else {
						/* delete utf-8 multibyte chars */
						/* char content */
						while (BarReadlineIsUtf8Content (buf[bufPos-1])) {
							BarReadlineMoveLeft (buf, &bufPos, &bufLen);
						}
						/* char length */
						if (BarReadlineIsUtf8Start (buf[bufPos-1])) {
							BarReadlineMoveLeft (buf, &bufPos, &bufLen);
						}
					}
					/* move caret back and delete last character */
					if (!noEcho) {
						printf ("\033[D\033[K");
						fflush (stdout);
					}
				} else if (bufPos == 0 && buf[bufPos] != '\0') {
					/* delete char at position 0 but don't move cursor any further */
					buf[bufPos] = '\0';
					if (!noEcho) {
						printf ("\033[K");
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
					if (!noEcho) {
						fputc (chr, stdout);
					}
					/* buffer full => return if requested */
					if (fullReturn && bufPos >= bufSize-1) {
						printf ("\n");
						return bufLen;
					}
				}
				break;
		}
	}
	return 0;
}

/*	Read string from stdin
 *	@param buffer
 *	@param buffer size
 *	@return number of bytes read from stdin
 */
size_t BarReadlineStr (char *buf, size_t bufSize, char noEcho,
		FILE *fd) {
	return BarReadline (buf, bufSize, NULL, 0, noEcho, fd);
}

/*	Read int from stdin
 *	@param write result into this variable
 *	@return number of bytes read from stdin
 */
size_t BarReadlineInt (int *ret, FILE *fd) {
	int rlRet = 0;
	char buf[16];

	rlRet = BarReadline (buf, sizeof (buf), "0123456789", 0, 0, fd);
	*ret = atoi ((char *) buf);

	return rlRet;
}

/*	Yes/No?
 *	@param defaul (user presses enter)
 */
int BarReadlineYesNo (char def, FILE *fd) {
	char buf[2];
	BarReadline (buf, sizeof (buf), "yYnN", 1, 0, fd);
	if (*buf == 'y' || *buf == 'Y' || (def == 1 && *buf == '\0')) {
		return 1;
	} else {
		return 0;
	}
}

