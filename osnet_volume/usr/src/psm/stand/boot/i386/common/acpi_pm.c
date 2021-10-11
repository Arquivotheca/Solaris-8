/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *	ACPI enumeration
 *
 *	This file contains ACPI related routines to provide
 *	motherboard's device configurations.
 */

#pragma ident	"@(#)acpi_pm.c	1.4	99/11/03 SMI"

#include <string.h>
#include <sys/types.h>
#include <sys/salib.h>
#include <sys/debug.h>
#include <sys/dosemul.h>
#include <sys/promif.h>
#include <sys/pci.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>
#include "board_res.h"
#include "acpi_res.h"

#ifndef ASSERT
#define	ASSERT(x)
#endif

#define	MemFailure()	prom_panic("boot.bin: out of memory")
#define	varoff		((int)&((Board *) 0)->var)

static void acpi_add_board(Board *bp);
static struct board *acpi_new_board();
static Board *acpi_resize_board(Board *bp, unsigned len);
static char *bkmem_realloc(char *buf, int size, int new_size);
void acpi_copy(struct real_regs *rp);
void acpi_enum(void);
static void acpi_trans_dev(acpi_obj dev, int bustype, int busno);
static int acpi_eval_int(acpi_obj dev, char *method, unsigned int *rint);
static int acpi_eval_buf(acpi_obj dev, char *method, acpi_val_t **rvp);

static Board *parse_res(Board *bp, u_char *rp);
static Board *crs_small_res(Board *bp, u_char *cp, u_char tag);
static Board *crs_large_res(Board *bp, u_char *cp, u_char tag);
static Board *acpi_add_res(Board *, unsigned short, ulong, ulong,
	unsigned short);
static int ffbs(u_int x);

extern void prom_panic();
extern unchar pci_cfg_getb(int, int, int, int);


Board *acpi_head = NULL;

#ifdef	ACPI_DEBUG

#define	ACPI_DBOARD	0x00000001	/* display all boards when done */
#define	ACPI_DENUM	0x00000002	/* debug acpi_enum() */
#define	ACPI_DRES	0x00000004	/* display resources during parsing */
#define	ACPI_DEVAL	0x00000008	/* debug acpi method evaluation */
#define	ACPI_DCOPY	0x00000010	/* debug acpi_copy - will mess up */
					/* bootconf screen */

int acpi_debug_flag = ACPI_DENUM | ACPI_DBOARD | ACPI_DEVAL;

extern int goany();

static void decompress_name(unsigned int id, char *np);
static void list_resources_boards(Board *bp);
static char *addtobuf(char *buf, int *buflen, char *str);
static void acpi_bdump(void);
static void acpi_debug(int dtype, char *fmt, ...);
static void acpi_pause(int dtype);

static const char hextab[] = "0123456789ABCDEF";

static void
decompress_name(unsigned int id, char *np)
{
	/*
	 *  Expand a PNP HID
	 *
	 *  It converts a 32-bit PNP id to a 7-byte ASCII device name,
	 * which is stored at "np".
	 */

	*np++ = '@' + ((id >> 2)  & 0x1F);
	*np++ = '@' + ((id << 3)  & 0x18) + ((id >> 13) & 0x07);
	*np++ = '@' + ((id >> 8)  & 0x1F);
	*np++ = hextab[(id >> 20) & 0x0F];
	*np++ = hextab[(id >> 16) & 0x0F];
	*np++ = hextab[(id >> 28) & 0x0F];
	*np++ = hextab[(id >> 24) & 0x0F];
	*np = 0;
}

char *
addtobuf(char *buf, int *buflen, char *str)
{
	if (strlen(buf) + strlen(str) + 1 > *buflen) {
		if ((buf = bkmem_realloc(buf, *buflen, *buflen + 160)) == NULL)
			MemFailure();
		*buflen += 160;
	}
	(void) strcat(buf, str);
	return (buf);
}

#define	RTYPE(rp) ((rp)->flags & (RESF_TYPE+RESF_DISABL+RESF_ALT))

/*
 *  Display resources associated with the board
 */
