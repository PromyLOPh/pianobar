/*
Copyright (c) 2013
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

#include <assert.h>

#include "piano.h"

#define PianoListForeach(l) for (; (l) != NULL; (l) = (void *) (l)->next)

/*	append element e to list l, return new list head
 */
void *PianoListAppend (PianoListHead_t * const l, PianoListHead_t * const e) {
	assert (e != NULL);
	assert (e->next == NULL);

	if (l == NULL) {
		return e;
	} else {
		PianoListHead_t *curr = l;
		while (curr->next != NULL) {
			curr = curr->next;
		}
		curr->next = e;
		return l;
	}
}

/*	prepend element e to list l, returning new list head
 */
void *PianoListPrepend (PianoListHead_t * const l, PianoListHead_t * const e) {
	assert (e != NULL);
	assert (e->next == NULL);

	e->next = l;
	return e;
}

/*	delete element e from list l, return new list head
 */
void *PianoListDelete (PianoListHead_t * const l, PianoListHead_t * const e) {
	assert (l != NULL);
	assert (e != NULL);

	PianoListHead_t *first = l, *curr = l, *prev = NULL;
	PianoListForeach (curr) {
		if (curr == e) {
			/* found it! */
			if (prev != NULL) {
				prev->next = curr->next;
			} else {
				/* no successor */
				first = curr->next;
			}
			break;
		}
		prev = curr;
	}

	return first;
}

/*	get nth element of list
 */
void *PianoListGet (PianoListHead_t * const l, const size_t n) {
	PianoListHead_t *curr = l;
	size_t i = n;

	PianoListForeach (curr) {
		if (i == 0) {
			return curr;
		}
		--i;
	}

	return NULL;
}

/*	count elements in list l
 */
size_t PianoListCount (const PianoListHead_t * const l) {
	assert (l != NULL);

	size_t count = 0;
	const PianoListHead_t *curr = l;

	PianoListForeach (curr) {
		++count;
	}

	return count;
}

