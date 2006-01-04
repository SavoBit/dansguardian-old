# Autoconf script
# Inspired by a script provided by Alex Ott
# 2004,2005 - Daniel Barron

AC_PREREQ(2.57)
AC_INIT(dansguardian, 2.9.3.3)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([dgconfig.h])

AC_CACHE_LOAD

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_CACHE_SAVE

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h])
AC_CHECK_HEADERS([string.h sys/socket.h sys/time.h syslog.h unistd.h locale.h])
AC_CHECK_HEADERS([sys/types.h sys/un.h sys/poll.h sys/resource.h])
AC_CHECK_HEADERS([pwd.h grp.h wait.h])
AC_CHECK_HEADERS([byteswap.h])

# check for endian.h
AC_CHECK_HEADER([endian.h],
  [AC_DEFINE(HAVE_ENDIAN_H, [], [Define if endian.h exists])],
  [AC_CHECK_HEADER([sys/endian.h],
    [AC_DEFINE(HAVE_SYS_ENDIAN_H, [], [Define if sys/endian.h exists])],
    [AC_CHECK_HEADER([machine/endian.h],
      [AC_DEFINE(HAVE_MACHINE_ENDIAN_H, [], [Define if machine/endian.h exists])],
      [AC_MSG_ERROR([no endian.h found! (or sys/endian.h, or machine/endian.h)])]
    )]
  )]
)
  

# check for zlib
AC_MSG_CHECKING(if zlib should be statically linked)
AC_ARG_ENABLE(static-zlib,
[  --enable-static-zlib=[[no]]  Enable static linking of zlib],
[ if test "x$enableval" = "xyes"; then
	staticzlib=true
	AC_MSG_RESULT(yes)
else
	staticzlib=false
	AC_MSG_RESULT(no)
fi
],
[
staticzlib=false
AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for zlib)
AC_ARG_WITH(zlib,
	[  --with-zlib=[[/path/to/zlib/]]  non-standard search path for zlib library],
	[ # check for header & func (in library) in given prefix
	CPPFLAGS="${CPPFLAGS} -I${withval}/include"
	if ! test "x$staticzlib" = "xtrue"; then
		LDFLAGS="${LDFLAGS} -L${withval}/lib"
	fi
	])
AC_CHECK_HEADERS([zlib.h])
if ! test "x$staticzlib" = "xtrue"; then
	AC_CHECK_LIB(z, gzdopen, [AC_MSG_RESULT(yes)], [AC_MSG_ERROR([no zlib!])])
	LDFLAGS="${LDFLAGS} -lz"
else
	AC_SUBST(ZLIBSOURCE, "${withval}/lib/libz.a")
fi
AM_CONDITIONAL(STATIC_ZLIB, test "x$staticzlib" = "xtrue")

# check whether accept() wants an int* or a socklen_t* as third argument
AC_MSG_CHECKING([type of 3rd argument to accept()])
AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
int accept (int, struct sockaddr *, int *);
], [],[
	AC_MSG_RESULT([int*])
	AC_DEFINE_UNQUOTED([ACCEPT_TYPE],[int], [type for 3rd argument to accept()])
],[
	AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
int accept (int, struct sockaddr *, socklen_t *);
	],[],[
		AC_MSG_RESULT([socklen_t*])
		AC_DEFINE_UNQUOTED([ACCEPT_TYPE],[socklen_t], [type for 3rd argument to accept()])
    ], [
		AC_MSG_RESULT([size_t*])
		AC_DEFINE_UNQUOTED([ACCEPT_TYPE],[size_t], [type for 3rd argument to accept()])
    ])
])

AC_CACHE_SAVE

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_MALLOC
AC_CHECK_FUNCS([dup2 gethostbyaddr gettimeofday inet_ntoa memset select])
AC_CHECK_FUNCS([socket strerror strstr strtol])
AC_CHECK_FUNCS([setuid setgid umask seteuid setreuid setlocale strdup])

