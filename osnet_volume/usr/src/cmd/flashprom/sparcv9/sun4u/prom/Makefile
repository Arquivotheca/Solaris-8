#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.5	99/12/06 SMI"
#

CMDDIR		= ../../../..
PLATFORM	= sun4u
PLATLINKS	=
PROMS		= SUNW,Ultra-1 SUNW,Ultra-2 SUNW,Ultra-4 SUNW,Ultra-Enterprise
PROMZS		= $(PROMS:%=%.Z)

include $(CMDDIR)/Makefile.cmd
include $(CMDDIR)/../Makefile.psm
include ../../../Makefile.com

USR_PSM_LIB_PROM_DIR	= $(USR_PSM_LIB_DIR)/prom
USR_PSM_LIB_PROMS	= $(PROMS:%=$(USR_PSM_LIB_PROM_DIR)/%)

OWNER			= root
GROUP			= sys
FILEMODE		= 0444
POFILE			=
CLOBBERFILES		= $(PROMS)

#
# Used when building links in /usr/platform/$(PLATFORM)/lib for prom
#
PROM_LINKS	= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/prom)
INS.slink6	= $(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/prom $@ $(CHOWNLINK) $(CHGRPLINK)

.KEEP_STATE:

$(PROM_LINKS):
	-$(INS.slink6)

all: $(PROMS)

install:

clean:

clobber:

lint:

# Check in the prom file before doing a make!

$(PROMS):	$(PROMZS)
	-@$(RM) $@
	zcat < $@.Z > $@
	@$(CHMOD) -w $@

FRC:

install: all $(USR_PSM_LIB_PROM_DIR) $(PROM_LINKS) $(USR_PSM_LIB_PROMS) 

$(USR_PSM_LIB_PROM_DIR):
	$(INS.dir.root.bin)

$(USR_PSM_LIB_PROM_DIR)/%:	%
	$(INS.file)

include $(CMDDIR)/Makefile.targ
include $(CMDDIR)/../Makefile.psm.targ
