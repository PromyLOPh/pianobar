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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "crypt_key_output.h"
#include "crypt_key_input.h"
#include "main.h"

#define byteswap32(x) (((x >> 24) & 0x000000ff) | ((x >> 8) & 0x0000ff00) | \
		((x << 8) & 0x00ff0000) | ((x << 24) & 0xff000000))

/*	hex string to array of unsigned int values
 *	@param hex string
 *	@param return array
 *	@param return size of array
 *	@return nothing, yet
 */
void PianoHexToInts (const char *strHex, unsigned int **retInts,
		size_t *retIntsN) {
	size_t i, strHexN = strlen (strHex);
	unsigned int *arrInts = calloc (strHexN / 8, sizeof (*arrInts));

	/* unsigned int = 4 bytes, 8 chars in hex */
	for (i = 0; i < strHexN; i++) {
		arrInts[i/8] |= ((strHex[i] < 'a' ? strHex[i]: strHex[i] + 9) &
				0x0f) << (7*4 - i*4);
	}
	*retInts = arrInts;
	*retIntsN = strHexN / 8;
}

/*	decipher int array; reverse engineered from pandora source
 *	@param decrypt-me
 *	@param decrypt-me-length
 *	@param return plain ints array
 */
void PianoDecipherInts (const unsigned int *cipherInts, size_t cipherIntsN,
		unsigned int **retPlainInts) {
	unsigned int *plainInts = calloc (cipherIntsN, sizeof (*plainInts));
	size_t i, j;
	unsigned int f, l, r, lrExchange;

	for (i = 0; i < cipherIntsN; i += 2) {
		l = cipherInts [i];
		r = cipherInts [i+1];

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
		plainInts [i] = l;
		plainInts [i+1] = r;
	}
	*retPlainInts = plainInts;
}

/*	int array to string
 *	@param int array
 *	@param length of array
 *	@return the string
 */
char *PianoIntsToString (const unsigned int *arrInts, size_t arrIntsN) {
	char *strDecoded = calloc (arrIntsN * 4 + 1, sizeof (*strDecoded));
	size_t i;
	unsigned int *tmp;

	for (i = 0; i < arrIntsN; i++) {
		/* map string to 4-byte int */
		tmp = (unsigned int *) &strDecoded[i*4];
		/* FIXME: big endian does not need to byteswap? */
		*tmp = byteswap32 (arrInts[i]);
	}
	return strDecoded;
}

/*	decrypt hex-encoded string
 *	@param hex string
 *	@return decrypted string
 */
char *PianoDecryptString (const char *strInput) {
	unsigned int *cipherInts, *plainInts;
	size_t cipherIntsN;
	char *strDecrypted;

	PianoHexToInts (strInput, &cipherInts, &cipherIntsN);
	PianoDecipherInts (cipherInts, cipherIntsN, &plainInts);
	strDecrypted = PianoIntsToString (plainInts, cipherIntsN);

	PianoFree (cipherInts, cipherIntsN * sizeof (*cipherInts));
	PianoFree (plainInts, cipherIntsN * sizeof (*plainInts));

	return strDecrypted;
}

/*	string to int array
 *	@param the string, length % 8 needs to be 0
 *	@param returns int array
 *	@param returns size of int array
 *	@return nothing
 */
void PianoBytesToInts (const char *strInput, unsigned int **retArrInts,
		size_t *retArrIntsN) {
	size_t i, j, neededStrLen, strInputN = strlen (strInput);
	unsigned int *arrInts;
	char shift;

	/* blowfish encrypts two 4 byte blocks */
	neededStrLen = strInputN;
	if (neededStrLen % 8 != 0) {
		/* substract overhead and add full 8 byte block */
		neededStrLen = neededStrLen - (neededStrLen % 8) + 8;
	}
	arrInts = calloc (neededStrLen / 4, sizeof (*arrInts));

	/* we must not read beyond the end of the string, so be a bit
	 * paranoid */
	i = 0;
	j = 0;
	while (i < strInputN) {
		shift = 24;
		while (shift >= 0 && i < strInputN) {
			arrInts[i/4] |= strInput[i] << shift;
			shift -= 8;
			i++;
		}
	}
	*retArrInts = arrInts;
	*retArrIntsN = neededStrLen / 4;
}

/*	encipher ints; reverse engineered from pandora flash client
 *	@param encipher this
 *	@param how many ints
 *	@param returns crypted ints; memory is allocated by this function
 */
void PianoEncipherInts (const unsigned int *plainInts, size_t plainIntsN,
		unsigned int **retCipherInts) {
	unsigned int *cipherInts = calloc (plainIntsN, sizeof (*cipherInts));
	size_t i, j;
	unsigned int f, l, r, lrExchange;

		for (i = 0; i < plainIntsN; i+=2) {
			l = plainInts [i];
			r = plainInts [i+1];
			
			for (j = 0; j < out_key_n; j++) {
				l ^= out_key_p[j];

				f = out_key_s[0][(l >> 24) & 0xff] +
						out_key_s [1][(l >> 16) & 0xff];
				f ^= out_key_s [2][(l >> 8) & 0xff];
				f += out_key_s [3][l & 0xff];
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
			cipherInts [i] = l;
			cipherInts [i+1] = r;
		}
	*retCipherInts = cipherInts;
}

/*	int array to hex-encoded string
 *	@param int array
 *	@param size of array
 *	@return string; memory is allocated here, don't forget to free it
 */
char *PianoIntsToHexString (const unsigned int *arrInts, size_t arrIntsN) {
	/* 4 bytes as hex (= 8 chars) */
	char *hexStr = calloc (arrIntsN * 4 * 2 + 1, sizeof (*hexStr));
	size_t i, writePos;
	unsigned char *intMap = (unsigned char *) arrInts;
	size_t intMapN = arrIntsN * sizeof (*arrInts);

	for (i = 0; i < intMapN; i++) {
		/* we need to swap the bytes again */
		writePos = i + (4 - (i % 4) * 2) - 1;
		hexStr[writePos*2] = (intMap[i] & 0xf0) < 0xa0 ? (intMap[i] >> 4) +
				'0' : (intMap[i] >> 4) + 'a' - 10;
		hexStr[writePos*2+1] = (intMap[i] & 0x0f) < 0x0a ? (intMap[i] & 0x0f) +
				'0' : (intMap[i] & 0x0f) + 'a' - 10;
	}
	return hexStr;
}

/*	blowfish-encrypt string; used before sending xml to server
 *	@param encrypt this
 *	@return encrypted, hex-encoded string
 */
char *PianoEncryptString (const char *strInput) {
	unsigned int *plainInts, *cipherInts;
	size_t plainIntsN;
	char *strHex;

	PianoBytesToInts (strInput, &plainInts, &plainIntsN);
	PianoEncipherInts (plainInts, plainIntsN, &cipherInts);
	strHex = PianoIntsToHexString (cipherInts, plainIntsN);

	PianoFree (plainInts, plainIntsN * sizeof (*plainInts));
	PianoFree (cipherInts, plainIntsN * sizeof (*cipherInts));

	return strHex;
}
