/*
Copyright (c) 2008 Lars-Dominik Braun

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

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"

#define PIANO_HTTP_BUFFER_SIZE 100000

/* FIXME: curl has a receive limit, use it! */
/*	callback for curl, writes data to buffer
 *	@param received data
 *	@param block size
 *	@param blocks received
 *	@param write data into this buffer
 *	@return written bytes
 */
size_t PianoCurlRetToVar (void *ptr, size_t size, size_t nmemb, void *stream) {
	char *charPtr = ptr;
	char *streamPtr = stream;
	size_t streamPtrN = strlen (streamPtr);

	if (streamPtrN + nmemb > PIANO_HTTP_BUFFER_SIZE) {
		printf ("buffer overflow...\n");
		return 0;
	} else {
		memcpy (streamPtr+streamPtrN, charPtr, size*nmemb);
		return size*nmemb;
	}
}

/* FIXME: we may use a callback given by the library client here. would be
 * more flexible... */
/*	post data to url and receive answer as string
 *	@param initialized curl handle
 *	@param call this url
 *	@param post this data
 *	@param put received data here, memory is allocated by this function
 *	@return nothing yet
 */
PianoReturn_t PianoHttpPost (CURL *ch, const char *url, const char *postData,
		char **retData) {
	struct curl_slist *headers = NULL;
	/* Let's hope nothing will be bigger than this... */
	char curlRet[PIANO_HTTP_BUFFER_SIZE];
	PianoReturn_t ret;

	headers = curl_slist_append (headers, "Content-Type: text/xml");

	curl_easy_setopt (ch, CURLOPT_URL, url);
	curl_easy_setopt (ch, CURLOPT_POSTFIELDS, postData);
	curl_easy_setopt (ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt (ch, CURLOPT_WRITEFUNCTION, PianoCurlRetToVar);
	/* don't verify certificate for now, it's easier ;) */
	curl_easy_setopt (ch, CURLOPT_SSL_VERIFYPEER, 0);
	memset (curlRet, 0, sizeof (curlRet));
	curl_easy_setopt (ch, CURLOPT_WRITEDATA, curlRet);

	if (curl_easy_perform (ch) == CURLE_OK) {
		ret = PIANO_RET_OK;
		*retData = strdup (curlRet);
	} else {
		ret = PIANO_RET_NET_ERROR;
		*retData = NULL;
	}

	curl_slist_free_all (headers);

	return ret;
}

/*	get data
 *	@param initialized curl handle
 *	@param call this url
 *	@param put received data here, memory is allocated by this function
 *	@return nothing yet
 */
PianoReturn_t PianoHttpGet (CURL *ch, const char *url, char **retData) {
	/* Let's hope nothing will be bigger than this... */
	char curlRet[PIANO_HTTP_BUFFER_SIZE];
	PianoReturn_t ret;

	curl_easy_setopt (ch, CURLOPT_URL, url);
	/* remove, as not needed, but set (maybe) set by PianoHttpPost */
	curl_easy_setopt (ch, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt (ch, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt (ch, CURLOPT_WRITEFUNCTION, PianoCurlRetToVar);
	memset (curlRet, 0, sizeof (curlRet));
	curl_easy_setopt (ch, CURLOPT_WRITEDATA, curlRet);

	if (curl_easy_perform (ch) == CURLE_OK) {
		ret = PIANO_RET_OK;
		*retData = strdup (curlRet);
	} else {
		ret = PIANO_RET_NET_ERROR;
		*retData = NULL;
	}

	return ret;
}
