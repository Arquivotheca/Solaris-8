#
# @(#)Makefile 1.9 96/05/04 SMI; from UCB
#
# make file for tip device drivers
#
# Current drivers:
#	BIZCOMP
#	DEC DF02-AC, DF03-AC
#	DEC DN-11/Able Quadracall
#	VENTEL 212+ (w/o echo)
#	VADIC 831 RS232 adaptor
#	VADIC 3451
#	HAYES SmartModem
#
# cmd/tip/aculib/Makefile

ACULIB=	aculib.a
OBJS=	biz22.o biz31.o df.o dn11.o hayes.o ventel.o v831.o v3451.o

include ../../Makefile.cmd

CPPFLAGS=	-I../ -DUSG $(CPPFLAGS.master)

.KEEP_STATE:

.PARALLEL: $(OBJS)

all:	$(ACULIB)

$(ACULIB): $(OBJS)
	$(AR) cr $(ACULIB) $(OBJS)

clean:
	$(RM) $(ACULIB) $(OBJS) core errs
