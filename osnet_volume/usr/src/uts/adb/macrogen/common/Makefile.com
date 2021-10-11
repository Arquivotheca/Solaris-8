#
#ident	"@(#)Makefile.com	1.2	98/01/19 SMI" 
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# uts/adb/macrogen/common/Makefile.com
#
#

include $(SRC)/Makefile.master

#
#	No text domain in the kernel.
#
DTEXTDOM =

.KEEP_STATE:

MACROGEN=	macrogen
MGENPP=		mgenpp
OBJECTS=	parser.o stabs.o gen_adb.o pat.o mem.o
CMNDIR=		../common

#       Default build targets.
#
all install: $(MACROGEN) $(MGENPP)

$(MACROGEN): $(OBJECTS)
	$(NATIVECC) -o $(MACROGEN) $(OBJECTS) -lm -ldl

$(MGENPP): $(CMNDIR)/$(MGENPP).sh
	$(RM) $@
	cat $(CMNDIR)/$(MGENPP).sh > $@
	$(CHMOD) +x $@

%.o: $(CMNDIR)/%.c
	$(NATIVECC) $(NATIVECFLAGS) -o $@ $<

lint: FRC

clean:
	-$(RM) $(MACROGEN) $(MGENPP)

clobber: clean
	-$(RM) $(OBJECTS)

FRC:
