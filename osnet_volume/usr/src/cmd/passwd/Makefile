#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.21	99/07/07 SMI"
#
# cmd/passwd/Makefile
#

PROG= passwd
TXTS= $(PROG).dfl
YPPASSWD= yppasswd
NISPASSWD= nispasswd

include ../Makefile.cmd

DFLTD= $(ROOTETC)/default
DIRS= $(DFLTD)

ETCDFLTPROG = $(PROG:%=$(DFLTD)/%)
$(ETCDFLTPROG) := FILEMODE = $(LIBFILEMODE)

# libunixpasswd.a is built in usr/lib/pam_modules/unix
LDLIBS += -L$(ROOT)/usr/lib/security -lunixpasswd
LDLIBS += -lcmd -lcrypt_i -lbsm -ldl -lpam -lnsl -lsldap -lsocket
LDLIBS += -lmp

FILEMODE = 06555
OWNER = root
GROUP = sys
TMPDIR = ./tmp
XGETFLAGS += -a -x $(PROG).xcl

.KEEP_STATE:

all: $(PROG) $(TXTS) $(YPPASSWD) $(NISPASSWD)

$(YPPASSWD):	$(PROG)
	@$(RM) $(YPPASSWD); $(LN) $(PROG) $(YPPASSWD)

$(NISPASSWD):	$(PROG)
	@$(RM) $(NISPASSWD); $(LN) $(PROG) $(NISPASSWD)

install: all $(DIRS) $(ROOTPROG) $(PROG).dfl $(ETCDFLTPROG)
	$(RM) $(ROOTBIN)/$(YPPASSWD)
	$(RM) $(ROOTBIN)/$(NISPASSWD)
	$(LN) $(ROOTBIN)/$(PROG) $(ROOTBIN)/$(YPPASSWD)
	$(LN) $(ROOTBIN)/$(PROG) $(ROOTBIN)/$(NISPASSWD)


$(DIRS):
	$(INS.dir)

$(DFLTD)/% : %
	$(MV) $(PROG) $(PROG).save
	$(LN) $(PROG).dfl $(PROG)
	$(INS.file)
	$(RM) $(PROG)
	$(MV) $(PROG).save $(PROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
