/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)share.c	1.10	98/07/17 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/share.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/t_lock.h>
#include <sys/errno.h>

int share_debug = 0;
#ifdef DEBUG
static void print_shares(struct vnode *);
static void print_share(struct shrlock *);
#endif

static int isreadonly(struct vnode *);

int
add_share(struct vnode *vp, struct shrlock *shr)
{
	struct shrlocklist *shrl;

	/*
	 * An access of zero is not legal, however some older clients
	 * generate it anyways.  Allow the request only if it is
	 * coming from a remote system.  Be generous in what you
	 * accept and strict in what you send.
	 */
	if ((shr->s_access == 0) && (GETSYSID(shr->s_sysid) == 0)) {
		return (EINVAL);
	}

	/*
	 * Sanity check to make sure we have valid options.
	 * There is known overlap but it doesn't hurt to be careful.
	 */
	if (shr->s_access & ~(F_RDACC|F_WRACC|F_RWACC)) {
		return (EINVAL);
	}
	if (shr->s_deny & ~(F_NODNY|F_RDDNY|F_WRDNY|F_RWDNY|F_COMPAT)) {
		return (EINVAL);
	}

	mutex_enter(&vp->v_lock);
	for (shrl = vp->v_shrlocks; shrl != NULL; shrl = shrl->next) {
		/*
		 * If the share owner matches previous request
		 * do special handling.
		 */
		if ((shrl->shr->s_sysid == shr->s_sysid) &&
		    (shrl->shr->s_pid == shr->s_pid) &&
		    (shrl->shr->s_own_len == shr->s_own_len) &&
		    bcmp(shrl->shr->s_owner, shr->s_owner,
		    shr->s_own_len) == 0) {

			/*
			 * If the existing request is F_COMPAT and
			 * is the first share then allow any F_COMPAT
			 * from the same process.  Trick:  If the existing
			 * F_COMPAT is write access then it must have
			 * the same owner as the first.
			 */
			if ((shrl->shr->s_deny & F_COMPAT) &&
			    (shr->s_deny & F_COMPAT) &&
			    ((shrl->next == NULL) ||
				(shrl->shr->s_access & F_WRACC)))
				break;
		}

		/*
		 * If a first share has been done in compatibility mode
		 * handle the special cases.
		 */
		if ((shrl->shr->s_deny & F_COMPAT) && (shrl->next == NULL)) {

			if (!(shr->s_deny & F_COMPAT)) {
				/*
				 * If not compat and want write access or
				 * want to deny read or
				 * write exists, fails
				 */
				if ((shr->s_access & F_WRACC) ||
				    (shr->s_deny & F_RDDNY) ||
				    (shrl->shr->s_access & F_WRACC)) {
					mutex_exit(&vp->v_lock);
					return (EAGAIN);
				}
				/*
				 * If read only file allow, this may allow
				 * a deny write but that is meaningless on
				 * a read only file.
				 */
				if (isreadonly(vp))
					break;
				mutex_exit(&vp->v_lock);
				return (EAGAIN);
			}
			/*
			 * This is a compat request and read access
			 * and the first was also read access
			 * we always allow it, otherwise we reject because
			 * we have handled the only valid write case above.
			 */
			if ((shr->s_access == F_RDACC) &&
			    (shrl->shr->s_access == F_RDACC))
				break;
			mutex_exit(&vp->v_lock);
			return (EAGAIN);
		}

		/*
		 * If we are trying to share in compatibility mode
		 * and the current share is compat (and not the first)
		 * we don't know enough.
		 */
		if ((shrl->shr->s_deny & F_COMPAT) && (shr->s_deny & F_COMPAT))
			continue;

		/*
		 * If this is a compat we check for what can't succeed.
		 */
		if (shr->s_deny & F_COMPAT) {
			/*
			 * If we want write access or
			 * if anyone is denying read or
			 * if anyone has write access we fail
			 */
			if ((shr->s_access & F_WRACC) ||
			    (shrl->shr->s_deny & F_RDDNY) ||
			    (shrl->shr->s_access & F_WRACC)) {
				mutex_exit(&vp->v_lock);
				return (EAGAIN);
			}
			/*
			 * If the first was opened with only read access
			 * and is a read only file we allow.
			 */
			if (shrl->next == NULL) {
				if ((shrl->shr->s_access == F_RDACC) &&
				    isreadonly(vp)) {
					break;
				}
				mutex_exit(&vp->v_lock);
				return (EAGAIN);
			}
			/*
			 * We still can't determine our fate so continue
			 */
			continue;
		}

		/*
		 * Simple bitwise test, if we are trying to access what
		 * someone else is denying or we are trying to deny
		 * what someone else is accessing we fail.
		 */
		if ((shr->s_access & shrl->shr->s_deny) ||
		    (shr->s_deny & shrl->shr->s_access)) {
			mutex_exit(&vp->v_lock);
			return (EAGAIN);
		}
	}

	shrl = kmem_alloc(sizeof (struct shrlocklist), KM_SLEEP);
	shrl->shr = kmem_alloc(sizeof (struct shrlock), KM_SLEEP);
	shrl->shr->s_access = shr->s_access;
	shrl->shr->s_deny = shr->s_deny;

	/*
	 * Make sure no other deny modes are also set with F_COMPAT
	 */
	if (shrl->shr->s_deny & F_COMPAT)
		shrl->shr->s_deny = F_COMPAT;
	shrl->shr->s_sysid = shr->s_sysid;		/* XXX ref cnt? */
	shrl->shr->s_pid = shr->s_pid;
	shrl->shr->s_own_len = shr->s_own_len;
	shrl->shr->s_owner = kmem_alloc(shr->s_own_len, KM_SLEEP);
	bcopy(shr->s_owner, shrl->shr->s_owner, shr->s_own_len);
	shrl->next = vp->v_shrlocks;
	vp->v_shrlocks = shrl;
#ifdef DEBUG
	if (share_debug)
		print_shares(vp);
#endif
	mutex_exit(&vp->v_lock);
	return (0);
}

