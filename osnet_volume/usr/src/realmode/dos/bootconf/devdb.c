/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * devdb.c -- routines to access the device database
 */

#ident	"@(#)devdb.c	1.100	99/11/08 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <names.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

#include "types.h"
#include "boards.h"
#include "boot.h"
#include "bus.h"
#include "debug.h"
#include "devdb.h"
#include "dir.h"
#include "err.h"
#include "gettext.h"
#include "main.h"
#include "menu.h"
#include "pci1275.h"
#include "spmalloc.h"
#include "tree.h"
#include "tty.h"
#include "ur.h"
#include "probe.h"

static char NoSeq = 0;		/* Don't sequence drivers flag. */
static char DoCpy = 0;		/* Copy fields of "master" records. */

static devtrans **ttp;		/* The translation table proper	*/
static unsigned ttx = 0;	/* Number of active entries */

static devtrans **dtp;		/* Device translation index table */
static unsigned dtx = 0;	/* Entries in "bef" table. */

devtrans **ptp;			/* The "plat" index table */
unsigned ptx = 0;		/* Number of "plat" befs */

#define	WARN_IF_NOT(EX) if (!(EX)) warn_devdb(#EX, __FILE__, __LINE__)
static int line_devdb = 0;	/* current line number of db file */

static char *master_entry_buf;

#ifndef max			/* Microsoft defines them, Sun doesn't!	*/
#define	max(x, y) (((x) > (y)) ? (x) : (y))
#define	min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define	varoff ((int)&((Board *)0)->var)
#define	DB_FILE	0
#define	DB_ITU 1

/*
 * Resource type names:
 *
 * These are used primarily in error messages and such. The
 * positions of the primary resources ("port", "irq", "dma", and
 * "memory") must match the corresponding RESF_<type> values.
 */
char *ResTypes[] = {
	"slot", "Port", "IRQ", "DMA", "Memory", "name"
};

/*
 * Resource tuple lengths.
 *
 * This table gives the number of DWORDs in a callback arg list for
 * a single resource of each of the types listed above.
 */
unsigned char ResTuples[] = {
	SlotTupleSize, PortTupleSize, IrqTupleSize, DmaTupleSize,
	MemTupleSize, NameTupleSize
};

/*
 * Table of device type names:
 *
 * We actually encode two names in each entry, separating them with
 * a null character. The first name is the "external" name that
 * appears on displays, the second is the "internal" name for the
 * device class from the Solaris realmode device database.
 */
const char *DevTypes[] = {
	"serial devices (e.g, modems)      \0"		"com",
	"keyboards                         \0"		"key",
	"memory extender cards             \0"		"mem",
	"multifunction devices             \0"		"mfc",
	"storage devices (& controllers)   \0"		"msd",
	"network adapters                  \0"		"net",
	"platform                          \0"		"plat",
	"miscellaneous devices             \0"		"oth",
	"parallel devices (e.g, printers)  \0"		"par",
	"pointing devices (e.g, mice)      \0"		"ptr",
	"video displays                    \0"		"vid",
};

/*
 * Table of bus type names: This table is used in a manner similar
 * to the device type table, above. The big difference is that the
 * first character of each string is the ESCD bus type code rather
 * than a descriptive name.
 */
static const char *BusTypes[] = {
	"\001\0"	"isa",		/* ISA (the most common)	    */
	"\002\0"	"eisa",		/* EISA (treated much like ISA)	    */
	"\003\0"	"pciclass",	/* PCI class			    */
	"\004\0"	"pci",		/* PCI devices are autoconfigurable */
	"\010\0"	"pcmcia",	/* So are PCMCIA (duh!)		    */
	"\020\0"	"pnpisa",	/* And so are PnP ISA cards.	    */
	"\201\0"	"vlb",		/* VLB is treated like ISA	    */
};

const int DevTypeCount = sizeof (DevTypes)/sizeof (DevTypes[0]);
#define	BusTypeCount (sizeof (BusTypes)/sizeof (BusTypes[0]))

/*
 * Function prototypes
 */
static int CmpDrv(const void *p, const void *q);
static int CmpDev(const void *p, const void *q);
static int CmpNam(const void *p, const void *q);
static devtrans * translate(devtrans *target, devtrans **indx);
static char *get_drvr(char *cp, char *buf, int len);
static char *get_code(char *cp, devtrans *trp, const char **tab);
static char *get_text(char *cp, char **cfp);
static devprop *parse_prop(char *cp);
static devtrans *db_read(int where, void *vp);
static void sort_tables();
static char *get_next_line(int where, void *vp);
static void db_rewind(FILE *);
static void warn_devdb(char *statement, char *code_name, int code_line);

/*
 * Driver Sort Comparison:
 *
 * After we've built the driver name translation table, we sort it
 * to make searching a bit faster. This is the qsort comparison
 * routine that orders the entries by driver name.
 *
 * NOTE: Multiple devices may be served by the same driver. In this
 *	 case, the first device to appear in the database is the one
 *	 we associate with the driver for error message/status info.
 */
static int
CmpDrv(const void *p, const void *q)
{
	devtrans *tp = *(devtrans **)p;
	devtrans *tq = *(devtrans **)q;
	int x = strcmp(tp->real_driver, tq->real_driver);

	return ((x || NoSeq) ? x : ((tp->seq < tq->seq) ? -1 : 1));
}

/*
 * Device Sort Comparison:
 *
 * We also build a table of pointers into the "devtrans" records
 * which is used to search for drivers given device IDs. This is
 * the qsort comparison for the secondary index sort.
 */
static int
CmpDev(const void *p, const void *q)
{
	devtrans *tp = *(devtrans **)p;
	devtrans *tq = *(devtrans **)q;

	if (tp->devid < tq->devid) {
		return (-1);
	}
	if (tp->devid > tq->devid) {
		return (1);
	}

	/*
	 * ids are equal - now check the bus type
	 */
	if ((tp->bustype == tq->bustype) || !tp->bustype || !tq->bustype) {
		return (0);
	}
	if (tp->bustype < tq->bustype) {
		return (-1);
	}
	return (1);
}

/*
 * This routine is used (by "qsort") to sort a list of "devtrans *" by name.
 */
static int
CmpNam(const void *p, const void *q)
{
	devtrans *dtp1 = (devtrans *)(((struct menu_list *)p)->datum);
	devtrans *dtp2 = (devtrans *)(((struct menu_list *)q)->datum);

	return (strcmp(dtp1->dname, dtp2->dname));
}

/*
 * Lookup a device translation:
 *
 * This routine performs a binary search of the given translation
 * index, looking for an entry that matches the target description
 * (typically, a dummy "trans" structure that contains just enough
 * data to satisfy the qsort comparison routines).
 * It returns a pointer to the matching translation structure, or
 * NULL if there is none.
 */
static devtrans *
translate(devtrans *target, devtrans **indx)
{
	int j;
	devtrans **lo = indx;
	devtrans **hi = indx + ((indx == ttp) ? ttx : dtx);
	int (*cmp)(const void *, const void *) = ((indx == ttp)?CmpDrv:CmpDev);

	while ((j = (hi - lo)) > 0) {
		devtrans **next = lo + (j >> 1);
		j = (*cmp)((const void *)&target, (const void *)next);

		if (j > 0) lo = next + 1;
		else if (j < 0) hi = next;
		else return (*next);
	}

	return (0);
}

/*
 * Get driver name from master file:
 *
 * This routine extracts a device driver name from the next field
 * of the current master file record (as indicated by "cp") and
 * stores in the buffer supplied.  The "len" argument gives the
 * size of the output buffer.
 *
 * Returns a pointer to the first byte following the driver name
 * in the input buffer.
 */
static char *
get_drvr(char *cp, char *outbuf, int len)
{
	char *xp;

	while (*cp && isspace(*cp)) {
		cp++;
	}

	xp = outbuf;

	/*
	 * Scan to the end of the driver name, copying into the
	 * devtrans structure as we go.  Stop at the end of the
	 * string, at white space, at a dot, or when the output buffer
	 * fills.
	 */

	while (*cp != '\0' &&
	    !isspace(*cp) &&
	    *cp != '.' &&
	    xp < outbuf + len - 1) {
		*xp++ = *cp++;
	}

	/*
	 * Skip to end or whitespace, skipping over anything after a '.'
	 * or left over after the output buffer filled.
	 */
	while (*cp && !isspace(*cp))
		cp++;

	*xp = '\0';

	/*
	 * If the driver name is "none", we nullify the string that
	 * we just put into the output buffer!
	 */
	if (strcmp(outbuf, "none") == 0)
		memset(outbuf, 0, len);

	return (cp);
}

/*
 * Encode ASCII device/bus type:
 *
 * This routine converts the ASCII bus/device type at "cp" to the
 * binary encoding used by bootconf, and stores the result in the
 * appropriate field ("category"/"bustype") of the devtrans record
 * at "trp". The "tab" argument points to a look-up table that
 * tells us which datum we're encoding.
 *
 * Returns a pointer to the first byte beyond the ASCII string that
 * we're encoding.
 */
