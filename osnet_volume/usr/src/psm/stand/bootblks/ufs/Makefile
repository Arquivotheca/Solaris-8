#
#ident	"@(#)Makefile	1.26	99/03/30 SMI"
#
# Copyright (c) 1989-1994,1998 by by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/Makefile
#

SUBDIRS	= $(MACH)

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

.KEEP_STATE:

all install clean clobber lint : $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

#
# install targets
#
$(USR_SBIN): $(USR)
	-$(INS.dir.root.bin)

$(USR_SBIN)/%:	% $(USR_SBIN)
	$(INS.file)
