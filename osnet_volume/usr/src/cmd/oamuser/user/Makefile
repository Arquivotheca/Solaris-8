#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
#
#"@(#)Makefile	1.14	99/04/08 SMI"
#
# cmd/oamuser/user/Makefile
#

include ../../Makefile.cmd

GREP=		grep

USERADD= 	useradd
USERDEL= 	userdel
USERMOD= 	usermod
GETUSRDEFS= 	getusrdefs
PUTUSRDEFS= 	putusrdefs
GETNGROUPS= 	getngroups
FINDUID=	finduid
EXPDATE=	expdate
PROFILE=	.profile
STDPROFILE=	stdprofile
ROLEADD=	roleadd
ROLEDEL=	roledel
ROLEMOD=	rolemod

#
#	Removing sysadm: Added SRCS to TXTS to continue NSE maintaince
#	of sources.
#
TXTS=		$(STDPROFILE) $(SRCS)

SBINPROG=	$(USERADD)   $(USERDEL)   $(USERMOD)
SYSADMPROG=	$(GETUSRDEFS)  $(PUTUSRDEFS) $(GETNGROUPS) $(EXPDATE) $(FINDUID)
SKELFILE=	$(PROFILE)
#
#	Removing sysadm: deleted $(SYSADMPROG) from this target.
#
PROG=		$(SBINPROG)
PRODUCT=	$(PROG) $(PROFILE)

ADD_OBJ=	useradd.o     putusrdefs.o     uid.o        homedir.o \
		groups.o      call_pass.o      userdefs.o   messages.o \
		val_lgrp.o    funcs.o

DEL_OBJ=	userdel.o     call_pass.o      rmfiles.o    isbusy.o \
		groups.o      messages.o       funcs.o

MOD_OBJ=	usermod.o     uid.o            movedir.o    groups.o \
		rmfiles.o     call_pass.o      isbusy.o     homedir.o \
		userdefs.o    messages.o       val_lgrp.o   funcs.o

GETDEFS_OBJ=	getdefs.o     userdefs.o
PUTDEFS_OBJ=	putdefs.o     putusrdefs.o     userdefs.o   messages.o
GETNGR_OBJ=	getngroups.o
FINDUIDOBJ=	finduid.o     uid.o
EXPDOBJ=	expdate.o

OBJECTS=	$(ADD_OBJ)     $(DEL_OBJ)    $(MOD_OBJ)    $(GETDEFS_OBJ) \
		$(PUTDEFS_OBJ) $(GETNGR_OBJ) $(FINDUIDOBJ) $(EXPDOBJ)

SRCS=		$(OBJECTS:.o=.c)

CLOBBERFILES=	$(PROFILE)
LIBDIR=		../lib
LIBUSRGRP=	$(LIBDIR)/lib.a
LIBADM=		-ladm
LOCAL=		../inc
HERE=		.
LINTFLAGS=	-u

ROOTUSRSADM=    $(ROOT)/usr/sadm
ROOTSYSADMBIN=  $(ROOTUSRSADM)/sysadm/bin
ROOTSKEL=	$(ROOTETC)/skel
INSSBINPROG=	$(SBINPROG:%=$(ROOTUSRSBIN)/%)
INSSYSADMPROG=	$(SYSADMPROG:%=$(ROOTSYSADMBIN)/%)
INSSKELFILE=	$(SKELFILE:%=$(ROOTSKEL)/%)

CPPFLAGS=	-I$(HERE) -I$(LOCAL) $(CPPFLAGS.master)
OWNER= 		root
GROUP= 		sys

$(INSSBINPROG)  := FILEMODE = 0555
$(INSSYSADMPROG):= FILEMODE = 0500
$(INSSKELFILE)  := FILEMODE = 0644
$(INSSKELFILE)  := GROUP = other
$(ROOTSKEL)	:= OWNER = root
$(ROOTSKEL)	:= GROUP = sys

$(USERADD) :=	OBJS = $(ADD_OBJ)
$(USERADD) :=	LIBS = $(LIBUSRGRP)

$(USERDEL) :=	OBJS = $(DEL_OBJ)
$(USERDEL) :=	LIBS = $(LIBUSRGRP)

$(USERMOD) :=	OBJS = $(MOD_OBJ)
$(USERMOD) :=	LIBS = $(LIBUSRGRP)

$(GETUSRDEFS) :=OBJS = $(GETDEFS_OBJ)
$(GETUSRDEFS) :=LIBS = $(LIBUSRGRP)

$(PUTUSRDEFS) :=OBJS = $(PUTDEFS_OBJ)
$(PUTUSRDEFS) :=LIBS = $(LIBUSRGRP)

$(GETNGROUPS) :=OBJS = $(GETNGR_OBJ)
$(GETNGROUPS) :=LIBS = $(LIBUSRGRP)

$(EXPDATE) :=	OBJS = $(EXPDOBJ)
$(EXPDATE) :=	LIBS = $(LIBUSRGRP)

$(FINDUID) :=   OBJS = $(FINDUIDOBJ)
$(FINDUID) :=	LIBS =

LDLIBS +=	-lsecdb

.KEEP_STATE:

.PARALLEL: $(OBJECTS)

all: 		$(PRODUCT) $(TXTS)

$(PROFILE):    $(STDPROFILE)
	$(GREP) -v "^#ident" $(STDPROFILE) > $(PROFILE)

$(PROG):	$$(OBJS) $$(LIBS)
	$(LINK.c) $(OBJS) -o $@ $(LIBS) $(LDLIBS)
	$(POST_PROCESS)

$(USERADD):	$(ADD_OBJ)
$(USERMOD):	$(MOD_OBJ)
$(USERDEL):	$(DEL_OBJ)
$(GETUSRDEFS):	$(GETDEFS_OBJ)
$(PUTUSRDEFS):	$(PUTDEFS_OBJ)
$(GETNGROUPS):	$(GETNGR_OBJ)
$(EXPDATE):	$(EXPDOBJ)
$(FINDUID):	$(FINDUIDOBJ)

#
#	Removing sysadm: deleted $(INSSYSADMPROG) from this target.
#
install:	all .WAIT $(ROOTSKEL) $(INSSBINPROG) $(INSSKELFILE)
		$(RM) $(ROOTUSRSBIN)/$(ROLEADD)
		$(LN) $(ROOTUSRSBIN)/$(USERADD) $(ROOTUSRSBIN)/$(ROLEADD)
		$(RM) $(ROOTUSRSBIN)/$(ROLEDEL)
		$(LN) $(ROOTUSRSBIN)/$(USERDEL) $(ROOTUSRSBIN)/$(ROLEDEL)
		$(RM) $(ROOTUSRSBIN)/$(ROLEMOD)
		$(LN) $(ROOTUSRSBIN)/$(USERMOD) $(ROOTUSRSBIN)/$(ROLEMOD)

$(ROOTSYSADMBIN)/%: %
	$(INS.file)

$(ROOTSKEL):
	$(INS.dir)

$(ROOTSKEL)/%:	%
	$(INS.file)

clean:
	$(RM) $(OBJECTS)

lint:	lint_SRCS

include ../../Makefile.targ
