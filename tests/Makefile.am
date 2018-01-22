## Process this file with automake to produce Makefile.in

AM_TESTS_ENVIRONMENT = \
    export CK_FORK=no;
TESTS = test_comcom
check_PROGRAMS = test_comcom
test_comcom_SOURCES = test_comcom.c
test_comcom_CFLAGS = @CHECK_CFLAGS@ -I$(top_builddir)/src
test_comcom_LDADD = $(top_builddir)/src/libcomcom.la @CHECK_LIBS@
