#
#ident  "@(#)Makefile 1.20     98/12/19 SMI"
#
# Copyright (c) 1990,1998 by Sun Microsystems, Inc.
#
# cmd/oampkg/Makefile
#

include ../Makefile.cmd
TEXT_DOMAIN= SUNW_OST_ADMIN

PRODUCTS=	pkgadd    pkginstall   pkgrm    pkgremove   pkginfo  \
		pkgproto  pkgchk       pkgmk   pkgscripts   installf \
		pkgtrans  pkgmv        pkgname  pkgparam

TXTS= 		Install Prototype README

PRODSUBDIRS=	$(PRODUCTS)
LIBSUBDIRS=	libinst
LIBDONE=	$(LIBSUBDIRS)/.target_done
SUBDIRS=	$(LIBSUBDIRS) $(PRODSUBDIRS)

ROOTPKGLOC=	$(ROOT)/var/sadm/pkg
ROOTPKGADM=	$(ROOT)/var/sadm/install
ROOTPKGADMIN=	$(ROOTPKGADM)/admin
ROOTPKGLOGS=	$(ROOTPKGADM)/logs
ROOTPKGINS=	$(ROOT)/usr/sadm/install
ROOTPKGBIN=	$(ROOTPKGINS)/bin
ROOTPKGSCR=	$(ROOTPKGINS)/scripts

INSDIRS=	$(ROOTBIN)	$(ROOTUSRSBIN)	$(ROOTPKGLOC)	$(ROOTPKGADM) \
		$(ROOTPKGADMIN)	$(ROOTPKGLOGS)  $(ROOTPKGINS)	$(ROOTPKGBIN) \
		$(ROOTPKGSCR)

$(CH)$(ROOTPKGLOC)   := DIRMODE = 555
$(ROOTPKGLOC)   := OWNER   = root
$(ROOTPKGLOC)   := GROUP   = sys
$(CH)$(ROOTPKGADM)   := DIRMODE = 555
$(CH)$(ROOTPKGSCR)   := DIRMODE = 555
$(CH)$(ROOTPKGADMIN)	:= DIRMODE = 555
$(CH)$(ROOTPKGLOGS)	:= DIRMODE = 555

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

#
# for message catalog files
#
_msg:=          TARGET= catalog
POFILE= oampkg.po
POFILES= $(SUBDIRS:%=%/%.po)

.KEEP_STATE:

.DONE:
	@rm -f $(LIBDONE)

all: $(PRODSUBDIRS) $(TXTS)

clean lint: $(SUBDIRS)

clobber:	$(SUBDIRS) local_clobber

local_clobber:
	$(RM) $(CLOBBERFILES)

install: $(INSDIRS) .WAIT $(PRODSUBDIRS) 

_msg:   $(SUBDIRS)
	 $(RM) $(POFILE)
	 cat $(POFILES) > $(POFILE)
	 $(RM) -f $(MSGDOMAIN)/$(POFILE)
	 cp $(POFILE) $(MSGDOMAIN)

$(INSDIRS):
	$(INS.dir)

$(PRODSUBDIRS): $(LIBSUBDIRS) FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(LIBSUBDIRS): $(LIBDONE)
	@cd $@; pwd; $(MAKE) $(TARGET)

$(LIBDONE):
	@touch $(LIBDONE)

FRC:
