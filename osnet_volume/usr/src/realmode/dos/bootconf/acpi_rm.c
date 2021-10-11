/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * acpi_rm.c - acpi enumerator
 */

#ident	"@(#)acpi_rm.c	1.2	99/11/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <names.h>
#include "types.h"
#include "boards.h"
#include "debug.h"
#include "devdb.h"
#include "err.h"
#include "enum.h"
#include "resmgmt.h"
#include "tty.h"
#include "acpi_rm.h"

Board *Head_acpi = NULL;
struct acpi_bc bc;

static void acpi_add_board(Board *bp);
static void acpi_del_board(Board **head, Board *bp);
static void acpi_replace_board(Board **head, Board *old_bp, Board *new_bp);
static Board *acpi_find_board(Board *start, unsigned long devid);
static void acpi_append_prop(Board *bp, Board *target_bp);
static int acpi_cmp_boards(Board *bp1, Board *bp2);
static Board *acpi_remove_conflicts(Board *bp1, Board *bp2);
static void acpi_translate_devdb(Board *bp);
static void acpi_display_conflict(Board *, Board *);
static void acpi_debug_conflict(Board *non_acpi, Board *acpi);
static void acpi_display_board(int dflag, Board *bp);
static void acpi_pause(void);
static void acpi_add_rest(void);
static unsigned long linear_addr(char *);

/*
 * acpi_cmp_boards return value definitions
 */
#define	ACPI_RES_MATCH		0	/* all resources match */
#define	ACPI_RES_NOTMATCH	1	/* none matches	*/
#define	ACPI_RES_B1_CONTAIN_B2	2	/* B1 has all B2's resources and more */
#define	ACPI_RES_B2_CONTAIN_B1	3	/* B2 has all B1's resources and more */
#define	ACPI_RES_CONFLICT	4	/* conflicts */

/*
 * To get all the ACPI device boards thru acpi_copy from boot.bin.
 * In order to save the scarce realmode memory, it will use acpi_copy to
 * copy the boards from boot.bin every time instead of maintaining an ACPI
 * list here and duplicates the boards when they are added to the main list.
 */
void
enumerator_acpi(int phase)
{
	char *bufp;
	Board *bp, *mbp;
	int status;

	if (phase == ACPI_COMPLETE) {
		if (Head_acpi != NULL)
			acpi_add_rest();
		return;
	}
	/* phase == ACPI_INIT */
	if (Head_acpi)
		free_chain_boards(&Head_acpi);
	bc.bc_this = (unsigned long) 0;
	if (acpi_copy(&bc) != 0) {
		debug(D_ACPI_VERBOSE, "first acpi_copy failed\n");
		return;
	}
	/* find the special motherboard device - it is always there */
	mbp = acpi_find_board(Head_board, CompressName("SUNFFE2"));
	if (mbp == NULL) {
		(void) iprintf_tty("enumerator_acpi: Motherboard device not "
			"found\n");
		return;
	}
	while (bc.bc_next) {
		if ((bufp = malloc(bc.bc_nextlen)) == NULL) {
			(void) iprintf_tty("enumerator_acpi: malloc failed\n");
			return;
		}
		bc.bc_buf = linear_addr(bufp);
		bc.bc_buflen = bc.bc_nextlen;
		bc.bc_this = bc.bc_next;
		if (acpi_copy(&bc) != 0) {
			free(bufp);
			debug(D_ACPI_VERBOSE, "acpi_copy failed\n");
			return;
		}
		bp = (Board *) bufp;
		bp->buflen = bp->reclen;
		/*
		 * For future enhancement, motherboard record should probably
		 * be replaced by the ACPI records.  ACPI breaks down the
		 * motherboard resources into many small devices like dma,
		 * pic, fpu, rtc... which is a good idea to show all these
		 * devices on the device tree.  However, each of these devices
		 * will require a board structure and that may not be memory
		 * friendly right now.
		 */
		status = acpi_cmp_boards(bp, mbp);
		if ((status == ACPI_RES_B2_CONTAIN_B1) ||
			(status == ACPI_RES_MATCH)) {
			/*
			 * motherboard has exactly the same or more resources,
			 * discard the acpi board
			 */
			mbp->flags |= BRDF_ACPI;
			free(bp);
		} else if (status == ACPI_RES_B1_CONTAIN_B2) {
			/*
			 * This shouldn't happen, but just in case
			 * replace the motherboard device with the
			 * acpi since it has more resources.
			 * Also, keep the special devid to make it
			 * consistant.
			 */
			bp->devid = mbp->devid;
			bp->flags |= BRDF_ACPI;
			bp->acpi_status |= ACPI_CONFLICT;
			del_board(mbp);
			add_board(bp);
			mbp = bp;
		} else {
			if (status == ACPI_RES_CONFLICT) {
				debug(D_ACPI_CONFLICT, "motherboard "
					"CONFLICT\n");
				acpi_debug_conflict(mbp, bp);
				mbp->flags |= BRDF_ACPI;
				bp = acpi_remove_conflicts(mbp, bp);
			}
			/*
			 * either ACPI_RES_CONFLICT or ACPI_RES_NOTMATCH
			 * will get here
			 */
			acpi_translate_devdb(bp);
			acpi_add_board(bp);
		}
	}
}

