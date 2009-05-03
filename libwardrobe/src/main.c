/*
Copyright (c) 2008-2009
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

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <waitress.h>

#include "wardrobe.h"
#include "md5.h"
#include "config.h"

#define WARDROBE_HTTP_SEND_SIZE 10*1024
#define WARDROBE_HTTP_RECV_SIZE 1024
#define WARDROBE_URL_SIZE 1024

/*	Initialize song structure
 *	@param wardrobe sond
 */
inline void WardrobeSongInit (WardrobeSong_t *ws) {
	memset (ws, 0, sizeof (*ws));
}

/*	initialize wardrobe handle (setup curl, e.g.)
 *	@param wardrobe handle
 */
inline void WardrobeInit (WardrobeHandle_t *wh) {
	memset (wh, 0, sizeof (*wh));
	WaitressInit (&wh->waith);
}

/*	free () replacement that does some checks and zeros memory
 *	@param pointer
 *	@param size or 0 to disable zeroing
 */
inline void WardrobeFree (void *ptr, size_t size) {
	if (ptr != NULL) {
		if (size > 0) {
			memset (ptr, 0, size);
		}
		free (ptr);
	}
}

/*	cleanup song
 *	@param song
 */
void WardrobeSongDestroy (WardrobeSong_t *ws) {
	WardrobeFree (ws->artist, 0);
	WardrobeFree (ws->title, 0);
	WardrobeFree (ws->album, 0);
	memset (ws, 0, sizeof (*ws));
}

/*	cleanup wardrobe handle
 *	@param initialized wardrobe handle
 */
void WardrobeDestroy (WardrobeHandle_t *wh) {
	WardrobeFree (wh->user, 0);
	WardrobeFree (wh->password, 0);
	WaitressFree (&wh->waith);
	memset (wh, 0, sizeof (*wh));
}

/*	get session id from last.fm; you don't have to call this manually
 *	@param wardrobe handle
 *	@return _OK or error
 */
WardrobeReturn_t WardrobeHandshake (WardrobeHandle_t *wh) {
	/* md5 hash length + long integer max + NULL */
	char url[WARDROBE_URL_SIZE], tmp[32+55+1], *tmpDigest, *pwDigest,
			ret[WARDROBE_HTTP_RECV_SIZE], postUrl[1024];
	WardrobeReturn_t fRet = WARDROBE_RET_ERR;
	time_t currTStamp = time (NULL);

	tmpDigest = WardrobeMd5Calc (wh->password);
	snprintf (tmp, sizeof (tmp), "%s%li", tmpDigest, currTStamp);
	pwDigest = WardrobeMd5Calc (tmp);
	snprintf (url, sizeof (url), "?hs=true&p=1.2&c=tst&v=1.0&u=%s&t=%li&a=%s",
			wh->user, currTStamp, pwDigest);
	
	WaitressSetHPP (&wh->waith, "post.audioscrobbler.com", "80", url);
	if (WaitressFetchBuf (&wh->waith, ret, sizeof (ret)) != WAITRESS_RET_OK) {
		return WARDROBE_RET_CONNECT_ERR;
	}

	/* parse answer */
	if (memcmp (ret, "OK", 2) == 0) {
		char *newlines[5];
		size_t i;
		newlines[0] = ret;
		/* split string */
		for (i = 1; i < sizeof (newlines) / sizeof (*newlines); i++) {
			newlines[i] = strchr (newlines[i-1]+1, '\n');
		}
		/* copy needed values */
		if (newlines[2] - newlines[1]-1 < sizeof (wh->authToken)) {
			memcpy (wh->authToken, newlines[1]+1, newlines[2] -
					newlines[1]-1);
		} else {
			printf ("buffer overflow!\n");
		}
		if (newlines[4] - newlines[3]-1 < sizeof (postUrl)) {
			memset (postUrl, 0, sizeof (postUrl));
			memcpy (postUrl, newlines[3]+1, newlines[4] -
					newlines[3]-1);
			WaitressSplitUrl (postUrl, wh->postHost, sizeof (wh->postHost),
					wh->postPort, sizeof (wh->postPort), wh->postPath,
					sizeof (wh->postPath));
		} else {
			printf ("buffer overflow!\n");
		}
		fRet = WARDROBE_RET_OK;
	} else if (memcmp (ret, "BADAUTH", 7) == 0) {
		fRet = WARDROBE_RET_BADAUTH;
	} else if (memcmp (ret, "BADTIME", 7) == 0) {
		fRet = WARDROBE_RET_BADTIME;
	} else if (memcmp (ret, "BANNED", 6) == 0) {
		fRet = WARDROBE_RET_CLIENT_BANNED;
	}

	WardrobeFree (tmpDigest, WARDROBE_MD5_DIGEST_LEN);
	WardrobeFree (pwDigest, WARDROBE_MD5_DIGEST_LEN);

	return fRet;
}

/*	_really_ submit song to last.fm
 *	@param wardrobe handle
 *	@param song
 *	@return _OK or error
 */
WardrobeReturn_t WardrobeSendSong (WardrobeHandle_t *wh,
		const WardrobeSong_t *ws) {
	char postContent[WARDROBE_HTTP_SEND_SIZE];
	char *urlencArtist, *urlencTitle, *urlencAlbum, ret[WARDROBE_HTTP_RECV_SIZE];
	WardrobeReturn_t fRet = WARDROBE_RET_ERR;

	urlencArtist = WaitressUrlEncode (ws->artist);
	urlencTitle = WaitressUrlEncode (ws->title);
	urlencAlbum = WaitressUrlEncode (ws->album);

	snprintf (postContent, sizeof (postContent), "s=%s&a[0]=%s&t[0]=%s&"
			"i[0]=%li&o[0]=P&r[0]=&l[0]=%li&b[0]=%s&n[0]=&m[0]=",
			wh->authToken, urlencArtist, urlencTitle, ws->started,
			ws->length, urlencAlbum);

	WaitressSetHPP (&wh->waith, wh->postHost, wh->postPort, wh->postPath);
	if (WaitressFetchBuf (&wh->waith, ret, sizeof (ret)) != WAITRESS_RET_OK) {
		return WARDROBE_RET_CONNECT_ERR;
	}

	if (memcmp (ret, "OK", 2) == 0) {
		fRet = WARDROBE_RET_OK;
	} else if (memcmp (ret, "BADSESSION", 10) == 0) {
		fRet = WARDROBE_RET_BADSESSION;
	}

	WardrobeFree (urlencArtist, 0);
	WardrobeFree (urlencTitle, 0);
	WardrobeFree (urlencAlbum, 0);

	return fRet;
}

/*	submit played track to last.fm
 *	@public yes
 *	@param wardrobe handle
 *	@param song data
 *	@return _OK or error
 */
WardrobeReturn_t WardrobeSubmit (WardrobeHandle_t *wh,
		const WardrobeSong_t *ws) {
	size_t i;
	WardrobeReturn_t fRet = WARDROBE_RET_ERR;

	if (strlen (wh->authToken) <= 0 && (fRet = WardrobeHandshake (wh)) !=
			WARDROBE_RET_OK) {
		return fRet;
	}
	for (i = 0; i < 2; i++) {
		fRet = WardrobeSendSong (wh, ws);
		if (fRet == WARDROBE_RET_OK) {
			return WARDROBE_RET_OK;
		} else if (fRet == WARDROBE_RET_BADSESSION &&
				(fRet = WardrobeHandshake (wh)) != WARDROBE_RET_OK) {
			return fRet;
		}
	}
	return fRet;
}

/*	error string
 *	@param error int
 *	@return human readable error string or NULL on error
 */
const char *WardrobeErrorToString (WardrobeReturn_t ret) {
	switch (ret) {
		case WARDROBE_RET_ERR:
			return "Unknown error.";
			break;

		case WARDROBE_RET_OK:
			return "Everything's fine :)";
			break;

		case WARDROBE_RET_CLIENT_BANNED:
			return "Client banned. Try to update your software.";
			break;
			
		case WARDROBE_RET_BADAUTH:
			return "Wrong username or password.";
			break;
		
		case WARDROBE_RET_BADTIME:
			return "System time wrong. Check your system time and "
					"correct it.";
			break;
		
		case WARDROBE_RET_BADSESSION:
			return "Bad session. Try to login again.";
			break;

		case WARDROBE_RET_CONNECT_ERR:
			return "Connection failed.";
			break;

		default:
			return "Unknown Error.";
			break;
	}
}

