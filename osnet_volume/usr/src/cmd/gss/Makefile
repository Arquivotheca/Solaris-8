#
# Copyright (c) 1997 - 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	99/12/11 SMI"
#
# cmd/gss/Makefile
#
# include global definitions
include ../../Makefile.master

COMMON_SUBDIRS=		\
	etc		\
	gssd		\
	gsscred		\
	gsscred_clean

i386_SUBDIRS=

sparc_SUBDIRS=

#
# commands that are messaged
# note that 'lp' comes first (see previous comment about 'lp')
#
MSGSUBDIRS= gssd gsscred

#
# commands that use dcgettext for localized time, LC_TIME
#
DCSUBDIRS=

#
# commands that belong only to the basic security module
#
BSMSUBDIRS=

#
# commands not owned by the systems group
#
BWOSDIRS= 

all :=		TARGET= all
install :=	TARGET= install
install_h :=	TARGET= install_h
check :=	TARGET= check
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg
_dc :=		TARGET= _dc

.KEEP_STATE:

SUBDIRS = $(COMMON_SUBDIRS)  $($(MACH)_SUBDIRS)

.PARALLEL:	$(BWOSDIRS) $(SUBDIRS) $(MSGSUBDIRS)  $(BSMSUBDIRS)

all install clean clobber lint: $(SUBDIRS) $(BSMSUBDIRS)

# those commands which install headers possibly needed by other commands.
install_h check:

_msg: $(MSGSUBDIRS) _dc

_dc: $(DCSUBDIRS)

$(BWOSDIRS) $(SUBDIRS) $(BSMSUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
