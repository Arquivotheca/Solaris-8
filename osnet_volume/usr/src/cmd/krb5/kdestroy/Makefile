#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.3	00/09/14 SMI"
#
# cmd/krb5/kdestroy/Makefile

PROG= kdestroy

OUTPUT_OPTION = -I. 

OBJS	= kdestroy.o kwarnd_clnt_stubs.o kwarnd_clnt.o kwarnd_handle.o kwarnd_xdr.o krpc_sys.o

SRCS	= $(OBJS:.o=.c)

include ../../Makefile.cmd
include $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5

TEXT_DOMAIN = SUNW_OST_OSCMD
POFILE = kdestroy.po 
POFILES = generic.po 

CPPFLAGS += -I../../../lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
		-I../kwarn

i386_CPPFLAGS = -DDSHLIB
sparc_CPPFLAGS += -D$(MACH)

LDLIBS += -R $(KRUNPATH)
LDLIBS += $(KMECHLIB) -lnsl -lmp -lc -ldl 

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:
all: $(PROG)

sparc_KPIC = -I$(SRC)/lib/libc/inc -I$(SRC)/lib/libc/$(MACH)/inc \
		-DPIC -D_TS_ERRNO -K pic
i386_KPIC =
KPIC = $($(MACH)_KPIC)

kdestroy:       $(OBJS)  
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

