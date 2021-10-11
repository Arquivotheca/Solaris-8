#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident "@(#)ff.mk	1.2	93/02/18 SMI"

ROOT=
INC = $(ROOT)/usr/include
TESTDIR = .
INSDIR1 = $(ROOT)/usr/lib/fs/s5fs
INSDIR2 = $(ROOT)/etc/fs/s5fs
INS = install
CFLAGS = -O -I$(INC)
LDFLAGS = -s
OBJS=

all:  ff

ff: ff.c ../fsck/l3.o $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ff ff.c ../fsck/l3.o $(OBJS) $(SHLIBS)

../fsck/l3.o: ../fsck/l3.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o ../fsck/l3.o ../fsck/l3.c $(OBJS) $(SHLIBS)

install: ff
	@if [ ! -d $(ROOT)/usr/lib/fs ]; \
		then \
		mkdir $(ROOT)/usr/lib/fs; \
		fi;
	@if [ ! -d $(INSDIR1) ]; \
		then \
		mkdir $(INSDIR1); \
		fi;
	$(INS) -f $(INSDIR1) -m 0555 -u bin -g bin $(TESTDIR)/ff



clean:
	-rm -f ff.o

clobber: clean
	rm -f ff
