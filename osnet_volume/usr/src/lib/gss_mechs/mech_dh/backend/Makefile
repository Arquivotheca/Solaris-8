#
#pragma ident	"@(#)Makefile	1.6	99/12/06 SMI"
#
# Copyright (c) 1995,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/gss_mechs/mech_dh/backend/Makefile
#
PROTOCOL_DIR = mech
SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

PROTO_FILE = dhmech_prot.x
DERIVED_FILES = mech/dhmech_prot.h mech/xdr_token.c
CLEANFILES += $(DERIVED_FILES)

# include library definitions
include ../../../Makefile.lib

TEXT_DOMAIN =	SUNW_OST_NETRPC
POFILES =	generic.po
POFILE = mech_dh.po

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
_msg :=		TARGET= _msg

.KEEP_STATE:
all:            $(DERIVED_FILES) .WAIT $(SUBDIRS)

install:        $(DERIVED_FILES) .WAIT $(SUBDIRS)

clean clobber delete lint catalog package:      $(SUBDIRS)

#
# Rules for building the derived files
#
mech/xdr_token.c: $(PROTOCOL_DIR)/dhmech_prot.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/dhmech_prot.x |\
	$(SED) -e 's!$(PROTOCOL_DIR)/dhmech_prot.h!dhmech_prot.h!' > $@

mech/dhmech_prot.h: mech/dhmech_prot.x
	$(RPCGEN) -h $(PROTOCOL_DIR)/dhmech_prot.x > $@

# include library targets
include ../../../Makefile.targ


_msg: $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext mech/*.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
