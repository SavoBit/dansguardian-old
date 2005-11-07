MAINTAINERCLEANFILES = Makefile.in

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/authplugins

SUBDIRS = .

FLISTS = proxy.conf ident.conf ip.conf

EXTRA_DIST = proxy.conf ident.conf ip.conf.in

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
