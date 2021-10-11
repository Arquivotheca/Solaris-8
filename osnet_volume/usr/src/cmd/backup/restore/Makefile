#
# cmd/backup/restore/Makefile
#
#
# Copyright (c) 1996,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# Copyright (c) 1983 Regents of the University of California.
# All rights reserved.  The Berkeley software License Agreement
# specifies the terms and conditions for redistribution.
#
# "@(#)Makefile	1.30	99/01/22 SMI"
#
#	CAUTION:  FSTYPE must be defined before including ../Makefile.backup,
#		  otherwise UTFROOTPKGUSRLIBFSTYPE doesn't get expanded
#		  properly and the magic doesn't happen.

FSTYPE=		ufs

include ../Makefile.backup

UFSRESTORE=	ufsrestore

PROG=		$(UFSRESTORE)
PRODUCT=	$(PROG)

OBJECTS=	dirs.o interactive.o main.o restore.o \
		symtab.o tape.o utilities.o
SRCS=		$(OBJECTS:%.o=%.c)

POFILES=	$(OBJECTS:%.o=%.po) ../lib/libdump.po
POFILE=		ufsrestore.po

# XXX This is only needed for the check: target.  It would be nice to
# XXX automatically generate the list when needed.
HEADERS=	../../../head/protocols/dumprestore.h \
	../include/byteorder.h ../include/memutils.h \
	../include/rmt.h restore.h

CLOBBERFILES=	$(PRODUCT) $(DEBUGPRODUCTS) *.ln $(POFILES)

# A list of static libraries that use libdl_stubs which fakes the real
# dynamic linker by returning static symbols from the pre-linked libraries.
# This is used for ufsrestore only.
#
# Need -u res_getbyname to FORCE the loading of -lresolv to fix bug 4123028
# else gethostbyname() calls itself recursively instead of res_gethostbyname()
STATICLIBS=	-ldl_stubs -u res_gethostbyname -lresolv -lnss_files \
		-lnss_nis -lnss_nisplus -lnss_compat -lnss_dns -lsocket \
		-lnsl -lstraddr

LOCAL=		.
GENERAL=	../include
GLOBAL=		../../../head
CPPFLAGS=	-I$(LOCAL) -I$(GENERAL) -I$(GLOBAL) \
			$(CPPFLAGS.master) -D_LARGEFILE64_SOURCE=1
LIBDUMP=	../lib/libdump.a
LINTLIBDUMP=	../lib/llib-ldump.ln
LDLIBS=		$(LDLIBS.cmd) -L../lib -ldump -dn -lsocket -lnsl $(STATICLIBS)

UFSROOTLINK=	$(UFSROOTUSRSBIN)/$(PROG)
LINKVALUE=	../lib/fs/$(FSTYPE)/$(PROG)

FILEMODE=	04555
OWNER=		root

.KEEP_STATE:

all:		$(PRODUCT) 

$(PROG):	$(OBJECTS) $$(LIBDUMP)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

$(LIBDUMP): FRC
	$(DO_LIBDIR)

FRC:

install:	all $(UFSROOTPKGUSRLIBFSTYPE) $(UFSROOTLINK)

lint:	$(SRCS) $(LINTLIBDUMP)
	$(LINT.c) $(SRCS) $(LINTLIBDUMP) 2>&1 \
	| sed -f lint.sed | grep -v '^[ 	]'

$(LINTLIBDUMP):	FRC
	cd ../lib; pwd; $(MAKE) lint
	pwd

check:
	$(CSTYLE) $(CSTYLEFLAGS) $(SRCS) $(HEADERS)
	$(HDRCHK) $(HDRCHKFLAGS) $(HEADERS)

clean:
	$(RM) $(OBJECTS) $(DEBUGOBJS) *.ln

$(UFSROOTLINK):
	-$(RM) $@; $(SYMLINK) $(LINKVALUE) $(UFSROOTLINK)

$(POFILE):	$(POFILES)
	$(RM) $@; cat $(POFILES) > $@

../lib/libdump.po:
	cd ../lib ; pwd ; $(MAKE)  libdump.po

include ../Makefile.targ