/*
 * This is called almost at the end of run_enum().
 * The default policy is to add the rest of the boards in the Head_acpi list
 * to the Head_board list.
 */
void
acpi_add_rest(void)
{
	Board *acpi_bp, *cbp, *next_bp, *new_bp;
	Resource *crp;
	int status;

	acpi_bp = Head_acpi;
	while (acpi_bp) {
		crp = board_conflict_resmgmt(acpi_bp, 0, &cbp);
		if (crp == NULL) {	/* no conflict */
			next_bp = acpi_bp->link;
			add_board(acpi_bp);
			acpi_bp = next_bp;
		} else if (cbp->bustype & (RES_BUS_ISA | RES_BUS_PNPISA |
			RES_BUS_I8042)) {
			/*
			 * try to resolve the conflicts only for those
			 * non self-enumerating buses devices
			 */
			acpi_bp->acpi_status |= ACPI_CONFLICT;
			status = acpi_cmp_boards(cbp, acpi_bp);
			debug(D_ACPI_CONFLICT, "acpi_add_rest:"
				" devid=%lx cmp status=%x\n", acpi_bp->devid,
				status);
			acpi_debug_conflict(cbp, acpi_bp);
			if (status == ACPI_RES_MATCH || status ==
				ACPI_RES_B1_CONTAIN_B2) {
				/*
				 * either the resources match or legacy has
				 * more
				 */
				cbp->flags |= BRDF_ACPI;
				next_bp = acpi_bp->link;
				free_board(acpi_bp);
				acpi_bp = next_bp;
			} else if (status == ACPI_RES_B2_CONTAIN_B1) {
				/* acpi has more resources than legacy */
				acpi_append_prop(cbp, acpi_bp);
				acpi_del_board(&Head_board, cbp);
			} else if (status == ACPI_RES_CONFLICT) {
				/*
				 * special handling for PNP0C02 - system
				 * peripheral resources
				 * It may be because the ACPI BIOSes are
				 * still in the early stage or the PNPBIOSes
				 * are too old, there are too many conflicts
				 * between the ACPIs and the PNPBIOSes on
				 * these information.  Therefore, flagging
				 * the conflicts on this type is not a
				 * good idea.  In the future, when ACPI is
				 * more reliable, PNPBIOS information should
				 * be discarded and just use the ACPI
				 * information.
				 */
				if (cbp->devid == CompressName("PNP0C02")) {
					/* remove conflicts from the legacy */
					new_bp = acpi_remove_conflicts(acpi_bp,
							cbp);
					if (new_bp != cbp) {
						/* cbp can be changed */
						acpi_replace_board(&Head_board,
							cbp, new_bp);
					} else if (resource_count(cbp) == 0) {
						/*
						 * no more resources on the
						 * legacy, so remove it
						 */
						acpi_append_prop(cbp, acpi_bp);
						acpi_del_board(&Head_board,
									cbp);
					}
					/*
					 * continue to check for more conflicts
					 */
				} else {
					/*
					 * Go check whether the acpi device is
					 * PNP0C02. If it is, then remove the
					 * conflicted resources from the acpi.
					 * Otherwise, display the conflicts to
					 * flag the user and add the board.
					 */
					goto chk_acpi_pnp0c02;
				}
			} else {
				/* shouldn't happen but just in case */
				debug(D_ACPI_CONFLICT, "acpi_add_rest: "
					"can't find the conflicts\n");
				next_bp = acpi_bp->link;
				add_board(acpi_bp);
				acpi_bp = next_bp;
			}
		} else {
			/*
			 * shouldn't conflict with self-enumerating
			 * bus device, but if the acpi device is PNP0C02
			 * then remove the conflicted resources from the
			 * acpi.  Otherwise, display the conflicts to flag
			 * the user but add the board anyway
			 */
			debug(D_ACPI_CONFLICT, "acpi_add_rest: conflict "
				"with self-enumerating bus device\n");
			acpi_debug_conflict(cbp, acpi_bp);
chk_acpi_pnp0c02:
			if (acpi_bp->devid == CompressName("PNP0C02")) {
				/* remove conflicts from the acpi */
				acpi_bp = acpi_remove_conflicts(cbp, acpi_bp);
				if (resource_count(acpi_bp) == 0) {
					/* goto the next one */
					next_bp = acpi_bp->link;
					free_board(acpi_bp);
					acpi_bp = next_bp;
				}
				/*
				 * else
				 * continue to check for more conflicts
				 */
			} else {
				acpi_display_conflict(cbp, acpi_bp);
				next_bp = acpi_bp->link;
				add_board(acpi_bp);
				acpi_bp = next_bp;
			}
		}
	}
	Head_acpi = NULL;
}

