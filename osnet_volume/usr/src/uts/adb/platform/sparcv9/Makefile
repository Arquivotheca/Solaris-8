#
#ident  "@(#)Makefile 1.3     98/01/30 SMI"
#
# Copyright (c) 1993,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/platform/sparcv9/Makefile
#
SUBDIRS=        sun4u

# conditional assignments
all :=          TARGET= all
install :=      TARGET= install
clean :=        TARGET= clean
clobber :=      TARGET= clobber
lint :=         TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
