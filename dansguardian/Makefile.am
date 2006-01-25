MAINTAINERCLEANFILES = Makefile.in
SUBDIRS= doc . data configs src
EXTRA_DIST = autogen.sh

install-data-local:
	if ! test -d "$(DESTDIR)$(DGLOGLOCATION)"; then \
		$(mkinstalldirs) $(DESTDIR)$(DGLOGLOCATION) && \
		chown -R $(DGPROXYUSER):$(DGPROXYGROUP) $(DESTDIR)$(DGLOGLOCATION); \
	fi
