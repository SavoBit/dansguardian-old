# Autoconf script
# Inspired by a script provided by Alex Ott
# 2004,2005 - Daniel Barron

AC_PREREQ(2.57)
AC_INIT(dansguardian, 2.9.6.2)
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
      # hmm... no endian.h. try this instead.
      [AC_MSG_WARN([no endian.h found! (or sys/endian.h, or machine/endian.h)])
      AC_C_BIGENDIAN()]
    )]
  )]
)
  

# check for zlib
AC_MSG_CHECKING(if zlib should be statically linked)
AC_ARG_ENABLE(static-zlib,
[AC_HELP_STRING([--enable-static-zlib@<:@=no@:>@], [Enable static linking of zlib])],
[ if test "x$enableval" = "xno"; then
	staticzlib=false
	AC_MSG_RESULT(no)
else
	staticzlib=true
	AC_MSG_RESULT(yes)
fi
],
[
staticzlib=false
AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for zlib)
AC_ARG_WITH(zlib,
	[AC_HELP_STRING([--with-zlib@<:@=NONE@:>@], [non-standard search path for zlib library])],
	[ # check for header & func (in library) in given prefix
	CPPFLAGS="${CPPFLAGS} -I${withval}/include"
	if test "x$staticzlib" = "xtrue"; then
		LIBS="-Bstatic -L${withval} -lz -Bdynamic ${LIBS}"
	else
		LIBS="-L${withval}/lib -lz ${LIBS}"
	fi
	],
	[ # if --with-zlib is not supplied, we still need -lz in LIBS.
	if test "x$staticzlib" = "xtrue"; then
		LIBS="-Bstatic -lz -Bdynamic ${LIBS}"
	else
		LIBS="-lz ${LIBS}"
	fi
	])
AC_CHECK_HEADERS([zlib.h])
if ! test "x$staticzlib" = "xtrue"; then
	AC_CHECK_LIB(z, gzdopen, [AC_MSG_RESULT(yes)], [AC_MSG_ERROR([no zlib!])])
fi
#AM_CONDITIONAL(STATIC_ZLIB, test "x$staticzlib" = "xtrue")

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
[AC_HELP_STRING([--with-dgdebug@<:@=off@:>@], [switch on debug build mode])],
[if test "x${withval}" == "xoff" ; then
  AC_MSG_RESULT(no)
else
  AC_MSG_RESULT(yes)
  dgdebug=${withval}
  AC_DEFINE(HAVE_DGDEBUG, 1, [Define to enable debug build mode])
fi], [
  # disable by default
  AC_MSG_RESULT(no)
])


