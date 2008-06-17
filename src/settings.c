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
#include <stdlib.h>
#include <stdio.h>

#include "settings.h"
#include "config.h"

/*	tries to guess your config dir; somehow conforming to
 *	http://standards.freedesktop.org/basedir-spec/basedir-spec-0.6.html
 *	@param name of the config file (can contain subdirs too)
 *	@param store the whole path here
 *	@param but only up to this size
 *	@return nothing
 */
void getXdgConfigDir (char *filename, char *retDir, size_t retDirN) {
	char *xdgConfigDir = NULL;

	if ((xdgConfigDir = getenv ("XDG_CONFIG_HOME")) != NULL &&
			strlen (xdgConfigDir) > 0) {
		/* special dir: $xdg_config_home */
		snprintf (retDir, retDirN, "%s/%s", xdgConfigDir, filename);
	} else {
		if ((xdgConfigDir = getenv ("HOME")) != NULL &&
				strlen (xdgConfigDir) > 0) {
			/* standard config dir: $home/.config */
			snprintf (retDir, retDirN, "%s/.config/%s", xdgConfigDir,
					filename);
		} else {
			/* fallback: working dir */
			snprintf (retDir, retDirN, "%s", filename);
		}
	}
}

void BarSettingsInit (BarSettings_t *settings) {
	memset (settings, 0, sizeof (*settings));
}

void BarSettingsDestroy (BarSettings_t *settings) {
	free (settings->controlProxy);
	free (settings->username);
	free (settings->password);
}

/*	read app settings from file; format is: key = value\n
 *	@param where to save these settings
 *	@return nothing yet
 */
void readSettings (BarSettings_t *settings) {
	char configfile[1024], key[256], val[256];
	FILE *configfd;

	getXdgConfigDir (PACKAGE "/config", configfile, sizeof (configfile));
	if ((configfd = fopen (configfile, "r")) == NULL) {
		printf ("config file at %s not found\n", configfile);
		return;
	}

	while (!feof (configfd)) {
		memset (val, 0, sizeof (*val));
		memset (key, 0, sizeof (*key));
		if (fscanf (configfd, "%255s = %255[^\n]", key, val) < 2) {
			/* invalid config line */
			continue;
		}
		if (strcmp ("control_proxy", key) == 0) {
			settings->controlProxy = strdup (val);
		} else if (strcmp ("control_proxy_type", key) == 0) {
			if (strcmp ("http", val) == 0) {
				settings->controlProxyType = CURLPROXY_HTTP;
			} else if (strcmp ("socks4", val) == 0) {
				settings->controlProxyType = CURLPROXY_SOCKS4;
			} else if (strcmp ("socks4a", val) == 0) {
				settings->controlProxyType = CURLPROXY_SOCKS4A;
			} else if (strcmp ("socks5", val) == 0) {
				settings->controlProxyType = CURLPROXY_SOCKS5;
			} else {
				/* error: don't use proxy at all */
				settings->controlProxyType = -1;
			}
		} else if (strcmp ("user", key) == 0) {
			settings->username = strdup (val);
		} else if (strcmp ("password", key) == 0) {
			settings->password = strdup (val);
		}
	}

	fclose (configfd);
}
