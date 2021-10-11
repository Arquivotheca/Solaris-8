/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 *
 *  probe.c - Device/resource probing.
 *
 *    This file implements the "legacy probe" interface to the Solaris real-
 *    mode device drivers ("*.bef" files).  Device probing and configuartion
 *    occurs in two phases:
 *
 *	The device probe phase:  Drivers are loaded into memory one by one
 *	   and allowed to probe for the devices under their control. Drivers
 *	   must callback to the "resource" routine (below) to make a res-
 *	   ervation on the each port address they intend to probe.  If the
 *	   reservation is granted (and the driver detects one of its devices
 *	   at the target address), other resources may be reserved as well.
 *	   The driver can then callback to the "node" routine (below) to
 *	   install the reserved configuration.
 *
 *	The resource probe phase:  Resource reservations (for everything other
 *	   than I/O addresses) may be conditional.  In this case the driver
 *	   provides a list of possible resources that could be used to satisfy
 *	   the device's needs.  When the probe is complete, devices with con-
 *	   ditional reservations are not immediately installed.
 *	   Instead, the corresponding configuration description is set aside
 *	   until all drivers have had a chance to probe for devices, at which
 *	   point the "resource_probe" routine builds a non-conflicting perm-
 *	   uation of all outstanding conditional reservations which it then
 *	   saves.
 */

#ident	"@(#)probe.c	1.100	99/05/26 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <names.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <befext.h>
#include "types.h"

#include "adv.h"
#include "boards.h"
#include "boot.h"
#include "debug.h"
#include "devdb.h"
#include "dir.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "menu.h"
#include "pnpbios.h"
#include "probe.h"
#include "prop.h"
#include "resmgmt.h"
#include "setjmp.h"
#include "tree.h"
#include "tty.h"
#include "bop.h"
#include "acpi_rm.h"

/*
 * Conflicts happen during callbacks from the driver. Unfortunately
 * we can't spill our guts at that point regarding the conflict because
 * some conflicts get resolved or are related to ports being in use
 * and the driver is just looking around. At the point that we know
 * a conflict has occurred we don't have the information necessary.
 * This structure is filled in when a conflict occurs and linked into
 * a global chain that can be used later.
 */
typedef struct conflictlist {
	struct conflictlist	*next;
	Board			*confbp;	/* .. current owner of res */
	int			restype;	/* .. which resource	*/
	long			base,		/* .. actual info	*/
				length;
	devtrans		*dtp;		/* .. who tripped us up	*/
} ConflictList;

static int CBeflg;		/* Reservation error flag		    */
static int CBxflg;		/* Error message indentation flag	    */
static int CBzflg;		/* Conflict return from "load()" routine    */
static int CBcflg;		/* Conflicts OK in probe if set		    */

static Board *CBeptr;		/* Pointer to conflicting board.	    */
static ConflictList *CBzlst;	/* List of conflict information		    */
static Board *cbp, *hwm;	/* Current board, permute high water mark   */
static Board *pgmdevs;		/* Programmable device list		    */

static int pflg;		/* Global print flag			    */
static char *bef;		/* Name of current driver file		    */
static devtrans *dtp;		/* Driver translation pointer		    */
static jmp_buf BEFexit;		/* Error exit context for bad drivers	    */
static char indent[80];		/* Buffer for status msg indentation	    */
static int gotnode;		/* New function/board flags		    */
static unsigned watchdog = 0;	/* Watchdog timeout			    */
static u_char probe_optional;	/* global - bef type			    */

#define	WATCHLIM 5000L		/* Max recursion count			    */
#define	UPPER_ISA_DIR 199
#define	UPPER_ISA_ALWAYS_DIR 99

/*
 * Get resource specification length
 *
 * Given resouce type "t", this macro delivers the length of the
 * corresponding resource as noted in the "j"th entry of the
 * callback resource "buf"fer.  This macro only works when called
 * from a callback routine.
 */

#define	ResLength(t) ((ResTuples[t] < 3) ? 1L : buf[j+1])

/*
 * Function prototypes
 */
void badbef(const char *msg);
int setres(int op, char *nam, DWORD *buf, DWORD *len);
int relres(int op, char *nam, DWORD *buf, DWORD *len);
int far resource(int op, char far *nam, DWORD far *buf, DWORD far *len);
int far node(int op);
int far property(int op, char *name, char **value, int *len, int bin);
int load(char *file);
int FindAny(Resource *rp, unsigned long *state);
int try(Board *bp, int n);
int permute(Board *bp, int n);
Board *resource_probe(Board *bp);
void remove_devs_probe(char *bef);
static void run_plat();

/*
 * Generate "BAD BEF" error message:
 *
 * This routine prints the specified error "msg", after first indent-
 * ing the current line as approprate. The we then longjmp() back to
 * the *.bef loader, which unloads the driver before things get ugly!
 */
void
badbef(const char *msg)
{
	if (!pflg) {
		clear_tty();
	}
	if (CBxflg++) {
		iprintf_tty(indent);
	}
	iprintf_tty("%s (%s) - %s", gettext("BAD BEF"), bef, gettext(msg));
	longjmp(BEFexit, 1); /* Unload driver before things get worse! */
}

/*
 * Reserve resources:
 *
 * This routine is invoked from the "resource" callback routine to
 * reserve resources on behalf of a given device. The "nam" argument
 * specifies the type of resource the driver is trying to reserve,
 * while the "buf" and "len" arguments specify which resources are
 * desired. The high order bits of the "op"-code may contain the
 * following flags:
 *
 *   RES_SHARE: If the resource may be shared with other devices
 *   RES_WEAK: If the resource may be usurped by other devices
 *   RES_USURP: If the resource is weak it will be taken from holder
 *
 * Returns 0 if the reservation is successful, a "befext" error code if not.
 */
int
setres(int op, char *nam, DWORD *buf, DWORD *len)
{
	int j;
	Board *confbp;
	u_short type, x, flag;

	/* Process according to resource type */
	flag = 0;
	if (op & RES_USURP)
		flag |= RESF_USURP;
	if (op & RES_SHARE)
		flag |= RESF_SHARE;
	switch (tolower(*nam)) {
	default:
		/*
		 * The driver has misspelled the resource name (and this
		 * despite the fact that we only check the first byte!)
		 */

		badbef("invalid resource type");
		/*NOTREACHED*/

	case 's':
		/*
		 * Slot number.  This should really come from top-down
		 * probing, but keyboard/mouse needs it and it's too
		 * much work to do now to change them to top-down.
		 */
		cbp->slot = buf[0];
		break;

	case 'n':
		/*
		 * Name reservation: This is where we pick up the EISA device
		 * name and bus type. If the driver is setting up a DCD, we
		 * pick up the vendor and device ID fields as well.
		 */
		cbp->bustype = buf[1];
		cbp->devid = buf[0];
		break;

	case 'i':
		x = RESF_Irq;
		for (j = 0; j < *len; j += ResTuples[x]) {
			/*
			 * Convert IRQ 2 to 9 because of the cascading used
			 * by the pc hardware.
			 */

			if (buf[j] == 2)
				buf[j] = 9;
		}
		goto reserve;
	case 'p': x = RESF_Port; goto reserve;
	case 'd': x = RESF_Dma;	goto reserve;
	case 'm': x = RESF_Mem;
	reserve:

		if (*len % ResTuples[x]) {
			/*
			 * The resource buffer length (in long words) must be
			 * a multiple of the tuple size. If it's not,
			 * generate an error.
			 */

			badbef("invalid buffer length");
		}

		for (j = 0; j < *len; j += ResTuples[x]) {
			/*
			 * Driver is trying to make a specific reservation.
			 * Run thru the "buf" list and check to make sure that
			 * none of the requested resources are already in use.
			 * Unless it's marked as a shared resource, in which
			 * case we skip the check and just add the resource.
			 */

			confbp = Query_resmgmt(Head_board, x | flag, buf[j],
				ResLength(x));
			if (confbp) {
				if ((op & RES_SILENT) == 0) {
					/*
					 * There's a conflict with another
					 * device. Save the resource type so
					 * we can print an informative error
					 * message if the driver can't resolve
					 * the conflict.
					 */

					ConflictList *clp;

					CBeflg = x;
					clp = malloc(sizeof (ConflictList));
					clp->restype = x;
					clp->confbp = confbp;
					clp->base = buf[j],
					clp->length = ResLength(x);
					clp->dtp = dtp;
					clp->next = CBzlst;
					CBzlst = clp;

				}
				return (RES_CONFLICT);
			}
		}

		type = x;
		if (op & RES_SHARE) {
			type |= RESF_SHARE;
		}
		if (op & RES_WEAK) {
			type |= RESF_WEAK;
		}
		if (op & RES_USURP) {
			type |= RESF_USURP;
		}
		for (j = 0; j < *len; j += ResTuples[x]) {
			/*
			 * Step thru the "buf" array and convert each tuple
			 * therein into a "Resource" record of the
			 * appropriate type.
			 */
			cbp = AddResource_devdb(cbp, type, buf[j],
			    ResLength(x));
		}
		break;
	}

	return (0);
}

int
relres(int op, char *nam, DWORD *buf, DWORD *len)
{
	/*
	 * Release resource reservations:
	 *
	 * This routine implements the RES_REL function and is called from
	 * the resource callback routine when the driver specifies RES_REL
	 * as the "op"-code. We remove the requested resources from the
	 * current function.
	 *
	 * This operation is handy when a driver needs a resource for probing,
	 * but that resource is not actually used by the device detected by
	 * the probe. Some drivers, for example, find the IRQ line by forcing
	 * the device to interrupt then polling all available IRQs to find the
	 * one that actually delivers the interrupt. Reservations placed on
	 * the non-interrupting IRQ must be released before the device can
	 * be configured.
	 *
	 * Returns the standard <befext.h> exit values.
	 */

	int j, x;
	unsigned n = 0;

#ifdef	__lint
	op++;
#endif

	switch (tolower(*nam)) {
	/* Process according to resource type */

	default:
		/*
		 * Bogus resource type code. Can't do much with this ..
		 */

		badbef("invalid operation");
		/*NOTREACHED*/

	case 'p':
		x = RESF_Port;
		goto release;
	case 'i':
		x = RESF_Irq;
		goto release;
	case 'd':
		x = RESF_Dma;
		goto release;
	case 'm':
		x = RESF_Mem;
	release:
		/*
		 * Check if Length is a proper multiple of the tuple size.
		 */
		if (*len % ResTuples[x])
			badbef("invalid buffer length");
		for (j = 0; j < *len; j += ResTuples[x]) {
			/*
			 * Caller may release multiple resources of the same
			 * type in a single call. Step thru the tuple buffer
			 * until we've done them all!
			 */

			Resource *rp = resource_list(cbp);
			int k;

			for (k = resource_count(cbp); k--; rp++) {
				/*
				 * Search the entire resource list until we
				 * find the entry that matches the tuple at
				 * "buf[j]"
				 */

				if ((x == (rp->flags & RESF_TYPE)) &&
				    (rp->base == buf[j]) &&
				    (rp->length == ResLength(x))) {
					/*
					 * This is it! Remove this resource
					 * entry from the resource list and
					 * increment the "n" register to note
					 * the number of bytes by which the
					 * board record has shrunk.
					 */

					(void) DelResource_devdb(cbp, rp);
					n += sizeof (Resource);
					break;
				}
			}
		}

		break;
	}

	if (n == 0) {
		/*
		 * Nothing got deleted, probably because the resource the
		 * caller was trying to delete wasn't reserved!
		 */
		badbef("releasing unreserved resource");
	}

	return (0);
}

int far
resource(int op, char far *nam, DWORD far *buf, DWORD far *len)
{
	/*
	 * Resource reservation callback:
	 *
	 * This is the "resource" callback for BEF legacy probes. It transfers
	 * control to one of the "res*" routines defined above based on
	 * the "op"-code provided by the caller.
	 *
	 * Returns zero if it works, RES_CONFLICT if the request is denied
	 * due to resource conflict. Longjmp's back to the *.bef loader if
	 * a serious error occurs.
	 */

	CBeflg = -1;	/* Reset the global error flag: The driver has	    */
			/* .. resolved any previous conflicts!		    */

	switch (op & 0xFF) {
		/*
		 * Process according to the callback command code.
		 * We don't support the RES_GET function: It's only
		 * used when the drivers are actually installed.
		 */

		case RES_SET: return (setres(op, nam, buf, len));
		case RES_REL: return (relres(op, nam, buf, len));
	}

	badbef("invalid resource callback");
	/*NOTREACHED*/
}

/*
 * Device node callback:
 *
 * This routine serves as the device node callback for realmode
 * drivers in the device probe phase. The driver uses this routine
 * to activate device nodes prior to probing and to deactivate them
 * (and to optionally install) when the probing is complete.
 * Three "op" codes are recognized:
 *
 *    NODE_START: Activate a device node prior to probing
 *    NODE_DONE:  Deactivate node after probing & install
 *    NODE_FREE:  Deactivate the current node without installing it.
 *
 * Routine returns 0 if it works, longjmp's back to the driver loader
 * if something goes wrong.
 */
int far
node(int op)
{
	Board *bp;
	Resource *rp;

	switch (op & 0xFF) {
	/* Process according to the "op"tion code */

	case NODE_START:
		/*
		 * The driver wants to activate a new device node. This means
		 * reinitializing the board record and resetting the various
		 * flags.
		 */

		if (!(gotnode ^= 1)) {
			/*
			 * We can't start a new node if the current one is
			 * still active!
			 */

			badbef("invalid node/resource protocol");
		}

		ResetBoard_devdb(cbp);
		cbp->devid = dtp->devid;

		cbp->bustype = RES_BUS_ISA;
		CBeflg = -1;
		return (NODE_OK);

	case NODE_FREE:
		/*
		 * Caller is giving up on this node. There's not much to
		 * do except reset the "gotnode" flag.
		 */

		if (gotnode ^= 1) {
			/*
			 * We can't free a node that's not active!
			 */

			badbef("invalid node/resource protocol");
		}

		if ((CBeflg >= 0) && (rp = primary_probe(cbp)) &&
		    (CBeflg != (rp->flags & RESF_TYPE))) {
			/*
			 * We appear to have bombed out due to a resource
			 * conflict (other than primary resource that defines
			 * the existance of the device). Set the
			 * global flag so that we can print out something after
			 * we've searched for all devices of this type.
			 */

			CBzflg = 1;
		} else if (CBzlst && (CBeflg != -1)) {
			/*
			 * At this point we have a conflict on our linked
			 * chain that shouldn't be there. Primary ports
			 * that aren't available aren't to be considered
			 * conflicts.
			 */

			ConflictList *clp = CBzlst;
			CBzlst = clp->next;
			free(clp);
		}
		return (NODE_OK);

	case NODE_DONE:
		/*
		 * Driver wants to install this node. If there are no
		 * conditional resource assignments, we can do it immediately.
		 * Otherwise, we'll have to wait for the resource probe phase.
		 */
		if (gotnode ^= 1) {
			/*
			 * We can't install a node that isn't active!
			 */
			badbef("invalid node/resource protocol");
		}

		/*
		 * Mark this as a scanned (probed) for bef.
		 */
		cbp->slotflags |= RES_SLOT_PROBE;
		if (probe_optional) {
			cbp->flags |= BRDF_DISK;
		}

		/*
		 * Don't add this node if it already exists and the unique
		 * flag is set.
		 */
		if (op & NODE_UNIQ) {
			for (bp = Head_board; bp; bp = bp->link)
				if (equal_boards(bp, cbp))
					return (NODE_OK);
		}
		bp = copy_boards(cbp);

		/*
		 * validate the dbentry now so that count boards has a
		 * cross reference point. dtp is setup in load().
		 */
		bp->dbentryp = dtp;

		/*
		 * The device is now fully configured. Add the board
		 * record to the Head_board list.
		 * Check with ACPI's list first.
		 */
		if ((bp = acpi_check(bp)) != NULL) {
			add_board(bp); /* Tell rest of system about device */
		}
		check_weak();
		Update_escd |= pflg;

		return (NODE_OK);
	case NODE_INCOMPLETE:
		/*
		 * Driver could not acquire all necessary resources due to
		 * conflicts, but we cannot free its current resources since
		 * the device really exists.  Flag the board to not generate
		 * a device tree node, but leave it in the board list so
		 * Its resources will stay reserved.
		 */
		if (gotnode ^= 1) {
			/*
			 * We can't process a node that's not active!
			 */
			badbef("invalid node/resource protocol");
		}
		if ((CBeflg >= 0) && (rp = primary_probe(cbp)) &&
		    (CBeflg != (rp->flags & RESF_TYPE))) {
			/*
			 * We appear to have bombed out due to a resource
			 * conflict (other than primary resource that defines
			 * the existance of the device). Set the
			 * global flag so that we can print out something after
			 * we've searched for all devices of this type.
			 */

			CBzflg = 1;
		} else if (CBzlst && (CBeflg != -1)) {
			/*
			 * At this point we have a conflict on our linked
			 * chain that shouldn't be there. Primary ports
			 * that aren't available aren't to be considered
			 * conflicts.
			 */

			ConflictList *clp = CBzlst;
			CBzlst = clp->next;
			free(clp);
		}
		/*
		 * Mark this as a scanned (probed) for bef.
		 */
		cbp->slotflags |= RES_SLOT_PROBE;
		if (probe_optional) {
			cbp->flags |= BRDF_DISK;
		}
		/*
		 * Mark this board to not generate a device tree node and
		 * as disabled.
		 */
		cbp->flags |= (BRDF_NOTREE|BRDF_DISAB);
		bp = copy_boards(cbp);
		/*
		 * validate the dbentry now so that count boards has a
		 * cross reference point. dtp is setup in load().
		 */
		bp->dbentryp = dtp;
		/*
		 * The device is partially configured. Add the board
		 * record to the Head_board list.
		 * Check with ACPI's list first.
		 */
		if ((bp = acpi_check(bp)) != NULL) {
			add_board(bp); /* Tell rest of system about device */
		}
		check_weak();
		return (NODE_OK);
	}

	badbef("invalid node callback");
	/*NOTREACHED*/
}

/*
 * Property operations callback
 *
 * This is the "properties" callback for BEF legacy probes.
 * We support setting either character or binary properties.
 *
 * Returns PROP_OK if it works, PROP_FAIL otherwise.
 * Longjmp's back to the *.bef loader if a serious error occurs.
 */
int far
property(int op, char *name, char **value, int *len, int bin)
{
	char buf[32]; /* max property name len is 31 */
	char *tp;

	CBeflg = -1;	/* Reset the global error flag: The driver has */
			/* .. resolved any previous conflicts! */

	if (cbp == NULL)
		return (PROP_FAIL);
	switch (op & 0xFF) {
		case PROP_SET:
			return (SetDevProp_devdb(&cbp->prop, name, *value,
			    *len, bin));
		case PROP_GET:
			*value = NULL;
			*len = 0;
			if (GetDevProp_devdb(&cbp->prop, name, value, len) <
			    0) {
				/*
				 * Try options node for this prop
				 */
				strcpy(buf, name);
				tp = read_prop(buf, "options");
				*value = tp;
				if (*value == NULL)
					return (PROP_OK);
				*len = strlen(*value) + 1;
			}
			return (PROP_OK);
		case PROP_SET_ROOT:
			out_bop("dev /\n");
			out_bop("setprop ");
			out_bop(name);
			out_bop(" ");
			out_bop(*value);
			out_bop("\n");
			return (PROP_OK);
		case PROP_GET_ROOT:
			*value = NULL;
			*len = 0;
			strcpy(buf, name);
			tp = read_prop(buf, "");	/* root node */
			*value = tp;
			if (*value == NULL)
				return (PROP_OK);
			*len = strlen(*value) + 1;
			return (PROP_OK);
	}

	badbef("invalid property callback");
	/*NOTREACHED*/
	return (PROP_FAIL);
}

static struct bef_interface bif = {
	/*
	 * BEF interface callback structure:
	 *
	 * Each driver (".bef" file) gets a pointer to this structure when it
	 * is invoked at its LEGACYPROBE entry point. The driver can then
	 * use it to locate the above routines to do resource allocation and
	 * to build device nodes.
	 */

	BEF_IF_VERS,	/* Interface version number */
	resource,	/* Ptr to resource allocation routine */
	node,		/* Ptr to device node allcoation routine */
	property,	/* Property node parser */
	bef_print_tty,	/* Driver putc callback */
	mem_adjust,	/* Driver memory re-size routine */
	0,		/* Driver base paragraph address */
	0,		/* Driver size in paragraphs */
};

const char prob_always[] = "solaris/drivers/isa.0";

/*
 * Load realmode driver:
 *
 * This routine loads the specified realmode driver at "file" and
 * invokes its LEGACYPROBE entry point to initiate probing for the
 * corresponding devices.
 *
 * Returns a non-zero value if probes fail due to resource conflicts.
 */
int
load(char *file)
{
	char *cp, *fp;
	static int recur = 0;

	CBzflg = 0;
	bef = strrchr(file, '/');
	if (!bef++) {
		bef = file;
	}

	probe_optional = strncmp(prob_always, file, sizeof (prob_always) - 1);

	if (dtp = TranslateDriver_devdb(bef)) {
		/*
		 * We were able to translate the driver name into a device
		 * type name (which becomes the "board" name for any devices
		 * that we create on this driver's behalf). Now we
		 * can actually load the driver.
		 */

		char *np = dtp->dname;
		CBxflg = gotnode = 0;
		CBeflg = -1;

		for (cp = indent; *np++; *cp++ = ' ');
		strcpy(cp, "  ");

		/*
		 * This is ugly. We don't load the com or lpt befs
		 * if these devices have been found by the pnpbios.
		 * This is due to problems woth weak resources, but
		 * also has the side benefit of speeding things up.
		 */
		if (Parallel_ports_found && (strcmp(bef, "lpt.bef") == 0)) {
			goto count;
		}
		if (Serial_ports_found && (strcmp(bef, "com.bef") == 0)) {
			goto count;
		}

		remove_devs_probe(bef);

		if (!(CBzflg = setjmp(BEFexit))) {
			/*
			 * Make sure longjmp buffer is set up before doing
			 * anything that might result in a "badbef" call!
			 */

			if (fp = LoadBef_befext(file)) {
				/*
				 * Driver loaded successfully. Make sure it
				 * supports the new LEGACYPROBE entry point.
				 */

				if (pflg) {
					lb_info_menu("Scanning: %s",
						dtp->dname);
					lb_inc_menu();
				}

				if (*(unsigned long *)(fp + BEF_EXTMAGOFF) ==
					    (unsigned long)BEF_EXTMAGIC) {
					/*
					 * Call into the driver's legacy probe
					 * entry point. The driver will call
					 * the "node" and "resource" routines
					 * above via the "bif" transfer vector.
					 * It returns after all probing is
					 * complete.
					 */

					(void) CallBef_befext(BEF_LEGACYPROBE,
					    &bif);

				} else if (!recur++) {
					/*
					 * Rotten beef! User seems to have
					 * some old drivers lying around.
					 */

					enter_menu(0, "MENU_BAD_BEF_MAGIC",
					    file);
				}

				FreeBef_befext();
				recur = 0;

			} else {
				/*
				 * This shouldn't happen. If driver fails to
				 * load we have a big problem!
				 */

				enter_menu(0, "MENU_BAD_BEF_LOAD", file);
			}
		}

count:;
	} else if (pflg) {
		/*
		 * No device translation available. Print error message and
		 * return with no further action.
		 */
		enter_menu(0, "MENU_NO_DB_ENTRY", bef);
	}

	return (CBzflg);
}

/*
 * Define next alternate resource:
 *
 * RESF_ANY resource records are used by DCDs to compactly encode
 * a range of resources that may be used in place of one another.
 * This routine locates the next alternate resource defined by the
 * RESF_ANY record at "rp", updates "*rp" to represent that specific
 * entry, and returns a non-zero value. A zero return means that
 * we've exhausted the possible resource assignments encoded in the
 * RESF_ANY record at "rp".
 *
 * The 32-bit "state" word is used to iterate over the alternates.
 */
int
FindAny(Resource *rp, unsigned long *state)
{
	int rc = -1;

	switch (rp->flags & (RESF_TYPE+RESF_ANY)) {
		/*
		 * There are two types of RESF_ANY records -- Range records
		 * and unit records:
		 */

		case RESF_Mem+RESF_ANY:
		case RESF_Port+RESF_ANY:
			/*
			 * .. Range records are used for Ports and Memory.
			 * The previous two resource records contain the
			 * min/max addresses and alignment/length in their
			 * "base" and "length" fields, respectively.
			 */

			rc = ((rp->base = *state) <= rp[-1].base);
			rp->length = rp[-1].length;
			*state += rp[-2].length;
			break;

		case RESF_Irq+RESF_ANY:
		case RESF_Dma+RESF_ANY:
			/*
			 * .. Unit records are used for IRQs and DMA channels.
			 * The previous resource record's "base" field
			 * contains a bit map that specifies which resources
			 * are valid alternates.
			 */

			rc = ((rp->base = ffbs(*state)) != 0);
			rp->base -= (rp->length = 1);
			*state &= ~(1 << rp->base);
			break;
	}

	return (rc);
}

/*
 * Try an alternate resource:
 *
 * This routine attempts to reserve the n'th resource in the given
 * function's resource group. The indicated resoruce is assumed to
 * be the first of a group of one or more alternates (i.e, it's
 * RESF_ALT and RESF_FIRST bits must be set).
 *
 * Returns a number whose absolute value gives the number of resource
 * records in the alternate group. If the number is positve, we
 * were able to successfully reserve all resources in the group. If
 * the number is negative, none of the resources in the group were
 * reserved.
 */
int
try(Board *bp, int n)
{
	Resource *rp = resource_list(bp) + n;
	unsigned long state;
	Board *bxp = 0;
	int j = 1, k = 0;
	int x = 0;
	int states_left;

	do {
		/*
		 * Check each entry in the resource group, stopping when
		 * we reach the first entry with a zero RESF_MULTI flag.
		 */
		if (((k += j) > 0) &&
		    ((rp->flags & (RESF_ALT+RESF_DISABL)) == RESF_ALT)) {
			/*
			 * So far so good, but we have to check the next
			 * resource record to see if it conflicts with
			 * anything that we've already configured in.
			 */
			int n1, t = rp->flags & RESF_TYPE;

			ASSERT(t && (t < RESF_Max));

			/*
			 * If this is a RESF_ANY record, advance the
			 * "rp" register to the place holder record
			 * and initialize the iteration state.
			 */
			if ((rp->flags & RESF_ANY) && !x++) {
				if ((t == RESF_Port) || (t == RESF_Mem)) {
					n1 = 2;
				} else {
					n1 = 1;
				}
				state = rp->base;
				rp += n1;
				k += n1;
			}

			if ((((states_left = FindAny(rp, &state)) != 0) &&
			    !(bxp = Query_resmgmt(pgmdevs, t,
			    rp->base, rp->length)) &&
			    !(bxp = Query_resmgmt(Head_board, t,
			    rp->base, rp->length))) || (CBcflg && (n1 < 2))) {
				/*
				 * We found a non-conflicting resource
				 * assignment (or, if "CBcflg" is set, we're
				 * willing to live with the conflict). Turn
				 * off the RESF_ALT flag to mark this as an
				 * actual resource assignment.
				 */
				rp->flags &= ~RESF_ALT;
				x = 0;
				bp->flags &= ~BRDF_DISAB;

			} else if ((rp->flags & RESF_ANY) && states_left) {
				/*
				 * If we're dealing with a RESF_ANY record,
				 * we don't want to advance past it until
				 * we've tried all the possibilities.
				 */
				k--;
				rp -= 1;

			} else {
				/*
				 * Resource assignment failed. Save info
				 * generate an error message later on.
				 */
				j = -1;
				k = 0 - k;
				CBeflg = t;
				CBeptr = bxp;
				rp->flags |= (RESF_CNFLCT | RESF_ALT);
				hwm = bp;
			}
		}

	} while ((rp++)->flags & RESF_MULTI);

	return (k);
}

/*
 * Assign conditional resources:
 *
 * This recursive routine steps thru the given function's resoruce
 * list, starting with the "n"th entry and checking all RESF_ALT
 * ressources in the current group. It applies the "try" routine to
 * each linked resource group until a successful reservation is
 * made, at which point it recursively processes the next alternate
 * resource group of the current function.
 *
 * If, upon entry, we find that there are no RESF_ALT reservations
 * remaining we assume that all resource assignments are complete and
 * we recursively try the next function or device in the programmable
 * device list.
 *
 * Returns a non-zero value if we can't complete the conditional
 * reservations given the curren state of the resource maps.
 */

int
permute(Board *bp, int n)
{
	int rc = resource_count(bp);
	Resource *rp = resource_list(bp) + n;

	if (watchdog < WATCHLIM) {
		watchdog++;
	} else {
		return (1);
	}

	while ((n < rc) && !(rp->flags & RESF_SUBFN)) {
		/*
		 * Scan thru this board's resource list until we find
		 * the first record of the next subfunction to be configured.
		 */
		rp += 1;
		n++;
	}

	if (n < rc) {
		/*
		 * We have an outstanding alternate resource group that needs
		 * to be assigned. The "rp" register points to the first
		 * record of this group.
		 *
		 * The "j" register gives the number of Resource records in
		 * the current group (usually 1, but may be more if MULTI
		 * bits are set). The "k" register is zero when this and all
		 * subsequent resources are successfully assigned.
		 */

		int j, k;
		int q = n;

		do {
			/*
			 * Try each alternate in the group until we complete
			 * the configuration or run out of alternates. The
			 * "try" routine does the dirty work, checking for
			 * conflicts and applying reservations as indicated.
			 */
			j = try(bp, n);
			k = ((j > 0) ? permute(bp, n+j) : 1);

			for (j = abs(j); j--; (n++, rp++)) {
				/*
				 * Advance the "rp" register over the resource
				 * records that were processed by the "try"
				 * routine. If the reservation failed, we
				 * will have to back out any partial resource
				 * assignments made by the "try" routine.
				 */

				if (k) {
					rp->flags |= RESF_ALT;
				}
			}

			ASSERT((n > q) && ((q = n) >= 0));
			ASSERT((n >= rc) || (rp->flags & RESF_FIRST));

		} while ((k != 0) && (n < rc) && !(rp->flags & RESF_SUBFN));

		/*
		 * When we fall out of the do loop, the "k" register gives
		 * the status of the operation. A zero means it worked, non-
		 * zero means we failed.
		 */

		return (k);
	}

	/*
	 * We're done with this function; recursively process:
	 *
	 *	Next board OR (if that was the last board)
	 *	Return success!
	 */
	return (((bp = bp->link) ? permute(bp, 0) : 0));
}

void
assign_prog_probe()
{
	Board *bp, *h2 = NULL, *bp2;

	deassign_prog_probe();

	/*
	 * Make a copy of Head_prog chain
	 */
	for (bp = Head_prog; bp; bp = bp->link) {
		bp2 = copy_boards(bp);
		bp2->link = h2;
		h2 = bp2;
	}
	bp = resource_probe(h2); /* assign the resources */

	/*
	 * release all the failures
	 */
	free_chain_boards(&bp);

	/*
	 * Finally program all the devices added to the Head_board
	 */
	for (bp = Head_board; bp; bp = bp->link) {
		if (bp->flags & BRDF_PGM) {
			program_enum(bp);
		}
	}
}

/*
 * Remove programmable devices from the Head Board list
 */
void
deassign_prog_probe()
{
	Board *bp, *nbp;

	for (bp = Head_board; bp; bp = nbp) {
		nbp = bp->link;
		if (bp->flags & BRDF_PGM) {
			unprogram_enum(bp);
			del_board(bp);
		}
	}
}

/*
 * This routine controls the device probe phase of autoconfiguration.
 * It applies the "load" routine (see above) to each realmode driver
 * in the "drivers" list. If "drivers" is NULL, it loads all real-
 * mode drivers in the "solaris\drivers" subdirectory in turn.
 *
 * Upon return, board records corresponding to the devices detected
 * by the probing drivers will be added to the device list.
 * Returns a non-zero value if one or more devices
 * fails to install due to resource conflicts.
 */
int
device_probe(char **drivers, int flags)
{
	DIR *dp;
	int rc;
	struct dirent *dep;
	char *cp, *np;
	char buf[PATH_MAX];

	rc = 0;
	pflg = (flags & DPROBE_PRINT);

	/*
	 * remove any programmable devices
	 */
	deassign_prog_probe();

	if (!cbp) {
		/*
		 * If we don't have a work buffer yet, allocate one now!
		 */
		cbp = new_board();
	}

	if (flags & (DPROBE_ALL|DPROBE_ALWAYS)) {
		/*
		 * Caller wants us to find and load realmode drivers based
		 * on flags argument. This is where the naming conventions
		 * for the driver directories are enforced.
		 *
		 * We do this by building a dummy "drivers" list that
		 * consists of all subdirectories below "solaris/drivers" that
		 * have names of the form:
		 *
		 *		isa.###
		 *
		 * where "###" is a string of decimal digits. When the
		 * DPROBE_ALWAYS bit is set, we loop through the
		 * names "isa.000"-"isa.099" in order. If the
		 * DPROBE_ALL bit is set, we loop though "isa.100"-
		 * "isa.199" in order. The order of driver loading
		 * within the same subdirectory is unspecified.
		 */

		int lower, upper, i;
		struct _stat st;

		if (_stat("solaris/drivers", &st) < 0) {
			/*
			 * Sanity check - we can't do anything if we
			 * can't get the driver directory open!
			 */

			iprintf_tty(gettext("can't open driver directory"));
			return (-1);
		}

		if (flags & DPROBE_ALWAYS) {
			/*
			 * probe-always driver namespace
			 */
			lower = 0;
			upper = UPPER_ISA_ALWAYS_DIR;
		} else {
			/*
			 * probe-all driver namespace
			 */
			lower = 100;
			upper = UPPER_ISA_DIR;
		}

		for (i = lower; i <= upper; i++) {

			/*
			 * Look for driver subdirectory name. If
			 * the opendir fails, it either doesn't exist
			 * or it isn't a directory.
			 */


			(void) sprintf(buf, "solaris/drivers/isa.%3.3d", i);
			if ((dp = opendir(buf)) == 0) {
				continue;
			}

			/*
			 * Found a driver subdirectory. Load all the drivers
			 * now to avoid multiple floppy swaps.
			 */
			while ((dep = readdir(dp)) != 0) {
				if (((np = strrchr(dep->d_name, '.')) != 0) &&
						    (tolower(np[1]) == 'b') &&
						    (tolower(np[2]) == 'e') &&
						    (tolower(np[3]) == 'f') &&
							    (np[4] == '\0')) {
					/*
					 * This file looks like a driver.
					 * Use the "load" routine to
					 * invoke it.
					 */

					(void) sprintf(buf,
					    "solaris/drivers/isa.%3.3d/", i);

					strcat(buf, dep->d_name);
					rc |= load(buf);
				}

			}
			closedir(dp);
		}
	}

	/*
	 * specific list was passed in
	 */
	while ((flags & (DPROBE_ALL|DPROBE_ALWAYS)) == 0 &&
		drivers && (cp = *drivers)) {
		/*
		 * Step thru the driver list and stat each file contained
		 * therein. If it's a directory, load all realmode drivers
		 * contained therein. Otherwise, we assume that the named
		 * file is, itself, a driver.
		 */

		struct _stat st;

		if (_stat(cp, &st) != 0) {
			/*
			 * This shouldn't happen! Caller should have vali-
			 * dated the driver name before calling us!
			 */

			enter_menu(0, "MENU_BEF_MISSING", cp);
			return (-1);

		} else if ((st.st_mode & _S_IFMT) == _S_IFDIR) {
			/*
			 * If this is a subdirectory, open it and look for
			 * drivers contained therein.
			 */

			dp = opendir(cp);
			ASSERT(dp != 0);

			while ((dep = readdir(dp)) != 0) {
				/*
				 * Got another file name out of the driver
				 * subdirectory. If it hase a ".bef" suffix
				 * we assume it's a driver.
				 */

				if (((np = strrchr(dep->d_name, '.')) != 0) &&
						    (tolower(np[1]) == 'b') &&
						    (tolower(np[2]) == 'e') &&
						    (tolower(np[3]) == 'f') &&
							    (np[4] == '\0')) {
					/*
					 * This file looks like a driver.
					 * Use the "load" routine to invoke it.
					 */

					(void) sprintf(buf,
						"%s/%s", cp, dep->d_name);

					rc |= load(buf);
				}
			}

			closedir(dp);

		} else {
			/*
			 * If the next entry in the driver list is not a
			 * subdirectory, assume it's a driver and "load" it.
			 */

			rc |= load(cp);
		}

		drivers++;
	}
	return (rc);
}

/*
 * Find a non-conflicting permutation of resources:
 *
 * This routine searches for a non-conflicting permutation of re-
 * source assignments for the devices chained to the list headed
 * at "bp". Upon return, all successfully configured board records
 * will be moved to the Head_board list with RESF_ALT bits cleared to
 * indicate those resources assigned to each function therein.
 *
 * Boards that cannot be configured due to conflict are removed from
 * the "bp" list and linked into a "failures" list. The head of
 * this list is resource_probe's return code. A NULL return means
 * that all boards in the target list were successfully configured.
 */
Board *
resource_probe(Board *bp)
{
	Board *nbp, *failures = 0;
	CBeflg = -1;

	/*
	 * Caller may pass in an empty list, in which case there's
	 * nothing to do!
	 */
	if ((pgmdevs = bp) != 0) {
		Board *xp;

		CBcflg = 0;
		watchdog = 0;
		pflg = 1;

		/*
		 * Keep permuting resources until we find a non-
		 * conflicting configuration. Each time "permute"
		 * returns a non-zero value, remove the board record
		 * at the high-water mark (i.e, the one causing the
		 * conflict) from the target list.
		 */
		while (bp && permute(bp, 0)) {
			if (bp == hwm) {
				bp = bp->link;
			} else {
				for (xp = bp; xp->link != hwm; xp = xp->link) {
					ASSERT(xp->link);
				}
				xp->link = hwm->link;
			}
			hwm->link = failures;
			failures = hwm;

			/*
			 * tell user what happened to this device.
			 */
			if (!Autoboot && !(CBeptr->flags & BRDF_DISAB)) {
				enter_menu(0, "MENU_PROG_WARN",
				    GetDeviceName_devdb(failures, 1),
				    GetDeviceName_devdb(CBeptr, 1));
				CBeptr->flags |= BRDF_DISAB;
			}
			CBeflg = -1;
		}

		while (bp) {
			nbp = bp->link;
			/* check with ACPI's list */
			if ((bp = acpi_check(bp)) != NULL) {
				/* Tell rest of system about device */
				add_board(bp);
			}
			bp = nbp;
		}
	}
	return (failures);
}

/*
 * Locate primary resource:
 *
 * This routine is used to locate a given boards primary resource.
 * This is the resource (usually an I/O port, but sometimes a memory
 * address) that defines the address where the driver probe for
 * this board.
 *
 * Returns a pointer to the primary resource, or NULL if there
 * doesn't appear to be any (some boards don't have primary resources).
 */

Resource *
primary_probe(Board *bp)
{
	int j;
	Resource *pp = 0;
	Resource *rp = resource_list(bp);
	unsigned mask = RESF_TYPE + RESF_DISABL + RESF_ALT;

	for (j = resource_count(bp); j--; rp++) {
		/*
		 * Scan the resource list looking for the lowest numbered
		 * I/O port (or, failing that, the lowest memory address).
		 * Resources can be listed in any order, so we just have to
		 * keep looking until we've checked everything.
		 */

		switch (rp->flags & mask) {
		/* Process according to resource type */

		case RESF_Mem:
			/*
			 * Memory resource: Although we prefer to identify
			 * devices by I/O port some devices are completely
			 * memory mapped, forcing us to identify them by
			 * memory address instead.
			 */

			if (!pp || (((pp->flags & RESF_TYPE) == RESF_Mem)) &&
						    (rp->base < pp->base)) {
				/*
				 * This is the lowest base memory address
				 * we've seen so far. Save it as possible
				 * identifying resource.
				 */

				pp = rp;
			}
			break;

		case RESF_Port:
			/*
			 * Port resource: Port addresses are the preferred
			 * means of identifying a device.
			 * For eisa we use the 1st port (which is always
			 * the slot space), otherwise we use the
			 * lowest numbered port as the ID address.
			 * XXX Perhaps we should always be returning the
			 * 1st entry as this maps to the unit address?
			 */
			if (bp->bustype & RES_BUS_EISA) {
				return (rp);
			}

			if (!pp || ((pp->flags & RESF_TYPE) != RESF_Port) ||
						    (rp->base < pp->base)) {
				/*
				 * .. And this port's base address is lower
				 * than anything we've seen so far.
				 */

				pp = rp;
			}
			break;
		}
	}

	return (pp);
}

void
handle_conflicts()
{
	/*
	 * We had conflicts during the probe phase and need
	 * to report them now. Give the user as much information
	 * as possible.
	 */

	ConflictList	*clp = CBzlst, *last;
	Board		*bp;
	Resource	*rp;
	int		rc;
	char		fmt[80];

	status_menu("", "MENU_CONFLICTS");

	while (clp) {
		(void) sprintf(fmt, " %s %s ",
			clp->dtp->dname, gettext("has a conflict"));
		catcheye_tty(fmt);
		bp = clp->confbp;
		rp = resource_list(bp);

		for (rc = resource_count(bp); rc--; rp++) {
			if ((clp->restype ==
				(rp->flags & (RESF_TYPE+RESF_ALT))) &&
			    ((clp->base >= rp->base &&
			    (clp->base < (rp->base + rp->length))) ||
			    (rp->base >= clp->base) &&
			    (rp->base < (clp->base + clp->length)))) {
				/*
				 * Found resource match
				 */
				break;
			}
		}
		iprintf_tty("\t%s %s %s: ",
		    ResTypes[clp->restype],
		    gettext("conflict with"),
		    GetDeviceName_devdb(bp, 1));

		switch (clp->restype) {
			case RESF_Irq:
			case RESF_Dma:
				iprintf_tty("%s %ld\n",
				    gettext("both use"), rp->base);
				break;

			case RESF_Port:
			case RESF_Mem:
				iprintf_tty("\n\t\t%s 0x%lx-%lx",
				    gettext("range of conflict"),
				    clp->base, clp->length);
				iprintf_tty(" %s 0x%lx-%lx\n",
				    gettext("and"),
				    rp->base, rp->length);
				break;
		}

		last = clp;
		clp = clp->next;
		free(last);
	}
	CBzlst = (ConflictList *)0;
	iputc_tty('\n');
	option_menu("MENU_HELP_CONFLICTS", Continue_options, NC_options);
}

void
do_all_probe(void)
{
	/*
	 * Throw up the incremental status menu and start
	 * the actual probing.
	 */
	int rc;
	DIR *dp;
	char *np;
	struct dirent *dep;
	int nml = 0;
	int i;
	char buf[PATH_MAX];
	devtrans *dtp;
	struct _stat st;
	extern devtrans **ptp;
	extern unsigned ptx;

	status_menu(Please_wait, "MENU_PROBE_DEVICES");

	lb_init_menu(12);
	lb_info_menu(gettext("Building driver list"));
	/*
	 * First create a list all the plats
	 */
	for (i = 0; i < ptx; i++) {
		dtp = ptp[i];
		(void) sprintf(buf, "solaris/drivers/plat.050/%s.bef",
				dtp->real_driver);
		if (_stat(buf, &st) == 0)	/* bef exists */
			nml++;
	}
	/*
	 * Then all driver names in the solaris/drivers/isa.1*
	 * directories to the list
	 */
	for (i = 100; i <= UPPER_ISA_DIR; i++) {
		/*
		 * Look for driver subdirectory name. If
		 * the opendir fails, it either doesn't exist
		 * or it isn't a directory.
		 */
		(void) sprintf(buf, "solaris/drivers/isa.%3.3d", i);
		if ((dp = opendir(buf)) == 0) {
			continue;
		}

		/*
		 * Found a driver subdirectory.
		 */
		lb_inc_menu();
		while (dep = readdir(dp)) {
			if (((np = strrchr(dep->d_name, '.')) != 0) &&
			    (tolower(np[1]) == 'b') &&
			    (tolower(np[2]) == 'e') &&
			    (tolower(np[3]) == 'f') &&
				    (np[4] == '\0')) {
				/* found a driver */
				*np = 0;
				dtp = TranslateDriver_devdb(dep->d_name);
				if (dtp) {
					nml++;
				}
			}
		}
		closedir(dp);
	}

	lb_scale_menu(nml);
	run_plat();
	rc = device_probe(NULL, DPROBE_ALL | DPROBE_PRINT);
	lb_finish_menu((char *)0);

	if (rc) {
		handle_conflicts();
	}
}

static struct menu_options Probe_options[] = {
	/*  Function key list for the main "probe" screen ...		    */

	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};

#define	NPROBE_OPTIONS (sizeof (Probe_options) / sizeof (*Probe_options))

int
do_selected_probe(void)
{
	DIR *dp;
	struct dirent *dep;
	char *np;
	char buf[PATH_MAX];
	struct menu_list isa_befs[100]; /* 100 isa befs should be enough */
	short drv_dir[100];
	int nml = 0;
	int i, j, k;
	char **drivers;
	devtrans *dtp;
	int done = 0, rc;
	struct _stat st;
	extern devtrans **ptp;
	extern unsigned ptx;

	status_menu(Please_wait, "MENU_CHECKING_DRIVERS");

	/*
	 * First create a list all the plats
	 */
	for (i = 0; i < ptx; i++) {
		dtp = ptp[i];
		(void) sprintf(buf, "solaris/drivers/plat.050/%s.bef",
				dtp->real_driver);
		if (_stat(buf, &st) == 0) {	/* bef exists */
			ASSERT(nml < 100);
			isa_befs[nml].datum = (void *) dtp;
			isa_befs[nml].string = dtp->dname;
			isa_befs[nml].flags = 0;
			drv_dir[nml] = i;
			nml++;
		}
	}

	/*
	 * Then add all driver names in the solaris/drivers/isa.*
	 * directories to the list
	 */
	for (i = 0; i <= UPPER_ISA_DIR; i++) {
		/*
		 * Look for driver subdirectory name. If
		 * the opendir fails, it either doesn't exist
		 * or it isn't a directory.
		 */
		(void) sprintf(buf, "solaris/drivers/isa.%3.3d", i);
		if ((dp = opendir(buf)) == 0) {
			continue;
		}

		/*
		 * Found a driver subdirectory.
		 */
		while (dep = readdir(dp)) {
			if (((np = strrchr(dep->d_name, '.')) != 0) &&
			    (tolower(np[1]) == 'b') &&
			    (tolower(np[2]) == 'e') &&
			    (tolower(np[3]) == 'f') &&
				    (np[4] == '\0')) {
				/* found a driver */
				*np = 0;
				dtp = TranslateDriver_devdb(dep->d_name);
				if (dtp) {
					ASSERT(nml < 100);
					isa_befs[nml].datum = (void *) dtp;
					isa_befs[nml].string = dtp->dname;
					isa_befs[nml].flags = 0;
					drv_dir[nml] = i;
					nml++;
				}
			}
		}
		closedir(dp);
	}

	reset_plat_props();

	/*
	 * Now present the menu of drivers to the user
	 */
	while (!done) {
		switch (select_menu("MENU_HELP_PROBE", Probe_options,
		    NPROBE_OPTIONS, isa_befs, nml, MS_ANY, "MENU_PROBE")) {

		case FKEY(2):
			for (j = k = 0; j < nml; j++) {
				/*
				 * Count the total number of devices selected
				 * for probing. Final count goes to the "k"
				 * register.
				 */

				if (isa_befs[j].flags & MF_SELECTED) k++;
			}

			if (k == 0) {
				/* None selected */
				beep_tty();
			} else {
				done = 1;
			}
			break;
		case FKEY(3):
			return (0);	/* return 0 if cancel */
		}
	}

	/*
	 * Create the list of drivers
	 */
	drivers = (char **)malloc((k + 1) * sizeof (char *));
	drivers[k] = NULL; /* Null terminate list of strings */
	for (i = 0, j = 0; j < k; i++) {
		if (isa_befs[i].flags & MF_SELECTED) {
			dtp = (devtrans *) isa_befs[i].datum;
			drivers[j] = malloc(PATH_MAX + 32);
			if (dtp->category == DCAT_PLAT)
				(void) sprintf(drivers[j],
					"solaris/drivers/plat.050/%s.bef",
					dtp->real_driver);
			else
				(void) sprintf(drivers[j],
					"solaris/drivers/isa.%3.3d/%s.bef",
					drv_dir[i], dtp->real_driver);
			j++;
		}
	}

	/*
	 * Load the drivers
	 */
	status_menu(Please_wait, "MENU_PROBE_DEVICES");
	run_enum(ENUM_TOP);

	lb_init_menu(12);
	lb_scale_menu(k);
	rc = device_probe(drivers, DPROBE_PRINT | DPROBE_LIST);
	lb_finish_menu((char *)0);

	run_enum(ENUM_BOT);

	if (rc)
		handle_conflicts();

	/*
	 * Finally released any malloc'ed memory
	 */
	for (i = 0; i < k; i++) {
		free(drivers[i]);
	}
	free(drivers);
	return (1);	/* normal return */
}

/*
 * Remove devices that were previously found by probing with this bef.
 */
void
remove_devs_probe(char *bef)
{
	Board		*bp, *nbp;
	int		real_len, bef_len;
	devtrans	*dtp;

	for (bp = Head_board; bp; bp = nbp) {
		nbp = bp->link;
		if (!(bp->slotflags & RES_SLOT_PROBE)) {
			continue;
		}

		dtp = bp->dbentryp;
		if (dtp && dtp->real_driver) {
			real_len = strlen(dtp->real_driver);
			ASSERT(strchr(bef, '.'));
			bef_len = strchr(bef, '.') - bef;
			/*
			 * Need to compare the length of real_driver
			 * which doesn't have the .bef extension with
			 * the length of 'bef' without it's extension.
			 * If they're not the same don't bother doing a
			 * strnicmp(). Example:
			 * dtp->real_driver	== "el"
			 * bef			== "elink.bef"
			 * Using just the string length from real_driver
			 * you'd find a match, first two characters, and
			 * flush out the "el" board record when you shouldn't
			 */
			if ((bef_len == real_len) &&
			    !_strnicmp(dtp->real_driver, bef, bef_len)) {
				del_board(bp);
				Update_escd = 1;
			}
		}
	}
}

static void
run_plat()
{
	int i, j;
	char buf[PATH_MAX * 2];
	char **drivers;
	devtrans *tp;
	struct _stat st;
	extern devtrans **ptp;
	extern unsigned ptx;

	if (ptx == 0)
		return;
	/*
	 * Create the list of drivers
	 */
	drivers = (char **)malloc((ptx + 1) * sizeof (char *));
	for (i = 0, j = 0; i < ptx; i++) {
		tp = ptp[i];
		(void) sprintf(buf, "solaris/drivers/plat.050/%s.bef",
				tp->real_driver);
		if (_stat(buf, &st) == 0) {
			drivers[j] = malloc(strlen(buf) + 1);
			strcpy(drivers[j], buf);
			j++;
		}
	}

	if (j == 0) {	/* nothing in solaris/drivers/plat */
		free(drivers);
		return;
	}

	drivers[j] = NULL; /* Null terminate list of strings */

	(void) device_probe(drivers, DPROBE_PRINT | DPROBE_LIST);

	/*
	 * Finally released any malloc'ed memory
	 */
	for (i = 0; i < j; i++) {
		free(drivers[i]);
	}
	free(drivers);
}
