#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/krb5/kadmin/kpasswd/Makefile
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#

OUTPUT_OPTION = -I. 

PROG= kpasswd

OBJS	= tty_kpasswd.o kpasswd.o
SRCS	= $(OBJS:.o=.c)

CLOBBERFILES += $(TESTPROG)

include ../../../Makefile.cmd
include $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5

TEXT_DOMAIN = SUNW_OST_OSCMD
POFILE = $(PROG).po
POFILES = generic.po

#
# Override $ROOTLIB
#
KRB5BINDIR=	$(ROOT)/usr/bin
KRB5LIBDIR=	$(ROOT)$(KERBLIBDIR)

$(KRB5BINDIR)/kpasswd :=	OWNER=	root
$(KRB5BINDIR)/kpasswd :=	GROUP=	sys

$(KRB5BINDIR)/%:	%
	$(INS.file)

CPPFLAGS += -I$(SRC)/lib -I$(SRC)/lib/gss_mechs/mech_krb5/include \
	-I$(SRC)/uts/common/gssapi/include/ \
	-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
	-I$(SRC)/lib/krb5 \
	-DHAVE_LIBSOCKET=1 -DHAVE_LIBNSL=1 -DHAVE_UNISTD_H=1 \
	-DHAVE_SYS_TIMEB_H=1 -DHAVE_ALLOCA_H=1 -DHAVE_FTIME=1 \
	-DHAVE_TIMEZONE -DUSE_KADM5_API_VERSION=1

COPTFLAG += -xs #-I$(KINCDIR) 

LDLIBS += -L $(KRB5LIBDIR)
LDLIBS += -R $(KRUNPATH) $(KMECHLIB) $(KCLNTLIB) $(KRB5LIBDIR)/libss.so \
	-lgss -ldl -lsocket -lnsl -lc

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:

all: $(PROG)

kpasswd:	$(OBJS) 
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all
	-$(RM) -rf $(KRB5BINDIR)/$(PROG)
	$(INS) -s -m $(DIRMODE) -d $(KRB5BINDIR)
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(KRB5BINDIR) $(PROG)

install_h:

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../../../Makefile.targ

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@
	$(RM) generic.po

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

