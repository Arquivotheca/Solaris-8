#
#ident	"@(#)Makefile.com	1.1	99/01/25 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/abi/daa2x/backends/common/sparc/Makefile.com

.KEEP_STATE:

LIBRARY =	libcommon.a

OBJECTS	=	errlog.o
		exclude.o
		extends.o
		frontend.o
		getnextstr.o
		libtrans.o
		main.o
		needproc.o
		preprocess.o

include		$(SRC)/lib/Makefile.lib

CPPFLAGS +=	-I../../inc
CLOBBERFILES +=	$(LINTLIB) $(LINTOUT) $(PICOBJ)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../src/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

all install:	$(PICOBJ)

#
# combine relocatable objects
#
$(PICOBJ): pics .WAIT  $(PICS)
	$(LD) -r -o $@ $(PICS)
	$(POST_PROCESS_O)

lint:		$(LINTLIB)

include		$(SRC)/lib/Makefile.targ
