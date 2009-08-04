/* ezxml.h
 *
 * Copyright 2004-2006 Aaron Voisine <aaron@voisine.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _EZXML_H
#define _EZXML_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>

#define EZXML_BUFSIZE 1024 // size of internal memory buffers
#define EZXML_NAMEM   0x80 // name is malloced
#define EZXML_TXTM    0x40 // txt is malloced
#define EZXML_DUP     0x20 // attribute name and value are strduped

typedef struct ezxml *ezxml_t;
struct ezxml {
    char *name;      // tag name
    char **attr;     // tag attributes { name, value, name, value, ... NULL }
    char *txt;       // tag character content, empty string if none
    size_t off;      // tag offset from start of parent tag character content
    ezxml_t next;    // next tag with same name in this section at this depth
    ezxml_t sibling; // next tag with different name in same section and depth
    ezxml_t ordered; // next tag, same section and depth, in original order
    ezxml_t child;   // head of sub tag list, NULL if none
    ezxml_t parent;  // parent tag, NULL if current tag is root tag
    short flags;     // additional information
};

// Given a string of xml data and its length, parses it and creates an ezxml
// structure. For efficiency, modifies the data by adding null terminators
// and decoding ampersand sequences. If you don't want this, copy the data and
// pass in the copy. Returns NULL on failure.
ezxml_t ezxml_parse_str(char *s, size_t len);

// returns the first child tag (one level deeper) with the given name or NULL
// if not found
ezxml_t ezxml_child(ezxml_t xml, const char *name);

// returns the next tag of the same name in the same section and depth or NULL
// if not found
#define ezxml_next(xml) ((xml) ? xml->next : NULL)

// Returns the Nth tag with the same name in the same section at the same depth
// or NULL if not found. An index of 0 returns the tag given.
ezxml_t ezxml_idx(ezxml_t xml, int idx);

// returns the name of the given tag
#define ezxml_name(xml) ((xml) ? xml->name : NULL)

// returns the given tag's character content or empty string if none
#define ezxml_txt(xml) ((xml) ? xml->txt : "")

// returns the value of the requested tag attribute, or NULL if not found
const char *ezxml_attr(ezxml_t xml, const char *attr);

// Traverses the ezxml sturcture to retrieve a specific subtag. Takes a
// variable length list of tag names and indexes. The argument list must be
// terminated by either an index of -1 or an empty string tag name. Example: 
// title = ezxml_get(library, "shelf", 0, "book", 2, "title", -1);
// This retrieves the title of the 3rd book on the 1st shelf of library.
// Returns NULL if not found.
ezxml_t ezxml_get(ezxml_t xml, ...);

// frees the memory allocated for an ezxml structure
void ezxml_free(ezxml_t xml);
    
// returns parser error message or empty string if none
const char *ezxml_error(ezxml_t xml);

#endif // _EZXML_H
