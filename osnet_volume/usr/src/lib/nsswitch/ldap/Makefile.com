#
#ident  "@(#)Makefile.com 1.2     99/08/24 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/ldap/Makefile.com
#

#LIBRARY= libnss_ldap.so
VERS= .1

OBJECTS= ldap_common.o getpwnam.o getspent.o ldap_utils.o getrpcent.o \
	getgrent.o getnetmasks.o getservent.o getbootparams.o \
	getether.o getprotoent.o getnetent.o gethostent.o gethostent6.o \
	getnetgrent.o getkeyent.o getauuser.o \
	getauthattr.o getexecattr.o getprofattr.o getuserattr.o

# include library definitions, do not change the order of include and DYNLIB
include ../../../Makefile.lib

MAPFILE=	../common/mapfile-vers

LINTOUT=	lint.out
LINTFLAGS=	-ux

SRCS=		$(OBJECTS:%.o=../common/%.c)

# if we call this "DYNLIB", it will automagically be expanded to
# libnss_ldap.so*, and we don't have a target for that.
DYNLIB1= nss_ldap.so$(VERS)
DYNLIB2= libnss_ldap.so$(VERS)

LIBS= $(DYNLIB1)

$(ROOTLIBDIR)/$(DYNLIB1) :=      FILEMODE= 755        
$(ROOTLIBDIR64)/$(DYNLIB1) :=    FILEMODE= 755
$(ROOTLIBDIR64)/$(DYNLIB2) :=    FILEMODE= 755

$(DYNLIB1) $(DYNLIB2):	pics .WAIT $$(PICS)
	$(BUILD.SO)
	$(POST_PROCESS_SO)

CLOBBERFILES +=	$(LINTOUT)

CLEANFILES +=	$(LINTOUT)

CPPFLAGS += -D_REENTRANT -I../../../libsldap/common
LDLIBS += -lsldap -lnsl -lsocket -lc
DYNFLAGS +=	-M $(MAPFILE)

.KEEP_STATE:

all: $(LIBS)

$(DYNLIB1) $(DYNLIB2):	$(MAPFILE)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

lint:
	$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1