/* To add bp to the Head_acpi list */
static void
acpi_add_board(Board *bp)
{
	Board *tbp;

	bp->link = NULL;
	bp->flags |= BRDF_ACPI;
	bp->acpi_status = ACPI_CLEAR;
	if (! bp->bustype)
		bp->bustype = RES_BUS_ISA;

	if (Head_acpi == NULL)
		Head_acpi = bp;
	else {
		/*
		 * Could maintain a tail pointer but the list is short
		 * and following the links should be quick.
		 */
		for (tbp = Head_acpi; tbp->link; tbp = tbp->link) {
			;
		}
		tbp->link = bp;
	}
}

/* To delete bp from the head and free bp but not its prop list */
static void
acpi_del_board(Board **head, Board *bp)
{
	Board *tbp;

	if (*head == bp) {
		*head = bp->link;
	} else {
		for (tbp = *head; tbp && (tbp->link != bp);
		    tbp = tbp->link) {
			;
		}
		tbp->link = bp->link;
	}
	/*
	 * use free instead of free_board because free_board will free
	 * prop list also
	 */
	free(bp);
}

/* To replace old_bp on the Head_acpi list with new_bp */
static void
acpi_replace_board(Board **head, Board *old_bp, Board *new_bp)
{
	Board *tbp;

	if (*head == old_bp) {
		*head = new_bp;
	} else {
		for (tbp = *head; tbp && (tbp->link != old_bp);
		    tbp = tbp->link) {
			;
		}
		tbp->link = new_bp;
	}
}

/* To find and setup the devdb entry in bp */
static void
acpi_translate_devdb(Board *bp)
{
	devtrans *dtp, *hdtp;

	dtp = TranslateDevice_devdb(bp->devid, bp->bustype);

	if (dtp == NULL) {
		debug(D_ACPI_VERBOSE, "acpi_translate_devdb: devid=%lx not "
			"found\n", bp->devid);
		return;
	}
	if (*dtp->real_driver != 0) {	/* not "none" */
		hdtp = TranslateDriver_devdb(dtp->real_driver);
		if (hdtp != NULL && hdtp != dtp) {
			bp->devid = hdtp->devid;
			dtp = hdtp;
		}
	}
	bp->dbentryp = dtp;
}

/* To search for a board that matches the devid from start */
static Board *
acpi_find_board(Board *start, unsigned long devid)
{
	Board *bp;

	for (bp = start; bp; bp = bp->link)
		if (bp->devid == devid || bp->acpi_hid == devid)
			return (bp);
	return (NULL);
}

