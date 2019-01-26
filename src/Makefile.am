## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I .

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"

if GCC
# GCC 8.2.0 prints unused result warning even when explicitly cast to void
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89031
AM_CFLAGS = -Wall -Werror -Wno-error=unused-result
endif

lib_LTLIBRARIES = libcomcom.la


libcomcom_la_SOURCES = \
	lib.c

libcomcom_la_LDFLAGS =

libcomcom_la_LIBADD =



include_HEADERS = \
	libcomcom.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcomcom-0.1.0.pc

EXTRA_DIST = \
	libcomcom-0.0.1.pc.in
