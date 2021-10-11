# Generated automatically from Makefile.in by configure.
#
# Makefile for Src subdirectory
#
# Copyright (c) 1996 Richard Coleman
# All rights reserved.
#
# Permission is hereby granted, without written agreement and without
# license or royalty fees, to use, copy, modify, and distribute this
# software and to distribute modified versions of this software for any
# purpose, provided that the above copyright notice and the following
# two paragraphs appear in all copies of this software.
#
# In no event shall Richard Coleman or the Zsh Development Group be liable
# to any party for direct, indirect, special, incidental, or consequential
# damages arising out of the use of this software and its documentation,
# even if Richard Coleman and the Zsh Development Group have been advised of
# the possibility of such damage.
#
# Richard Coleman and the Zsh Development Group specifically disclaim any
# warranties, including, but not limited to, the implied warranties of
# merchantability and fitness for a particular purpose.  The software
# provided hereunder is on an "as is" basis, and Richard Coleman and the
# Zsh Development Group have no obligation to provide maintenance,
# support, updates, enhancements, or modifications.
#
#ident	"@(#)Makefile	1.3	99/10/22 SMI"

# zsh version
VERSION = 3.0.6

PROG=zsh

include ../../Makefile.cmd

top_srcdir = ..
srcdir     = .

# I hate to do this, but it _isn't_ our code.
LOCAL_CFLAGS = -errtags=yes -erroff=E_END_OF_LOOP_CODE_NOT_REACHED \
    -erroff=E_EMPTY_TRANSLATION_UNIT -erroff=E_NON_CONST_INIT

INCLUDES = -I.. -I. -I$(srcdir)
DEFS     = -DHAVE_CONFIG_H -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS   +=  $(DEFS) $(INCLUDES) $(LOCAL_CFLAGS)
		
LIBS   += -lnsl -ltermlib


AWK = nawk
SED = sed

.SUFFIXES:
.SUFFIXES: .c .o .pro

.c.pro:
	$(SED) -n -f $(srcdir)/makepro.sed $< > $@

# this header file is parsed to generate signal names (signames.h)
SIGNAL_H = $(ROOT)/usr/include/sys/iso/signal_iso.h

# this header file is parsed to generate limits, if available
RLIMITS_INC_H = $(ROOT)/usr/include/sys/resource.h

# headers included in distribution
DIST_HDRS = version.h globals.h hashtable.h prototypes.h signals.h \
system.h zle.h zsh.h ztype.h

# generated headers
GEN_HDRS = signames.h ../config.h rlimits.h

# zsh headers necessary for compilation
HDRS = $(DIST_HDRS) $(GEN_HDRS)

# zsh C source
SRCS = builtin.c compat.c cond.c exec.c glob.c hashtable.c hist.c init.c \
input.c jobs.c lex.c linklist.c loop.c math.c mem.c params.c parse.c \
signals.c subst.c text.c utils.c watch.c zle_bindings.c zle_hist.c \
zle_main.c zle_misc.c zle_move.c zle_refresh.c zle_tricky.c zle_utils.c \
zle_vi.c zle_word.c

# generated prototypes
PROTO = builtin.pro compat.pro cond.pro exec.pro glob.pro hashtable.pro \
hist.pro init.pro input.pro jobs.pro lex.pro linklist.pro loop.pro \
math.pro mem.pro params.pro parse.pro signals.pro subst.pro text.pro \
utils.pro watch.pro zle_hist.pro zle_main.pro \
zle_misc.pro zle_move.pro zle_refresh.pro zle_tricky.pro zle_utils.pro \
zle_vi.pro zle_word.pro

# object files
OBJS = builtin.o compat.o cond.o exec.o glob.o hashtable.o \
hist.o init.o input.o jobs.o lex.o linklist.o loop.o \
math.o mem.o params.o parse.o signals.o subst.o text.o \
utils.o watch.o zle_bindings.o zle_hist.o zle_main.o \
zle_misc.o zle_move.o zle_refresh.o zle_tricky.o zle_utils.o \
zle_vi.o zle_word.o

# auxiliary files
AUX = Makefile.in .indent.pro signames.awk rlimits.awk makepro.sed \
      ansi2knr.c

# all files in this directory included in the distribution
DIST = $(DIST_HDRS) $(SRCS) $(AUX)

.KEEP_STATE:

.PARALLEL: $(OBJS)

# default target
all: $(PROG)

install: $(ROOTPROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LIBS)
	$(POST_PROCESS)

signames.h: signames.awk
	$(AWK) -f $(srcdir)/signames.awk $(SIGNAL_H) > signames.h

prototypes.h: $(PROTO)

# this file will not be made if limits are unavailable:
# silent so the warning doesn't appear unless necessary
rlimits.h: rlimits.awk $(RLIMITS_INC_H)
	@echo '$(AWK) -f $(srcdir)/rlimits.awk $(RLIMITS_INC_H) > rlimits.h'; \
	$(AWK) -f $(srcdir)/rlimits.awk $(RLIMITS_INC_H) > rlimits.h

$(OBJS): $(HDRS)

$(PROTO): makepro.sed

clean: 
	rm -f $(OBJS) signames.h rlimits.h *.pro

include ../../Makefile.targ
