#
# uts/sparc/warlock/Makefile
#
#pragma ident	"@(#)Makefile	1.8	99/10/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#	This makefile drives the production of the fas driver kernel module.
#
#	sparc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE = ../..

#
#	Define the module and object file sets.
#
MODULE		= warlock
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

.KEEP_STATE:

CFLAGS += -I../../common/sys -I../../sun/sys/scsi -D_KERNEL
#
#	Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
TOUCH		= touch
SCCS		= sccs
TEST		= test

#
#	lock_lint rules
#
warlock: $(MODULE).ok 


warlock.ok: ddi_dki_impl.ll scsi.ll
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/warlock/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -DDEBUG -o $@ $<


.NO_PARALLEL:
warlock.usb:
	@cd ../usba; rm -f *.ll *.ok; make warlock
	@cd ../ohci; rm -f *.ll *.ok; make warlock
	@cd ../usb_mid; rm -f *.ll *.ok; make warlock
	@cd ../hid; rm -f *.ll *.ok; make warlock
	@cd ../scsa2usb; rm -f *.ll *.ok; make warlock


.NO_PARALLEL:
warlock.scsi:
	@cd ../dad; rm -f *.ll *.ok; make warlock
	@cd ../dada; rm -f *.ll *.ok; make warlock
	@cd ../esp; rm -f *.ll *.ok; make warlock
	@cd ../fas; rm -f *.ll *.ok; make warlock
	@cd ../glm; rm -f *.ll *.ok; make warlock
	@cd ../ifp; rm -f *.ll *.ok; make warlock
	@cd ../isp; rm -f *.ll *.ok; make warlock
	@cd ../pln; rm -f *.ll *.ok; make warlock
	@cd ../sd; rm -f *.ll *.ok; make warlock
	@cd ../ses; rm -f *.ll *.ok; make warlock
	@cd ../st; rm -f *.ll *.ok; make warlock
	@cd ../ssd; rm -f *.ll *.ok; make warlock
	@cd ../soc; rm -f *.ll *.ok; make warlock
	@cd ../uata; rm -f *.ll *.ok; make warlock