/*
 * to append the prop from bp to target_bp, also copy pnpbios info if available
 */
static void
acpi_append_prop(Board *bp, Board *target_bp)
{
	devprop *pp;

	if (bp->flags & BRDF_PNPBIOS) {
		/* copy pnpbios type info if available */
		target_bp->flags |= BRDF_PNPBIOS;
		target_bp->bus_u.pnpb_acpi.pnpbios =
				bp->bus_u.pnpb_acpi.pnpbios;
	}
	if (bp->prop != NULL) {
		if (target_bp->prop == NULL)
			target_bp->prop = bp->prop;
		else {
			for (pp = target_bp->prop; pp->next; pp = pp->next) {
				;
			}
			pp->next = bp->prop;
		}
	}
}


/*
 * To check whether the device exists in the Head_acpi list.
 * If it exists, then check whether the resources are matched.
 * By the time when ACPI is well established, acpi_check() can be removed
 * because we should use the ACPI information over the PNPBIOS.
 * Also, the legacy befs should call bootconf and get whatever resource
 * information provided by the ACPI instead.
 */
Board *
acpi_check(Board *bp)
{
	Board *acpi_bp, *start_bp;
	int status;

	start_bp = Head_acpi;
	while (start_bp != NULL) {
		acpi_bp = acpi_find_board(start_bp, bp->devid);
		if (acpi_bp == NULL) {
			debug(D_ACPI_VERBOSE, "acpi_check: devid=%lx "
				"not found\n", bp->devid);
			break;
		} else if (acpi_bp->bustype != bp->bustype) {
			debug(D_ACPI_VERBOSE, "acpi_check: devid=%lx "
				"found but different bustype - legacy=%x "
				"acpi=%x\n", bp->devid, bp->bustype,
				acpi_bp->bustype);
			start_bp = acpi_bp->link;
			continue;
		}
		status = acpi_cmp_boards(bp, acpi_bp);
		if (status != ACPI_RES_MATCH) {
			debug(D_ACPI_CONFLICT, "acpi_check: devid=%lx "
				"cmp status=%x\n", bp->devid, status);
			acpi_debug_conflict(bp, acpi_bp);
		}
		if ((status == ACPI_RES_MATCH) ||
			(status == ACPI_RES_B1_CONTAIN_B2)) {
			/*
			 * if they are matched or the legacy has more resources,
			 * free the acpi and use the legacy
			 */
			bp->flags |= BRDF_ACPI;
			acpi_del_board(&Head_acpi, acpi_bp);
			return (bp);
		} else if (status == ACPI_RES_B2_CONTAIN_B1) {
			/* acpi has more resources than legacy */
			acpi_append_prop(bp, acpi_bp);
			free(bp);
			return (NULL);
		} else if (status == ACPI_RES_CONFLICT) {
			/* there are conflicts between legacy and acpi */
			acpi_bp->acpi_status |= ACPI_CONFLICT;
			/*
			 * special handling for PNP0C02 - system peripheral
			 * resources
			 * refer comments in acpi_add_rest()
			 */
			if (bp->devid == CompressName("PNP0C02")) {
				bp = acpi_remove_conflicts(acpi_bp, bp);
				if (resource_count(bp) == 0) {
					acpi_append_prop(bp, acpi_bp);
					free(bp);
					return (NULL);
				}
			}
			/*
			 * don't do anything right now
			 * handle the conflicts at acpi_add_rest()
			 */
			if (bp->flags & BRDF_PNPBIOS) {
				/* copy pnpbios type info if available */
				acpi_bp->flags |= BRDF_PNPBIOS;
				acpi_bp->bus_u.pnpb_acpi.pnpbios =
						bp->bus_u.pnpb_acpi.pnpbios;
			}
			return (bp);
		}
		/* else ACPI_RES_NOTMATCH */
		start_bp = acpi_bp->link;
	}
	return (bp);
}

/*
 * dflag's definition for acpi_display_board
 */
#define	ACPI_DEBUG	1
#define	ACPI_DISPLAY	2

