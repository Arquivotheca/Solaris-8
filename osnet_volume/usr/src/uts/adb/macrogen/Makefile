#
#ident  "@(#)Makefile 1.6     98/01/09 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# uts/adb/macrogen/Makefile
#
SUBDIRS=        $(MACH)

# conditional assignments
all :=          TARGET= all
install :=      TARGET= install
clean :=        TARGET= clean
clobber :=      TARGET= clobber
lint :=         TARGET= lint

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
