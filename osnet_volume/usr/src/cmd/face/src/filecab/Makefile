#
#ident	"@(#)Makefile	1.7	99/03/30 SMI"
#
# Copyright (c) 1990,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/face/src/filecab/Makefile

# include global definitions
include ../../../../Makefile.master

SUBDIRS = fileb

SFILES=		.faceprofile
WFILES=		WASTEBASKET/.pref
PFILES=		pref/.environ pref/.variables pref/.colorpref
STDFILES=	$(SFILES) $(WFILES) $(PFILES)
EXTERNFILES=	allobjs detect.tab
VMFILES=	pathalias
TERRLOG=	TERRLOG

APPLFILES=	$(STDFILES:%=standard/%) \
		$(EXTERNFILES:%=oasys/info/OH/externals/%) \
		$(VMFILES) $(TERRLOG)

# include common face definitions and install rules
include ../Makefile.com

ROOTDIRS=	$(ROOTSTD)/WASTEBASKET $(ROOTSTD)/pref \
		$(ROOTOASYS)/tmp

ROOTTERRLOG=	$(ROOTOASYS)/tmp/$(TERRLOG)
ROOTSTDF=	$(STDFILES:%=$(ROOTSTD)/%)
ROOTEXTERNF=	$(EXTERNFILES:%=$(ROOTEXTERN)/%)
ROOTVMSYSF=	$(VMFILES:%=$(ROOTVMSYS)/%)

DIRMODE=	755
FILEMODE=	644

# conditional assignments
#
all :=          TARGET = all
install :=      TARGET = install
clean :=        TARGET = clean
clobber :=      TARGET = clobber
lint :=         TARGET = lint

$(ROOTTERRLOG) :=	FILEMODE=	622
$(ROOTTERRLOG) :=	OWNER=		bin
# The following is necessary because make gets confused.
$(ROOTDIRS) :=		OWNER=		root

# installation rule
$(ROOTOASYS)/tmp/% : %
	$(INS.file)

.KEEP_STATE:

all: 	$(APPLFILES) $(SUBDIRS)

$(TERRLOG):
	$(RM) $@; touch $@

install: all .WAIT $(ROOTDIRS) .WAIT \
	$(ROOTTERRLOG) $(ROOTSTDF) $(ROOTEXTERNF) $(ROOTVMSYSF) $(SUBDIRS)

$(ROOTTERRLOG): $(ROOTDIRS) $(TERRLOG)

$(ROOTDIRS): $(ROOTSTD) $(ROOTOASYS) $(ROOTEXTERN) $(ROOTVMSYS)
	$(INS.dir)

clean clobber : $(SUBDIRS)
	$(RM) $(TERRLOG)

lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
