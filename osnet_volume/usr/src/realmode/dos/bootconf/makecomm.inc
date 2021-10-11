#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# common makefile included by DOS and UNIX makefiles
#
#ident	"@(#)makecomm.inc	1.62	99/10/07 SMI"
#
OBJS =\
	80421275.obj\
	acpi_rm.obj\
	adv.obj\
	aliases.obj\
	asm_util.obj\
	befext.obj\
	befinst.obj\
	bios.obj\
	biosprim.obj\
	boards.obj\
	boot.obj\
	bop.obj\
	bus.obj\
	cfname.obj\
	config.obj\
	debug.obj\
	devdb.obj\
	dir.obj\
	eisa.obj\
	enum.obj\
	eprintf.obj\
	err.obj\
	escd.obj\
	fprintf.obj\
	gettext.obj\
	help.obj\
	hrt.obj\
	isa1275.obj\
	itu.obj\
	kbd.obj\
	main.obj\
	menu.obj\
	mount.obj\
	mpspec.obj\
	names.obj\
	open.obj\
	pci.obj\
	pci1275.obj\
	pciutil.obj\
	pnp.obj\
	pnp1275.obj\
	pnpbios.obj\
	printf.obj\
	probe.obj\
	prop.obj\
	resmgmt.obj\
	spmalloc.obj\
	sprintf.obj\
	tree.obj\
	tty_in.obj\
	tty_out.obj\
	ur.obj\
	version.obj\
	vfprintf.obj\
	vgaprobe.obj\
	vsprintf.obj\


# C source files

CSRC =\
	80421275.c\
	acpi_rm.c\
	adv.c\
	aliases.c\
	befext.c\
	befinst.c\
	bios.c\
	biosprim.c\
	boards.c\
	boot.c\
	bop.c\
	bus.c\
	cfname.c\
	config.c\
	debug.c\
	devdb.c\
	dir.c\
	eisa.c\
	enum.c\
	eprintf.c\
	err.c\
	escd.c\
	fprintf.c\
	gettext.c\
	help.c\
	hrt.c\
	isa1275.c\
	itu.c\
	kbd.c\
	main.c\
	makedeps.c\
	menu.c\
	mount.c\
	mpspec.c\
	names.c\
	open.c\
	pci.c\
	pci1275.c\
	pciutil.c\
	pnp.c\
	pnp1275.c\
	printf.c\
	probe.c\
	prop.c\
	resmgmt.c\
	spmalloc.c\
	sprintf.c\
	tree.c\
	tty_in.c\
	tty_out.c\
	ur.c\
	version.c\
	vfprintf.c\
	vgaprobe.c\
	vsprintf.c\


#  C header files:
HSRC =\
	80421275.h\
	acpi_rm.h\
	adv.h\
	aliases.h\
	befinst.h\
	bios.h\
	biosprim.h\
	boards.h\
	boot.h\
	bop.h\
	bus.h\
	cfname.h\
	config.h\
	debug.h\
	dev_info.h\
	devdb.h\
	dir.h\
	eisa.h\
	enum.h\
	err.h\
	escd.h\
	gettext.h\
	help.h\
	hrt.h\
	isa1275.h\
	itu.h\
	kbd.h\
	main.h\
	menu.h\
	mount.h\
	mps_table.h\
	mpspec.h\
	open.h\
	pci.h\
	pci1275.h\
	pcihrt.h\
	pciutil.h\
	pnp.h\
	pnp1275.h\
	probe.h\
	prop.h\
	resmgmt.h\
	spmalloc.h\
	tree.h\
	tty.h\
	types.h\
	ur.h\
	version.h\

#
#  Overlays have been implemented but the cost of re-reading the compressed
#  bootconf.exe image multiple times is currently too high.  Some thought
#  needs to be given to cost reduction before overlays are enabled for good.
#
#USE_OVRLAY=link.def
#
USE_OVRLAY=

bootconf.exe: $(OBJS) $(USE_OVRLAY)
	set LINK=/seg:200
	cl $(LFLAGS) -o bootconf.exe $(OBJS) $(USE_OVRLAY)
	if exist escd.rf del escd.rf

clean:
	if exist *.obj del *.obj
	if exist *.exe del *.exe
	if exist *.map del *.map
	if exist escd.rf del escd.rf
	if exist make.out del make.out
	if exist makedeps.inc del makedeps.inc
