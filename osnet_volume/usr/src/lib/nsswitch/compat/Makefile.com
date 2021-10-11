#
#ident	"@(#)Makefile.com	1.3	97/08/27 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# lib/nsswitch/compat/Makefile
#
LIBRARY= libnss_compat.a
VERS= .1

OBJECTS= getpwent.o getgrent.o getspent.o compat_common.o

# include library definitions, do not change the order of include and DYNLIB1
include ../../../Makefile.lib

MAPFILE=	../common/mapfile-vers

# if we call this "DYNLIB", it will automagically be expanded to
# libnss_compat.so*, and we don't have a target for that.
DYNLIB1= nss_compat.so$(VERS)

$(ROOTLIBDIR)/$(DYNLIB1) :=      FILEMODE= 755
$(ROOTLIBDIR64)/$(DYNLIB1) :=    FILEMODE= 755

$(DYNLIB1):	pics .WAIT $$(PICS)
	$(BUILD.SO)
	$(POST_PROCESS_SO)

LINTFLAGS=
CPPFLAGS += -D_REENTRANT
LDLIBS += -lnsl -lc
DYNFLAGS += -M $(MAPFILE)

LIBS += $(DYNLIB1)

.KEEP_STATE:

all: $(LIBS)

$(DYNLIB1):	$(MAPFILE)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
