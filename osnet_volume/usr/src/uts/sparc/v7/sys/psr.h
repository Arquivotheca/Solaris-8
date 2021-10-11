/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

#ifndef _SYS_PSR_H
#define	_SYS_PSR_H

#pragma ident	"@(#)psr.h	1.3	96/09/12 SMI" /* from SunOS psl.h 1.2 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definition of bits in the SPARC PSR (Processor Status Register)
 *  ________________________________________________________________________
 * | IMPL | VER	|	ICC	| resvd	| EC | EF | PIL	| S | PS | ET | CWP |
 * |	  |	| N | Z | V | C |	|    |	  |	|   |	 |    |	    |
 * |------|-----|---|---|---|---|-------|----|----|-----|---|----|----|-----|
 *  31  28 27 24  23  22  21  20 19   14  13   12  11  8   7   6    5  4   0
 *
 * Reserved bits are defined to be initialized to zero and must
 * be preserved if written, for compatabily with future revisions.
 */

#define	PSR_CWP		0x0000001F	/* current window pointer */
#define	PSR_ET		0x00000020	/* enable traps */
#define	PSR_PS		0x00000040	/* previous supervisor mode */
#define	PSR_S		0x00000080	/* supervisor mode */
#define	PSR_PIL		0x00000F00	/* processor interrupt level */
#define	PSR_EF		0x00001000	/* enable floating point unit */
#define	PSR_EC		0x00002000	/* enable coprocessor */
#define	PSR_RSV		0x000FC000	/* reserved */
#define	PSR_ICC		0x00F00000	/* integer condition codes */
#define	PSR_C		0x00100000	/* carry bit */
#define	PSR_V		0x00200000	/* overflow bit */
#define	PSR_Z		0x00400000	/* zero bit */
#define	PSR_N		0x00800000	/* negative bit */
#define	PSR_VER		0x0F000000	/* mask version */
#define	PSR_IMPL	0xF0000000	/* implementation */

#define	PSL_ALLCC	PSR_ICC		/* for portability */

#ifndef _ASM
typedef int	psw_t;
#endif

/*
 * Handy psr values.
 */
#define	PSL_USER	(PSR_S)		/* initial user psr */
#define	PSL_USERMASK	(PSR_ICC)	/* user variable psr bits */

#define	PSL_UBITS	(PSR_ICC|PSR_EF)	/* user modifiable fields */
						/* should set PSR_EC also */

#ifndef	__sparcv9cpu	/* hide overlapping macro */
/*
 * Macros to decode psr.
 */
#define	USERMODE(ps)	(((ps) & PSR_PS) == 0)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSR_H */
