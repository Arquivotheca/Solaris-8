#
#pragma ident	"@(#)Makefile	1.5	99/12/06 SMI"
#
# Copyright (c) 1995,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/gss_mechs/mech_dh/dh192/Makefile
#

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../../../Makefile.lib

SED=    sed
CP=     cp
GREP=   grep

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
delete :=       TARGET= delete
install :=      TARGET= install
lint :=         TARGET= lint
catalog :=      TARGET= catalog
package :=      TARGET= package

.KEEP_STATE:
all:            $(SUBDIRS)

install:        $(SUBDIRS)

clean clobber delete lint catalog package:      $(SUBDIRS)

# include library targets
include ../../../Makefile.targ


_msg: $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

_errlst.po:
	$(RM) messages.po
	$(XGETTEXT) -a nsl/_errlst.c
	$(SED) -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext */*.[ch] nis/*/*.[ch]*`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
