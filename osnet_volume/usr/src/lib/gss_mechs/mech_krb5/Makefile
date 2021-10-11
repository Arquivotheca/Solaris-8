#
#pragma ident	"@(#)Makefile	1.5	99/12/11 SMI"
#
# Copyright (c) 1995,1997,1999, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/gss_mechs/mech_krb5/Makefile
#
# This make file will build mech_krb5.so.1. This shared object
# contains all the functionality needed to support the Kereros V5 GSS-API
# mechanism. No other Kerberos libraries are needed.
#

include ../../../Makefile.master

SUBDIRS= $(MACH)/do $(MACH)/gl $(BUILD64) $(MACH64)/do $(MACH64)/gl

# include library definitions
include ../../Makefile.lib

GREP= find . \( -name SCCS -prune -o -name '*.[ch]' \) -print | sort | xargs grep

sparcv9_C_PICFLAGS =  -K PIC
TEXT_DOMAIN = SUNW_OST_NETRPC
POFILE = mech_krb5.po
POFILES = generic.po

HDRS=

CHECKHDRS= $(HDRS:%.h=%.check)

$(ROOTDIRS)/%:  %
	$(INS.file)

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
install :=      TARGET= install
lint :=         TARGET= lint

.KEEP_STATE:

all:    .WAIT $(SUBDIRS)

lint:   .WAIT $(SUBDIRS)

install: install_dir all .WAIT $(SUBDIRS)

# override ROOTLIBDIR and ROOTLINKS
ROOTLIBDIR=     $(ROOT)/usr/lib/gss

install_dir:	$(ROOTLIBDIR) $(BUILD64)

install_h:

clean:  $(SUBDIRS)

clobber: $(SUBDIRS)
	$(RM) $(POFILE) $(POFILES)

check: $(CHECKHDRS)

do_pkg:
	cd pkg ; pwd ; $(MAKE) install

$(ROOTLIBDIR):
	$(INS.dir)


# include library targets
# include ../../Makefile.targ

$(MACH)/do $(MACH)/gl $(MACH64)/do $(MACH64)/gl:      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:


FRC:

_msg: $(MSGDOMAIN) .WAIT $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po: FRC
	$(RM) messages.po
	-$(XGETTEXT) $(XGETFLAGS) `$(GREP) -s -l gettext`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)