static void
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
	char *ResTypes[] = {
		"slot", "Port", "IRQ", "DMA", "Memory", "name"
	};
	char No_resources[] = "No resources";
	char XS_res[] = "Too many resources - use modify to inspect resources";

	blen = 160;
	if ((buf = bkmem_alloc(blen)) == NULL)
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
				printf("rcount=%d restype=%d rp->base=%lx "
					"rp->len=%lx\n", resource_cnt,
					restype, rp->base, rp->length);
				rbp = resbuf;
				/*
				 * If we have already put out resources
				 * put out the proper seperator, ',' if
				 * more resources of this type, or ';' if
				 * we are starting a new resource type
				 */
				if (resource_cnt) {
					(void) sprintf(lp, "%c ",
					    print_rname ? ';' : ',');
					lp += 2;
				} else {
					if (neednl) {
						buf = addtobuf(buf, &blen,
							"\n");
						neednl = 0;
					}
					(void) sprintf(lp, "     ");
					lp += 5;
				}
				/*
				 * If we are starting a new resource type,
				 * put out the name of the resource
				 */
				if (print_rname) {
					(void) sprintf(resbuf, "%s: ",
					    ResTypes[restype]);
					rbp += strlen(ResTypes[restype]) + 2;
					print_rname = 0;
				}

				/*
				 * Generate the info for this resource
				 */
				(void) sprintf(rbp, fmt, rp->base);
				rbp += strlen(rbp);
				len = rp->base + rp->length - 1;

				if (rp->length > 1) {
					(void) sprintf(rbp, "-%lX", len);
					rbp += strlen(rbp);
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
						bkmem_free(buf, blen);
						printf("%s\n", XS_res);
						return;
					}
					/*
					 * Emit the line buffer and
					 * start a new line
					 */
					buf = addtobuf(buf, &blen, linebuf);
					buf = addtobuf(buf, &blen, "\n");
					lp = linebuf;
					(void) sprintf(linebuf, "     ");
					lp += 5;
				}
				/*
				 * Put the new resource into the line buffer
				 */
				(void) sprintf(lp, "%s", resbuf);
				lp += strlen(lp);
			}
		}
	}
	if (resource_cnt == 0) {
		if (neednl)
			buf = addtobuf(buf, &blen, "\n     ");
		else
			buf = addtobuf(buf, &blen, "     ");
		buf = addtobuf(buf, &blen, (char *)No_resources);
	} else
		buf = addtobuf(buf, &blen, linebuf);
	printf("%s\n", buf);
	bkmem_free(buf, blen);
}

void
acpi_bdump(void)
{
	int i;
	Board *bp;

	for (i = 0, bp = acpi_head; bp != NULL; bp = bp->link, i++) {
		printf("ACPI board %d bp=%lx:-\n", i, bp);
		printf("devid=%lx bustype=%d\n", bp->devid, bp->bustype);
		list_resources_boards(bp);
		(void) goany();
	}
}


static void
acpi_debug(int dtype, char *fmt, ...)
{
	va_list adx;

	if (dtype & acpi_debug_flag) {
		va_start(adx, fmt);
		prom_vprintf(fmt, adx);
		va_end(adx);
	}
}

static void
acpi_pause(int dtype)
{
	if (dtype & acpi_debug_flag)
		(void) goany();
}

#endif

/*
 * Add board to global boards chain (at the end).
 */
void
acpi_add_board(Board *bp)
{
	Board *tbp;

	/* Shrink board to actual used size */
	bp = (Board *)bkmem_realloc((char *)bp, bp->buflen, bp->reclen);
	bp->buflen = bp->reclen;
	bp->link = 0;
	if (acpi_head == NULL)
		acpi_head = bp;
	else {
		/*
		 * Could maintain a tail pointer but the list is short
		 * and following the links should be quick.
		 */
		for (tbp = acpi_head; tbp->link; tbp = tbp->link) {
			;
		}
		tbp->link = bp;
	}
}

#define	INITIAL_BOARD_SIZE 256

