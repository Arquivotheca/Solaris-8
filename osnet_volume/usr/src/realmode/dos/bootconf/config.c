/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * config.c -- routines to handle "configure devices" menus
 */

#ident	"@(#)config.c	1.91	99/01/20 SMI"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <names.h>
#include "types.h"

#include "boards.h"
#include "boot.h"
#include "biosprim.h"
#include "config.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "menu.h"
#include "probe.h"
#include "resmgmt.h"
#include "tty.h"

#define	LINE_MAX 70	/* Max chars before adjusting a resource list */

char *ResourceName(Resource *rp);
char *Adjust(char *list, int x);
char *ListResources(Board *bp, int t, int x);
Board *GetResource(Board *bp, int t);
Board *DefineBoard(u_long id);
int add_config();

/*
 * Format a resource record for display:
 *
 * This routine formats the "meat" of a resource record for display.
 * Although all resource records have the same internal format, their
 * external (i.e, ASCII) respresentation differs according to type.
 *
 * Returns a pointer to a static buffer containing the formatted (i.e,
 * printable) resource record.
 */
char *
ResourceName(Resource *rp)
{
	int t = (rp->flags & RESF_TYPE);
	static char *buf = 0;
	static unsigned int bufsz;
	unsigned long ead;
	char *cp = 0;
	int x = 5;
	char *shared;

	if (buf == 0) {
		buf = malloc(64); /* initial buffer size */
		bufsz = 64;
	}
	if (rp->flags & RESF_DISABL) {
		strcpy(buf, gettext("Disable"));
	} else do if (t == (rp->flags & RESF_TYPE)) {
		/*
		 * Formatting of other resource records is controlled by the
		 * resource type. We do this in a loop to check for other
		 * resource assignments of the current type in the current
		 * choice list (e.g, discontiguous port assignments).
		 */
		if (!cp) cp = buf;
		else {
			int bufused = cp - buf;

			if (bufused >= (bufsz - 34)) {
				bufsz <<= 1;
				buf = realloc(buf, bufsz);
				cp = buf + bufused;
			}
			cp += sprintf(cp, ", ");
		}
		shared = "";
		if (rp->flags & RESF_SHARE) {
			shared = " (shared)";
		}

		switch (t) {

		case RESF_Irq:
		case RESF_Dma:
			/* IRQs and DMA channels displayed in decimal .. */
			cp += sprintf(cp, "%ld%s", rp->base, shared);
			break;

		case RESF_Port: x = 3;
			/* FALLTHROUGH */
		case RESF_Mem:
			/* I/O ports and memory locations displayed in hex */
			ead = rp->length - 1;
			if (ead) {
				ead += rp->base;
			}
			if (ead) {
				cp += sprintf(cp, "%0*lX-%0*lX%s",
				    x, rp->base, x, ead, shared);
			} else {
				cp += sprintf(cp, "%0*lX%s", x, rp->base,
				    shared);
			}
		}

	} while ((rp++)->flags & RESF_MULTI);

	return (buf);
}

/*
 * Adjust a long resource line:
 *
 * This routine adjusts a long resource list so that it doesn't get
 * truncated off the end of the screen. It does this by inserting
 * a newline after the last printable character that will fit on the
 * current line.
 *
 * Returns a pointer to a dynamically allocated buffer that contains
 * the adjusted resource list.
 */
char *
Adjust(char *list, int x)
{
	int j, k, t;
	char *buf, *lp, *cp = list;
	int n;

	t = strlen(list);
	n = (t / LINE_MAX) + 1;
	lp = list;
	for (j = x+3; *cp++ != ':'; j++) {
		;
	}
	if ((buf = cp = malloc(strlen(list) + (n * j))) == 0) {
		MemFailure();
	}

	/*
	 * Loop until we've processed the entire resource list string.
	 * The "cp"/"list" registers point to the next output/input
	 * byte, respectively.
	 */
	while (*list) {
		char *xp = strchr(list, ',');

		if (!xp++) {
			xp = strchr(list, 0);
		}
		if ((xp - lp) >= LINE_MAX) {
			/*
			 * The current line isn't going to make it onto the
			 * screen. Split it off and indent the appropriate
			 * amount.
			 */
			while (isspace(*list)) {
				list++;
			}
			while (isspace(cp[-1])) {
				cp--;
			}
			lp = list;
			*cp++ = '\n';
			for (n = j; --n; *cp++ = ' ') {
				;
			}
		}

		memcpy(cp, list, k = (xp - list));
		list += k;
		cp += k;
	}
	*cp = 0;
	return (buf);
}

