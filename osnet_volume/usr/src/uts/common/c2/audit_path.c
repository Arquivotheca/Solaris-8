/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audit_path.c	1.24	97/11/20 SMI"

/*
 * @(#)audit_path.c 2.7 92/02/16 SMI; SunOS CMW
 * @(#)audit_path.c 4.2.1.2 91/05/08 SMI; BSM Module
 *
 * This code does the audit path processes. Part of this is still in
 * audit.c and will be moved here when time permits.
 *
 * Note that audit debuging is enabled here. We will turn it off at
 * beta shipment.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/kmem.h>		/* for KM_SLEEP */
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/pathname.h>
#include <sys/acct.h>
#include <sys/cmn_err.h>
#include <c2/audit.h>
#include <c2/audit_kernel.h>
#include <c2/audit_record.h>
#include <sys/sysmacros.h>

extern kmutex_t  au_stat_lock;
extern kmutex_t  au_membuf_lock;
extern kmutex_t  cwrd_lock;


#define	ROOT 1		/* copy only ROOT path */
#define	DIR  2		/* copy only DIR  path */

/*
 * au_wait is, by default, always M_WAIT and is used to control how
 * au_getclr will obtain au_membufs for audit tokens and other uses. In
 * the case of interrupt routines and timeout's, we don't want to sleep
 * if au_membufs are low. The interrupt routine or whoever is going to perform
 * auditing must change au_wait to M_DONTWAIT; au_close will change this
 * flag back to M_WAIT by default.
 */

int au_wait = WAIT;

/*
 * These are the routines which keep the p_path structure up to date.
 */

/*
 * ROUTINE:	GETCW
 * PURPOSE:	Allocate memory for audit data current working directory
 * CALLBY:	AUDIT_INIT
 * NOTE:
 * TODO:
 * QUESTION:
 */

struct cwrd *
getcw()

{	/* GETCW */

	struct cwrd *p;

	call_debug(4);
	AS_INC(as_memused, (uint)sizeof (struct cwrd));
	p = kmem_zalloc(sizeof (struct cwrd), KM_SLEEP);
	return (p);

}	/* GETCW */







/*
 * ROUTINE:	BSM_CWINCR
 * PURPOSE:	Increment the audit data current working directory reference
 *		count
 * CALLBY:	AUDIT_NEWPROC
 * NOTE:
 * TODO:
 * QUESTION:
 */

void
bsm_cwincr(cwp)

	struct cwrd *cwp;

{	/* BSM_CWINCR */

	cwp->cwrd_ref++;

}	/* BSM_CWINCR */







/*
 * ROUTINE:	BSM_CWFREE
 * PURPOSE:	Decrement the audit data current working directory
 *		referenc count and deallocate the memory if count reach zero.
 * CALLBY:	AUDIT_PFREE
 *		AUF_CHROOT
 *		AUF_FCHROOT
 *		AUF_CHDIR
 *		AUF_FCHDIR
 * NOTE:
 * TODO:
 * QUESTION:	should we clear entry to pcwd in process?
 */

void
bsm_cwfree(cwp)

	struct cwrd *cwp;

{	/* BSM_CWFREE */

	if (--(cwp->cwrd_ref) > 0)
		return;
	if (cwp->cwrd_dir != (caddr_t)0) {
		AS_DEC(as_memused, cwp->cwrd_ldbuf);
		kmem_free((caddr_t)cwp->cwrd_dir, cwp->cwrd_ldbuf);
	}
	if (cwp->cwrd_root != (caddr_t)0) {
		AS_DEC(as_memused, cwp->cwrd_lrbuf);
		kmem_free((caddr_t)cwp->cwrd_root, cwp->cwrd_lrbuf);
	}
	AS_DEC(as_memused, sizeof (struct cwrd));
	kmem_free((caddr_t)cwp, sizeof (struct cwrd));

}	/* BSM_CWFREE */







int
au_token_size(m)
	token_t *m;
{
	int i;

	if (m == (token_t *)0)
		return (0);

	for (i = 0; m != (token_t *)0; m = m->next_buf)
		i += m->len;
	return (i);
}

token_t *
au_set(cp, size)
	caddr_t  cp;
	u_int    size;
{
	au_buff_t *head;
	au_buff_t *tail;
	au_buff_t *m;
	u_int	l;

	head = (au_buff_t *)0;
	tail = (au_buff_t *)0;	/* only to satisfy lint */
	while (size) {
		m = au_get_buff(WAIT);
		l = MIN(size, AU_BUFSIZE);
		bcopy(cp, memtod(m, char *), l);
		m->len = l;

		if (head)
			tail->next_buf = m;	/* tail set if head set */
		else
			head = m;
		tail = m;
		size -= l;
		cp += l;
	}

	return (head);
}

#ifdef REMOVE
token_t *
au_dup_token(mold)
	token_t *mold;
{
	token_t *result;
	token_t *mnew;

	if (mold == (token_t *)0)
		return ((token_t *)0);

	result = au_getclr(au_wait);
	mnew = result;
	*mnew = *mold;
	for (mold = mold->next_buf; mold != (token_t *)0;
		mold = mold->next_buf) {
		mnew->next_buf = au_getclr(au_wait);
		mnew = mnew->next_buf;
		*mnew = *mold;
	}
	mnew->next_buf = (token_t *)0;
	return (result);
}

/*
 * This is REALLY bad.
 * Like dup, but add a trailing NULL byte.
 */
