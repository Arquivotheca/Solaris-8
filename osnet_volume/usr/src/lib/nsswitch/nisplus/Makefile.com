#
#ident	"@(#)Makefile.com	1.6	99/05/17 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/nisplus/Makefile
#
LIBRARY= libnss_nisplus.a
VERS= .1

OBJECTS= nisplus_common.o \
		gethostent.o gethostent6.o getrpcent.o \
		getnetent.o getprotoent.o getservent.o \
		getpwnam.o getgrent.o getspent.o \
		bootparams_getbyname.o ether_addr.o \
		getnetgrent.o netmasks.o getprinter.o \
		getauthattr.o getexecattr.o getprofattr.o getuserattr.o \
		getauuser.o

# include library definitions, do not change order of include and DYNLIB1
include ../../../Makefile.lib

MAPFILE=	../common/mapfile-vers

DYNLIB1=	nss_nisplus.so$(VERS)

$(ROOTLIBDIR)/$(DYNLIB1) :=      FILEMODE= 755        
$(ROOTLIBDIR64)/$(DYNLIB1) :=    FILEMODE= 755

$(DYNLIB1):	pics .WAIT $$(PICS)
	$(BUILD.SO)
	$(POST_PROCESS_SO)

LINTFLAGS=
CPPFLAGS += -D_REENTRANT
LDLIBS += -lnsl -lsocket -lc
DYNFLAGS +=	-M $(MAPFILE)

LIBS += $(DYNLIB1)

.KEEP_STATE:

all: $(LIBS)

$(DYNLIB1):	$(MAPFILE)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