Board *
acpi_new_board()
{
	Board *bp;

	if (!(bp = (Board *)bkmem_alloc(INITIAL_BOARD_SIZE))) {
		MemFailure();
	}
	bp->reclen = ((int)&((Board *)0)->var);
	bp->buflen = INITIAL_BOARD_SIZE;

	return (bp);
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
acpi_resize_board(Board *bp, unsigned len)
{
	ASSERT(bp != 0);
	/*
	 * Keep doubling the size of the record buffer until it's
	 * big enougn to hold "len" more bytes of data.
	 */
	while (len >= (bp->buflen - bp->reclen)) {
		unsigned n = bp->buflen << 1;

		ASSERT((bp->buflen & 0x8000) == 0);
		bp = (Board *)bkmem_realloc((char *)bp, bp->buflen, n);
		bp->buflen = n;
	}
	return (bp);
}

static char *
bkmem_realloc(char *buf, int len, int new_len)
{
	char *new_buf;
	int copy_len;

	if ((new_buf = bkmem_alloc(new_len)) == NULL)
		MemFailure();
	copy_len = (len <= new_len) ? len : new_len;
	(void) memcpy(new_buf, buf, copy_len);
	bkmem_free(buf, len);
	return (new_buf);
}

/*
 * Since the ACPI data are located above 1 Mb, it will be very difficult for
 * the realmode bootconf.exe to access them.  Also, it will be a big challenge
 * to fit the ACPI interpreter/namespace into the bootconf.exe.  Therefore,
 * boot.bin will complete all the ACPI enumeration and then transfer all
 * the information retrieved to the bootconf thru this routine acpi_copy().
 * This routine shares the same entry point with the int21 oem bios call
 * (AH=0xFE) and it gets called if BX=0xFD.
 *
 * Here is the common structure used to communicate between boot.bin and
 * bootconf.exe:-
 *
 * struct acpi_bc {
 *	unsigned long bc_buf;		- 32-bit linear buf addr
 *	unsigned long bc_this;		- this board address to copy
 *	unsigned long bc_next;		- next board addres to copy
 *	unsigned short bc_buflen;	- length of buf in bytes
 *	unsigned short bc_nextlen;	- length of next board
 *	unsigned long bc_flag;
 * };
 *
 * bc_this should be set to 0 for the first time it gets called.  It will
 * not do any copy for the first time instead it will set bc_next and
 * bc_nextlen to the address and the size of first record.  Once this
 * structure gets returned to the caller, the caller will allocate a
 * buffer of bc_nextlen size and save the address of the buffer in bc_buf.
 * The caller will then move bc_next to bc_this, bc_nextlen to bc_buflen
 * and call acpi_copy again.  acpi_copy will copy the board data at bc_this
 * to bc_buf and setup transfer by saving the next record address in bc_next
 * and its size in bc_nextlen.  After the last record is copied, bc_next
 * will be set to 0.
 */
void
acpi_copy(struct real_regs *rp)
{
	struct acpi_bc *bcp;
	Board *tbp;

	bcp = (struct acpi_bc *)DS_DX(rp);	/* get acpi_bc struct */

#ifdef	ACPI_DEBUG
	acpi_debug(ACPI_DCOPY, "acpi_copy enter: %lx %lx %lx %lx %lx %lx %x\n",
		bcp, bcp->bc_buf, bcp->bc_buflen, bcp->bc_this,
		bcp->bc_next, bcp->bc_nextlen, bcp->bc_flag);
#endif

	/* sanity check */
	if (bcp == 0) {
		acpi_client_status(ACPI_BOOT_BOOTCONF, ACPI_CLIENT_OFF);
		AX(rp) = 1;
		return;
	}
	if (bcp->bc_this == 0) { /* start */
		if ((acpi_options_prop & ACPI_OUSER_MASK) == ACPI_OUSER_OFF) {
			acpi_client_status(ACPI_BOOT_BOOTCONF,
						ACPI_CLIENT_OFF);
			AX(rp) = 1;
			return;
		}
		bcp->bc_next = (unsigned long) acpi_head;
		if (acpi_head != NULL) {
			bcp->bc_nextlen = acpi_head->buflen;
			acpi_client_status(ACPI_BOOT_BOOTCONF, ACPI_CLIENT_ON);
		}
	} else {
		bcopy((char *)bcp->bc_this, (char *)bcp->bc_buf,
			bcp->bc_buflen);
		tbp = (Board *) bcp->bc_this;
		bcp->bc_next = (unsigned long) tbp->link;
		if (tbp->link)
			bcp->bc_nextlen = tbp->link->buflen;
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DCOPY, "acpi_copy return: %x %x %x %x %x %x\n",
			bcp->bc_buf, bcp->bc_buflen, bcp->bc_this,
			bcp->bc_next, bcp->bc_nextlen, bcp->bc_flag);
#endif
	}
	AX(rp) = 0;
}

