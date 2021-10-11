/*
 * Copyright (c) 1990-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if ! defined(regset_h)
#define regset_h

#pragma ident	"@(#)regset.h	1.6	97/11/22 SMI"


/*
 * OS independent structure to hold register values. The Pcs layer is supposed
 * to provide information in this format.
 *
 * Usually more information is available from the OS than the debugger uses.
 * Specifically the FPU state is quite fancy. We'll just define the minimum
 * here, and add to it as needed.
 */


#ifdef sparc
/*
 * Register (General) Set 
 */

typedef struct RegGSet_t {
    unsigned long       g[8];           /* global registers */
    unsigned long       i[8];           /* in registers */
    unsigned long       l[8];           /* local logisters */
    unsigned long       o[8];           /* out registers */
    unsigned long       y;              /* multiply overflow */
    unsigned long       psr;            /* processor status word */
    unsigned long       wim;            /* (register) Window Invalid Mask */
    unsigned long       tbr;            /* ??? */
    unsigned long       pc;             /* program counter */
    unsigned long       npc;            /* next PC */
} RegGSet_t;


/*
 * Register (Floating point) Set
 * FPU registers aren't always available, hence the 'valid' field.
 */

typedef struct RegFSet_t {
    Boolean             valid;
    union {
        float           f[32];          /* float (single precision) view */
        double          d[16];          /* double (precision) view */
    } u;
    unsigned long       fsr;            /* FPU status register */
    unsigned long       fq;
} RegFSet_t;

#elif defined(i386)

#include <sys/reg.h> 
 
typedef struct RegGSet_t {
        long    regs[NGREG];
} RegGSet_t;
 
/* The following definitions are taken from <ieeefp.h> */
/* They are better suited for our needs than the one from <sys/reg.h>  [VT]
 */

 
typedef struct _fpreg {         /* structure of a temp real fp register */
        unsigned short significand[4];  /* 64 bit mantissa value */
        unsigned short exponent;        /* 15 bit exp + sign bit */
}_fpreg;
 
typedef struct  RegFSet_t {     /* saved state info from an exception */
        unsigned long   cw,     /* control word */
                        sw,     /* status word after fnclex-not useful */
                        tag,    /* tag word */
                        ipoff,  /* %eip register */
                        cssel,  /* code segment selector */
                        dataoff, /* data operand address */
                        datasel; /* data operand selector */
        struct _fpreg _st[8];   /* saved register stack */
        unsigned long status;   /* status word saved at exception */
} RegFSet_t;

#else
#error Unknown architecture!
#endif /* sparc/i386 */
#endif /* regset_h */
