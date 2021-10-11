#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.3	97/11/12 SMI"
#
# lib/gss_mechs/mech_dummy/Makefile
#
#
# This make file will build mech_dummy.so.1. This shared object
# contains all the functionality needed to support the Dummy GSS-API
# mechanism. 
#

LIBRARY= mech_dummy.a
VERS = .1

# objects are listed by source directory

MECH=	dmech.o 

OBJECTS= $(MECH) 

# include library definitions
include ../../../Makefile.lib

CPPFLAGS += -I../../libgss -I$(SRC)/uts/common/gssapi/include \
	    -I$(ROOT)/usr/include/gssapi $(DEBUG)


OS_FLAGS = -DHAVE_LIBSOCKET=1 -DHAVE_LIBNSL=1 -DTIME_WITH_SYS_TIME=1 \
 -DHAVE_UNISTD_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_REGEX_H=1 -DHAVE_REGEXP_H=1 \
 -DHAVE_RE_COMP=1 -DHAVE_REGCOMP=1 -DPOSIX_TYPES=1 -DNDBM=1 -DAN_TO_LN_RULES=1

LIBS = $(LINTLIB)

LDLIBS += -lgss -lnsl -ldl -lc -lmp

.KEEP_STATE:

DUPLICATE_SRC += dmech.c

#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@

objs/%.o profs/%.o pics/%.o: ../mech/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

# for lint
dmech.c:	../mech/dmech.c
		rm -f dmech.c
		cp ../mech/dmech.c .

lint: $(SRCS:.c=.ln) $(LINTLIB)

# include library targets
include ../../../Makefile.targ

$(LIBRARY) : $(OBJS)
$(DYNLIB) : $(PICS)
