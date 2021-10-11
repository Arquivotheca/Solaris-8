/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_info_olds.c	1.10	98/08/12 SMI"

/*
 * The dr_info_*.c files determine devices on a specified board and
 * their current usage by the system.
 *
 * This file deals with parsing the Sun Online DiskSuite data base
 * file and determining if board devices are part of this subsystem.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/mkdev.h>
#include <sys/modctl.h>

#include "dr_info.h"

#ifdef	DR_OLDS

#define	ONLINETAB	"/etc/opt/SUNWmd/md.cf"
#define	ONLINEDBTAB	"/etc/opt/SUNWmd/mddb.cf"

/* No system header file defines this routine -- Make lint happy */
extern int modctl();

/*
 * These structures are used to hold information about the pseudo devices
 * created by the SUN disk online suite subsystem
 */
typedef struct clist_t *clistp_t;

typedef struct clist_t {
	clistp_t	next;
	char		*cname;		/* /dev or dN name */
} clist_t;

typedef struct online_t *onlinep_t;

typedef struct online_t {
	onlinep_t	next;
	char		*name;		/* name of the pseudo device (eg d0) */
	dr_iop_t	ds_entry;	/* dr_io_t entry created for d0 */
	int		is_mirror;	/* components pseudo not real disks */
	clistp_t	component;
} online_t;

static int build_disksuite_tree(onlinep_t op, dr_iop_t dp);
static void find_online_db_entries(dr_leafp_t *leaf_array);
static onlinep_t find_online_entries(void);
static char *getline(FILE *fd);
static char *gettoken(char **bufpp);
static void free_online_entries(onlinep_t op);


/*
 * add_disksuite_to_device_tree
 *
 * Determine if the SUN online DiskSuite package is in use
 * and if it is using any disks on the board.  DiskSuite
 * pseudo devices are comprised of a number of real disk partitions.
 *
 * First we parse the DiskSuite configuration file finding all the
 * pseudo disks and their disk partition components.  Then we go
 * through all these components and see if they reside on the board.
 *
 * For each pseudo disk which uses the board, we create a dr_io_t
 * tree structure consisting of the pseudo disk (eg dN) which as two
 * leaf devices chained to it (the /dev/md/dsk and /dev/md/rdsk entries).
 * This tree is linked to the end of the sibling list of the major
 * device (eg sdN) of the first pseudo disk component found to be
 * on the board.
 *
 * For each disk partition which is a component of the pesudo disk, we save a
 * pointer to the pseudo disks dr_io_t tree.
 */
void
add_disksuite_to_device_tree(dr_leafp_t **leaf_array)
{
	onlinep_t	head, op, next, meta;
	clistp_t	cp;
	dr_leaf_t	search_node, *sp, **fp;
	int		save_num_leaves;

	/* Check for online database locations */
	find_online_db_entries(*leaf_array);

	/* if no online entries, simply return */
	if ((head = find_online_entries()) == NULL)
		return;

	/* Keep track of if we add more device leaves */
	save_num_leaves = num_leaves;

	/*
	 * Now go through the non-mirror pseudo disk entries.  If any
	 * of the component disks are on this board, then
	 * create an entry for this metadisk.
	 */
	next = head;
	while (next) {

		op = next;
		next = op->next;

		/*
		 * If this is a mirror device, it's components are OLDS
		 * metadisks.  This is handled in the next loop.
		 */
		if (op->is_mirror)
			continue;

		/*
		 * Check for component matches.  These components are
		 * /dev/dsk names.
		 */
		cp = op->component;
		while (cp) {

			/* see if this component is on the board */
			search_node.dev_name = cp->cname;
			sp = &search_node;
			fp = bsearch((void *)&sp, (void *)(*leaf_array),
				    num_leaves, sizeof (dr_leafp_t),
				    compare_devname);

			cp = cp->next;
			if (fp == NULL) {
				/* no match */
				continue;
			}

			/*
			 * Match, create and link into dr_io_t tree
			 * the entry for the pseudo disk.
			 */
			if (op->ds_entry == NULL) {

				if (build_disksuite_tree(op,
							    (*fp)->major_dev)) {
					free_online_entries(head);
					return;
				}
			}

			/*
			 * Now save a pointer to the pseudo disk in
			 * each component which is on the board.
			 */
			(*fp)->ds_dev = op->ds_entry;
		}
	}

	/*
	 * Now go through the mirror metadisks and find out if
	 * they reside on the board.  Mirrored disk componenets are
	 * metadisks which were checked for board usage in the above
	 * loop.  If those metadisks reside on the board, their ds_entry
	 * field (pointer to dr_io_t structure created for the metadisk)
	 * is non-zero.
	 *
	 * So, find the mirrored metadisks and see if the mirror components
	 * reside on the board.  If so, create a new dr_io_t structure for
	 * the mirrored metadisk and mark the ds_dev field of the component
	 * metadisk so it is noted as being part of the mirrored metadisk.
	 */
	next = head;
	while (next) {

		op = next;
		next = op->next;

		/*
		 * If this is not a mirror device, we've already matched
		 * it's components in the loop above.
		 */
		if (!op->is_mirror)
			continue;

		cp = op->component;
		while (cp) {
			/*
			 * look for the metadisk entry which is a
			 * component of the mirroed disk.
			 */
			meta = head;
			while (meta != NULL) {
				if (strcmp(cp->cname, meta->name) == 0)
					break;
				meta = meta->next;
			}

			if (meta == NULL) {
				dr_loginfo("%s: cannot find component match\n",
					    ONLINETAB);
				free_online_entries(head);
				return;
			}

			cp = cp->next;

			/*
			 * If the mirrored component is not on this
			 * board, then the mirror is not on this board.
			 */
			if (meta->ds_entry == NULL)
				continue;

			/*
			 * Create the dr_io_t structure entry for the
			 * mirrored device if it doesn't already exist.
			 * It gets linked to the end of the disk
			 * list in which the meta component is a part of.
			 */
			if (op->ds_entry == NULL) {
				if (build_disksuite_tree(op,
							    meta->ds_entry)) {
					free_online_entries(head);
					return;
				}
			}

			/*
			 * Now save a pointer to the mirrored pseudo disk
			 * for each metadisk component which is on the board.
			 * The ds_dev entry is part of the leaf or partition
			 * structure.
			 */
			meta->ds_entry->dv_leaf->ds_dev = op->ds_entry;

		}
	}

	/*
	 * Our last order of business is to expand the leaf_array to
	 * include the leaf entries we just created.
	 */
	if (save_num_leaves != num_leaves) {

		(*leaf_array) = realloc((*leaf_array),
				    sizeof (dr_leafp_t)*num_leaves);
		if ((*leaf_array) == NULL) {
			dr_logerr(DRV_FAIL, 0,
				    "realloc failed (leaf_array)");
			free_online_entries(head);
			return;
		}

		num_leaves = save_num_leaves;
		op = head;
		while (op) {
			if (op->ds_entry) {
				(*leaf_array)[num_leaves++] =
					op->ds_entry->dv_leaf;
				(*leaf_array)[num_leaves++] =
					op->ds_entry->dv_leaf->next;
			}
			op = op->next;
		}

		qsort((void *)(*leaf_array), num_leaves, sizeof (dr_leafp_t),
			    compare_devname);
	}

	free_online_entries(head);
}

/*
 * build_disksuite_tree
 *
 * Given the name of a pseudo disk, create a dr_io_t
 * tree structure consisting of the pseudo disk (eg dN) which has two
 * leaf devices chained to it (the /dev/md/dsk and /dev/md/rdsk entries).
 * This tree is linked to the end of the sibling list of the major
 * device (eg sdN) of the first pseudo disk component found to be
 * on the board.
 *
 * Input:
 *	op - online entry we create the pseudo disk entry for.
 *	dp - major device the pseudo disk gets linked into sibling
 *		chain.
 *
 * Function Return: 0 success, != error
 */
static int
build_disksuite_tree(onlinep_t op, dr_iop_t dp)
{
	dr_iop_t	major;
	dr_leafp_t	minor1, minor2;
	char		*name_major, *name1, *name2;
	int		size;

	/* Allocate the structures we need */
	major = dr_dev_malloc();
	minor1 = dr_leaf_malloc();
	minor2 = dr_leaf_malloc();

	size = strlen("/dev/md/rdsk/") +
		strlen(op->name) + 1;

	name_major = strdup("d");
	name1 = malloc(size);
	name2 = malloc(size);

	if (major == NULL || minor1 == NULL || minor2 == NULL ||
	    name_major == NULL || name1 == NULL || name2 == NULL) {

		free(major); free(minor1); free(minor2);
		free(name_major); free(name1); free(name2);
		return (1);
	}

	op->ds_entry = major;

	/*
	 * Build the pseudo disk tree
	 */
	minor2->major_dev = major;
	minor2->dev_name = name2;
	sprintf(name2, "/dev/md/dsk/%s", op->name);

	minor1->next = minor2;
	minor1->major_dev = major;
	minor1->dev_name = name1;
	sprintf(name1, "/dev/md/rdsk/%s", op->name);

	major->dv_parent = dp->dv_parent;
	major->dv_name = name_major;
	if (sscanf(op->name, "d%d", &major->dv_instance) != 1)
		dr_loginfo("build_disksuite_tree: malformed device name (%s)",
			    op->name);
	major->dv_addr = strdup(dp->dv_addr);
	major->dv_node_type = dp->dv_node_type;
	major->dv_leaf = minor1;

	/* Link onto the end of the sibling list */
	while (dp->dv_sibling != NULL)
		dp = dp->dv_sibling;
	dp->dv_sibling = major;

	return (0);
}

/*
 * find_online_db_entries
 *
 * Open the Sun online DiskSuite database configuration file
 * and determine if any of the database copies reside on this
 * board.
 *
 * The database configuration flie looks as follows:
 *
 *	#metadevice database location file do not hand edit
 *	#driver minor_t daddr_t checksum
 *	sd      247     16      -436
 *	sd      247     1050    -1470
 *
 * We're only interested in the first two columns which identify the
 * major/minor parition where the database copy resides.  If a database
 * copy if found on a partition, then mark it as NOTNET_OLDS_DB.
 *
 * When this routine is called, all leaf nodes are present and sorted by
 * /dev name in the global leaf array.
 *
 * getline() takes care of comment and continuation lines.
 */
static void
find_online_db_entries(dr_leafp_t *leaf_array)
{
	FILE		*fd;
	char		*buf;
	char		*p;
	int		prev_devt, db_devt, i;
	minor_t		minor;
	major_t		major;

	if ((fd = fopen(ONLINEDBTAB, "r")) == NULL) {
		/* No online database table so nothing to do */
		return;
	}

	/* used to optimize our searches */
	prev_devt = -1;

	/* Now parse each line entry */
	while (buf = getline(fd)) {

		p = gettoken(&buf);
		if (p == NULL) {
			/* blank line */
			continue;
		}

		/*
		 * This token is the disk type name (eg, sd, ssd).
		 * Translate it into a major number
		 */
		if (modctl(MODGETMAJBIND, p, strlen(p)+1, &major) == -1) {
			dr_loginfo("find_online_db_entries: Cannot find " \
				"major number for '%s'\n errno=%d", p, errno);
			(void) fclose(fd);
			return;
		}

		/* Now get the minor number and convert to decimal */
		p = gettoken(&buf);
		if (p == NULL) {
			dr_loginfo("find_online_db_entries: unexpected EOL\n");
			(void) fclose(fd);
			return;
		}

		minor = atoi(p);
		db_devt = (int)makedev(major, minor);

#ifdef NOTDEF
		/* DEBUG */
		dr_loginfo("Major %d Minor %d\n", major, minor);
#endif NOTDEF

		/*
		 * The database config file can have multiple entries, one
		 * for each database copy which resides on the partition.
		 * Only try to find the partition once.
		 */
		if (db_devt == prev_devt)
			continue;
		prev_devt = db_devt;

		/*
		 * Now search throught the leaf array looking for a hit
		 * on major/minor.
		 */
		for (i = 0; i < num_leaves; i++) {
			if (leaf_array[i]->device_id == db_devt) {
				leaf_array[i]->notnetflags |= NOTNET_OLDS_DB;
				break;
			}
		}
	}
	(void) fclose(fd);
}

