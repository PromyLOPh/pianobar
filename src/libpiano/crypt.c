/*
Copyright (c) 2008-2012
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

#include <string.h>
#include <assert.h>
#include <gcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "crypt.h"

/*	decrypt hex-encoded, blowfish-crypted string: decode 2 hex-encoded blocks,
 *	decrypt, byteswap
 *	@param hex string
 *	@param decrypted string length (without trailing NUL)
 *	@return decrypted string or NULL
 */
char *PianoDecryptString (const char * const input, size_t * const retSize) {
	size_t inputLen = strlen (input);
	gcry_error_t gret;
	unsigned char *output;
	size_t outputLen = inputLen/2;

	assert (inputLen%2 == 0);

	output = calloc (outputLen+1, sizeof (*output));
	/* hex decode */
	for (size_t i = 0; i < outputLen; i++) {
		char hex[3];
		memcpy (hex, &input[i*2], 2);
		hex[2] = '\0';
		output[i] = strtol (hex, NULL, 16);
	}

	gcry_cipher_hd_t h;
	gcry_cipher_open (&h, GCRY_CIPHER_BLOWFISH, GCRY_CIPHER_MODE_ECB, 0);
	gcry_cipher_setkey (h, (unsigned char *) "R=U!LH$O2B#", 11);
	gret = gcry_cipher_decrypt (h, output, outputLen, NULL, 0);
	if (gret) {
		fprintf (stderr, "Failure: %s/%s\n", gcry_strsource (gret), gcry_strerror (gret));
		return NULL;
	}

	gcry_cipher_close (h);
	*retSize = outputLen;

	return (char *) output;
}

/*	blowfish-encrypt/hex-encode string
 *	@param encrypt this
 *	@return encrypted, hex-encoded string
 */
char *PianoEncryptString (const char *s) {
	unsigned char *paddedInput, *hexOutput;
	size_t inputLen = strlen (s);
	/* blowfish expects two 32 bit blocks */
	size_t paddedInputLen = (inputLen % 8 == 0) ? inputLen : inputLen + (8-inputLen%8);
	gcry_error_t gret;

	paddedInput = calloc (paddedInputLen+1, sizeof (*paddedInput));
	memcpy (paddedInput, s, inputLen);

	gcry_cipher_hd_t h;
	gcry_cipher_open (&h, GCRY_CIPHER_BLOWFISH, GCRY_CIPHER_MODE_ECB, 0);
	gcry_cipher_setkey (h, (unsigned char *) "6#26FRL$ZWD", 11);
	gret = gcry_cipher_encrypt (h, paddedInput, paddedInputLen, NULL, 0);
	if (gret) {
		fprintf (stderr, "Failure: %s/%s\n", gcry_strsource (gret), gcry_strerror (gret));
		return NULL;
	}

	hexOutput = calloc (paddedInputLen*2+1, sizeof (*hexOutput));
	for (size_t i = 0; i < paddedInputLen; i++) {
		snprintf ((char * restrict) &hexOutput[i*2], 3, "%02x", paddedInput[i]);
	}

	gcry_cipher_close (h);
	free (paddedInput);

	return (char *) hexOutput;
}

