MAINTAINERCLEANFILES = Makefile.in \
		       default.conf fancy.conf

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/downloadmanagers

SUBDIRS = .

FLISTS = default.conf fancy.conf

extra_DIST = default.conf.in fancy.conf.in

install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(DGDATADIR) && \
	for l in $(FLISTS) ; do \
		echo "$(INSTALL_DATA) $$l $(DESTDIR)$(DGDATADIR)/$$l"; \
		$(INSTALL_DATA) $$l $(DESTDIR)$(DGDATADIR)/$$l; \
	done


uninstall-local:
	for l in $(FLISTS) ; do \
		rm -f $(DESTDIR)$(DGDATADIR)/$$l ; \
	done
