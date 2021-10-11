#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# cmd/krb5/klist/Makefile

PROG= klist

OUTPUT_OPTION = -I. 

OBJS	= klist.o                       
SRCS	= $(OBJS:.o=.c)
RSRC	= klist.c 

# CLOBBERFILES += $(RSRC) 

include ../../Makefile.cmd
include $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5

TEXT_DOMAIN = SUNW_OST_OSCMD
POFILE = klist.po
POFILES = generic.po

CPPFLAGS += -I$(SRC)/lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/uts/common/gssapi/mechs/krb5/include

LDLIBS += -R $(KRUNPATH)
LDLIBS += $(KMECHLIB) -lnsl -lmp -lc -ldl

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:

all: $(PROG)

klist:       $(OBJS)  
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

include ../../Makefile.targ

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES) 
	$(RM) $@ 
	$(CAT) $(POFILES) > $@ 

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `($(GREP) -l gettext *.[ch] || echo /dev/null)`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po