AC_CACHE_SAVE

AC_MSG_CHECKING(for backside with both hands)
AC_MSG_RESULT(no)
AC_MSG_CHECKING(build os)
AC_CANONICAL_BUILD
AC_MSG_RESULT($build_os)

dnl Darwin bits are needed as early versions don't define _BSD_SOCKLEN_T_ and
dnl a slightly different version of accept().
dnl The BSD bits are due to using a slightly different connect() but
dnl they could probably be replaced with a test for struct sockaddr_un.sun_len
dnl instead.

AC_MSG_CHECKING(for OS tweaks needed)
case $build_os in
  darwin*)
    AC_DEFINE(HAVE_DARWIN, 1, [Define if might need tweaks for Darwin versions])
    AC_DEFINE(HAVE_BSD, 1, [Define if might need tweaks for xBSD versions])
    AC_MSG_RESULT(yes)
  ;;
  hp-ux*)
    AC_DEFINE(HAVE_BSD, 1, [Define if might need tweaks for xBSD versions])
    AC_MSG_RESULT(yes)
  ;;
  openbsd*)
    AC_DEFINE(HAVE_BSD, 1, [Define if might need tweaks for xBSD versions])
    AC_MSG_RESULT(yes)
  ;;
  freebsd*)
    AC_DEFINE(HAVE_BSD, 1, [Define if might need tweaks for xBSD versions])
    AC_MSG_RESULT(yes)
  ;;
  netbsd*)
    AC_DEFINE(HAVE_BSD, 1, [Define if might need tweaks for xBSD versions])
    AC_MSG_RESULT(yes)
  ;;
  *)
    AC_MSG_RESULT(no)
  ;;
esac

AC_MSG_CHECKING(for debug option)
AC_ARG_WITH(dgdebug,
[  --with-dgdebug=[[off]]    switch on debug build mode],
[if test "${withval}" == "on" ; then
  AC_MSG_RESULT(yes)
  dgdebug=${withval}
  AC_DEFINE(HAVE_DGDEBUG, 1, [Define to enable debug build mode])
else
  AC_MSG_RESULT(no)
fi], [
  # disable by default
  AC_MSG_RESULT(no)
])


AC_MSG_CHECKING(for proxy user)
AC_ARG_WITH(proxyuser,
[  --with-proxyuser=[[nobody]]       name of proxy user],
[if test "${withval}" != "" ; then
  AC_MSG_RESULT(yes)
  proxyuser=${withval}
else
  AC_MSG_RESULT(no)
  proxyuser=nobody
fi], [
  # disable by default
  AC_MSG_RESULT(no)
  proxyuser=nobody
])
AC_SUBST(DGPROXYUSER, "$proxyuser")

AC_MSG_CHECKING(for proxy group)
AC_ARG_WITH(proxygroup,
[  --with-proxygroup=[[nobody]]      name of proxy group],
[if test "${withval}" != "" ; then
  AC_MSG_RESULT(yes)
  proxygroup=${withval}
else
  AC_MSG_RESULT(no)
  proxygroup=nobody
fi], [
  # disable by default
  AC_MSG_RESULT(no)
  proxygroup=nobody
])
AC_SUBST(DGPROXYGROUP, "$proxygroup")

AC_MSG_CHECKING(for piddir)
AC_ARG_WITH(piddir,
[  --with-piddir=[[PREFIX/LOCALSTATEDIR/run]]  path for pid file],
[if test "${withval}" != "" ; then
  AC_MSG_RESULT(yes)
  piddir=${withval}
else
  AC_MSG_RESULT(no)
  piddir="${localstatedir}/run"
fi], [
  # disable by default
  AC_MSG_RESULT(no)
  piddir="${localstatedir}/run"
])
AC_FINALIZE_VAR(DGPIDDIR,"$piddir")
AC_SUBST(DGPIDDIR)