static char *
get_code(char *cp, devtrans *trp, const char **tab)
{
	char *xp;
	int x = (tab == DevTypes);
	int n;

	if (x) {
		n = DevTypeCount;
	} else {
		n = BusTypeCount;
	}
	while (*cp && isspace(*cp)) {
		cp++;
	}
	for (xp = cp; *xp && !isspace(*xp); xp++) {
		*xp = tolower(*xp);
	}
	if (*xp) {
		*xp++ = 0;
	}

	while (n-- > 0) {
		/*
		 * Search the lookup table for an entry whose name matches
		 * the target (which we just converted to lower case JIC).
		 */

		if (strcmp(cp, strchr(tab[n], 0) + 1) == 0) {
			/*
			 * Matched! Store encoded value and exit. Note
			 * that the value we match against is concatenated
			 * behind the base string in the lookup table.
			 */
			if (x) {
				trp->category = n + 1;
			} else {
				trp->bustype = *tab[n];
			}
			break;
		}
	}

	return (xp);
}

/*
 * Get text string from master record:
 *
 * This routine extracts a string of ASCII text from the device
 * database master record at "cp" and stores a pointer to said
 * string at "*cfp". We remove any enclosing quotes and plant a
 * null byte at the end of the extracted text.
 *
 * Returns a pointer to the first byte beyond the extracted text
 * in the input buffer.
 */
static char *
get_text(char *cp, char **cfp)
{
	int qt;
	char *xp;

	while (*cp && isspace(*cp)) {
		cp++;
	}
	if ((qt = (*cp == '"')) != 0) {
		cp++;
	}
	xp = cp;

	while (*cp && (qt ? (*cp != '"') : !isspace(*cp))) {
		cp++;
	}
	*cfp = ((cp > xp) ? xp : 0);
	if (*cp) {
		*cp++ = 0;
	}
	return (cp);
}

/*
 * Build a property list:
 *
 * This routine extracts "<name>=<value>" pairs from the text string
 * at "cp", builds a corresponding linked list of "devprop" structs,
 * and resturns the head of this list to the caller.
 */
static devprop *
parse_prop(char *cp)
{
	devprop *dpp = 0;

	while (cp && *cp) {
		/*
		 * Keep going until we get to the end of the input string.
		 * Each iteration of this loop adds another "devprop" struct
		 * to the property list.
		 *
		 * Multiple properties must be separated in the input string
		 * by whitespace. That's whitespace, not commas! This is
		 * to allow us to create property values with commas in them.
		 */
		int j, x;
		devprop *dxp;
		char *np, *vp = 0;
		while (*cp && isspace(*cp)) cp++;

		if (*(np = cp)) {
			/*
			 * The "np" register now points to the <name> part
			 * of a <name>=<value> pair. Scan up to the equal
			 * sign and terminate the name string with a null
			 * byte.
			 *
			 * Note that the "=<value>" clause is optional. If
			 * it's omitted, we create a property node with a
			 * name but no corresponding value.
			 */
			while (*cp && !isspace(*cp) && (*cp != '=')) {
				cp++;
			}
			x = (*cp == '=');
			if (*cp) {
				*cp++ = 0;
			}

			while (*cp && isspace(*cp)) {
				cp++;
			}
			if (*cp == '=') {
				x = *cp++;
			}

			if (x != 0) {
				/*
				 * We found an equal sign! Advance the "vp"
				 * register to the start of the corresponding
				 * <value> field (and make sure it's null
				 * terminated).
				 */
				while (*cp && isspace(*cp)) {
					cp++;
				}
				for (vp = cp; *cp && !isspace(*cp); cp++) {
					;
				}
				if (*cp) {
					*cp++ = 0;
				}
			}

			x = strlen(np) + 1;
			j = (vp && *vp) ? (strlen(vp)+1) : 0;
			dxp = (devprop *)calloc(1, sizeof (devprop) + x + j);

			if (!dxp) {
				MemFailure();
			}
			strcpy(dxp->name, np);
			dxp->next = dpp;
			dpp = dxp;

			if ((dxp->vof = (j ? x : 0)) != 0) {
				/*
				 * Node has a <value> field. Concatenate
				 * it behind the name string.
				 */

				strcpy(&dxp->name[x], vp);
			}
		}
	}

	return (dpp);
}

/*
 * Read next master record:
 *
 * This routine reads the next line of the Solaris realmode database
 * file ("devicedb\master"), and partially converts it to "devtrans"
 * format. The only part that isn't converted is the optional
 * property list. Returns a pointer to the "devtrans" record that
 * corresponds to the line of the master file that we processed, or
 * NULL to indicate EOF.
 *
 * The reason we don't fully convert the property list is that we
 * don't want to allocate dynamic memory at this time. All data
 * delivered by this routine resides in static buffers!
 */
