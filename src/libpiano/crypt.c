/*
Copyright (c) 2008-2010
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "crypt_key_output.h"
#include "crypt_key_input.h"
#include "piano_private.h"

#define byteswap32(x) ((((x) >> 24) & 0x000000ff) | \
		(((x) >> 8) & 0x0000ff00) | \
		(((x) << 8) & 0x00ff0000) | \
		(((x) << 24) & 0xff000000))

#define hostToBigEndian32(x) htonl(x)
#define bigToHostEndian32(x) ntohl(x)

/*	decrypt hex-encoded, blowfish-crypted string: decode 2 hex-encoded blocks,
 *	decrypt, byteswap
 *	@param hex string
 *	@return decrypted string or NULL
 */
#define INITIAL_SHIFT 28
#define SHIFT_DEC 4
unsigned char *PianoDecryptString (const unsigned char *strInput) {
	/* hex-decode => strlen/2 + null-byte */
	uint32_t *iDecrypt;
	unsigned char *strDecrypted;
	unsigned char shift = INITIAL_SHIFT, intsDecoded = 0, j;
	/* blowfish blocks, 32-bit */
	uint32_t f, l, r, lrExchange;

	if ((iDecrypt = calloc (strlen ((char *) strInput)/2/sizeof (*iDecrypt)+1,
			sizeof (*iDecrypt))) == NULL) {
		return NULL;
	}
	strDecrypted = (unsigned char *) iDecrypt;

	while (*strInput != '\0') {
		/* hex-decode string */
		if (*strInput >= '0' && *strInput <= '9') {
			*iDecrypt |= (*strInput & 0x0f) << shift;
		} else if (*strInput >= 'a' && *strInput <= 'f') {
			/* 0xa (hex) = 10 (decimal), 'a' & 0x0f == 1 => +9 */
			*iDecrypt |= ((*strInput+9) & 0x0f) << shift;
		}
		if (shift > 0) {
			shift -= SHIFT_DEC;
		} else {
			shift = INITIAL_SHIFT;
			/* initialize next dword */
			*(++iDecrypt) = 0;
			++intsDecoded;
		}

		/* two 32-bit hex-decoded boxes available => blowfish decrypt */
		if (intsDecoded == 2) {
			l = *(iDecrypt-2);
			r = *(iDecrypt-1);

			for (j = in_key_n + 1; j > 1; --j) {
				l ^= in_key_p [j];
				
				f = in_key_s [0][(l >> 24) & 0xff] +
						in_key_s [1][(l >> 16) & 0xff];
				f ^= in_key_s [2][(l >> 8) & 0xff];
				f += in_key_s [3][l & 0xff];
				r ^= f;
				/* exchange l & r */
				lrExchange = l;
				l = r;
				r = lrExchange;
			}
			/* exchange l & r */
			lrExchange = l;
			l = r;
			r = lrExchange;
			r ^= in_key_p [1];
			l ^= in_key_p [0];

			*(iDecrypt-2) = bigToHostEndian32 (l);
			*(iDecrypt-1) = bigToHostEndian32 (r);

			intsDecoded = 0;
		}
		++strInput;
	}

	return strDecrypted;
}
#undef INITIAL_SHIFT
#undef SHIFT_DEC

/*	blowfish-encrypt/hex-encode string
 *	@param encrypt this
 *	@return encrypted, hex-encoded string
 */
unsigned char *PianoEncryptString (const unsigned char *strInput) {
	const size_t strInputN = strlen ((char *) strInput);
	/* num of 64-bit blocks, rounded to next block */
	size_t blockN = strInputN / 8 + 1;
	uint32_t *blockInput, *blockPtr;
	/* output string */
	unsigned char *strHex, *hexPtr;
	const char *hexmap = "0123456789abcdef";

	if ((blockInput = calloc (blockN*2, sizeof (*blockInput))) == NULL) {
		return NULL;
	}
	blockPtr = blockInput;

	if ((strHex = calloc (blockN*8*2 + 1, sizeof (*strHex))) == NULL) {
		return NULL;
	}
	hexPtr = strHex;

	memcpy (blockInput, strInput, strInputN);

	while (blockN > 0) {
		/* encryption blocks */
		uint32_t f, lrExchange;
		register uint32_t l, r;
		int i;

		l = hostToBigEndian32 (*blockPtr);
		r = hostToBigEndian32 (*(blockPtr+1));
		
		/* encrypt blocks */
		for (i = 0; i < out_key_n; i++) {
			l ^= out_key_p[i];

			f = out_key_s[0][(l >> 24) & 0xff] +
					out_key_s[1][(l >> 16) & 0xff];
			f ^= out_key_s[2][(l >> 8) & 0xff];
			f += out_key_s[3][l & 0xff];
			r ^= f;
			/* exchange l & r */
			lrExchange = l;
			l = r;
			r = lrExchange;
		}
		/* exchange l & r again */
		lrExchange = l;
		l = r;
		r = lrExchange;
		r ^= out_key_p [out_key_n];
		l ^= out_key_p [out_key_n+1];

		/* swap bytes again... */
		l = byteswap32 (l);
		r = byteswap32 (r);

		/* hex-encode encrypted blocks */
		for (i = 0; i < 4; i++) {
			*hexPtr++ = hexmap[(l & 0xf0) >> 4];
			*hexPtr++ = hexmap[l & 0x0f];
			l >>= 8;
		}
		for (i = 0; i < 4; i++) {
			*hexPtr++ = hexmap[(r & 0xf0) >> 4];
			*hexPtr++ = hexmap[r & 0x0f];
			r >>= 8;
		}

		/* two! 32-bit blocks encrypted (l & r) */
		blockPtr += 2;
		--blockN;
	}

	free (blockInput);

	return strHex;
}
