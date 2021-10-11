#
#ident	"@(#)Makefile	1.7	94/08/25 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/oamuser/group/Makefile
#

include ../../Makefile.cmd

GROUPADD= 	groupadd
GROUPDEL= 	groupdel
GROUPMOD= 	groupmod
ADDGRPMEM= 	addgrpmem
GETPRIMARY= 	getprimary
FINDGID= 	findgid
#
#	Removing sysadm: Added SRCS to TXTS to continue NSE maintaince
#	of sources.
#
TXTS= 		$(SRCS)

SBINPROG=	$(GROUPADD)   $(GROUPDEL)   $(GROUPMOD)
SYSADMPROG=	$(ADDGRPMEM)  $(GETPRIMARY) $(FINDGID)
#
#	Disabling sysadm: deleted $(SYSADMPROG) from this target.
#
PROG=		$(SBINPROG)

ADD_OBJ=	groupadd.o    add_group.o   gid.o       messages.o
DEL_OBJ=	groupdel.o    del_group.o   messages.o
MOD_OBJ=	groupmod.o    mod_group.o   messages.o
GRPMEM_OBJ=	addgrpmem.o
GETPR_OBJ=	getprimary.o
FINDGID_OBJ=	findgid.o     gid.o

OBJECTS=	$(ADD_OBJ)    $(DEL_OBJ)    $(MOD_OBJ)  \
		$(GRPMEM_OBJ) $(GETPR_OBJ)  $(FINDGID_OBJ)
SRCS=		$(OBJECTS:.o=.c)

LIBDIR=		../lib
LIBUSRGRP=	$(LIBDIR)/lib.a
LOCAL=		../inc
HERE=		.
LINTFLAGS=	-u

ROOTUSRSADM=    $(ROOT)/usr/sadm
ROOTSYSADMBIN=  $(ROOTUSRSADM)/sysadm/bin
INSSBINPROG=	$(SBINPROG:%=$(ROOTUSRSBIN)/%)
INSSYSADMPROG=	$(SYSADMPROG:%=$(ROOTSYSADMBIN)/%)

CPPFLAGS=	-I$(HERE) -I$(LOCAL) $(CPPFLAGS.master)
FILEMODE= 	0555
OWNER= 		root
GROUP= 		sys

$(GROUPADD) :=	OBJS = $(ADD_OBJ)
$(GROUPADD) :=	LDLIBS += $(LIBUSRGRP)

$(GROUPDEL) :=	OBJS = $(DEL_OBJ)
$(GROUPDEL) :=	LDLIBS += $(LIBUSRGRP)

$(GROUPMOD) :=	OBJS = $(MOD_OBJ)
$(GROUPMOD) :=	LDLIBS += $(LIBUSRGRP)

$(ADDGRPMEM) :=	OBJS = $(GRPMEM_OBJ)
$(ADDGRPMEM) :=	LDLIBS += $(LIBUSRGRP) 

$(GETPRIMARY) :=OBJS = $(GETPR_OBJ)
$(GETPRIMARY) :=LDLIBS += $(LIBUSRGRP)

$(FINDGID) :=   OBJS = $(FINDGID_OBJ)

.KEEP_STATE:

all: 		$(PROG) $(TXTS)

$(PROG):	$$(OBJS) $(LIBUSRGRP)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(GROUPADD):	$(ADD_OBJ)
$(GROUPMOD):	$(MOD_OBJ)
$(GROUPDEL):	$(DEL_OBJ)
$(ADDGRPMEM):	$(GRPMEM_OBJ)
$(GETPRIMARY):	$(GETPR_OBJ)
$(FINDGID):	$(FINDGID_OBJ)

#
#	Disabling sysadm: deleted $(INSSYSADMPROG) from this target.
#
install:	all $(INSSBINPROG)

$(ROOTSYSADMBIN)/%: %
	$(INS.file)

clean:
	$(RM) $(OBJECTS)

lint:	lint_SRCS

include ../../Makefile.targ
