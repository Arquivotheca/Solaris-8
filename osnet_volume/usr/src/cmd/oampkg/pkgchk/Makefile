#
#ident	"@(#)Makefile	1.22	98/12/19 SMI"
#
# Copyright (c) 1990, 1994, 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/oampkg/pkgchk/Makefile
#
PROG=		pkgchk

OBJECTS=	main.o checkmap.o ckentry.o
SRCS=		$(OBJECTS:.o=.c)

include ../../Makefile.cmd
TEXT_DOMAIN= SUNW_OST_ADMIN
#
# for messaging catalog
#
POFILE= pkgchk.po
POFILES= $(SRCS:%.c=%.po)

GENERAL=	../hdrs
LIBPKGDIR=	../../../lib/libpkg
CPPFLAGS=	-I$(GENERAL) -I$(LIBPKGDIR) $(CPPFLAGS.master)
LIBINST=	../libinst/libinst.a
LIBPKG=		-lpkg
LIBADM=		-ladm
LDLIBS +=	$(LDLIBS.cmd) $(LIBINST) $(LIBPKG) $(LIBADM) -ll -lnsl -lsocket

LINK.c=     $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

OWNER=		root
GROUP=		sys

.KEEP_STATE:

all: 		$(PROG) 

$(PROG):	$(OBJECTS) $(LIBINST)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

catalog:        $(POFILE)

$(POFILE):      $(POFILES)
	$(RM) 	$@
	cat     $(POFILES)      > $@

$(LIBINST): FRC
	@if [ ! -f $(@D)/.target_done ]; then \
		cd $(@D); pwd; $(MAKE) $(TARGET); \
	fi
	
FRC:

install:	all $(ROOTUSRSBINPROG)

lint:		lint_SRCS

clean:
	$(RM) $(OBJECTS)

include ../../Makefile.targ
