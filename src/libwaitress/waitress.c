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

#define _POSIX_C_SOURCE 1 /* required by getaddrinfo() */
#define _BSD_SOURCE /* snprintf() */

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
#include <errno.h>
#include <assert.h>

#include "config.h"
#include "waitress.h"

typedef struct {
	char *buf;
	size_t pos;
} WaitressFetchBufCbBuffer_t;

void WaitressInit (WaitressHandle_t *waith) {
	memset (waith, 0, sizeof (*waith));
	waith->socktimeout = 30000;
}

void WaitressFree (WaitressHandle_t *waith) {
	free (waith->url.url);
	free (waith->proxy.url);
	memset (waith, 0, sizeof (*waith));
}

/*	Proxy set up?
 *	@param Waitress handle
 *	@return true|false
 */
bool WaitressProxyEnabled (const WaitressHandle_t *waith) {
	return waith->proxy.host != NULL;
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
 *	@param returned url struct
 *	@return url is a http url? does not say anything about its validity!
 */
static bool WaitressSplitUrl (const char *inurl, WaitressUrl_t *retUrl) {
	assert (inurl != NULL);
	assert (retUrl != NULL);

	static const char *httpPrefix = "http://";
	
	/* is http url? */
	if (strncmp (httpPrefix, inurl, strlen (httpPrefix)) == 0) {
		enum {FIND_USER, FIND_PASS, FIND_HOST, FIND_PORT, FIND_PATH, DONE}
				state = FIND_USER, newState = FIND_USER;
		char *url, *urlPos, *assignStart;
		const char **assign = NULL;

		url = strdup (inurl);
		retUrl->url = url;

		urlPos = url + strlen (httpPrefix);
		assignStart = urlPos;

		if (*urlPos == '\0') {
			state = DONE;
		}

		while (state != DONE) {
			const char c = *urlPos;

			switch (state) {
				case FIND_USER: {
					if (c == ':') {
						assign = &retUrl->user;
						newState = FIND_PASS;
					} else if (c == '@') {
						assign = &retUrl->user;
						newState = FIND_HOST;
					} else if (c == '/') {
						/* not a user */
						assign = &retUrl->host;
						newState = FIND_PATH;
					} else if (c == '\0') {
						assign = &retUrl->host;
						newState = DONE;
					}
					break;
				}

				case FIND_PASS: {
					if (c == '@') {
						assign = &retUrl->password;
						newState = FIND_HOST;
					} else if (c == '/') {
						/* not a password */
						assign = &retUrl->port;
						newState = FIND_PATH;
					} else if (c == '\0') {
						assign = &retUrl->port;
						newState = DONE;
					}
					break;
				}

				case FIND_HOST: {
					if (c == ':') {
						assign = &retUrl->host;
						newState = FIND_PORT;
					} else if (c == '/') {
						assign = &retUrl->host;
						newState = FIND_PATH;
					} else if (c == '\0') {
						assign = &retUrl->host;
						newState = DONE;
					}
					break;
				}

				case FIND_PORT: {
					if (c == '/') {
						assign = &retUrl->port;
						newState = FIND_PATH;
					} else if (c == '\0') {
						assign = &retUrl->port;
						newState = DONE;
					}
					break;
				}

				case FIND_PATH: {
					if (c == '\0') {
						assign = &retUrl->path;
						newState = DONE;
					}
					break;
				}

				case DONE:
					break;
			} /* end switch */

			if (assign != NULL) {
				*assign = assignStart;
				*urlPos = '\0';
				assignStart = urlPos+1;

				state = newState;
				assign = NULL;
			}

			++urlPos;
		} /* end while */

		/* fixes for our state machine logic */
		if (retUrl->user != NULL && retUrl->host == NULL && retUrl->port != NULL) {
			retUrl->host = retUrl->user;
			retUrl->user = NULL;
		}
		return true;
	} /* end if strncmp */

	return false;
}

/*	Parse url and set host, port, path
 *	@param Waitress handle
 *	@param url: protocol://host:port/path
 */
bool WaitressSetUrl (WaitressHandle_t *waith, const char *url) {
	return WaitressSplitUrl (url, &waith->url);
}

/*	Set http proxy
 *	@param waitress handle
 *  @param url, e.g. http://proxy:80/
 */
bool WaitressSetProxy (WaitressHandle_t *waith, const char *url) {
	return WaitressSplitUrl (url, &waith->proxy);
}

/*	Callback for WaitressFetchBuf, appends received data to NULL-terminated buffer
 *	@param received data
 *	@param data size
 *	@param buffer structure
 */
static WaitressCbReturn_t WaitressFetchBufCb (void *recvData, size_t recvDataSize,
		void *extraData) {
	char *recvBytes = recvData;
	WaitressFetchBufCbBuffer_t *buffer = extraData;

	if (buffer->buf == NULL) {
		if ((buffer->buf = malloc (sizeof (*buffer->buf) *
				(recvDataSize + 1))) == NULL) {
			return WAITRESS_CB_RET_ERR;
		}
	} else {
		char *newbuf;
		if ((newbuf = realloc (buffer->buf,
				sizeof (*buffer->buf) *
				(buffer->pos + recvDataSize + 1))) == NULL) {
			free (buffer->buf);
			return WAITRESS_CB_RET_ERR;
		}
		buffer->buf = newbuf;
	}
	memcpy (buffer->buf + buffer->pos, recvBytes, recvDataSize);
	buffer->pos += recvDataSize;
	*(buffer->buf+buffer->pos) = '\0';

	return WAITRESS_CB_RET_OK;
}

/*	Fetch string. Beware! This overwrites your waith->data pointer
 *	@param waitress handle
 *	@param result buffer, malloced (don't forget to free it yourself)
 */
WaitressReturn_t WaitressFetchBuf (WaitressHandle_t *waith, char **buf) {
	WaitressFetchBufCbBuffer_t buffer;
	WaitressReturn_t wRet;

	memset (&buffer, 0, sizeof (buffer));

	waith->data = &buffer;
	waith->callback = WaitressFetchBufCb;

	wRet = WaitressFetchCall (waith);
	*buf = buffer.buf;
	return wRet;
}

/*	poll wrapper that retries after signal interrupts, required for socksify
 *	wrapper
 */
static int WaitressPollLoop (struct pollfd *fds, nfds_t nfds, int timeout) {
	int pollres = -1;
	int pollerr = 0;

	do {
		pollres = poll (fds, nfds, timeout);
		pollerr = errno;
		errno = 0;
	} while (pollerr == EINTR || pollerr == EINPROGRESS || pollerr == EAGAIN);

	return pollres;
}

/*	write () wrapper with poll () timeout
 *	@param socket fd
 *	@param write buffer
 *	@param write count bytes
 *	@param reuse existing pollfd structure
 *	@param timeout (microseconds)
 *	@return WAITRESS_RET_OK, WAITRESS_RET_TIMEOUT or WAITRESS_RET_ERR
 */
static WaitressReturn_t WaitressPollWrite (int sockfd, const char *buf, size_t count,
		struct pollfd *sockpoll, int timeout) {
	int pollres = -1;

	sockpoll->events = POLLOUT;
	pollres = WaitressPollLoop (sockpoll, 1, timeout);
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
static WaitressReturn_t WaitressPollRead (int sockfd, char *buf, size_t count,
		struct pollfd *sockpoll, int timeout, ssize_t *retSize) {
	int pollres = -1;

	sockpoll->events = POLLIN;
	pollres = WaitressPollLoop (sockpoll, 1, timeout);
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

/*	get default http port if none was given
 */
static const char *WaitressDefaultPort (WaitressUrl_t *url) {
	return url->port == NULL ? "80" : url->port;
}

/*	Receive data from host and call *callback ()
 *	@param waitress handle
 *	@return WaitressReturn_t
 */
WaitressReturn_t WaitressFetchCall (WaitressHandle_t *waith) {
	struct addrinfo hints, *res;
	int sockfd;
	char recvBuf[WAITRESS_RECV_BUFFER];
	char writeBuf[2*1024];
	ssize_t recvSize = 0;
	WaitressReturn_t wRet = WAITRESS_RET_OK;
	struct pollfd sockpoll;
	int pollres;
	/* header parser vars */
	char *nextLine = NULL, *thisLine = NULL;
	enum {HDRM_HEAD, HDRM_LINES, HDRM_FINISHED} hdrParseMode = HDRM_HEAD;
	char statusCode[3], val[256];
	unsigned int bufFilled = 0;

	/* initialize */
	waith->contentLength = 0;
	waith->contentReceived = 0;
	memset (&hints, 0, sizeof hints);

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Use proxy? */
	if (WaitressProxyEnabled (waith)) {
		if (getaddrinfo (waith->proxy.host,
				WaitressDefaultPort (&waith->proxy), &hints, &res) != 0) {
			return WAITRESS_RET_GETADDR_ERR;
		}
	} else {
		if (getaddrinfo (waith->url.host,
				WaitressDefaultPort (&waith->url), &hints, &res) != 0) {
			return WAITRESS_RET_GETADDR_ERR;
		}
	}

	if ((sockfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		freeaddrinfo (res);
		return WAITRESS_RET_SOCK_ERR;
	}
	sockpoll.fd = sockfd;

	/* we need shorter timeouts for connect() */
	fcntl (sockfd, F_SETFL, O_NONBLOCK);

	/* increase socket receive buffer */
	const int sockopt = 256*1024;
	setsockopt (sockfd, SOL_SOCKET, SO_RCVBUF, &sockopt, sizeof (sockopt));

	/* non-blocking connect will return immediately */
	connect (sockfd, res->ai_addr, res->ai_addrlen);

	sockpoll.events = POLLOUT;
	pollres = WaitressPollLoop (&sockpoll, 1, waith->socktimeout);
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

	const char *path = waith->url.path;
	if (waith->url.path == NULL) {
		/* avoid NULL pointer deref */
		path = "";
	} else if (waith->url.path[0] == '/') {
		/* most servers don't like "//" */
		++path;
	}

	/* send request */
	if (WaitressProxyEnabled (waith)) {
		snprintf (writeBuf, sizeof (writeBuf),
			"%s http://%s:%s/%s HTTP/1.0\r\n",
			(waith->method == WAITRESS_METHOD_GET ? "GET" : "POST"),
			waith->url.host,
			WaitressDefaultPort (&waith->url), path);
	} else {
		snprintf (writeBuf, sizeof (writeBuf),
			"%s /%s HTTP/1.0\r\n",
			(waith->method == WAITRESS_METHOD_GET ? "GET" : "POST"),
			path);
	}
	WRITE_RET (writeBuf, strlen (writeBuf));

	snprintf (writeBuf, sizeof (writeBuf),
			"Host: %s\r\nUser-Agent: " PACKAGE "\r\n", waith->url.host);
	WRITE_RET (writeBuf, strlen (writeBuf));

	if (waith->method == WAITRESS_METHOD_POST && waith->postData != NULL) {
		snprintf (writeBuf, sizeof (writeBuf), "Content-Length: %zu\r\n",
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

	/* receive answer */
	nextLine = recvBuf;
	while (hdrParseMode != HDRM_FINISHED) {
		READ_RET (recvBuf+bufFilled, sizeof (recvBuf)-1 - bufFilled, &recvSize);
		if (recvSize == 0) {
			/* connection closed too early */
			CLOSE_RET (WAITRESS_RET_CONNECTION_CLOSED);
		}
		bufFilled += recvSize;
		memset (recvBuf+bufFilled, 0, sizeof (recvBuf) - bufFilled);
		thisLine = recvBuf;

		/* split */
		while ((nextLine = strchr (thisLine, '\n')) != NULL &&
				hdrParseMode != HDRM_FINISHED) {
			/* make lines parseable by string routines */
			*nextLine = '\0';
			if (*(nextLine-1) == '\r') {
				*(nextLine-1) = '\0';
			}
			/* skip \0 */
			++nextLine;

			switch (hdrParseMode) {
				/* Status code */
				case HDRM_HEAD:
					if (sscanf (thisLine, "HTTP/1.%*1[0-9] %3[0-9] ",
							statusCode) == 1) {
						if (memcmp (statusCode, "200", 3) == 0 ||
								memcmp (statusCode, "206", 3) == 0) {
							/* everything's fine... */
						} else if (memcmp (statusCode, "403", 3) == 0) {
							CLOSE_RET (WAITRESS_RET_FORBIDDEN);
						} else if (memcmp (statusCode, "404", 3) == 0) {
							CLOSE_RET (WAITRESS_RET_NOTFOUND);
						} else {
							CLOSE_RET (WAITRESS_RET_STATUS_UNKNOWN);
						}
						hdrParseMode = HDRM_LINES;
					} /* endif */
					break;

				/* Everything else, except status code */
				case HDRM_LINES:
					/* empty line => content starts here */
					if (*thisLine == '\0') {
						hdrParseMode = HDRM_FINISHED;
					} else {
						memset (val, 0, sizeof (val));
						if (sscanf (thisLine, "Content-Length: %255c", val) == 1) {
							waith->contentLength = atol (val);
						}
					}
					break;

				default:
					break;
			} /* end switch */
			thisLine = nextLine;
		} /* end while strchr */
		memmove (recvBuf, thisLine, thisLine-recvBuf);
		bufFilled -= (thisLine-recvBuf);
	} /* end while hdrParseMode */

	/* push remaining bytes */
	if (bufFilled > 0) {
		waith->contentReceived += bufFilled;
		if (waith->callback (thisLine, bufFilled, waith->data) ==
				WAITRESS_CB_RET_ERR) {
			CLOSE_RET (WAITRESS_RET_CB_ABORT);
		}
	}

	/* receive content */
	do {
		READ_RET (recvBuf, sizeof (recvBuf), &recvSize);
		if (recvSize > 0) {
			waith->contentReceived += recvSize;
			if (waith->callback (recvBuf, recvSize, waith->data) ==
					WAITRESS_CB_RET_ERR) {
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

const char *WaitressErrorToStr (WaitressReturn_t wRet) {
	switch (wRet) {
		case WAITRESS_RET_OK:
			return "Everything's fine :)";
			break;

		case WAITRESS_RET_ERR:
			return "Unknown.";
			break;

		case WAITRESS_RET_STATUS_UNKNOWN:
			return "Unknown HTTP status code.";
			break;

		case WAITRESS_RET_NOTFOUND:
			return "File not found.";
			break;
		
		case WAITRESS_RET_FORBIDDEN:
			return "Forbidden.";
			break;

		case WAITRESS_RET_CONNECT_REFUSED:
			return "Connection refused.";
			break;

		case WAITRESS_RET_SOCK_ERR:
			return "Socket error.";
			break;

		case WAITRESS_RET_GETADDR_ERR:
			return "getaddr failed.";
			break;

		case WAITRESS_RET_CB_ABORT:
			return "Callback aborted request.";
			break;

		case WAITRESS_RET_HDR_OVERFLOW:
			return "HTTP header overflow.";
			break;

		case WAITRESS_RET_PARTIAL_FILE:
			return "Partial file.";
			break;
	
		case WAITRESS_RET_TIMEOUT:
			return "Timeout.";
			break;

		case WAITRESS_RET_READ_ERR:
			return "Read error.";
			break;

		case WAITRESS_RET_CONNECTION_CLOSED:
			return "Connection closed by remote host.";
			break;

		default:
			return "No error message available.";
			break;
	}
}

#ifdef TEST
/* test cases for libwaitress */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "waitress.h"

#define streq(x,y) (strcmp(x,y) == 0)

/*	string equality test (memory location or content)
 */
static bool streqtest (const char *x, const char *y) {
	return (x == y) || (x != NULL && y != NULL && streq (x, y));
}

/*	test WaitressSplitUrl
 *	@param tested url
 *	@param expected user
 *	@param expected password
 *	@param expected host
 *	@param expected port
 *	@param expected path
 */
static void compareUrl (const char *url, const char *user,
		const char *password, const char *host, const char *port,
		const char *path) {
	WaitressUrl_t splitUrl;

	memset (&splitUrl, 0, sizeof (splitUrl));

	WaitressSplitUrl (url, &splitUrl);

	bool userTest, passwordTest, hostTest, portTest, pathTest, overallTest;

	userTest = streqtest (splitUrl.user, user);
	passwordTest = streqtest (splitUrl.password, password);
	hostTest = streqtest (splitUrl.host, host);
	portTest = streqtest (splitUrl.port, port);
	pathTest = streqtest (splitUrl.path, path);

	overallTest = userTest && passwordTest && hostTest && portTest && pathTest;

	if (!overallTest) {
		printf ("FAILED test(s) for %s\n", url);
		if (!userTest) {
			printf ("user: %s vs %s\n", splitUrl.user, user);
		}
		if (!passwordTest) {
			printf ("password: %s vs %s\n", splitUrl.password, password);
		}
		if (!hostTest) {
			printf ("host: %s vs %s\n", splitUrl.host, host);
		}
		if (!portTest) {
			printf ("port: %s vs %s\n", splitUrl.port, port);
		}
		if (!pathTest) {
			printf ("path: %s vs %s\n", splitUrl.path, path);
		}
	} else {
		printf ("OK for %s\n", url);
	}
}

int main () {
	/* WaitressSplitUrl tests */
	compareUrl ("http://www.example.com/", NULL, NULL, "www.example.com", NULL,
			"");
	compareUrl ("http://www.example.com", NULL, NULL, "www.example.com", NULL,
			NULL);
	compareUrl ("http://www.example.com:80/", NULL, NULL, "www.example.com",
			"80", "");
	compareUrl ("http://www.example.com:/", NULL, NULL, "www.example.com", "",
			"");
	compareUrl ("http://:80/", NULL, NULL, "", "80", "");
	compareUrl ("http://www.example.com/foobar/barbaz", NULL, NULL,
			"www.example.com", NULL, "foobar/barbaz");
	compareUrl ("http://www.example.com:80/foobar/barbaz", NULL, NULL,
			"www.example.com", "80", "foobar/barbaz");
	compareUrl ("http://foo:bar@www.example.com:80/foobar/barbaz", "foo", "bar",
			"www.example.com", "80", "foobar/barbaz");
	compareUrl ("http://foo:@www.example.com:80/foobar/barbaz", "foo", "",
			"www.example.com", "80", "foobar/barbaz");
	compareUrl ("http://foo@www.example.com:80/foobar/barbaz", "foo", NULL,
			"www.example.com", "80", "foobar/barbaz");
	compareUrl ("http://:foo@www.example.com:80/foobar/barbaz", "", "foo",
			"www.example.com", "80", "foobar/barbaz");
	compareUrl ("http://:@:80", "", "", "", "80", NULL);
	compareUrl ("http://", NULL, NULL, NULL, NULL, NULL);
	compareUrl ("http:///", NULL, NULL, "", NULL, "");
	compareUrl ("http://foo:bar@", "foo", "bar", "", NULL, NULL);

	return EXIT_SUCCESS;
}
#endif /* TEST */

