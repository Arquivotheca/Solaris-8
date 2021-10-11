#
#ident	"@(#)Makefile	1.2	99/10/17 SMI"

#### Start of system configuration section. ####

srcdir = .

PROG1=	less
PROG2=	lesskey
PROG3=	lessecho
PROG=	$(PROG1) $(PROG2) $(PROG3)
CLOBBERFILES += $(PROG)

SHELL = /bin/sh

OBJS=	main.o screen.o brac.o ch.o charset.o cmdbuf.o \
	command.o decode.o edit.o filename.o forwback.o \
	help.o ifile.o input.o jump.o line.o linenum.o \
	lsystem.o mark.o optfunc.o option.o opttbl.o os.o \
	output.o position.o prompt.o search.o signal.o \
	tags.o ttyin.o version.o  

ROOTSRC= $(ROOT)/usr/share/src/less
DISTFILES:sh= cat ./distfiles
ROOTDISTFILES= $(DISTFILES:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444
$(ROOTSRC)/configure := FILEMODE= 0555
MAN1FILES=	less.1 lesskey.1

include ../Makefile.cmd

LDLIBS += -lgen -lcurses -ltermcap
CFLAGS += -I.

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG)

install: all $(ROOTPROG) install_src install_man

install_src: $(ROOTSRC) .WAIT $(ROOTDISTFILES)

install_man: $(ROOTMAN1FILES)

$(ROOTSRC):
	$(INS.dir)

$(ROOTSRC)/%: %
	$(INS.file)

less: $(OBJS)
	$(LINK.c) -o less $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

lesskey: lesskey.o version.o
	$(LINK.c) -o lesskey lesskey.o version.o
	$(POST_PROCESS)

lessecho: lessecho.o version.o
	$(LINK.c) -o lessecho lessecho.o version.o
	$(POST_PROCESS)

$(OBJS): ${srcdir}/less.h ${srcdir}/funcs.h defines.h 

clean:
	rm -f *.o core

include ../Makefile.targ
