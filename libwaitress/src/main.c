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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <poll.h>

#include "config.h"
#include "waitress.h"

typedef struct {
	char *buf;
	char *pos;
	size_t max;
} WaitressFetchBufCbBuffer_t;

inline void WaitressInit (WaitressHandle_t *waith) {
	memset (waith, 0, sizeof (*waith));
	waith->socktimeout = 30000;
}

inline void WaitressFree (WaitressHandle_t *waith) {
	memset (waith, 0, sizeof (*waith));
}

/*	Proxy set up?
 *	@param Waitress handle
 *	@return true|false
 */
inline char WaitressProxyEnabled (const WaitressHandle_t *waith) {
	return *waith->proxyHost != '\0' && *waith->proxyPort != '\0';
}

/*	Set http proxy
 *	@param waitress handle
 *	@param host
 *	@param port
 */
inline void WaitressSetProxy (WaitressHandle_t *waith, const char *host,
		const char *port) {
	strncpy (waith->proxyHost, host, sizeof (waith->proxyHost)-1);
	strncpy (waith->proxyPort, port, sizeof (waith->proxyPort)-1);
}

/*	urlencode post-data
 *	@param encode this
 *	@return malloc'ed encoded string, don't forget to free it
 */
char *WaitressUrlEncode (const char *in) {
	size_t inLen = strlen (in);
	/* worst case: encode all characters */
	char *out = calloc (inLen * 3 + 1, sizeof (*in));
	const char *inPos = in;
	char *outPos = out;

	while (inPos - in < inLen) {
		if (!isalnum (*inPos) && *inPos != '_' && *inPos != '-' && *inPos != '.') {
			*outPos++ = '%';
			snprintf (outPos, 3, "%02x", *inPos & 0xff);
			outPos += 2;
		} else {
			/* copy character */
			*outPos++ = *inPos;
		}
		++inPos;
	}

	return out;
}

/*	Split http url into host, port and path
 *	@param url
 *	@param return buffer: host
 *	@param host buffer size
 *	@param return buffer: port, defaults to 80
 *	@param port buffer size
 *	@param return buffer: path
 *	@param path buffer size
 *	@param 1 = ok, 0 = not a http url; if your buffers are too small horrible
 *			things will happen... But 1 is returned anyway.
 */
char WaitressSplitUrl (const char *url, char *retHost, size_t retHostSize,
		char *retPort, size_t retPortSize, char *retPath, size_t retPathSize) {
	size_t urlSize = strlen (url);
	const char *urlPos = url, *lastPos;
	
	if (urlSize > sizeof ("http://")-1 &&
			memcmp (url, "http://", sizeof ("http://")-1) == 0) {
		memset (retHost, 0, retHostSize);
		memset (retPort, 0, retPortSize);
		strncpy (retPort, "80", retPortSize-1);
		memset (retPath, 0, retPathSize);

		urlPos += sizeof ("http://")-1;
		lastPos = urlPos;

		/* find host */
		while (*urlPos != ':' && *urlPos != '/' && urlPos - lastPos < retHostSize-1) {
			*retHost++ = *urlPos++;
		}
		lastPos = urlPos;

		/* port, if available */
		if (*urlPos == ':') {
			/* skip : */
			++urlPos;
			++lastPos;
			while (*urlPos != '/' && urlPos - lastPos < retPortSize-1) {
				*retPort++ = *urlPos++;
			}
		}
		lastPos = urlPos;

		/* path */
		while (*urlPos != '\0' && *urlPos != '#' && urlPos - lastPos < retPathSize-1) {
			*retPath++ = *urlPos++;
		}
	} else {
		return 0;
	}
	return 1;
}

/*	Parse url and set host, port, path
 *	@param Waitress handle
 *	@param url: protocol://host:port/path
 */
inline char WaitressSetUrl (WaitressHandle_t *waith, const char *url) {
	return WaitressSplitUrl (url, waith->host, sizeof (waith->host),
		waith->port, sizeof (waith->port), waith->path, sizeof (waith->path));
}

