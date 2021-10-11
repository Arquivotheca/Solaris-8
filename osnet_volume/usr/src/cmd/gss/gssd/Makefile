#
# Copyright (c) 1995,1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/gss/gssd/Makefile
#
#pragma ident	"@(#)Makefile	1.55	99/12/06 SMI"
#

TESTPROG = gssdtest 

OUTPUT_OPTION = -I. 

PROG= gssd

GSSD_BASEOBJS = gssd.o gssd_proc.o gssd_generic.o gssd_getuid.o
GSSC_BASEOBJS = gssdtest.o gssd_release_name_and_type.o gssd_clnt_stubs.o \
		gssd_handle.o

GD_OBJS	= gssd_svc.o
GC_OBJS	= gssd_clnt.o
G_OBJS	= gssd_xdr.o 
GSSDOBJS = $(GSSD_BASEOBJS) $(GD_OBJS) $(G_OBJS)
GSSCOBJS = $(GSSC_BASEOBJS) $(GC_OBJS) $(G_OBJS)

GSSD_LINTS = $(GSSD_BASEOBJS:.o=.c)
GSSC_LINTS = $(GSSC_BASEOBJS:.o=.c)

ROBJS	= $(GD_OBJS) $(GC_OBJS) $(G_OBJS)
OBJS	= $(GSSD_BASEOBJS) $(GD_OBJS) $(GSSC_BASEOBJS) $(GC_OBJS) $(G_OBJS)
SRCS	= $(OBJS:.o=.c)
RSRC	= $(ROBJS:.o=.c)
RSRC 	+= gssd.h

CLOBBERFILES += $(TESTPROG)

include ../../Makefile.cmd

TEXT_DOMAIN = SUNW_OST_NETRPC
POFILE = $(PROG).po
POFILES = generic.po

#
# Override $ROOTLIB
#
ROOTLIB=	$(ROOT)/usr/lib/gss

DIRS=	$(ROOTLIB)

$(ROOTLIB)/gssd :=	OWNER=	root
$(ROOTLIB)/gssd :=	GROUP=	bin

CPPFLAGS += -I$(SRC)/uts/common/gssapi/include
COPTFLAG += -xs #-I$(KINCDIR) 

LDLIBS += -lgss -lnsl -lsocket -ldl

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:

all: $(PROG) $(TESTPROG)

$(ROOTLIB):
	$(INS.dir)

$(ROOTLIB)/%:	%
	$(INS.file)

gssd:	$(GSSDOBJS) 
	$(LINK.c) $(GSSDOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

gssdtest:       $(GSSCOBJS) 
	$(LINK.c) $(GSSCOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

GSSDX=	$(SRC)/uts/common/gssapi/gssd.x
gssd.x:	$(GSSDX)
	rm -f $@
	cp $(GSSDX) $@

#  Rules to generate derived rpcgen files from gssd.x spec file.

gssd.h:        gssd.x
	$(RM) $@
	$(RPCGEN) -M -h gssd.x > $@

gssd_clnt.c:   gssd.x
	$(RM) $@
	$(RPCGEN) -M -l gssd.x > $@

gssd_svc.c:    gssd.x
	$(RM) $@
	$(RPCGEN) -M -m gssd.x > $@

gssd_xdr.c:    gssd.x
	$(RM) $@
	$(RPCGEN) -M -c gssd.x > $@

$(OBJS): gssd.h

install: all $(DIRS) $(ROOTLIBPROG)

install_h:

clean:
	$(RM) $(OBJS) $(RSRC) gssd.x

lint_gssd:
	$(LINT.c) $(GSSD_LINTS)

lint_gssc:
	$(LINT.c) $(GSSC_LINTS)

lint:	lint_gssd lint_gssc

include ../../Makefile.targ


$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po: FRC
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

FRC:

