MAINTAINERCLEANFILES = Makefile.in \
		       weightedphraselist bannedphraselist exceptionphraselist

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/lists

SUBDIRS = phraselists . contentscanners

WLISTS = bannedextensionlist bannediplist bannedmimetypelist \
	 bannedregexpurllist bannedsitelist bannedurllist banneduserlist \
	 contentregexplist exceptioniplist exceptionsitelist \
	 exceptionurllist exceptionuserlist filtergroupslist greysitelist \
	 greyurllist pics bannedphraselist weightedphraselist \
	 exceptionphraselist bannediplist exceptioniplist

EXTRA_DIST = bannedextensionlist bannediplist bannedmimetypelist \
	 bannedregexpurllist bannedsitelist bannedurllist banneduserlist \
	 contentregexplist exceptioniplist exceptionsitelist \
	 exceptionurllist exceptionuserlist filtergroupslist greysitelist \
	 greyurllist pics bannedphraselist.in weightedphraselist.in \
	 exceptionphraselist.in bannediplist exceptioniplist

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
