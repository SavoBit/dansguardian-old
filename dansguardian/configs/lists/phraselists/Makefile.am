MAINTAINERCLEANFILES    = Makefile.in

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/lists/phraselists
PHRASELISTS = chat drugadvocacy dutchbadwords frenchbadwords frenchpornography gambling \
	      germanpornography goodphrases googlesearches gore illegaldrugs intolerance \
	      italianbadwords legaldrugs personals pornography violence \
	      warezhacking weapons


install-data-local: 
	for l in $(PHRASELISTS); do \
		$(mkinstalldirs) $(DESTDIR)$(DGDATADIR)/$$l && \
		for f in $(srcdir)/$$l/weighted $(srcdir)/$$l/exception $(srcdir)/$$l/banned; do \
		   if test -f $$f ; then \
			echo "$(INSTALL_DATA) $$f $(DESTDIR)$(DGDATADIR)/$$l"; \
			$(INSTALL_DATA) $$f $(DESTDIR)$(DGDATADIR)/$$l; \
	           fi \
		done \
	done

uninstall-local:
	for l in $(PHRASELISTS); do \
		for f in $(srcdir)/$$l/weighted $(srcdir)/$$l/exception; do \
	        	rm -f $(DESTDIR)$(DGDATADIR)/$$l/`basename $$f`; \
		done \
	done

dist-hook:
	for phrase in $(PHRASELISTS); do \
	  if test "$$phrase" = .; then :; else \
	    test -d $(distdir)/$$phrase \
	    || mkdir $(distdir)/$$phrase \
	    || exit 1; \
	    cp -p $(srcdir)/$$l/weighted $(distdir)/$$phrase \
	      || exit 1; \
	  fi; \
	done		
