/*
Copyright (c) 2009
	Lars-Dominik Braun <PromyLOPh@lavabit.com>

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

#ifndef _WAITRESS_H
#define _WAITRESS_H

#include <stdlib.h>

#define WAITRESS_HOST_SIZE 100
/* max: 65,535 */
#define WAITRESS_PORT_SIZE 6
#define WAITRESS_PATH_SIZE 1000
#define WAITRESS_RECV_BUFFER 10*1024

typedef enum {WAITRESS_METHOD_GET = 0, WAITRESS_METHOD_POST} WaitressMethod_t;

typedef struct {
	char host[WAITRESS_HOST_SIZE];
	char port[WAITRESS_PORT_SIZE];
	char path[WAITRESS_PATH_SIZE];
	WaitressMethod_t method;
	const char *extraHeaders;
	const char *postData;
	size_t contentLength;
	size_t contentReceived;
	char proxyHost[WAITRESS_HOST_SIZE];
	char proxyPort[WAITRESS_PORT_SIZE];
	/* extra data handed over to callback function */
	void *data;
	char (*callback) (void *, size_t, void *);
	int socktimeout;
} WaitressHandle_t;

typedef enum {WAITRESS_RET_ERR = 0, WAITRESS_RET_OK, WAITRESS_RET_STATUS_UNKNOWN,
		WAITRESS_RET_NOTFOUND, WAITRESS_RET_FORBIDDEN, WAITRESS_RET_CONNECT_REFUSED,
		WAITRESS_RET_SOCK_ERR, WAITRESS_RET_GETADDR_ERR,
		WAITRESS_RET_CB_ABORT, WAITRESS_RET_HDR_OVERFLOW,
		WAITRESS_RET_PARTIAL_FILE, WAITRESS_RET_TIMEOUT, WAITRESS_RET_READ_ERR}
		WaitressReturn_t;

void WaitressInit (WaitressHandle_t *);
void WaitressFree (WaitressHandle_t *);
void WaitressSetProxy (WaitressHandle_t *, const char *, const char *);
char *WaitressUrlEncode (const char *);
char WaitressSplitUrl (const char *, char *, size_t, char *, size_t, char *,
		size_t);
char WaitressSetUrl (WaitressHandle_t *, const char *);
void WaitressSetHPP (WaitressHandle_t *, const char *, const char *,
		const char *);
WaitressReturn_t WaitressFetchBuf (WaitressHandle_t *, char **);
WaitressReturn_t WaitressFetchCall (WaitressHandle_t *);

#endif /* _WAITRESS_H */