AC_MSG_CHECKING(for logdir)
AC_ARG_WITH(logdir,
[  --with-logdir=[[PREFIX/LOCALSTATEDIR/log/dansguardian]]  path for log files],
[if test "${withval}" != "" ; then
  AC_MSG_RESULT(yes)
  logdir=${withval}
else
  AC_MSG_RESULT(no)
  logdir="${localstatedir}/log/${PACKAGE_NAME}"
fi], [
  # disable by default
  AC_MSG_RESULT(no)
  logdir="${localstatedir}/log/${PACKAGE_NAME}"
])
AC_FINALIZE_VAR(DGLOGLOCATION,"$logdir")
AC_SUBST(DGLOGLOCATION)

# asking user if they want PCRE support
AC_MSG_CHECKING(for PCRE support)
AC_ARG_ENABLE(
pcre,
[  --enable-pcre=[[no]]  Enable support for the PCRE library],
[ if test "x$enableval" = "xyes"; then
	pcre=true
	AC_MSG_RESULT(yes)
else
	pcre=false
	AC_MSG_RESULT(no)
fi
],
[ # disable by default
	pcre=false
	AC_MSG_RESULT(no)
]
)
if test "x$pcre" = "xtrue"; then
	AC_CHECK_PROG(
		      PCRE,
		      pcre-config,
		      yes,
		      no
	)
	if ! test $PCRE = "yes"; then
		AC_MSG_ERROR([pcre-config not found!])
	fi
	PCRE_LIBS=`pcre-config --libs`
	PCRE_PREFIX=`pcre-config --prefix`

	LDFLAGS="${LDFLAGS} ${PCRE_LIBS} -lpcreposix"
	CPPFLAGS="${CPPFLAGS} -I${PCRE_PREFIX}/include"

	AC_MSG_CHECKING([for REG_DOTALL in pcreposix.h])
	AC_LANG_PUSH(C)
	AC_COMPILE_IFELSE([
#include "${PCRE_PREFIX}/include/pcreposix.h"
#ifndef REG_DOTALL
	a syntax error
#endif
		], AC_MSG_RESULT(yes), AC_MSG_FAILURE([pcreposix.h does not define REG_DOTALL. Do you have PCRE version 6.0 or above?]))
	AC_LANG_POP(C)

	AC_DEFINE([HAVE_PCRE],[],[Define to enable PCRE support])

else
	AC_CHECK_FUNCS([regcomp])
fi
AM_CONDITIONAL(HAVE_PCRE, test "x$pcre" = "xtrue")

# asking user if they want clamav support
AC_MSG_CHECKING(for libclamav support)
#USE_CLAMAV=false
AC_ARG_ENABLE(
clamav,
[  --enable-clamav=[[no]]  Enable support for the libClamAV content scanner],
[ if test "x$enableval" = "xyes"; then
	dnl detecting clamav and make conditional for it
	#PKG_CHECK_MODULES(LIBCLAMAV, libclamav,
	#	[AC_MSG_RESULT(yes)
	#	USE_CLAMAV=true
	#	AC_DEFINE([HAVE_CLAMAV],[],[Define to enable ClamAV content scanner])]
	#)
	#pkg_check_modules requires pkg-config to be installed which seems an overkill
	#and extra work - maybe better to let the user choose with a configure option?
	#Thu 10th February 2005 - DB
	AC_MSG_RESULT(yes)
	AC_CHECK_PROG(
		      CLAMAV,
		      clamav-config,
		      yes,
		      no
	)
	if ! test $CLAMAV = "yes"; then
		AC_MSG_ERROR([libClamAV support enabled, but clamav-config not found!])
	fi
	CLAMAV_LIBS=`clamav-config --libs`
	CLAMAV_PREFIX=`clamav-config --prefix`

	LDFLAGS="${LDFLAGS} ${CLAMAV_LIBS} -lclamav"
	CPPFLAGS="${CPPFLAGS} -I${CLAMAV_PREFIX}/include"

	AC_DEFINE([HAVE_CLAMAV],[],[Define to enable ClamAV content scanner])

	clamav=true
else
	clamav=false
	AC_MSG_RESULT(no)
fi
],
[ #disable by default
 	clamav=false
	AC_MSG_RESULT(no)
]
)
AM_CONDITIONAL(HAVE_CLAMAV, test "x$clamav" = "xtrue")

