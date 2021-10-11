#
#ident	"@(#)Makefile	1.21	98/12/19 SMI"
#
# Copyright (c) 1990,1998 by Sun Microsystems, Inc.
#
# cmd/oampkg/pkgadd/Makefile
#
PKGADD=		pkgadd

PROG=		$(PKGADD)

OBJECTS=	main.o quit.o presvr4.o
SRCS=		$(OBJECTS:.o=.c)

include ../../Makefile.cmd
TEXT_DOMAIN= SUNW_OST_ADMIN
#
# For messaging catalog
#
POFILE = $(PKGADD).po
POFILES= $(OBJECTS:%.o=%.po)

GENERAL=	../hdrs
LIBPKGDIR=	../../../lib/libpkg
CPPFLAGS=	-I. -I$(GENERAL) -I$(LIBPKGDIR) $(CPPFLAGS.master)
LIBINST=	../libinst/libinst.a
LDLIBS= 	$(LDLIBS.cmd) $(LIBINST) -lpkg -ladm

OWNER=		root
GROUP=		sys

ROOTUSRSBINLINK=$(ROOTUSRSBIN)/pkgask

.KEEP_STATE:

all: 		$(PROG) 

catalog:        $(POFILE)

$(POFILE):      $(POFILES)
	$(RM) $@
	cat     $(POFILES)      > $@

$(PROG):	$(OBJECTS) $(LIBINST)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

$(LIBINST): FRC
	@if [ ! -f $(@D)/.target_done ]; then \
		cd $(@D); pwd; $(MAKE) $(@F); \
	fi
	
FRC:

install:	all $(ROOTUSRSBINPROG) $(ROOTUSRSBINLINK)

$(ROOTUSRSBINLINK): $(ROOTUSRSBINPROG)
	$(RM) $@; $(LN) $(ROOTUSRSBINPROG) $@

lint:		lint_SRCS

clean:
	$(RM) $(OBJECTS)

include ../../Makefile.targ