/*
 * find_online_entries
 *
 * open the SUN online DiskSuite configuration file and
 * determine the current configuration.  Note that
 * this file is documentated to contain all current
 * disk configurations and is updated automatically
 * whenever this configuration changes (except for
 * hot spares). Contents of this file is documented
 * in md.cf(4).
 *
 * Lines in this file we're interested in look like
 *
 *	d0 2 1 /dev/dsk/c0t0d0s0 /dev/dsk/cntndnsn
 *
 * where d0 is the name of a pseudo disk, and the other
 * entries indicate the real disk partitions which make up the pseudo one.
 *
 * Some lines may have the format of:
 *
 *	d0 -m d1 d2
 *	d10 -t d11 d12
 *
 * where the first line defines d0 to be a mirrored metadevice and
 * the second line defines d10 to be a metatrans with d11 as the master
 * device and (optionally) d12 as the logging device.  Both mirrored
 * and trans metadisks have components which are meta-devices.
 *
 * Additional flag arguments which we ignore are:
 *	-h hotswap_num
 *	-i interlace_size
 *
 * Additional nuances of the file syntax such as continutation and comment
 * lines are taken care of by getline().  The parsing here is somewhat
 * crude, but does the job...  Since this file is machine generated,
 * syntax and other sorts of errors should not be present.
 *
 * Return value:  NULL is returned if no file exists or
 *	if there are syntax errors in the file.
 */
static onlinep_t
find_online_entries(void)
{
	FILE		*fd;
	char		*buf;
	char		*p;
	onlinep_t	head, tp;
	clistp_t	cp;

	if ((fd = fopen(ONLINETAB, "r")) == NULL) {
		/* No online table so nothing to do */
		return (NULL);
	}

	/* Now parse each line entry */
	head = NULL;
	while (buf = getline(fd)) {

		p = gettoken(&buf);
		if (p == NULL) {
			/* blank line */
			continue;
		}

		/*
		 * We're interested in lines defining a pseudo disk.
		 * These lines start with the pseudo disk name (dN).
		 */
		if (*p != 'd')
			continue;

		/*
		 * Allocate, link in, and start to fill in the online entry
		 */
		tp = calloc(1, sizeof (online_t));
		if (tp == NULL || (tp->name = strdup(p)) == NULL) {
			dr_logerr(DRV_FAIL, 0, "malloc failed (online)");
			free_online_entries(head);
			(void) fclose(fd);
			return (NULL);
		}
		if (head) {
			tp->next = head;
		}
		head = tp;

		p = gettoken(&buf);
		while (p != NULL) {

			/* Process arguments */
			if (*p == '-') {

				if (*(p+1) == 'm' || *(p+1) == 't')
					tp->is_mirror = 1;
				else if (*(p+1) == 'h' || *(p+1) == 'i') {
					/* ignore -h and -i options */
					p = gettoken(&buf);
				}

			/* only non-numeric entries are interesting */
			} else if (!isdigit(*p)) {

				cp = calloc(1, sizeof (clist_t));
				if (cp == NULL ||
				    (cp->cname = strdup(p)) == NULL) {
					dr_logerr(DRV_FAIL, 0,
						    "malloc failed (online)");
					free_online_entries(head);
					(void) fclose(fd);
					return (NULL);
				}

				if (tp->component) {
					cp->next = tp->component;
				}
				tp->component = cp;
			}
			p = gettoken(&buf);
		}

		if (tp->component == NULL) {
			dr_loginfo("%s: malformed file\n", ONLINETAB);
			free_online_entries(head);
			(void) fclose(fd);
			return (NULL);
		}
	}

	/* All done, return the online list */
	(void) fclose(fd);
	return (head);
}

