/*	Copyright (c) 1991, 1996, 1997 Sun Microsystems, Inc (SMI) */
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ufs_log.c	1.10	97/11/14 SMI"

#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/modctl.h>
#include <sys/errno.h>
#include <sys/fs/ufs_inode.h>

#include <sys/fs/ufs_filio.h>
#include <sys/fs/ufs_log.h>

/*
 * UFS to LUFS OPS
 *	Entry points are NULL until lufs module is loaded.
 *	Except for ``lufs_snarf'' which will attempt to load
 *	the module before giving up.
 */
extern int	ufs_lufs_nosys(), ufs_lufs_snarf();
extern void	ufs_lufs_nosys_void();
struct lufsops lufsops = {
	ufs_lufs_nosys,		/* enable */
	ufs_lufs_nosys,		/* disable */
	ufs_lufs_snarf,		/* snarf */
	ufs_lufs_nosys_void,	/* unsnarf */
	ufs_lufs_nosys_void,	/* empty */
	ufs_lufs_nosys_void,	/* strategy */
};
/*
 * Dummy lufs ops
 */
int
ufs_lufs_nosys()
{
	return (ENOTSUP);
}
void
ufs_lufs_nosys_void()
{
}
int
ufs_lufs_snarf(ufsvfs_t *ufsvfsp, struct fs *fs, int ronly)
{
	int		error = 0;
	static int	lufs_load();

	error = lufs_load();
	if (error == 0)
		if (lufsops.lufs_snarf != ufs_lufs_snarf)
			LUFS_SNARF(ufsvfsp, fs, ronly, error);
	return (error);
}

static kmutex_t lufs_mutex;

void
ufs_lufs_init(void)
{
	mutex_init(&lufs_mutex, NULL, MUTEX_DEFAULT, NULL);
}

/*
 * Internal Logging UFS (embedded harpy logging from SDS 4.1)
 */
/*
 * Load the logging misc module (/kernel/misc/lufs)
 */
static int
lufs_load()
{
	int	id, error = 0;

	mutex_enter(&lufs_mutex);
	if (lufsops.lufs_snarf == ufs_lufs_snarf) {
		id = modload("misc", "ufs_log");
		if (id == -1)
			error = ENOTSUP;
	}
	mutex_exit(&lufs_mutex);
	return (error);
}

/* ARGSUSED */
int
ufs_fiologenable(struct vnode *vp, struct fiolog *ufl, struct cred *cr)
{
	int		error = 0;
	fiolog_t	fl;

	/*
	 * Load /kernel/misc/lufs
	 */
	error = lufs_load();
	if (error)
		return (error);
	/*
	 * Enable logging
	 */
	if (copyin(ufl, &fl, sizeof (fl)))
		return (EFAULT);
	LUFS_ENABLE(vp, &fl, cr, error);
	if (copyout(&fl, ufl, sizeof (*ufl)))
		return (EFAULT);

	return (error);
}

/* ARGSUSED */
int
ufs_fiologdisable(struct vnode *vp, struct fiolog *ufl, struct cred *cr)
{
	int		error = 0;
	struct fiolog	fl;

	/*
	 * Load /kernel/misc/lufs
	 */
	error = lufs_load();
	if (error)
		return (error);

	/*
	 * Disable logging
	 */
	if (copyin(ufl, &fl, sizeof (fl)))
		return (EFAULT);
	LUFS_DISABLE(vp, &fl, error);
	if (copyout(&fl, ufl, sizeof (*ufl)))
		return (EFAULT);

	return (error);
}

/*
 * ufs_fioislog
 *	Return true if log is present and active; otherwise false
 */
/* ARGSUSED */
int
ufs_fioislog(struct vnode *vp, uint32_t *islog, struct cred *cr)
{
	ufsvfs_t	*ufsvfsp	= VTOI(vp)->i_ufsvfs;

	if (suword32(islog, (ufsvfsp && ufsvfsp->vfs_log)))
		return (EFAULT);
	return (0);
}