/*
 *	nlmid	sysid	pid
 *	=====	=====	===
 *	!=0	!=0	=0	in cluster; NLM lock
 *	!=0	=0	=0	in cluster; special case for NLM lock
 *	!=0	=0	!=0	in cluster; PXFS local lock
 *	!=0	!=0	!=0	cannot happen
 *	=0	!=0	=0	not in cluster; NLM lock
 *	=0	=0	!=0	not in cluster; local lock
 *	=0	=0	=0	cannot happen
 *	=0	!=0	!=0	cannot happen
 */
static int
is_match_for_del(struct shrlock *shr, struct shrlock *element)
{
	int nlmid1, nlmid2;
	int result = 0;

	nlmid1 = GETNLMID(shr->s_sysid);
	nlmid2 = GETNLMID(element->s_sysid);

	if (nlmid1 != 0) {		/* in a cluster */
		if (GETSYSID(shr->s_sysid) != 0 && shr->s_pid == 0) {
			/*
			 * Lock obtained through nlm server.  Just need to
			 * compare whole sysids.  pid will always = 0.
			 */
			result = shr->s_sysid == element->s_sysid;
		} else if (GETSYSID(shr->s_sysid) == 0 && shr->s_pid == 0) {
			/*
			 * This is a special case.  The NLM server wishes to
			 * delete all share locks obtained through nlmid1.
			 */
			result = (nlmid1 == nlmid2);
		} else if (GETSYSID(shr->s_sysid) == 0 && shr->s_pid != 0) {
			/*
			 * Lock obtained locally through PXFS.  Match nlmids
			 * and pids.
			 */
			result = (nlmid1 == nlmid2 &&
				shr->s_pid == element->s_pid);
		}
	} else {			/* not in a cluster */
		result = ((shr->s_sysid == 0 &&
			shr->s_pid == element->s_pid) ||
			(shr->s_sysid != 0 &&
				shr->s_sysid == element->s_sysid));
	}
	return (result);
}

int
del_share(struct vnode *vp, struct shrlock *shr)
{
	struct shrlocklist *shrl;
	struct shrlocklist **shrlp;
	int found = 0;

	mutex_enter(&vp->v_lock);
	/*
	 * Delete the shares with the matching sysid and owner
	 * But if own_len == 0 and sysid == 0 delete all with matching pid
	 * But if own_len == 0 delete all with matching sysid.
	 */
	shrlp = &vp->v_shrlocks;
	while (*shrlp) {
		if ((shr->s_own_len == (*shrlp)->shr->s_own_len &&
				    (bcmp(shr->s_owner, (*shrlp)->shr->s_owner,
						shr->s_own_len) == 0)) ||

			(shr->s_own_len == 0 &&
				is_match_for_del(shr, (*shrlp)->shr))) {

			shrl = *shrlp;
			*shrlp = shrl->next;

			/* XXX deref sysid */
			kmem_free(shrl->shr->s_owner, shrl->shr->s_own_len);
			kmem_free(shrl->shr, sizeof (struct shrlock));
			kmem_free(shrl, sizeof (struct shrlocklist));
			found++;
			continue;
		}
		shrlp = &(*shrlp)->next;
	}

	mutex_exit(&vp->v_lock);
	return (found ? 0 : EINVAL);
}

