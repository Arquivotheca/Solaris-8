#
# realmode/pkgdefs/Makefile
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

#ident	"@(#)Makefile	1.2	00/09/14 SMI"

include $(SRC)/Makefile.master

sparc_SUBDIRS=

i386_SUBDIRS= \
	CDKcdboot \
	SUNWrmodr

COMMON_SUBDIRS= \
	SUNWrmodu

SUBDIRS= $($(MACH)_SUBDIRS) $(COMMON_SUBDIRS)


all :=		TARGET= all
install :=	TARGET= install
install_h :=	TARGET= install_h
clean :=	TARGET= clean
clobber :=	TARGET= clobber

%: %.ksh
	$(RM) $@
	cp $< $@
	chmod +x $@

.KEEP_STATE:

.PARALLEL: $(SUBDIRS)

all install: $(SUBDIRS)

clean clobber: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