void
acpi_enum()
{
	acpi_obj rootobj, sbobj;

	if (acpi_init() != ACPI_OK) {
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DENUM, "acpi_init() failed\n");
		acpi_pause(ACPI_DENUM);
#endif
		return;
	}
	if ((rootobj = acpi_rootobj()) == NULL) {
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DENUM, "acpi_rootobj() returned NULL\n");
		acpi_pause(ACPI_DENUM);
#endif
		acpi_disable();
		return;
	}
	if ((sbobj = acpi_findobj(rootobj, "_SB", 0)) == NULL) {
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DENUM, "acpi_findobj() failed for _SB\n");
		acpi_pause(ACPI_DENUM);
#endif
		acpi_disable();
		return;
	}
	acpi_trans_dev(sbobj, 0, 0);
#ifdef	ACPI_DEBUG
	if ((acpi_debug_flag & ACPI_DBOARD) && (acpi_head != NULL))
		acpi_bdump();
#endif
}

#define	PNP_PCI_BUS	0x030AD041		/* PNP0A03 */
#define	PNP_ISA_BUS	0x000AD041		/* PNP0A00 */

#define	PNP_PCI_INT_LNK	0x0F0CD041		/* PNP0C0F */

static void
acpi_trans_dev(acpi_obj dev, int bustype, int busno)
{
	acpi_obj cdev, ndev, obj;
	acpi_val_t *crs_vp;
	unsigned int devid, status, uid, adr;
	Board *bp;
	acpi_nameseg_t devname;
	int newbus, newbusno, rv;

#ifdef	ACPI_DEBUG
	char name[10], dname[10];
	int i;
#endif
	bp = (Board *) NULL;
	newbus = bustype;
	newbusno = busno;

	devname = acpi_nameseg(dev);
#ifdef	ACPI_DEBUG
	for (i = 0; i < 4; i++)
		dname[i] = devname.cseg[i];
	dname[4] = 0;
	acpi_debug(ACPI_DENUM, "acpi_nameseg() returned iseg=%x cseg=%s\n",
		devname.iseg, dname);
#endif
	/* evaluate _INI once if it exists according to the spec */
	if ((obj = acpi_findobj(dev, "_INI", ACPI_EXACT)) != NULL) {
		/* don't care about the return value */
		(void) acpi_eval(obj, NULL, NULL);
	}
	devid = 0;
	if (acpi_eval_int(dev, "_HID", &devid) == ACPI_OK) {
#ifdef	ACPI_DEBUG
		decompress_name(devid, name);
		acpi_debug(ACPI_DENUM, "hid=%x name=%s\n", devid, name);
#endif
		if (devid == PNP_PCI_BUS) {
			newbus = RES_BUS_PCI;
			/*
			 * If no _BBN, then this is the only pci bus.
			 * Set newbusno to 0.
			 */
			if (acpi_eval_int(dev, "_BBN",
				(unsigned int *) &newbusno) != ACPI_OK)
				newbusno = 0;
#ifdef	ACPI_DEBUG
			acpi_debug(ACPI_DENUM, "devid=PNP_PCI_BUS busno=%x\n",
					newbusno);
#endif
		} else if (devid == PNP_ISA_BUS) {
			newbus = RES_BUS_ISA;
			newbusno = 0;
#ifdef	ACPI_DEBUG
			acpi_debug(ACPI_DENUM, "devid=PNP_ISA_BUS\n");
#endif
		}
	} else if ((bustype == RES_BUS_PCI) &&
		(acpi_eval_int(dev, "_ADR", &adr) == ACPI_OK)) {
		/* no _HID, check _ADR if it is on pci bus */
		int devno, func;
		unchar class, subclass;

		/* adr = devno << 16 | func */
		devno = adr >> 16;
		func = adr & 0xFFFF;
		class = pci_cfg_getb(busno, devno, func, PCI_CONF_BASCLASS);
		if (class == PCI_CLASS_BRIDGE) {
			subclass = pci_cfg_getb(busno, devno, func,
				PCI_CONF_SUBCLASS);
			if (subclass == PCI_BRIDGE_ISA) {
				devid = PNP_ISA_BUS;
				newbus = RES_BUS_ISA;
				newbusno = 0;
#ifdef	ACPI_DEBUG
				acpi_debug(ACPI_DENUM, "PCI_BRIDGE_ISA "
					"found\n");
#endif
			} else if (subclass == PCI_BRIDGE_PCI) {
				devid = PNP_PCI_BUS;
				newbusno = (int)pci_cfg_getb(busno,
					devno, func, PCI_BCNF_SECBUS);
#ifdef	ACPI_DEBUG
				acpi_debug(ACPI_DENUM, "PCI_BRIDGE_PCI "
					"found: newbusno=%x\n", newbusno);
#endif
			}
		}
	}
	if (devid && (devid != PNP_PCI_INT_LNK)) {
		/*
		 * pci bus address space is not supported right now
		 * will handle it when PCI Hot Plug using ACPI is supported
		 */
		if ((devid != PNP_PCI_BUS) && ((rv = acpi_eval_int(dev, "_STA",
			&status)) != ACPI_EEVAL)) {
			/*
			 * According to the new spec., it is okay that a
			 * device doesn't have a "_STA" object (i.e. ACPI_EOBJ).
			 * If the _STA object is not present, then assume
			 * the device is present and enabled.
			 */
#ifdef	ACPI_DEBUG
			acpi_debug(ACPI_DENUM, "bus=%x ", newbus);
			acpi_debug(ACPI_DENUM, "device=%s status=%x\n", dname,
					status);
#endif
			if ((rv == ACPI_EOBJ) || ((status & STA_PRESENT) &&
				(status & STA_ENABLE))) {
				if (acpi_eval_int(dev, "_UID", &uid))
					/* no UID, so set it to -1 */
					uid = (u_long) -1;
#ifdef	ACPI_DEBUG
				else
					acpi_debug(ACPI_DENUM, "device=%s "
						"uid:\n", dname, uid);
#endif
				if (! acpi_eval_buf(dev, "_CRS", &crs_vp)) {
#ifdef	ACPI_DEBUG
					acpi_debug(ACPI_DENUM, "device=%s "
						"parse _CRS:\n", dname);
#endif
					bp = acpi_new_board();
					bp->devid = devid;
					bp->acpi_hid = devid;
					bp->acpi_uid = uid;
					bp->acpi_nseg = devname.iseg;
					bp->acpi_status = ACPI_CLEAR;
					bp = parse_res(bp, crs_vp->acpi_valp);
					acpi_val_free(crs_vp);
					bp->bustype = (unsigned char) newbus;
					bp->acpi_id = (unsigned long) bp;
					acpi_add_board(bp);
				}
			}
		}
	}
#ifdef	ACPI_DEBUG
	acpi_pause(ACPI_DENUM);
#endif
	if ((cdev = acpi_childdev(dev)) != NULL)
		acpi_trans_dev(cdev, newbus, newbusno);
	if ((ndev = acpi_nextdev(dev)) != NULL)
		acpi_trans_dev(ndev, bustype, busno);
}

