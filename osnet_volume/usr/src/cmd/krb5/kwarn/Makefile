#
# Copyright (c) 1998,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/krb5/kwarnd/Makefile
#
#pragma ident	"@(#)Makefile	1.2	00/09/14 SMI"
#

TESTPROG = kwarndtest 

OUTPUT_OPTION = -I. 

PROG= ktkt_warnd

KWARNDOBJS = kwarnd.o kwarnd_svc.o kwarnd_proc.o kwarnd_generic.o kwarnd_send.o
KWARNCOBJS = kwarndtest.o kwarnd_clnt.o \
		kwarnd_clnt_stubs.o kwarnd_handle.o
G_OBJS   = kwarnd_xdr.o 

OBJS	= $(KWARNDOBJS) $(KWARNCOBJS) $(G_OBJS)
SRCS	= $(OBJS:.o=.c)
RSRC	= kwarnd.h kwarnd_clnt.c kwarnd_svc.c kwarnd_xdr.c

CLOBBERFILES += $(TESTPROG)

include ../../Makefile.cmd

TEXT_DOMAIN = SUNW_OST_OSCMD
POFILE = $(PROG).po
POFILES = generic.po

$(ROOTLIB)/%:	%
	$(INS.file)

$(ROOTLIB)/kwarnd :=	OWNER=	root
$(ROOTLIB)/kwarnd :=	GROUP=	sys

COPTFLAG += -xs #-I$(KINCDIR) 
CPPFLAGS +=	-D_REENTRANT
LDLIBS += -lnsl -lsocket -ldl -lthread

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:

all: $(PROG) $(TESTPROG)

ktkt_warnd:	$(G_OBJS) $(KWARNDOBJS) 
	$(LINK.c) $(KWARNDOBJS) $(G_OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

kwarndtest:       $(G_OBJS) $(KWARNCOBJS) 
	$(LINK.c) $(KWARNCOBJS) $(G_OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

#  Rules to generate derived rpcgen files from kwarnd.x spec file.

kwarnd.h:        kwarnd.x
	$(RM) $@
	$(RPCGEN) -M -h kwarnd.x > $@

kwarnd_clnt.c:   kwarnd.x
	$(RM) $@
	$(RPCGEN) -M -l kwarnd.x > $@

kwarnd_svc.c:    kwarnd.x
	$(RM) $@
	$(RPCGEN) -M -m kwarnd.x > $@

kwarnd_xdr.c:    kwarnd.x
	$(RM) $@
	$(RPCGEN) -M -c kwarnd.x > $@

$(OBJS): kwarnd.h

install: all
	-$(RM) -rf $(KRB5LIB)/$(PROG)
	$(INS) -s -m $(DIRMODE) -d $(KRB5LIB)
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(KRB5LIB) $(PROG)

install_h: $(RSRC)

clean:
	$(RM) $(OBJS) $(RSRC)

lint: lint_SRCS

include ../../Makefile.targ

#additional dependencies

$(LIBRARY) : $(OBJS)
$(DYNLIB) : $(PICS)

FRC:

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po: FRC
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po
