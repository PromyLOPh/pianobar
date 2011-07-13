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

#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 1 /* fileno() */
#define _BSD_SOURCE /* setlinebuf() */
#define _DARWIN_C_SOURCE /* setlinebuf() on OS X */
#endif

#include <termios.h>
#include <stdio.h>

/*	en/disable echoing for stdin
 *	@param 1 = enable, everything else = disable
 */
void BarTermSetEcho (char enable) {
	struct termios termopts;

	tcgetattr (fileno (stdin), &termopts);
	if (enable == 1) {
		termopts.c_lflag |= ECHO;
	} else {
		termopts.c_lflag &= ~ECHO;
	}
	tcsetattr(fileno (stdin), TCSANOW, &termopts);
}

/*	en/disable stdin buffering; when enabling line-buffer method will be
 *	selected for you
 *	@param 1 = enable, everything else = disable
 */
void BarTermSetBuffer (char enable) {
	struct termios termopts;

	tcgetattr (fileno (stdin), &termopts);
	if (enable == 1) {
		termopts.c_lflag |= ICANON;
		setlinebuf (stdin);
	} else {
		termopts.c_lflag &= ~ICANON;
		setvbuf (stdin, NULL, _IONBF, 1);
	}
	tcsetattr(fileno (stdin), TCSANOW, &termopts);
}

/*	Save old terminal settings
 *	@param save settings here
 */
void BarTermSave (struct termios *termOrig) {
	tcgetattr (fileno (stdin), termOrig);
}

/*	Restore terminal settings
 *	@param Old settings
 */
void BarTermRestore (struct termios *termOrig) {
	tcsetattr (fileno (stdin), TCSANOW, termOrig);
}

