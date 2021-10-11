#
# Copyright (c) 1997,1999, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/krb5/kadmin/ktutil/Makefile
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#

OUTPUT_OPTION = -I. 

PROG= ktutil

OBJS	= ktutil.o ktutil_ct.o ktutil_funcs.o
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
ROOTLIB=	$(ROOT)/usr/lib/gss

$(ROOTLIB)/%:	%
	$(INS.file)

$(ROOTLIB)/gssd :=	OWNER=	root
$(ROOTLIB)/gssd :=	GROUP=	sys

CPPFLAGS += -I$(SRC)/uts/common/gssapi/include \
	-I$(SRC)/lib -I$(SRC)/lib/gss_mechs/mech_krb5/include \
	-I$(SRC)/uts/common/gssapi/include  \
	-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
	-DHAVE_LIBSOCKET=1 -DHAVE_LIBNSL=1 -DHAVE_UNISTD_H=1 \
	-DHAVE_SYS_TIMEB_H=1 -DHAVE_ALLOCA_H=1 -DHAVE_FTIME=1 \
	 -DHAVE_TIMEZONE
COPTFLAG += -xs #-I$(KINCDIR) 

LDLIBS += -R $(KRUNPATH) $(KMECHLIB) $(ROOT)$(KERBLIBDIR)/libss.so \
	-L $(ROOT_KLIBDIR_GL) \
	-ldl -lsocket -lnsl -lc

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:

all: $(PROG)

ktutil:	$(OBJS) 
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all
	-$(RM) -rf $(KRB5BIN)/$(PROG)
	$(INS) -s -m $(DIRMODE) -d $(KRB5BIN)
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(KRB5BIN) $(PROG)

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
