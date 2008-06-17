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

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "piano.h"
#include "crypt.h"

void PianoXmlStructParser (xmlNode *structRoot,
		void (*callback) (char *, xmlNode *, void *), void *data);
char *PianoXmlGetNodeText (xmlNode *node);

/*	parse fault and get fault type
 */
void PianoXmlIsFaultCb (char *key, xmlNode *value, void *data) {
	PianoReturn_t *ret = data;
	char *valueStr = PianoXmlGetNodeText (value);
	char *matchStart, *matchEnd, *matchStr;

	if (strcmp ("faultString", key) == 0) {
		*ret = PIANO_RET_ERR;
		/* find fault identifier in a string like this:
		 * com.savagebeast.radio.api.protocol.xmlrpc.RadioXmlRpcException:
		 * 192.168.160.78|1213101717317|AUTH_INVALID_TOKEN|
		 * Invalid auth token */
		if ((matchStart = strchr (valueStr, '|')) != NULL) {
			if ((matchStart = strchr (matchStart+1, '|')) != NULL) {
				if ((matchEnd = strchr (matchStart+1, '|')) != NULL) {
					matchStr = calloc (matchEnd - (matchStart+1)+1,
							sizeof (*matchStr));
					memcpy (matchStr, matchStart+1, matchEnd -
							(matchStart+1));
					/* translate to our error message system */
					if (strcmp ("AUTH_INVALID_TOKEN", matchStr) == 0) {
						*ret = PIANO_RET_AUTH_TOKEN_INVALID;
					} else if (strcmp ("AUTH_INVALID_USERNAME_PASSWORD",
							matchStr) == 0) {
						*ret = PIANO_RET_AUTH_USER_PASSWORD_INVALID;
					} else {
						*ret = PIANO_RET_ERR;
						printf (PACKAGE ": Unknown error %s in %s\n",
								matchStr, valueStr);
					}
					free (matchStr);
				}
			}
		}
	}
}

/*	check whether pandora returned an error or not
 *	@author PromyLOPh
 *	@added 2008-06-16
 *	@param document root of xml doc
 *	@return _RET_OK or fault code (_RET_*)
 */
PianoReturn_t PianoXmlIsFault (xmlNode *docRoot) {
	xmlNode *faultStruct;
	PianoReturn_t ret;

	/* FIXME: we could get into troubles when fault is not the first child
	 * (pandora yould add whitespace e.g.) */
	if (docRoot->children != NULL &&
			docRoot->children->type == XML_ELEMENT_NODE &&
			xmlStrEqual (docRoot->children->name, (xmlChar *) "fault")) {
		/* FIXME: detect fault type */
		faultStruct = docRoot->children->children->children;
		PianoXmlStructParser (faultStruct, PianoXmlIsFaultCb, &ret);
		return ret;
	}
	return PIANO_RET_OK;
}

/*	parses things like this:
 *	<struct>
 *		<member>
 *			<name />
 *			<value />
 *		</member>
 *		<!-- ... -->
 *	</struct>
 *	@author PromyLOPh
 *	@added 2008-06-03
 *	@param xml node named "struct" (or containing a similar structure)
 *	@param who wants to use this data? callback: content of <name> as
 *			string, content of <value> as xmlNode (may contain other nodes
 *			or text), additional data used by callback(); don't forget
 *			to *copy* data taken from <name> or <value> as they will be
 *			freed soon
 *	@param extra data for callback
 */
void PianoXmlStructParser (xmlNode *structRoot,
		void (*callback) (char *, xmlNode *, void *), void *data) {

	xmlNode *curNode, *memberNode, *valueNode;
	xmlChar *key;

	/* get all <member> nodes */
    for (curNode = structRoot->children; curNode; curNode = curNode->next) {
        if (curNode->type == XML_ELEMENT_NODE &&
				xmlStrEqual ((xmlChar *) "member", curNode->name)) {
			key = NULL;
			valueNode = NULL;
			/* check children for <name> or <value> */
			for (memberNode = curNode->children; memberNode;
					memberNode = memberNode->next) {
				if (memberNode->type == XML_ELEMENT_NODE) {
					if (xmlStrEqual ((xmlChar *) "name", memberNode->name)) {
						key = memberNode->children->content;
					} else if (xmlStrEqual ((xmlChar *) "value",
							memberNode->name)) {
						valueNode = memberNode->children;
					}
				}
			}
			/* this will ignore empty <value /> nodes, but well... */
			if (key != NULL && valueNode != NULL) {
				(*callback) ((char *) key, valueNode, data);
			}
        }
	}
}

/*	create xml parser from string
 *	@author PromyLOPh
 *	@added 2008-06-16
 *	@param xml document
 *	@param returns document pointer (needed to free memory later)
 *	@param returns document root
 *	@return _RET_ERR or _RET_OK
 */
