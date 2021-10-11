/* @(#) bmtrace.c 1.1 92/01/13 */
#include "utrace.h"

#define ITER 10000
int i;

extern void trace_close();

#define UTR_BM01 21
#define UTR_BM02 22

#define UTR_BM11 23
#define UTR_BM12 24

#define UTR_BM21 25
#define UTR_BM22 26

#define UTR_BM31 27
#define UTR_BM32 28

#define UTR_BM41 29
#define UTR_BM42 30

#define UTR_BM51 31
#define UTR_BM52 32

#define UTR_BMstr51 33
#define UTR_BMstr52 34

int 	d11	=	11,
	d21	=	21, 
	d22	=	22, 
	d31	=	31, 
	d32	=	32, 
	d33	=	33, 
	d41	=	41, 
	d42	=	42, 
	d43	=	43, 
	d44	=	44, 
	d51	=	51, 
	d52	=	52, 
	d53	=	53, 
	d54	=	54, 
	d55	=	55
	;

char str51 [20] = "str51";
char str52 [20] = "str52";
char str53 [20] = "str53";
char str54 [20] = "str54";
char str55 [20] = "str55";

main()
{
	atexit(trace_close);
	trace_on();
	enable_tracepoint(UTR_FAC_TRACE, UTR_BM01);
	enable_tracepoint(UTR_FAC_TRACE, UTR_BM02);

	enable_tracepoint(UTR_FAC_TRACE, UTR_BM11);
	enable_tracepoint(UTR_FAC_TRACE, UTR_BM12);

	enable_tracepoint(UTR_FAC_TRACE, UTR_BM21);
	enable_tracepoint(UTR_FAC_TRACE, UTR_BM22);

	enable_tracepoint(UTR_FAC_TRACE, UTR_BM31);
	enable_tracepoint(UTR_FAC_TRACE, UTR_BM32);

	enable_tracepoint(UTR_FAC_TRACE, UTR_BM41);
	enable_tracepoint(UTR_FAC_TRACE, UTR_BM42);

	enable_tracepoint(UTR_FAC_TRACE, UTR_BM51);
	enable_tracepoint(UTR_FAC_TRACE, UTR_BM52);

	enable_tracepoint(UTR_FAC_TRACE, UTR_BMstr51);
	enable_tracepoint(UTR_FAC_TRACE, UTR_BMstr52);

	for (i = 0; i < ITER; i++) {
		TRACE_0 (UTR_FAC_TRACE, UTR_BM01, "bm ev01");
		TRACE_0 (UTR_FAC_TRACE, UTR_BM02, "bm ev02");
	}

	for (i = 0; i < ITER; i++) {
		TRACE_1 (UTR_FAC_TRACE, UTR_BM11, "bm ev11:data 0x%x", d11);
		TRACE_1 (UTR_FAC_TRACE, UTR_BM12, "bm ev12:data 0x%x", d11);
	}

	for (i = 0; i < ITER; i++) {
		TRACE_2 (UTR_FAC_TRACE, UTR_BM21, "bm ev21:data 0x%x 0x%x", 
		    d21, d22);
		TRACE_2 (UTR_FAC_TRACE, UTR_BM22, "bm ev22:data 0x%x 0x%x", 
		    d21, d22);
	}

	for (i = 0; i < ITER; i++) {
		TRACE_3 (UTR_FAC_TRACE, UTR_BM31, "bm ev31:data 0x%x 0x%x 0x%x",
	    	    d31, d32, d33);
		TRACE_3 (UTR_FAC_TRACE, UTR_BM32, "bm ev32:data 0x%x 0x%x 0x%x",
	    	    d31, d32, d33);
	}

	for (i = 0; i < ITER; i++) {
		TRACE_4 (UTR_FAC_TRACE, UTR_BM41, 
		    "bm ev41:data 0x%x 0x%x 0x%x 0x%x", d41, d42, d43, d44);
		TRACE_4 (UTR_FAC_TRACE, UTR_BM42,
		    "bm ev42:data 0x%x 0x%x 0x%x 0x%x", d41, d42, d43, d44);
	}

	for (i = 0; i < ITER; i++) {
		TRACE_5 (UTR_FAC_TRACE, UTR_BM51,
		    "bm ev51:data 0x%x 0x%x 0x%x 0x%x 0x%x",
		    d51, d52, d53, d54, d55);
		TRACE_5 (UTR_FAC_TRACE, UTR_BM52,
		    "bm ev52:data 0x%x 0x%x 0x%x 0x%x 0x%x",
		    d51, d52, d53, d54, d55);
	}

	for (i = 0; i < ITER; i++) {
		TRACE_5 (UTR_FAC_TRACE, UTR_BMstr51,
		    "bm evstr51:data %s %s %s %s %s",
		    str51, str52, str53, str54, str55);
		TRACE_5 (UTR_FAC_TRACE, UTR_BMstr52,
		    "bm evstr52:data %s %s %s %s %s",
		    str51, str52, str53, str54, str55);
	}
}