static devtrans *
db_read(int where, void *vp)
{
	int x = 1;
	char *cp, *xp;
	static devtrans dt;
	static char *more = 0;

	if ((cp = more) != 0) {
		/*
		 * The master record we converted on the previous invocation
		 * describes multiple device types, and "cp" points to the
		 * next associated device ID. Check to see if this is the
		 * last ID in the list and nullify the "more" flag if so.
		 */
		x = 0;
		if (more = strchr(cp, '|')) {
			*more++ = 0;
		}

	} else {
		/*
		 * We're done with the previous record; read lines from the
		 * master file until we find another one!
		 */
next:
		memset(&dt, 0, sizeof (devtrans));

		/*
		 * Keep reading lines out of the master file until
		 * we reach the end-of-file. Ignore comment lines,
		 * the "version" line, and all leading whitespace!
		 */

		while (cp = get_next_line(where, vp)) {
			/*
			 * Increment for file syntax error reporting
			 */
			line_devdb++;

			while (*cp && isspace(*cp)) {
				cp++;
			}

			if (*(xp = cp) && (*cp != '#') &&
			    strncmp(cp, "version", sizeof ("version")-1)) {
				/*
				 * Found another live entry; "cp" points to
				 * the first device ID on this line. Advance
				 * "xp" to point beyond the device ID field
				 * so that we can pick up other stuff.
				 */

				while (*xp && !isspace(*xp)) {
					/*
					 * Find the end of the device ID, but
					 * watch for alternators as we go.
					 */
					if ((*xp++ == '|') && !more) {
						/*
						 * There are multiple IDs for
						 * this device. Save the
						 * address of the next one.
						 */

						more = xp;
						xp[-1] = 0;
					}
				}

				*xp++ = 0; /* Terminate the device ID field */

				xp = get_drvr(xp, dt.unix_driver,
					sizeof (dt.unix_driver));
				xp = get_code(xp, &dt, DevTypes);
				xp = get_code(xp, &dt, BusTypes);
				xp = get_drvr(xp, dt.real_driver,
					sizeof (dt.real_driver));
				xp = get_text(xp, &dt.dname);
				dt.proplist = (devprop *) xp;
				break;
			}
		}
	}

	if (cp == 0) {
		/*
		 * Return a null pointer to indicate that we've reached the
		 * end of the master file.
		 */
		return (0);

	} else {
		/*
		 * Encode the device ID according to bus type ...
		 */
		switch (dt.bustype) {

		case RES_BUS_PCI:
			/*
			 * PCI devices: Vendor ID in high order 16 bits,
			 * device ID code in low order 16 bits.
			 */
			WARN_IF_NOT(more == 0);
			if (more != 0) {
				goto next;
			}
			WARN_IF_NOT(strncmp(cp, "pci", 3) == 0);
			if (strncmp(cp, "pci", 3) != 0) {
				goto next;
			}
			dt.devid = (strtol(cp+3, &cp, 16) << 16);
			if (*cp == ',') {
				dt.devid |= strtol(cp+1, &cp, 16);
			}
			break;

		case RES_BUS_PCICLASS:
			/*
			 * PCI class  eg pciclass,010100
			 */
			WARN_IF_NOT(strncmp(cp, "pciclass,", 9) == 0);
			if (strncmp(cp, "pciclass,", 9) != 0) {
				goto next;
			}
			dt.devid = strtol(cp+9, &cp, 16);
			break;

		default:
			/*
			 * Everything else uses the EISA device ID encoding.
			 */
			dt.devid = CompressName(cp);
			cp += 7;
			break;
		}

		WARN_IF_NOT(*cp == 0);
		if (*cp != 0) {
			goto next;
		}
	}

	if (x && DoCpy) {
		/*
		 * On the second pass over the file we have to make
		 * permanent copies of the device name and the property list.
		 * We only do this once per master file line, tho: Not
		 * once per translation.
		 */

		if (dt.dname) {
			dt.dname = strdup(dt.dname);
		}
		dt.proplist = parse_prop((char *)dt.proplist);
	}

	return (&dt);
}

/*
 * Add a resource to the given function
 */