/*
 * To display the resources information of the two conflicted boards
 */
static void
acpi_display_conflict(Board *non_acpi, Board *acpi)
{
	char name[8];

	(void) iprintf_tty("Warning: Resource Conflict - both devices are "
		"added\n");
	DecompressName(non_acpi->devid, name);
	(void) iprintf_tty("NON-ACPI device: %s\n", name);
	acpi_display_board(ACPI_DISPLAY, non_acpi);
	DecompressName(acpi->devid, name);
	(void) iprintf_tty("ACPI device: %s\n", name);
	acpi_display_board(ACPI_DISPLAY, acpi);
	acpi_pause();
}

/*
 * To display the resources information of the two conflicted boards
 * only if D_ACPI_CONFLICT is turned on
 */
static void
acpi_debug_conflict(Board *non_acpi, Board *acpi)
{
	char name[8];

	if (Debug & D_ACPI_CONFLICT) {
		DecompressName(non_acpi->devid, name);
		debug(D_ACPI_CONFLICT, "NON-ACPI board: %s\n", name);
		acpi_display_board(ACPI_DEBUG, non_acpi);
		DecompressName(acpi->devid, name);
		debug(D_ACPI_CONFLICT, "ACPI board: %s\n", name);
		acpi_display_board(ACPI_DEBUG, acpi);
	}
}

/* To display the resources information of a board */
static void
acpi_display_board(int dflag, Board *bp)
{
	char *buf;

	buf = list_resources_boards(bp);
	if (buf) {
		if (dflag == ACPI_DEBUG)
			debug(D_ACPI_CONFLICT, "%s\n", buf);
		else
			(void) iprintf_tty("%s\n", buf);
		free(buf);
	}
}

/* to force a pause so user can know what going on */
static void
acpi_pause(void)
{
	int i, max;

	/* scroll to the end of screen to force a prompt */
	max = maxli_tty();
	for (i = curli_tty(); i <= max; i++)
		(void) iprintf_tty(" \n");
}

/*
 * To compare the resources between bp1 and bp2 and return the result.
 * return value can be one of the followings:-
 * ACPI_RES_MATCH - resources are exactly matched
 * ACPI_RES_NOTMATCH - none of the resources matched
 * ACPI_RES_B1_CONTAIN_B2 - bp1 has all the bp2's resources and more
 * ACPI_RES_B2_CONTAIN_B1 - bp2 has all the bp1's resources and more
 * ACPI_RES_CONFLICT - resource conflicts between bp1 and bp2
 */
static int
acpi_cmp_boards(Board *bp1, Board *bp2)
{

	long rc1 = resource_count(bp1);
	long rc2 = resource_count(bp2);
	Resource *rp1 = resource_list(bp1);
	Resource *rp2;
	u_int type1;
	u_long base1, len1, end1, end2;
	short i, j, match_cnt, b1_inc_b2_cnt, b2_inc_b1_cnt, conflict;

	match_cnt = b1_inc_b2_cnt = b2_inc_b1_cnt = conflict = 0;
	for (i = 0; i < rc1; rp1++, i++) {
		type1 = rp1->flags & (RESF_TYPE | RESF_ALT);
		base1 = rp1->base;
		len1 = rp1->length;
		end1 = base1 + (len1 - 1);

		for (j = 0, rp2 = resource_list(bp2); j < rc2; rp2++, j++) {
			/*
			 * Scan thru the resource list looking for
			 * records of the current type that match
			 * the target range.
			 */
			if (type1 == (rp2->flags & (RESF_TYPE | RESF_ALT))) {
				end2 = rp2->base + (rp2->length - 1);
				if ((base1 == rp2->base) &&
					(len1 == rp2->length)) {
					/* exact match */
					match_cnt++;
					break;
				} else if ((base1 >= rp2->base) &&
					(end1 <= end2)) {
					/*
					 * b1's resources are all matched,
					 * no need to continue
					 */
					b2_inc_b1_cnt++;
					break;
				} else if ((base1 <= rp2->base) &&
					(end1 >= end2)) {
					/* b1 has more resources so continue */
					b1_inc_b2_cnt++;
				} else if (((base1 < rp2->base) &&
					(end1 >= rp2->base) &&
					(end1 <= end2)) || ((end1 > end2) &&
					(base1 >= rp2->base) &&
					(base1 <= end2))) {
					conflict++;
					break;
				}
			}
		}
	}
	if ((rc1 == rc2) && (match_cnt == rc1))
		return (ACPI_RES_MATCH);
	else if ((match_cnt + b1_inc_b2_cnt + b2_inc_b1_cnt + conflict) == 0)
		return (ACPI_RES_NOTMATCH);
	else if ((rc1 >= rc2) && ((match_cnt + b1_inc_b2_cnt) == rc2))
		return (ACPI_RES_B1_CONTAIN_B2);
	else if ((rc2 >= rc1) && ((match_cnt + b2_inc_b1_cnt) == rc1))
		return (ACPI_RES_B2_CONTAIN_B1);
	else
		return (ACPI_RES_CONFLICT);
}

