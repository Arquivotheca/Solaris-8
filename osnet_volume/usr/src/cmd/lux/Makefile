#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.17	99/10/17 SMI"
#
# cmd/lux/Makefile
#
#      luxadm (Administration program for the PHOTON) Makefile 
#

include	../Makefile.cmd

PROG		= luxadm
CPPFLAGS	+= -Ihdrs \
		-I$(SRC)/lib/storage/common/common/hdrs \
		-I$(SRC)/lib/storage/libg_fc/common/hdrs \
		-I$(SRC)/lib/storage/liba5k/common/hdrs \
		-DDEVCTL_LIB
LDLIBS 		+= -ldevinfo -ldevice -lkstat \
		-L$(ROOT)/usr/lib \
		-la5k -lg_fc -lthread
LINTFLAGS	+= -u
LINTLIBS	= $(LDLIBS.cmd)

OBJECTS		= adm.o ssa.o diag.o hotplug.o \
		envsen.o setboot.o fcupdate.o fcalupdate.o devctl.o \
		funct.o qlgcupdate.o

GENCAT		= gencat
GENMSG		= genmsg
GENMSGFLAGS	= -d
GENMSG.c	= $(GENMSG) $(GENMSGFLAGS) -p '$(COMPILE.cpp)'

GENCAT_PROG	= a5k_g_fc_i18n_cat

GENCAT_PROG_DIR	= $(SRC)/lib/storage/common

GENMSG_PROG	= $(PROG).msg

GENMSG_SRCS	= $(OBJECTS:%.o=%.c)

LINT_OBJS	= $(OBJECTS:%.o=%.ln)

.KEEP_STATE:

all debug browser debug_browser: $(PROG)

gencat		: $(GENCAT_PROG_DIR)/$(GENCAT_PROG)
$(GENCAT_PROG_DIR)/$(GENCAT_PROG): $(GENMSG_PROG)
	$(GENCAT) $@ $(GENMSG_PROG)

$(GENMSG_PROG):	$(GENMSG_SRCS)
	$(GENMSG.c) -o $@ $(GENMSG_SRCS)

$(PROG):	$(OBJECTS)
		$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
		$(POST_PROCESS)

debug	:= CFLAGS = -g
browser	:= CFLAGS = -xsb
debug_browser	:= CFLAGS = -xsb -g

lint: $(LINT_OBJS)
	$(LINT.c) $(LINTFLAGS) $(LINT_OBJS) $(LINTLIBS)

clean:
	$(RM) $(OBJECTS)
	$(RM) $(LINT_OBJS)
	$(RM) $(GENMSG_PROG)

CLOBBERFILES += $(GENCAT_PROG_DIR)/$(GENCAT_PROG)

FILEMODE=       644
DIRMODE=        755
GROUP=          bin
FIRMWARED=	$(ROOTLIB)/firmware
FCODED=         $(ROOTLIB)/firmware/fc_s
FCODE=          $(FCODED)/fcal_s_fcode
LUXFIRMWARED=	$(ROOTLIB)/locale/C/LC_MESSAGES
LUXIBFIRMWARE=	$(LUXFIRMWARED)/ibfirmware
LUXGENCAT_PROG=	$(LUXFIRMWARED)/$(GENCAT_PROG)

$(ROOTUSRSBINPROG) :=		FILEMODE=	555

install: all $(ROOTUSRSBINPROG) $(LUXIBFIRMWARE) $(FCODE) $(LUXGENCAT_PROG)

$(FIRMWARED):
	$(INS.dir)

$(FCODED):	$(FIRMWARED)
	$(INS.dir)

$(FCODE):	$(FCODED) fcal_s_fcode
	$(INS.file) fcal_s_fcode

$(LUXFIRMWARED):
	$(INS.dir)

$(LUXIBFIRMWARE):	$(LUXFIRMWARED) ibfirmware
	$(INS.file) ibfirmware	

$(LUXGENCAT_PROG):	$(LUXFIRMWARED) $(GENCAT_PROG_DIR)/$(GENCAT_PROG)
	$(INS.file) $(GENCAT_PROG_DIR)/$(GENCAT_PROG)	

include	../Makefile.targ