PianoReturn_t PianoXmlInitDoc (char *xml, xmlDocPtr *doc, xmlNode **docRoot) {
	*doc = xmlReadDoc ((xmlChar *) xml, NULL, NULL, 0);
	PianoReturn_t ret;

	if (*doc == NULL) {
		printf (PACKAGE ": error while parsing this xml document\n%s\n", xml);
		return PIANO_RET_XML_INVALID;
	}

	*docRoot = xmlDocGetRootElement (*doc);

	if ((ret = PianoXmlIsFault (*docRoot)) != PIANO_RET_OK) {
		return ret;
	}

	return PIANO_RET_OK;
}

/*	get text from <value> nodes; some of them have <boolean>, <string>
 *	or <int> subnodes, just ignore them
 *	@author PromyLOPh
 *	@added 2008-06-13
 *	@param xml node <value>
 */
char *PianoXmlGetNodeText (xmlNode *node) {
	/* FIXME: this is not the correct way; we should check the node type
	 * as well */
	if (node->content != NULL) {
		return (char *) node->content;
	} else if (node->children != NULL &&
			node->children->content != NULL) {
		return (char *) node->children->content;
	}
	return NULL;
}

/*	structParser callback; writes userinfo to PianoUserInfo structure
 *	@author PromyLOPh
 *	@added 2008-06-03
 *	@param value identifier
 *	@param value node
 *	@param pointer to userinfo structure
 *	@return nothing
 */
void PianoXmlParseUserinfoCb (char *key, xmlNode *value, void *data) {
	PianoUserInfo_t *user = data;
	char *valueStr = PianoXmlGetNodeText (value);

	/* FIXME: should be continued later */
	if (strcmp ("webAuthToken", key) == 0) {
		user->webAuthToken = strdup (valueStr);
	} else if (strcmp ("authToken", key) == 0) {
		user->authToken = strdup (valueStr);
	} else if (strcmp ("listenerId", key) == 0) {
		user->listenerId = strdup (valueStr);
	}
}

void PianoXmlParseStationsCb (char *key, xmlNode *value, void *data) {
	PianoStation_t *station = data;
	char *valueStr = PianoXmlGetNodeText (value);

	if (strcmp ("stationName", key) == 0) {
		station->name = strdup (valueStr);
	} else if (strcmp ("stationId", key) == 0) {
		station->id = strdup (valueStr);
	}
}

/* FIXME: copy & waste */
void PianoXmlParsePlaylistCb (char *key, xmlNode *value, void *data) {
	PianoSong_t *song = data;
	char *valueStr = PianoXmlGetNodeText (value);

	if (strcmp ("audioURL", key) == 0) {
		/* last 48 chars of audioUrl are encrypted, but they put the key
		 * into the door's lock; dumb pandora... */
		const char urlTailN = 48;
		char *urlTail, *urlTailCrypted = valueStr + (strlen (valueStr) - urlTailN);
		urlTail = PianoDecryptString (urlTailCrypted);
		song->audioUrl = calloc (strlen (valueStr) + 1, sizeof (char));
		strncpy (song->audioUrl, valueStr, strlen (valueStr) - urlTailN);
		/* FIXME: the key seems to be broken... so ignore 8 x 0x08 postfix;
		 * urlTailN/2 because the encrypted hex string is now decoded */
		strncat (song->audioUrl, urlTail, (urlTailN/2)-8);
		free (urlTail);

	} else if (strcmp ("artistSummary", key) == 0) {
		song->artist = strdup (valueStr);
	} else if (strcmp ("musicId", key) == 0) {
		song->musicId = strdup (valueStr);
	} else if (strcmp ("matchingSeed", key) == 0) {
		song->matchingSeed = strdup (valueStr);
	} else if (strcmp ("userSeed", key) == 0) {
		song->userSeed = strdup (valueStr);
	} else if (strcmp ("focusTraitId", key) == 0) {
		song->focusTraitId = strdup (valueStr);
	} else if (strcmp ("songTitle", key) == 0) {
		song->title = strdup (valueStr);
	} else if (strcmp ("rating", key) == 0) {
		if (strcmp (valueStr, "1") == 0) {
			song->rating = PIANO_RATE_LOVE;
		} else {
			song->rating = PIANO_RATE_NONE;
		}
	}
}

/*	parses userinfos sent by pandora as login response
 *	@author PromyLOPh
 *	@added 2008-06-03
 *	@param piano handle
 *	@param utf-8 string
 *	@return _RET_OK or error
 */
PianoReturn_t PianoXmlParseUserinfo (PianoHandle_t *ph, char *xml) {
	xmlNode *docRoot;
	xmlDocPtr doc;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &doc, &docRoot)) != PIANO_RET_OK) {
		return ret;
	}

	/* <methodResponse> <params> <param> <value> <struct> */
	xmlNode *structRoot = docRoot->children->children->children->children;
	PianoXmlStructParser (structRoot, PianoXmlParseUserinfoCb, &ph->user);

	xmlFreeDoc (doc);
	return PIANO_RET_OK;
}

