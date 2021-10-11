/*
 * Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)dptcmd.c	1.27	98/06/04 SMI\n"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name:	DPT PM2011/9x, PM2012/9x SCSI HBA	(dptcmd.c)
 *
 */

#include <types.h>
#include <dev_info.h>
#include <bef.h>
#include <befext.h>
#include <stdio.h>
#include "..\scsi.h"
#include "dpt.h"

/*
#define DEBUG
#define PAUSE
*/

#ifdef DEBUG
    #pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
    #pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
# define Dprintf(x) printf x
# ifdef PAUSE
#  define Dpause() { printf("<<PRESS ANY KEY>>"); kbchar(); printf("\n"); }
# else
#  define Dpause()
# endif
#else
# define Dprintf(x)
# define Dpause()
# ifdef PAUSE
#  pragma message ( __FILE__ ": <<WARNING! PAUSE is on but DEBUG is NOT!>>" )
# endif
#endif

#pragma comment ( compiler )
#pragma comment ( user, "dptcmd.c	1.27	98/06/04" )


char ident[] = "DPT";
#define MAXADAPTERS	16
#define DPT_VID		0x1044
#define DPT_2024	0xA400
#define EISA_OFFSET 0xC88

int num_adapters = 0;
struct {
	ushort iobase;		/* Base address */
	ushort device;		/* Device ID (PCI) or base address (others) */
	ushort boardtype;	/* ISA, EISA or PCI type board */
	ushort pci_vendor_id;	/* Vendor ID for PCI board */
	ushort pci_model_id;	/* Model ID for PCI board */
	unchar pci_bus;		/* Bus number for PCI board */
	unchar pci_device;	/* Device number for PCI board */
}
dpt_info[MAXADAPTERS] = { 0 };

struct pinquiry_data {
	struct inquiry_data inqd;
	char	reserved[256];
} pinqd;

#define	DEV_INDEX(i)	((i)->junk[0]) 		/* index to dpt_info */
#define	DEV_CHAN(i)	((i)->junk[1]) 		/* channel */

int	dpt_index = 0;
int	dpt_chan = 0;

extern ushort		devs;
extern DEV_INFO		dev_info[];


#define ISABOARD	1
#define EISABOARD	2
#define PCIBOARD	3

/* These definitions provide the stack for resident code.
 * They are used by low.s.
 */
#define STACKSIZE	2000	/* Resident stack size in words */
ushort stack[STACKSIZE];
ushort stacksize = STACKSIZE;

/* ushort dpt_possible_isa_bases[] = { 0x1F0, 0x170, 0x330, 0x230, 0 };*/
/*
 * dmick - other bases removed because they upset the usc.bef, for
 * which we have no source
 */
ushort dpt_possible_isa_bases[] = { 0x1F0, 0x230, 0 };
static ushort devtab[] = { DPT_2024 };
static int devtab_items = sizeof devtab / sizeof devtab[0];

DPT_CCB dpt_ccb;
DPT_STAT statbuf;
struct ReadConfig rcfg;

#define LongAddr(x, y) longaddr((ushort)(x), (ushort)(y))
void milliseconds();

/* ---- Start of Prototype Area ---- */
void find_pci_ctlrs();
void find_eisa_ctlrs();
void find_isa_ctlrs();
dpt_init_cmd(ushort, DPT_CCB *);
static int dpt_ISAProbe(short, short *, short *);
static bzero(unchar *, ushort);
static addr3(unchar *, ushort, ushort);
static size2(unchar *, ushort);
static size3(unchar *, ushort, ushort);
static size4(unchar *, ushort, ushort);
dpt_EATA_ReadConfig(ushort, BYTE *);
repinsw(ushort, ushort *, ushort);
repoutsw(ushort, ushort *, ushort);
#ifdef DEBUG
static putdat(unchar *, ushort);
#endif
/* ---- End of Prototype Area ---- */

#define NameName	"name"
#define PortName	"port"
#define DmaName		"dma"
#define AddrName	"addr"
#define IrqName		"irq"
#define SlotName	"slot"
#define SetRes1(Res) SetRes2(Res, 0)
#define SetRes2(Res, v1) {						\
	val[0] = Res; val[1] = v1; val[2] = 0;				\
	len = Res##TupleSize;						\
	switch (set_res(Res##Name, val, &len, 0)) {			\
		case RES_FAIL: node_op(NODE_FREE); return;		\
		case RES_CONFLICT: node_op(NODE_FREE); continue;	\
	}								\
}

void
legacyprobe()
{
	int	i;
	short	Port, Dma, Irq;
	DWORD	val[3], len;

	/* ---- loop through the possible ports look for a board ---- */
	for (i = 0; Port = dpt_possible_isa_bases[i]; i++) {
		if (node_op(NODE_START) == NODE_FAIL)
		  return;

		/* ---- the port size might need to change ---- */
		SetRes2(Port, HA_AUX_STATUS + 1);
		if (dpt_ISAProbe(Port, &Dma, &Irq)) {
			if (Irq) SetRes1(Irq);
			SetRes1(Dma);
			node_op(NODE_DONE);
		} else
		  	node_op(NODE_FREE);
	}
}
#undef SetRes1
#undef SetRes2

#define GetRes(Res) {							\
	len = Res##TupleSize;						\
	if (get_res(Res##Name, val, &len) == RES_FAIL) {		\
		printf("Can't find %s resource!\n", #Res);		\
		node_op(NODE_FREE);					\
		return (BEF_FAIL);					\
	}								\
	Res = val[0];							\
}

installonly()
{
	DWORD	val[3], len;
	int	Rtn = BEF_FAIL;
	unsigned Name, Port, Bus, Slot;
	unsigned long vvvvdddd, Addr;

	do {
		if (node_op(NODE_START) == NODE_FAIL)
		  	return (Rtn);

		/* ---- special case. Bus type is stored in name res ---- */
		GetRes(Name); Bus = val[1];

		switch (Bus) {
		      case RES_BUS_PCI:
			vvvvdddd = val[0];
			GetRes(Port);
			GetRes(Addr);

			/* ---- from find_pci_ctlrs() ---- */
			Port += 0x10;
			dpt_info[num_adapters].iobase = Port;
			dpt_info[num_adapters].device = Addr;
			dpt_info[num_adapters].boardtype = PCIBOARD;
			dpt_info[num_adapters].pci_vendor_id = vvvvdddd >> 16;
			dpt_info[num_adapters].pci_model_id = vvvvdddd & 0xffff;
			dpt_info[num_adapters].pci_bus = (Addr >> 8) & 0xff;
			dpt_info[num_adapters].pci_device = (Addr >> 3) & 0x1f;

			break;
			
		      case RES_BUS_ISA:
			GetRes(Port);
			dpt_info[num_adapters].iobase = Port;
			dpt_info[num_adapters].device = Port;
			dpt_info[num_adapters].boardtype = ISABOARD;
			break;

		      case RES_BUS_EISA:
			GetRes(Slot);
			Port = (Slot << 12) | EISA_OFFSET;
			dpt_info[num_adapters].iobase = Port;
			dpt_info[num_adapters].device = Port;
			dpt_info[num_adapters].boardtype = EISABOARD;
			break;
		}
		dpt_initboard(num_adapters++);
		node_op(NODE_DONE);
		
		Rtn = BEF_OK;	/* .... installed at least one board */
	} while (1);
}
#undef GetRes

ulong
dpt_swap4(long q)
{
	union halves answer;
	union halves q1;

	q1.l = q;
	answer.s[1] = (((q1.s[0] & 0xFF) << 8) | (q1.s[0] >> 8));
	answer.s[0] = (((q1.s[1] & 0xFF) << 8) | (q1.s[1] >> 8));
	return (answer.l);
}

dpt_send_cmd(port, addrlo, addrhi, command)
ushort port;
ushort addrlo, addrhi;
ushort command;
{
	Dprintf(("dpt_send_cmd: port %x, addr %x.%x, cmd %d\n",
		port, addrhi, addrlo, command));
	outb(port + 2, addrlo);
	outb(port + 3, addrlo >> 8);
	outb(port + 4, addrhi);
	outb(port + 5, addrhi >> 8);
	outb(port + 7, command);
}

/*
 * dpt_init_cmd -- Execute a SCSI command during init time (no interrupts)   
 */
dpt_init_cmd(ushort port,DPT_CCB *ccbp)
{

   int ret  = 0;
   int index;

   /* Some PCI calls will give the PCI device code rather than the
    * port address.  Do a translation first.
    */
   for (index = 0; index < num_adapters; index++) {
	if (dpt_info[index].boardtype == PCIBOARD &&
		dpt_info[index].device == port) {
		Dprintf(("dpt_init_cmd: translating device %x to %x\n",
			port, dpt_info[index].iobase));
	    port = dpt_info[index].iobase;
	    break;
	}
   }

   Dprintf(("dpt_init_cmd: command on port %x\n", port));
   ccbp->SCSI_status = HA_SELTO;

   /* Wait for controller not busy */
   if (dpt_wait(port + HA_STATUS, HA_ST_BUSY, 0, HA_ST_BUSY)) {
	   Dprintf(("dpt_init_cmd: status stayed busy\n"));
	   return(1);
   }

   if (ccbp->cp_option & HA_CP_QUICK) {
      /* ### Can we remove the intr disable/enable? */
      intr_disable();
      outb(port + HA_COMMAND, CP_PIO_CMD);

      /* Wait for DRQ Interrupt       */
      if (dpt_wait(port + HA_STATUS, 0xFF, HA_ST_DATA_RDY, 0)) {
         intr_enable();
         Dprintf(("dpt_init_cmd: never saw DRQ\n"));
         return(1);
      }

      /* Send the Command Packet      */
      repoutsw(port+HA_DATA, (ushort *)ccbp, (sizeof(EATA_CP)+1)/2 );
      outb(port + HA_STATUS, CP_TRUCATE_CMD);

      /* Wait for Command Complete Interrupt       */
      if (dpt_wait(port + HA_AUX_STATUS, HA_AUX_INTR, HA_AUX_INTR, 0)) {
         ret = 1;
      }
      else {
         ccbp->SCSI_status = 0;
         ccbp->ctlr_status = inb(port + HA_STATUS) & HA_ST_ERROR;
      }
      intr_enable();
   }
   else {
      if (dpt_wait(port + HA_AUX_STATUS, HA_AUX_BUSY, 0, HA_AUX_BUSY)) {
	      Dprintf(("dpt_init_cmd: dpt_wait not busy failed\n"));
	      ret = 1;
      }
      else {
         intr_disable();
         dpt_send_cmd ( port, ccbp->ccb_addr, CP_DMA_CMD );

         /* Wait for Command Complete Interrupt       */
         if (dpt_wait(port+HA_AUX_STATUS, HA_AUX_INTR, HA_AUX_INTR, 0)) {
		 Dprintf(("dpt_init_cmd: dpt_wait for interrupt failed\n"));
		 ret = 1;
         }
         else {
            register DPT_STAT *spp = (DPT_STAT *)&statbuf;
            ccbp->ctlr_status = spp->sp_hastat & HA_STATUS_MASK;
            ccbp->SCSI_status = spp->sp_SCSI_stat;
            if (ccbp->ctlr_status || ccbp->SCSI_status) {
		    Dprintf(("dpt_init_cmd: ctlr_status %x, SCSI_status %x\n",
			    ccbp->ctlr_status, ccbp->SCSI_status));
		    ret = 1;
	    }
         }
         intr_enable();
      }
   }

   if (inb(port + HA_STATUS) & HA_ST_ERROR)
      ret = 1;

   Dprintf(("dpt_init_cmd: end status was %x\n", inb(port + HA_STATUS)));
   Dpause();

   return(ret);

}

/******************************************************************************
** dpt_wait - Wait for a controller register to achieve a specific state.    **
**    Arguments : port, bit mask and two sub-masks                           **
**    To return normally, all the bits in the first sub-mask must be ON,     **
**    all the bits in the second sub-mask must be OFF.                       **
**    If 15 seconds pass without the controller achieving the desired bit    **
**    configuration, we return 1, else  0.                                   **
******************************************************************************/
dpt_wait ( register ushort port, ushort mask, ushort onbits, ushort offbits )
{
   register int i;

   for (i=15000; i;  i--)  {
      register ushort maskval = inb(port) & mask;

      if (((maskval & onbits)== onbits) && ((maskval & offbits)== 0))
         return(0);
      milliseconds(1);
#ifdef DEBUG
	putchar('.');
#endif
   }

   return(1);
}

dev_read(DEV_INFO *info, long start_block, ushort count, ushort bufo,
	 ushort bufs)
{
   register DPT_CCB      *ccbp = &dpt_ccb;
   long totbytes;
   register int i;

   /* Quick and dirty hack to allow long result of multiply */
   for (totbytes = 0, i = 0; i < count; i++) {
      totbytes += info->MDBdev.scsi.bsize;
   }

   bzero ( (unchar *)ccbp, sizeof(DPT_CCB) );
   ccbp->cp_cdb[0]            = SX_READ;
   ccbp->cp_cdb[1]	      = info->MDBdev.scsi.lun << 5;
   *(ulong *)&ccbp->cp_cdb[2] = dpt_swap4(start_block);
   ccbp->cp_cdb[7]            = count >> 8;
   ccbp->cp_cdb[8]            = count & 0xFF;
   ccbp->cp_option            = HA_DATA_IN;
/*   ccbp->cp_option          = HA_DATA_IN + HA_AUTO_REQSEN;   */
   ccbp->cp_dataLen           = dpt_swap4(totbytes);
   ccbp->cp_dataDMA           = dpt_swap4( LongAddr(bufo, bufs));
   ccbp->cp_reqLen            = DPT_SENSE;
   ccbp->cp_reqDMA            = dpt_swap4( LongAddr(ccbp->ccb_sense, myds()) );
   ccbp->cp_statDMA           = dpt_swap4( LongAddr(&statbuf, myds()) );
   ccbp->cp_vp                = ccbp;
   ccbp->cp_vpseg             = myds();
   ccbp->ccb_addr             = LongAddr(ccbp, myds());
   ccbp->cp_id                = info->MDBdev.scsi.targ | DEV_CHAN(info) << 5;
   ccbp->cp_msg0              = HA_IDENTIFY_MSG + info->MDBdev.scsi.lun;

   for (i=1;; i++) {
      if (dpt_init_cmd(info->base_port, ccbp) == 0)
         break;
      if (i >= 2)
         return (UNDEF_ERROR);
   }
   return (0);
}

dev_init(DEV_INFO *info)
{
	Dprintf(("dev_init: not implemented yet\n"));
	return (0);
}

dev_sense ( ushort base_port, ushort targid, ushort lun )
{
   register DPT_CCB      *ccbp = &dpt_ccb;
   register int i;
   BYTE chan = 0;

#ifdef WORKING
   /* ### For some reason SC_RSENSE does not work properly.
    * We seem to get good sense data but we also overwrite
    * a portion of memory.  For now just do nothing.
    */
   bzero ( (unchar *)ccbp, sizeof(DPT_CCB) );
   ccbp->cp_cdb[0]      = SC_RSENSE;
   ccbp->cp_cdb[1]	= lun << 5;
   ccbp->cp_cdb[4]      = DPT_SENSE;
   ccbp->cp_option      = HA_DATA_IN;
   ccbp->cp_dataLen     = dpt_swap4((ulong)DPT_SENSE);
   ccbp->cp_dataDMA     = dpt_swap4( LongAddr(ccbp->ccb_scratch, myds()) );
   ccbp->cp_reqLen      = DPT_SENSE;
   ccbp->cp_reqDMA      = dpt_swap4( LongAddr(ccbp->ccb_sense, myds()) );
   ccbp->cp_statDMA     = dpt_swap4( LongAddr(&statbuf, myds()) );
   ccbp->cp_vp          = ccbp;
   ccbp->cp_vpseg       = myds();
   ccbp->ccb_addr       = LongAddr(ccbp, myds());

   /*
    * channel is saved globally and in dev_info structure; look
    * in both to guess which channel is desired.
    */
   if (dpt_chan)
	chan = dpt_chan;
   if (DEV_CHAN(&dev_info[devs]))
	chan = dpt_chan;
	
   ccbp->cp_id            = targid | chan << 5;
   ccbp->cp_msg0        = HA_IDENTIFY_MSG + lun;

   for (i=0;; i++) {
      if (dpt_init_cmd(base_port, ccbp) == 0)
         break;
      if (i >= 2) {
         return (1);
      }
   }
# ifdef DEBUG
   printf("dev_sense: sense data ");
   putdat(ccbp->ccb_scratch, DPT_SENSE);
# endif
#endif
   return (0);
}

static
bzero(unchar *p, ushort n )
{
   for (; n > 0; n--)
      *p++ = 0;
}

static
addr3(unchar *home, ushort offset, ushort segment)
{
   unsigned short x;

   x = segment + (offset >> 4);
   home[0] = (x >> 12);
   home[1] = (x >> 4);
   home[2] = ((x << 4) | (offset & 0xF));
}

static
size2(unchar *home, ushort size)
{
   home[0] = (size >> 8);
   home[1] = (size & 0xFF);
}

static
size3(unchar *home, ushort sizel, ushort sizeh)
{
   home[0] = (sizeh & 0xFF);
   home[1] = (sizel >> 8);
   home[2] = (sizel & 0xFF);
}

static
size4(unchar *home, ushort sizel, ushort sizeh )
{
   home[0] = (sizeh >> 8);
   home[1] = (sizeh & 0xFF);
   home[2] = (sizel >> 8);
   home[3] = (sizel & 0xFF);
}

dev_readcap(DEV_INFO *info)
{
   register DPT_CCB      *ccbp = &dpt_ccb;
   register int i;
   extern struct readcap_data readcap_data;

   bzero ( (unchar *)ccbp, sizeof(DPT_CCB) );
   ccbp->cp_cdb[0]        = SX_READCAP;
   ccbp->cp_cdb[1]	  = info->MDBdev.scsi.lun << 5;
   ccbp->cp_option        = HA_DATA_IN;
/*   ccbp->cp_option      = HA_DATA_IN + HA_AUTO_REQSEN;   */
   ccbp->cp_dataLen       = dpt_swap4((ulong)sizeof(struct readcap_data));
   ccbp->cp_dataDMA       = dpt_swap4( LongAddr(&readcap_data, myds()) );
   ccbp->cp_reqLen        = DPT_SENSE;
   ccbp->cp_reqDMA        = dpt_swap4( LongAddr(ccbp->ccb_sense, myds()) );
   ccbp->cp_statDMA       = dpt_swap4( LongAddr(&statbuf, myds()) );
   ccbp->cp_vp            = ccbp;
   ccbp->cp_vpseg         = myds();
   ccbp->ccb_addr         = LongAddr(ccbp, myds());
   ccbp->cp_id            = info->MDBdev.scsi.targ | DEV_CHAN(info) << 5;
   ccbp->cp_msg0          = HA_IDENTIFY_MSG + info->MDBdev.scsi.lun;

   for (i=0;; i++) {
      if (dpt_init_cmd(info->base_port, ccbp) == 0)
         break;
      if (i >= 2)
         return (1);
   }
   return (0);
}

dev_motor(DEV_INFO *info, int start)
{
   register DPT_CCB      *ccbp = &dpt_ccb;
   register int i;
   extern struct readcap_data readcap_data;

   bzero ( (unchar *)ccbp, sizeof(DPT_CCB) );
   ccbp->cp_cdb[0]        = SC_STRT_STOP;
   ccbp->cp_cdb[1]	  = info->MDBdev.scsi.lun << 5;
   ccbp->cp_cdb[4]        = start;
   ccbp->cp_vp            = ccbp;
   ccbp->cp_vpseg         = myds();
   ccbp->ccb_addr         = LongAddr(ccbp, myds());
   ccbp->cp_id            = info->MDBdev.scsi.targ | DEV_CHAN(info) << 5;
   ccbp->cp_msg0          = HA_IDENTIFY_MSG + info->MDBdev.scsi.lun;

   return (dpt_init_cmd(info->base_port, ccbp));
}

dev_lock(DEV_INFO *info, int lock)
{
   register DPT_CCB      *ccbp = &dpt_ccb;
   register int i;
   extern struct readcap_data readcap_data;

   bzero ( (unchar *)ccbp, sizeof(DPT_CCB) );
   ccbp->cp_cdb[0]        = SC_REMOV;
   ccbp->cp_cdb[1]	  = info->MDBdev.scsi.lun << 5;
   ccbp->cp_cdb[4]        = lock;
   ccbp->cp_vp            = ccbp;
   ccbp->cp_vpseg         = myds();
   ccbp->ccb_addr         = LongAddr(ccbp, myds());
   ccbp->cp_id            = info->MDBdev.scsi.targ | DEV_CHAN(info) << 5;
   ccbp->cp_msg0          = HA_IDENTIFY_MSG + info->MDBdev.scsi.lun;

   return (dpt_init_cmd(info->base_port, ccbp));
}

init_dev(ushort base_port, ushort dataseg)
{
}

dev_inquire ( ushort base_port, ushort targid, ushort lun )
{
   register DPT_CCB      *ccbp = &dpt_ccb;
   extern struct inquiry_data inqd;
   BYTE chan = 0;

   Dprintf(("dev_inquire() target %d, lun %d\n", targid, lun));

   bzero ( (unchar *)ccbp, sizeof(DPT_CCB) );
   ccbp->cp_cdb[0]        = SC_INQUIRY;
   ccbp->cp_cdb[1]	  = lun << 5;
   ccbp->cp_cdb[4]        = sizeof(struct inquiry_data);
   ccbp->cp_option        = HA_DATA_IN;
/*   ccbp->cp_option      = HA_DATA_IN + HA_AUTO_REQSEN;   */
   ccbp->cp_dataLen       = dpt_swap4((ulong)sizeof(struct inquiry_data));
   ccbp->cp_reqLen        = DPT_SENSE;
   ccbp->cp_vp            = ccbp;
   ccbp->cp_vpseg         = myds();

   ccbp->cp_reqDMA        = dpt_swap4( LongAddr(ccbp->ccb_sense, myds()) );
   ccbp->cp_dataDMA       = dpt_swap4( LongAddr(&pinqd, myds()) );
   ccbp->cp_statDMA       = dpt_swap4( LongAddr(&statbuf, myds()) );
   ccbp->ccb_addr         = LongAddr(ccbp, myds());

   /*
    * channel is saved globally and in dev_info structure; look
    * in both to guess which channel is desired.
    */
   if (dpt_chan) {
	chan = dpt_chan;
#ifdef DEBUG
   	Dprintf(("dpt_chan %x\n", chan));
#endif
   }
   if (DEV_CHAN(&dev_info[devs])) {
	chan = DEV_CHAN(&dev_info[devs]);
#ifdef DEBUG
   	Dprintf(("dpt_chan %x\n", chan));
#endif
   }

   ccbp->cp_id            = targid | chan << 5;
   ccbp->cp_msg0          = HA_IDENTIFY_MSG + lun;

   inqd.inqd_pdt    = INQD_PDT_NOLUN;

   if ( dpt_init_cmd(base_port, ccbp) ) {
      /* Problem with 'inquiry' ? NOTE : Some devices (notably
       * the Kodak 6800 WORM drive) violate the SCSI spec and
       * return UNIT ATTENTION on an INQUIRY.  To work around
       * this 6800 problem we try to do the inquiry TWICE unless
       * the board indicates that we timed out.
       */

	   Dprintf(("inquire data (first inquiry failed):\n"));
      if (ccbp->ctlr_status == HA_SELTO) {
	      Dprintf(("inquire data (failed /w selection timeout):\n" ));
	      return (2);
      }

      if (dpt_init_cmd(base_port, ccbp) ) {
	      Dprintf(("inquire data (second retry inquiry failed):\n"));
	      return (1);
      }
   }

   inqd = pinqd.inqd;
   if (inqd.inqd_pdt == INQD_PDT_NOLUN) {
	   Dprintf(("inquire data (no such lun):\n"));
	   return (1);
   }

   Dprintf(("inquire data (successful inquiry):\n"));
   Dprintf(("peripheral type: %x\ndevice type: %x\n vendor string: %s\n",
	   inqd.inqd_pdt, inqd.inqd_dtq, inqd.inqd_vid));
   Dpause();
   return (0);
}

dev_find()
{
   int boardnum;

   Dprintf(("%s dev_find: dpt_ccb is %x\n", &dpt_ccb));
   find_pci_ctlrs();
   find_eisa_ctlrs();
   find_isa_ctlrs();

   for (boardnum = 0; boardnum < num_adapters; boardnum++) {
      dpt_initboard(boardnum);
   }
}

dpt_ispresent ( ushort port )
{
   register int status;
   register int retry = 2;
   long     loopc = 50000L;

   status = inb( port + HA_STATUS );

   /* If controller is present and Sane then it should be presenting   *
   ** SeekComplete and Ready.  A 0xFF is invalid and usually indicates *
   ** a tri-stated bus indicating no card present.                     */

   Dprintf(("dpt_ispresent: looking for controller at %x\nstatus port: 0x%x\n",
	   port, status));
   Dpause();

   if (status == 0xFF) {
	   Dprintf(("Tri-state bus at %x\n", port + HA_STATUS));
	   return(0);
   }

   if (inb(port + HA_AUX_STATUS) == 0xFF) {
	   Dprintf(("Tri-state bus at %x\n", port + HA_AUX_STATUS));
	   return(0);
   }

   status &= 0xFE;           /* Ignore any last cmd errors */

   do {

      for (loopc = 0;; loopc++) {
         status = inb( port + HA_STATUS ) & 0xFE;
         if (status == HA_ST_SEEK_COMP + HA_ST_READY) {
		 Dprintf(("DPT is present at %x\n", port));
		 return (1);
         }
         if (loopc >= 1000)
            break;
         milliseconds(1);
      }

      /* If any retries Issue a hard reset command to the controller.  */
      if (retry--) {
	 Dprintf(("DPT is being reset at %x\n", port));
         outb(port + HA_COMMAND, CP_EATA_RESET );
         /* Delay 2 seconds for the ctlr to settle */
         for (loopc=0; loopc < 2000; loopc++)
             milliseconds(1);
      }

   } while (retry);

   Dprintf(("DPT is not present at %x\n", port));
   return(0);

}

void
dpt_intr_enable(ushort base_port)
{
	/* Clear any pending interrupt */
	(void)inb(base_port + HA_STATUS);

	/* Wait for controller not busy */
	if (dpt_wait(base_port + HA_STATUS, HA_ST_BUSY, 0, HA_ST_BUSY)) {
		Dprintf(("dpt_intr_enable: status stayed busy before cmd\n"));
		return;
	}

	/* Send an EATA immediate enable command */
	outb(base_port + HA_IMMED_MOD, CP_EI_MOD_ENABLE);
	outb(base_port + HA_COMMAND, CP_EATA_IMMED);

	/* Wait for controller not busy */
	if (dpt_wait(base_port + HA_STATUS, HA_ST_BUSY, 0, HA_ST_BUSY)) {
		Dprintf(("dpt_intr_enable: status stayed busy after cmd\n"));
		return;
	}
}

/*
 * dpt_initboard - Initialize DPT SCSI Host Adapter.
 */
dpt_initboard ( ushort index )
{
   register DPT_CCB      *ccbp = &dpt_ccb;
   struct   ReadConfig   *EATAcfg= (struct ReadConfig *)(ccbp->ccb_scratch);
   struct   inquiry_data *inq    = (struct inquiry_data *)(ccbp->ccb_scratch);
   BYTE    ha_id;
   int      dev_id, lun, max_targ, max_lun;
   int	ret;
   ushort  base_port = dpt_info[index].iobase;
   char *vpath = "pci1044,a400@";
   char *mpath = "mscsi@0,0";
   char path[64];
   char *bp = path;
   int chan, maxchan;
   int slot;
   int i;

   if (dpt_ispresent(base_port)==0) {
	Dprintf(("Don't see dpt at %X\n", base_port));
	return (0);
   }

   /* Get Host Adapter Specific Info  */
   if (dpt_EATA_ReadConfig(base_port, (char *)EATAcfg)) {
	Dprintf(("Can't read config info\n"));
	return (0);
   }

   /* If DMA is not supported then give error, if Primary Ctlr PANIC */
   if ( !EATAcfg->DMAsupported) {
	   Dprintf(("DPT does not support BusMaster DMA\n"));
	   return (0);
   }

   /* If this card requires a DMA Channel then set it up now.        */
   /* Also this means its an ISA Bus Master !           */
   if (EATAcfg->DMAChannelValid) {
      dpt_DMA_Setup(EATAcfg->DMA_Channel);
   }

   /* Find the SCSI ID of the adapter */
   ha_id = EATAcfg->HBA[3];

   Dprintf(("Found DPT %s SCSI adapter /w addr %x\n",
	   dpt_info[index].boardtype == PCIBOARD ? "PCI" :
	   dpt_info[index].boardtype == EISABOARD ? "EISA" : "ISA",
	   base_port));
   dpt_intr_enable(base_port);

   /*********************************************************************
   ** Now loop through the installed devices, checking device types.   **
   *********************************************************************/

   if (EATAcfg->MaxChannel)
	maxchan = EATAcfg->MaxChannel + 1;
   else
	maxchan = 1;

   max_targ = EATAcfg->MaxScsiID;
   max_lun  = EATAcfg->MaxLUN;
#ifdef DEBUG
   Dprintf(("MaxChannel %x maxchan %x\n", EATAcfg->MaxChannel, maxchan));
#endif

   for (chan = 0; chan < maxchan; chan++) {
    for (dev_id=0;  dev_id < max_targ;  dev_id++)  {
      if (dev_id == ha_id)  {       /* Skip controller's SCSI ID */
         continue;
      }


      for (lun=0; lun < max_lun ; lun++) {   /** Check luns on dev_id      **/

	 dpt_chan = DEV_CHAN(&dev_info[devs]) = chan;
	 dpt_index = DEV_INDEX(&dev_info[devs]) = index;
	 ret = dev_inquire(base_port, dev_id, lun);

	 if (ret == 2)
	    break;
	 if (ret)
	    continue;
	 if (chan) {
		if (dpt_info[index].boardtype == PCIBOARD) {
			slot = dpt_info[index].pci_device;
			/*
			 * Vendor / device part.
			 */
			i = 0;
			while (vpath[i])
				*bp++ = vpath[i++];

			/*
			 * Slot number.
			 */
			if ((slot >> 4) & 0xf)
				*bp++ = ((slot >> 4) & 0xf) + '0';
	
			slot &= 0xF;
			*bp++ = (slot < 10) ? slot + '0' : (slot - 10) + 'a';
			*bp++ = '/';
		}

		/* mscsi bus */
		if (chan < 10)
	    		mpath[6] = '0' + chan;
	    	else
	    		mpath[6] = 'a' + chan - 10;

		i = 0;
		while (mpath[i])
			*bp++ = mpath[i++];
		*bp++ = '\0';
       
		bp = dev_info[devs].user_bootpath;

		i = 0;
		while (path[i])
			*bp++ = path[i++];
		*bp++ = '\0';
	 }
	 if (dpt_info[index].boardtype == PCIBOARD) {
	    scsi_dev_pci(dpt_info[index].device, dev_id, lun,
		dpt_info[index].pci_bus, dpt_info[index].pci_vendor_id,
		dpt_info[index].pci_model_id, dpt_info[index].pci_device, 0);
	 } else {
	    scsi_dev(dpt_info[index].device, dev_id, lun);
	 }
      }   /* For LUN */
    }      /* For ID  */
   }      /* For CHAN  */

   /** Setup for IOCTL interface   **/
   return(1);
}

/* PCI configuration space offsets */
#define PCI_CMDREG      0x04
#define PCI_BASEAD      0x10

/* Bits in PCI_CMDREG */
#define PCI_CMD_IOEN    1

void
find_pci_ctlrs()
{
        int num;
        int dev;
        ushort id;
        ushort cmd_reg;
        ulong address;

        if (!is_pci()) {
                Dprintf(("%s find_pci_ctlrs(): Not a PCI machine\n", ident));
                return;
        }

        Dprintf(("%s find_pci_ctlrs(): Looking for PCI devices\n", ident));
        for (dev = 0; dev < devtab_items; dev++) {
                for (num = 0; ; num++) {
                        if (!pci_find_device(DPT_VID, devtab[dev], num, &id)) {
                                break;
                        }
                        if (!pci_read_config_word(id, PCI_CMDREG, &cmd_reg))
                                continue;

                        /*
                         * Assume device is disabled if I/O space is
                         * disabled.
                         */
                        if ((cmd_reg & PCI_CMD_IOEN) == 0)
                                continue;

                        if (!pci_read_config_dword(id, PCI_BASEAD, &address))
                                continue;
                        address &= ~1;
			address += 0x10;

                        Dprintf(("%s find_pci_ctlrs(): found device %x, number %x at address %x\n", ident, devtab[dev], num, address));
			Dpause();

                        if (num_adapters < MAXADAPTERS) {
                                dpt_info[num_adapters].iobase =
                                        (ushort)address;
                                dpt_info[num_adapters].device = id;
                                dpt_info[num_adapters].boardtype = PCIBOARD;
				dpt_info[num_adapters].pci_vendor_id = DPT_VID;
				dpt_info[num_adapters].pci_model_id =
					devtab[dev];
				dpt_info[num_adapters].pci_bus =
					(id >> 8) & 0xFF;
				dpt_info[num_adapters].pci_device =
					(id >> 3) & 0x1F;

                                num_adapters++;
                        }
                }
        }
        Dprintf(("%s find_pci_ctlrs(): Finished looking for PCI devices\n",
		ident));
}

void
find_eisa_ctlrs()
{
   ushort slotnum = 0;
   register ushort slot = EISA_OFFSET;
   BYTE     lookForPrimary = 1;
   
#ifdef DEBUG
   putstr(ident);
   putstr(" find_eisa_ctlrs(): Looking for EISA devices\r\n");
#endif
   /********************************************************************
   ** Scan through the EISA slot address looking for our ID's.  First **
   **   a complete pass needs to be made looking for a PRIMARY ctlr,  **
   **   since the boot ctlr could have a higher EISA address than any **
   **   SECONDARY ctlrs.  We must configure the PRIMARY FIRST !      **
   **   Finally, make a pass thru looking for just SECONDARY ctlrs.   **
   ********************************************************************/   
   for ( slotnum++; slotnum <= 0xF; slotnum++) {
       slot = ((slotnum << 12) | EISA_OFFSET);
       if (
           /** Check for DPT ID Pal **/
           (inb(slot-0x08)!=0x12 || inb(slot-0x07)!=0x14) &&
           /** Check for NEC ID Pal on DPT EISA Ctlrs **/
           (inb(slot-0x08)!=0x38 || inb(slot-0x07)!=0xA3 ||
           inb(slot-0x06)!=0x82 || inb(slot-0x05)!=0x01 )) {
          continue;
       }

       if (dpt_EATA_ReadConfig(slot, (BYTE *)&rcfg)) {
          continue;
       }

       if (lookForPrimary && rcfg.Secondary) {
          if (slotnum = 0xF) {
             slotnum = 0;
             lookForPrimary = 0;
          }
          continue;
       }
       if (!lookForPrimary && !rcfg.Secondary) {
          continue;
       }

#ifdef DEBUG
       putstr(ident);
       putstr(" find_eisa_ctlrs(): Found EISA device in slot ");
       put1hex(slotnum);
       putstr("\r\n");
#endif
       if (num_adapters == MAXADAPTERS) {
          break;
       }

       dpt_info[num_adapters].iobase = slot;
       dpt_info[num_adapters].device = slot;
       dpt_info[num_adapters].boardtype = EISABOARD;

       num_adapters++;

       if (lookForPrimary) {
          lookForPrimary = 0;
          slotnum = 0;
          continue;
       }
   }
#ifdef DEBUG
   putstr(ident);
   putstr(" find_eisa_ctlrs(): Finished looking for EISA devices\r\n");
#endif
}

void
find_isa_ctlrs()
{
   int   i;

   Dprintf(("%s find_isa_ctlrs(): Looking for ISA devices\n", ident));

   for (i = 0; dpt_possible_isa_bases[i]; i++)
     	dpt_ISAProbe(dpt_possible_isa_bases[i], 0, 0);

   Dprintf(("%s find_isa_ctlrs(): Finished looking for ISA devices\n", ident));

}

/*
 * dpt_ISAProbe -- see if ISA card is available. returns 1 = yes, 0 = no
 */
static int
dpt_ISAProbe(short Port, short *Dma, short *Irq)
{
	DPT_CCB    		*ccbp = &dpt_ccb;
	struct ReadConfig	*EATAcfg;

	EATAcfg = (struct ReadConfig *)(ccbp->ccb_scratch);
	
	if (!dpt_ispresent(Port)) return (0);

	Dprintf(("Found a possible ISA controller at 0x%x\n", Port));

	/* Get Host Adapter Specific Info.  DMAChannelValid will
	 * be true for an ISA board and false for EISA.
	 * We don't want to "find" a board which is really the
	 * IDE-compatible addresses for an EISA board.
	 */
	if (dpt_EATA_ReadConfig(Port, (char *)EATAcfg)) {
		Dprintf(("dpt_EATA_ReadConfig failed\n"));
		return (0);
	}
	if (!EATAcfg->DMAChannelValid) {
		Dprintf(("It was really an EISA board\n"));
		return (0);
	} else if (Dma) {
	  	/*
		 * Dma channel 5,6,7,0 maps from
		 * 3,2,1,0
		 */
	  
	  	*Dma = (8 - EATAcfg->DMA_Channel) & 7;
	}
	if (Irq) {
		*Irq = EATAcfg->IRQ_Number;
	}
	
	Dprintf(("Really looks like an ISA\n"));
	
	if (num_adapters == MAXADAPTERS)
		return (0);
	
	dpt_info[num_adapters].iobase = Port;
	dpt_info[num_adapters].device = Port;
	dpt_info[num_adapters].boardtype = ISABOARD;
	num_adapters++;
	return (1);
}

/*
 * dpt_EATA_ReadConfig -- Issue an EATA Read Config Command, Process PIO just
 *		in case we have a PM2011 with the ROM disabled, so it can't
 *		do DMA yet.
 */
dpt_EATA_ReadConfig(ushort port, BYTE *buf)
{
   register int padcnt;
   register int loopc;
   struct ReadConfig *r = (struct ReadConfig *)buf;
   int len = 0;
   int ret = 0;

   /* Wait for controller not busy */
   for (loopc = 0;; loopc++) {
      if ((inb(port + HA_STATUS) & HA_ST_BUSY) == 0)
         break;
      if (loopc >= 10) {
#ifdef DEBUG
         putstr("dpt_EATA_ReadConfig: stuck busy\r\n");
#endif
         return (1);
      }
      milliseconds(1);
   }

   /* Send the Read Config EATA PIO Command */
   outb(port + HA_STATUS, CP_READ_CFG_PIO);

#ifdef DEBUG
  putstr("dpt_EATA_ReadConfig: waiting for DRQ interrupt\r\n");
#endif
   /* Wait for DRQ Interrupt       */
   if (dpt_wait(port + HA_STATUS, 0xFF, HA_ST_DATA_RDY, 0)) {
      inb(port + HA_STATUS);
#ifdef DEBUG
      putstr("dpt_EATA_ReadConfig: no DRQ interrupt\r\n");
#endif
      return(1);
   } 
   ret = inb(port + HA_STATUS) & HA_ST_ERROR ? 1 : 0;

   /* Take the Config Data         */
   repinsw(port+HA_DATA, (ushort *)buf, 2);
   len = dpt_swap4(*(long *)r->ConfigLength);
   if (len & 1)
	len++;

   repinsw(port+HA_DATA, (ushort *)buf + 2,
	   len/2 );

   /* Take the remaining data      */
   for (padcnt = (512 - len)/2; padcnt; padcnt--)
      inw(port + HA_DATA);

   ret = inb(port + HA_STATUS) & HA_ST_ERROR ? 1 : 0;
   /*Wrong signature*/
   if ((r->EATAsignature[0] != 'E') || (r->EATAsignature[1] != 'A') ||
       (r->EATAsignature[2] != 'T') || (r->EATAsignature[3] != 'A') || ret) {
#ifdef DEBUG
      putstr("dpt_EATA_ReadConfig: bad signature\r\n");
#endif
      return (1);
   }

   /* If this card requires a DMA Channel then set it up now.        */
   if (r->DMAChannelValid) { 
#ifdef DEBUG
      putstr("dpt_EATA_ReadConfig: setting up DMA\r\n");
#endif
      dpt_DMA_Setup(r->DMA_Channel);
}

   /* If DMA Mode supported then re-issue DMA mode ReadConfig to get */
   /*  any differences when running DMA mode.           */
   if (r->DMAsupported) {

      /* Wait for controller not busy */
      for (loopc = 0;; loopc++) {
         if ((inb(port + HA_STATUS) & HA_ST_BUSY) == 0)
            break;
         if (loopc >= 10) {
#ifdef DEBUG
            putstr("dpt_EATA_ReadConfig: stuck busy(2)\r\n");
#endif
            return (1);
         }
         milliseconds(1);
      }

      dpt_send_cmd(port, LongAddr(buf, myds()), CP_READ_CFG_DMA);

      /* Wait for busy indication */
      for (loopc = 0;; loopc++) {
         if (inb(port + HA_STATUS) & HA_ST_BUSY)
            break;
         if (loopc >= 10) {
#ifdef DEBUG
            putstr("dpt_EATA_ReadConfig: no busy bit\r\n");
#endif
            return (1);
         }
         milliseconds(1);
      }
      for (loopc = 0;; loopc++) {
         if ((inb(port + HA_STATUS) & HA_AUX_INTR) == 0)
            break;
         if (loopc >= 10) {
#ifdef DEBUG
            putstr("dpt_EATA_ReadConfig: no intr\r\n");
#endif
            return (1);
         }
         milliseconds(1);
      }

      ret = inb(port + HA_STATUS) & HA_ST_ERROR ? 1 : 0;
   }   

   /** If using an ISA port address and it's not 1F0 and there was **/
   /** no error then set the secondary bit for EATA Config.   **/
   if ( port < 0x1000 && port != 0x1F0 && ret != 0)
      r->Secondary = 1;

   return(ret);
}

/*
 * dpt_DMA_Setup -- Setup the DMA Channel specified.
 */
dpt_DMA_Setup ( int chan )
{
   register int Channel;

   Channel = (8 - chan) & 7;  /* DMA channel 5,6,7,0 maps from 3,2,1,0 */

   if (Channel < 4) {
      outb(DMA0_3MD, Channel | CASCADE_DMA);
      milliseconds ( 1 );
      outb(DMA0_3MK, Channel);
   }
   else {
      outb(DMA4_7MD, (Channel & 3) | CASCADE_DMA);
      milliseconds ( 1 );
      outb(DMA4_7MK,  Channel & 3);
   }

   return(Channel);
}

repinsw(ushort port, ushort *buffer, ushort count)
{
   while (count-- > 0)
      *buffer++ = inw(port);
}

repoutsw(ushort port, ushort *buffer, ushort count)
{
   while (count-- > 0)
      outw(*buffer++);
}

#ifdef DEBUG
static putdat(unchar *p, ushort n)
{
   while (n-- > 0) {
      put2hex(*p++);
      putchar(' ');
   }
   putstr("\r\n");
}
#endif

