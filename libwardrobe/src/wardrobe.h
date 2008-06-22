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

/* public api, not stable yet */

#include <curl/curl.h>

typedef struct {
	char *user;
	char *password;
	char authToken[100];
	char postUrl[1024];
	CURL *ch;
} WardrobeHandle_t;

typedef struct {
	char *artist;
	char *title;
	time_t started;
	time_t length;
} WardrobeSong_t;

typedef enum {WARDROBE_RET_ERR, WARDROBE_RET_OK,
		WARDROBE_RET_CLIENT_BANNED, WARDROBE_RET_BADAUTH,
		WARDROBE_RET_BADTIME, WARDROBE_RET_BADSESSION} WardrobeReturn_t;

void WardrobeInit (WardrobeHandle_t *wh);
void WardrobeSongInit (WardrobeSong_t *ws);
void WardrobeSongDestroy (WardrobeSong_t *ws);
void WardrobeDestroy (WardrobeHandle_t *wh);
WardrobeReturn_t WardrobeSubmit (WardrobeHandle_t *wh,
		WardrobeSong_t *ws);
char *WardrobeErrorToString (WardrobeReturn_t ret);
