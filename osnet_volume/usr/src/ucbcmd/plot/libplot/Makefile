#
#ident	"@(#)Makefile	1.2	92/12/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# include global definitions
include ../../../Makefile.master

SUBDIRS= aed bitgraph dumb gigi hp2648 hp7221 imagen plot \
		t300 t300s t4013 t4014 t450

all:=     TARGET= all
clean:=   TARGET= clean
clobber:= TARGET= clobber
install:= TARGET= install
lint:=    TARGET= lint

.KEEP_STATE:
all clean clobber lint:  $(SUBDIRS)

install:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)


FRC:

