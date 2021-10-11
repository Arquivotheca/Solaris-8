#
#ident	"@(#)Makefile.com	1.7	99/05/27 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/files/Makefile.com
#
LIBRARY= libnss_files.a
VERS= .1

OBJECTS= bootparams_getbyname.o ether_addr.o getgrent.o gethostent.o \
	gethostent6.o getnetent.o getprotoent.o getpwnam.o getrpcent.o \
	getprinter.o getservent.o getspent.o netmasks.o files_common.o \
	getauthattr.o getprofattr.o getexecattr.o getuserattr.o getauuser.o

# include library definitions, do not change order of include and DYNLIB1
include ../../../Makefile.lib

MAPFILE=	../common/mapfile-vers

# if we call this "DYNLIB", it will automagically be expanded to
# libnss_files.so*, and we don't have a target for that.
DYNLIB1=	nss_files.so$(VERS)

$(ROOTLIBDIR)/$(DYNLIB1) :=      FILEMODE= 755
$(ROOTLIBDIR64)/$(DYNLIB1) :=    FILEMODE= 755

$(DYNLIB1):	pics .WAIT $$(PICS)
	$(BUILD.SO)
	$(POST_PROCESS_SO)

# See below.
PSLIB= $(DYNLIB1).ps
ROOTETCLIB= $(ROOT)/etc/lib
ROOTPSLIB=  $(ROOTETCLIB)/$(DYNLIB1)
CLOBBERFILES += $(PSLIB)

$(ROOTPSLIB) :=	FILEMODE= 755

$(ROOTETCLIB)/%: %.ps
	$(INS.rename)

LINTFLAGS=
CPPFLAGS += -D_REENTRANT -I../../../libc/inc
LDLIBS += -lnsl -lc
DYNFLAGS +=	-M $(MAPFILE)

ZDEFS=
LIBS += $(DYNLIB1)

.KEEP_STATE:

$(DYNLIB1):	$(MAPFILE)

all: $(LIBS) $(PSLIB)

# Generate a library version nss_files.so$(VERS).ps that does not have
# a declared dependency on libc.  The ".ps" is stripped off the name
# before it is installed on the root partition in /etc/lib.  This is
# needed for diskless clients and for hosts that mount /usr over NFS.
#
$(PSLIB): pics .WAIT $$(PICS)
	$(LD) -o $@ -dy -G $(DYNFLAGS) $(PICS)
	$(POST_PROCESS_SO)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
