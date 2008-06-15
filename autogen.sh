#!/bin/sh

for DIR in . libpiano; do
	cd $DIR
	libtoolize --force \
	&& aclocal \
	&& automake --add-missing \
	&& autoconf
done
