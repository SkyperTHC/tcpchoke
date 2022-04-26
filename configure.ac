dnl Process this File with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([tcpchoke],[1.0.0])
AC_CONFIG_AUX_DIR(config)
AC_CANONICAL_TARGET

dnl we use automake
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER(config.h)
AM_PROG_AR

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

dnl
dnl Use these compiler flags if we have gcc.
dnl
if test $ac_cv_c_compiler_gnu = yes; then
  CCOPTS='-O2 -Wall'
  CFLAGS="$CCOPTS $CFLAGS"
fi
test "x$prefix" != "xNONE" || prefix="/usr/local"
test "x$exec_prefix" != "xNONE" || exec_prefix="${prefix}"
trydir_i="${prefix}/include /usr/include"
trydir_l="${prefix}/lib /usr/lib"

if test "${prefix}" != "/usr/local" ; then
	trydir_i="${trydir_i} /usr/local/include"
	trydir_l="${trydir_l} /usr/local/lib"
fi

dnl Try include paths (especially on OSX)
trydir_i="${trydir_i} /usr/local/opt/openssl/include"
for xincdir in $trydir_i ; do
	if test ! -d "$xincdir" ; then
		continue;
	fi
	if test x"${INCLUDES}" = x; then
		INCLUDES="-I${xincdir}";
	else
		INCLUDES="$INCLUDES -I${xincdir}";
	fi
done
CPPFLAGS="-I${srcdir}/../include ${INCLUDES} $CPPFLAGS"

dnl Try library paths...
trydir_l="${trydir_l} /usr/local/opt/openssl/lib"
for xlibdir in $trydir_l ; do
	if test ! -d "$xlibdir" ; then
		continue;
	fi
	if test -f "${xlibdir}/libssl.a"; then
		STATIC_LIBSSLDIR="${xlibdir}"
	fi
	if test x"${LIBDIR}" = x; then
		LIBDIR="-L${xlibdir}";
	else
		LIBDIR="$LIBDIR -L${xlibdir}";
	fi
done
LDFLAGS="${LIBDIR} $LDFLAGS"

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(sys/time.h sys/endian.h unistd.h fnmatch.h string.h utmp.h utmpx.h pty.h openssl/srp.h util.h libutil.h netinet/in_systm.h sys/loadavg.h libproc.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_FUNC_MEMCMP

dnl If size_t is not defined, define size_t to be unsigned. 
AC_TYPE_SIZE_T 
dnl If uid_t is not defined, define uid_t to be int and gid_t to be int.
AC_TYPE_UID_T

AC_ARG_ENABLE(static,
[  --enable-static          Compile static binary],
	[STATIC="yes"], [STATIC="no"]
)

AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(nsl, gethostbyname)
AC_CHECK_LIB(event, event_base_new)

AC_CHECK_FUNCS(gettimeofday memcpy strchr strlcat forkpty openpty getline stat64 open64 statvfs64 accept4 connectx)

AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug], [Enable debug information.]),
	[debug=true AC_DEFINE(DEBUG, 1, [Debug infos])]
)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo "
${PACKAGE_NAME}-${PACKAGE_VERSION} has been configured:

 Host..............: ${host}
 Compiler..........: ${CC}
 Compiler flags....: ${CFLAGS_STATIC}${CFLAGS}
 Preprocessor flags: ${CPPFLAGS}
 Linker flags......: ${LDFLAGS}
 Libraries.........: ${LIBS}

Configuration complete. Now type: make all install"

