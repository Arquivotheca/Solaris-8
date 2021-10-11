#
#ident	"@(#)Makefile.cmd	1.2	99/05/14 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/abi/spectrans/Makefile.cmd

.KEEP_STATE:

include $(SRC)/cmd/Makefile.cmd

PROG_BIN =	$(ROOTLIB)/abi/$(PROG)
.PRECIOUS:	$(PROG)

U_LIB	=	parse
U_BASE	=	../../parser
U_DIR	= 	$(U_BASE)/$(MACH)
U_LIB_A	=	$(U_DIR)/lib$(U_LIB).a

CFLAGS +=	-v
CPPFLAGS +=	-I$(U_BASE)
LDFLAGS	+=	-L$(U_DIR)
LINTFLAGS +=	-xsuF -errtags=yes

LDLIBS	+=	-l$(U_LIB) -lgen
LINTLIBS =	-L$(U_DIR) -l$(U_LIB)

SRCS	=	$(OBJECTS:%.o=../%.c)

all:	$(PROG)

%.o:	../%.y
	$(YACC.y) $<
	$(COMPILE.c) -o $@ y.tab.c
	$(RM) y.tab.c

%.o:	../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(PROG): $(U_LIB_A) $(OBJECTS) $(YACC_OBJS)
	$(LINK.c) -o $@ $(OBJECTS) $(YACC_OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(U_LIB_A):
	@cd $(U_DIR); pwd; $(MAKE) all

install: $(PROG_BIN)

$(PROG_BIN) :=	FILEMODE = 755
$(PROG_BIN): $(PROG)
	$(INS.file) $(PROG)

clean:
	-$(RM) $(OBJECTS) $(YACC_OBJS)

clobber: clean
	-$(RM) $(PROG) $(CLOBBERFILES)

lint:
	$(LINT.c) $(SRCS) $(LINTLIBS)
