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

#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "const.h"
#include "main.h"
#include "piano.h"
#include "http.h"
#include "xml.h"

char *PianoEncryptString (char *strInput);

/*	initialize piano handle, set up curl handle and settings
 *	@author PromyLOPh
 *	@added 2008-06-05
 *	@param piano handle
 *	@return nothing
 */
void PianoInit (PianoHandle_t *ph) {
	memset (ph, 0, sizeof (*ph));
	ph->curlHandle = curl_easy_init ();
	/* FIXME: 64-bit may make this hack useless */
	snprintf (ph->routeId, sizeof (ph->routeId), "%07liP", time (NULL)>>8);
	/* at the moment we don't need publicity */
	curl_easy_setopt (ph->curlHandle, CURLOPT_USERAGENT, PIANO_USERAGENT);
	/* set tor as control connection proxy */
	curl_easy_setopt (ph->curlHandle, CURLOPT_PROXY, "localhost:9050");
	curl_easy_setopt (ph->curlHandle, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4A);
	curl_easy_setopt (ph->curlHandle, CURLOPT_CONNECTTIMEOUT, 60);
}

/*	free complete station list
 *	@author PromyLOPh
 *	@added 2008-06-09
 *	@param piano handle
 */
void PianoDestroyStations (PianoHandle_t *ph) {
	PianoStation_t *curStation, *lastStation;

	curStation = ph->stations;
	while (curStation != NULL) {
		free (curStation->name);
		free (curStation->id);
		lastStation = curStation;
		curStation = curStation->next;
		memset (lastStation, 0, sizeof (*lastStation));
		free (lastStation);
	}
	ph->stations = NULL;
}

/* FIXME: copy & waste */
/*	free _all_ elements of playlist
 *	@author PromyLOPh
 *	@added 2008-06-09
 *	@param piano handle
 *	@return nothing
 */
void PianoDestroyPlaylist (PianoHandle_t *ph) {
	PianoSong_t *curSong, *lastSong;

	curSong = ph->playlist;
	while (curSong != NULL) {
		free (curSong->audioUrl);
		free (curSong->artist);
		free (curSong->focusTraitId);
		free (curSong->matchingSeed);
		free (curSong->musicId);
		free (curSong->title);
		free (curSong->userSeed);
		lastSong = curSong;
		curSong = curSong->next;
		memset (lastSong, 0, sizeof (*lastSong));
		free (lastSong);
	}
	ph->playlist = NULL;
}

/*	frees the whole piano handle structure
 *	@author PromyLOPh
 *	@added 2008-06-05
 *	@param piano handle
 *	@return nothing
 */
void PianoDestroy (PianoHandle_t *ph) {
	curl_easy_cleanup (ph->curlHandle);
	/* FIXME: only free if pointer != NULL */
	free (ph->user.webAuthToken);
	free (ph->user.authToken);
	free (ph->user.listenerId);

	PianoDestroyStations (ph);
	PianoDestroyPlaylist (ph);
	memset (ph, 0, sizeof (*ph));
}

/*	authenticates user
 *	@author PromyLOPh
 *	@added 2008-06-05
 *	@param piano handle
 *	@param username (utf-8 encoded)
 *	@param password (plaintext, utf-8 encoded)
 *	@return nothing
 */
void PianoConnect (PianoHandle_t *ph, char *user, char *password) {
	/* sync */
	char url[PIANO_URL_BUFFER_SIZE];
	char *requestStr = PianoEncryptString ("<?xml version=\"1.0\"?>"
			"<methodCall><methodName>misc.sync</methodName>"
			"<params></params></methodCall>");
	char *retStr, requestStrPlain[10000];

	/* sync (is the return value used by pandora? for now: ignore result) */
	snprintf (url, sizeof (url), PIANO_RPC_URL "rid=%s&method=sync",
			ph->routeId);
	PianoHttpPost (ph->curlHandle, url, requestStr, &retStr);
	free (requestStr);
	free (retStr);

	/* authenticate */
	snprintf (requestStrPlain, sizeof (requestStrPlain), 
			"<?xml version=\"1.0\"?><methodCall>"
			"<methodName>listener.authenticateListener</methodName>"
			"<params><param><value><int>%li</int></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"</params></methodCall>", time (NULL), user, password);
	requestStr = PianoEncryptString (requestStrPlain);
	snprintf (url, sizeof (url), PIANO_SECURE_RPC_URL "rid=%s"
			"&method=authenticateListener", ph->routeId);
	PianoHttpPost (ph->curlHandle, url, requestStr, &retStr);
	PianoXmlParseUserinfo (ph, retStr);

	free (requestStr);
	free (retStr);
}

/*	get all stations for authenticated user (so: PianoConnect needs to
 *	be run before)
 *	@author PromyLOPh
 *	@added 2008-06-05
 *	@param piano handle filled with some authentication data by PianoConnect
 *	@return nothing
 */
void PianoGetStations (PianoHandle_t *ph) {
	char xmlSendBuf[10000], url[PIANO_URL_BUFFER_SIZE];
	char *requestStr, *retStr;

	snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
			"<methodCall><methodName>station.getStations</methodName>"
			"<params><param><value><int>%li</int></value></param>"
			"<param><value><string>%s</string></value></param>"
			"</params></methodCall>", time (NULL), ph->user.authToken);
	requestStr = PianoEncryptString (xmlSendBuf);
	snprintf (url, sizeof (url), PIANO_RPC_URL
			"rid=%s&lid=%s&method=getStations", ph->routeId,
			ph->user.listenerId);
	PianoHttpPost (ph->curlHandle, url, requestStr, &retStr);
	PianoXmlParseStations (ph, retStr);
	free (retStr);
	free (requestStr);
}