int
acpi_eval_int(acpi_obj dev, char *method, unsigned int *rint)
{
	acpi_val_t *rvp;
	acpi_obj obj;

#ifdef	ACPI_DEBUG
	acpi_debug(ACPI_DEVAL, "acpi_eval_int: method=%s\n", method);
#endif
	if ((obj = acpi_findobj(dev, method, ACPI_EXACT)) != NULL) {
		if (acpi_eval(obj, NULL, &rvp) == ACPI_OK) {
			if (rvp->type == ACPI_INTEGER) {
#ifdef	ACPI_DEBUG
				acpi_debug(ACPI_DEVAL, "acpi_eval returned OK "
					"acpi_ival=%x\n", rvp->acpi_ival);
#endif
				*rint = rvp->acpi_ival;
				acpi_val_free(rvp);
				return (ACPI_OK);
			} else
				acpi_val_free(rvp);
		}
		/* object found but failed to evaluate */
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DEVAL, "acpi_eval_int failed: ACPI_EEVAL\n");
#endif
		return (ACPI_EEVAL);
	}
	/* object not found */
#ifdef	ACPI_DEBUG
	acpi_debug(ACPI_DEVAL, "acpi_eval_int failed: ACPI_EOBJ\n");
#endif
	return (ACPI_EOBJ);
}

/*
 * evaluate the method under dev which returns a buffer
 */