Board *
AddResource_devdb(Board *bp1, unsigned short type, long start, long len)
{
	unsigned short flags;
	Board *bp;
	Resource *rp;
	int i;

	flags = type & ~RESF_TYPE;
	type &= RESF_TYPE;
	bp = ResizeBoard_devdb(bp1, sizeof (Resource));

	if (bp->resoff == 0) {
		/*
		 * This is the first resource of the function.
		 * Mark it as such and save the list offset
		 * in the function record.
		 */
		bp->resoff = bp->reclen;
		rp = (Resource *)((char *)bp + bp->reclen);
		rp->flags = (RESF_FIRST | RESF_SUBFN);
	} else {
		/*
		 * Add the new resource next to the same type.
		 */
		rp = (Resource *)((char *)bp + bp->reclen) - 1;
		for (i = resource_count(bp); i; i--, rp--) {
			if ((rp->flags & RESF_TYPE) == type) {
				if (i == resource_count(bp)) {
					rp->flags |= RESF_MULTI;
					rp[1].flags &= RESF_MULTI;
				}
				break;
			}
			*(rp + 1) = *rp; /* copy record up */
			rp->flags = RESF_MULTI;
		}
		rp++;
		if (i == 0) {
			rp->flags = (RESF_FIRST | RESF_SUBFN | RESF_MULTI);
			rp[1].flags &= ~(RESF_FIRST | RESF_SUBFN);
		}
	}
	bp->reclen += sizeof (Resource);
	/*
	 * Some enumerators/drivers are confused about the cascaded
	 * irq. Convert any requests for irq 2 to irq 9, except
	 * for the motherboard which really does reserve the cascaded irq.
	 */
	if ((type == RESF_Irq) && (start == 2) && (bp->devid !=
		CompressName("SUNFFE2"))) {
			start = 9;
	}
	rp->base = (u_long) start;
	rp->length = (u_long) len;
	rp->flags |= type | flags;
	bp->rescnt[type-1]++;

	/*
	 * Save, if appropriate, the lowest device memory address greater
	 * or equal to 16MB.
	 */
	if ((type == RESF_Mem) && ((u_long) start >= 0x1000000UL)) {
		if (low_dev_memaddr == 0) {
			low_dev_memaddr = (u_long) start;
		} else {
			low_dev_memaddr = min(low_dev_memaddr, (u_long) start);
		}
	}

	return (bp);
}

/*
 * Delete the named resource from the function.
 * assumes only one function in a board
 *
 * Returns 1 when last resource in Board is removed
 */
int
DelResource_devdb(Board *bp, Resource *rp)
{
	Resource *erp;
	int nr = resource_count(bp);
	int i;

	bp->rescnt[(rp->flags & RESF_TYPE) - 1]--;

	/*
	 * Fix up the flags of the previous and next rp
	 */
	if ((rp != resource_list(bp)) && ((rp - 1)->flags & RESF_MULTI) &&
	    ((rp->flags & RESF_MULTI) == 0)) {
		(rp - 1)->flags &= ~RESF_MULTI;
	}
	if ((rp->flags & RESF_MULTI) && (rp->flags & RESF_FIRST)) {
		(rp + 1)->flags |= RESF_FIRST;
	}

	/*
	 * Copy the resources up
	 */
	erp = resource_list(bp) + nr - 1;
	for (i = (erp - rp); i; i--, rp++) {
		*rp = *(rp + 1);
	}

	bp->reclen -= sizeof (Resource);
	if (nr == 1) { /* last resource */
		bp->resoff = 0;
		return (1);
	}
	return (0);
}

/*
 * Resize a board record:
 *
 * This routine may be used to verify that a board record is large
 * enough to hold a "len"-byte data structure. If so, we return the
 * records address. If not, we reallocate the record into a bigger
 * buffer and return the new address.
 *
 * Panics if we run out of memory.
 */
Board *
ResizeBoard_devdb(Board *bp, unsigned len)
{
	ASSERT(bp != 0);

	/*
	 * Keep doubling the size of the record buffer until it's
	 * big enougn to hold "len" more bytes of data.
	 */
	while (len >= (bp->buflen - bp->reclen)) {
		unsigned n = bp->buflen << 1;

		ASSERT((bp->buflen & 0x8000) == 0);
		bp = (Board *) realloc(bp, n);
		bp->buflen = n;
	}

	return (bp);
}

/*
 * Reset a board record buffer:
 *
 * This routine resets the board record at "bp", marking its current
 * record size equal to zero and clearing the header. If the buffer
 * "len"gth argument is non-zero, we set the buffer length to this
 * value; otherwise the buffer length remains unchanged.
 */
void
ResetBoard_devdb(Board *bp)
{
	u_short len = bp->buflen;
	if (bp->prop) {
		devprop *pp, *npp;

		for (pp = bp->prop; pp; pp = npp) {
			npp = pp->next;
			free(pp);
		}
	}
	memset((void *)bp, 0, varoff);

	bp->reclen = varoff;
	bp->buflen = len;
}

/*
 * Search the driver translation table:
 *
 * This routine searches the driver translation table looking for
 * the first entry describing a device serviced by the indicated
 * realmode driver at "dp". It returns a pointer to the corresponding
 * entry, or NULL if there are no devices serviced by the target
 * driver.
 *
 * If multiple device types are handled by the given driver, we
 * return the "devtrans" record associated with the first such
 * device to appear in the "master" file.
 */
devtrans *
TranslateDriver_devdb(char *dp)
{
	char *cp, buf[sizeof (((devtrans *)0)->real_driver)];

	for (cp = buf; *dp && (*dp != '.'); dp++) {
		*cp++ = tolower(*dp);
	}
	*cp = '\0';

	return (translate((devtrans *)
	    (buf - (int)((devtrans *)0)->real_driver), ttp));
}

