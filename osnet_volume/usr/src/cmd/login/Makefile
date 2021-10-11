#
#ident	"@(#)Makefile	1.22	98/05/28 SMI"
#
# Copyright (c) 1989, 1997 by Sun Microsystems, Inc.
#

PROG= login
OBJS= login.o
SRCS= $(OBJS:%.o=%.c)
ONC_SRCS=$(SRCS:%.c=%.c_onc_plus)

include ../Makefile.cmd

DFLTD= $(ROOTETC)/default
DIRS= $(DFLTD)
LOGINDEVPERM= logindevperm
LOGINDEVPERMSRC= $(LOGINDEVPERM).sh
ROOTLOGINDEVPERM= $(LOGINDEVPERM:%=$(ROOTETC)/%)

ETCDFLTPROG = $(PROG:%=$(DFLTD)/%)
$(ETCDFLTPROG) := FILEMODE = $(LIBFILEMODE)
$(ETCDFLTPROG) := GROUP = sys
$(ROOTLOGINDEVPERM) := FILEMODE = 644
$(ROOTLOGINDEVPERM) := GROUP = sys
FILEMODE= 4555
OWNER= root

CLOBBERFILES +=	$(LOGINDEVPERM) $(ONC_SRCS)

CPPFLAGS += -DSYSV -DCONSOLE='"/dev/console"' -DSECURITY -I../../lib/libcmd/inc

# libsocket is included for the ruserok() function.
LDLIBS += -lcmd -lcrypt_i -lbsm -lsocket -lnsl -ldl -lpam
CFLAGS += -v

.KEEP_STATE:


all: $(PROG) $(PROG).dfl $(ROOTLOGINDEVPERM)

$(LOGINDEVPERM): $(LOGINDEVPERMSRC)
	$(RM) $(LOGINDEVPERM)
	/bin/sh $(LOGINDEVPERMSRC) > $(LOGINDEVPERM)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(DIRS) $(ROOTPROG) $(PROG).dfl $(ETCDFLTPROG) $(ROOTLOGINDEVPERM)

$(DIRS):
	$(INS.dir)

$(DFLTD)/% : %
	$(MV) $(PROG) $(PROG).save
	$(LN) $(PROG).dfl $(PROG)
	$(INS.file)
	$(RM) $(PROG)
	$(MV) $(PROG).save $(PROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../Makefile.targ

# make ONC_PLUS using suffix rule
#

ONC_PLUS: $(ONC_SRCS)