/*
 * Clean up all local share reservations
 */
void
cleanshares(struct vnode *vp, pid_t pid)
{
	struct shrlock shr;

	if (vp->v_shrlocks == NULL)
		return;

	shr.s_access = 0;
	shr.s_deny = 0;
	shr.s_pid = pid;
	shr.s_sysid = 0;
	shr.s_own_len = 0;
	shr.s_owner = NULL;

	(void) del_share(vp, &shr);
}

static int
is_match_for_has_remote(int32_t sysid1, int32_t sysid2)
{
	int result = 0;

	if (GETNLMID(sysid1) != 0) { /* in a cluster */
		if (GETSYSID(sysid1) != 0) {
			/*
			 * Lock obtained through nlm server.  Just need to
			 * compare whole sysids.
			 */
			result = (sysid1 == sysid2);
		} else if (GETSYSID(sysid1) == 0) {
			/*
			 * This is a special case.  The NLM server identified
			 * by nlmid1 wishes to find out if it has obtained
			 * any share locks on the vnode.
			 */
			result = (GETNLMID(sysid1) == GETNLMID(sysid2));
		}
	} else {			/* not in a cluster */
		result = ((sysid1 != 0 && sysid1 == sysid2) ||
		    (sysid1 == 0 && sysid2 != 0));
	}
	return (result);
}


/*
 * Determine whether there are any shares for the given vnode
 * with a remote sysid. Returns zero if not, non-zero if there are.
 * If sysid is non-zero then determine if this sysid has a share.
 *
 * Note that the return value from this function is potentially invalid
 * once it has been returned.  The caller is responsible for providing its
 * own synchronization mechanism to ensure that the return value is useful.
 */
int
shr_has_remote_shares(vnode_t *vp, int32_t sysid)
{
	struct shrlocklist *shrl;
	int result = 0;

	mutex_enter(&vp->v_lock);
	shrl = vp->v_shrlocks;
	while (shrl) {
		if (is_match_for_has_remote(sysid, shrl->shr->s_sysid)) {

			result = 1;
			break;
		}
		shrl = shrl->next;
	}
	mutex_exit(&vp->v_lock);
	return (result);
}

static int
isreadonly(struct vnode *vp)
{
	return (vp->v_type != VCHR && vp->v_type != VBLK &&
		vp->v_type != VFIFO && (vp->v_vfsp->vfs_flag & VFS_RDONLY));
}

#ifdef DEBUG
static void
print_shares(struct vnode *vp)
{
	struct shrlocklist *shrl;

	if (vp->v_shrlocks == NULL) {
		printf("<NULL>\n");
		return;
	}

	shrl = vp->v_shrlocks;
	while (shrl) {
		print_share(shrl->shr);
		shrl = shrl->next;
	}
}

static void
print_share(struct shrlock *shr)
{
	int i;

	if (shr == NULL) {
		printf("<NULL>\n");
		return;
	}

	printf("    access(%d):	", shr->s_access);
	if (shr->s_access & F_RDACC)
		printf("R");
	if (shr->s_access & F_WRACC)
		printf("W");
	if ((shr->s_access & (F_RDACC|F_WRACC)) == 0)
		printf("N");
	printf("\n");
	printf("    deny:	");
	if (shr->s_deny & F_COMPAT)
		printf("C");
	if (shr->s_deny & F_RDDNY)
		printf("R");
	if (shr->s_deny & F_WRDNY)
		printf("W");
	if (shr->s_deny == F_NODNY)
		printf("N");
	printf("\n");
	printf("    sysid:	%d\n", shr->s_sysid);
	printf("    pid:	%d\n", shr->s_pid);
	printf("    owner:	[%d]", shr->s_own_len);
	printf("'");
	for (i = 0; i < shr->s_own_len; i++)
		printf("%02x", (unsigned)shr->s_owner[i]);
	printf("'\n");
}
#endif
