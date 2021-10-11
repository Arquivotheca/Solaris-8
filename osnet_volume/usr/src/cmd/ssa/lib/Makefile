#
# Copyright (c) 1993-1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	96/05/20 SMI"
#
# cmd/ssa/lib/Makefile
#
#      Sparc Storage Array Interface Library
#

include		../../../lib/Makefile.lib

LIBRARY		= libssa.a

CPPFLAGS	+= -D_REENTRANT -I../sys -v

OBJECTS		=  funct.o io.o uscsi_cmd.o genf.o

LINT_OBJS	= $(OBJECTS:%.o=%.ln)

.KEEP_STATE:

# Create a static library 

all debug browser debug_browser:	$(LIBRARY)
debug	:= CFLAGS += -g
browser	:= CFLAGS += -xsb
debug_browser	:= CFLAGS += -xsb -g

$(LIBRARY):	$(OBJECTS)
	$(AR) crv libssa.a $(OBJECTS)

install: all

lint: $(LINT_OBJS)
	$(LINT.c) $(LINT_OBJS)

clean:
	$(RM) $(OBJECTS)
	$(RM) $(LINT_OBJS)

clobber: clean
	$(RM) $(LIBRARY)

.PRECIOUS: $(LIBRARY)
