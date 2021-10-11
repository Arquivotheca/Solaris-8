#
#ident	"@(#)Makefile	1.3	99/09/12 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All Rights Reserved.
#

CMDDIR		= ../../..
PLATFORM	= sun4u
PLATLINKS	=
CLASS		= 32

include $(CMDDIR)/Makefile.cmd
include $(CMDDIR)/../Makefile.psm
include ../../Makefile.com

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

SUBDIRS		= initpkg prom doc
SCRIPT		= flash-update.sh
POFILE		=
CLOBBERFILES	= Nothing_to_remove

USR_PSM_LIB_PROG = $(SCRIPT:%=$(USR_PSM_LIB_DIR)/%)

$(USR_PSM_LIB_PROG):=	FILEMODE = 0755
$(USR_PSM_LIB_PROG):=	OWNER = root
$(USR_PSM_LIB_PROG):=	GROUP = sys

#
# Used when building links in /usr/platform/$(PLATFORM)/lib for flash-update.sh
#
SCRIPT_LINKS	= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/$(SCRIPT))
INS.slink6	= $(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/$(SCRIPT) $@ $(CHOWNLINK) $(CHGRPLINK)

.KEEP_STATE:

$(SCRIPT_LINKS):
	-$(INS.slink6)

all: $(SUBDIRS) $(SCRIPT)

install: $(SUBDIRS) $(USR_PSM_LIB_PROG) $(SCRIPT_LINKS)

clean: $(SUBDIRS)

lint: $(SUBDIRS)

clobber: $(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include $(CMDDIR)/Makefile.targ
include $(CMDDIR)/../Makefile.psm.targ
