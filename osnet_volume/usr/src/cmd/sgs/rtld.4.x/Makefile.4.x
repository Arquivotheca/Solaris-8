#
#ident        "@(#)Makefile.4.x 1.9     95/12/18 SMI"
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
# On a 4.x machine, point ROOT at the header files you're using, and do
#
#	% sccs edit ld.so
#	% make -f Makefile.4.x all
#	<test it a lot>
#	% sccs delget ld.so
#
# Unfortunately, <sys/isa_defs.h> contains a '#error' line that makes the 4.x
# cpp choke (even though it shouldn't parse the error clause).  You may need to
# delete the '#' sign to make the linker compile.

OBJS=	rtldlib.o rtld.4.x.o rtsubrs.o div.o umultiply.o rem.o zero.o

all:	${OBJS}
	ld -o ld.so -Bsymbolic -assert nosymbolic -assert pure-text ${OBJS}

%.o:%.s
	as -k -P -I$(ROOT)/usr/include -D_SYS_SYS_S -D_ASM $<
	mv -f a.out $*.o

%.o:%.c
	cc -c -O -I$(ROOT)/usr/include -pic -D_NO_LONGLONG $<
