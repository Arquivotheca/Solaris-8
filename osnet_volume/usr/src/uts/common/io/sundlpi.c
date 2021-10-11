#ident	"@(#)sundlpi.c	1.10	97/03/10 SMI"

/*
 * Copyright (c) 1990-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  Common Sun DLPI routines.
 */

#include	<sys/types.h>
#include	<sys/sysmacros.h>
#include	<sys/systm.h>
#include	<sys/stream.h>
#include	<sys/strsun.h>
#include	<sys/dlpi.h>

#define		DLADDRL		(80)

void
dlbindack(
	queue_t		*wq,
	mblk_t		*mp,
	t_scalar_t	sap,
	void		*addrp,
	t_uscalar_t	addrlen,
	t_uscalar_t	maxconind,
	t_uscalar_t	xidtest)
{
	union DL_primitives	*dlp;
	size_t			size;

	size = sizeof (dl_bind_ack_t) + addrlen;
	if ((mp = mexchange(wq, mp, size, M_PCPROTO, DL_BIND_ACK)) == NULL)
		return;

	dlp = (union DL_primitives *) mp->b_rptr;
	dlp->bind_ack.dl_sap = sap;
	dlp->bind_ack.dl_addr_length = addrlen;
	dlp->bind_ack.dl_addr_offset = sizeof (dl_bind_ack_t);
	dlp->bind_ack.dl_max_conind = maxconind;
	dlp->bind_ack.dl_xidtest_flg = xidtest;
	bcopy(addrp, mp->b_rptr + sizeof (dl_bind_ack_t), addrlen);

	qreply(wq, mp);
}

void
dlokack(
	queue_t		*wq,
	mblk_t		*mp,
	t_uscalar_t	correct_primitive)
{
	union DL_primitives	*dlp;

	if ((mp = mexchange(wq, mp, sizeof (dl_ok_ack_t), M_PCPROTO,
	    DL_OK_ACK)) == NULL)
		return;
	dlp = (union DL_primitives *) mp->b_rptr;
	dlp->ok_ack.dl_correct_primitive = correct_primitive;
	qreply(wq, mp);
}

void
dlerrorack(
	queue_t		*wq,
	mblk_t		*mp,
	t_uscalar_t	error_primitive,
	t_uscalar_t	errno,
	t_uscalar_t	unix_errno)
{
	union DL_primitives	*dlp;

	if ((mp = mexchange(wq, mp, sizeof (dl_error_ack_t), M_PCPROTO,
	    DL_ERROR_ACK)) == NULL)
		return;
	dlp = (union DL_primitives *) mp->b_rptr;
	dlp->error_ack.dl_error_primitive = error_primitive;
	dlp->error_ack.dl_errno = errno;
	dlp->error_ack.dl_unix_errno = unix_errno;
	qreply(wq, mp);
}

void
dluderrorind(
	queue_t		*wq,
	mblk_t		*mp,
	void		*addrp,
	t_uscalar_t	addrlen,
	t_uscalar_t	errno,
	t_uscalar_t	unix_errno)
{
	union DL_primitives	*dlp;
	char			buf[DLADDRL];
	size_t			size;

	if (addrlen > DLADDRL)
		addrlen = DLADDRL;

	bcopy(addrp, buf, addrlen);

	size = sizeof (dl_uderror_ind_t) + addrlen;

	if ((mp = mexchange(wq, mp, size, M_PCPROTO, DL_UDERROR_IND)) == NULL)
		return;

	dlp = (union DL_primitives *) mp->b_rptr;
	dlp->uderror_ind.dl_dest_addr_length = addrlen;
	dlp->uderror_ind.dl_dest_addr_offset = sizeof (dl_uderror_ind_t);
	dlp->uderror_ind.dl_unix_errno = unix_errno;
	dlp->uderror_ind.dl_errno = errno;
	bcopy((caddr_t) buf,
		(caddr_t) (mp->b_rptr + sizeof (dl_uderror_ind_t)), addrlen);
	qreply(wq, mp);
}

void
dlphysaddrack(
	queue_t		*wq,
	mblk_t		*mp,
	void		*addrp,
	t_uscalar_t	len)
{
	union DL_primitives	*dlp;
	size_t			size;

	size = sizeof (dl_phys_addr_ack_t) + len;
	if ((mp = mexchange(wq, mp, size, M_PCPROTO, DL_PHYS_ADDR_ACK)) == NULL)
		return;
	dlp = (union DL_primitives *) mp->b_rptr;
	dlp->physaddr_ack.dl_addr_length = len;
	dlp->physaddr_ack.dl_addr_offset = sizeof (dl_phys_addr_ack_t);
	bcopy(addrp, mp->b_rptr + sizeof (dl_phys_addr_ack_t), len);
	qreply(wq, mp);
}
