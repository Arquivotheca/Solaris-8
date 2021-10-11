#
#ident	"@(#)Makefile	1.24	94/11/29 SMI"
#
# Copyright (c) 1989-1994, by Sun Microsystems, Inc.
#
# psm/stand/bootblks/hsfs/Makefile
#
SUBDIRS	= $(MACH)

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

.KEEP_STATE:

all clean clobber lint : $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# XXX	There's no point in installing the hsfs stuff if we
#	never use it (or test it!)
#
install: FRC

FRC:

