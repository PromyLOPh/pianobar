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

#ifndef __FreeBSD__
#define _BSD_SOURCE /* required by strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ezxml.h>
#include <assert.h>

#include "piano.h"
#include "crypt.h"
#include "config.h"
#include "piano_private.h"

static void PianoXmlStructParser (const ezxml_t,
		void (*callback) (const char *, const ezxml_t, void *), void *);
static char *PianoXmlGetNodeText (const ezxml_t);

/*	parse fault and get fault type
 *	@param xml <name> content
 *	@param xml <value> node
 *	@param return error string
 *	@return nothing
 */
static void PianoXmlIsFaultCb (const char *key, const ezxml_t value,
		void *data) {
	PianoReturn_t *ret = data;
	char *valueStr = PianoXmlGetNodeText (value);
	char *matchStart, *matchEnd;

	if (strcmp ("faultString", key) == 0) {
		*ret = PIANO_RET_ERR;
		/* find fault identifier in a string like this:
		 * com.savagebeast.radio.api.protocol.xmlrpc.RadioXmlRpcException:
		 * 192.168.160.78|1213101717317|AUTH_INVALID_TOKEN|
		 * Invalid auth token */
		if ((matchStart = strchr (valueStr, '|')) != NULL) {
			if ((matchStart = strchr (matchStart+1, '|')) != NULL) {
				if ((matchEnd = strchr (matchStart+1, '|')) != NULL) {
					/* changes text in xml node, but we don't care... */
					*matchEnd = '\0';
					++matchStart;
					/* translate to our error message system */
					if (strcmp ("AUTH_INVALID_TOKEN", matchStart) == 0) {
						*ret = PIANO_RET_AUTH_TOKEN_INVALID;
					} else if (strcmp ("AUTH_INVALID_USERNAME_PASSWORD",
							matchStart) == 0) {
						*ret = PIANO_RET_AUTH_USER_PASSWORD_INVALID;
					} else if (strcmp ("LISTENER_NOT_AUTHORIZED",
							matchStart) == 0) {
						*ret = PIANO_RET_NOT_AUTHORIZED;
					} else if (strcmp ("INCOMPATIBLE_VERSION",
							matchStart) == 0) {
						*ret = PIANO_RET_PROTOCOL_INCOMPATIBLE;
					} else if (strcmp ("READONLY_MODE", matchStart) == 0) {
						*ret = PIANO_RET_READONLY_MODE;
					} else if (strcmp ("STATION_CODE_INVALID",
							matchStart) == 0) {
						*ret = PIANO_RET_STATION_CODE_INVALID;
					} else if (strcmp ("STATION_DOES_NOT_EXIST",
							matchStart) == 0) {
						*ret = PIANO_RET_STATION_NONEXISTENT;
					} else if (strcmp ("OUT_OF_SYNC", matchStart) == 0) {
						*ret = PIANO_RET_OUT_OF_SYNC;
					} else if (strcmp ("PLAYLIST_END", matchStart) == 0) {
						*ret = PIANO_RET_PLAYLIST_END;
					} else if (strcmp ("QUICKMIX_NOT_PLAYABLE", matchStart) == 0) {
						*ret = PIANO_RET_QUICKMIX_NOT_PLAYABLE;
					} else if (strcmp ("REMOVING_TOO_MANY_SEEDS", matchStart) == 0) {
						*ret = PIANO_RET_REMOVING_TOO_MANY_SEEDS;
					} else {
						*ret = PIANO_RET_ERR;
						printf (PACKAGE ": Unknown error %s in %s\n",
								matchStart, valueStr);
					}
				}
			}
		}
	} else if (strcmp ("faultCode", key) == 0) {
		/* some errors can only be identified by looking at their id */
		/* detect pandora's ip restriction */
		if (strcmp ("12", valueStr) == 0) {
			*ret = PIANO_RET_IP_REJECTED;
		}
	}
}

/*	check whether pandora returned an error or not
 *	@param document root of xml doc
 *	@return _RET_OK or fault code (_RET_*)
 */
static PianoReturn_t PianoXmlIsFault (ezxml_t xmlDoc) {
	PianoReturn_t ret;

	if ((xmlDoc = ezxml_child (xmlDoc, "fault")) != NULL) {
		xmlDoc = ezxml_get (xmlDoc, "value", 0, "struct", -1);
		PianoXmlStructParser (xmlDoc, PianoXmlIsFaultCb, &ret);
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
 *	@param xml node named "struct" (or containing a similar structure)
 *	@param who wants to use this data? callback: content of <name> as
 *			string, content of <value> as xmlNode (may contain other nodes
 *			or text), additional data used by callback(); don't forget
 *			to *copy* data taken from <name> or <value> as they will be
 *			freed soon
 *	@param extra data for callback
 */
static void PianoXmlStructParser (const ezxml_t structRoot,
		void (*callback) (const char *, const ezxml_t, void *), void *data) {
	ezxml_t curNode, keyNode, valueNode;
	char *key;

	/* get all <member> nodes */
    for (curNode = ezxml_child (structRoot, "member"); curNode; curNode = curNode->next) {
		/* reset variables */
		key = NULL;
		valueNode = keyNode = NULL;

		keyNode = ezxml_child (curNode, "name");
		if (keyNode != NULL) {
			key = ezxml_txt (keyNode);
		}

		valueNode = ezxml_child (curNode, "value");
		/* this will ignore empty <value /> nodes, but well... */
		if (*key != '\0' && valueNode != NULL) {
			(*callback) ((char *) key, valueNode, data);
		}
	}
}

/*	create xml parser from string
 *	@param xml document
 *	@param returns document pointer (needed to free memory later)
 *	@param returns document root
 *	@return _OK or error
 */
static PianoReturn_t PianoXmlInitDoc (char *xmlStr, ezxml_t *xmlDoc) {
	PianoReturn_t ret;

	if ((*xmlDoc = ezxml_parse_str (xmlStr, strlen (xmlStr))) == NULL) {
		return PIANO_RET_XML_INVALID;
	}

	if ((ret = PianoXmlIsFault (*xmlDoc)) != PIANO_RET_OK) {
		ezxml_free (*xmlDoc);
		return ret;
	}

	return PIANO_RET_OK;
}

/*	get text from <value> nodes; some of them have <boolean>, <string>
 *	or <int> subnodes, just ignore them
 *	@param xml node <value>
 */
static char *PianoXmlGetNodeText (const ezxml_t node) {
	char *retTxt = NULL;

	retTxt = ezxml_txt (node);
	/* no text => empty string */
	if (*retTxt == '\0') {
		retTxt = ezxml_txt (node->child);
	}
	return retTxt;
}

/*	structParser callback; writes userinfo to PianoUserInfo structure
 *	@param value identifier
 *	@param value node
 *	@param pointer to userinfo structure
 *	@return nothing
 */
static void PianoXmlParseUserinfoCb (const char *key, const ezxml_t value,
		void *data) {
	PianoUserInfo_t *user = data;
	char *valueStr = PianoXmlGetNodeText (value);

	if (strcmp ("webAuthToken", key) == 0) {
		user->webAuthToken = strdup (valueStr);
	} else if (strcmp ("authToken", key) == 0) {
		user->authToken = strdup (valueStr);
	} else if (strcmp ("listenerId", key) == 0) {
		user->listenerId = strdup (valueStr);
	}
}

static void PianoXmlParseStationsCb (const char *key, const ezxml_t value,
		void *data) {
	PianoStation_t *station = data;
	char *valueStr = PianoXmlGetNodeText (value);

	if (strcmp ("stationName", key) == 0) {
		station->name = strdup (valueStr);
	} else if (strcmp ("stationId", key) == 0) {
		station->id = strdup (valueStr);
	} else if (strcmp ("isQuickMix", key) == 0) {
		station->isQuickMix = (strcmp (valueStr, "1") == 0);
	} else if (strcmp ("isCreator", key) == 0) {
		station->isCreator = (strcmp (valueStr, "1") == 0);
	}
}

static void PianoXmlParsePlaylistCb (const char *key, const ezxml_t value,
		void *data) {
	PianoSong_t *song = data;
	char *valueStr = PianoXmlGetNodeText (value);

	if (strcmp ("audioURL", key) == 0) {
		/* last 48 chars of audioUrl are encrypted, but they put the key
		 * into the door's lock... */
		const char urlTailN = 48;
		const size_t valueStrN = strlen (valueStr);
		char *urlTail = NULL,
				*urlTailCrypted = &valueStr[valueStrN - urlTailN];

		/* don't try to decrypt if string is too short (=> invalid memory
		 * reads/writes) */
		if (valueStrN > urlTailN &&
				(urlTail = PianoDecryptString (urlTailCrypted)) != NULL) {
			if ((song->audioUrl = calloc (valueStrN + 1,
					sizeof (*song->audioUrl))) != NULL) {
				memcpy (song->audioUrl, valueStr, valueStrN - urlTailN);
				/* FIXME: the key seems to be broken... so ignore 8 x 0x08
				 * postfix; urlTailN/2 because the encrypted hex string is now
				 * decoded */
				memcpy (&song->audioUrl[valueStrN - urlTailN], urlTail,
						urlTailN/2 - 8);
			}
			free (urlTail);
		}
	} else if (strcmp ("artRadio", key) == 0) {
		song->coverArt = strdup (valueStr);
	} else if (strcmp ("artistSummary", key) == 0) {
		song->artist = strdup (valueStr);
	} else if (strcmp ("musicId", key) == 0) {
		song->musicId = strdup (valueStr);
	} else if (strcmp ("userSeed", key) == 0) {
		song->userSeed = strdup (valueStr);
	} else if (strcmp ("songTitle", key) == 0) {
		song->title = strdup (valueStr);
	} else if (strcmp ("rating", key) == 0) {
		if (strcmp (valueStr, "1") == 0) {
			song->rating = PIANO_RATE_LOVE;
		} else {
			song->rating = PIANO_RATE_NONE;
		}
	} else if (strcmp ("isPositive", key) == 0) {
		if (strcmp (valueStr, "1") == 0) {
			song->rating = PIANO_RATE_LOVE;
		} else {
			song->rating = PIANO_RATE_BAN;
		}
	} else if (strcmp ("stationId", key) == 0) {
		song->stationId = strdup (valueStr);
	} else if (strcmp ("albumTitle", key) == 0) {
		song->album = strdup (valueStr);
	} else if (strcmp ("fileGain", key) == 0) {
		song->fileGain = atof (valueStr);
	} else if (strcmp ("audioEncoding", key) == 0) {
		if (strcmp (valueStr, "aacplus") == 0) {
			song->audioFormat = PIANO_AF_AACPLUS;
		} else if (strcmp (valueStr, "mp3") == 0) {
			song->audioFormat = PIANO_AF_MP3;
		} else if (strcmp (valueStr, "mp3-hifi") == 0) {
			song->audioFormat = PIANO_AF_MP3_HI;
		}
	} else if (strcmp ("artistMusicId", key) == 0) {
		song->artistMusicId = strdup (valueStr);
	} else if (strcmp ("feedbackId", key) == 0) {
		song->feedbackId = strdup (valueStr);
	} else if (strcmp ("songDetailURL", key) == 0) {
		song->detailUrl = strdup (valueStr);
	} else if (strcmp ("trackToken", key) == 0) {
		song->trackToken = strdup (valueStr);
	}
}

/*	parses userinfos sent by pandora as login response
 *	@param piano handle
 *	@param utf-8 string
 *	@return _RET_OK or error
 */
PianoReturn_t PianoXmlParseUserinfo (PianoHandle_t *ph, char *xml) {
	ezxml_t xmlDoc, structNode;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}

	/* <methodResponse> <params> <param> <value> <struct> */
	structNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", 0, "struct", -1);
	PianoXmlStructParser (structNode, PianoXmlParseUserinfoCb, &ph->user);

	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

static void PianoXmlParseQuickMixStationsCb (const char *key, const ezxml_t value,
		void *data) {
	char ***retIds = data;
	char **ids = NULL;
	size_t idsN = 0;
	ezxml_t curNode;

	if (strcmp ("quickMixStationIds", key) == 0) {
		for (curNode = ezxml_child (ezxml_get (value, "array", 0, "data", -1), "value");
				curNode; curNode = curNode->next) {
			idsN++;
			if (ids == NULL) {
				if ((ids = calloc (idsN, sizeof (*ids))) == NULL) {
					*retIds = NULL;
					return;
				}
			} else {
				/* FIXME: memory leak (on failure) */
				if ((ids = realloc (ids, idsN * sizeof (*ids))) == NULL) {
					*retIds = NULL;
					return;
				}
			}
			ids[idsN-1] = strdup (PianoXmlGetNodeText (curNode));
		}
		/* append NULL: list ends here */
		idsN++;
		/* FIXME: copy&waste */
		if (ids == NULL) {
			if ((ids = calloc (idsN, sizeof (*ids))) == NULL) {
				*retIds = NULL;
				return;
			}
		} else {
			if ((ids = realloc (ids, idsN * sizeof (*ids))) == NULL) {
				*retIds = NULL;
				return;
			}
		}
		ids[idsN-1] = NULL;

		*retIds = ids;
	}
}

/*	parse stations returned by pandora
 *	@param piano handle
 *	@param xml returned by pandora
 *	@return _RET_OK or error
 */
PianoReturn_t PianoXmlParseStations (PianoHandle_t *ph, char *xml) {
	ezxml_t xmlDoc, dataNode;
	PianoReturn_t ret;
	char **quickMixIds = NULL, **curQuickMixId = NULL;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}

	dataNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", 0, "array",
			0, "data", -1);

	for (dataNode = ezxml_child (dataNode, "value"); dataNode;
			dataNode = dataNode->next) {
		PianoStation_t *tmpStation;

		if ((tmpStation = calloc (1, sizeof (*tmpStation))) == NULL) {
			ezxml_free (xmlDoc);
			return PIANO_RET_OUT_OF_MEMORY;
		}

		PianoXmlStructParser (ezxml_child (dataNode, "struct"),
				PianoXmlParseStationsCb, tmpStation);

		/* get stations selected for quickmix */
		if (tmpStation->isQuickMix) {
			PianoXmlStructParser (ezxml_child (dataNode, "struct"),
					PianoXmlParseQuickMixStationsCb, &quickMixIds);
		}
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
	/* set quickmix flags after all stations are read */
	if (quickMixIds != NULL) {
		curQuickMixId = quickMixIds;
		while (*curQuickMixId != NULL) {
			PianoStation_t *curStation = PianoFindStationById (ph->stations,
					*curQuickMixId);
			if (curStation != NULL) {
				curStation->useQuickMix = 1;
			}
			free (*curQuickMixId);
			curQuickMixId++;
		}
		free (quickMixIds);
	}

	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

/*	parse "create station" answer (it returns a new station structure)
 *	@param piano handle
 *	@param xml document
 *	@return nothing yet
 */
PianoReturn_t PianoXmlParseCreateStation (PianoHandle_t *ph, char *xml) {
	ezxml_t xmlDoc, dataNode;
	PianoStation_t *tmpStation;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}

	dataNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", 0, "struct", -1);

	if ((tmpStation = calloc (1, sizeof (*tmpStation))) == NULL) {
		ezxml_free (xmlDoc);
		return PIANO_RET_OUT_OF_MEMORY;
	}
	PianoXmlStructParser (dataNode, PianoXmlParseStationsCb, tmpStation);
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
	
	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

/*	parse "add seed" answer, nearly the same as ParseCreateStation
 *	@param piano handle
 *	@param xml document
 *	@param update this station
 */
PianoReturn_t PianoXmlParseAddSeed (PianoHandle_t *ph, char *xml,
		PianoStation_t *station) {
	ezxml_t xmlDoc, dataNode;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}

	dataNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", 0, "struct", -1);
	PianoDestroyStation (station);
	PianoXmlStructParser (dataNode, PianoXmlParseStationsCb, station);
	
	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

static PianoReturn_t PianoXmlParsePlaylistStruct (ezxml_t xml,
		PianoSong_t **retSong) {
	PianoSong_t *playlist = *retSong, *tmpSong;
	
	if ((tmpSong = calloc (1, sizeof (*tmpSong))) == NULL) {
		return PIANO_RET_OUT_OF_MEMORY;
	}

	PianoXmlStructParser (ezxml_child (xml, "struct"), PianoXmlParsePlaylistCb,
			tmpSong);
	/* begin linked list or append */
	if (playlist == NULL) {
		playlist = tmpSong;
	} else {
		PianoSong_t *curSong = playlist;
		while (curSong->next != NULL) {
			curSong = curSong->next;
		}
		curSong->next = tmpSong;
	}

	*retSong = playlist;

	return PIANO_RET_OK;
}

/*	parses playlist; used when searching too
 *	@param piano handle
 *	@param xml document
 *	@param return: playlist
 */
PianoReturn_t PianoXmlParsePlaylist (PianoHandle_t *ph, char *xml,
		PianoSong_t **retPlaylist) {
	ezxml_t xmlDoc, dataNode;
	PianoReturn_t ret = PIANO_RET_OK;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}

	dataNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", 0, "array",
			0, "data", -1);

	for (dataNode = ezxml_child (dataNode, "value"); dataNode;
			dataNode = dataNode->next) {
		if ((ret = PianoXmlParsePlaylistStruct (dataNode, retPlaylist)) !=
				PIANO_RET_OK) {
			break;
		}
	}

	ezxml_free (xmlDoc);

	return ret;
}