/*
 * getline
 *
 * Return a pointer to a NULL terminated line.  Ignore
 * comment lines (# in col 1).  Also append all continuation lines
 * (`\` is the coninutation character) into a single line
 *
 *
 * Function return: NULL, no more lines, otherwise a pointer
 *	to the line buffer.
 */
static char *
getline(FILE *fd)
{
	static char	line[1024];
	char 		*cp;
	int		c;

doit_again:
	/* ignore comment lines */
	c = fgetc(fd);
	while (c == '#') {
		/* zip to the end of the line */
		while ((c = fgetc(fd)) != '\n' && (c != EOF))
		    /* empty */;

		/* first char on next line */
		c = fgetc(fd);
	}

	/* Now gather up the line */
	cp = line;
	while (c != EOF) {

		/* continuation line */
		if (c == '\\') {

			/* zip to the end of the line */
			while ((c = fgetc(fd)) != '\n' && (c != EOF))
				/* empty */;
			c = fgetc(fd);
			if (c == EOF)
				break;
		}

		/* end of line */
		if (c == '\n') {
			break;
		}

		/* add to the line */
		*cp++ = c;
		c = fgetc(fd);

		/*
		 * If we exceed the line length, just goto eol and
		 * return what we have.  Very unlikely this will
		 * even occur.  Leave room for the null string terminator.
		 */
		if (cp+1 == &line[sizeof (line)]) {
			while ((c = fgetc(fd)) != '\n' && (c != EOF))
				/* empty */;
			break;
		}
	}

	if (cp == line) {
		/*
		 * no chars on the line.  Either a blank
		 * line or we've hit EOF
		 */
		if (c != EOF)
			goto doit_again;
		else
			return (NULL);
	}

	*cp = 0;
	return (line);
}

/*
 * gettoken
 *
 * Return a token.  tokens are characters separated by white space
 * (blank, tab, end of string).
 *
 * Input: bufpp	- the address of a buffer pointer where the line
 *		to parse is located.  The buffer pointer is updated
 *		as a result of this operation.  buffer is null terminated
 *
 * Output: Function return value is a pointer to a null terminated
 *	token string or NULL if no token is available.
 */
static char *
gettoken(char **bufpp)
{
	char *p, *buf;

	buf = *bufpp;

	/* skip past leading white space */
	while (*buf == ' ' || *buf == '\t')
		buf++;

	/* now find the end of the token */
	p = buf;
	while (*buf != ' ' && *buf != '\t' && *buf != 0)
		buf++;

	/* null terminate string p and update bufp */
	if (*buf != 0) {
		*buf = 0;
		*bufpp = buf+1;
	} else
		*bufpp = buf;

	if (p == buf)
		/* no string */
		return (NULL);
	else {
		return (p);
	}
}

/*
 * free_online_entries
 *
 * free all allocated space associated with the online entries
 */
static void
free_online_entries(onlinep_t op)
{
	onlinep_t	top;
	clistp_t	cp, tcp;

	while (op != NULL) {

		cp = op->component;
		while (cp != NULL) {
			if (cp->cname) free(cp->cname);
			tcp = cp->next;
			free(cp);
			cp = tcp;
		}

		if (op->name) free(op->name);
		top = op->next;
		free(op);
		op = top;
	}
}
#endif	DR_OLDS
