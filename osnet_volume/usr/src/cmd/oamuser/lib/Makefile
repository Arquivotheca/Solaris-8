#
#ident	"@(#)Makefile	1.5	96/09/05 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/oamuser/lib/Makefile
#
LIBRARY= 	lib.a

DATEFILE= 	ugdates
DATEFILESRC= 	ugdates.dat
TXT= 		$(DATEFILESRC)

OBJECTS= 	putgrent.o     errmsg.o     file.o     vgid.o     vgname.o \
		vgroup.o       vuid.o       get_ngm.o  vlogin.o \
		dates.o        vexpire.o    prerrno.o

# include library definitions
include ../../Makefile.cmd
include ../../../lib/Makefile.lib

GREP= 		grep
FILEMODE=	$(LIBFILEMODE)

PRODUCT=	$(LIBRARY) $(DATEFILE)

# Must retain `lib', since default expands to nothing
LLINTLIB=	llib-l$(LIBRARY:lib%.a=lib).ln

CLEANFILES=	$(LLINTLIB)
CLOBBERFILES=	$(DATEFILE)

GENERAL=	../inc
CPPFLAGS=	-I. -I$(GENERAL) $(CPPFLAGS.master)
ARFLAGS=	cr
AROBJS=		`$(LORDER) $(OBJS) | $(TSORT)`
LINTFLAGS=	-u

ROOTUSRSADM=	$(ROOT)/usr/sadm
ROOTUSRSADMFILE=$(DATEFILE:%=$(ROOTUSRSADM)/%)

.KEEP_STATE:

all:		$(PRODUCT) $(TXT)

$(DATEFILE):	$(DATEFILESRC)
	$(GREP) -v "^#ident" $(DATEFILESRC) > $(DATEFILE)

install:	all $(DATEFILE) $(ROOTUSRSADMFILE)

$(ROOTUSRSADM)/%: %
	$(INS.file)

lint:		$(LLINTLIB)

$(LLINTLIB):	$(SRCS)
	$(LINT.c) -o $(LIBRARY:lib%.a=lib) $(SRCS) > $(LINTOUT) 2>&1

include ../../../lib/Makefile.targ