# asking user if they want clamd support
AC_MSG_CHECKING(for clamd support)
AC_ARG_ENABLE(
clamd,
[  --enable-clamd=[[no]]  Enable support for the ClamD content scanner],
[ if test "x$enableval" = "xyes"; then
	AC_MSG_RESULT(yes)
	clamd=true
	AC_DEFINE([ENABLE_CLAMD],[],[Define to enable ClamD content scanner])
else
	AC_MSG_RESULT(no)
	clamd=false
fi],
[
	AC_MSG_RESULT(no)
	clamd=false
]
)
AM_CONDITIONAL(ENABLE_CLAMD, test "x$clamd" = "xtrue")

# asking user if they want icap support
AC_MSG_CHECKING(for icap support)
AC_ARG_ENABLE(
icap,
[  --enable-icap=[[no]]  Enable support for ICAP AV server content scanner],
[ if test "x$enableval" = "xyes"; then
	AC_MSG_RESULT(yes)
	icap=true
	AC_DEFINE([ENABLE_ICAP],[],[Define to enable ICAP content scanner])
else
	AC_MSG_RESULT(no)
	icap=false
fi],
[
	AC_MSG_RESULT(no)
	icap=false
]
)
AM_CONDITIONAL(ENABLE_ICAP, test "x$icap" = "xtrue")

# asking user if they want kavd support
AC_MSG_CHECKING(for kavd support)
AC_ARG_ENABLE(
kavd,
[  --enable-kavd=[[no]]  Enable support for the Kaspersky AV daemon content scanner],
[ if test "x$enableval" = "xyes"; then
	AC_MSG_RESULT(yes)
	kavd=true
	AC_DEFINE([ENABLE_KAVD],[],[Define to enable KAVD content scanner])
else
	AC_MSG_RESULT(no)
	kavd=false
fi],
[
	AC_MSG_RESULT(no)
	kavd=false
]
)
AM_CONDITIONAL(ENABLE_KAVD, test "x$kavd" = "xtrue")

dnl detecting kaspersky antivirus client
#KAV_PATH_KAVCLIENT
#AM_CONDITIONAL(HAVE_KAVCLIENT, test "x$kavclient_found" = "xyes")
#kav stuff is not finished Wed 09th February 2005 - DB

# asking user if they want fancy downloadmanager support
AC_MSG_CHECKING(for fancy download manager support)
AC_ARG_ENABLE(
fancydm,
[  --enable-fancydm=[[yes]]  Enable support for the fancy download manager],
[ if test "x$enableval" = "xyes"; then
	AC_MSG_RESULT(yes)
	fancydm=true
	AC_DEFINE([ENABLE_FANCYDM],[],[Define to enable fancy download manager])
else
	fancydm=false
	AC_MSG_RESULT(no)
fi],
[
	fancydm=true
	AC_MSG_RESULT(yes)
	AC_DEFINE([ENABLE_FANCYDM],[],[Define to enable fancy download manager])
]
)
AM_CONDITIONAL(ENABLE_FANCYDM, test "x$fancydm" = "xtrue")

