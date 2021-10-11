#
#ident	"@(#)Makefile	1.9	96/03/31 SMI"
#
# Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
#
# lib/fn/Makefile
#

SUBDIRS= libxfn libxfn.1 libfn_spf libfn_p initial context address


install	:= TARGET = install
install_h := TARGET = install_h
private_h := TARGET = private_h
all	:= TARGET = all
clean	:= TARGET = clean
clobber	:= TARGET = clobber
check	:= TARGET = check
debug	:= TARGET = debug
tcov	:= TARGET = tcov
analyse	:= TARGET = analyse

all debug install install_h private_h:  $(SUBDIRS)
clean clobber check tcov analyse:  $(SUBDIRS)

# no linting since it is pretty useless for C++
lint:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

.KEEP_STATE:
