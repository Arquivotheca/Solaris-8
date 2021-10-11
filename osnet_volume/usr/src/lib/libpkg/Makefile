#
#ident	"@(#)Makefile	1.13	98/12/19 SMI"
#
# Copyright (c) 1990,1998 by Sun Microsystems, Inc.
#
# lib/libpkg/Makefile
#
LIBRARY= libpkg.a

OBJECTS= \
	canonize.o	ckparam.o	ckvolseq.o	cvtpath.o	\
	devtype.o	dstream.o	gpkglist.o	gpkgmap.o	\
	isdir.o		logerr.o	mappath.o	ncgrpw.o	\
	nhash.o		pkgexecl.o	pkgexecv.o	pkgmount.o	\
	pkgtrans.o	pkgxpand.o	ppkgmap.o	progerr.o	\
	putcfile.o	rrmdir.o	runcmd.o	srchcfile.o	\
	tputcfent.o	verify.o

# include library definitions
include ../Makefile.lib

CPPFLAGS=	-I. $(CPPFLAGS.master)
LINTFLAGS=	-u
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)
TEXT_DOMAIN= SUNW_OST_ADMIN

#
# for messaging catalog
POFILE = libpkg.po
POFILES= $(OBJECTS:%.o=%.po)

BUILD.po= $(XGETTEXT) $(XGETFLAGS) $<.i ;\
	$(RM) $@ ;\
	if [ -f $(TEXT_DOMAIN).po ]; then \
		$(CAT) $(TEXT_DOMAIN).po >> messages.po;\
	fi;\
	sed "/^domain/d" < messages.po > $@ ;\
	$(RM) messages.po $<.i $(TEXT_DOMAIN).po

.KEEP_STATE:

all: $(LIBS) $(TXTS)

_msg:   $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	cp $(POFILE) $(MSGDOMAIN)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat $(POFILES) > $@

install: all $(ROOTLIBS)

lint:	$(LINTLIB)

$(MSGDOMAIN):
	$(INS.dir)


# include library targets
include ../Makefile.targ