/*	Set host, port, path
 *	@param Waitress handle
 *	@param host
 *	@param port (getaddrinfo () needs a string...)
 *	@param path, including leading /
 */
inline void WaitressSetHPP (WaitressHandle_t *waith, const char *host,
		const char *port, const char *path) {
	strncpy (waith->host, host, sizeof (waith->host)-1);
	strncpy (waith->port, port, sizeof (waith->port)-1);
	strncpy (waith->path, path, sizeof (waith->path)-1);
}

/*	Callback for WaitressFetchBuf, appends received data to NULL-terminated buffer
 *	@param received data
 *	@param data size
 *	@param buffer structure
 */
char WaitressFetchBufCb (void *recvData, size_t recvDataSize, void *extraData) {
	char *recvBytes = recvData;
	WaitressFetchBufCbBuffer_t *buffer = extraData;

	if (buffer->pos - buffer->buf + recvDataSize < buffer->max) {
		memcpy (buffer->pos, recvBytes, recvDataSize);
		buffer->pos += recvDataSize;
		*buffer->pos = '\0';
	} else {
		printf (PACKAGE ": Buffer overflow!\n");
		return 0;
	}

	return 1;
}

/*	Fetch string. Beware! This overwrites your waith->data pointer
 *	@param waitress handle
 *	@param Connect to host
 *	@param Port or service
 *	@param Resource path
 *	@param result buffer
 *	@param result buffer size
 */
WaitressReturn_t WaitressFetchBuf (WaitressHandle_t *waith, char *buf,
		size_t bufSize) {
	WaitressFetchBufCbBuffer_t buffer;

	buffer.buf = buffer.pos = buf;
	buffer.max = bufSize;
	waith->data = &buffer;
	waith->callback = WaitressFetchBufCb;

	return WaitressFetchCall (waith);
}

/*	write () wrapper with poll () timeout
 *	@param socket fd
 *	@param write buffer
 *	@param write count bytes
 *	@param reuse existing pollfd structure
 *	@param timeout (microseconds)
 *	@return WAITRESS_RET_OK, WAITRESS_RET_TIMEOUT or WAITRESS_RET_ERR
 */
WaitressReturn_t WaitressPollWrite (int sockfd, const char *buf, size_t count,
		struct pollfd *sockpoll, int timeout) {
	int pollres = -1;

	sockpoll->events = POLLOUT;
	pollres = poll (sockpoll, 1, timeout);
	if (pollres == 0) {
		return WAITRESS_RET_TIMEOUT;
	} else if (pollres == -1) {
		return WAITRESS_RET_ERR;
	}
	if (write (sockfd, buf, count) == -1) {
		return WAITRESS_RET_ERR;
	}
	return WAITRESS_RET_OK;
}

/*	read () wrapper with poll () timeout
 *	@param socket fd
 *	@param write to this buf, not NULL terminated
 *	@param buffer size
 *	@param reuse existing pollfd struct
 *	@param timeout (in microseconds)
 *	@param read () return value/written bytes
 *	@return WAITRESS_RET_OK, WAITRESS_RET_TIMEOUT, WAITRESS_RET_ERR
 */
WaitressReturn_t WaitressPollRead (int sockfd, char *buf, size_t count,
		struct pollfd *sockpoll, int timeout, ssize_t *retSize) {
	int pollres = -1;

	sockpoll->events = POLLIN;
	pollres = poll (sockpoll, 1, timeout);
	if (pollres == 0) {
		return WAITRESS_RET_TIMEOUT;
	} else if (pollres == -1) {
		return WAITRESS_RET_ERR;
	}
	if ((*retSize = read (sockfd, buf, count)) == -1) {
		return WAITRESS_RET_READ_ERR;
	}
	return WAITRESS_RET_OK;
}

