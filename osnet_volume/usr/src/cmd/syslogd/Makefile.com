#
#ident	"@(#)Makefile.com	1.3	99/03/30 SMI"
#
# Copyright (c) 1991-1993, 1997, 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/syslogd/Makefile.com
#

PROG= 		syslogd
ROTATESCRIPT=	newsyslog
CONFIGFILE=	syslog.conf
TXTS= 		syslog.conf
PRODUCT=	$(PROG) $(ROTATESCRIPT)
OBJS=		syslogd.o queue.o list.o conf.o
SRCS=		$(OBJS:%.o=../%.c)
LLOBJS=		$(OBJS:%.o=%.ll)

include ../../Makefile.cmd

$(PROG) 	:= LDLIBS += -lnsl -lpthread -ldoor
CCVERBOSE	=
CPPFLAGS	+= -D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT
CPPFLAGS	+= -DNDEBUG

VARSYSLOG=	syslog
VARAUTHLOG=	authlog
ROOTVARLOGD=	$(ROOT)/var/log

OWNER=		root
GROUP=		sys

ROOTETCCONFIG=	$(CONFIGFILE:%=$(ROOTETC)/%)
ROOTLIBROTATE=	$(ROTATESCRIPT:%=$(ROOTLIB)/%)
ROOTVARSYSLOG=	$(VARSYSLOG:%=$(ROOTVARLOGD)/%)
ROOTVARAUTHLOG=	$(VARAUTHLOG:%=$(ROOTVARLOGD)/%)

$(ROOTUSRSBINPROG) 	:= FILEMODE = 0555
$(ROOTUSRLIBROTATE)	:= FILEMODE = 0555
$(ROOTETCCONFIG)	:= FILEMODE = 0644
$(ROOTVARSYSLOG)	:= FILEMODE = 0644
$(ROOTVARAUTHLOG)	:= FILEMODE = 0600

$(ROOTVARLOGD)/% : %
	$(INS.file)

$(ROOTETC)/%:	../%
	$(INS.file)

$(ROOTLIB)/%:	../%
	$(INS.file)

.KEEP_STATE:

.SUFFIXES:	$(SUFFIXES) .ll

.c.ll:
	$(CC) $(CFLAGS) $(CPPFLAGS) -Zll -o $@ $<

.PARALLEL: $(OBJS)


$(VARSYSLOG) $(VARAUTHLOG):
	$(ECHO) '\c' > $@

%.o: ../%.c
	$(COMPILE.c) $<

%.ll: ../%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -Zll -o $@ $<

syslogd: $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

logfiles: $(ROOTVARSYSLOG) $(ROOTVARAUTHLOG)

clean:
	$(RM) $(OBJS) $(LLOBJS) $(VARSYSLOG) $(VARAUTHLOG)

lint:	lint_SRCS

lock_lint:	$(LLOBJS)

include ../../Makefile.targ
