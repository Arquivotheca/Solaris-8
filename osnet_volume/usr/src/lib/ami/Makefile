#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.13 00/09/14 SMI"
#
#	Makefile for AMI (libami & ami.jar)
#

SUBDIRS=	jarami .WAIT libami

# include library definitions
include ../Makefile.lib

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
check :=	TARGET= check
install :=      TARGET= install
lint :=      	TARGET= lint
install_h :=	TARGET= install_h
debug := 	TARGET= debug
_msg := 	TARGET= _msg

all clean clobber install debug _msg: $(SUBDIRS)
check lint install_h: libami

jarami libami: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

