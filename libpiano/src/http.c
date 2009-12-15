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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <waitress.h>

#include "piano_private.h"
#include "crypt.h"
#include "http.h"

/*	post data to url and receive answer as string
 *	@param initialized waitress handle
 *	@param null-terminated post data string
 *	@param receive buffer
 *	@param buffer size
 *	@return _RET_OK or _RET_NET_ERROR
 */
PianoReturn_t PianoHttpPost (WaitressHandle_t *waith, const char *postData,
		char **retData) {
	PianoReturn_t pRet = PIANO_RET_NET_ERROR;
	char *reqPostData = PianoEncryptString (postData);

	if (reqPostData == NULL) {
		return PIANO_RET_OUT_OF_MEMORY;
	}

	waith->extraHeaders = "Content-Type: text/xml\r\n";
	waith->postData = reqPostData;
	waith->method = WAITRESS_METHOD_POST;

	if (WaitressFetchBuf (waith, retData) == WAITRESS_RET_OK &&
			*retData != NULL) {
		pRet = PIANO_RET_OK;
	}

	PianoFree (reqPostData, 0);

	return pRet;
}

/*	http get request, return server response body
 *	@param initialized waitress handle
 *	@param receive buffer
 *	@param buffer size
 *	@return _RET_OK or _RET_NET_ERROR
 */
PianoReturn_t PianoHttpGet (WaitressHandle_t *waith, char **retData) {
	waith->extraHeaders = NULL;
	waith->postData = NULL;
	waith->method = WAITRESS_METHOD_GET;

	if (WaitressFetchBuf (waith, retData) == WAITRESS_RET_OK &&
			*retData != NULL) {
		return PIANO_RET_OK;
	}
	return PIANO_RET_NET_ERROR;
}
