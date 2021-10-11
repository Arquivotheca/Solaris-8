#
#ident	"@(#)Makefile	1.26	95/02/25 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/asppp/aspppd/Makefile
#

include ../../Makefile.cmd

CSTYLE.c =	/opt/teamware/bin/cstyle

LIBRARY =	../libconn/libconn.a

SBINPROG =	aspppd

BNU = ../../bnu

LIBS =		$(LIBRARY) # later $(LINTLIB)

CLOBBERFILES =	$(SBINPROG)
CLEANFILES =	$(OBJECTS)

OBJECTS = aspppd.o auth.o call.o diag.o fds.o fifo.o ifconfig.o iflist.o \
	  ip.o ipd.o log.o parse.o path.o ppp.o route.o uucp_glue.o

SRCS = $(OBJECTS:%.o=%.c)

HDRS = aspppd.h call.h diag.h fds.h fifo.h ifconfig.h iflist.h ip.h ipd.h \
       ipd_ioctl.h log.h parse.h path.h ppp.h ppp_ioctl.h route.h uucp_glue.h

SOURCES = $(SRCS) $(HDRS)

# installed modules
ROOTUSRSBINPROG =	$(SBINPROG:%=$(ROOTUSRSBIN)/%)

CPPFLAGS += -I. -I$(BNU) 

LDLIBS +=	$(LIBRARY) -lnsl -lsocket

# conditional assignments
#
all:=		TARGET = all
install:=	TARGET = install
clean:=		TARGET = clean
clobber:=	TARGET = clobber
lint:=		TARGET = lint
cstyle:=	TARGET = cstyle
debug:=		CFLAGS = -g

# file modes, owners and groups for the install target
#

$(ROOTUSRSBINPROG) :=	FILEMODE = 	0555
$(ROOTUSRSBINPROG) :=	OWNER = root
$(ROOTUSRSBINPROG) :=	GROUP =	sys

.KEEP_STATE:

.INIT: $(HDRS)

all:  $(SBINPROG) 

$(SBINPROG): $(OBJECTS) $(LIBRARY)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

install: $(ROOTUSRSBINPROG)

lint: 
	$(LINT.c) $(SRCS)

cstyle:
	$(CSTYLE.c) $(SOURCES)

debug:	$(SBINPROG)

clean:
	rm -f $(OBJECTS) *~ ,*

clobber: clean
	rm -f $(SBINPROG)

sources: $(SOURCES)


