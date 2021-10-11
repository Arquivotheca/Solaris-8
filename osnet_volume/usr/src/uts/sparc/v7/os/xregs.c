/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xregs.c	1.6	98/03/09 SMI"

#include <sys/t_lock.h>
#include <sys/proc.h>
#include <sys/ucontext.h>
#include <sys/archsystm.h>

/*
 * Association of extra register state with a struct ucontext is
 * done by placing an xrs_t within the uc_mcontext filler area.
 *
 * The following routines provide an interface for this association.
 */

/*
 * clear the struct ucontext extra register state pointer
 */
/*ARGSUSED*/
void
xregs_clrptr(klwp_id_t lwp, ucontext_t *uc)
{
	uc->uc_mcontext.xrs.xrs_id = 0;
	uc->uc_mcontext.xrs.xrs_ptr = NULL;
}

/*
 * indicate whether or not an extra register state
 * pointer is associated with a struct ucontext
 */
/*ARGSUSED*/
int
xregs_hasptr(klwp_id_t lwp, ucontext_t *uc)
{
	return (uc->uc_mcontext.xrs.xrs_id == XRS_ID);
}

/*
 * get the struct ucontext extra register state pointer field
 */
/*ARGSUSED*/
caddr_t
xregs_getptr(klwp_id_t lwp, ucontext_t *uc)
{
	if (uc->uc_mcontext.xrs.xrs_id == XRS_ID)
		return (uc->uc_mcontext.xrs.xrs_ptr);
	return (NULL);
}

/*
 * set the struct ucontext extra register state pointer field
 */
/*ARGSUSED*/
void
xregs_setptr(klwp_id_t lwp, ucontext_t *uc, caddr_t xrp)
{
	uc->uc_mcontext.xrs.xrs_id = XRS_ID;
	uc->uc_mcontext.xrs.xrs_ptr = xrp;
}

/*
 * extra register state manipulation routines
 */

int xregs_exists = 0;

/*
 * fill in the extra register state area specified with
 * the specified lwp's extra register state information
 */
/*ARGSUSED*/
void
xregs_get(klwp_id_t lwp, caddr_t xrp)
{}

/*
 * fill in the extra register state area specified with the
 * specified lwp's non-floating-point extra register state
 * information
 */
/*ARGSUSED*/
void
xregs_getgregs(klwp_id_t lwp, caddr_t xrp)
{}

/*
 * fill in the extra register state area specified with the
 * specified lwp's floating-point extra register state information
 */
/*ARGSUSED*/
void
xregs_getfpregs(klwp_id_t lwp, caddr_t xrp)
{}

/*
 * set the specified lwp's extra register
 * state based on the specified input
 */
/*ARGSUSED*/
void
xregs_set(klwp_id_t lwp, caddr_t xrp)
{}

/*
 * set the specified lwp's non-floating-point extra
 * register state based on the specified input
 */
/*ARGSUSED*/
void
xregs_setgregs(klwp_id_t lwp, caddr_t xrp)
{}

/*
 * set the specified lwp's floating-point extra
 * register state based on the specified input
 */
/*ARGSUSED*/
void
xregs_setfpregs(klwp_id_t lwp, caddr_t xrp)
{}

/*
 * return the size of the extra register state
 */
/*ARGSUSED*/
int
xregs_getsize(proc_t *p)
{
	return (0);
}
