#
#ident	"@(#)Makefile	1.1	91/07/26 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
# cmd/aset/Makefile

PROG= aset.restore aset asetenv

FILES= tune.high tune.low tune.med uid_aliases

MASTERSFILES= $(FILES:%=masters/%)

SUBDIRS = util tasks

include ../Makefile.cmd

DIRMODE= 700
FILEMODE= 700
OWNER= root

# macros used by install
ROOTASET= $(ROOT)/usr/aset
ASETDIRS= archives masters reports tasks tmp util
ROOTDIRS= $(ROOTASET) $(ASETDIRS:%=$(ROOTASET)/%)
ROOTFILES= $(FILES:%=$(ROOTASET)/masters/%)
ROOTPROG= $(PROG:%=$(ROOTASET)/%)

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

$(ROOTFILES) := FILEMODE= 600

# install rules
$(ROOTASET)/masters/%: masters/%
	$(INS.file)

$(ROOTASET)/%: %
	$(INS.file)

.KEEP_STATE:

all: $(PROG) $(MASTERSFILES) $(SUBDIRS)

clean lint: $(SUBDIRS)

install: $(PROG) $(ROOTDIRS) $(ROOTPROG) $(ROOTFILES) $(SUBDIRS)

clobber: $(SUBDIRS)
	$(RM) $(PROG)

$(ROOTDIRS):
	$(INS.dir)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
