/*
Copyright (c) 2008-2011
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

#ifndef _XML_H
#define _XML_H

#include "piano.h"

PianoReturn_t PianoXmlParseUserinfo (PianoHandle_t *ph, const char *xml);
PianoReturn_t PianoXmlParseStations (PianoHandle_t *ph, const char *xml);
PianoReturn_t PianoXmlParsePlaylist (PianoHandle_t *ph, const char *xml,
		PianoSong_t **);
PianoReturn_t PianoXmlParseSearch (const char *searchXml,
		PianoSearchResult_t *searchResult);
PianoReturn_t PianoXmlParseSimple (const char *xml);
PianoReturn_t PianoXmlParseCreateStation (PianoHandle_t *ph,
		const char *xml);
PianoReturn_t PianoXmlParseAddSeed (PianoHandle_t *ph, const char *xml,
		PianoStation_t *station);
PianoReturn_t PianoXmlParseGenreExplorer (PianoHandle_t *ph,
		const char *xmlContent);
PianoReturn_t PianoXmlParseTranformStation (const char *searchXml);
PianoReturn_t PianoXmlParseNarrative (const char *xml, char **retNarrative);
PianoReturn_t PianoXmlParseSeedSuggestions (char *, PianoSearchResult_t *);
PianoReturn_t PianoXmlParseGetStationInfo (char *, PianoStationInfo_t *);

char *PianoXmlEncodeString (const char *s);

#endif /* _XML_H */
