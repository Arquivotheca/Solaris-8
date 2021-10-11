#
#ident	"@(#)Makefile	1.11	99/01/25 SMI"
#
# Copyright (c) 1993,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/librac/Makefile
#
include ../../Makefile.master

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint

.KEEP_STATE:

all:  $(LIBS) .WAIT $(SUBDIRS)

txts: $(TXTS)

install: all .WAIT $(SUBDIRS) 

clean clobber lint: $(SUBDIRS)

MAPFILE=	mapfile-vers

include ../Makefile.targ

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

