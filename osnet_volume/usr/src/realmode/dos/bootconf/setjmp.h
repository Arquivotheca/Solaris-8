/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  We have to use a special version of setjmp/longjmp when running in
 *  realmode to handle BEF stack switching ...
 */

#ifndef	_SETJMPX_H
#define	_SETJMPX_H

#ident "@(#)setjmp.h   1.6   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef	unsigned jmp_buf[10];	  /* Use customized DOS versions ...	    */

int setjmp(jmp_buf);
void longjmp(jmp_buf, int);

#ifdef	__cplusplus
}
#endif

#endif /* _SETJMPX_H */