int
acpi_eval_buf(acpi_obj dev, char *method, acpi_val_t **rvp)
{
	acpi_obj obj;
#ifdef	ACPI_DEBUG
	int i;
	unsigned char *cp;
#endif

#ifdef	ACPI_DEBUG
	acpi_debug(ACPI_DEVAL, "acpi_eval_buf: method=%s\n", method);
#endif
	if ((obj = acpi_findobj(dev, method, ACPI_EXACT)) != NULL) {
		if (acpi_eval(obj, NULL, rvp) == ACPI_OK) {
			if ((*rvp)->type == ACPI_BUFFER) {
#ifdef	ACPI_DEBUG
				acpi_debug(ACPI_DEVAL, "acpi_eval returned OK "
					"acpi_valp=%x len=%x content:-\n",
					(*rvp)->acpi_valp, (*rvp)->length);
				acpi_pause(ACPI_DEVAL);
				cp = (unsigned char *) (*rvp)->acpi_valp;
				for (i = 0; i < (*rvp)->length; i++) {
					acpi_debug(ACPI_DEVAL, "%x ",
						*(cp + i));
					if (((i + 1) % 20) == 0)
						acpi_debug(ACPI_DEVAL, "\n");
				}
				acpi_debug(ACPI_DEVAL, "\n");
				acpi_pause(ACPI_DEVAL);
#endif
				return (ACPI_OK);
			} else
				acpi_val_free(*rvp);
		}
		/* object found but failed to evaluate */
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DEVAL, "acpi_eval_int failed: ACPI_EEVAL\n");
#endif
		return (ACPI_EEVAL);
	}
	/* object not found */
#ifdef	ACPI_DEBUG
	acpi_debug(ACPI_DEVAL, "acpi_eval_int failed: ACPI_EOBJ\n");
#endif
	return (ACPI_EOBJ);
}

Board *
parse_res(Board *bp, u_char *rp)
{
	u_char tag;

	while ((tag = *rp) != END_TAG) {
		if (tag & 0x80) {
			bp = crs_large_res(bp, rp, tag);
			rp += (*((short *)(rp + 1))) + 3;
		} else {
			bp = crs_small_res(bp, rp, tag);
			rp += (tag & 7) + 1;
		}
	}
#ifdef	ACPI_DEBUG
	acpi_debug(ACPI_DRES, "end tag\n");
	acpi_pause(ACPI_DRES);
#endif
	return (bp);
}

Board *
crs_large_res(Board *bp, u_char *cp, u_char tag)
{
	u_int base, len;

#ifdef	ACPI_DEBUG
	acpi_debug(ACPI_DRES, "crs_large_res entered cp=%lx tag=%x\n", cp, tag);
	acpi_pause(ACPI_DRES);
#endif
	switch (tag) {
	case MEMORY24_RANGE_DESCRIPTOR:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "24-bit memory range descriptor\n");
#endif
		if (*(u_int *)(cp + 4) != *(u_int *)(cp + 6)) {
#ifdef	ACPI_DEBUG
			acpi_debug(ACPI_DRES, "ERROR: 24bit mem min and max "
				"differ\n");
#endif
			return (bp);
		}
		len = *(u_int *)(cp + 10);
		if (len) {
			base = *(u_int *)(cp + 4);
			base <<= 8; /* convert to 24 bit addr */
			len <<= 8; /* convert to 24 bit addr */
			bp = acpi_add_res(bp, RESF_Mem, base, len,
				(unsigned short)(cp[3] & 0x1));
		}

		break;

	case MEMORY32_RANGE_DESCRIPTOR:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "32-bit memory range descriptor\n");
#endif
		if (*(u_int *)(cp + 4) != *(u_int *)(cp + 8)) {
#ifdef	ACPI_DEBUG
			acpi_debug(ACPI_DRES, "ERROR: 32bit mem min and max "
				"differ\n");
#endif
			return (bp);
		}
		len = *(u_int *)(cp + 16);
		if (len) {
			base = *(u_int *)(cp + 4);
			bp = acpi_add_res(bp, RESF_Mem, base, len,
				(unsigned short)(cp[3] & 0x1));
		}
		break;

	case MEMORY32_FIXED_DESCRIPTOR:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "32-bit fixed location memory range "
			"descriptor\n");
#endif
		len = *(u_int *)(cp + 8);
		if (len) {
			base = *(u_int *)(cp + 4);
			bp = acpi_add_res(bp, RESF_Mem, base, len,
				(unsigned short)(cp[3] & 0x1));
		}
		break;
	/*
	 * address space descriptor will be needed when supporting PCI
	 * HotPlug using ACPI.  A new resource type will be needed
	 * for the address space.
	 */
	case DWORD_ADDR_SPACE_DESCRIPTOR:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "crs_large_res: Dword address space "
			"descriptor not supported\n");
