#
#ident	"@(#)Makefile	1.19	99/10/26 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/asppp/asppp/Makefile
#

include ../../Makefile.cmd

INCLUDES =		-I../aspppd 

CPPFLAGS +=		$(INCLUDES)
 
OBJECTS =		aspppls.o

SRCS =			$(OBJECTS:%.o=%.c)

HDRS =			../aspppd/fifo.h

SOURCES =		$(SRCS) $(HDRS)

SBINPROG =		aspppls

ROOTUSRSBINPROG =	$(SBINPROG:%=$(ROOTUSRSBIN)/%)

CSTYLE.c =		/opt/teamware/bin/cstyle

debug:=			CFLAGS = -g

# file modes, owners and groups for the install target
#

$(ROOTUSRSBINPROG) :=	FILEMODE = 	04555
$(ROOTUSRSBINPROG) :=	OWNER = root
$(ROOTUSRSBINPROG) :=	GROUP =	bin

.KEEP_STATE:

.INIT: $(HDRS)

all: $(SBINPROG)

$(SBINPROG): $(OBJECTS)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

install: $(ROOTUSRSBINPROG)

$(ROOTUSRSBINPROG): $(SBINPROG)	

debug: $(SBINPROG)

clean:
	rm -f $(OBJECTS) *~ ,*

clobber: clean
	rm -f aspppls

sources: $(SOURCES)

lint:
	$(LINT.c) $(SOURCES)

cstyle:
	$(CSTYLE.c) $(SOURCES)
