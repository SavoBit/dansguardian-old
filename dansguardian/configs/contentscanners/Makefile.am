MAINTAINERCLEANFILES = Makefile.in \
		       clamav.conf clamdscan.conf icapscan.conf \
		       kavav.conf kavdscan.conf

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/contentscanners

SUBDIRS = .

FLISTS = clamav.conf clamdscan.conf icapscan.conf kavav.conf \
         kavdscan.conf

extra_DIST = clamav.conf.in clamdscan.conf.in icapscan.conf.in \
             kavav.conf.in kavdscan.conf.in

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
