#
#ident	"@(#)Makefile	1.14	97/08/12 SMI"        
#
# Copyright (c) 1990, 1994, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/iconv/Makefile

include ../Makefile.cmd

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS +=	$(MACH64)


all	:= TARGET = all
install	:= TARGET = install
clean	:= TARGET = clean
clobber	:= TARGET = clobber
lint	:= TARGET = lint
_msg	:= TARGET = catalog

.KEEP_STATE:

#.PARALLEL: $(SUBDIRS)

all install: $(SUBDIRS)

_msg:	$(SUBDIRS)

install:	$(ROOTPROG)

clean:	$(SUBDIRS)

clobber:	$(SUBDIRS)

lint:	$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
