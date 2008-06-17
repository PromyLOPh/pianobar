#!/bin/sh

for DIR in . libpiano; do
	cd $DIR
	libtoolize --force \
	&& aclocal \
	&& autoheader \
	&& automake --add-missing \
	&& autoconf
done