/*
 * To resolve resource conflicts between boards cbp and target_bp.
 * It can have several ways to resolve the conflicts but the only way
 * implemented right now is to remove all the conflicts from the target_bp.
 * target_bp may or may not be changed, but it will be returned on either
 * cases.
 */
static Board*
acpi_remove_conflicts(Board *cbp, Board *target_bp)
{
	long t_rc, c_rc;
	Resource *t_rp, *c_rp;
	u_int t_type;
	u_long t_base, t_len, t_end, c_end;
	short i, j;

	t_rc = resource_count(target_bp);
	c_rc = resource_count(cbp);
	t_rp = resource_list(target_bp);
	for (i = 0; i < t_rc; t_rp++, i++) {
		t_type = t_rp->flags & (RESF_TYPE | RESF_ALT);
		t_base = t_rp->base;
		t_len = t_rp->length;
		t_end = t_base + (t_len - 1);

		for (j = 0, c_rp = resource_list(cbp); j < c_rc; c_rp++, j++) {
			if (t_type == (c_rp->flags & (RESF_TYPE | RESF_ALT))) {
				c_end = c_rp->base + (c_rp->length - 1);
				if (((t_base == c_rp->base) &&
					(t_len == c_rp->length)) ||
					((t_base >= c_rp->base) &&
					(t_end <= c_end))) {
					/*
					 * target resource is completely
					 * matched, so delete it from target_bp
					 */
					(void) DelResource_devdb(target_bp,
						t_rp);
					t_rp--;
					break;
				} else if ((t_base >= c_rp->base) &&
					(t_base <= c_end)) {
					/*
					 * overlap at the beginning, so
					 * recalculate base and len to omit
					 * the overlapped portion
					 */
					t_rp->base = c_rp->base + c_rp->length;
					t_len = t_rp->length = t_end - c_end;
					t_base = t_rp->base = c_rp->base +
								c_rp->length;
				} else if ((t_base < c_rp->base) &&
					(c_rp->base <= t_end) &&
					(t_end <= c_end)) {
					/*
					 *overlap at the end, so recalculate
					 * the len only
					 */
					t_len = t_rp->length = c_rp->base -
								t_base;
					t_end = t_base + (t_len - 1);
				} else if ((t_base < c_rp->base) &&
					(t_end > c_end)) {
					/*
					 * the overlapped portion is in the
					 * middle, so split the resource into
					 * two portions
					 */
					t_rp->length = t_len = c_rp->base -
								t_base;
					target_bp = AddResource_devdb(target_bp,
						t_type & RESF_TYPE, c_end + 1,
						t_end - c_end);
					t_end = t_base + (t_len - 1);
					/* target_bp likely has been changed */
					t_rp = resource_list(target_bp) + i;
					t_rc = resource_count(target_bp);
				}
			}
		}
	}
	return (target_bp);
}

/* convert segment/offset pair to physical address */
static unsigned long
linear_addr(char *bp)
{
	ushort offset;
	unsigned long answer;

	answer = ((unsigned long) bp & 0xFFFF0000) >> 12;
	offset = (ushort) ((unsigned long) bp & 0xFFFF);
	answer += offset;
	return (answer);
}
