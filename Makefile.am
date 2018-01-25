## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src tests

dist_doc_DATA = \
	README \
	COPYING \
	AUTHORS \
	ChangeLog \
	INSTALL \
	NEWS \
	HACKING



# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(docdir)

@DX_RULES@

doc: doxygen-run

MOSTLYCLEANFILES = $(DX_CLEANFILES)
