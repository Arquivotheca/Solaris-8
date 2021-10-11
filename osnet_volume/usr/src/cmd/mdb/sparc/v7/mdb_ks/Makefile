#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

MODULE = mdb_ks.so
MDBTGT = kvm

MODSRCS = mdb_ks.c

include ../../../../Makefile.cmd
include ../../Makefile.sparcv7
include ../../../Makefile.module

#
# We are not actually hardwiring some dependency on sun4u, we just need to
# include sys/param.h with _MACHDEP defined, and this forces the inclusion of
# machparam.h, even though we don't use anything there.  This is a temporary
# kludge until we invent -DDONTINCLUDEMACHPARAM or something.
#
CPPFLAGS += -I$(SRC)/uts/sun4u
