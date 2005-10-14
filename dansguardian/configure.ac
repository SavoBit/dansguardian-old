# Autoconf script
# Inspired by a script provided by Alex Ott
# 2004,2005 - Daniel Barron

AC_PREREQ(2.57)
AC_INIT(dansguardian, 2.9.0.1)
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
AC_CHECK_HEADERS([endian.h])

# check for zlib
AC_CHECK_HEADERS([zlib.h])
AC_CHECK_LIB(z, gzdopen, [ZLIB="-lz"])
AC_SUBST(ZLIB)

AC_CACHE_SAVE

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([socklen_t])
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_MALLOC
AC_CHECK_FUNCS([dup2 gethostbyaddr gettimeofday inet_ntoa memset regcomp select])
AC_CHECK_FUNCS([socket strerror strstr strtol])
AC_CHECK_FUNCS([setuid setgid umask seteuid setreuid setlocale strdup])

AC_CACHE_SAVE

AC_MSG_CHECKING(for backside with both hands)
AC_MSG_RESULT(no)
AC_MSG_CHECKING(build os)
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
	LIBCLAMAV_LIBS="$CLAMAV_LIBS -lclamav"
	LIBCLAMAV_CFLAGS="-I${CLAMAV_PREFIX}/include"
	AC_DEFINE([HAVE_CLAMAV],[],[Define to enable ClamAV content scanner])
	AC_SUBST([LIBCLAMAV_LIBS])
	AC_SUBST([LIBCLAMAV_CFLAGS])
	clamav=true
	AC_MSG_RESULT(yes)
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

dnl detecting kaspersky antivirus client
#KAV_PATH_KAVCLIENT
#AM_CONDITIONAL(HAVE_KAVCLIENT, test "x$kavclient_found" = "xyes")
#kav stuff is not finished Wed 09th February 2005 - DB

AC_DEFINE_UNQUOTED([DG_CONFIGURE_OPTIONS], ["$ac_configure_args"], [Record configure-time options])

libdir="${libdir}/${PACKAGE_NAME}"
AC_FINALIZE_VAR(DGLIBDIR,"${libdir}")
AC_SUBST(DGLIBDIR)
AC_FINALIZE_VAR(DGBINDIR,"${bindir}")
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
	        configs/lists/contentscanners/Makefile
	        configs/lists/weightedphraselist
	        configs/lists/exceptionphraselist
	        configs/lists/bannedphraselist
	        configs/downloadmanagers/Makefile
	        configs/downloadmanagers/default.conf
	        configs/downloadmanagers/fancy.conf
	        configs/contentscanners/Makefile
	        configs/contentscanners/clamav.conf
	        configs/contentscanners/clamdscan.conf
	        configs/contentscanners/icapscan.conf
	        configs/contentscanners/kavav.conf
	        configs/contentscanners/kavdscan.conf
                src/Makefile
                src/platform.h
		])

AC_OUTPUT