# asking user if they want NTLM auth support
AC_MSG_CHECKING(for NTLM support)
AC_ARG_ENABLE(
ntlm,
[  --enable-ntlm=[[no]]  Enable support for the NTLM auth plugin],
[ if test "x$enableval" = "xyes"; then
	AC_MSG_RESULT(yes)
	ntlm=true
	AC_DEFINE([ENABLE_NTLM],[],[Define to enable NTLM auth plugin])
	# now need to check if they're using an iconv library, rather
	# than a native iconv implementation
	AC_MSG_CHECKING(for iconv)
	AC_ARG_WITH(libiconv,
		[  --with-libiconv=[[/path/to/iconv/]]  use this if compiling on a system which
                    requires an external iconv library (only used in
                    conjunction with NTLM auth plugin). Set to "yes"
                    if no non-standard search path is required.],
		[ # check for header & func (in library) in given prefix
		if test "x$withval" = "xyes"; then
			LDFLAGS="${LDFLAGS} -liconv"
		else
			CPPFLAGS="${CPPFLAGS} -I${withval}/include"
			LDFLAGS="${LDFLAGS} -L${withval}/lib -liconv"
		fi
		AC_CHECK_LIB(iconv, iconv, [AC_MSG_RESULT(linked)], [AC_MSG_ERROR([--with-libiconv used, but no libiconv found!])])
		],
		[ # check for native header & func (no library)
		AC_CHECK_FUNCS(iconv, [AC_MSG_RESULT(native)], [AC_MSG_ERROR([--with-libiconv not used, but no native iconv found!])])
		])
		
else
	AC_MSG_RESULT(no)
	ntlm=false
fi],
[
	AC_MSG_RESULT(no)
	ntlm=false
]
)
AM_CONDITIONAL(ENABLE_NTLM, test "x$ntlm" = "xtrue")

AC_DEFINE_UNQUOTED([DG_CONFIGURE_OPTIONS], ["$ac_configure_args"], [Record configure-time options])

libdir="${libdir}/${PACKAGE_NAME}"
AC_FINALIZE_VAR(DGLIBDIR,"${libdir}")
AC_SUBST(DGLIBDIR)
AC_FINALIZE_VAR(DGBINDIR,"${sbindir}")
AC_SUBST(DGBINDIR)
AC_FINALIZE_VAR(DGCONFDIR,"${sysconfdir}/${PACKAGE_NAME}")
AC_SUBST(DGCONFDIR)
AC_FINALIZE_VAR(DGDATADIR,"${datadir}/${PACKAGE_NAME}")
AC_SUBST(DGDATADIR)
AC_FINALIZE_VAR(DGCONFFILE,"${sysconfdir}/${PACKAGE_NAME}/${PACKAGE_NAME}.conf")
AC_SUBST(DGCONFFILE)

AC_CONFIG_FILES([Makefile
	        data/Makefile
	        data/languages/Makefile
	        data/scripts/Makefile
	        data/scripts/bsd-init
	        data/scripts/dansguardian
	        data/scripts/logrotation
	        data/scripts/solaris-init
	        data/scripts/systemv-init
	        doc/Makefile
	        configs/dansguardian.conf
	        configs/dansguardianf1.conf
	        configs/Makefile
	        configs/lists/Makefile
	        configs/lists/phraselists/Makefile
	        configs/lists/blacklists/Makefile
	        configs/lists/contentscanners/Makefile
	        configs/lists/weightedphraselist
	        configs/lists/exceptionphraselist
	        configs/lists/bannedphraselist
		configs/lists/bannedurllist
		configs/lists/bannedsitelist
	        configs/downloadmanagers/Makefile
	        configs/downloadmanagers/default.conf
	        configs/downloadmanagers/fancy.conf
	        configs/contentscanners/Makefile
	        configs/contentscanners/clamav.conf
	        configs/contentscanners/clamdscan.conf
	        configs/contentscanners/icapscan.conf
	        configs/contentscanners/kavav.conf
	        configs/contentscanners/kavdscan.conf
		configs/authplugins/Makefile
		configs/authplugins/ip.conf
		configs/lists/authplugins/Makefile
                src/Makefile
                src/platform.h
		])

AC_OUTPUT
