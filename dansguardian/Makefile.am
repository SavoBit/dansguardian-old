MAINTAINERCLEANFILES = Makefile.in
SUBDIRS= doc . data configs src

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(DGLOGLOCATION) && \
	chown -R $(DGPROXYUSER):$(DGPROXYGROUP) $(DESTDIR)$(DGLOGLOCATION)
