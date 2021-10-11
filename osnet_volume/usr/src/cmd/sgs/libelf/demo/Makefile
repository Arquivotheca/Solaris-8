

PROGS= pcom acom dcom tpcom
LDLIBS =	-lelf
tpcom :=	LDLIBS +=	-lthread

.KEEP_STATE:

all: $(PROGS)

test: test1 test2 test3 test4 test5

test1: pcom FRC
	@ echo ""
	@ echo "<<< Test 1 >>>"
	@ echo "Print comments of pcom"
	@ echo
	pcom pcom
	@ echo
	@ echo "Print comments from archive libelf.a"
	@ echo 
	pcom /usr/lib/libelf.a

test2: dcom pcom FRC
	@ echo ""
	@ echo "<<< Test 2 >>>"
	@ echo "delete the comment section from pcom"
	@ echo ""
	dcom pcom
	pcom pcom

test3: pcom acom test2 FRC
	@ echo ""
	@ echo "<<< Test 3 >>>"
	@ echo "update comments from pcom and then"
	@ echo "print them out."
	@ echo ""
	acom "Newly Updated Comments" pcom
	@ echo ""
	pcom pcom



test4: acom pcom test3 FRC
	@ echo ""
	@ echo "<<< Test 4 >>>"
	@ echo "append to the comment section of pcom"
	@ echo ""
	acom "This comment has been appended" pcom
	pcom pcom

test5: $(PROGS) FRC
	@ echo ""
	@ echo "<<< Test 5 >>>"
	@ echo "Relabel the new utilities using the new utilities"
	@ echo ""
	dcom $(PROGS)
	cp acom acom.safe
	acom.safe "libelf Demonstration Tools" $(PROGS)
	pcom $(PROGS)
	rm -f acom.safe

test6: tpcom FRC
	@ echo ""
	@ echo "<<< Test 6 >>>"
	@ echo "Using the threaded tpcom go through and examine"
	@ echo "all archives in /usr/lib.  This is an output intensive"
	@ echo "test."
	@ echo ""
	tpcom /usr/lib/lib*.a

clean: FRC
	rm -f $(PROGS) *.o core

FRC:
