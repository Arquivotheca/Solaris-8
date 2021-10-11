#
# Copyright (c) 1990,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.10	99/03/30 SMI"
#
# cmd/dispadmin/Makefile
#

PROG= dispadmin
RT=   RT$(PROG)
TS=   TS$(PROG)
IA=   IA$(PROG)
PROGS= $(PROG) $(RT) $(TS) $(IA)

include ../Makefile.cmd


ROOTDIR=	$(ROOT)/usr/lib/class
ROOTDIRS=	$(ROOTDIR) $(ROOTDIR)/RT $(ROOTDIR)/TS $(ROOTDIR)/IA
ROOTPROG=	$(PROG:%=$(ROOTUSRSBIN)/%)
ROOTRT=		$(RT:%=$(ROOTDIR)/RT/%)
ROOTTS=		$(TS:%=$(ROOTDIR)/TS/%)
ROOTIA=		$(IA:%=$(ROOTDIR)/IA/%)

GROUP = bin

# this would be simpler if we renamed rtdispadmin.c and tsdispadmin.c
OBJECTS= $(PROG).o rt$(PROG).o ts$(PROG).o ia$(PROG) subr.o

# conditional assignments, because of above names
$(PROG):=	OBJ= $(PROG).o
$(RT):=		OBJ= rt$(PROG).o
$(TS):=		OBJ= ts$(PROG).o
$(IA):=		OBJ= ia$(PROG).o

# install rules
$(ROOTDIR)/% \
$(ROOTDIR)/RT/% \
$(ROOTDIR)/IA/% \
$(ROOTDIR)/TS/% : %
	$(INS.file)

.KEEP_STATE:
 
all: $(PROGS) 

$(PROGS): $$(OBJ) subr.o
	$(LINK.c) -o $@ $(OBJ) subr.o $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG) $(ROOTRT) $(ROOTTS) $(ROOTIA)

# Don't re-install directories already installed by Targetdirs
#$(ROOTDIRS):
#	$(INS.dir)

clean:
	$(RM) $(OBJECTS)
 
lint:   lint_SRCS
 
include ../Makefile.targ
