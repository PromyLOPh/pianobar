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

#include "const.h"
#include "http.h"


/* FIXME: curl has a receive limit, use it! */
/*	callback for curl, writes data to buffer
 *	@author PromyLOPh
 *	@added 2008-06-05
 *	@param received data
 *	@param block size
 *	@param blocks received
 *	@param write data into this buffer
 *	@return written bytes
 */
size_t PianoCurlRetToVar (void *ptr, size_t size, size_t nmemb, void *stream) {
	char *charPtr = ptr;
	char *streamPtr = stream;

	if (strlen (streamPtr) + nmemb > PIANO_HTTP_BUFFER_SIZE) {
		printf ("buffer overflow...\n");
		return 0;
	} else {
		memcpy (streamPtr+strlen(streamPtr), charPtr, size*nmemb);
		return size*nmemb;
	}
}

/* FIXME: we may use a callback given by the library client here. would be
 * more flexible... */
/*	post data to url and receive answer as string
 *	@author PromyLOPh
 *	@added 2008-06-05
 *	@param initialized curl handle
 *	@param call this url
 *	@param post this data
 *	@param put received data here, memory is allocated by this function
 *	@return nothing yet
 */
void PianoHttpPost (CURL *ch, char *url, char *postData, char **retData) {
	struct curl_slist *headers = NULL;
	/* Let's hope nothing will be bigger than this... */
	char curlRet[PIANO_HTTP_BUFFER_SIZE];

	headers = curl_slist_append (headers, "Content-Type: text/xml");

	curl_easy_setopt (ch, CURLOPT_URL, url);
	curl_easy_setopt (ch, CURLOPT_POSTFIELDS, postData);
	curl_easy_setopt (ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt (ch, CURLOPT_WRITEFUNCTION, PianoCurlRetToVar);
	/* don't verify certificate for now, it's easier ;) */
	curl_easy_setopt (ch, CURLOPT_SSL_VERIFYPEER, 0);
	memset (curlRet, 0, sizeof (curlRet));
	curl_easy_setopt (ch, CURLOPT_WRITEDATA, curlRet);

	curl_easy_perform (ch);

	curl_slist_free_all (headers);

	*retData = calloc (strlen (curlRet) + 1, sizeof (char));
	strcpy (*retData, curlRet);
}
