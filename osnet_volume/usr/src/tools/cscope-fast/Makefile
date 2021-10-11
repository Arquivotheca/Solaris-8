#pragma	ident	"@(#)Makefile	1.2	99/01/13 SMI"

PROG= cscope-fast

OBJS= 	main.o dir.o crossref.o scanner.o lookup.o command.o display.o \
	find.o edit.o exec.o help.o history.o input.o menu.o alloc.o \
	cgrep.o compath.o invlib.o logdir.o \
	mouse.o mygetenv.o mygetwd.o mypopen.o \
	vpaccess.o vpfopen.o vpinit.o vpopen.o vpstat.o
SRCS=	$(OBJS:%.o=%.c)
CLEANFILES += $(OBJS)
TMPDIR= /tmp

include ../Makefile.tools

# these three are because we cannot seem to redefine the size of YYLMAX,
# and thus yytext, in the code because yytext is defined before our code
# is seen. YYLMAX is supposed to be STMTMAX+PATLEN+1.
PATLEN=		250
STMTMAX=	10000
YYLMAX=		10251
CFLAGS +=	-DPATLEN=$(PATLEN) -DSTMTMAX=$(STMTMAX) -DYYLMAX=$(YYLMAX)

CCMODE=		-Xa
CFLAGS +=	$(CCVERBOSE)
LDLIBS +=	-lgen -lcurses -ll

.KEEP_STATE:

.PARALLEL: $(OBJS)

all:	$(PROG)

# because of goto's in the scanner
scanner.o := CCVERBOSE=

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all .WAIT  $(ROOTONBLDMACHPROG)

clean:
	$(RM) $(CLEANFILES)

lint: lint_SRCS

include ../Makefile.targ