/*	check for exception only
 *	@param xml string
 *	@return _OK or error
 */
PianoReturn_t PianoXmlParseSimple (char *xml) {
	ezxml_t xmlDoc;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}

	ezxml_free (xmlDoc);

	return ret;
}

/*	xml struct parser callback, used in PianoXmlParseSearchCb
 */
static void PianoXmlParseSearchArtistCb (const char *key, const ezxml_t value,
		void *data) {
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
 */
static void PianoXmlParseSearchCb (const char *key, const ezxml_t value,
		void *data) {
	PianoSearchResult_t *searchResult = data;
	ezxml_t curNode;

	if (strcmp ("artists", key) == 0) {
		/* skip <value><array><data> */
		for (curNode = ezxml_child (ezxml_get (value, "array", 0, "data", -1), "value");
				curNode; curNode = curNode->next) {
			PianoArtist_t *artist;
			
			if ((artist = calloc (1, sizeof (*artist))) == NULL) {
				/* fail silently */
				break;
			}

			memset (artist, 0, sizeof (*artist));

			PianoXmlStructParser (ezxml_child (curNode, "struct"),
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
	} else if (strcmp ("songs", key) == 0) {
		for (curNode = ezxml_child (ezxml_get (value, "array", 0, "data", -1), "value");
				curNode; curNode = curNode->next) {
			if (PianoXmlParsePlaylistStruct (curNode, &searchResult->songs) !=
					PIANO_RET_OK) {
				break;
			}
		}
	}
}

/*	parse search result; searchResult is nulled before use
 *	@param xml document
 *	@param returns search result
 *	@return nothing yet
 */
PianoReturn_t PianoXmlParseSearch (char *xml,
		PianoSearchResult_t *searchResult) {
	ezxml_t xmlDoc, dataNode;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}
	
	dataNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", 0, "struct", -1);
	/* we need a "clean" search result (with null pointers) */
	memset (searchResult, 0, sizeof (*searchResult));
	PianoXmlStructParser (dataNode, PianoXmlParseSearchCb, searchResult);

	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

/*	FIXME: copy&waste (PianoXmlParseSearch)
 */
PianoReturn_t PianoXmlParseSeedSuggestions (char *xml,
		PianoSearchResult_t *searchResult) {
	ezxml_t xmlDoc, dataNode;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}
	
	dataNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", -1);
	/* we need a "clean" search result (with null pointers) */
	memset (searchResult, 0, sizeof (*searchResult));
	/* reuse seach result parser; structure is nearly the same */
	PianoXmlParseSearchCb ("artists", dataNode, searchResult);

	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

/*	encode reserved xml chars
 *	TODO: remove and use ezxml_ampencode
 *	@param encode this
 *	@return encoded string or NULL
 */
char *PianoXmlEncodeString (const char *s) {
	char *replacements[] = {"&&amp;", "'&apos;", "\"&quot;", "<&lt;",
			">&gt;", NULL};
	char **r, *sOut, *sOutCurr, found;

	if ((sOut = calloc (strlen (s) * 5 + 1, sizeof (*sOut))) == NULL) {
		return NULL;
	}

	sOutCurr = sOut;

	while (*s != '\0') {
		r = replacements;
		found = 0;
		while (*r != NULL) {
			if (*s == *r[0]) {
				found = 1;
				strcat (sOutCurr, (*r) + 1);
				sOutCurr += strlen ((*r) + 1);
				break;
			}
			r++;
		}
		if (!found) {
			*sOutCurr = *s;
			sOutCurr++;
		}
		s++;
	}
	return sOut;
}

PianoReturn_t PianoXmlParseGenreExplorer (PianoHandle_t *ph, char *xml) {
	ezxml_t xmlDoc, catNode;
	PianoReturn_t ret;

    if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
        return ret;
    }

	/* get all <member> nodes */
    for (catNode = ezxml_child (xmlDoc, "category"); catNode;
			catNode = catNode->next) {
		PianoGenreCategory_t *tmpGenreCategory;
		ezxml_t genreNode;

		if ((tmpGenreCategory = calloc (1, sizeof (*tmpGenreCategory))) == NULL) {
			ezxml_free (xmlDoc);
			return PIANO_RET_OUT_OF_MEMORY;
		}

		tmpGenreCategory->name = strdup (ezxml_attr (catNode, "categoryName"));

		/* get genre subnodes */
		for (genreNode = ezxml_child (catNode, "genre"); genreNode;
				genreNode = genreNode->next) {
			PianoGenre_t *tmpGenre;

			if ((tmpGenre = calloc (1, sizeof (*tmpGenre))) == NULL) {
				ezxml_free (xmlDoc);
				return PIANO_RET_OUT_OF_MEMORY;
			}

			/* get genre attributes */
			tmpGenre->name = strdup (ezxml_attr (genreNode, "name"));
			tmpGenre->musicId = strdup (ezxml_attr (genreNode, "musicId"));

			/* append station */
			if (tmpGenreCategory->genres == NULL) {
				tmpGenreCategory->genres = tmpGenre;
			} else {
				PianoGenre_t *curGenre =
						tmpGenreCategory->genres;
				while (curGenre->next != NULL) {
					curGenre = curGenre->next;
				}
				curGenre->next = tmpGenre;
			}
		}
		/* append category */
		if (ph->genreStations == NULL) {
			ph->genreStations = tmpGenreCategory;
		} else {
			PianoGenreCategory_t *curCat = ph->genreStations;
			while (curCat->next != NULL) {
				curCat = curCat->next;
			}
			curCat->next = tmpGenreCategory;
		}
	}

	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

/*	dummy function, only checks for errors
 *	@param xml doc
 *	@return _OK or error
 */
PianoReturn_t PianoXmlParseTranformStation (char *xml) {
	ezxml_t xmlDoc;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}
	
	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

/*	parses "why did you play ...?" answer
 *	@param xml
 *	@param returns the answer
 *	@return _OK or error
 */
PianoReturn_t PianoXmlParseNarrative (char *xml, char **retNarrative) {
	ezxml_t xmlDoc, dataNode;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}

	/* <methodResponse> <params> <param> <value> $textnode */
	dataNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", -1);
	*retNarrative = strdup (ezxml_txt (dataNode));

	ezxml_free (xmlDoc);

	return ret;
}

/*	seed bag, required because seedId is not part of artist/song struct in
 *	pandora's xml response
 */
struct PianoXmlParseSeedBag {
	char *seedId;
	PianoSong_t *song;
	PianoArtist_t *artist;
	PianoStation_t *station;
};

/*	parse seed struct
 */
static void PianoXmlParseSeedCb (const char *key, const ezxml_t value,
		void *data) {
	struct PianoXmlParseSeedBag *bag = data;

	assert (bag != NULL);

	if (strcmp ("song", key) == 0) {
		assert (bag->song == NULL);

		if ((bag->song = calloc (1, sizeof (*bag->song))) == NULL) {
			return;
		}

		PianoXmlStructParser (ezxml_child (value, "struct"),
				PianoXmlParsePlaylistCb, bag->song);
	} else if (strcmp ("artist", key) == 0) {
		assert (bag->artist == NULL);

		if ((bag->artist = calloc (1, sizeof (*bag->artist))) == NULL) {
			return;
		}

		PianoXmlStructParser (ezxml_child (value, "struct"),
				PianoXmlParseSearchArtistCb, bag->artist);
	} else if (strcmp ("nonGenomeStation", key) == 0) {
		/* genre stations are "non genome" station seeds */
		assert (bag->station == NULL);

		if ((bag->station = calloc (1, sizeof (*bag->station))) == NULL) {
			return;
		}

		PianoXmlStructParser (ezxml_child (value, "struct"),
				PianoXmlParseStationsCb, bag->station);
	} else if (strcmp ("seedId", key) == 0) {
		char *valueStr = PianoXmlGetNodeText (value);
		bag->seedId = strdup (valueStr);
	}
}

/*	parse getStation xml struct
 */
static void PianoXmlParseGetStationInfoCb (const char *key, const ezxml_t value,
		void *data) {
	PianoStationInfo_t *info = data;

	if (strcmp ("seeds", key) == 0) {
		const ezxml_t dataNode = ezxml_get (value, "array", 0, "data", -1);
		for (ezxml_t seedNode = ezxml_child (dataNode, "value"); seedNode;
					seedNode = seedNode->next) {
			struct PianoXmlParseSeedBag bag;
			memset (&bag, 0, sizeof (bag));

			PianoXmlStructParser (ezxml_child (seedNode, "struct"),
					PianoXmlParseSeedCb, &bag);

			assert (bag.song != NULL || bag.artist != NULL ||
					bag.station != NULL);

			if (bag.seedId == NULL) {
				/* seeds without id are useless */
				continue;
			}

			/* FIXME: copy&waste */
			if (bag.song != NULL) {
				bag.song->seedId = bag.seedId;

				if (info->songSeeds == NULL) {
					info->songSeeds = bag.song;
				} else {
					PianoSong_t *curSong = info->songSeeds;
					while (curSong->next != NULL) {
						curSong = curSong->next;
					}
					curSong->next = bag.song;
				}
			} else if (bag.artist != NULL) {
				bag.artist->seedId = bag.seedId;

				if (info->artistSeeds == NULL) {
					info->artistSeeds = bag.artist;
				} else {
					PianoArtist_t *curSong = info->artistSeeds;
					while (curSong->next != NULL) {
						curSong = curSong->next;
					}
					curSong->next = bag.artist;
				}
			} else if (bag.station != NULL) {
				bag.station->seedId = bag.seedId;

				if (info->stationSeeds == NULL) {
					info->stationSeeds = bag.station;
				} else {
					PianoStation_t *curStation = info->stationSeeds;
					while (curStation->next != NULL) {
						curStation = curStation->next;
					}
					curStation->next = bag.station;
				}
			} else {
				free (bag.seedId);
			}
		}
	} else if (strcmp ("feedback", key) == 0) {
		const ezxml_t dataNode = ezxml_get (value, "array", 0, "data", -1);
		for (ezxml_t feedbackNode = ezxml_child (dataNode, "value"); feedbackNode;
					feedbackNode = feedbackNode->next) {
			if (PianoXmlParsePlaylistStruct (feedbackNode, &info->feedback) !=
					PIANO_RET_OK) {
				break;
			}
		}
	}
}

/*	parse getStation response
 */
PianoReturn_t PianoXmlParseGetStationInfo (char *xml,
		PianoStationInfo_t *stationInfo) {
	ezxml_t xmlDoc, dataNode;
	PianoReturn_t ret;

	if ((ret = PianoXmlInitDoc (xml, &xmlDoc)) != PIANO_RET_OK) {
		return ret;
	}
	
	dataNode = ezxml_get (xmlDoc, "params", 0, "param", 0, "value", 0, "struct", -1);
	PianoXmlStructParser (dataNode, PianoXmlParseGetStationInfoCb, stationInfo);

	ezxml_free (xmlDoc);

	return PIANO_RET_OK;
}

