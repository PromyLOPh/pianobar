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

struct PianoHttpBuffer {
	size_t size; /* size of string without NUL-byte */
	char *buf; /* NUL-terminated string */
};

/*	callback for curl, writes data to buffer
 *	@param received data
 *	@param block size
 *	@param blocks received
 *	@param write data into this buffer
 *	@return written bytes
 */
size_t PianoCurlRetToVar (void *ptr, size_t size, size_t nmemb,
		void *stream) {
	struct PianoHttpBuffer *curlRet = stream;

	if (curlRet->buf == NULL) {
		curlRet->buf = malloc (nmemb + 1);
		curlRet->size = 0;
	} else {
		curlRet->buf = realloc (curlRet->buf, curlRet->size + nmemb + 1);
	}
	memcpy (curlRet->buf + curlRet->size, ptr, size*nmemb);
	curlRet->size += nmemb;
	curlRet->buf[curlRet->size] = 0;

	return size*nmemb;
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
	struct PianoHttpBuffer curlRet = {0, NULL};
	PianoReturn_t ret;

	headers = curl_slist_append (headers, "Content-Type: text/xml");

	curl_easy_setopt (ch, CURLOPT_URL, url);
	curl_easy_setopt (ch, CURLOPT_POSTFIELDS, postData);
	curl_easy_setopt (ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt (ch, CURLOPT_WRITEFUNCTION, PianoCurlRetToVar);
	/* don't verify certificate for now, it's easier ;) */
	curl_easy_setopt (ch, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt (ch, CURLOPT_WRITEDATA, (void *) &curlRet);

	if (curl_easy_perform (ch) == CURLE_OK) {
		ret = PIANO_RET_OK;
		*retData = curlRet.buf;
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
	struct PianoHttpBuffer curlRet = {0, NULL};
	PianoReturn_t ret;

	curl_easy_setopt (ch, CURLOPT_URL, url);
	curl_easy_setopt (ch, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt (ch, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt (ch, CURLOPT_WRITEFUNCTION, PianoCurlRetToVar);
	curl_easy_setopt (ch, CURLOPT_WRITEDATA, (void *) &curlRet);

	if (curl_easy_perform (ch) == CURLE_OK) {
		ret = PIANO_RET_OK;
		*retData = curlRet.buf;
	} else {
		ret = PIANO_RET_NET_ERROR;
		*retData = NULL;
	}

	return ret;
}
