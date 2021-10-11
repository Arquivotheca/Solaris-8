#
# Copyright (c) 1995,1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.6	97/11/06 SMI"
#
LIBRARY= rpcsec.a
VERS = .1

GSSRPC= rpcsec_gss.o rpcsec_gss_misc.o rpcsec_gss_utils.o svc_rpcsec_gss.o

OBJECTS= $(GSSRPC)

# include library definitions
include ../../Makefile.lib

MAPFILE=	../mapfile-vers
SRCS=	$(OBJECTS:%.o=../%.c)

# librpcsec build rules

objs/%.o profs/%.o pics/%.o: ../%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@

CPPFLAGS +=     -D_REENTRANT -I$(SRC)/uts/common/gssapi/include  \
		-I$(SRC)/uts/common
$(PICS) := 	CFLAGS += -xF
$(PICS) := 	CCFLAGS += -xF

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

LIBS  = $(DYNLIB)

LDLIBS += -lgss -lnsl -lmp -lc -ldl
DYNFLAGS += -M $(MAPFILE)

TEXT_DOMAIN= SUNW_OST_NETRPC

.KEEP_STATE:

lint: $(LINTLIB)

# include library targets
include ../../Makefile.targ

$(LIBRARY) : $(OBJS)
$(DYNLIB) : $(PICS) $(MAPFILE)
