MAINTAINERCLEANFILES = Makefile.in \
		       weightedphraselist bannedphraselist exceptionphraselist

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/lists

SUBDIRS = phraselists . contentscanners

WLISTS = bannedextensionlist bannediplist bannedmimetypelist \
	 bannedregexpurllist bannedsitelist bannedurllist banneduserlist \
	 contentregexplist exceptioniplist exceptionsitelist \
	 exceptionurllist exceptionuserlist filtergroupslist greysitelist \
	 greyurllist pics bannedphraselist \
	 exceptionphraselist bannediplist exceptioniplist

extra_DIST = weightedphraselist.in bannedphraselist.in	\
	exceptionphraselist.in

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

