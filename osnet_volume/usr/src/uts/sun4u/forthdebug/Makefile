#
# uts/sun4u/forthdebug/Makefile
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident   "@(#)Makefile 1.44     99/10/07 SMI"
#
#       This makefile drives the production of the forthdebug kernel file
#
#       sun4u implementation architecture dependent
#

#
#       Path to the base of the uts directory tree (usually /usr/src/uts).
#

UTSBASE = ../..

#
#       Define the module and object file sets.
#

FORTHDEBUG	= forthdebug
FDBG_MODULE	= $(OBJS_DIR)/$(FORTHDEBUG)
ROOTMODULE      = $(ROOT_PSM_MISC_DIR)/$(FORTHDEBUG)

FTH_FILES 	= \
	common.fth	scsi.fth	sfmmu.fth 	fas.fth \
	esp.fth		isp.fth		sd.fth		sparc.fth \
	st.fth 		sun4u.fth 	glm.fth		inet.fth \
	ufs.fth		cachefs.fth	uata.fth	dad.fth \
	ohci.fth	usbms.fth	hubd.fth	usb_mid.fth \
	ifp.fth		hid.fth		usba.fth	spitfire.fth \
	usbkbm.fth	sunddi.fth	pci.fth 	udfs.fth \
	fcimpl.fth	fcp.fth		usoc.fth	scsa2usb.fth

FILES		= $(FTH_FILES:%=$(OBJS_DIR)/%)
KDBG		= kdbg.fth
FDBG_TMP	= $(OBJS_DIR)/fth.tmp
TMP_C		= $(OBJS_DIR)/$(*F).c
TMP_S		= $(OBJS_DIR)/$(*F).s
TMP_TMP		= $(OBJS_DIR)/$(*F).tmp

MODELFILE_32	= data32.fth
MODELFILE_64	= data64.fth
MODELFILE	= data$(CLASS).fth

#
#       Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

#
#	Override defaults
#
CLEANFILES	= $(FILES) $(FDBG_TMP) Nothing_to_remove
CLOBBERFILES	= $(FDBG_MODULE) $(CLEANFILES) Nothing_to_remove

#
#	Define targets
#
ALL_TARGET	= $(FDBG_MODULE)
INSTALL_TARGET	= $(FDBG_MODULE) $(ROOTMODULE)

#
#	Default build targets
#
.KEEP_STATE:

.SUFFIXES: .o .c .fdbg .fth

.PARALLEL:	$(FTH_FILES)

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:

modlintlib:

clean.lint:

install:	$(INSTALL_DEPS)

$(FDBG_MODULE):	$(MODELFILE) $(KDBG) $(FILES)
	cat $(MODELFILE) $(KDBG) $(FILES) > $(FDBG_TMP)
	@wc -c $(FDBG_TMP) | \
		awk '{size = $$1 + 32; printf("\ %d\n", size)}' > $@
	cat $(FDBG_TMP) >> $@
	@rm $(FDBG_TMP)

$(OBJS_DIR)/%.fth:	%.fdbg
	@grep "^#" $< > $(TMP_C)
	$(CC) $(CFLAGS) $(CPPFLAGS) -g -S -o $(TMP_S) $(TMP_C)
	@grep -v "^#" $< > $(TMP_TMP)
	-$(STABS) -t forth -m $(MODEL) $(TMP_TMP) < $(TMP_S) > $@
	@rm $(TMP_C) $(TMP_S) $(TMP_TMP)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
