/*
 * Copyright (c) 1987-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FRAME_H
#define	_SYS_FRAME_H

#pragma ident	"@(#)frame.h	1.15	97/04/25 SMI"	/* sys4-3.2L 1.1 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definition of the sparc stack frame (when it is pushed on the stack).
 */
struct frame {
	long	fr_local[8];		/* saved locals */
	long	fr_arg[6];		/* saved arguments [0 - 5] */
	struct frame	*fr_savfp;	/* saved frame pointer */
	long	fr_savpc;		/* saved program counter */
#if !defined(__sparcv9)
	char	*fr_stret;		/* struct return addr */
#endif	/* __sparcv9 */
	long	fr_argd[6];		/* arg dump area */
	long	fr_argx[1];		/* array of args past the sixth */
};

#ifdef _SYSCALL32
/*
 * Kernels view of a 32-bit stack frame
 */
struct frame32 {
	int	fr_local[8];		/* saved locals */
	int	fr_arg[6];		/* saved arguments [0 - 5] */
	caddr32_t fr_savfp;		/* saved frame pointer */
	int	fr_savpc;		/* saved program counter */
	caddr32_t fr_stret;		/* struct return addr */
	int	fr_argd[6];		/* arg dump area */
	int	fr_argx[1];		/* array of args past the sixth */
};
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FRAME_H */
