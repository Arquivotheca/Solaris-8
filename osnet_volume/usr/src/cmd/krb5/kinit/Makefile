#
# Copyright (c) 1997-1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.2	00/09/14 SMI"
#
# cmd/krb5/kinit/Makefile

PROG= kinit

OUTPUT_OPTION = -I. 

OBJS	= kinit.o kwarnd_clnt_stubs.o kwarnd_clnt.o kwarnd_handle.o kwarnd_xdr.o
SRCS	= $(OBJS:.o=.c)

DEFS =  -DHAVE_LIBSOCKET=1 -DHAVE_LIBNSL=1 -DHAVE_PWD_H=1 -DHAVE_COMPILE=1 -DHAVE_STEP=1 

include ../../Makefile.cmd
include $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5

TEXT_DOMAIN = SUNW_OST_OSCMD
POFILE = kinit.po
POFILES = generic.po

CPPFLAGS += -I../../lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/lib/gss_mechs/mech_krb5 \
		-I$(SRC)/lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
		-I../kwarn

LDLIBS += -R $(KRUNPATH)
LDLIBS += $(KMECHLIB) -lnsl -lmp -lc -ldl -lresolv -lintl

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:

all: $(PROG)

kinit:       $(OBJS)  
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

%.o: ../kwarn/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

install: all
	-$(RM) -rf $(KRB5BIN)/$(PROG)
	$(INS) -s -m $(DIRMODE) -d $(KRB5BIN)
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(KRB5BIN) $(PROG)

install_h:

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../../Makefile.targ

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES) 
	$(RM) $@ 
	$(CAT) $(POFILES) > $@ 

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `($(GREP) -l gettext *.[ch] || echo /dev/null)`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po
