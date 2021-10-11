#
#ident  "@(#)Makefile 1.1     99/07/02 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5_psr/sparcv9/Makefile
#
SUBDIRS= sun4u

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber

.KEEP_STATE:

all install clean clobber : $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
