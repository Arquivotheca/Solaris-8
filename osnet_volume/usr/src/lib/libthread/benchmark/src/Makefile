# @(#)Makefile 1.9 92/09/18
# Env var LIBTHDIR = dir containing libthread.a 
# install /net/ugly/export/mt/ui/obj/ld in /usr/ccs/bin/ld

# the following to get utrace.h and vtrace.h if used in a test program
TRACE_INC1=../../inc
TRACE_INC2=../../inc/sys

ARCH:sh =uname -m
LIBTHDIR=../../$(ARCH)
MTLIBDIR=../../../libc/sparc

BIN=../bin

CFLAGS +=  -I.

EOPTS=

%:	%.c /usr/include/thread.h
	cc -dy $(CFLAGS) -lthread $< -o $(BIN)/$@

%_h:	%.c /usr/include/thread.h
	cc -dy $(CFLAGS) -lthread $< -o $(BIN)/$@

%_s:	%.c /usr/include/thread.h
	cc -dn $(CFLAGS)  $< -lthread -o $(BIN)/$@

%_p:	%.c /usr/include/thread.h
	cc $(CFLAGS)  -D_POSIX_SOURCE ${EOPTS} $< -lthread -o $(BIN)/$@

# ansi-c conformance

%_a:	%.c /usr/include/thread.h
	cc $(CFLAGS)  -Xc ${EOPTS} $< -lthread -o $(BIN)/$@

# link with libthread_d.a which has internal debugging macros turned on
%_d:	%.c /usr/include/thread.h
	cc $(CFLAGS)  $< -lthread_d -o $(BIN)/$@


# link with instrumented libthread

%_t: %.c /usr/include/thread.h 
	cc $(CFLAGS)  -DUTRACE  $< -lthread_trace -o $(BIN)/$@
	 
# link with libthread which has internal instrumentation compiled in

%_it: %.c /usr/include/thread.h
	cc $(CFLAGS)  -DUTRACE $<  -lthread_itrace -o $(BIN)/$@

# link only with libc, not with the threads library

%_l:	%.c /usr/include/sys/lwp.h
	cc ${CFLAGS} $< -o $(BIN)/$@
	
# link with liblwp.a and libthread with the old trap for flushing windows. 
# (for sun4m)

%_l_ot:	%.c /usr/include/sys/lwp.h
	cc ${CFLAGS} $< -o $(BIN)/$@ 
	
# link with libthread with the old trap for flushing windows. (for sun4m)

%_ot: %.c /usr/include/thread.h
	cc $(CFLAGS) ${EOPTS} $< $(LIBTHDIR)/libthread_sun4m.a -o $(BIN)/$@

clean:
	rm *.o core
