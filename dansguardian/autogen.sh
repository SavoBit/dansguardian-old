#! /bin/sh
aclocal && libtoolize --force --copy && aclocal && autoheader && automake --add-missing --copy && autoconf
