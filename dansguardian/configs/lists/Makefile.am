MAINTAINERCLEANFILES = Makefile.in \
		       weightedphraselist bannedphraselist exceptionphraselist \
		       bannedsitelist bannedurllist

DGDATADIR = $(sysconfdir)/$(PACKAGE_NAME)/lists

SUBDIRS = phraselists . contentscanners blacklists

WLISTS = bannedextensionlist bannediplist bannedmimetypelist \
	 bannedregexpurllist bannedsitelist bannedurllist banneduserlist \
	 contentregexplist exceptioniplist exceptionsitelist \
	 exceptionurllist exceptionuserlist filtergroupslist greysitelist \
	 greyurllist pics bannedphraselist weightedphraselist \
	 exceptionphraselist bannediplist exceptioniplist urlregexplist \
	 exceptionregexpurllist

EXTRA_DIST = bannedextensionlist bannediplist bannedmimetypelist \
	 bannedregexpurllist bannedsitelist bannedurllist banneduserlist \
	 contentregexplist exceptioniplist exceptionsitelist \
	 exceptionurllist exceptionuserlist filtergroupslist greysitelist \
	 greyurllist pics bannedphraselist.in weightedphraselist.in \
	 exceptionphraselist.in bannediplist exceptioniplist urlregexplist \
	 exceptionregexpurllist

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
