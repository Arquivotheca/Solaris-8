/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)intp.c	1.24	97/06/22 SMI" /* from S5R4 1.6 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pathname.h>
#include <sys/disp.h>
#include <sys/exec.h>
#include <sys/kmem.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern int intpexec();

static struct execsw esw = {
	intpmagicstr,
	0,
	2,
	intpexec,
	NULL
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_execops;

static struct modlexec modlexec = {
	&mod_execops, "exec mod for interp", &esw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlexec, NULL
};

_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	return (mod_remove(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * Crack open a '#!' line.
 */
static int
getintphead(struct vnode *vp, struct intpdata *idatap)
{
	int error;
	char *cp, *linep = idatap->intp;
	ssize_t resid;

	/*
	 * Read the entire line and confirm that it starts with '#!'.
	 */
	if (error = vn_rdwr(UIO_READ, vp, linep, INTPSZ, (offset_t)0,
	    UIO_SYSSPACE, 0, (rlim64_t)0, CRED(), &resid))
		return (error);
	if (resid > INTPSZ-2 || linep[0] != '#' || linep[1] != '!')
		return (ENOEXEC);
	/*
	 * Blank all white space and find the newline.
	 */
	for (cp = &linep[2]; cp < &linep[INTPSZ] && *cp != '\n'; cp++)
		if (*cp == '\t')
			*cp = ' ';
	if (cp >= &linep[INTPSZ])
		return (ENOEXEC);
	ASSERT(*cp == '\n');
	*cp = '\0';

	/*
	 * Locate the beginning and end of the interpreter name.
	 * In addition to the name, one additional argument may
	 * optionally be included here, to be prepended to the
	 * arguments provided on the command line.  Thus, for
	 * example, you can say
	 *
	 * 	#! /usr/bin/awk -f
	 */
	for (cp = &linep[2]; *cp == ' '; cp++)
		;
	if (*cp == '\0')
		return (ENOEXEC);
	idatap->intp_name = cp;
	while (*cp && *cp != ' ')
		cp++;
	if (*cp == '\0')
		idatap->intp_arg = NULL;
	else {
		*cp++ = '\0';
		while (*cp == ' ')
			cp++;
		if (*cp == '\0')
			idatap->intp_arg = NULL;
		else {
			idatap->intp_arg = cp;
			while (*cp && *cp != ' ')
				cp++;
			*cp = '\0';
		}
	}
	return (0);
}

int
intpexec(
	struct vnode *vp,
	struct execa *uap,
	struct uarg *args,
	struct intpdata *idatap,
	int level,
	long *execsz,
	int setid,
	caddr_t exec_file,
	struct cred *cred)
{
	vnode_t *nvp;
	int error = 0;
	struct intpdata idata;
	struct pathname intppn;
	char devfd[14];
	int fd = -1;

	if (level) {		/* Can't recurse */
		error = ENOEXEC;
		goto bad;
	}

	ASSERT(idatap == (struct intpdata *)NULL);

	/*
	 * Allocate a buffer to read in the interpreter pathname.
	 */
	idata.intp = kmem_alloc(INTPSZ, KM_SLEEP);
	if (error = getintphead(vp, &idata))
		goto fail;

	/*
	 * Look the new vnode up.
	 */
	if (error = pn_get(idata.intp_name, UIO_SYSSPACE, &intppn))
		goto fail;
	if (error = lookuppn(&intppn, NULL, FOLLOW, NULLVPP, &nvp)) {
		pn_free(&intppn);
		goto fail;
	}
	pn_free(&intppn);

	if (setid) { /* close security hole */
		(void) strcpy(devfd, "/dev/fd/");
		if (error = execopen(&vp, &fd)) {
			VN_RELE(nvp);
			goto fail;
		}
		numtos(fd, &devfd[8]);
		args->fname = devfd;
	}

	error = gexec(&nvp, uap, args, &idata, ++level,
		execsz, exec_file, cred);
	VN_RELE(nvp);

fail:
	kmem_free(idata.intp, INTPSZ);
	if (error && fd != -1)
		(void) execclose(fd);
bad:
	return (error);
}
