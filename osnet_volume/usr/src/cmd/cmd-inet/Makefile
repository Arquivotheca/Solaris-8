#
#ident	"@(#)Makefile	1.14	99/05/12 SMI"
#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#
# include global definitions
include	../Makefile.cmd

SUBDIR1= common
SUBDIR2= etc sbin
SUBDIR3= usr.bin usr.sbin usr.lib usr.sadm
SUBDIRS= $(SUBDIR1) $(SUBDIR2) $(SUBDIR3)
POFILES=        $(SUBDIR3:%=%/%.po)
POFILE= cmd-inet.po

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=          TARGET= _msg

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install lint: $(SUBDIR1) .WAIT $(SUBDIR2) $(SUBDIR3)

clean:		$(SUBDIRS)

clobber:	$(SUBDIRS) local_clobber

local_clobber:
	$(RM) $(CLOBBERFILES)

_msg: $(SUBDIR3)
	$(RM) $(POFILE)
	cat $(POFILES) > $(POFILE)
	$(RM)  $(MSGDOMAIN)/$(POFILE)
	cp $(POFILE) $(MSGDOMAIN)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
