/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * boards.c -- routines to build lists of devices for the menus to display
 */

#ident "@(#)boards.c   1.70   99/05/28 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boards.h"
#include "boot.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "menu.h"
#include "probe.h"
#include "prop.h"
#include "resmgmt.h"
#include "tty.h"

static char No_resources[] = "No resources";
static char XS_res[] =
	"Too many resources - use modify to inspect resources";

/*
 * Return descriptive text for the device at bp.
 */
char *
format_device_id(Board *bp, char *buf, int verbose)
{
	char *dp, *xp, *sp, *np;

	dp = GetDeviceName_devdb(bp, verbose);
	if (!dp) {
		dp = (char *)gettext("Unknown device");
	}
	if (strchr(dp, ':') != NULL) {
		xp = buf;
		for (sp = dp; *sp != ':'; sp++)
			*xp++ = *sp; /* copy BUSTYPE: */
		*xp++ = *sp++; /* copy colon */
		*xp++ = *sp++; /* copy space */
		if (!verbose && (np = strstr(sp, "class: ")) != NULL)
			sp = np + strlen("class: ");
		/*
		 * Mark disabled devices
		 */
		if (bp->flags & BRDF_DISAB) {
			*xp++ = '(';
			*xp++ = '!';
			*xp++ = ')';
			*xp++ = ' ';
		}
		strcpy(xp, sp);
	} else {
		strcpy(buf, dp);
	}
	return (buf);
}

/*
 * Qsort compare function for menu list sorting
 */
int
menu_comp(const void *p1, const void *p2)
{
	return (strcmp(((struct menu_list *)p1)->string,
		((struct menu_list *)p2)->string));
}

/*
 * Builds a menu-list of devices.
 */
void
menu_list_boards(struct menu_list **listp, int *nlistp, int verbose)
{
	char *cp, *cp2;
	Board *bp;
	int j;
	struct menu_list *mlp;

	for (j = 0, bp = Head_board; bp; bp = bp->link) {
		j++;
	}

	ASSERT(j > 0); /* Because there has to be a motherboard record! */
	*listp = (struct menu_list *)calloc(j, sizeof (struct menu_list));
	if (*listp == 0) {
		MemFailure();
	}
	*nlistp = 0;

	for (mlp = *listp, bp = Head_board; bp; bp = bp->link) {
		mlp->datum = (void *) bp;
		if ((cp = (char *)malloc(200)) == NULL) {
			MemFailure();
		}
		cp = format_device_id(bp, cp, verbose);
		if (verbose) {
			cp2 = list_resources_boards(bp);
			cp = realloc(cp, strlen(cp) + strlen(cp2) + 2);
			strcat(cp, "\n");
			strcat(cp, cp2);
			free(cp2);
		} else {
			cp = realloc(cp, strlen(cp) + 1);
		}
		if (!verbose) {
			/*
			 * Prune out Bridge devices, used BIOS memory,
			 * bus configuration ports, etc.
			 */
			if (strstr(cp, gettext(" bridge")) != NULL ||
			    strstr(cp, gettext("Bridge")) != NULL ||
			    strstr(cp, gettext("Unknown")) != NULL ||
			    strstr(cp, gettext("BIOS memory")) != NULL ||
			    strstr(cp, gettext("bus configuration")) != NULL ||
			    strstr(cp, gettext("extra resources")) != NULL ||
			    strstr(cp, gettext("System peripheral")) != NULL ||
			    strstr(cp, gettext("configuration port")) != NULL ||
			    strstr(cp, gettext("Built In Features")) != NULL ||
			    strstr(cp, gettext("Unspecified")) != NULL) {
				free(cp);
				continue;
			}
			/*
			 * Add keyboard layout to keyboard description
			 */
			if (strstr(cp, "System keyboard") != NULL) {
				char *lp;

				lp = read_prop("kbd-type", "options");
				if (lp != NULL) {
					cp = realloc(cp, strlen(cp) +
						strlen(lp) + 4);
					strcat(cp, " (");
					strcat(cp, lp);
					strcat(cp, ")");
				}
			}
		}
		mlp->string = cp;
		mlp++;
		*nlistp += 1;
	}
	qsort(*listp, *nlistp, sizeof (struct menu_list), menu_comp);
}

/*
 * Frees a menu-list of devices.
 */
void
free_boards_list(struct menu_list *listp, int nlistp)
{
	int i;

	for (i = 0; i < nlistp; i++)
		if (listp && listp[i].string)
			free(listp[i].string);
	if (listp)
		free(listp);
}

/*
 * Free a board record
 * frees any malloc`d data hanging from the board structure, then frees
 * the board itself.
 */
void
free_board(Board *bp)
{
	devprop *pp, *npp;

	for (pp = bp->prop; pp; pp = npp) {
		npp = pp->next;
		free(pp);
	}
	free(bp);
}

/*
 * Makes a copy of a board record:
 *
 * Allocates a buffer to hold a copy of the "src" board record,
 * copies the contents of said record into the new buffer, and
 * returns the buffer address.
 */
Board *
copy_boards(Board *src)
{
	devprop *pp, *npp;
	Board *bp;
	int len;

	bp = (Board *)malloc(src->buflen);
	if (bp == (Board *)0)
		MemFailure();
	memcpy(bp, src, src->buflen);
	bp->prop = (devprop *)0;
	/*
	 * Copy property list, note: this results in a reversed list
	 * on the new node, but this is not considered a problem.
	 */
	for (pp = src->prop; pp; pp = pp->next) {
		len = pp->len + strlen(pp->name) + 1 + sizeof (devprop);
		npp = (devprop *)malloc(len);
		if (npp == (devprop *)0)
			MemFailure();
		memcpy(npp, pp, len);
		npp->next = bp->prop;
		bp->prop = npp;
	}
	return (bp);
}

/*
 * Extract active resources:
 *
 * This routine extracts the active resources of the indicated
 * function, copying them into a separate "Resource" array whose
 * address is stored at "rpp".  Returns the number of entries
 * extracted.
 */
static int
extract_resources(Board *bp, Resource **rpp)
{
	int n = 0;
	int fc = resource_count(bp);
	Resource *xp, *rp = resource_list(bp);

	while (fc--) {
		n += !((rp++)->flags & (RESF_ALT+RESF_DISABL));
	}
	xp = (Resource *)malloc(n * sizeof (Resource));
	if ((*rpp = xp) == 0) {
		MemFailure();
	}
	rp = resource_list(bp);

	for (fc = resource_count(bp); fc--; rp++) {
		/*
		 *  Make a second pass over the resource list, copying each
		 *  active record into the work buffer we just bought.
		 */

		if (!(rp->flags & (RESF_ALT+RESF_DISABL))) {
			/*
			 *  Here's another active resource.  Copy it into
			 *  the resource buffer!
			 */

			memcpy(xp++, rp, sizeof (Resource));
		}
	}

	return (n);
}

/*
 * Sort comparison for board equality test:
 *
 * This routine compares the resource record at "p" with the one
 * at "q" to determine which sorts first.  The sort order isn't
 * really important, as long as it's consistant!
 */
static int
ResCmp(const void *p, const void *q)
{
	Resource *rp = (Resource *)p;
	Resource *rq = (Resource *)q;
	unsigned long xp = (rp->flags & RESF_TYPE);
	unsigned long xq = (rq->flags & RESF_TYPE);

	if (xp == xq) {
		/*
		 *  If resource types are equal, try looking at the base
		 *  addresses.
		 */
		xp = rp->base;
		xq = rq->base;
		if (xp == xq) {
			/*
			 *  If base addresses are equal, use resource length to
			 *  break the tie.
			 */
			xp = rp->length;
			xq = rq->length;
		}
	}
	return ((xp < xq) ? -1 : (xp > xq));
}

/*
 * Determine if two board records are identical:
 *
 * This routine may be used to determine whether or not the board
 * records at "bp" and "bq" describe the same device.  It returns
 * non-zero if they do; zero if they don't.
 *
 * By "identical", we mean that both devices have the same device
 * ID, the same bus type, and have the same resources allocated to
 * identical functions.
 */
int
equal_boards(Board *bp, Board *bq)
{
	int equal = 0;

	/*
	 * Boards aren't identical unless they have the same device ID,
	 * bus type, and resource counts.  If so, start comparing resources
	 */
	if ((bp->devid == bq->devid) && (bp->bustype == bq->bustype)) {
		/*
		 *  Extract the resources associated with the next
		 *  function of each board, then check to see if we
		 *  got an equal number of resource records in each
		 *  case.
		 */
		Resource *rp, *rq;
		int k = extract_resources(bp, &rp);

		if (k == extract_resources(bq, &rq)) {
			/*
			 *  If resource counts are equal, sort both
			 *  resource lists, then compare the indivi-
			 *  dual records one-by-one.
			 */

			Resource *xp, *xq;
			qsort(rp, k, sizeof (Resource), ResCmp);
			qsort(rq, k, sizeof (Resource), ResCmp);

			for (equal = 1, xp = rp, xq = rq; k--; xp++, xq++) {
				/*
				 *  Lists are sorted, so any mismatch
				 *  indicates non-indentical board
				 *  records.
				 */

				if (((xp->flags & RESF_TYPE) !=
				    (xq->flags & RESF_TYPE)) ||
				    (xp->base != xq->base) ||
				    (xp->length != xq->length)) {
					/*
					 *  No match!  Bail out of
					 *  both inner and outer
					 *  loops.
					 */
					equal = 0;
					break;
				}
			}
		}
		free(rp);
		free(rq);
	}
	return (equal);
}

char *
addtobuf(char *buf, int *buflen, char *str)
{
	if (strlen(buf) + strlen(str) + 1 > *buflen) {
		if ((buf = (char *)realloc(buf, *buflen + 160)) == NULL)
			MemFailure();
		*buflen += 160;
	}
	strcat(buf, str);
	return (buf);
}

/*
 *  Display resources associated with the board
 */
char *
list_resources_boards(Board *bp)
{
	int mp;
	unsigned char nlines = 1;
	long len;
	char *buf, *lp, *rbp;
	char linebuf[80];
	char resbuf[32];
	int rc, restype, print_rname;
	Resource *rp;
	char *fmt;
	int resource_cnt = 0;
	int neednl, blen;

	blen = 160;
	if ((buf = malloc(blen)) == NULL)
		MemFailure();
	*buf = '\0';
	linebuf[0] = '\0';
	neednl = 0;
	/*
	 * for PCI devices emit a line with bus/device/function info
	 */
	if (bp->bustype == RES_BUS_PCI && bp->devid != 0) {
		(void) sprintf(linebuf,
			"     bus: %d, device: %d, function: %d",
			bp->pci_busno, bp->pci_devfunc >> FUNF_DEVSHFT,
			bp->pci_devfunc & FUNF_FCTNNUM);
		buf = addtobuf(buf, &blen, linebuf);
		linebuf[0] = '\0';
		neednl = 1;
	}
	lp = linebuf;
	for (restype = 1; restype < RESF_Max; restype++) {
		rp = resource_list(bp);
		print_rname = 1;

		if (restype == RESF_Irq || restype == RESF_Dma) {
			fmt = "%ld";
		} else {
			fmt = "%lX";
		}
		for (rc = resource_count(bp); rc--; rp++) {
			if (RTYPE(rp) == restype) {
				rbp = resbuf;
				/*
				 * If we have already put out resources
				 * put out the proper seperator, ',' if
				 * more resources of this type, or ';' if
				 * we are starting a new resource type
				 */
				if (resource_cnt) {
					lp += sprintf(lp, "%c ",
					    print_rname ? ';' : ',');
				} else {
					if (neednl) {
						buf = addtobuf(buf, &blen,
							"\n");
						neednl = 0;
					}
					lp += sprintf(lp, "     ");
				}
				/*
				 * If we are starting a new resource type,
				 * put out the name of the resource
				 */
				if (print_rname) {
					rbp += sprintf(resbuf, "%s: ",
					    ResTypes[restype]);
					print_rname = 0;
				}

				/*
				 * Generate the info for this resource
				 */
				rbp += sprintf(rbp, fmt, rp->base);
				len = rp->base + rp->length - 1;

				if (rp->length > 1) {
					rbp += sprintf(rbp, "-%lX", len);
				}
				resource_cnt++;

				mp = strlen(linebuf);
				/*
				 * Check for line too long
				 */
				if (mp + strlen(resbuf) >= 67) {
					/*
					 * The Unisys Eisa video
					 * (ATI4402) has ~100 ports
					 * (slot aliases) which won't
					 * fit on 1 screen!
					 */
					if (++nlines > 12) {
						free(buf);
						buf = strdup(gettext(XS_res));
						return (buf);
					}
					/*
					 * Emit the line buffer and
					 * start a new line
					 */
					buf = addtobuf(buf, &blen, linebuf);
					buf = addtobuf(buf, &blen, "\n");
					lp = linebuf;
					lp += sprintf(linebuf, "     ");
				}
				/*
				 * Put the new resource into the line buffer
				 */
				lp += sprintf(lp, "%s", resbuf);
			}
		}
	}
	if (resource_cnt == 0) {
		if (neednl)
			buf = addtobuf(buf, &blen, "\n     ");
		else
			buf = addtobuf(buf, &blen, "     ");
		buf = addtobuf(buf, &blen, (char *)gettext(No_resources));
	} else
		buf = addtobuf(buf, &blen, linebuf);
	return (buf);
}

/*
 * Add board to global boards chain (at the end).
 */
void
add_board(Board *bp)
{
	Board *tbp;

	/*
	 * Shrink board to actual used size
	 */
	bp = (Board *)realloc(bp, bp->reclen);
	bp->buflen = bp->reclen;

	bp->link = 0;
	if (Head_board == NULL) {
		Head_board = bp;
	} else {
		/*
		 * Could maintain a tail pointer but the list is short
		 * and following the links should be quick.
		 */
		for (tbp = Head_board; tbp->link; tbp = tbp->link) {
			;
		}
		tbp->link = bp;
	}

	/*
	 * Save in each Board a ptr to the device db entry (devtrans)
	 * to save having to call TranslateDevice_devdb numerous times.
	 */
	if ((bp->flags & BRDF_NOTREE) == 0 || (bp->flags & BRDF_DISAB)) {
		bp->dbentryp = TranslateDevice_devdb(bp->devid, bp->bustype);
	}
	/*
	 * Bug fix 4042854 - If the pci function which has a sub vendor
	 * id isn't mapped in the database, see if the vendor/device is...
	 */
	if ((bp->bustype == RES_BUS_PCI) && (bp->dbentryp == NULL)) {
		if (bp->pci_subvenid) {
			bp->dbentryp = TranslateDevice_devdb(
			    (((u_long) bp->pci_venid) << 16) | bp->pci_devid,
			    RES_BUS_PCI);
		}
		/*
		 * If there still isn't a database match try looking for a
		 * pciclass entry.
		 */
		if (bp->dbentryp == NULL) {
			bp->dbentryp = TranslateDevice_devdb(bp->pci_class,
				RES_BUS_PCICLASS);
		}
	}
}

void
del_board(Board *bp)
{
	Board *tbp;

	if (Head_board == bp) {
		Head_board = Head_board->link;
	} else {
		for (tbp = Head_board; tbp && (tbp->link != bp);
		    tbp = tbp->link) {
			;
		}
		ASSERT(tbp);
		tbp->link = bp->link;
	}
	free_board(bp);
	Update_escd = 1;
	check_weak();
}

#define	INITIAL_BOARD_SIZE 256

struct board *
new_board()
{
	Board *bp;

	if (!(bp = (Board *)calloc(1, INITIAL_BOARD_SIZE))) {
		MemFailure();
	}
	bp->reclen = ((int)&((Board *)0)->var);
	bp->buflen = INITIAL_BOARD_SIZE;

	return (bp);
}

/*
 * Frees up every board in the chain of boards
 * and sets the head pointer to NULL.
 */
void
free_chain_boards(Board **head)
{
	Board *bp;

	while ((bp = *head) != 0) {
		*head = bp->link;
		free_board(bp);
	}
}
