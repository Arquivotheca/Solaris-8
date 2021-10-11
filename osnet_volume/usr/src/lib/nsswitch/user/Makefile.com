#
#ident	"@(#)Makefile.com	1.3	99/05/14 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/user/Makefile.com
#
LIBRARY= libnss_user.a
VERS= .1

OBJECTS= getprinter.o user_common.o

# include library definitions, do not change order of include and DYNLIB1
include ../../../Makefile.lib

MAPFILE=	../common/mapfile-vers

# if we call this "DYNLIB", it will automagically be expanded to
# libnss_files.so*, and we don't have a target for that.
DYNLIB1=	nss_user.so$(VERS)

$(ROOTLIBDIR)/$(DYNLIB1) :=      FILEMODE= 755
$(ROOTLIBDIR64)/$(DYNLIB1) :=    FILEMODE= 755

$(DYNLIB1):	pics .WAIT $$(PICS)
	$(BUILD.SO)
	$(POST_PROCESS_SO)

# See below.

LINTFLAGS=
CPPFLAGS += -D_REENTRANT -I../../../libc/inc
LDLIBS += -lnsl -lc
DYNFLAGS +=	-M $(MAPFILE)

ZDEFS=
LIBS = $(DYNLIB1)

$(DYNLIB1) := DYNFLAGS += -zdefs

.KEEP_STATE:

$(DYNLIB1):	$(MAPFILE)

all: $(LIBS)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
