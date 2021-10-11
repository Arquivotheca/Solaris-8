#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

#pragma	ident	"@(#)Makefile	1.18	00/09/14 SMI"

include ../../Makefile.cmd

AUTOCONFPROG=	llc2_autoconfig
CONFIGPROG=	llc2_config
LLC2UTILS=	llc2_stats llc2_loop2 llc2_loop3 llc2_loop4 llc2_tcap \
		llc2_tparser
PROG=		$(CONFIGPROG) $(AUTOCONFPROG) $(LLC2UTILS)
SRCS=		llc2_stats.c llc2_loop2.c llc2_loop3.c llc2_loop4.c \
		llc2_tcap.c llc2_tparser.c
OBJS=		llc2_autoconfig.o llc2_config.o llc2_conf.o $(SRCS:%.c=%.o)
AUTOCONFOBJS=	llc2_autoconfig.o llc2_conf.o
CONFIGOBJS=	llc2_config.o llc2_conf.o
AUTOCONFSRCS=	$(AUTOCONFOBJS:%.o=%.c)
CONFIGSRCS=	$(CONFIGOBJS:%.o=%.c)

POFILES = $(OBJS:%.o=%.po)

GENHEADERS=	/bin/sh ./genheaders
SRCTAG=		ild_ftbl.h dlpi_ftbl.h llc2_ftbl.h SAM_ftbl.h

CLOBBERFILES=	$(PROG) $(OBJS) $(SRCTAG)

INC_PATH += ../../../uts/common/io/llc2 -I../../../uts/common/sys

CPPFLAGS +=	-I$(INC_PATH) -DSTATIC=""  -DSOLARIS
LDLIBS += -lcmd

ROOTUSRLIBDIR	=	$(ROOTLIB)/llc2
ROOTUSRLIBPROG	=	$(PROG:%=$(ROOTLIB)/llc2/%)

$(AUTOCONFPROG)	:= LDLIBS += -ldevinfo

.KEEP_STATE:

all: $(PROG)

$(AUTOCONFPROG): $(AUTOCONFOBJS) llc2_conf.h
	$(LINK.c) $(AUTOCONFOBJS) -o $@ $(LDLIBS)

$(CONFIGPROG): $(CONFIGOBJS) llc2_conf.h
	$(LINK.c) $(CONFIGOBJS) -o $@ $(LDLIBS)

llc2_tparser: $(SRCTAG) llc2_tparser.o
	$(LINK.c) llc2_tparser.o -o $@ $(LDLIBS)


FILEMODE=	555

$(ROOTUSRLIBDIR):
	$(INS.dir)

$(ROOTLIB)/llc2/%: %
	$(INS.file)

install: $(PROG) $(ROOTUSRLIBDIR) $(ROOTUSRLIBPROG)

clobber:
	$(RM) $(CLOBBERFILES) $(POFILES)

clean: 
	$(RM) $(CLOBBERFILES) $(POFILES)

lint:	#$(SUBDIRS)
	$(LINT.c) $(AUTOCONFSRCS)
	$(LINT.c) $(CONFIGSRCS)
	$(LINT.c) $(SRCS)

# for messaging catalog
#
_msg: $(POFILES)
	cp $(POFILES) $(MSGDOMAIN)

