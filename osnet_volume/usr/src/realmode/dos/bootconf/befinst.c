/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * befinst.c -- handles installation of befs and the bef callback
 */

#ident "@(#)befinst.c   1.69   99/10/07 SMI"

#include "types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <befext.h>
#include <ctype.h>
#include <biosmap.h>
#include <dos.h>

#include "befinst.h"
#include "menu.h"
#include "boot.h"
#include "devdb.h"
#include "debug.h"
#include "enum.h"
#include "err.h"
#include "gettext.h"
#include "menu.h"
#include "prop.h"
#include "tty.h"
#include "bop.h"

static Board *BefDev;	/* current bef device */
static u_char first_dev;

static int resource_befinst(int op, char *name, u_long *buf, u_long *len);
static int prop_befinst(int op, char *name, char **value, int *len, int bin);
static int node_befinst(int op);
static int bios_mem_save;
static void(cdecl interrupt far * cdecl int_13_save)();
static void(cdecl interrupt far * cdecl int_fb_save)();
void(cdecl interrupt far * cdecl int_13_initial)();

#define	BIOS_MEM_SIZE bdap->RealMemSize

/*
 * BEF interface callback structure:
 *
 * Each driver (".bef" file) gets a pointer to this structure when
 * it is invoked at its BEF_INSTALLONLY entry point. The driver can
 * then use it to do resource allocation and to get the next device.
 */
static struct bef_interface bif =
{
	BEF_IF_VERS,		/* Interface version number */
	resource_befinst,	/* Ptr to resource allocation routine */
	node_befinst,		/* Ptr to device node allcoation routine */
	prop_befinst,		/* Property node handler */
	bef_print_tty,		/* Driver putc callback routine */
	mem_adjust,		/* Driver memory re-size routine */
	0,			/* Driver base paragraph address */
	0,			/* Driver size in paragraphs */
};

void
init_befinst()
{
	int_13_initial = _dos_getvect(0x13);
}

int
install_bef_befinst(char *bef_name, Board *bp, int bios_primary)
{
	int devid = 0;
	char *fp;
	char *path;
	char filename[13]; /* devtrans.real_driver + 4 for ".bef" */
	extern void free_biosprim_buf_befext(void);

	/*
	 * add ".bef" to driver name
	 */
	(void) strcpy(filename, bef_name);
	(void) strcat(filename, ".bef");
	debug(D_FLOW, "installing driver %s\n", filename);

	if (!Autoboot) {
		char buf[24];

		(void) sprintf(buf, gettext("Driver %s"), filename);
		status_menu(Please_wait, "MENU_LOADING", buf);
	}
	bios_mem_save = BIOS_MEM_SIZE;
	int_13_save = _dos_getvect(0x13);
	int_fb_save = _dos_getvect(0xfb);
	if (!(path = GetBefPath_befext(filename))) {
		/* We seem to have lost the driver!			*/
		enter_menu(0, "MENU_BEF_MISSING", filename);

	} else if (!(fp = LoadBef_befext(path))) {
		/* We've got a driver, but it doesn't load!		*/
		enter_menu(0, "MENU_BAD_BEF_LOAD", path);
	} else {
		BefDev = bp;
		first_dev = 1;
		if (bios_primary) {
			_dos_setvect(0x13, int_13_initial);
			free_biosprim_buf_befext();
			BIOS_MEM_SIZE = bios_mem_save;
		}

		if (*(long *)(fp + BEF_EXTMAGOFF) == (long)BEF_EXTMAGIC) {
			/*
			 * New style driver.
			 * Call the install-only entry point.
			 */
			debug(D_FLOW, "calling driver %s at %x:%x\n",
				filename, (ushort)((long)fp >> 16),
				(ushort)((long)fp & 0xFFFF));
			devid = CallBef_befext(BEF_INSTALLONLY,
			    (struct bef_interface *)&bif);
		} else {
			enter_menu(0, "MENU_BAD_BEF_LOAD", path);
		}
	}

	debug(D_FLOW, "return from driver: %s, devid 0x%x\n", filename, devid);
	return (devid);
}

/*
 * De-install the last installed bef unless it is the BIOS primary.
 */
void
deinstall_bef_befinst()
{
	_dos_setvect(0x13, int_13_save);
	_dos_setvect(0xfb, int_fb_save);
	FreeBef_befext(); /* Free the bef buffer */
	BIOS_MEM_SIZE = bios_mem_save;
}

/*
 * Resource allocation callback:
 *
 * This is the "resource" callback for BEF installation.
 * It implements the RES_GET command, which is used to obtain
 * the configuration resources on behalf of a given device.
 * It also implements enough of the RES_SET command to allow
 * drivers to specify which device functions they're intrested in.
 * The "nam" argument specifies the type of resource the driver
 * is trying to get, and the "buf" and "len" arguments are used
 * to return the resources.
 *
 * Extraction of resources is done on the current device
 * held in the global BefDev.
 */
static int
resource_befinst(int op, char *name, u_long *buf, u_long *len)
{
	int t, x;
	u_short j;
	Resource *rp;
	long sz = (long)*len;
	rp = resource_list(BefDev);

	if (op != RES_GET) {
		return (RES_FAIL);
	}

	/*
	 *  Get resources currently assigned to this device/function.
	 *  We have to process these by function type ...
	 */

	switch (tolower(*name)) {

	case 'n':
		if (sz > 0) {
			/*
			 *  Give user the device name (and optional bus type).
			 */
			*len = 1;
			buf[0] = BefDev->devid;
			if (sz > 1) {
				buf[1] = BefDev->bustype;
				*len = 2;
			}
			return (RES_OK);
		}
		break;

	case 's':
		if (sz > 0) {
			/*
			 *  Slot number.  Return device's slot number
			 *  and optional slot flags.
			 */

			*len = 1;
			buf[0] = BefDev->slot;
			if (sz > 1) {
				buf[1] = BefDev->slotflags;
				*len = 2;
			}
			return (RES_OK);
		}

		break;

	case 'a':
		if ((sz > 0) && (BefDev->bustype == RES_BUS_PCI)) {
			/*
			 * Return the pci style bus address:-
			 * <8 bit bus no> <5 bit dev> <3 bit func>
			 */
			*len = 1;
			buf[0] = (BefDev->pci_busno << 8) |
			    BefDev->pci_devfunc;
			return (RES_OK);
		}

		break;


	case 'p': t = RESF_Port; x = 1; goto sch;
	case 'm': t = RESF_Mem;  x = 1; goto sch;
	case 'i': t = RESF_Irq;  x = 0; goto sch;
	case 'd': t = RESF_Dma;  x = 0;

sch:
		rp = resource_list(BefDev);
		for (*len = 0, j = resource_count(BefDev); j--; rp++) {
			/*
			 *  All other resource types must be extracted
			 *  from the current function's resource list.
			 */

			if ((t == (rp->flags &
			    (RESF_TYPE + RESF_ALT + RESF_DISABL))) &&
			    ((sz -= (x+2)) >= 0)) {
				/*
				 *  Another resource of the proper type.
				 *  Copy resource info to caller's buf.
				 */
				*len += (x+2);
				*buf++ = rp->base;
				if (x) {
					*buf++ = rp->length;
				}
				*buf++ = rp->EISAflags;
			}
		}
		return (RES_OK);
	}
	return (RES_FAIL);
}

/*
 * Device node callback:
 *
 * This routine serves as the device node callback for realmode drivers
 * in the installation phase. The driver uses this routine to get the
 * next device's configuration. The only "op" code supported for
 * installation is NODE_START.
 */
static int
node_befinst(int op)
{
	devtrans *dtp, *ndtp;
	Board *next;

	switch (op) {
	case NODE_DONE:
		return (NODE_OK);
	case NODE_START:
		break;
	default:
		debug(D_ERR, "node_befinst: unrecognised op %d\n", op);
		return (NODE_FAIL);
	}
	if (first_dev) {
		first_dev = 0;
		debug(D_FLOW, "node_befinst: returning 1st device\n");
		return (NODE_OK);
	}
	/*
	 * If we aren't dealing with a network , then
	 * move to the next device (Board) maps to the same bef
	 */
	dtp = BefDev->dbentryp;
	if (dtp->category == DCAT_NET) {
		return (NODE_FAIL);
	}

	/*
	 * Get next non channel board
	 */
	for (next = BefDev->beflink; next && (next->flags & BRDF_CHAN);
	    next = next->beflink) {
		;
	}
	if (next) {
		ndtp = next->dbentryp;
		if (strcmp(ndtp->real_driver, dtp->real_driver) == 0) {
			debug(D_FLOW, "node_befinst: found another device\n");
			BefDev = next;
			return (NODE_OK);
		}
	}
	BefDev = next;
	debug(D_FLOW, "node_befinst: no more devices\n");
	return (NODE_FAIL);
}

/*
 * Property operations callback
 *
 * This is the "properties" callback for BEF installonly calls.
 * We support setting either character or binary properties.
 *
 * Returns PROP_OK if it works, PROP_FAIL otherwise.
 */
static int
prop_befinst(int op, char *name, char **value, int *len, int bin)
{
	char buf[32]; /* max property name len is 31 */
	char *tp;

	if (BefDev == NULL)
		return (PROP_FAIL);
	switch (op & 0xFF) {
		case PROP_SET:
			return (SetDevProp_devdb(&BefDev->prop, name, *value,
			    *len, bin));
		case PROP_GET:
			*value = NULL;
			*len = 0;
			if (GetDevProp_devdb(&BefDev->prop, name, value, len) <
			    0) {
				char *tp;

				/*
				 * Try options node for this prop
				 */
				(void) strcpy(buf, name);
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
			(void) strcpy(buf, name);
			tp = read_prop(buf, "");	/* root node */
			*value = tp;
			if (*value == NULL)
				return (PROP_OK);
			*len = strlen(*value) + 1;
			return (PROP_OK);
	}

	return (PROP_FAIL);
}
