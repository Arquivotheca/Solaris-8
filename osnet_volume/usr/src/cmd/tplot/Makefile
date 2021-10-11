#
#ident	"@(#)Makefile	1.2	92/06/24 SMI"
#
# Copyright (c) 1992 by Sun Microsystems, Inc.
#
# cmd/tplot/Makefile

TPROG= t4014 t300 t300s t450
PROG= $(TPROG) vplot
SHFILES= tplot

# include common definitions
include ../Makefile.cmd

CLOBBERFILES += $(SHFILES)

# conditional assignments
t% := LDLIBS += -l$(@F:t%=%) -lm

.KEEP_STATE:

all: $(PROG) $(SHFILES)

$(TPROG): driver.o
	$(LINK.c) -o $@ driver.o $(LDLIBS)
	$(POST_PROCESS)

$(ROOTSHFILES):	$(SHFILES)

vplot: vplot.o banner.o
	$(LINK.c) -o $@ vplot.o banner.o $(LDLIBS)
	$(POST_PROCESS)

install: $(ROOTLIBPROG) $(ROOTSHFILES)

clean:
	$(RM) driver.o vplot.o banner.o

# include common targets
include ../Makefile.targ
