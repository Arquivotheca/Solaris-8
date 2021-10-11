#
#ident	"@(#)Makefile	1.7	96/03/31 SMI"
#
# Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
#
# lib/fn/context/onc_ns/Makefile
#

SUBDIRS= libfnsp nisplus nis files


install	:= TARGET = install
install_h := TARGET = install_h
all	:= TARGET = all
clean	:= TARGET = clean
clobber	:= TARGET = clobber
debug	:= TARGET = debug
tcov	:= TARGET = tcov
analyse	:= TARGET = analyse

all debug install install_h:  $(SUBDIRS)
clean clobber tcov analyse:  $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

.KEEP_STATE:
