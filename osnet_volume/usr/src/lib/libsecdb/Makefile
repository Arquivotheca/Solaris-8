#
# Copyright (c) 1999, by Sun Microsystems, Inc. All rights reserved.
#
# lib/libsecdb/Makefile
#
#ident	"@(#)Makefile	1.2	99/07/16 SMI"
#

include ../Makefile.lib

SUBDIRS=	spec .WAIT help $(MACH) $(BUILD64) $(MACH64)

all:=		TARGET= all
clean:=		TARGET= clean
clobber:=	TARGET= clobber
install:=	TARGET= install
lint:=		TARGET= lint

.KEEP_STATE:

ROOTETC = 		$(ROOT)/etc
ROOTETCSECURITY =	$(ROOTETC)/security
$(ROOTETCSECURITY) :=	DIRMODE = 0755
$(ROOTETCSECURITY) :=	OWNER = root
$(ROOTETCSECURITY) :=	GROUP = sys

EFILES =	user_attr
EFILESRC =	$(EFILES:%=%)
ETCFILES =	$(EFILES:%=$(ROOTETC)/%)
$(ETCFILES) :=	FILEMODE = 0644
$(ETCFILES) :=	OWNER = root
$(ETCFILES) :=	GROUP = sys

ESFILES =		auth_attr exec_attr prof_attr policy.conf
ESSRC =			$(ESFILES:%=%)
ETCSECURITYFILES =	$(ESFILES:%=$(ROOTETCSECURITY)/%)
$(ETCSECURITYFILES) :=	FILEMODE = 0644
$(ETCSECURITYFILES) :=	OWNER = root
$(ETCSECURITYFILES) :=	GROUP = sys

all clean clobber delete install lint package: $(SUBDIRS)

install:	install_data

install_data:	$(ETCSECURITYFILES) $(ETCFILES)

$(ETCSECURITYFILES) $(ETCFILES): $(ETCSECURITY) $(ROOTETCSECURITY)

$(ETCSECURITY)/%: %
	$(INS.file)

$(ROOTETCSECURITY):
	$(INS.dir)

$(ROOTETCSECURITY)/%: %
	$(INS.file)

$(ROOTETC)/% : %
	$(INS.file)

spec help $(MACH) $(MACH64) :	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