AC_MSG_CHECKING(for proxy user)
AC_ARG_WITH(proxyuser,
[AC_HELP_STRING([--with-proxyuser@<:@=nobody@:>@], [name of proxy user])],
[if test "x${withval}" != "x" ; then
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
[AC_HELP_STRING([--with-proxygroup@<:@=nobody@:>@], [name of proxy group])],
[if test "x${withval}" != "x" ; then
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
[AC_HELP_STRING([--with-piddir@<:@=PREFIX/LOCALSTATEDIR/run@:>@], [path for pid file])],
[if test "x${withval}" != "x" ; then
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
[AC_HELP_STRING([--with-logdir@<:@=PREFIX/LOCALSTATEDIR/log/dansguardian@:>@], [path for log files])],
[if test "x${withval}" != "x" ; then
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
[AC_HELP_STRING([--enable-pcre@<:@=no@:>@], [Enable support for the PCRE library])],
[ if test "x$enableval" = "xno"; then
	pcre=false
	AC_MSG_RESULT(no)
else
	pcre=true
	AC_MSG_RESULT(yes)
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
	PCRE_LIBS=`pcre-config --libs-posix`
	PCRE_PREFIX=`pcre-config --prefix`

	LIBS="${PCRE_LIBS} ${LIBS}"
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
AC_ARG_ENABLE(
clamav,
[AC_HELP_STRING([--enable-clamav@<:@=no@:>@], [Enable support for the libClamAV content scanner])],
[ if test "x$enableval" = "xno"; then
	clamav=false
	AC_MSG_RESULT(no)
	CLAMAVSUPPORT="!! Not compiled !! "
	CLAMAVSHM="!! Not supported on this platform !!"
else
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

	LIBS="${CLAMAV_LIBS} -lclamav ${LIBS}"
	dnl LIBS="-L${CLAMAV_PREFIX}/lib -lclamav ${LIBS}"
	CPPFLAGS="${CPPFLAGS} -I${CLAMAV_PREFIX}/include"

	AC_DEFINE([HAVE_CLAMAV],[],[Define to enable ClamAV content scanner])
	
	# check for shm_open (& shm_unlink)
	AC_CHECK_HEADERS([sys/mman.h])
	AC_CHECK_FUNCS([shm_open])
	if test "x$ac_cv_func_shm_open" = "xno"; then
		AC_CHECK_LIB(rt, shm_open, [
			AC_MSG_RESULT(yes)
			AC_DEFINE([HAVE_CLAMAV_SHM],[],[Define to enable POSIX Shared Memory support in ClamAV content scanner])
			LIBS="-lrt ${LIBS}"
			CLAMAVSHM=""
		], [
			AC_MSG_RESULT(no)
			CLAMAVSHM="!! Not supported on this platform !!"
		])
	fi

	clamav=true
	CLAMAVSUPPORT=""
fi
],
[ #disable by default
 	clamav=false
	AC_MSG_RESULT(no)
	CLAMAVSUPPORT="!! Not compiled !! "
	CLAMAVSHM="!! Not supported on this platform !!"
]
)
AM_CONDITIONAL(HAVE_CLAMAV, test "x$clamav" = "xtrue")
AC_SUBST(CLAMAVSUPPORT)
AC_SUBST(CLAMAVSHM)

# asking user if they want clamd support
AC_MSG_CHECKING(for clamd support)
AC_ARG_ENABLE(
clamd,
[AC_HELP_STRING([--enable-clamd@<:@=no@:>@], [Enable support for the ClamD content scanner])],
[ if test "x$enableval" = "xno"; then
	AC_MSG_RESULT(no)
	clamd=false
	CLAMDSUPPORT="!! Not compiled !! "
else
	AC_MSG_RESULT(yes)
	clamd=true
	CLAMDSUPPORT=""
	AC_DEFINE([ENABLE_CLAMD],[],[Define to enable ClamD content scanner])
fi],
[
	AC_MSG_RESULT(no)
	clamd=false
	CLAMDSUPPORT="!! Not compiled !! "
]
)
AM_CONDITIONAL(ENABLE_CLAMD, test "x$clamd" = "xtrue")
AC_SUBST(CLAMDSUPPORT)

# asking user if they want icap support
AC_MSG_CHECKING(for icap support)
AC_ARG_ENABLE(
icap,
[AC_HELP_STRING([--enable-icap@<:@=no@:>@], [Enable support for ICAP AV server content scanner])],
[ if test "x$enableval" = "xno"; then
	AC_MSG_RESULT(no)
	icap=false
	ICAPSUPPORT="!! Not compiled !! "
else
	AC_MSG_RESULT(yes)
	icap=true
	AC_DEFINE([ENABLE_ICAP],[],[Define to enable ICAP content scanner])
	ICAPSUPPORT=""
fi],
[
	AC_MSG_RESULT(no)
	icap=false
	ICAPSUPPORT="!! Not compiled !! "
]
)
AM_CONDITIONAL(ENABLE_ICAP, test "x$icap" = "xtrue")
AC_SUBST(ICAPSUPPORT)

# asking user if they want kavd support
AC_MSG_CHECKING(for kavd support)
AC_ARG_ENABLE(
kavd,
[AC_HELP_STRING([--enable-kavd@<:@=no@:>@], [Enable support for the Kaspersky AV daemon content scanner])],
[ if test "x$enableval" = "xno"; then
	AC_MSG_RESULT(no)
	kavd=false
	KAVDSUPPORT="!! Not compiled !! "
else
	AC_MSG_RESULT(yes)
	kavd=true
	KAVDSUPPORT=""
	AC_DEFINE([ENABLE_KAVD],[],[Define to enable KAVD content scanner])
fi],
[
	AC_MSG_RESULT(no)
	kavd=false
	KAVDSUPPORT="!! Not compiled !! "
]
)
AM_CONDITIONAL(ENABLE_KAVD, test "x$kavd" = "xtrue")
AC_SUBST(KAVDSUPPORT)

dnl detecting kaspersky antivirus client
#KAV_PATH_KAVCLIENT
#AM_CONDITIONAL(HAVE_KAVCLIENT, test "x$kavclient_found" = "xyes")
#kav stuff is not finished Wed 09th February 2005 - DB
KAVAVSUPPORT="!! Unimplemented !! "
AC_SUBST(KAVAVSUPPORT)

# asking user if they want fancy downloadmanager support
AC_MSG_CHECKING(for fancy download manager support)
AC_ARG_ENABLE(
fancydm,
[AC_HELP_STRING([--enable-fancydm@<:@=yes@:>@], [Enable support for the fancy download manager])],
[ if test "x$enableval" = "xno"; then
	fancydm=false
	FANCYSUPPORT="#!! Not compiled !! "
	AC_MSG_RESULT(no)
else
	AC_MSG_RESULT(yes)
	fancydm=true
	FANCYSUPPORT=""
	AC_DEFINE([ENABLE_FANCYDM],[],[Define to enable fancy download manager])
fi],
[
	fancydm=true
	FANCYSUPPORT=""
	AC_MSG_RESULT(yes)
	AC_DEFINE([ENABLE_FANCYDM],[],[Define to enable fancy download manager])
]
)
AM_CONDITIONAL(ENABLE_FANCYDM, test "x$fancydm" = "xtrue")
AC_SUBST(FANCYSUPPORT)

# asking user if they want NTLM auth support
AC_MSG_CHECKING(for NTLM support)
AC_ARG_ENABLE(
ntlm,
[AC_HELP_STRING([--enable-ntlm@<:@=no@:>@], [Enable support for the NTLM auth plugin])],
[ if test "x$enableval" = "xno"; then
	AC_MSG_RESULT(no)
	ntlm=false
	NTLMSUPPORT="!! Not compiled !! "
else
	AC_MSG_RESULT(yes)
	ntlm=true
	NTLMSUPPORT=""
	AC_DEFINE([ENABLE_NTLM],[],[Define to enable NTLM auth plugin])
	# now need to check if they're using an iconv library, rather
	# than a native iconv implementation
	AC_ARG_WITH(libiconv,
		[AC_HELP_STRING([--with-libiconv@<:@=NONE@:>@], [Specify search path on a system which requires an external iconv library (only used in conjunction with NTLM auth plugin).])],
		[ # check for header & func (in library) in given prefix
			if test "x$withval" != "x"; then
				CPPFLAGS="${CPPFLAGS} -I${withval}/include"
				LIBS="-L${withval}/lib -liconv ${LIBS}"
			else
				LIBS="-liconv ${LIBS}"
			fi
			AC_CHECK_LIB(iconv, iconv, [AC_MSG_RESULT(linked)], [
				AC_MSG_RESULT(no)
				AC_CHECK_LIB(iconv, libiconv, [AC_MSG_RESULT(linked)], [
					AC_MSG_ERROR([no libiconv found in given search path! (needed by NTLM plugin)])
				])
			])
		],
		[ # check for native header & func (no library)
			AC_CHECK_FUNCS(iconv, [AC_MSG_RESULT(native)], [
				AC_MSG_RESULT(no)
				# native not found, so look for linked
				LIBS="-liconv ${LIBS}"
				AC_CHECK_LIB(iconv, iconv, [
					AC_MSG_RESULT(linked)
				], [
					AC_MSG_RESULT(no)
					AC_CHECK_LIB(iconv, libiconv, [AC_MSG_RESULT(linked)], [
						AC_MSG_ERROR([no native or standard library iconv function found! (needed by NTLM plugin - try again with "--with-libiconv"?)])
					])
				])
			])
		]
	)
fi],
[
	AC_MSG_RESULT(no)
	ntlm=false
	NTLMSUPPORT="!! Not compiled !! "
]
)
AM_CONDITIONAL(ENABLE_NTLM, test "x$ntlm" = "xtrue")
AC_SUBST(NTLMSUPPORT)

AC_DEFINE_UNQUOTED([DG_CONFIGURE_OPTIONS], ["$ac_configure_args"], [Record configure-time options])

libdir="${libdir}/${PACKAGE_NAME}"

AC_ARG_WITH(sysconfsubdir,
	[AC_HELP_STRING([--with-sysconfsubdir@<:@=dansguardian@:>@], [subdirectory under sysconfdir in which to place config files])],
	[if test "x$withval" != "x"; then
		dgsysconfdir="${sysconfdir}/${withval}"
	else
		dgsysconfdir="${sysconfdir}"
	fi],
	[dgsysconfdir="${sysconfdir}/${PACKAGE_NAME}"])

AC_FINALIZE_VAR(DGLIBDIR,"${libdir}")
AC_SUBST(DGLIBDIR)
AC_FINALIZE_VAR(DGBINDIR,"${sbindir}")
AC_SUBST(DGBINDIR)
AC_FINALIZE_VAR(DGCONFDIR,"${dgsysconfdir}")
AC_SUBST(DGCONFDIR)
AC_FINALIZE_VAR(DGDATADIR,"${datadir}/${PACKAGE_NAME}")
AC_SUBST(DGDATADIR)
AC_FINALIZE_VAR(DGCONFFILE,"${dgsysconfdir}/${PACKAGE_NAME}.conf")
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
		configs/lists/downloadmanagers/Makefile
                src/Makefile
                src/platform.h
		])

AC_OUTPUT