/*	parse stations returned by pandora
 *	@author PromyLOPh
 *	@added 2008-06-04
 *	@param piano handle
 *	@param xml returned by pandora
 *	@return _RET_OK or error
 */
PianoReturn_t PianoXmlParseStations (PianoHandle_t *ph, char *xml) {
	xmlNode *docRoot, *curNode;
	xmlDocPtr doc;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &doc, &docRoot)) != PIANO_RET_OK) {
		return ret;
	}

	/* <methodResponse> <params> <param> <value> <array> <data> */
	xmlNode *dataRoot = docRoot->children->children->children->children->children;
    for (curNode = dataRoot->children; curNode; curNode = curNode->next) {
        if (curNode->type == XML_ELEMENT_NODE &&
				xmlStrEqual ((xmlChar *) "value", curNode->name)) {
			PianoStation_t *tmpStation = calloc (1, sizeof (*tmpStation));
			PianoXmlStructParser (curNode->children,
					PianoXmlParseStationsCb, tmpStation);
			/* start new linked list or append */
			if (ph->stations == NULL) {
				ph->stations = tmpStation;
			} else {
				PianoStation_t *curStation = ph->stations;
				while (curStation->next != NULL) {
					curStation = curStation->next;
				}
				curStation->next = tmpStation;
			}
		}
	}

	xmlFreeDoc (doc);
	return PIANO_RET_OK;
}

/*	parse "create station" answer (it returns a new station structure)
 *	@author PromyLOPh
 *	@added 2008-06-16
 *	@param piano handle
 *	@param xml document
 *	@return nothing yet
 */
PianoReturn_t PianoXmlParseCreateStation (PianoHandle_t *ph, char *xml) {
	xmlNode *docRoot;
	xmlDocPtr doc;
	PianoStation_t *tmpStation;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &doc, &docRoot)) != PIANO_RET_OK) {
		return ret;
	}

	/* get <struct> node */
	xmlNode *dataRoot = docRoot->children->children->children->children;
	tmpStation = calloc (1, sizeof (*tmpStation));
	PianoXmlStructParser (dataRoot, PianoXmlParseStationsCb, tmpStation);
	/* FIXME: copy & waste */
	/* start new linked list or append */
	if (ph->stations == NULL) {
		ph->stations = tmpStation;
	} else {
		PianoStation_t *curStation = ph->stations;
		while (curStation->next != NULL) {
			curStation = curStation->next;
		}
		curStation->next = tmpStation;
	}
	
	xmlFreeDoc (doc);

	return PIANO_RET_OK;
}

/*	parses playlist; used when searching too
 *	@author PromyLOPh
 *	@added 2008-06-12
 *	@param piano handle
 *	@param xml document
 */
PianoReturn_t PianoXmlParsePlaylist (PianoHandle_t *ph, char *xml) {
	xmlNode *docRoot, *curNode;
	xmlDocPtr doc;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &doc, &docRoot)) != PIANO_RET_OK) {
		return ret;
	}

	/* <methodResponse> <params> <param> <value> <array> <data> */
	xmlNode *dataRoot = docRoot->children->children->children->children->children;
    for (curNode = dataRoot->children; curNode; curNode = curNode->next) {
        if (curNode->type == XML_ELEMENT_NODE &&
				xmlStrEqual ((xmlChar *) "value", curNode->name)) {
			PianoSong_t *tmpSong = calloc (1, sizeof (*tmpSong));
			PianoXmlStructParser (curNode->children,
					PianoXmlParsePlaylistCb, tmpSong);
			/* begin linked list or append */
			if (ph->playlist == NULL) {
				ph->playlist = tmpSong;
			} else {
				PianoSong_t *curSong = ph->playlist;
				while (curSong->next != NULL) {
					curSong = curSong->next;
				}
				curSong->next = tmpSong;
			}
		}
	}

	xmlFreeDoc (doc);

	return PIANO_RET_OK;
}

/*	parse simple answers like this: <?xml version="1.0" encoding="UTF-8"?>
 *	<methodResponse><params><param><value>1</value></param></params>
 *	</methodResponse>
 *	@author PromyLOPh
 *	@added 2008-06-10
 *	@param xml string
 *	@return
 */
PianoReturn_t PianoXmlParseSimple (char *xml) {
	xmlNode *docRoot;
	xmlDocPtr doc;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &doc, &docRoot)) != PIANO_RET_OK) {
		return ret;
	}

	xmlNode *val = docRoot->children->children->children->children;
	if (xmlStrEqual (val->content, (xmlChar *) "1")) {
		ret = PIANO_RET_OK;
	} else {
		ret = PIANO_RET_ERR;
	}

	xmlFreeDoc (doc);

	return ret;
}

