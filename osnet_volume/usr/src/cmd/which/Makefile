#
# ident	"@(#)Makefile	1.5	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# Makefile for cmd/which
#

PROG =		which

include		../Makefile.cmd


# C shell build rule
.csh:
	$(RM) $@
	cat $< > $@
	chmod +x $@

.SUFFIXES: .csh

.KEEP_STATE :

all:		$(PROG) 

install :	all $(ROOTPROG)

clean lint:

include		../Makefile.targ
