#
#ident	"@(#)Makefile	1.1	98/12/19 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# cmd/oampkg/pkgname/Makefile
#
PROG=		pkgname

OBJECTS=	pkgname.o
SRCS=		$(OBJECTS:%.o=%.c)

include ../../Makefile.cmd
TEXT_DOMAIN= SUNW_OST_ADMIN
#
# For messaging catalog
#
POFILE= pkgname.po

GENERAL=	../hdrs
LIBPKGDIR=	../../../lib/libpkg
CPPFLAGS=	-I$(GENERAL) -I$(LIBPKGDIR) $(CPPFLAGS.master)
LIBINST=	../libinst/libinst.a
LIBPKG=		-lpkg
LIBADM=		-ladm
LDLIBS= 	$(LDLIBS.cmd) $(LIBINST) $(LIBPKG) $(LIBADM)

ROOTPKGBIN=	$(ROOT)/usr/sadm/install/bin
ROOTPKGBINPROG=	$(PROG:%=$(ROOTPKGBIN)/%)
OWNER=		root
GROUP=		sys

.KEEP_STATE:

all: 		$(PROG)

$(PROG):	$(OBJECTS) $(LIBINST)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

catalog:        $(POFILE)

$(LIBINST): FRC
	@if [ ! -f $(@D)/.target_done ]; then \
		cd $(@D); pwd; $(MAKE) $(TARGET); \
	fi

FRC:

install:	all $(ROOTPKGBINPROG)

$(ROOTPKGBIN)/%:	%
	$(INS.file)

lint:	lint_SRCS

clean:
	$(RM) $(OBJECTS)

include ../../Makefile.targ
