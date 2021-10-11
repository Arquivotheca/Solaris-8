#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/09/12 SMI"
#

CMDDIR		= ../../../..
DOCS		= flashupdate.txt
PLATFORM	= sun4u
PLATLINKS	=

include $(CMDDIR)/Makefile.cmd
include $(CMDDIR)/../Makefile.psm
include ../../../Makefile.com

USR_PSM_DOC_DIR		= $(USR_PSM_DIR)/doc
USR_PSM_DOCS		= $(DOCS:%=$(USR_PSM_DOC_DIR)/%)

OWNER			= root
GROUP			= sys
FILEMODE		= 444
POFILE			=
CLOBBERFILES		= Nothing_to_remove

DOC_LINKS	= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/doc)
INS.slink6	= $(RM) -r $@; $(SYMLINK) ../$(PLATFORM)/doc $@ $(CHOWNLINK) $(CHGRPLINK)

.KEEP_STATE:

$(DOC_LINKS):
	-$(INS.slink6)

all: $(DOCS)

install:

clean:

clobber:

lint:

install: all $(USR_PSM_DOC_DIR) $(DOC_LINKS) $(USR_PSM_DOCS)

$(USR_PSM_DOC_DIR):
	$(INS.dir.root.bin)

$(USR_PSM_DOC_DIR)/%:	%
	$(INS.file)

include $(CMDDIR)/Makefile.targ
include $(CMDDIR)/../Makefile.psm.targ