/* FIXME: compiler macros are ugly... */
#define CLOSE_RET(ret) close (sockfd); return ret;
#define WRITE_RET(buf, count) \
		if ((wRet = WaitressPollWrite (sockfd, buf, count, \
				&sockpoll, waith->socktimeout)) != WAITRESS_RET_OK) { \
			CLOSE_RET (wRet); \
		}
#define READ_RET(buf, count, size) \
		if ((wRet = WaitressPollRead (sockfd, buf, count, \
				&sockpoll, waith->socktimeout, size)) != WAITRESS_RET_OK) { \
			CLOSE_RET (wRet); \
		}

/*	Receive data from host and call *callback ()
 *	@param waitress handle
 *	@return WaitressReturn_t
 */
WaitressReturn_t WaitressFetchCall (WaitressHandle_t *waith) {
	struct addrinfo hints, *res;
	int sockfd;
	char recvBuf[WAITRESS_RECV_BUFFER];
	char statusBuf[3];
	char writeBuf[2*1024];
	ssize_t recvSize = 0;
	WaitressReturn_t wRet = WAITRESS_RET_OK;
	struct pollfd sockpoll;
	int pollres;

	/* initialize */
	waith->contentLength = 0;
	waith->contentReceived = 0;
	memset (&hints, 0, sizeof hints);

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Use proxy? */
	if (WaitressProxyEnabled (waith)) {
		if (getaddrinfo (waith->proxyHost, waith->proxyPort, &hints, &res) != 0) {
			return WAITRESS_RET_GETADDR_ERR;
		}
	} else {
		if (getaddrinfo (waith->host, waith->port, &hints, &res) != 0) {
			return WAITRESS_RET_GETADDR_ERR;
		}
	}

	if ((sockfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		freeaddrinfo (res);
		return WAITRESS_RET_SOCK_ERR;
	}
	sockpoll.fd = sockfd;

	fcntl (sockfd, F_SETFL, O_NONBLOCK);

	/* non-blocking connect will return immediately */
	connect (sockfd, res->ai_addr, res->ai_addrlen);

	sockpoll.events = POLLOUT;
	pollres = poll (&sockpoll, 1, waith->socktimeout);
	freeaddrinfo (res);
	if (pollres == 0) {
		return WAITRESS_RET_TIMEOUT;
	} else if (pollres == -1) {
		return WAITRESS_RET_ERR;
	}
	/* check connect () return value */
	socklen_t pollresSize = sizeof (pollres);
	getsockopt (sockfd, SOL_SOCKET, SO_ERROR, &pollres, &pollresSize);
	if (pollres != 0) {
		return WAITRESS_RET_CONNECT_REFUSED;
	}

	/* send request */
	if (WaitressProxyEnabled (waith)) {
		snprintf (writeBuf, sizeof (writeBuf),
			"%s http://%s:%s%s HTTP/1.0\r\n",
			(waith->method == WAITRESS_METHOD_GET ? "GET" : "POST"),
			waith->host, waith->port, waith->path);
	} else {
		snprintf (writeBuf, sizeof (writeBuf),
			"%s %s HTTP/1.0\r\n",
			(waith->method == WAITRESS_METHOD_GET ? "GET" : "POST"),
			waith->path);
	}
	WRITE_RET (writeBuf, strlen (writeBuf));

	snprintf (writeBuf, sizeof (writeBuf),
			"Host: %s\r\nUser-Agent: " PACKAGE "\r\n", waith->host);
	WRITE_RET (writeBuf, strlen (writeBuf));

	if (waith->method == WAITRESS_METHOD_POST && waith->postData != NULL) {
		snprintf (writeBuf, sizeof (writeBuf), "Content-Length: %u\r\n",
				strlen (waith->postData));
		WRITE_RET (writeBuf, strlen (writeBuf));
	}
	
	if (waith->extraHeaders != NULL) {
		WRITE_RET (waith->extraHeaders, strlen (waith->extraHeaders));
	}
	
	WRITE_RET ("\r\n", 2);

	if (waith->method == WAITRESS_METHOD_POST && waith->postData != NULL) {
		WRITE_RET (waith->postData, strlen (waith->postData));
	}

	/* throw "HTTP/1.1 " (length 9) away... */
	READ_RET (recvBuf, 9, &recvSize);

	/* parse status code */
	READ_RET (statusBuf, sizeof (statusBuf), &recvSize);
	switch (statusBuf[0]) {
		case '2':
			if (statusBuf[1] == '0' &&
					(statusBuf[2] == '0' || statusBuf[2] == '6')) {
				/* 200 OK/206 Partial Content */
			} else {
				CLOSE_RET (WAITRESS_RET_STATUS_UNKNOWN);
			}
			break;

		case '4':
			/* 40X? */
			switch (statusBuf[1]) {
				case '0':
					switch (statusBuf[2]) {
						case '3':
							/* 403 Forbidden */
							CLOSE_RET (WAITRESS_RET_FORBIDDEN);
							break;
	
						case '4':
							/* 404 Not found */
							CLOSE_RET (WAITRESS_RET_NOTFOUND);
							break;
	
						default:
							CLOSE_RET (WAITRESS_RET_STATUS_UNKNOWN);
							break;
					}
					break;

				default:
					CLOSE_RET (WAITRESS_RET_STATUS_UNKNOWN);
					break;
			}
			break;

		default:
			CLOSE_RET (WAITRESS_RET_STATUS_UNKNOWN);
			break;
	}

	/* read and parse header, max sizeof(recvBuf) */
	char *bufPos = recvBuf, *hdrMark = NULL, *hdrBegin = recvBuf;
	READ_RET (recvBuf, sizeof (recvBuf), &recvSize);

	while (bufPos - recvBuf < recvSize) {
		/* mark header name end */
		if (*bufPos == ':' && hdrMark == NULL) {
			hdrMark = bufPos;
		} else if (*bufPos == '\n') {
			/* parse header */
			if (hdrMark != NULL && (bufPos - hdrBegin > 12) &&
					memcmp ("Content-Length", hdrBegin, 12) == 0) {
				/* unsigned integer max: 4,294,967,295 = 10 digits */
				char tmpSizeBuf[11];

				memset (tmpSizeBuf, 0, sizeof (tmpSizeBuf));

				/* skip ':' => +1; skip '\r' suffix => -1 */
				if ((bufPos-1) - (hdrMark+1) > sizeof (tmpSizeBuf)-1) {
					CLOSE_RET (WAITRESS_RET_HDR_OVERFLOW);
				}
				memcpy (tmpSizeBuf, hdrMark+1, (bufPos-1) - (hdrMark+1));
				waith->contentLength = atol (tmpSizeBuf);
			}

			/* end header, containts \r\n == empty line */
			if (bufPos - hdrBegin < 2 && hdrMark == NULL) {
				++bufPos;
				/* push remaining bytes to callback */
				if (bufPos - recvBuf < recvSize) {
					waith->contentReceived += recvSize - (bufPos - recvBuf);
					if (!waith->callback (bufPos, recvSize - (bufPos - recvBuf),
							waith->data)) {
						CLOSE_RET (WAITRESS_RET_CB_ABORT);
					}
				}
				break;
			}

			hdrBegin = bufPos+1;
			hdrMark = NULL;
		}
		++bufPos;
	}

	/* receive content */
	do {
		READ_RET (recvBuf, sizeof (recvBuf), &recvSize);
		if (recvSize > 0) {
			waith->contentReceived += recvSize;
			if (!waith->callback (recvBuf, recvSize, waith->data)) {
				wRet = WAITRESS_RET_CB_ABORT;
				break;
			}
		}
	} while (recvSize > 0);

	close (sockfd);

	if (wRet == WAITRESS_RET_OK && waith->contentReceived < waith->contentLength) {
		return WAITRESS_RET_PARTIAL_FILE;
	}
	return wRet;
}

#undef CLOSE_RET
#undef WRITE_RET
#undef READ_RET

