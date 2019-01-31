dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

AC_INIT(libcomcom, 0.1.2)

AC_CONFIG_MACRO_DIR([.])

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11])

AM_SILENT_RULES([yes])

AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC
AC_SUBST(GCC)
AM_CONDITIONAL([GCC], [test xGCC != ''])

LT_INIT

AC_CHECK_DECLS([execvpe], [], [], [[#include <unistd.h>]])

PKG_CHECK_MODULES([CHECK], [check >= 0.10], [], [])

DX_INIT_DOXYGEN([LibComCom], [Doxyfile], [doc])


AC_OUTPUT([
Makefile
src/libcomcom-0.1.0.pc
src/Makefile
tests/Makefile
])
