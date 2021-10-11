#
#ident	"@(#)Makefile	1.13	99/09/20 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# ucblib/librpcsoc/Makefile
#

include $(SRC)/Makefile.master

SUBDIRS = spec .WAIT $(MACH) $(BUILD64) $(MACH64)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

#conditional assignments
all :=		TARGET=all
install :=      TARGET= install
clean :=        TARGET= clean
clobber :=      TARGET= clobber
lint :=         TARGET= lint

.KEEP_STATE:

all:	.WAIT $(SUBDIRS)

install: all .WAIT $(SUBDIRS)

install_h: $(ROOTHDRS)

clean clobber: $(SUBDIRS)

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
