MAINTAINERCLEANFILES = Makefile.in

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/authplugins

SUBDIRS = .

FLISTS = proxy-basic.conf ident.conf ip.conf

if ENABLE_NTLM
FLISTS += proxy-ntlm.conf
endif

EXTRA_DIST = proxy-basic.conf ident.conf ip.conf.in proxy-ntlm.conf

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
