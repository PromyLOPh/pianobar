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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "crypt_key_output.h"
#include "crypt_key_input.h"
#include "main.h"

/*	hex string to array of unsigned int values
 *	@param hex string
 *	@param return array
 *	@param return size of array
 *	@return nothing, yet
 */
void PianoHexToInts (char *strHex, unsigned int **retInts, size_t *retIntsN) {
	size_t i;
	char hexInt[9];
	unsigned int *arrInts = calloc (strlen (strHex) / 8, sizeof (*arrInts));

	/* FIXME: error handling: string too short, e.g. */
	/* unsigned int = 4 bytes, 8 chars in hex */
	for (i = 0; i < strlen (strHex); i += 8) {
		memset (hexInt, 0, sizeof (hexInt));
		memcpy (hexInt, strHex+i, sizeof (hexInt)-1);
		sscanf (hexInt, "%x", &arrInts[i/8]);
	}
	*retInts = arrInts;
	/* FIXME: copy & waste */
	*retIntsN = strlen (strHex) / 8, sizeof (*arrInts);
}

/*	decipher int array; reverse engineered from pandora source
 *	@param decrypt-me
 *	@param decrypt-me-length
 *	@param return plain ints array
 *	@return nothing, yet
 */
void PianoDecipherInts (unsigned int *cipherInts, size_t cipherIntsN,
		unsigned int **retPlainInts) {
	unsigned int *plainInts = calloc (cipherIntsN, sizeof (*plainInts));

	size_t i;

	for (i = 0; i < cipherIntsN; i += 2) {
		unsigned int _loc2 = cipherInts [i];
		unsigned int _loc3 = cipherInts [i+1];
		unsigned int _loc6;
		size_t n_count;

		for (n_count = in_key_n + 1; n_count > 1; --n_count) {
			_loc2 = _loc2 ^ in_key_p [n_count];
			
			unsigned int _loc4 = _loc2;
			#if 0
			unsigned short _loc8 = _loc4 & 0xff;
			_loc4 = _loc4 >> 8;
			unsigned short _loc9 = _loc4 & 0xff;
			_loc4 = _loc4 >> 8;
			unsigned short _loc10 = _loc4 & 0xff;
			_loc4 = _loc4 >> 8;
			unsigned short _loc11 = _loc4 & 0xff;
			#endif
			unsigned short _loc8 = _loc4 & 0xff;
			unsigned short _loc9 = (_loc4 >> 8) & 0xff;
			unsigned short _loc10 = (_loc4 >> 16) & 0xff;
			unsigned short _loc11 = (_loc4 >> 24) & 0xff;
			unsigned int _loc5 = in_key_s [0][_loc11] +
					in_key_s [1][_loc10];
			_loc5 = _loc5 ^ in_key_s [2][_loc9];
			_loc5 = _loc5 + in_key_s [3][_loc8];
			_loc3 = _loc3 ^ _loc5;
			_loc6 = _loc2;
			_loc2 = _loc3;
			_loc3 = _loc6;
		}
		_loc6 = _loc2;
		_loc2 = _loc3;
		_loc3 = _loc6;
		_loc3 = _loc3 ^ in_key_p [1];
		_loc2 = _loc2 ^ in_key_p [0];
		plainInts [i] = _loc2;
		plainInts [i+1] = _loc3;
	}
	*retPlainInts = plainInts;
}

/*	int array to string
 *	@param int array
 *	@param length of array
 *	@return the string
 */
char *PianoIntsToString (unsigned int *arrInts, size_t arrIntsN) {
	char *strDecoded = calloc (arrIntsN * 4 + 1, sizeof (*strDecoded));
	size_t i;

	for (i = 0; i < arrIntsN; i++) {
		snprintf (&strDecoded[i*4], arrIntsN * 4, "%c%c%c%c", ((arrInts[i] >> 24) & 0xff), ((arrInts[i] >> 16) & 0xff), ((arrInts[i] >> 8) & 0xff), ((arrInts[i] >> 0) & 0xff));
	}
	return strDecoded;
}

/*	decrypt hex-encoded string
 *	@param hex string
 *	@return decrypted string
 */
