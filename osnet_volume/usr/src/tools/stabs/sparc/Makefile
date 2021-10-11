#
# Copyright (c) 1991-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/01/11 SMI"
#

STABS_SRC = ..

#
#       Define the module and object file sets.
#
STABS_PROG	= stabs	  # name doesn't conflict with Makefile.master STABS
PROG	= stabs	  # name doesn't conflict with Makefile.master STABS
STABS_SRCS	= $(STABS_SRC)/main.c $(STABS_SRC)/stabs.c \
		  $(STABS_SRC)/forth.c $(STABS_SRC)/genassym.c \
		  $(STABS_SRC)/squander.c
STABS_OBJS	= $(STABS_SRCS:$(STABS_SRC)/%.c=%.o)
LINTS		= $(STABS_OBJS:%.o=%.ln)

SRCS		= $(STABS_SRCS)
OBJECTS		= $(STABS_OBJS)

#
#       Include common rules.
#
CLEANFILES	= $(OBJECTS)

include ../../Makefile.tools

FILEMODE        = 555
OWNER		= root
GROUP		= bin

CFLAGS		+= -v

INC_PATH = -I$(STABS_SRC)

#
#       Default build targets.
#
.KEEP_STATE:

.PARALLEL: $(STABS_OBJS)

all: $(STABS_PROG)

install: all .WAIT $(ROOTONBLDMACHPROG)

lint:	$(SRCS)
	@$(LINT) $(LINTFLAGS) $(SRCS)

clean:
	$(RM) $(CLEANFILES)

clobber: clean
	$(RM) stabs

clean.lint:
	$(RM) $(LINTS)

$(STABS_PROG): $(STABS_OBJS)
	$(LINK.c) -o $@ $(STABS_OBJS) -lm

%.o: $(STABS_SRC)/%.c
	$(COMPILE.c) $(INC_PATH) -o $@ -c $<

include ../../Makefile.targ
