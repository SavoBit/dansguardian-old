MAINTAINERCLEANFILES = Makefile.in

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/lists/contentscanners

SUBDIRS = .

WLISTS = exceptionvirusmimetypelist exceptionvirusextensionlist \
         exceptionvirussitelist exceptionvirusurllist

install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(DGDATADIR) && \
	for l in $(WLISTS) ; do \
		echo "$(INSTALL_DATA) $$l $(DESTDIR)$(DGDATADIR)/$$l"; \
		$(INSTALL_DATA) $$l $(DESTDIR)$(DGDATADIR)/$$l; \
	done


uninstall-local:
	for l in $(WLISTS) ; do \
		rm -f $(DESTDIR)$(DGDATADIR)/$$l ; \
	done

dist-hook:
	for lang in $(WLISTS); do \
	  if test "$$lang" = .; then :; else \
	    test -d $(distdir)/$$lang \
	    || mkdir $(distdir)/$$lang \
	    || exit 1; \
	    cp -p  $(distdir)/$$lang \
	      || exit 1; \
	  fi; \
	done		

