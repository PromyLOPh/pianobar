#!/bin/sh

for DIR in . libpiano libwardrobe; do
	echo "autogen in " $DIR
	cd $DIR
	libtoolize --force \
	&& aclocal \
	&& autoheader \
	&& automake --add-missing \
	&& autoconf
	cd -
done
