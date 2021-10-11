#
#ident	"@(#)Makefile.com	1.6	99/05/17 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/nis/Makefile.com
#
LIBRARY= libnss_nis.a
VERS= .1

OBJECTS= nis_common.o \
	getgrent.o getpwnam.o getspent.o \
	gethostent.o gethostent6.o getrpcent.o \
	getservent.o getnetent.o getprotoent.o \
	bootparams_getbyname.o ether_addr.o \
	getnetgrent.o netmasks.o getprinter.o \
	getauthattr.o getauuser.o getexecattr.o getprofattr.o getuserattr.o

# include library definitions, do not change the order of include and DYNLIB
include ../../../Makefile.lib

MAPFILE=	../common/mapfile-vers

# if we call this "DYNLIB", it will automagically be expanded to
# libnss_nis.so*, and we don't have a target for that.
DYNLIB1= nss_nis.so$(VERS)

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