#endif
		break;
	case WORD_ADDR_SPACE_DESCRIPTOR:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "crs_large_res: Word address space "
			"descriptor not supported\n");
#endif
		break;
	case QWORD_ADDR_SPACE_DESCRIPTOR:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "crs_large_res: QWORD_ADDR_SPACE_"
			"DESCRIPTOR not supported\n");
#endif
		break;
	/*
	 * bootconf doesn't use extended irq descriptor
	 * it will be used only when APIC is enabled
	 */
	case EXTENDED_IRQ_DESCRIPTOR:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "crs_large_res: Extended IRQ descriptor "
			"not supported\n");
#endif
		break;
	case VENDOR_DEFINED_LARGE:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "crs_large_res: VENDOR_DEFINED_LARGE "
			"not supported\n");
#endif
		break;
	}
	return (bp);
}

Board *
crs_small_res(Board *bp, u_char *cp, u_char tag)
{
	u_char dmas;
	u_char dma;
	u_short irqs;
	u_int irq;
	ulong base, len;
	u_short flag;

#ifdef	ACPI_DEBUG
	acpi_debug(ACPI_DRES, "crs_small_res entered cp=%lx tag=%x\n", cp, tag);
	acpi_pause(ACPI_DRES);
#endif
	switch (tag & 0x78) {
	case FIXED_LOCATION_IO_DESCRIPTOR:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "fixed location i/o descriptor\n");
#endif
		len = (ulong) *(cp + 3);
		if (len) {
			base = (ulong) *(u_short *)(cp + 1);
			bp = acpi_add_res(bp, RESF_Port, base, len, 0);
		}
		break;

	case IO_PORT_DESCRIPTOR:
		/*
		 * This io port is in the configured resources
		 * So the start and end better be the same
		 */
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "i/o port descriptor\n");
#endif
		len = (ulong) *(cp + 7);
		if (len) {
			base = (ulong) *(u_short *)(cp + 2);
			bp = acpi_add_res(bp, RESF_Port, base, len, 0);
		}
		break;

	case DMA_FORMAT:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "dma descriptor\n");
#endif
		flag = ((cp[2] & 0x03) ? RES_DMA_SIZE_16B : 0) |
			(((unsigned short) cp[2] & 0x60) << 7);
		for (dmas = *(cp + 1); dmas; dmas ^= (1 << dma)) {
			dma = ffbs((ulong) dmas) - 1;
			bp = acpi_add_res(bp, RESF_Dma, (ulong) dma, 1, flag);
		}
		break;

	case IRQ_FORMAT:
#ifdef	ACPI_DEBUG
		acpi_debug(ACPI_DRES, "irq descriptor\n");
#endif
		len = tag & 0x7;
		if (len == 3) {
			flag = ((cp[3] & 0x10) ? RES_IRQ_SHARE : 0) |
				((cp[3] & 0x01) ? RES_IRQ_TRIGGER : 0);
		} else
			flag = 0;
		for (irqs = *(u_short *)(cp + 1); irqs; irqs ^= (1 << irq)) {
			irq = ffbs((ulong) irqs) - 1;
			bp = acpi_add_res(bp, RESF_Irq, (ulong) irq, 1, flag);
		}
		break;
	}
	return (bp);
}

/*
 * Add a resource to the given function
 */
Board *
acpi_add_res(Board *bp1, unsigned short type, ulong start, ulong len,
		unsigned short eflag)
{
	unsigned short flags;
	Board *bp;
	Resource *rp;
	int i;

	flags = type & ~RESF_TYPE;
	type &= RESF_TYPE;
	bp = acpi_resize_board(bp1, sizeof (Resource));

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
	rp->base = start;
	rp->length = len;
	rp->EISAflags = eflag;
	rp->flags |= type | flags;
	bp->rescnt[type-1]++;

	return (bp);
}

/*
 * Return bit position of least significant bit set in argument,
 * starting numbering from 1.
 */
int
ffbs(u_int x)
{
	int j, n = (x != 0);

	static unsigned int mask[5] = {
	    0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF
	};

	j = 5;
	while (x && j--) {
		unsigned int m = mask[j];

		if (x & m) x &= m;
		else n += (1 << j);
	}

	return (n);
}