token_t *
au_gather(mold)
	token_t *mold;
{
	token_t *result;
	token_t *mnew;

	result = au_getclr(au_wait);
	mnew = result;
	if (mold != (token_t *)0) {
		*mnew = *mold;
		for (mold = mold->next_buf; mold != (token_t *)0;
		    mold = mold->next_buf) {
			mnew->next_buf = au_getclr(au_wait);
			mnew = mnew->next_buf;
			*mnew = *mold;
		}
		mnew->next_buf = au_getclr(au_wait);
		mnew = mnew->next_buf;
	}
	mnew->len = 1;
	mnew->next_buf = (token_t *)0;
	return (result);
}
#endif /* REMOVE */

token_t *
au_getclr(how)
	int how;
{
	token_t	*token;	/* result */

	token = (token_t *)au_get_buff(how);
	if (token == (token_t *)0)
		panic("au_getclr: No memory");
	token->len = 0;
	return (token);
}

void
au_toss_token(m)
	token_t *m;
{

	au_free_rec((au_buff_t *)m);
}







/*
 * Duplicate cwd structure, replacing root (or dir, if root == NULL).
 */

/*
 * ROUTINE:	CWDUP
 * PURPOSE:
 * CALLBY:	AUDIT_CHDIREC
 * NOTE:
 * TODO:
 * QUESTION:
 */

struct cwrd *
cwdup(ucwd, flg)

	struct cwrd *ucwd;
	int flg;

{	/* CWDUP */

	struct cwrd *cwp;

	AS_INC(as_memused, (uint)sizeof (struct cwrd));
	cwp = kmem_zalloc(sizeof (struct cwrd), KM_SLEEP);
	cwp->cwrd_ref = 1;

	if (flg & DIR) {
		/* note only necessary storage obtained */
		cwp->cwrd_ldbuf  = ucwd->cwrd_dirlen;
		cwp->cwrd_dirlen = ucwd->cwrd_dirlen;
		AS_INC(as_memused, ucwd->cwrd_dirlen);
		cwp->cwrd_dir = kmem_alloc(ucwd->cwrd_dirlen, KM_SLEEP);
		bcopy(ucwd->cwrd_dir, cwp->cwrd_dir, cwp->cwrd_dirlen);
	}

	if (flg & ROOT) {
		/* note only necessary storage obtained */
		cwp->cwrd_lrbuf   = ucwd->cwrd_rootlen;
		cwp->cwrd_rootlen = ucwd->cwrd_rootlen;
		AS_INC(as_memused, ucwd->cwrd_rootlen);
		cwp->cwrd_root = kmem_alloc(ucwd->cwrd_rootlen, KM_SLEEP);
		bcopy(ucwd->cwrd_root, cwp->cwrd_root, cwp->cwrd_rootlen);
	}
	return (cwp);

}	/* CWDUP */

token_t *
au_append_token(chain, m)
	token_t *chain;
	token_t *m;
{
	token_t *mbp;

	if (chain == (token_t *)0)
		return (m);

	if (m == (token_t *)0)
		return (chain);

	for (mbp = chain; mbp->next_buf != (token_t *)0; mbp = mbp->next_buf)
		;
	mbp->next_buf = m;
	return (chain);
}

#ifdef NOTYET
/*
 * Converts a string into a chain of au_membufs.
 */
token_t *
au_stot(d, length)
	char *d;
	u_int length;
{
	int l;
	au_buff_t *m, *n, *s;

	n = s = (au_buff_t *)0;
	while (length) {
		m = au_getclr(au_wait);
		l = MIN(length, AU_BUFSIZE);
		au_append_buf(d, l, m);
		if (n == (au_buff_t *)0)
			s = n = m;
		else {
			n->next_buf = m;
			n = m;
		}
		length -= l;
		d += l;
	}
	return (s);
}
#endif	/* NOTYET */








u_int
audit_fixpath(s, ls)
	char *s;	/* source path */
	u_int ls;	/* length of source string */
{
	int id;		/* index of where we are in destination string */
	int is;		/* index of where we are in source string */
	int slashseen;	/* have we seen a slash */

	if (*s != '/') {
		printf("audit_copypath input %p : %s\n", (void *)s, s);
		panic("bad audit_copypath input");
	}
	slashseen = 0;
	for (is = 0, id = 0; is < ls; is++) {
			/* thats all folks, we've reached the end of input */
		if (s[is] == '\0') {
			if (id > 1 && s[id-1] == '/') {
				--id;
			}
			s[id++] = '\0';
			break;
		}
			/* previous character was a / */
		if (slashseen) {
			if (s[is] == '/')
				continue;	/* another slash, ignore it */
		} else if (s[is] == '/') {
				/* we see a /, just copy it and try again */
			slashseen = 1;
			s[id++] = '/';
			continue;
		}
			/* /./ seen */
		if (s[is] == '.' && s[is+1] == '/') {
			is += 1;
			continue;
		}
			/* XXX/. seen */
		if (s[is] == '.' && s[is+1] == '\0') {
			if (id > 1) id--;
			continue;
		}
			/* XXX/.. seen */
		if (s[is] == '.' && s[is+1] == '.' && s[is+2] == '\0') {
			is += 1;
			if (id > 0) id--;
			while (id > 0 && s[--id] != '/');
			id++;
			continue;
		}
			/* XXX/../ seen */
		if (s[is] == '.' && s[is+1] == '.' && s[is+2] == '/') {
			is += 2;
			if (id > 0) id--;
			while (id > 0 && s[--id] != '/');
			id++;
			continue;
		}
		while (is < ls && (s[id++] = s[is++]) != '/');
		is--;
	}
	return (id);
}
