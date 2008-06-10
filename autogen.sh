#!/bin/sh

libtoolize --force \
&& aclocal \
&& automake --add-missing \
&& autoconf \
&& ./configure
