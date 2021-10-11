#
# Copyright (c) 1993,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.17	99/01/25 SMI"
#
# lib/libnisdb/Makefile
#
LIBRARY= libnisdb.a
VERS=	.2
PROTOCOL_DIR= $(ROOT)/include/rpcsvc

SED=	sed

OBJECTS = \
	db_entry.o db_entry_c_xdr.o \
	db_item.o db_item_c_xdr.o \
	db_vers.o db_vers_c_xdr.o \
	db_pickle.o \
	db_table.o db_table_c_xdr.o \
	db_index_entry.o db_index_entry_c.o \
	db_index.o db_index_c_xdr.o \
	db_scheme.o db_scheme_c_xdr.o \
	db_query.o db_query_c_xdr.o \
	db_mindex.o db_mindex_c_xdr.o \
	db_log_entry.o db_log_entry_c_xdr.o \
	db_log.o \
	db.o \
	xdr_nullptr.o \
	db_dictionary.o db_dictionary_c_xdr.o \
	db_dictxdr.o db_dictlog.o db_dictlog_c_xdr.o \
	nis_db.o
#


X_FILES= \
    db_c.x             db_dictionary_c.x db_entry_c.x  db_index_c.x     \
    db_index_entry_c.x db_item_c.x       db_log_c.x    db_log_entry_c.x \
    db_mindex_c.x      db_query_c.x      db_scheme_c.x db_table_c.x     \
    db_vers_c.x	       db_dictlog_c.x

DERIVED_HEADERS= $(X_FILES:%_c.x=%_c.h) $(X_FILES:%_c.x=%.h)
DERIVED_SOURCES= $(X_FILES:%_c.x=%_c_xdr.c)
DERIVED_FILES= $(DERIVED_HEADERS) $(DERIVED_SOURCES)

# delete the derived files when cleaning up
CLEANFILES += $(DERIVED_FILES)

# include library definitions
include ../Makefile.lib

MAPDIR=		./spec/$(MACH)
MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

SRCS= db_dictionary_c_xdr.c db_dictlog_c_xdr.c db_dictxdr.c \
db_entry_c_xdr.c db_index_c_xdr.c db_index_entry_c_xdr.c db_item_c_xdr.c \
db_log_c_xdr.c db_log_entry_c_xdr.c db_mindex_c_xdr.c db_query_c_xdr.c \
db_scheme_c_xdr.c db_table_c_xdr.c db_vers_c_xdr.c
#db_c_xdr.c xdr_nullptr.c

DYNFLAGS +=	-M $(MAPFILE)

LIBS += $(DYNLIB)
ZDEFS=
LDLIBS += -lnsl -ldl -lc

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint

.KEEP_STATE:

all:	spec .WAIT $(DERIVED_FILES) $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)

lint: $(SRCS:.c=.ln) $(LINTLIB) spec

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

objs/%.o pics/%.o profs/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o profs/%.o: %.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

#
# Pattern matching rules that define how to build the derived files.
#
%_c.h: %_c.x
	$(RM) $@
	$(RPCGEN) -DUSINGC -h -o $@ $<

%.h: %_c.x
	$(RM) $@
	$(RPCGEN) -h -o $@ $<

%_c_xdr.c: %_c.x
	$(RM) $@
	$(RPCGEN) -DUSINGC -c -o $@ $<

# rename the xdr_db_free_entry() in the db_table_c_xdr.c.  So the one
# in db_index_entry_c is used instead.
DB_TABLE=db_table_c

$(DB_TABLE)_xdr.c:	$(DB_TABLE).x
	$(RM) $@
	$(RPCGEN) -DUSINGC -c $(DB_TABLE).x |\
	$(SED) -e 's/^xdr_db_free_entry(/__OBSOLETED_xdr_db_free_entry(/' > $@

clean clobber: spec

spec:	FRC
	@cd $@;pwd; $(MAKE) $(TARGET)


# include library targets
include ../Makefile.targ

FRC:
