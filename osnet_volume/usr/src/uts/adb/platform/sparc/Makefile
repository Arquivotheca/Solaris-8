#
#ident	"@(#)Makefile	1.5	99/10/04 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# uts/adb/platform/sparc/Makefile
#
SUBDIRS=	sun4d sun4m sun4u

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

