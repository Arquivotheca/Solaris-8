/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vold_label.c	1.20	96/03/06 SMI"

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<ctype.h>
#include	<pwd.h>
#include	<grp.h>
#include	<syslog.h>
#include	<errno.h>
#include	<string.h>
#include	<rpc/rpc.h>
#include	<sys/param.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<sys/time.h>
#include	<sys/stat.h>
#include	<rpcsvc/nfs_prot.h>
#include	<netinet/in.h>
#include	<sys/mnttab.h>
#include	<sys/mntent.h>
#include	<sys/mount.h>
#include	<netdb.h>
#include	<sys/signal.h>
#include	<sys/file.h>
#include	<setjmp.h>
#include	<netconfig.h>
#include	<locale.h>
#include	<ulimit.h>
#include	<dlfcn.h>

#include	"vold.h"
#include	"label.h"

#define	LAB_ALLOC_CHUNK	10

static struct labsw 	**labsw;
static struct labsw	*lastlabsw;
static int 		nlabs = 0;
static int		nalloclabs = 0;


void
label_new(struct labsw *lsw)
{
	int		i;
	int		na;			/* for nalloclabs */
	struct labsw	**nlabsw; 		/* for new labsw */


	/* see if the label already exists */
	for (i = 0; i < nlabs; i++) {
		if (labsw[i] == lsw) {
			/* label already in list */
			lastlabsw = lsw;
			return;
		}
	}

	/* see if we need to allocate another chunk of label structs */
	if (nlabs == nalloclabs) {
		/* is the list empty ?? */
		if (labsw == 0) {
			/* allocate first block of label pointers */
			nalloclabs = LAB_ALLOC_CHUNK;
			labsw = (struct labsw **)calloc(nalloclabs,
			    sizeof (struct labsw *));
		} else {
			/* add a block of pointers */
			na = nalloclabs;
			nalloclabs += LAB_ALLOC_CHUNK;
			nlabsw = (struct labsw **)calloc(nalloclabs,
			    sizeof (struct labsw *));
			/* copy over the current struct pointers */
			for (i = 0; i < na; i++) {
				nlabsw[i] = labsw[i];
			}
			free(labsw);		/* free old block */
			labsw = nlabsw;
		}
	}

	/* allocate the next label from the block */
	labsw[nlabs++] = lsw;

	/* keep track of last one allocated */
	lastlabsw = lsw;
}


/*
 * Yes, well, this is a really ugly but it lets us build l_devlist
 * in the configuration code.  This interface should probably be
 * rethought.
 */
struct labsw *
label_getlast()
{
	return (lastlabsw);
}


size_t
label_size(int type)
{
	return (labsw[type]->l_size);
}


bool_t
label_compare(label *la1, label *la2)
{
	/* if types don't match forget it */
	if (la1->l_type != la2->l_type) {
		return (FALSE);
	}

	/* call the compare routine for label's type */
	return ((*labsw[la1->l_type]->l_compare)(la1, la2));
}


char *
label_key(label *la)
{
	return ((*labsw[la->l_type]->l_key)(la));
}


char *
label_ident(int type)
{
	if (type == -1) {
		return ("error");
	}
	return (labsw[type]->l_ident);
}


size_t
label_xdrsize(int type)
{
	return (labsw[type]->l_xdrsize);
}


int
label_type(char *s)
{
	int	i;

	for (i = 0; i < nlabs; i++) {
		if (strcmp(s, labsw[i]->l_ident) == 0) {
			return (i);
		}
	}

	debug(1, "No support for label type %s on this machine\n", s);
	return (-1);
}


enum laread_res
label_scan(int fd, char *devtype, label *la, struct devs *dp)
{
	enum laread_res	res = L_UNRECOG;
	struct labsw	*lsw;
	int		i;
	int		j;


	/* scan each label type */
	for (i = 0; i < nlabs; i++) {
		lsw = labsw[i];
		for (j = 0; lsw->l_devlist[j]; j++) {
			if (strcmp(lsw->l_devlist[j], devtype) == 0) {
				res = (*lsw->l_read)(fd, la, dp);
				if (res != L_UNRECOG) {
					la->l_type = i;
					return (res);
				}
			}
		}
	}
	la->l_type = -1;
	return (res);	/* probably L_UNRECOG */
}


/* XXX: not currently used */
enum laread_res
label_read(int fd, label *la, struct devs *dp)
{
	return ((*labsw[la->l_type]->l_read)(fd, la, dp));
}


/* XXX: not currently used */
int
label_write(char *name, label *la)
{
	return ((*labsw[la->l_type]->l_write)(name, la));
}


void
label_xdr(label *l, enum xdr_op op, void **data)
{
	if (l->l_type != -1) {
		(*labsw[l->l_type]->l_xdr)(l, op, data);
	} else {
		debug(1, "couldn't map label type\n");
	}
}


void
label_setup(label *la, vol_t *v, struct devs *dp)
{
	struct devsw	*dsw = dp->dp_dsw;


	v->v_label.l_type = la->l_type;
	v->v_label.l_label = la->l_label;
	v->v_obj.o_gid = (gid_t)-1;
	v->v_obj.o_uid = (uid_t)-1;
	v->v_obj.o_mode = (mode_t)-1;
	v->v_obj.o_ctime.tv_sec = 0;
	v->v_obj.o_mtime.tv_sec = 0;
	v->v_obj.o_nlinks = 1;
	/*
	 * call the label function to see if it can get any of this stuff
	 * off the label.
	 */
	if (labsw[la->l_type]->l_setup) {
		(*labsw[la->l_type]->l_setup)(v);
	}

	if (v->v_obj.o_uid == -1) {
		v->v_obj.o_uid = dsw->d_uid;
	}

	if (v->v_obj.o_gid == -1) {
		v->v_obj.o_gid = dsw->d_gid;
	}

	if (v->v_obj.o_mode == -1) {
		v->v_obj.o_mode = dsw->d_mode;
	}

	if (v->v_obj.o_ctime.tv_sec == 0) {
		v->v_obj.o_ctime = current_time;
	}

	if (v->v_obj.o_mtime.tv_sec == 0) {
		v->v_obj.o_mtime = current_time;
	}

	if (dp->dp_dsw->d_flags & D_RMONEJECT) {
		v->v_flags |= V_RMONEJECT;
	}

	if (dp->dp_dsw->d_flags & D_MEJECTABLE) {
		v->v_flags |= V_MEJECTABLE;
	}
}
