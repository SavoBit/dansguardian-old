MAINTAINERCLEANFILES = Makefile.in \
                       dansguardian.conf dansguardianf1.conf

SUBDIRS = lists downloadmanagers contentscanners .

DATATOPDIR = $(sysconfdir)/$(PACKAGE_NAME)

FLISTS = dansguardian.conf dansguardianf1.conf

EXTRA_DIST = dansguardian.conf.in dansguardianf1.conf.in

install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(DATATOPDIR) && \
	for l in $(FLISTS) ; do \
		echo "$(INSTALL_DATA) $$l $(DESTDIR)$(DATATOPDIR)/$$l"; \
		$(INSTALL_DATA) $$l $(DESTDIR)$(DATATOPDIR)/$$l; \
	done

uninstall-local:
	for l in $(FLISTS) ; do \
		rm -f $(DESTDIR)$(DATATOPDIR)/$$l ; \
	done

