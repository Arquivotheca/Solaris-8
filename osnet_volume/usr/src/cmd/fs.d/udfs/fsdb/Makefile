#
#pragma ident	"@(#)Makefile	1.9	99/10/17 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		udfs
LIBPROG=	fsdb

include		../../Makefile.fstype

CPPFLAGS +=     -D_LARGEFILE64_SOURCE
LDLIBS += -lc2 -lmalloc -ll -ly -ladm
YFLAGS="-d"

SRCS=	fsdb.c ud_lib.c

fsdb : fsdb_yacc.o fsdb_lex.o ud_lib.o fsdb.o
	$(LINK.c) -o $@ fsdb.o fsdb_yacc.o fsdb_lex.o \
			ud_lib.o $(LDLIBS)
	$(POST_PROCESS)

fsdb.o : fsdb.c
	$(COMPILE.c) -o $@ fsdb.c
	$(POST_PROCESS_O)

ud_lib.o : ../fstyp/ud_lib.c
	rm -f ud_lib.c
	cp ../fstyp/ud_lib.c ud_lib.c
	rm -f ud_lib.h
	cp ../fstyp/ud_lib.h ud_lib.h
	$(COMPILE.c) -o $@ ud_lib.c
	$(POST_PROCESS_O)

y.tab.c : fsdb_yacc.y
	yacc ${YFLAGS} fsdb_yacc.y

fsdb_yacc.o : y.tab.c
	$(COMPILE.c) -o $@ y.tab.c
	$(POST_PROCESS_O)

lex.yy.c : fsdb_lex.l
	lex -e fsdb_lex.l

fsdb_lex.o : lex.yy.c
	$(COMPILE.c) -o $@ lex.yy.c
	$(POST_PROCESS_O)

clean :
	rm -f ud_lib.c ud_lib.h
	rm -f fsdb.o ud_lib.o fsdb_yacc.o fsdb_lex.o
	rm -f fsdb_yacc.c fsdb_lex.c y.tab.c y.tab.h lex.yy.c

# for messaging catalog
#
POFILE= fsdb.po

# for messaging catalog
#
catalog:        $(POFILE)

CATSRCS=	$(SRCS) lex.yy.c y.tab.c

$(POFILE):      $(CATSRCS)
	$(RM) $@
	$(COMPILE.cpp) $(CATSRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d" messages.po >  $@
	$(RM) $(POFILE).i messages.po

lint: y.tab.c lex.yy.c
	rm -f ud_lib.h
	cp ../fstyp/ud_lib.h ud_lib.h
	rm -f ud_lib.c
	cp ../fstyp/ud_lib.c ud_lib.c
	$(LINT.c) -mu $(SRCS)

putback :
	-cstyle -p fsdb.c
	-keywords fsdb.c
	-keywords Makefile

