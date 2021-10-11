#
#ident	"@(#)Makefile	1.19	98/12/19 SMI"
#
# Copyright (c) 1995,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/oampkg/libinst/Makefile
#
LIBRARY=	libinst.a

OBJECTS=copyf.o		dockdeps.o	echo.o		eptstat.o	\
	finalck.o	findscripts.o	fixpath.o	flex_dev.o	\
	isreloc.o	lockinst.o	mntinfo.o	nblk.o		\
	ocfile.o	pathdup.o	pkgdbmerg.o	procmap.o	\
	pkgobjmap.o	psvr4ck.o	ptext.o		putparam.o	\
	qreason.o	qstrdup.o	setadmin.o	setlist.o	\
	srcpath.o	scriptvfy.o	stub.o		doulimit.o	\
	dryrun.o	listmgr.o	is_local_host.o

# include library definitions
include ../../../lib/Makefile.lib

TEXT_DOMAIN= SUNW_OST_ADMIN
#
# messaging catalog
#
POFILE = libinst.po
POFILES= $(OBJECTS:%.o=%.po)

GENERAL=	../hdrs
LIBPKGDIR=	../../../lib/libpkg
CPPFLAGS=	-I$(GENERAL) -I$(LIBPKGDIR) $(CPPFLAGS.master)
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

.KEEP_STATE:

all: 		$(LIBS) $(TXTS)

catalog: $(POFILE)

$(POFILE): $(POFILES)
	$(RM)	$@
	cat $(POFILES)  > $@

install:	all

lint:		$(LINTLIB)

# include library targets
include ../../../lib/Makefile.targ
