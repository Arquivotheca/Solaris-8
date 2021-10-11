#
#ident   "@(#)Makefile 1.16     98/01/29 SMI"
#
# Copyright (c) 1992-1994,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/Makefile
#

include $(SRC)/Makefile.master

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)
SUBDIRS += platform

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

