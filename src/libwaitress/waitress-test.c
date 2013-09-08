/*
Copyright (c) 2009-2013
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

/* test cases for libwaitress */

/* we are testing static methods and therefore have to include the .c */
#include "waitress.c"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define streq(a,b) (strcmp(a,b) == 0)

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

/*	compare two strings
 */
void compareStr (const char *result, const char *expected) {
	if (!streq (result, expected)) {
		printf ("FAIL for %s, result was %s\n", expected, result);
	} else {
		printf ("OK for %s\n", expected);
	}
}

/*	test entry point
 */
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

	/* WaitressBase64Encode tests */
	compareStr (WaitressBase64Encode ("M"), "TQ==");
	compareStr (WaitressBase64Encode ("Ma"), "TWE=");
	compareStr (WaitressBase64Encode ("Man"), "TWFu");
	compareStr (WaitressBase64Encode ("The quick brown fox jumped over the lazy dog."),
			"VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkb2cu");
	compareStr (WaitressBase64Encode ("The quick brown fox jumped over the lazy dog"),
			"VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkb2c=");
	compareStr (WaitressBase64Encode ("The quick brown fox jumped over the lazy do"),
			"VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkbw==");

	return EXIT_SUCCESS;
}

