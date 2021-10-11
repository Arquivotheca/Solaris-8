#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile.sol	1.17	99/10/07 SMI
#
CFILES=\
	acpi_rm.c adv.c befext.c befinst.c bios.c biosprim.c boards.c boot.c \
	bop.c bus.c cfname.c config.c debug.c devdb.c dir.c eisa.c enum.c \
	eprintf.c err.c escd.c fprintf.c gettext.c help.c hrt.c isa1275.c \
	itu.c kbd.c main.c menu.c mount.c mpspec.c names.c open.c pci.c \
	pci1275.c pciutil.c pnp.c pnp1275.c pnpbios.c printf.c probe.c \
	prop.c resmgmt.c spmalloc.c sprintf.c tree.c tty_in.c tty_out.c ur.c \
	version.c vfprintf.c vgaprobe.c vsprintf.c lint.c

INCDIRS= \
	-I. \
	-I../common/include \
	-I../inc \
	-I/la/ws/cpj/dos.inc \
	-I../../../psm/stand/boot/i386/common

LINTFLAGS= -nsmF -DDEBUG -D_MSC_VER=400 -D_cdecl= -D_huge= -D_far= -D_near= -D_segment=int -D_interrupt=

lint: filter
	lint $(LINTFLAGS) $(INCDIRS) $(CFILES) > lint.all 2>&1
	./filter lint.all > lint.out
	diff lint.gold lint.out

