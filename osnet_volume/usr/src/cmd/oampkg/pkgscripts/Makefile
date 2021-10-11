#
#ident	"@(#)Makefile	1.18	98/12/19 SMI"
#
# Copyright (c) 1990,1998 by Sun Microsystems, Inc.
#
# cmd/oampkg/pkgscripts/Makefile
#
PROG=		cmdexec
SHELLS=		r.build i.build i.sed i.awk r.sed r.awk i.CompCpio
ADMINFILE=	default
PRODUCT=	$(PROG) $(SHELLS)

SRCS=		$(PROG:%=%.c)

include ../../Makefile.cmd
TEXT_DOMAIN= SUNW_OST_ADMIN
#
# for messagin catalog file
#
POFILE= pkgscripts.po
POFILES= $(SRCS:%.c=%.po)

LIBPKG=		-lpkg
LIBPKGDIR=	../../../lib/libpkg
CPPFLAGS=	-I$(LIBPKGDIR) $(CPPFLAGS.master)
LDLIBS=		$(LDLIBS.cmd) $(LIBPKG)

FILEMODE=	0555
OWNER=		root
GROUP=		sys
ROOTPKGADM=	$(ROOT)/var/sadm/install/admin
ROOTPKGSCR=	$(ROOT)/usr/sadm/install/scripts
ROOTPKGSCRPROG=	$(PROG:%=$(ROOTPKGSCR)/%)
ROOTPKGSCRSHELL=$(SHELLS:%=$(ROOTPKGSCR)/%)
ROOTPKGADMFILE= $(ADMINFILE:%=$(ROOTPKGADM)/%)

$(ROOTPKGADMFILE) := FILEMODE = 0444

.KEEP_STATE:

all: 		$(PRODUCT) $(ADMINFILE) 

catalog: $(POFILE)

$(POFILE):      $(POFILES)
	$(RM) 	$@
	cat     $(POFILES)      > $@

install:	all .WAIT $(ROOTPKGSCRPROG) $(ROOTPKGSCRSHELL) $(ROOTPKGADMFILE)

$(ROOTPKGSCR)/%: %
	$(INS.file)

$(ROOTPKGADM)/%: %
	$(INS.file)

lint:		lint_SRCS

clean:

include ../../Makefile.targ