/*
 * Search device translation table:
 *
 * Given a device ID and bus type, this routine returns a pointer
 * corresponding device translation record. Returns NULL if the
 * indicated device is not in the translation table.
 */
devtrans *
TranslateDevice_devdb(unsigned long id, u_char bt)
{
	devtrans dmy;

	memset(&dmy, 0, sizeof (dmy));
	dmy.devid = id;
	dmy.bustype = bt;

	return (translate(&dmy, dtp));
}

/*
 * Given a pointer to a device node (or a device descriptor), this
 * routine returns a pointer to an ASCII representation of the device
 * name. If the verbose flag is not set, the name is only the descriptive
 * text for the device. Things like vendor number/device number are skipped.
 */
char *
GetDeviceName_devdb(Board *bp, int verbose)
{
	return ((*bus_ops[ffbs(bp->bustype)].bp_to_desc)(bp, verbose));
}

char *
GetDeviceId_devdb(Board *bp, char *buf)
{
	/*
	 * Given a pointer to a device node (or a device descriptor node),
	 * this routine converts the corresponding device ID to its 1275
	 * style name. If the "buf" argument is non-null, it is
	 * assumed to point to a buffer which is to receive the formatted
	 * output; otherwise we do the formatting in a local buffer.
	 *
	 * Returns the address of the externally formatted device ID.
	 */

	static char idbuf[32];
	if (buf == 0) buf = idbuf;

	/*
	 * External device ID format depends on the bus type ...
	 */
	switch (bp->bustype) {
	case RES_BUS_PCI:
		bp_to_name_pci1275(bp, buf);
		break;

	default:
		/*
		 * Everything else uses the EISA encoding.
		 * Call "DecompressName" to do the dirty work.
		 */
		DecompressName(bp->devid, buf);
		break;
	}

	return (buf);
}

/*
 * Search a property list:
 *
 * This routine may be used to search the indicated devprop "list"
 * for an entry with the given "name". If we find such an entry,
 * we store a pointer to the corresponding property "value" and
 * the length and then return 0. Otherwise, we return -1.
 */
int
GetDevProp_devdb(devprop **list, char *name, char **value, int *len)
{
	devprop *dvp;
	*value = 0;

	/*
	 * A simple linear search thru the property list. We're
	 * assuming that most device nodes don't have that many
	 * properties.
	 */
	for (dvp = *list; dvp; dvp = dvp->next) {
		if (strcmp(name, dvp->name) == 0) {
			/*
			 * A match! Store the address of the property value
			 * at "*value" and length at "*len" and return success.
			 */
			*value = prop_value(dvp);
			*len = dvp->len;
			return (PROP_OK);
		}
	}

	return (PROP_FAIL);
}

/*
 * Update a property list:
 *
 * This routine may be used to add devprop elements to the indicated
 * "list" (or to change the value of elements that are already in
 * the list). If the "value" pointer is NULL, we delete the "name"d
 * proprety from the list. Otherwise we create (or update) the
 * named element with the indicated value.
 *
 * Returns 0 if it works, -1 if there's no memory.
 */
int
SetDevProp_devdb(devprop **list, char *name, void *value, int len, int bin)
{
	devprop *dvp, *dxp = 0;
	devprop **lptr;
	int n = strlen(name) + 1;

	if (value != NULL) {
		/*
		 * We've got a value, so allocate a new devprop
		 * record to hold it. This record will eventually replace
		 * any existing record of the same name.
		 */
		if (!(dxp = (devprop *)malloc(sizeof (devprop) + len + n))) {
			/*
			 * Can't get memory for the devprop record. Return
			 * error indicator.
			 */
			return (PROP_FAIL);
		}

		memset(dxp, 0, sizeof (devprop));
		dxp->bin = bin;
		dxp->vof = n;
		dxp->len = len;

		strcpy(dxp->name, name);
		memcpy(prop_value(dxp), value, len);
	}

	lptr = list;
	for (dvp = *list; dvp; dvp = dvp->next) {
		/*
		 * Search the property list for an entry with the given name.
		 * If we find one, we'll have to update it with the new value.
		 */
		if (strcmp(dvp->name, name) == 0) {
			/*
			 * Matched! If we've got a new record ("dxp" is non-
			 * null), swap it into place. Otherwise remove the
			 * current record from the list. In either case we
			 * can free up the old record.
			 */
			if (dxp) {
				dxp->next = dvp->next;
			} else {
				dxp = dvp->next;
			}
			*lptr = dxp;

			free(dvp);
			return (PROP_OK);
		}
		lptr = &dvp->next;
	}

	if (dxp) {
		*lptr = dxp;
	}
	return (PROP_OK);
}

