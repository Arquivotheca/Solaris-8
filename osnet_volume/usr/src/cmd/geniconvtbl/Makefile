# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/05/25 SMI"
#
# cmd/geniconvtbl/Makefile
#


include ../Makefile.cmd

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS +=	$(MACH64)

SAMPLES= samples

all	:= TARGET = all
install	:= TARGET = install
clean	:= TARGET = clean
clobber	:= TARGET = clobber
lint	:= TARGET = lint
_msg	:= TARGET = catalog

.KEEP_STATE:

.PARALLEL: $(SUBDIRS)

all install: $(SUBDIRS) $(SAMPLES)

_msg:		$(SUBDIRS)

install:	$(ROOTPROG) $(SAMPLES)

clean:		$(SUBDIRS) $(SAMPLES)

clobber:	$(SUBDIRS) $(SAMPLES)

lint:		$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(SAMPLES): .WAIT $(SUBDIRS)
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
