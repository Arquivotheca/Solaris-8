#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.25	99/05/14 SMI"
#

include ../Makefile.cmd

include ./Makefile.locale

SUBDIRS =	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

POFILE=  localedef.po
POFILES= err.po
XGETFLAGS=
CLOBBERFILES +=	locale_description

.KEEP_STATE:

.NO_PARALLEL:	$(SUBDIRS)

all:	$(SUBDIRS)

install:	$(ROOTDIRS) $(ROOTSYMLN) .WAIT $(ROOTCDESC) \
	$(SUBDIRS) \
	$(ROOTI18NEXT)/$(SB_DENSE_EXTENSIONS) \
	$(ROOTI18NEXT)/$(GENERIC_EUCBC_EXTENSIONS) \
	$(ROOTISOSRCDIR) $(ROOTISOSRCS)

clean lint:	$(SUBDIRS)

clobber:	$(SUBDIRS) clobber_local

clobber_local:
	$(RM) $(CLOBBERFILES)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ./Makefile.target

include ../Makefile.targ
