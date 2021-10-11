/*
 * Copyright 1990 by Sun Microsystems, Inc.
 */

#ident	"@(#)mem_kern.c	1.5	94/10/30 SMI"

/*
 * Memory pixrect (non)creation in kernel
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/pixrect.h>
/* #include "/usr/include/pixrect/pixrect.h" */

int	mem_rop();
int	mem_putcolormap();
int	mem_putattributes();

struct	pixrectops mem_ops = {
	mem_rop,
	mem_putcolormap,
	mem_putattributes,
#ifdef _PR_IOCTL_KERNEL_DEFINED
	0
#endif
};

/*ARGSUSED*/
int
mem_rop(dpr, dx, dy, dw, dh, op, spr, sx, sy)
Pixrect *dpr;
int dx, dy, dw, dh;
int op;
Pixrect *spr;
int sx, sy;
{
#ifdef DEBUG
	cmn_err(CE_PANIC, "mem_rop: pixrects not supported.");
#endif
	return (PIX_ERR); /* fail */
}

/*ARGSUSED*/
int
mem_putcolormap(pr, index, count, red, green, blue)
Pixrect *pr;
int index, count;
u_char red[], green[], blue[];
{
#ifdef DEBUG
	cmn_err(CE_PANIC,
	    "mem_putcolormap: pixrects not supported.");
#endif
	return (PIX_ERR); /* fail */
}

/*ARGSUSED*/
int
mem_putattributes(pr, planes)
Pixrect *pr;
int *planes;
{
#ifdef DEBUG
	cmn_err(CE_PANIC,
	    "mem_putattributes: pixrects not supported.");
#endif
	return (PIX_ERR);
}
