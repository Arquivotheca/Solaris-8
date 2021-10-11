#
# Copyright (c) 1992-1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.32	99/12/11 SMI"
#
# lib/libgss/Makefile
#
include ../../Makefile.master

# defines the duplicate sources we share with gsscred
GSSCRED_DIR = ../../cmd/gss/gsscred
DUPLICATE_SRC = gsscred_xfn.c gsscred_utils.c gsscred_file.c # gen_oids.c
CLEAN_SRC = $(DUPLICATE_SRC) gen_oids.c

SUBDIRS= spec .WAIT $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../Makefile.lib

#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@

#add the gsscred directory as part of the include path

HDRS=

ROOTDIRS= $(ROOT)/usr/include

ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

GSSMECH_DIR=	$(ROOT)/usr/lib/gss

#install rule
$(ROOTDIRS)/%: %
	$(INS.file)


all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=         TARGET= lint
_msg :=         TARGET= _msg


.KEEP_STATE:

all:  $(DUPLICATE_SRC) .WAIT $(SUBDIRS)

lint:	$(DUPLICATE_SRC) .WAIT $(SUBDIRS)

install: $(GSSMECH_DIR) all .WAIT $(SUBDIRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

LIBRARY=	libgss.a
TEXT_DOMAIN=	SUNW_OST_NETRPC
MSGDIRS=	.
POFILE=		$(LIBRARY:.a=.po)
POFILES=	generic.po

SED=	sed
GREP=	grep
CP=	cp

clean clobber:	$(SUBDIRS)

check: $(CHECKHDRS)

#copy the source files that we share with the gsscred utility
gsscred_xfn.c: $(GSSCRED_DIR)/gsscred_xfn.c
	rm -f gsscred_xfn.c
	$(CP) -p $(GSSCRED_DIR)/gsscred_xfn.c .

gsscred_utils.c: $(GSSCRED_DIR)/gsscred_utils.c
	rm -f gsscred_utils.c
	$(CP) -p $(GSSCRED_DIR)/gsscred_utils.c .

gsscred_file.c:	$(GSSCRED_DIR)/gsscred_file.c
	rm -f gsscred_file.c
	$(CP) -p $(GSSCRED_DIR)/gsscred_file.c .

$(ROOTDIRS):
	$(INS.dir)

$(GSSMECH_DIR):
	$(INS.dir)

$(ROOTETC)/%: %
	$(INS.file)

# include library targets
include ../Makefile.targ

#additional dependencies

spec $(MACH) $(MACH64):   FRC
	@cd $@; pwd; $(MAKE) $(TARGET)


_msg:   $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]*` 
	$(SED) -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

FRC:
