/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident   "@(#)benv_sync.c 1.5 99/12/06 SMI"

#include "benv.h"
#include <unistd.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

extern ddi_prop_t *get_proplist(char *name);
extern int verbose;

/*
 * Delayed-write descriptor.
 */
typedef struct dw_des {
	char *path;		/* file path */
	list_t *wlist;		/* list of writes to the file */
} dw_des_t;

/*
 * Delayed-write request.
 */
typedef struct dw_req {
	caddr_t adr;		/* data address */
	u_int len;		/* data length */
	int num;		/* write number */
} dw_req_t;

static dw_des_t *
new_dwd(char *path)
{
	dw_des_t *dwd;

	dwd = (dw_des_t *)malloc(sizeof (dw_des_t));
	(void) memset(dwd, 0, sizeof (dw_des_t));

	dwd->path = path;
	dwd->wlist = new_list();

	return (dwd);
}

/*
 * Parse a delayed-write property into a delayed-write request.
 * The property name is of the form 'writeN' where N represents
 * the Nth write request to the pathname encoded in the property's value.
 */
static void
parse_dw(ddi_prop_t *prop, char **path, dw_req_t **dwrp)
{
	int plen;
	caddr_t tok;
	dw_req_t *dwr;

	dwr = (dw_req_t *)malloc(sizeof (dw_req_t));
	dwr->num = atoi(&prop->prop_name[5]);

	tok = prop->prop_val;
	plen = *((int *)tok);
	tok += 4;
	*path = (char *)tok;
	tok += plen;
	dwr->len = *((int *)tok);
	tok += 4;
	dwr->adr = tok;

	*dwrp = dwr;
}

/*
 * Sync a delayed-write out request to a file.
 */
static void
sync_dwr(char *relpath, dw_req_t *dwr)
{
	int fd;
	char *p, *path;

	path = strcats(boottree, "/", relpath, NULL);
	p = strrchr(path, '/');
	*p = NULL;


	if (access(path, F_OK) != 0) {
		if (mkdirp(path, 755) == -1)
			exit(_error(PERROR, "cannot mkdir %s", path));
	}

	*p = '/';

	if ((fd = open(path, O_RDWR|O_CREAT|O_TRUNC, 0644)) == -1)
		exit(_error(PERROR, "cannot open %s", path));


	if (write(fd, dwr->adr, dwr->len) != dwr->len)
		exit(_error(PERROR, "cannot write %s", path));

	if (verbose)
		printf("sync \"%s\" size %d data <%s>\n",
		    path, dwr->len, dwr->adr);

	close(fd);
	free(path);
}

/*
 * Sync all delayed-write requests from the boot.
 */
void
sync_benv(void)
{
	ddi_prop_t *prop, *plist;
	list_t *dwlp, *pl, *wl;
	dw_des_t *dwd;
	dw_req_t *dwr, *dwrlast;

	if ((plist = get_proplist("delayed-writes")) == NULL) {
		if (verbose)
			printf("no delayed writes.\n");
		return;
	}

	dwlp = new_list();

	/*
	 * Parse each property into a delayed-write request for
	 * for a given file.
	 */
	for (prop = plist; prop != NULL; prop = prop->prop_next) {
		if (strncmp(prop->prop_name, "write", 5) == 0) {
			char *path;

			parse_dw(prop, &path, &dwr);

			if (verbose)
				printf("write \"%s\"\n", path);

			/*
			 * Get a delayed-write descriptor for each
			 * new file.  Add all delayed-write requests
			 * for a given file to the descriptor's request
			 * list.
			 */
			for (pl = dwlp->next; pl != dwlp; pl = pl->next) {
				dwd = (dw_des_t *)pl->item;

				if (strcmp(dwd->path, path) == 0)
					break;
			}

			if (pl == dwlp) {
				dwd = new_dwd(path);
				add_item((void *)dwd, dwlp);
			}

			add_item((void *)dwr, dwd->wlist);
		}
	}

	/*
	 * Process the last request for each path.  We only need to
	 * do the last request since the writes are destructive and
	 * overwrite all previous file contents.
	 */
	for (pl = dwlp->next; pl != dwlp; pl = pl->next) {
		dwd = (dw_des_t *)(pl->item);
		dwrlast = NULL;

		for (wl = dwd->wlist->next; wl != dwd->wlist; wl = wl->next) {
			dwr = (dw_req_t *)(wl->item);
			if (dwrlast == NULL || dwr->num >= dwrlast->num)
				dwrlast = dwr;
		}

		if (dwrlast != NULL)
			sync_dwr(dwd->path, dwrlast);
	}
}
