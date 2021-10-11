#
# ident	"@(#)Makefile	1.11	98/08/12 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/dr_daemon/sparc/Makefile
#
SUBDIRS =       sun4u

all :=          TARGET= all
install :=      TARGET= install
clean :=        TARGET= clean
clobber :=      TARGET= clobber
lint :=         TARGET= lint

.KEEP_STATE:

all install lint clean clobber: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