/*
 * Initialize device database
 *
 * This routine reads the master device data base file and uses the
 * data contained therein to create the device/driver translation
 * tables at "ttp" and "dtp".
 */
void
init_devdb()
{
	FILE *fp;
	devtrans *tp, *dxp;
	int pidx;

	if (!(fp = fopen("solaris/devicedb/master", "r"))) {
		/*
		 * We can't do much without a "master" file!
		 */
		fatal("can't open device data base: %!");
	}
	master_entry_buf = malloc(MASTER_LINE_MAX);

	db_rewind(fp);
	while (tp = db_read(DB_FILE, (void *)fp)) {
		ttx++; /* Count number of translations */
		if (tp->category == DCAT_PLAT)
			ptx++;
	}
	ASSERT(((unsigned long)ttx * sizeof (devtrans)) <= 0xFFF0UL);
	db_rewind(fp);
	DoCpy++;

	/*
	 * In the following we cannot use spcl_malloc for allocating dtp
	 * because the code seems to use address arithmetic somewhere,
	 * probably buried in the sort and/or translation code.
	 */
	if (!(ttp = (devtrans **)spcl_malloc(ttx * sizeof (devtrans *))) ||
	    !(dtp = (devtrans **)malloc(ttx * sizeof (devtrans *))) ||
	    (ptx != 0 &&
		!(ptp = (devtrans **)spcl_malloc(ptx * sizeof (devtrans *)))) ||
	    !(dxp = (devtrans *)spcl_malloc(ttx * sizeof (devtrans)))) {
		/*
		 * We've got three tables here: The device translation table
		 * proper (dxp) and two indexes into it: One sorted by realmode
		 * driver name (ttp) and one sorted by device ID (dtp).
		 */
		MemFailure();
	}

	pidx = 0;
	while (tp = db_read(DB_FILE, (void *)fp)) {
		/*
		 * Make a second pass over the master file, this time saving
		 * a copy of each devtrans record in dynamically allocated
		 * memory and planting pointers to it in the two index arrays.
		 */
		ASSERT(dtx < ttx); dxp[dtx] = *tp;
		ttp[dtx] = dtp[dtx] = &dxp[dtx];
		if (tp->category == DCAT_PLAT)
			ptp[pidx++] = &dxp[dtx];
		dxp[dtx].seq = dtx;
		dtx++;
	}

	ASSERT(dtx == ttx);
	ASSERT(pidx == ptx);
	free(master_entry_buf);
	fclose(fp);
	sort_tables();
}

static void
sort_tables()
{
	u_int n;

	NoSeq = 0;
	qsort(dtp, dtx, sizeof (devtrans *), CmpDev);
	qsort(ttp, ttx, sizeof (devtrans *), CmpDrv);
	NoSeq = 1;

	for (n = 1; n < ttx; n++) {
		/*
		 * Now search the driver cross reference index looking for
		 * duplicate driver names.
		 */
		if (!CmpDrv((const void *)&ttp[n], (const void *)&ttp[n-1])) {
			/*
			 * We've seen this driver before. Remove the
			 * duplicate entry from the list so we don't screw
			 * up the binary search.
			 */
			memcpy(&ttp[n], &ttp[n+1],
			    (--ttx - n) * sizeof (devtrans *));
			n -= 1;
		}
	}
	ttp = (struct devtrans **)
	    spcl_realloc(ttp, ttx * sizeof (struct devtrans *));
	for (n = 1; n < dtx; n++) {
		/*
		 * Now search the device ID index looking for duplicate
		 * entries. There shouldn't be any, but it's better to be
		 * safe than sorry!
		 */
		if (!CmpDev((const void *)&dtp[n], (const void *)&dtp[n-1])) {
			/*
			 * Remove duplicate entry from the cross reference!
			 */
			memcpy(&dtp[n], &dtp[n+1],
			    ((--dtx - n) * sizeof (devtrans *)));
			n -= 1;
		}
	}
}

static struct menu_options isa_options[] = {
	/* Options for selecting device */

	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};

#define	NISA_OPTIONS (sizeof (isa_options)/sizeof (*isa_options))

/*
 * Build a menu of ISA devices from the master file to allow the user to
 * pick an already defined ISA id. Display this menu and also give the
 * user the option of defining his own id.
 */
