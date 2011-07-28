/*
Copyright (c) 2009-2011
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

#ifndef _WAITRESS_H
#define _WAITRESS_H

#include <stdlib.h>
#include <stdbool.h>

#define WAITRESS_RECV_BUFFER 10*1024

typedef enum {WAITRESS_METHOD_GET = 0, WAITRESS_METHOD_POST} WaitressMethod_t;

typedef enum {WAITRESS_CB_RET_ERR, WAITRESS_CB_RET_OK} WaitressCbReturn_t;

typedef struct {
	char *url; /* splitted url, unusable */
	const char *user;
	const char *password;
	const char *host;
	const char *port;
	const char *path; /* without leading '/' */
} WaitressUrl_t;

typedef struct {
	WaitressUrl_t url;
	WaitressMethod_t method;
	const char *extraHeaders;
	const char *postData;
	size_t contentLength;
	size_t contentReceived;
	WaitressUrl_t proxy;
	/* extra data handed over to callback function */
	void *data;
	WaitressCbReturn_t (*callback) (void *, size_t, void *);
	int socktimeout;
} WaitressHandle_t;

typedef enum {WAITRESS_RET_ERR = 0, WAITRESS_RET_OK, WAITRESS_RET_STATUS_UNKNOWN,
		WAITRESS_RET_NOTFOUND, WAITRESS_RET_FORBIDDEN, WAITRESS_RET_CONNECT_REFUSED,
		WAITRESS_RET_SOCK_ERR, WAITRESS_RET_GETADDR_ERR,
		WAITRESS_RET_CB_ABORT, WAITRESS_RET_HDR_OVERFLOW,
		WAITRESS_RET_PARTIAL_FILE, WAITRESS_RET_TIMEOUT, WAITRESS_RET_READ_ERR,
		WAITRESS_RET_CONNECTION_CLOSED
} WaitressReturn_t;

void WaitressInit (WaitressHandle_t *);
void WaitressFree (WaitressHandle_t *);
bool WaitressSetProxy (WaitressHandle_t *, const char *);
char *WaitressUrlEncode (const char *);
bool WaitressSetUrl (WaitressHandle_t *, const char *);
WaitressReturn_t WaitressFetchBuf (WaitressHandle_t *, char **);
WaitressReturn_t WaitressFetchCall (WaitressHandle_t *);
const char *WaitressErrorToStr (WaitressReturn_t);

#endif /* _WAITRESS_H */