char *PianoDecryptString (char *strInput) {
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
void PianoBytesToInts (char *strInput, unsigned int **retArrInts,
		size_t *retArrIntsN) {
	size_t i, j, neededStrLen = strlen (strInput);
	unsigned int *arrInts;
	char shift;

	/* blowfish encrypts two 4 byte blocks */
	neededStrLen = strlen (strInput);
	if (neededStrLen % 8 != 0) {
		/* substract overhead and add full 8 byte block */
		neededStrLen = neededStrLen - (neededStrLen % 8) + 8;
	}
	arrInts = calloc (neededStrLen / 4, sizeof (*arrInts));

	/* we must not read beyond the end of the string, so be a bit
	 * paranoid */
	shift = 24;
	i = 0;
	j = 0;
	while (i < strlen (strInput)) {
		shift = 24;
		while (shift >= 0 && i < strlen (strInput)) {
			arrInts[i/4] |= strInput[i] << shift;
			shift -= 8;
			i++;
		}
	}
	*retArrInts = arrInts;
	*retArrIntsN = neededStrLen / 4;
}

/*	decipher ints; reverse engineered from pandora flash client
 *	@param encipher this
 *	@param how many ints
 *	@param returns crypted ints; memory is allocated by this function
 *	@return nothing yet
 */
void PianoEncipherInts (unsigned int *plainInts, size_t plainIntsN,
		unsigned int **retCipherInts) {
	unsigned int *cipherInts = calloc (plainIntsN, sizeof (*cipherInts));
	size_t i, _loc7;

		for (i = 0; i < plainIntsN; i+=2) {
			/* ++ encipher */
			unsigned int _loc2 = plainInts [i];
			unsigned int _loc3 = plainInts [i+1];
			unsigned int _loc6;
			
			for (_loc7 = 0; _loc7 < out_key_n; _loc7++) {
				_loc2 = _loc2 ^ out_key_p[_loc7];
				unsigned int _loc4 = _loc2;
				#if 0
				unsigned int _loc8 = _loc4 & 0xff;
				_loc4 >>= 8;
				unsigned int _loc9 = _loc4 & 0xff;
				_loc4 >>= 8;
				unsigned int _loc10 = _loc4 & 0xff;
				_loc4 >>= 8;
				unsigned _loc11 = _loc4 & 0xff;
				#endif
				unsigned int _loc8 = _loc4 & 0xff;
				unsigned int _loc9 = (_loc4 >> 8) & 0xff;
				unsigned int _loc10 = (_loc4 >> 16) & 0xff;
				unsigned _loc11 = (_loc4 >> 24) & 0xff;
				unsigned int _loc5 = out_key_s[0][_loc11] +
						out_key_s [1][_loc10];
				_loc5 ^= out_key_s [2][_loc9];
				_loc5 += out_key_s [3][_loc8];
				_loc3 ^= _loc5;
				_loc6 = _loc2;
				_loc2 = _loc3;
				_loc3 = _loc6;
			}
			_loc6 = _loc2;
			_loc2 = _loc3;
			_loc3 = _loc6;
			_loc3 ^= out_key_p [out_key_n];
			_loc2 ^= out_key_p [out_key_n+1];
			cipherInts [i] = _loc2;
			cipherInts [i+1] = _loc3;
		}
	*retCipherInts = cipherInts;
}

/*	int array to hex-encoded string
 *	@param int array
 *	@param size of array
 *	@return string; memory is allocated here, don't forget to free it
 */
char *PianoIntsToHexString (unsigned int *arrInts, size_t arrIntsN) {
	/* 4 bytes as hex (= 8 chars) */
	char *hexStr = calloc (arrIntsN * 4 * 2 + 1, sizeof (*hexStr));
	size_t i;

	for (i = 0; i < arrIntsN; i++) {
		snprintf (hexStr+i*4*2, arrIntsN * 4 * 2 + 1, "%08x", arrInts[i]);
	}
	return hexStr;
}

/*	blowfish-encrypt string; used before sending xml to server
 *	@param encrypt this
 *	@return encrypted, hex-encoded string
 */
char *PianoEncryptString (char *strInput) {
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