/*	xml struct parser callback, used in PianoXmlParseSearchCb
 *	@author PromyLOPh
 *	@added 2008-06-12
 */
void PianoXmlParseSearchArtistCb (char *key, xmlNode *value, void *data) {
	PianoArtist_t *artist = data;
	char *valueStr = PianoXmlGetNodeText (value);

	if (strcmp ("artistName", key) == 0) {
		artist->name = strdup (valueStr);
	} else if (strcmp ("musicId", key) == 0) {
		artist->musicId = strdup (valueStr);
	}
}

/*	callback for xml struct parser used in PianoXmlParseSearch, "switch" for
 *	PianoXmlParseSearchArtistCb and PianoXmlParsePlaylistCb
 *	@author PromyLOPh
 *	@added 2008-06-12
 */
void PianoXmlParseSearchCb (char *key, xmlNode *value, void *data) {
	PianoSearchResult_t *searchResult = data;

	if (strcmp ("artists", key) == 0) {
		xmlNode *curNode;

		/* skip <value><array><data> */
		for (curNode = value->children->children; curNode;
				curNode = curNode->next) {
	        if (curNode->type == XML_ELEMENT_NODE &&
					xmlStrEqual ((xmlChar *) "value", curNode->name)) {

				PianoArtist_t *artist = calloc (1, sizeof (*artist));
				memset (artist, 0, sizeof (*artist));

				PianoXmlStructParser (curNode->children,
						PianoXmlParseSearchArtistCb, artist);

				/* add result to linked list */
				if (searchResult->artists == NULL) {
					searchResult->artists = artist;
				} else {
					PianoArtist_t *curArtist = searchResult->artists;
					while (curArtist->next != NULL) {
						curArtist = curArtist->next;
					}
					curArtist->next = artist;
				}
			}
		}
	} else if (strcmp ("songs", key) == 0) {
		xmlNode *curNode;

		for (curNode = value->children->children; curNode;
				curNode = curNode->next) {
	        if (curNode->type == XML_ELEMENT_NODE &&
					xmlStrEqual ((xmlChar *) "value", curNode->name)) {
				/* FIXME: copy & waste */
				PianoSong_t *tmpSong = calloc (1, sizeof (*tmpSong));
				PianoXmlStructParser (curNode->children,
						PianoXmlParsePlaylistCb, tmpSong);
				/* begin linked list or append */
				if (searchResult->songs == NULL) {
					searchResult->songs = tmpSong;
				} else {
					PianoSong_t *curSong = searchResult->songs;
					while (curSong->next != NULL) {
						curSong = curSong->next;
					}
					curSong->next = tmpSong;
				}
			}
		}
	}
}

/*	parse search result; searchResult is nulled before use
 *	@author PromyLOPh
 *	@added 2008-06-12
 *	@param xml document
 *	@param returns search result
 *	@return nothing yet
 */
PianoReturn_t PianoXmlParseSearch (char *searchXml,
		PianoSearchResult_t *searchResult) {
	xmlNode *docRoot;
	xmlDocPtr doc;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (searchXml, &doc, &docRoot)) != PIANO_RET_OK) {
		return ret;
	}
	
	xmlNode *structRoot = docRoot->children->children->children->children;
	/* we need a "clean" search result (with null pointers) */
	memset (searchResult, 0, sizeof (*searchResult));
	PianoXmlStructParser (structRoot, PianoXmlParseSearchCb, searchResult);

	xmlFreeDoc (doc);

	return PIANO_RET_OK;
}

/*	encode reserved xml chars
 *	@author PromyLOPh
 *	@added 2008-06-10
 *	@param encode this
 *	@return encoded string
 */
char *PianoXmlEncodeString (const char *s) {
	char *replacements[] = {"&&amp;", "'&apos;", "\"&quot;", "<&lt;", ">&gt;"};
	char *s_new = NULL;
	unsigned int s_new_n = 0;
	unsigned int i;
	char found = 0;

	s_new_n = strlen (s) + 1;
	s_new = calloc (s_new_n, 1);

	while (*s) {
		found = 0;
		for (i = 0; i < sizeof (replacements) / sizeof (*replacements); i++) {
			if (*s == replacements[i][0]) {
				/* replacements[i]+1 is our replacement, and -1 'cause we
				 * "overwrite" one byte */
				s_new_n += strlen (replacements[i]+1)-1;
				s_new = realloc (s_new, s_new_n);
				strncat (s_new, replacements[i]+1, strlen (replacements[i]+1));
				found = 1;
				/* no need to look for other entities */
				break;
			}
		}
		if (!found) {
			strncat (s_new, s, 1);
		}
		s++;
	}
	return s_new;
}