/*
 * Build list of resource assignments:
 *
 * This routine builds a displayable list (in the specified output
 * "buf"fer) of the resources of type "t" that are currently assigned
 * to the board at "bp". Returns zero if there are no resources
 * of type "t" currently assigned to the board.
 */
char *
ListResources(Board *bp, int t, int x)
{
	char *type = ResTypes[t];
	Resource *rp = resource_list(bp);
	static char *buf = 0;
	static unsigned bufsz;
	unsigned bufused, rl;
	char *cp = 0;
	int k;

	if (buf == 0) {
		buf = malloc(64); /* initial buffer size */
		bufsz = 64;
	}
	for (k = resource_count(bp); k--; rp++) {
		/*
		 * Step thru the board's resource list looking for all
		 * entries of type "t"
		 */
		if (t == (rp->flags & RESF_TYPE)) {
			/*
			 * Found another one! Concatenate the external
			 * "ResourceName" to the display buffer.
			 */
			if (cp) {
				cp += sprintf(cp, ", ");
			} else {
				cp = buf + sprintf(buf, "%s: ", type);
			}

			bufused = cp - buf;
			rl = strlen(ResourceName(rp));
			if ((bufused + rl + 10) >= bufsz) {
				bufsz = ((bufused + rl) << 1);
				buf = realloc(buf, bufsz);
				cp = buf + bufused;
			}

			cp += sprintf(cp, "%s", ResourceName(rp));
			while (rp->flags & RESF_MULTI) {
				rp++;
				k--;
			}
		}
	}

	return (cp ? Adjust(buf, x) : "");
}

static struct menu_options Review_opts[] = {
	/* Function key list for the "Review devices" screen ... */

	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Add Device" },
	{ FKEY(4), MA_RETURN, "Delete Device" },
};

#define	NREVIEW_OPTIONS (sizeof (Review_opts) / sizeof (*Review_opts))

/*
 * Display a list of the devices (and resources)
 * and give the user the option to add, modify or delete them.
 */
void
do_config(void)
{
	struct menu_list *config_list;
	int nconfig_list;
	struct menu_list *choice;
	int done = 0;
	int change = 1; /* configuration changed */
	int nopts;

	/*
	 * Keep re-displaying the configured device list until user
	 * decides (s)he's done (i.e, presses the F2 key).
	 */
	while (!done) {
		assign_prog_probe();

		if (change) {
			menu_list_boards(&config_list, &nconfig_list, 1);
			change = 0;
		}
		nopts = NREVIEW_OPTIONS;
		switch (select_menu("MENU_HELP_REVIEW", Review_opts,
		    nopts, config_list, nconfig_list, MS_ZERO_ONE,
		    "MENU_REVIEW")) {

		case FKEY(2): /* Back */
		case '\n':
			done = 1;
			break;

		case FKEY(3): /* Add device */
			deassign_prog_probe();
			change = add_config();
			break;

		case FKEY(4): /* Delete device */
			/*
			 * User claims to have selected a device. Let's see
			 * if we can figure out which device that was.
			 */
			if (choice = get_selection_menu(config_list,
			    nconfig_list)) {
				Board *bp = (Board *) choice->datum;
				if (bp->bustype == RES_BUS_PNPISA) {
					/* can't delete isa pnp devs */
					enter_menu(0, "MENU_DEL_PNP");
				} else if (bp->devid ==
				    CompressName("SUNFFE2")) {
					/* can't delete motherboard */
					enter_menu(0, "MENU_DEL_MOTHR");
				} else {
					del_board(bp);
					change = 1;
				}
			} else {
				/*
				 * No device selected. Beep the console.
				 */
				beep_tty();
			}
			break;
		}
		if (change || done)
			free_boards_list(config_list, nconfig_list);
	}
}

static struct menu_options Get_options[] = {
	/* Options for "Get Resource" menu */

	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
	{ '\n', MA_RETURN, 0 }
};

#define	NGET_OPTIONS (sizeof (Get_options)/sizeof (*Get_options))

static char Decimal[] =
	"Valid input is a decimal number between 0 and %d, inclusive.";

static char Hexadecimal[] =
	"Valid input is a hexadecimal start address followed by an optional\n"
	"hyphen and end address in the range %lX-%lX.";

/*
 * Get user-keyed resource specification:
 *
 * This routine is used to interactively add resource specifications
 * to a self-describing board record that the "DefineBoard" is trying
 * to set up. Adding a new resource may cause the board record to
 * be resized, so we deliver the new record address as our return
 * value.
 */
Board *
GetResource(Board *bp, int t)
{
	int base;
	long a, e, ax, ex;
	char buf[32], *cp;
	char vt[256];
	int shared;
	char *help;

	cp = "";
	buf[0] = 0;

	/*
	 * Establish minimum start and maximum end values based on
	 * resource type. Also adjust the valid type explanation
	 * "vt" accordingly.
	 */
	switch (t) {
		case RESF_Port:
			(void) sprintf(vt, gettext(Hexadecimal), 0L, 0xFFFFL);
			ax = 0; ex = 0xFFFF;
			base = 16;
			help = "MENU_HELP_IO";
			break;

		case RESF_Mem:
			(void) sprintf(vt, gettext(Hexadecimal), 0xA0000L,
				0xFFFFFL);
			ax = 0xA0000; ex = 0xFFFFF;
			base = 16;
			help = "MENU_HELP_MEM";
			break;

		case RESF_Irq:
			(void) sprintf(vt, gettext(Decimal), 15);
			ax = 0; ex = 15;
			base = 10;
			help = "MENU_HELP_IRQ";
			break;

		case RESF_Dma:
			(void) sprintf(vt, gettext(Decimal), 7);
			ax = 0; ex = 7;
			base = 10;
			help = "MENU_HELP_DMA";
			break;
	}

	/*
	 * Main loop. Keep displaying the input screen until user
	 * enters a valid resource specification of cancels out.
	 */
	while (cp) {
		switch (input_menu(help, Get_options,
		    NGET_OPTIONS, buf, sizeof (buf), MI_ANY,
		    "MENU_GETRESOURCE", ResTypes[t], vt, cp)) {
		case FKEY(2):
		case '\n':
			/* Apply; validate user-keyed input. */
			shared = 0;
			if (((a = strtol(buf, &cp, base)) >= 0) && (cp > buf) &&
			    (a >= ax) && (a <= ex)) {
				/*
				 * Base resource address looks good, now check
				 * for ending address specification.
				 */
				if (t == RESF_Mem) {
					e = a + 1023;
				} else {
					e = a;
				}
				if ((*cp == 's') || (*cp == 'S')) {
					shared = 1;
					cp++;
				}

				if ((base == 16) && (*cp == '-')) {
					cp++;
					/*
					 * Only resources specified in hex can
					 * have ending addresses. Go find it
					 */
					if (((e = strtol(cp, &cp, 16)) <= 0) ||
					    (e <= a) || (e > ex)) {
						/*
						 * Ending address is bogus!
						 */
						goto no;
					}
					if ((*cp == 's') || (*cp == 'S')) {
						shared = 1;
						cp++;
					}
				}

				if ((t == RESF_Mem) &&
				    ((a & 255) || ((e+1-a) & 1023))) {
					/*
					 * Memory resources have some
					 * additional alignment constraints.
					 */
					cp = (char *)gettext
					    ("Bad memory alignment");
					break;
				}

				if ((t == RESF_Irq) && (a == 2)) {
					a = e = 9;
				}

				if (cp != 0) {
					long l = (e-a)+1;

					if (Query_resmgmt(bp, t, a, l)) {
						cp = (char *)gettext(
						    "Duplicate resource");
						break;
					}

					return (AddResource_devdb(bp,
					    shared ? (t | RESF_SHARE) : t,
					    a, l));
				}
			}

		no:	cp = (char *)gettext("Invalid resource specification");
			break;

		case FKEY(3):
			/* Cancel; return unmodified board record. */
			return (bp);
		}
	}
	/*NOTREACHED*/
}

static struct menu_options Rss_options[] = {
	/* Options for selecting resource list to modify */

	{ FKEY(2), MA_RETURN, "Continue"},
	{ FKEY(3), MA_RETURN, "Cancel" },
	{ FKEY(4), MA_RETURN, "Add" }
};

#define	NRSS_OPTIONS (sizeof (Rss_options)/sizeof (*Rss_options))

/*
 * This routine may be used to add ISA devices
 *
 * Returns a pointer to the device we defined, or NULL if user
 * changes hir mind.
 */
Board *
DefineBoard(u_long id)
{
	int j, k;
	Board *bp;
	char *cp = "";
	struct menu_list *choice;

	bp = new_board();
	bp->devid = id;
	bp->bustype = RES_BUS_ISA;
	bp->flags = BRDF_DISK;
	cp = "";

	/*
	 * Resource assignment
	 */
	for (;;) {
		int n = 0;
		struct menu_list mlist[RESF_Max];

		/*
		 * Display the current resource assignments in the
		 * resource display buffers.
		 */
		for (j = 1; j < RESF_Max; j++) {
			char buf[32];
			char *rlp = 0;

			if (bp->rescnt[n] > 0) {
				/* Resources exist, list them. */
				rlp = ListResources(bp, j, 0);
			}

			if (!rlp) {
				/* No resources, Use empty list. */
				(void) sprintf(buf, "%s:", ResTypes[j]);
				rlp = strdup(buf);
			}

			mlist[n].string = rlp;
			mlist[n].flags = 0;
			n++;
		}

		j = select_menu("MENU_HELP_RESSELECT", Rss_options,
		    NRSS_OPTIONS, mlist, n, MS_ZERO_ONE, "MENU_RESSELECT", cp);
		cp = "";
		for (k = 0; k < n; free(mlist[k++].string)) {
			;
		}

		/*
		 * Process according to function keystroke ...
		 */
		switch (j) {
			case FKEY(2):
				if (choice = get_selection_menu(mlist, n)) {
					/*
					 * Found the resource type. Go ahead
					 * and process it!
					 */
					n = (choice-mlist) + 1; /* Res type */
					bp = GetResource(bp, n);

				} else {
					/*
					 * No type selected, beep the console.
					 */
					beep_tty();
				}

				break;

			case FKEY(4):
				/*
				 * We only return records that have
				 * resources assigned to them. Empty
				 * device descriptions are bogus!
				 */
				if (resource_count(bp) > 0) {
					return (bp);
				}

				cp = (char *)gettext("No resources assigned");
				break;

			case FKEY(3):
				/*
				 * User is bailing out. Free up the board
				 * record and exit!
				 */
				free_board(bp);
				return (0);
		}
	}
	/*NOTREACHED*/
}

/*
 * Allow the user to define an ISA device and then check for conflicts.
 */
int
add_config()
{
	Board *bp, *cbp;
	Resource *crp;
	u_long id;

	id = get_isa_id_devdb();
	if (id == 0) { /* did user cancel the get id */
		return (0);
	}

	bp = DefineBoard(id);
	if (bp == NULL) { /* did user cancel the add? */
		return (0);
	}
	if (crp = board_conflict_resmgmt(bp, 0, &cbp)) {
		u_short t;
		char *res_type;

		t = crp->flags & RESF_TYPE;
		if (t == RESF_Port) {
			res_type = "I/O port";
		} else {
			res_type = ResTypes[t];
		}
		enter_menu(0, "MENU_PRIME_CONFLICT", res_type,
		    GetDeviceName_devdb(cbp, 1), res_type,
		    ResourceName(crp));
		free_board(bp);
	} else {
		Update_escd = 1;
		add_board(bp);
		return (1);
	}
	return (0);
}