/*	get next songs for station (usually four tracks)
 *	@author PromyLOPh
 *	@added 2008-06-05
 *	@param piano handle
 *	@param station id
 *	@return nothing yet
 */
void PianoGetPlaylist (PianoHandle_t *ph, char *stationId) {
	char xmlSendBuf[10000], url[PIANO_URL_BUFFER_SIZE];
	char *requestStr, *retStr;

	/* FIXME: remove static numbers */
	snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
			"<methodCall><methodName>playlist.getFragment</methodName>"
			"<params><param><value><int>%li</int></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>15941546</string></value></param>"
			"<param><value><string>181840822</string></value></param>"
			"<param><value><string></string></value></param>"
			"<param><value><string></string></value></param>"
			"<param><value><string>aacplus</string></value></param>"
			"</params></methodCall>", time (NULL), ph->user.authToken,
			stationId);
	requestStr = PianoEncryptString (xmlSendBuf);
	snprintf (url, sizeof (url), PIANO_RPC_URL
			"rid=%s&lid=%s&method=getFragment&arg1=%s&arg2=15941546"
			"&arg3=181840822&arg4=&arg5=&arg6=aacplus", ph->routeId,
			ph->user.listenerId, stationId);
	PianoHttpPost (ph->curlHandle, url, requestStr, &retStr);
	PianoXmlParsePlaylist (ph, retStr);
	free (retStr);
	free (requestStr);
}

/*	love or ban track (you cannot remove your rating, so PIANO_RATE_NONE is
 *	not allowed)
 *	@author PromyLOPh
 *	@added 2008-06-10
 *	@public yes
 *	@param piano handle
 *	@param track will be added to this stations loved tracks list
 *	@param rate this track
 *	@param your rating
 *	@return value from return enum
 */
PianoReturn_t PianoRateTrack (PianoHandle_t *ph, PianoStation_t *station,
		PianoSong_t *song, PianoSongRating_t rating) {
	char xmlSendBuf[10000], url[PIANO_URL_BUFFER_SIZE];
	char *requestStr, *retStr;
	PianoReturn_t ret = PIANO_RET_ERR;

	snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
			"<methodCall><methodName>station.addFeedback</methodName>"
			"<params><param><value><int>%li</int></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><boolean>%i</boolean></value></param>"
			"<param><value><boolean>0</boolean></value></param>"
			"</params></methodCall>", time (NULL), ph->user.authToken,
			station->id, song->musicId, song->matchingSeed, song->userSeed,
			/* sometimes focusTraitId is not set, dunno why yet */
			(song->focusTraitId == NULL) ? "" : song->focusTraitId,
			(rating == PIANO_RATE_LOVE) ? 1 : 0);
	requestStr = PianoEncryptString (xmlSendBuf);
	snprintf (url, sizeof (url), PIANO_RPC_URL
			"rid=%s&lid=%s&method=addFeedback&arg1=%s&arg2=%s"
			"&arg3=%s&arg4=%s&arg5=%s&arg6=%s&arg7=false", ph->routeId,
			ph->user.listenerId, station->id, song->musicId,
			song->matchingSeed, song->userSeed,
			(song->focusTraitId == NULL) ? "" : song->focusTraitId,
			(rating == PIANO_RATE_LOVE) ? "true" : "false");
	PianoHttpPost (ph->curlHandle, url, requestStr, &retStr);
	ret = PianoXmlParseRate (retStr);
	free (requestStr);
	free (retStr);

	return ret;
}

PianoReturn_t PianoRenameStation (PianoHandle_t *ph, PianoStation_t *station,
		char *newName) {
	char xmlSendBuf[10000], url[PIANO_URL_BUFFER_SIZE];
	char *requestStr, *retStr, *urlencodedNewName;
	PianoReturn_t ret = PIANO_RET_ERR;

	snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
			"<methodCall><methodName>station.setStationName</methodName>"
			"<params><param><value><int>%li</int></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"<param><value><string>%s</string></value></param>"
			"</params></methodCall>", time (NULL), ph->user.authToken,
			station->id, /* FIXME: xml-encode this */ newName);
	requestStr = PianoEncryptString (xmlSendBuf);

	urlencodedNewName = curl_easy_escape (ph->curlHandle, newName, 0);
	snprintf (url, sizeof (url), PIANO_RPC_URL "rid=%s&lid=%s"
			"&method=setStationName&arg1=%s&arg2=%s", ph->routeId,
			ph->user.listenerId, station->id, urlencodedNewName);
	PianoHttpPost (ph->curlHandle, url, requestStr, &retStr);
	ret = PianoXmlParseSimple (retStr);
	
	curl_free (urlencodedNewName);
	free (requestStr);
	free (retStr);

	return ret;
}
