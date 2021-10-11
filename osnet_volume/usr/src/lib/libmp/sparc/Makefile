#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.8	00/09/14 SMI"
#
# lib/libmp/sparc/Makefile

OLIBRARY =	libmp.so.1

MAPDIR=	../spec/sparc

include ../Makefile.com

# For libmp.so.1

CLEANFILES +=	pics/pics_mp.o
CLOBBERFILES += $(OLIBRARY)

# permissions for old library
$(ROOTLIBDIR)/$(OLIBRARY) :=	FILEMODE= 755

.KEEP_STATE:

$(OLIBRARY):	pics/pics_mp.o libmp.so.2
	$(CC) $(CFLAGS) -o $@ -G -h $(OLIBRARY) -z text $(ZDEFS) \
		pics/pics_mp.o $(LDLIBS) libmp.so.2
	$(POST_PROCESS_SO)


all: $(LIBS) $(OLIBRARY)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLIBDIR)/$(OLIBRARY)