u_long
get_isa_id_devdb()
{
	u_int n_isa, i, j;
	struct menu_list *listp, *choice;
	u_long id;

	/*
	 * Count the number of ISA devices in the master file
	 * Note we count those marked as "isa" and "all".
	 */
	for (n_isa = 0, i = 0; i < dtx; i++) {
		if ((dtp[i]->bustype == RES_BUS_ISA) ||
		    (dtp[i]->bustype == 0)) {
			n_isa++;
		}
	}

	/*
	 * allocate enough entries
	 */
	listp = (struct menu_list *)calloc(n_isa, sizeof (struct menu_list));
	if (listp == 0) {
		MemFailure();
	}

	/*
	 * Create the table
	 */
	for (i = 0, j = 0; i < dtx; i++) {
		if ((dtp[i]->bustype == RES_BUS_ISA) ||
		    (dtp[i]->bustype == 0)) {
			listp[j].datum = (void *) dtp[i];
			listp[j].string = dtp[i]->dname;
			j++;
		}
	}
	qsort(listp, n_isa, sizeof (struct menu_list), CmpNam);

	/*
	 * Now remove duplicate entries
	 */
	for (i = 1; i < n_isa; i++) {
		if (strcmp(listp[i].string, "Unsupported ISA device") == 0) {
			listp[i].flags |= MF_SELECTED;
		}
		if (strcmp(listp[i].string, listp[i - 1].string) == 0) {
			memcpy(&listp[i], &listp[i + 1],
			    ((--n_isa - i) * sizeof (struct menu_list)));
			i--;
		}
	}

redisplay:
	switch (select_menu("MENU_HELP_ISA", isa_options,
	    NISA_OPTIONS, listp, n_isa, MS_ZERO_ONE, "MENU_ISA")) {

	case FKEY(2):
		if ((choice = get_selection_menu(listp, n_isa)) == NULL) {
			/* user didn't pick one */
			beep_tty();
			goto redisplay;
		}
		id = ((devtrans *)choice->datum)->devid;
		free(listp);
		return (id);

	case FKEY(3):
		free(listp);
		return (0);
	default:
		beep_tty();
		goto redisplay;
	}
}

/*
 * Get the next master file line
 *
 * Note an fp of NULL means we want to read from an internal buffer
 * supplied by itu, otherwise we read from the master file pointed
 * to by fp.
 */
static char *
get_next_line(int where, void *vp)
{
	char *cp;
	static int toggle = 1;

	switch (where) {
	case DB_FILE:
		if (cp = fgets(master_entry_buf, MASTER_LINE_MAX, (FILE *)vp)) {
			ASSERT(cp == master_entry_buf);
			if (strchr(cp, '\n') == NULL) {
				fatal("master file line too long max is %d\n",
					MASTER_LINE_MAX);
			}
			return (cp);
		}
		return (0);

	case DB_ITU:
		/*
		 * Want to ensure only one line is read, by emulating
		 * a one line file.
		 */
		if (toggle) {
			toggle = 0; /* set up for next time */
			return ((char *)vp);
		} else {
			toggle = 1; /* set up for next time */
			return (0);
		}
	}
	/*NOTREACHED*/
}

static void
db_rewind(FILE *fp)
{
	rewind(fp);
	line_devdb = 0;
}

static void
warn_devdb(char *statement, char *code_name, int code_line)
{
	debug(D_ERR, "Syntax error in master file at line %d\n\n"
	    "Assertion - \"%s\" at %s:%d\n",
	    line_devdb, statement, code_name, code_line);
	warning("Syntax error in master file at line %d\n\n"
	    "Assertion - \"%s\" at %s:%d\n",
	    line_devdb, statement, code_name, code_line);
}

/*
 * This function is used by ITU to update the device database.
 */
void
master_file_update_devdb(char *master_file_line)
{
	devtrans *tp, *tp2;
	u_int i;

	while (tp = db_read(DB_ITU, master_file_line)) {
		/*
		 * Check if we can replace an old entry.
		 */
		for (i = 0; i < dtx; i++) {
			if (!CmpDev((const void *)&tp, (const void *)&dtp[i])) {
				/*
				 * Copy in new one
				 *
				 * Note we can't free the property list or
				 * name of the old entry in case it is
				 * shared with other entries.
				 */
				*dtp[i] = *tp;
				continue;
			}
		}
		if (i == dtx) {
			/*
			 * Must be a new entry - add it to the database
			 */
			if (!(tp2 = spcl_malloc(sizeof (*tp2))) ||
			    !(dtp = spcl_realloc(dtp, (dtx + 1) *
				sizeof (tp))) ||
			    !(ttp = spcl_realloc(ttp, (ttx + 1) *
				sizeof (tp))) ||
			    (tp->category == DCAT_PLAT &&
			    !(ptp = spcl_realloc(ptp, (ptx + 1) *
				sizeof (tp))))) {
				MemFailure();
			}
			*tp2 = *tp; /* copy new one */
			/*
			 * assign an arbitrary large sequence number
			 * so that its the last entry in the table.
			 */
			tp2->seq = 0xffff;
			dtp[dtx++] = tp2;
			ttp[ttx++] = tp2;
			if (tp->category == DCAT_PLAT) {
				ptp[ptx++] = tp2;
			}
		}
		sort_tables();
	}
}
