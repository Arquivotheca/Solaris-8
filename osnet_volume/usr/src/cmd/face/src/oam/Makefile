#
#ident	"@(#)Makefile	1.5	94/08/12 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# /usr/src/cmd/face/src/oam/Makefile

include		../../../Makefile.cmd

# include common face definitions and rules
include		../Makefile.com

SUBDIRS=	dos programs users

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

GROUP=		sys

APPLFILES=	Menu.FACE Menu.mail Menu.spell Menu.dos Menu.programs \
		Menu.users face.mi FACE.help

# Directories to install
ROOTDIRS=	$(ROOTFACE)/mail_services \
		$(ROOTFACE)/spell_checker \
		$(ROOTFACE)/dos_programs \
		$(ROOTFACE)/programs \
		$(ROOTFACE)/users

# Files to install
ROOTFILES=	$(ROOTINTF)/face.mi \
		$(ROOTFACE)/Menu.FACE \
		$(ROOTFACE)/FACE.help \
		$(ROOTFACE)/mail_services/Menu.mail \
		$(ROOTFACE)/spell_checker/Menu.spell \
		$(ROOTFACE)/dos_programs/Menu.dos \
		$(ROOTFACE)/programs/Menu.programs \
		$(ROOTFACE)/users/Menu.users

FILEMODE=	755

$(ROOTDIRS):
	-$(INS.dir)

$(ROOTINTF)/face.mi :=		FILEMODE=	644

# local installation rules
#
$(ROOTFACE)/mail_services/% \
$(ROOTFACE)/spell_checker/% \
$(ROOTFACE)/dos_programs/% \
$(ROOTFACE)/programs/% \
$(ROOTFACE)/users/% : %
	$(INS.file)

.KEEP_STATE:

all: $(APPLFILES) $(SUBDIRS)

install: all $(ROOTDIRS) $(ROOTFILES) $(SUBDIRS)

clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

