/*
 * Copyright (c) 1996 Sun Microsystems, Inc.  All rights reserved.
 *
 * ieef -- Intel 596 EtherExpress 32 Flash/Unisys Desktop embedded 596
 */
#ident	"@(#)ieef.c	1.24	96/09/26 SMI\n"

typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char unchar;

#include <types.h>
#include <bef.h>
#include <befext.h>
#include <stdio.h>
#include "common.h"
#include "rplagent.h"
#include "ieef.h"

int	splhi(void);
void	splx(int);

#if defined(DEBUG) || defined(DEBUG1) || defined(DEBUG2) || \
    defined(DEBUG3) || defined(DEBUG4) || defined(DEBUG5) || \
    defined(DEBUG6)
void PAK(void) {
	putstr("\r\nPRESS A KEY TO CONTINUE");
	kbchar();
	putstr("\r\n");
}
#endif

/*
*[]------------------------------------------------------------[]
* | Debug Information Section					|
*[]------------------------------------------------------------[]
*/
/* #define DEBUG /* */
#ifdef DEBUG
#pragma message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#define	Dprintf(f, x) if (DebugIeef & IEEF_##f) printf x
#define	IEEF_FLOW	0x0001	/* ... general flow of program */
#define	IEEF_ERR	0x0002	/* ... error conditions */
#define	IEEF_BIOS	0x0004	/* ... data on bios calls */
#define	IEEF_FLOW_IRQ	0x0008	/* ... info. during interrupts */
#define	IEEF_TEST	0x0010	/* ... printf that will not be checked in */
#define	IEEF_MEDIA 0x0020  /* ... to debug just the media detect */
#define	IEEF_ALL	-1	/* ... enable all flags */
int	DebugIeef = IEEF_ALL;
#else
#define	Dprintf(f, x)
#endif
/*
*[]------------------------------------------------------------[]
* | End of debug section
*[]------------------------------------------------------------[]
*/

extern int Network_Traffic;	/* to identify phase of booting */
extern int Media_Type;		/* ethernet or token ring */
InstallSpace(Name, 1);
InstallSpace(Slot, 1);
InstallSpace(Port, 1);
InstallSpace(Irq, 1);
InstallSpace(Mem, 1);
InstallSpace(Dma, 1);

static struct eisaTab {
	unsigned char id[4];
	char *ident;
	ushort slots;
#define	ZERO_ONLY	1
#define	NON_ZERO	2
	ushort hwtype;
} eisaTab[] = {
{ 0x55, 0xc2, 0x00, 0x19, "Unisys596",	ZERO_ONLY, IEEF_HW_UNISYS	},
{ 0x25, 0xd4, 0x10, 0x10, "I32Flash",	NON_ZERO,  IEEF_HW_FLASH	},
{ 0x55, 0xc2, 0x00, 0x48, "UnisysFla",	NON_ZERO,  IEEF_HW_UNISYS_FLASH	},
{ 0x25, 0xd4, 0x10, 0x60, "EEpro100",	NON_ZERO,  IEEF_HW_EE100_EISA	},
};
static numEISAtypes = sizeof (eisaTab) / sizeof (struct eisaTab);

static struct pciTab {
	unsigned short vendor;
	unsigned short product;
	char *ident;
	ushort hwtype;
} pciTab[] = {
	{	0x8086, 0x1227, "EEpro100", IEEF_HW_EE100_PCI	},
};
static numPCItypes = sizeof (pciTab) / sizeof (struct pciTab);

#define	MAX_ADAPTERS	20	/* Maximum number of adapters supported */
struct {
	short index;
	unsigned short hwtype;
	ushort ioaddr;
	ushort pci_id;
	char *ident;
} ieef_tab[MAX_ADAPTERS];
ushort	num_adapters;

/* Tables of supported speeds, in order of preference, 0 terminated */
int	ee100_speed_tab[] = { 100, 10, 0 };

char	ident[12] = "I32Flash",
	driver_name[] = "ieef";
ushort	Inetboot_IO_Addr;	/* correspond to reg=0,0,0 in ieef.conf file */
	Inetboot_Mem_Base;
unchar	MAC_Addr[6];
ushort	Rx_Buf_Len;
int	Listen_Posted = FALSE;
unchar _far	*Rx_Buffer;
recv_callback_t	Call_Back;

/*
 * The 596 uses host memory as its device transmit and receive buffer
 * memory.  This real mode driver uses 1 Tx and 2 Rx buffers, and each
 * buffer is BUFSIZE bytes in size.  This comes to 6K.  Add to it another
 * 1K for the buffer descriptors and such.  So allocate 7K to this driver.
 * This memory has to start from a 16 byte boundary.
 * Memory map:
 * 0-1k: initialization and command data structures, including
 *      scp_t       ieef_scp;
 *      iscp_t      ieef_iscp;
 *      scb_t       ieef_scb;
 *      gen_cmd_t   ieef_cmds[IEEF_NCMDS];
 *      rfd_t       ieef_rfd[IEEF_NFRAMES];
 *      rbd_t       ieef_rbd[IEEF_NFRAMES];
 *      tbd_t       ieef_tbd[IEEF_NXMIT];
 * 1K-3K: 1st receive buffer
 * 3k-5k: 2nd receive buffer
 * 5k-7k: transmit buffer
 */
#define	CMD_AREA  1024
#define	BUFSIZE	  1536
#define	MEM_SIZE  (16 + CMD_AREA + (IEEF_NFRAMES+IEEF_NXMIT)*BUFSIZE)
volatile char board_mem[MEM_SIZE];

int	DebugFlag = 0;

/* Driver variables */
struct ieefinstance	ieef,
			*ieefp = &ieef;
xmit_cmd_t		xcmd;

/* ---- Start of Prototype Area ---- */
int	ieef_adapter_init(struct ieefinstance *);
int	ieef_speek_ok(struct ieefinstance *);
int	ieef_init_board(struct ieefinstance *);
void	ieef_start_board_int(struct ieefinstance *);
void	ieef_start_ru(struct ieefinstance *);
void	ieef_stop_board_int(struct ieefinstance *);
void	ieef_port_ca_nid(struct ieefinstance *);
void	lock_data_rate(struct ieefinstance *);
int	ieef_saddr(unchar *);
int	ieef_alloc_buffers(struct ieefinstance *);
void	ieef_reset_rfa(struct ieefinstance *);
int	ieef_avail_tdb(struct ieefinstance *);
int	ieef_add_command(struct ieefinstance *, gen_cmd_t *, int);
void	ieef_cmd_complete(struct ieefinstance *, short);
void	ieef_xmit_complete(struct ieefinstance *, short);
void	ieef_re_q(struct ieefinstance *, volatile rfd_t *);
void	ieef_process_recv(struct ieefinstance *, short);
void	ieef_configure(struct ieefinstance *, int);
void	ieef_configure_flash32(struct ieefinstance *, int);
void	ieef_configure_ee100(struct ieefinstance *, int);
int	ieef_avail_cmd(struct ieefinstance *);
void	ieef_rfa_fix(struct ieefinstance *);
int	eisaIdMatch(unchar *, ushort);
void 	ieef_detect_media(struct ieefinstance *);
/* ---- End of Prototype Area ---- */

#define	MAX_EISA_SLOT	15
#define	MAX_PCI_INSTANCES	32
#define	PCINUM(x)	((x) - (MAX_EISA_SLOT + 1))
#define	PCIBOARD(x)	(PCINUM(x) / MAX_PCI_INSTANCES)
#define	PCIINDEX(x)	(PCINUM(x) % MAX_PCI_INSTANCES)

#define	PCI_CMDREG	0x04
#define	PCI_BASEAD	0x10
#define	PCI_BASEAD2	0x14
#define	PCI_INTR	0x3C
#define	PCI_CMD_IOEN	1
#define	PCI_CMD_BMASTER	4

/*
 * ISAAddr -- given instance 'idx' return port/size pair
 *		ADATER_TABLE_OK = okay, ADAPTER_TABLE_END = stop
 */
ISAAddr(short idx, short *Port, short *Portsize)
{
	return (ADAPTER_TABLE_END);
}

/*
 * ISAProbe -- given the port see if a non-Plug-and-Play card exists
 *		ADAPTER_FOUND = okay, ADAPTER_NOT_FOUND = fail
 */
ISAProbe(short port)
{
	return (ADAPTER_NOT_FOUND);
}

/*
 * ISAIdentify -- given the port return as much info as possible
 *		routine doesn't get called because ISAProbe always
 *		returns ADAPTER_NOT_FOUND;
 */
void
ISAIdentify(short Port, short *Irq, short *Mem, short *Memsize)
{
}

/*
 * InstallConfig -- setup internal structures for card at given resources
 *			0 = okay, !0 = failure
 */
InstallConfig(void)
{
	int	k;		/* .... general index */
	short	id,		/* .... pci id value */
		j,		/* .... temporary value for checking */
		Addr;		/* .... this is the id value */
	DWORD	val[2], len;

	switch (Name_Space[1]) {
		case RES_BUS_PCI:
		len = 1;
		if (get_res("addr", val, &len) != RES_OK)
			return (1);
		for (k = 0; k < numPCItypes; k++) {
			if (pci_find_device(pciTab[k].vendor,
			    pciTab[k].product, k, &id) && (id == (short)val[0]))
				break;
		}

		if ((k == numPCItypes) ||
		    (pci_read_config_word(id, PCI_CMDREG, &j) == 0) ||
		    ((j & PCI_CMD_IOEN) == 0))
			return (1);

		ieef_tab[num_adapters].index = k;
		ieef_tab[num_adapters].hwtype = pciTab[k].hwtype;
		ieef_tab[num_adapters].ident = pciTab[k].ident;
		ieef_tab[num_adapters].pci_id = id;
		ieef_tab[num_adapters].ioaddr = (ushort)Port_Space[0];
		num_adapters++;
		break;

		case RES_BUS_EISA:
		/*
		 * This cards eisa record doesn't specify a port range
		 * because all of the address within a given range are expected
		 * to be available by eisa cards. The frameworks master file
		 * can have a $port resource added for this card in which
		 * case Port_Space will have the slot number times 0x1000 plus
		 * an offset of 0xc80. This driver wasn't coded to have a port
		 * number with the actual address in it so we'll just ignore
		 * the value here.
		 */

		Port_Space[0] = Slot_Space[0] * 0x1000;

		for (k = 0; k < numEISAtypes; k++) {
			if (((Slot_Space[0] == 0) &&
			    (eisaTab[k].slots == NON_ZERO)) ||
			    ((Slot_Space[0] != 0) &&
			    (eisaTab[k].slots == ZERO_ONLY)))
				continue;

			if (eisaIdMatch(eisaTab[k].id,
					(ushort)Port_Space[0] + 0xc80)) {
				ieef_tab[num_adapters].index =
				    (short)Slot_Space[0];
				ieef_tab[num_adapters].hwtype =
				    eisaTab[k].hwtype;
				ieef_tab[num_adapters].ident =
				    eisaTab[k].ident;
				ieef_tab[num_adapters].ioaddr =
				    (ushort)Port_Space[0];
				num_adapters++;
			}
		}
		break;

		default:
		printf("Bus not handled\n");
		return (1);
	}
	return (0);
}

/*
 * Probe whether there is a recognized adapter at position "index".
 * Returns:
 *	ADAPTER_FOUND		if found
 *	ADAPTER_NOT_FOUND	if not found
 *	ADAPTER_TABLE_END	if this is the last logical "index" number
 *
 * For this driver, index is interpreted as follows:
 *
 *	index <= MAX_EISA_SLOT 		interpret index as an EISA slot
 *					number and return ADAPTER_FOUND
 *					if there is a supported board
 *					in that slot, otherwise return
 *					ADAPTER_NOT_FOUND.
 *
 *	calculate PCIboard and		use PCIboard as an index into a
 *	PCIindex from index		table of supported PCI devices.
 *					Return ADAPTER_TABLE_END if off
 *					the end of the PCI device table.
 *					Otherwise look for instance number
 *					PCIindex of the board.
 *
 *	The above algorithm is designed to uniquely identify any board
 *	without the need to have previously identified any others.
 *
 *	The value of MAX_PCI_INSTANCES above was chosen because it is
 *	greater than the number of lines on a standard x86 console.  We
 *	will get into trouble with the MDB menu long before we have that
 *	many instances of any board.
 */
int
AdapterProbe(short index)
{
	ushort	port;
	ushort	id;
	ushort	temp;
	int	i;
	int	ts;

	/* Look for index already in table */
	for (ts = 0; ts < num_adapters; ts++)
		if (ieef_tab[ts].index == index)
			return (ADAPTER_FOUND);

	/* Might as well stop if the table is full */
	if (num_adapters >= MAX_ADAPTERS)
		return (ADAPTER_TABLE_END);

	if (index < 0)
		return (ADAPTER_TABLE_END);

	if (index <= MAX_EISA_SLOT) {
		/* Index is in EISA range */
		port = (index * 0x1000) + 0xc80;
		for (ts = 0; ts < numEISAtypes; ts++) {
			if (index == 0 && (eisaTab[ts].slots == NON_ZERO))
				continue;
			if (index != 0 && (eisaTab[ts].slots == ZERO_ONLY))
				continue;
			if (eisaIdMatch(eisaTab[ts].id, port)) {
				ieef_tab[num_adapters].index = index;
				ieef_tab[num_adapters].hwtype =
					eisaTab[ts].hwtype;
				ieef_tab[num_adapters].ident =
					eisaTab[ts].ident;
				num_adapters++;
#ifdef DEBUG6
				putstr("ieef: found EISA adapter\r\n");
#endif
				return (ADAPTER_FOUND);
			}
		}
		return (ADAPTER_NOT_FOUND);
	}

	if (!is_pci())
		return (ADAPTER_TABLE_END);

	ts = PCIBOARD(index);
	if (ts < numPCItypes) {
		/* Index is in PCI range.  Look for enabled device */
		if (pci_find_device(pciTab[ts].vendor, pciTab[ts].product,
				PCIINDEX(index), &id) == 0)
			return (ADAPTER_NOT_FOUND);
		if (pci_read_config_word(id, PCI_CMDREG, &temp) == 0)
			return (ADAPTER_NOT_FOUND);
		if ((temp & PCI_CMD_IOEN) == 0)
			return (ADAPTER_NOT_FOUND);
		ieef_tab[num_adapters].index = index;
		ieef_tab[num_adapters].hwtype = pciTab[ts].hwtype;
		ieef_tab[num_adapters].ident = pciTab[ts].ident;
		ieef_tab[num_adapters].pci_id = id;
		num_adapters++;
#ifdef DEBUG6
		putstr("ieef: found PCI adapter\r\n");
#endif
		return (ADAPTER_FOUND);
	}

	return (ADAPTER_TABLE_END);
}

void
AdapterIdentify(short index, ushort *ioaddr, ushort *irq,
		ushort *rambase, ushort *ramsize)
{
	static ushort irqs1[4] = {3, 7, 12, 15}; /* when inb(430) bit7 is set */
	static ushort irqs2[4] = {5, 9, 10, 11}; /* otherwise */
	static ushort UnisysIRQ[4] = {9, 10, 14, 15};	/* Unisys specific */
	unchar	c;
	ushort	i = 0;
	ushort	port;
	ushort	id;
	ushort	temp;
	unchar	conf_byte;
	ulong	ltemp;
	int	ts;
	int	ai;

	/* Look for index in ieef_tab.  Fail if not found */
	for (ai = 0; ai < num_adapters; ai++)
		if (ieef_tab[ai].index == index)
			break;
	if (ai == num_adapters)
		return;

	*rambase = 0;
	*ramsize = 0;

	switch (ieef_tab[ai].hwtype) {
	case IEEF_HW_UNISYS:
		*ioaddr = 0x300;	/* Unisys hardcoded it */
		c = (inb(0x28) & 0xc0) >> 6;	/* get bits 7, 6 */
		*irq = UnisysIRQ[c];
		break;
	case IEEF_HW_UNISYS_FLASH:
		*ioaddr = index * 0x1000;
		c = inb(*ioaddr + 0xc88);
		i = (c & 0x06) >> 1;
		*irq = UnisysIRQ[i];
		break;
	case IEEF_HW_FLASH:
	case IEEF_HW_EE100_EISA:
		*ioaddr = index * 0x1000;
		c = inb(*ioaddr + 0x430);
		if (c & 0x80) {
			/* extended interrupt */
			i = (c & 0x06) >> 1;
			*irq = irqs1[i];
		} else {
			c = inb(*ioaddr + 0xc88);
			i = (c & 0x06) >> 1;
			*irq = irqs2[i];
		}
		break;
	case IEEF_HW_EE100_PCI:
		ts = PCIBOARD(index);
		if (ts >= numPCItypes)
			goto bad_config;
		/* Index is in PCI range.  Look for enabled device */
		if (pci_find_device(pciTab[ts].vendor, pciTab[ts].product,
				PCIINDEX(index), &id) == 0)
			goto bad_config;
		if (pci_read_config_dword(id, PCI_BASEAD2, &ltemp) == 0)
			goto bad_config;
		*ioaddr = (unsigned short)(ltemp & ~1);
		if (pci_read_config_byte(id, PCI_INTR, &conf_byte) == 0)
			goto bad_config;
		if (conf_byte == 0 || conf_byte > 15)
			goto bad_config;
		*irq = conf_byte;
		if (pci_read_config_byte(id, PCI_CMDREG, &conf_byte) == 0)
			goto bad_config;
		if ((conf_byte & PCI_CMD_BMASTER) == 0 &&
				pci_write_config_byte(id, PCI_CMDREG,
					conf_byte | PCI_CMD_BMASTER) == 0)
			goto bad_config;
		net_dev_pci(PCI_COOKIE_TO_BUS(id), pciTab[ts].vendor,
			pciTab[ts].product, PCI_COOKIE_TO_DEV(id),
			PCI_COOKIE_TO_FUNC(id));
		break;
	default:
		return;
	}

bad_config:

	ieef_tab[ai].ioaddr = *ioaddr;
	for (i = 0; i < 9; i++)
		ident[i] = ieef_tab[ai].ident[i];
}

int
AdapterInit(ushort ioaddr, ushort irq, ushort rambase, ushort ramsize)
{
	ushort	ieef_num;
	int	*speed_tab;
	int	speed_index;

#ifdef DEBUG6
	if (DebugFlag) {
		putstr("AdapterInit(), ds=0x");
		puthex(myds());
		putstr(" xcmd=0x");
		putptr((char _far *)&xcmd);
		putstr("\r\n");
	}
#endif

	/* Look for ioaddr in ieef_tab.  Fail if not found */
	for (ieef_num = 0; ieef_num < num_adapters; ieef_num++) {
		Dprintf(FLOW, ("ieef_tab[%d].ioaddr 0x%x, Port = 0x%x\n",
		    ieef_num, ieef_tab[ieef_num].ioaddr, ioaddr));
		if (ieef_tab[ieef_num].ioaddr == ioaddr)
			break;
	}
	if (ieef_num == num_adapters) {
		Dprintf(ERR, ("Adapter not found in ieef_tab\n"));
		goto fatal_error;
	}

	ieefp->ieef_type = ieef_tab[ieef_num].hwtype;
	ieefp->ieef_num = ieef_num;
	ieefp->ieef_speed = 100;

	ieefp->ieef_ioaddr = ioaddr;
	ieefp->irq_level = irq;
	ieef_port_ca_nid(ieefp);

	/*
	 * In the Solaris driver, the following are configurable properties.
	 * Here we will only use the default values as in the ieef.conf file.
	 */
	ieefp->ieef_framesize = BUFSIZE;
	ieefp->ieef_nframes = IEEF_NFRAMES;
	ieefp->ieef_xbufsize = BUFSIZE;
	ieefp->ieef_xmits = IEEF_NXMIT;

	if (ieefp->ieef_type & IEEF_HW_EE100) {
		speed_tab = ee100_speed_tab;
		for (speed_index = 0; speed_tab[speed_index]; speed_index++) {
			ieefp->ieef_speed = speed_tab[speed_index];
			ieef_adapter_init(ieefp);
			if (ieef_speed_ok(ieefp) == SUCCESS) {
				Dprintf(FLOW,
				    ("AdapterInit: selected data rate 0x%x\n",
				    ieefp->ieef_speed));
				return (SUCCESS);
			}
		}
	} else {
		ieef_adapter_init(ieefp);
		ieef_detect_media(ieefp);
		return (SUCCESS);
	}

	Dprintf(ERR, ("Failed to find speed\n"));
fatal_error:
	putstr("\r\n\
Adapter is not configured properly or is not attached to the network.\r\n");
	putstr("Cannot initialize adapter, press Ctrl-Alt-Del to reboot.\r\n");
#ifdef DEBUG6
	PAK();
#endif
fatal_loop:
	intr_enable();		/* so Ctrl-Alt-Del will work */
	goto fatal_loop;
}

void
AdapterReceive(unchar _far *buffer, ushort buflen, recv_callback_t func)
{
	int	oldspl = splhi();

#ifdef DEBUG5
	if (DebugFlag) {
		putstr("AdapterReceive: buflen ");
		putnum(buflen);
		putstr("\r\n");
	}
#endif
	Rx_Buffer = buffer;
	Rx_Buf_Len = buflen;
	Call_Back = func;
	Listen_Posted = TRUE;
	splx(oldspl);
}

int
ieef_adapter_init(struct ieefinstance *ieefp)
{
	unchar	irqcontrol;
	ulong	ltemp;
	ushort	i;

	/* set latched-mode-interrupts option, and clear interrupt status */
	switch (ieefp->ieef_type) {
	case IEEF_HW_UNISYS_FLASH:
		/* reset the board first */
		outl(ieefp->ieef_ioaddr + ieefp->ieef_port, (long)0);
		microseconds(100);
		irqcontrol = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		irqcontrol |= 0x18;
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, irqcontrol);
		break;
	case IEEF_HW_FLASH:
		/* reset the board first */
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port, 0);
		microseconds(100);
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port + 2, 0);
		microseconds(100);
		irqcontrol = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		irqcontrol |= 0x18;
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, irqcontrol);
		break;
	case IEEF_HW_EE100_EISA:
		/* reset the board first */
		outl(ieefp->ieef_ioaddr + ieefp->ieef_port, 0L);
		microseconds(100);
		irqcontrol = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		irqcontrol |= 0x18;
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, irqcontrol);
		break;
	case IEEF_HW_EE100_PCI:
		/* reset the board first */
		outl(ieefp->ieef_ioaddr + ieefp->ieef_port, 0L);
		microseconds(100);
		ltemp = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		ltemp |= 0x118;
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, ltemp);
		break;
	case IEEF_HW_UNISYS:
		/* reset any pending interrupt status */
		outb(ieefp->ieef_ioaddr + 0x10, 1);
		outb(ieefp->ieef_ioaddr + 0x10, 0);
		microseconds(100);
		(void) inb(ieefp->ieef_ioaddr + 0x10);
		break;
	}

	/*
	 * Do anything necessary to prepare the board for operation
	 * short of actually starting the board.
	 */

	/* set up shared memory and tell board about it */
	Dprintf(FLOW, ("(ieef_init_board)"));
	if ((ieef_init_board(ieefp)) != SUCCESS) {
		ieef_stop_board_int(ieefp);
		return (FAILURE);
	}


	Dprintf(FLOW, ("(get MAC_addr)"));
	for (i = 0; i < ETHERADDRL; i++) {
		switch (ieefp->ieef_type) {
		case IEEF_HW_EE100_PCI:
			(void) pci_read_config_byte(
				ieef_tab[ieefp->ieef_num].pci_id,
				ieefp->ieef_nid + i, &MAC_Addr[i]);
			break;
		default:
			MAC_Addr[i] = inb(ieefp->ieef_ioaddr +
				ieefp->ieef_nid + i);
			break;
		}
	}

	/* configure the board */
	Dprintf(FLOW, ("(ieef_start_board_init)"));
	ieef_start_board_int(ieefp);

	Dprintf(FLOW, ("(ieef_saddr)"));
	/* tell the board our address */
	ieef_saddr(MAC_Addr);

	Dprintf(FLOW, ("(ieef_adapter_int OK)"));
	return (SUCCESS);
}

int
ieef_speed_ok(struct ieefinstance *ieefp)
{
	dump_cmd_t	dcmd;
	int		tries;
	int		i;
#define	LINK_BYTE	1
#define	INTEGRITY_BIT	0x40

	/* Wait for the line to settle */
	milliseconds(1000);

	/*
	 * Link integrity sometimes seems to be set even when the
	 * speed is wrong.  Do not assume the speed is correct
	 * unless the integrity bit is consistently set.
	 */
	for (tries = 0; tries < 10; tries++) {

		dcmd.dump_status = CS_EL | CS_INT | CS_CMD_DUMP;
		dcmd.dump_bufp = ieefp->rbufsp[0];

		if (ieef_add_command(ieefp, (gen_cmd_t *)&dcmd,
		    sizeof (dcmd)) == -1) {
#ifdef DEBUG
			if (DebugFlag)
				putstr(
				    "ieef_speed_ok: couldn't dump device.\r\n");
#endif
			return (FAILURE);
		}

		/* Wait for command to complete */
		for (i = 0; i < 1000; i++) {
			if (ieefp->last_cb == 0xffff)
				break;
			milliseconds(1);
		}
		if (i == 1000)
			return (FAILURE);

#ifdef DEBUG
		putstr("ieef_speed_ok: dump bytes ");
		put2hex((unchar)ieefp->rbufsv[0][0]);
		putchar(' ');
		put2hex((unchar)ieefp->rbufsv[0][1]);
		putchar(' ');
		put2hex((unchar)ieefp->rbufsv[0][2]);
		putstr("\r\n");
#endif

		/* Speed is bad if link integrity bit is not set */
		if ((ieefp->rbufsv[0][LINK_BYTE] & INTEGRITY_BIT) == 0)
			return (FAILURE);

		/* Wait a while before trying again */
		milliseconds(100);
	}
	return (SUCCESS);
}

/*
 *  ieef_init_board() -- initialize the specified network board.
 */
int
ieef_init_board(struct ieefinstance *ieefp)
{
	volatile char *ptr = board_mem;
	ulong	seg, off;
	int	i;
	unchar	status;

	/* Must first zero out the whole memory area */
	for (i = 0; i < MEM_SIZE; i++)
		*ptr++ = 0;

	/*
	 * point to the driver reserved memory area for device memory
	 * align it to start at 16-byte boundary.
	 */
	ptr = board_mem;
	while (((uint)ptr % 16) != 0)
		ptr++;

#ifdef DEBUG3
	if (DebugFlag) {
		putstr("init_board(): board_mem = 0x");
		putptr((char _far *)(ptr));
		putstr("\r\n");
	}
#endif

	/* kmem_map is in seg:off format for long pointers */
	ieefp->kmem_map = (struct ieef_shmem _far *)ptr;

	/* pmem_map is in (seg << 4 + off) absolute address format */
	seg = FP_SEG(ieefp->kmem_map);
	off = FP_OFF(ieefp->kmem_map);
	ieefp->pmem_map = (seg << 4) + off;
#ifdef DEBUG3
	putstr("pmem_map=");
	putptr((char _far *)(ieefp->pmem_map));
	putstr("\r\n");
#endif

	/*
	 * Other data structure pointers.
	 * All the pointers are in absoluate address format so casting
	 * a far pointer to (long) does not do the right thing.
	 */
	ieefp->kmem_map->ieef_scp.scp_sysbus = 0x44;
	ieefp->kmem_map->ieef_scp.scp_iscp = ieefp->pmem_map + sizeof (scp_t);
	ieefp->kmem_map->ieef_iscp.iscp_busy = 1;
	ieefp->kmem_map->ieef_iscp.iscp_scb = ieefp->pmem_map +
	    sizeof (scp_t) + sizeof (iscp_t);
	ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_NOP;
	ieefp->kmem_map->ieef_scb.scb_cbl = ieefp->pmem_map +
	    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t);
	ieefp->kmem_map->ieef_scb.scb_rfa = ieefp->pmem_map +
	    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t) +
	    IEEF_NCMDS * sizeof (gen_cmd_t);
#ifdef DEBUG2
	putstr("SCB_RFA=0x");
	putptr((char _far *)(ieefp->kmem_map->ieef_scb.scb_rfa));
	putstr("\r\n");
#endif
	ieefp->last_cb = 0xffff;
	ieefp->current_cb = 0xffff;
	ieefp->current_frame = 0;
	ieefp->begin_frame = 0;
	ieefp->last_frame = ieefp->ieef_nframes - 1;
	ieefp->mcs_count = 0;

	if ((ieef_alloc_buffers(ieefp)) == FAILURE)
		return (FAILURE);

	ieef_reset_rfa(ieefp);

	/*
	 * Issue PORT command to tell 596 top of memory.  Note that
	 * it must begin on a 16 byte boundary because the first
	 * 4 bits tell the 596 the type of PORT command.
	 */
	seg = (ieefp->pmem_map >> 16);
	off = (ieefp->pmem_map & 0xffff);

#ifdef DEBUG3
	if (DebugFlag) {
		putstr("seg = 0x");
		puthex((ushort)seg);
		putstr(" off = 0x");
		puthex((ushort)off);
		putstr("\r\n");
	}
#endif

	switch (ieefp->ieef_type) {
	case IEEF_HW_UNISYS_FLASH:
	case IEEF_HW_EE100_EISA:
	case IEEF_HW_EE100_PCI:
		/* Unisys add-on board wants to have all 32 bits at once */
		outl(ieefp->ieef_ioaddr + ieefp->ieef_port,
		    ieefp->pmem_map | (long)IEEF_NEWSCP);
		break;
	default:
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port,
		    (ushort)off | IEEF_NEWSCP);
		outw(ieefp->ieef_ioaddr + ieefp->ieef_port + 2, (ushort)seg);
	}
	CHANNEL_ATTENTION(ieefp);

	/* wait long enough for the init to complete */
	milliseconds(10);
	if (ieefp->kmem_map->ieef_iscp.iscp_busy)
		return (FAILURE);

#if 0
	/* setting the new SCP causes an interrupt, clear it */
	COMMAND_QUIESCE(ieefp);
	if (status = (ieefp->kmem_map->ieef_scb.scb_status & SCB_ACK_MSK))
		ieefp->kmem_map->ieef_scb.scb_command = status;

	CHANNEL_ATTENTION(ieefp);
	COMMAND_QUIESCE(ieefp);
#endif

#ifdef DEBUG3
	putstr("success: ");
	PAK();
#endif
	return (SUCCESS);
}

/*
 *  ieef_start_board_int() -- configure the board and allow transmits.
 */
void
ieef_start_board_int(struct ieefinstance *ieefp)
{
	unchar ctmp;
	ulong ltmp;

#ifdef DEBUG3
	if (DebugFlag)
		putstr("start_board_int()\r\n");
#endif
	ieef_configure(ieefp, 0);

	/* reset and retrigger the interrupt latch */
	switch (ieefp->ieef_type) {
	case IEEF_HW_FLASH:
	case IEEF_HW_UNISYS_FLASH:
	case IEEF_HW_EE100_EISA:
		ctmp = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			(unchar)(ctmp | 0x30));
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			(unchar)(ctmp & ~0x30));
		break;
	case IEEF_HW_EE100_PCI:
		ltmp = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, ltmp | 0x30);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			ltmp & ~0x30);
		break;
	default:
		break;
	}
}

void
ieef_start_ru(struct ieefinstance *ieefp)
{
#ifdef DEBUG1
	if (DebugFlag)
		putstr("start_ru()\r\n");
#endif
	COMMAND_QUIESCE(ieefp);
	ieefp->kmem_map->ieef_scb.scb_rfa = ieefp->pmem_map +
	    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t) +
	    IEEF_NCMDS * sizeof (gen_cmd_t) +
	    ieefp->current_frame * sizeof (rfd_t);
	ieefp->kmem_map->ieef_scb.scb_command = SCB_RUC_STRT;
	CHANNEL_ATTENTION(ieefp);
	COMMAND_QUIESCE(ieefp);
}

/*
 *  ieef_stop_board_int() -- stop board receiving
 */
void
ieef_stop_board_int(struct ieefinstance *ieefp)
{
#ifdef DEBUG2
	if (DebugFlag)
		putstr("stop_board_int()\r\n");
#endif
	COMMAND_QUIESCE(ieefp);
	ieefp->kmem_map->ieef_scb.scb_command = SCB_RUC_SUSPND;
	CHANNEL_ATTENTION(ieefp);
	COMMAND_QUIESCE(ieefp);
}

void
ieef_port_ca_nid(struct ieefinstance *ieefp)
{
	switch (ieefp->ieef_type) {
	case IEEF_HW_UNISYS:
		ieefp->ieef_port = IEEF_UNISYS_PORT;
		ieefp->ieef_ca = IEEF_UNISYS_CA;
		ieefp->ieef_nid = IEEF_UNISYS_NID;
		ieefp->ieef_user_pins = PLXE_REGISTER_1;
		break;
	case IEEF_HW_FLASH:
	case IEEF_HW_UNISYS_FLASH:
	case IEEF_HW_EE100_EISA:
		ieefp->ieef_port = PLXE_PORT_OFFSET;
		ieefp->ieef_ca = PLXE_CA_OFFSET;
		ieefp->ieef_nid = PLXE_ADDRESS_PROM;
		ieefp->ieef_int_control = FL32_EXTEND_INT_CONTROL;
		ieefp->ieef_user_pins = PLXE_REGISTER_1;
		break;
	case IEEF_HW_EE100_PCI:
		ieefp->ieef_port = PLXP_PORT_OFFSET;
		ieefp->ieef_ca = PLXP_CA_OFFSET;
		ieefp->ieef_nid = PLXP_NODE_ADDR_REGISTER;
		ieefp->ieef_int_control = PLXP_INTERRUPT_CONTROL;
		ieefp->ieef_user_pins = PLXP_USER_PINS;
		break;
	default:
		break;
	}
}

void
lock_data_rate(struct ieefinstance *ieefp)
{
	unchar  portbyte;
	ulong   portword;

	Dprintf(TEST, ("(lock_data_rate)"));
	if (ieefp->ieef_type == IEEF_HW_EE100_PCI) {
		portword = 0;
		portword |= 0xf0;
		portword |= 1;
		Dprintf(TEST, ("o"));
		(void) outl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
		    portword);
		Dprintf(TEST, ("p"));
		microseconds(10000);

		Dprintf(TEST, ("i"));
		portword = inl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portword &= ~4;
		portword |= 2;
		Dprintf(TEST, ("o"));
		(void) outl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
		    portword);
		Dprintf(TEST, ("p"));
		microseconds(10000);
		Dprintf(TEST, ("i"));
		portword = inl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portword &= ~2;
		portword |= 4;
		Dprintf(TEST, ("o"));
		(void) outl(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
		    portword);
		Dprintf(TEST, ("p"));
		microseconds(10000);
		Dprintf(TEST, ("d"));
	} else if (ieefp->ieef_type == IEEF_HW_EE100_EISA) {
		portbyte = inb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portbyte |= 1;
		(void) outb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
		    portbyte);
		microseconds(10000);

		portbyte = inb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portbyte &= ~4;
		portbyte |= 2;
		(void) outb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
		    portbyte);
		microseconds(10000);
		portbyte = inb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portbyte &= ~2;
		portbyte |= 4;
		(void) outb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins,
		    portbyte);
		microseconds(10000);
	}
}
/*
 *  ieef_saddr() -- set the physical network address on the board
 */
int
ieef_saddr(unchar *node_addr)
{
	ias_cmd_t icmd;
	int	i;

	icmd.ias_status = CS_EL | CS_INT | CS_CMD_IASET;
	for (i = 0; i < ETHERADDRL; i++)
		icmd.ias_addr[i] = node_addr[i];

	if (ieef_add_command(ieefp, (gen_cmd_t *)&icmd,
	    sizeof (icmd)) == -1) {
#ifdef DEBUG3
		if (DebugFlag)
			putstr("ieef: Could not set address.\r\n");
#endif
		return (-1);
	}
	return (0);
}

/*
 *  Called when a packet is ready to be transmitted. A pointer to an
 *  M_DATA message that contains the packet is passed to this routine.
 *  The complete LLC header is contained in the message's first message
 *  block, and the remainder of the packet is contained within
 *  additional M_DATA message blocks linked to the first message block.
 */
void
AdapterSend(char _far *packet, ushort packetlen)
{
	int	old_spl;
	int	i;
	int	s;

	old_spl = splhi();

	/* Get an available TBD */
	s = ieef_avail_tbd(ieefp);
	if (s == -1) {
#ifdef DEBUG4
		if (DebugFlag)
			putstr("ieef_avail_tbd() returns -1\r\n");
#endif
		splx(old_spl);
		return;
	}

#ifdef DEBUG3
	if (DebugFlag) {
		putstr("AdapterSend, slot = ");
		putnum(s);
		putstr("\r\n");
	}
#endif

	/* Copy the packet to the TBD's associated buffers */
#ifdef DEBUG
	if (DebugFlag) {
		putstr("packet = 0x");
		putptr(packet);
		putstr(" transmit buffer = 0x");
		putptr(
		    (char _far *)(ieefp->kmem_map->ieef_tbd[s].tbd_v_buffer));
		putstr("\r\n");
	}
#endif

	for (i = 0; i < packetlen; i++) {
		ieefp->kmem_map->ieef_tbd[s].tbd_v_buffer[i] = packet[i];
	}

	ieefp->kmem_map->ieef_tbd[s].tbd_size = (long)(packetlen | CS_EOF);
	ieefp->kmem_map->ieef_tbd[s].tbd_next = (long)0xffffffff;

	/* Form the XMIT command */
	xcmd.xmit_status = CB_SF | CS_EL | CS_INT | CS_CMD_XMIT;
	xcmd.xmit_tbd = ieefp->pmem_map + sizeof (scp_t) +
	    sizeof (iscp_t) + sizeof (scb_t) +
	    IEEF_NCMDS * sizeof (gen_cmd_t) +
	    IEEF_NFRAMES * sizeof (rfd_t) +
	    IEEF_NFRAMES * sizeof (rbd_t) + s * sizeof (tbd_t);
#ifdef DEBUG
	if (DebugFlag) {
		putstr("xmit_tbd = 0x");
		putptr((char _far *)(xcmd.xmit_tbd));
		putstr("\r\n");
	}
#endif
	xcmd.xmit_v_tbd = &(ieefp->kmem_map->ieef_tbd[s]);
	xcmd.xmit_tcb_cnt = 0;

	/* Add it to the command chain */
	if (ieef_add_command(ieefp, (gen_cmd_t *)&xcmd,
	    sizeof (xcmd)) == -1) {
		splx(old_spl);
		return;
	}

	splx(old_spl);
}

/*
 *  ieefintr() -- interrupt from board to inform us that a receive or
 *  transmit has completed.
 */
void
AdapterInterrupt(void)
{
	int	old_spl = splhi();
	short	status;
	unchar	pl_stat;
	ulong	pl_stat_l;

	/*
	 * Disable CPU interrupts and set the proper data segment value
	 */

	switch (ieefp->ieef_type) {
	case IEEF_HW_FLASH:
	case IEEF_HW_UNISYS_FLASH:
	case IEEF_HW_EE100_EISA:
		/* Mask interrupts and reset the latch */
		pl_stat = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		pl_stat |= 0x30;
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, pl_stat);
		break;
	case IEEF_HW_EE100_PCI:
		/* Mask interrupts and reset the latch */
		pl_stat_l = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		pl_stat_l |= 0x30;
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, pl_stat_l);
		break;
	case IEEF_HW_UNISYS:
		/* clear the interrupt status */
		(void) inb(ieefp->ieef_ioaddr + 0x10);
		break;
	}

	status = ieefp->kmem_map->ieef_scb.scb_status & SCB_ACK_MSK;

#ifdef DEBUG4
	if (DebugFlag) {
		putstr("AdapterInterrupt: status 0x");
		puthex(status);
		putstr("\r\n");
	}
#endif

	while (status != 0) {
		switch (ieefp->ieef_type) {
		case IEEF_HW_FLASH:
		case IEEF_HW_UNISYS_FLASH:
		case IEEF_HW_EE100_EISA:
			/* Reset the interrupt latch */
			pl_stat = inb(ieefp->ieef_ioaddr +
			    ieefp->ieef_int_control);
			pl_stat |= 0x10;
			outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			    pl_stat);
			break;
		case IEEF_HW_EE100_PCI:
			/* Reset the interrupt latch */
			pl_stat_l = inl(ieefp->ieef_ioaddr +
			    ieefp->ieef_int_control);
			pl_stat_l |= 0x10;
			outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control,
			    pl_stat_l);
			break;
		}

		/* Inform the board that the interrupt has been received */
		COMMAND_QUIESCE(ieefp);
		ieefp->kmem_map->ieef_scb.scb_command = status;
		CHANNEL_ATTENTION(ieefp);

		/* Check for transmit complete interrupt */
		if (status & SCB_ACK_CX)
			ieef_cmd_complete(ieefp, status);

		/* Check for receive completed */
		if (status & SCB_ACK_FR)
			ieef_process_recv(ieefp, status);

		/* Check for Receive No Resources */
		if (status & SCB_ACK_RNR) {
			/*
			 * make certain nothing arrived since last time I
			 * checked
			*/
			ieef_process_recv(ieefp, status);

			/* Resume the receive unit */
			if (!(ieefp->kmem_map->ieef_scb.scb_status &
			    SCB_RUS_READY)) {
				ieef_rfa_fix(ieefp);
				ieef_start_ru(ieefp);
			}
		}
		status = ieefp->kmem_map->ieef_scb.scb_status & SCB_ACK_MSK;
	}

	COMMAND_QUIESCE(ieefp);
	switch (ieefp->ieef_type) {
	case IEEF_HW_FLASH:
	case IEEF_HW_UNISYS_FLASH:
	case IEEF_HW_EE100_EISA:
		/* release the interrupt latch */
		pl_stat = inb(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		pl_stat &= ~(0x30);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_int_control, pl_stat);
		break;
	case IEEF_HW_EE100_PCI:
		/* release the interrupt latch */
		pl_stat_l = inl(ieefp->ieef_ioaddr + ieefp->ieef_int_control);
		pl_stat_l &= ~(0x30);
		outl(ieefp->ieef_ioaddr + ieefp->ieef_int_control, pl_stat_l);
		break;
	}

	if (ieefp->irq_level > 7)
		outb(0xA0, 0x20);
	outb(0x20, 0x20);

	splx(old_spl);
}

void interrupt
AdapterISR()
{
	intr_disable();
	set_ds();
	AdapterInterrupt();
	intr_enable();
}

/*
 * Function to allocate buffers used for transmit and receive frames.
 * Also initializes command buffers, and xmit buffers.
 */
int
ieef_alloc_buffers(struct ieefinstance *ieefp)
{
	int	s;

	/*
	 * We store the physical addresses of the receive buffers.
	 * These are absolute addresses, not seg:off pointers.
	 */
	for (s = 0; s < IEEF_NFRAMES; s++) {
		ieefp->rbufsp[s] = ieefp->pmem_map + (long)(CMD_AREA) +
						(long)(s * BUFSIZE);
		ieefp->rbufsv[s] = (char _far *)
			((long)ieefp->kmem_map + (long)(CMD_AREA) +
				(long)(s * BUFSIZE));
	}

	/* Store the physical address of the transmit buffer */
	for (s = 0; s < IEEF_NXMIT; s++) {
		ieefp->xbufsp[s] = ieefp->pmem_map + (long)(CMD_AREA) +
					(long)((IEEF_NFRAMES) * BUFSIZE) +
					(long)(s * BUFSIZE);
		ieefp->xbufsv[s] = (char _far *)
			((long)ieefp->kmem_map + (long)(CMD_AREA) +
				(long)((IEEF_NFRAMES) * BUFSIZE) +
				(long)(s * BUFSIZE));
		ieefp->tbdavail[s] = 1;
	}

	/* Set up the command buffers */
	for (s = 0; s < IEEF_NCMDS; s++)
		ieefp->cmdavail[s] = TRUE;

	/* Set up the xmit buffers and tbds */
	for (s = 0; s < IEEF_NXMIT; s++) {
		ieefp->kmem_map->ieef_tbd[s].tbd_size = 0;
		ieefp->kmem_map->ieef_tbd[s].tbd_next = 0;
		ieefp->kmem_map->ieef_tbd[s].tbd_buffer = ieefp->xbufsp[s];
		ieefp->kmem_map->ieef_tbd[s].tbd_v_buffer = ieefp->xbufsv[s];
	}

	return (SUCCESS);
}

/*
 * Function to set up the Receive Frame Area
 */
void
ieef_reset_rfa(struct ieefinstance *ieefp)
{
	int	s;

#ifdef DEBUG5
	if (DebugFlag)
		putstr("reset_rfa()\r\n");
#endif
	/* Set up the RFDs first */
	for (s = 0; s < (ieefp->ieef_nframes - 1); s++) {
		/*
		 * We use the 'flexible mode' (i.e. all data stored in the
		 * receive buffer and none in the RFD itself)
		 */
		ieefp->kmem_map->ieef_rfd[s].rfd_status = 0;
		ieefp->kmem_map->ieef_rfd[s].rfd_ctlflags = FLEXIBLE_MODE;

		/* Each RFD points to the next (except 1st, see below) */
		ieefp->kmem_map->ieef_rfd[s].rfd_next = ieefp->pmem_map +
		    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t) +
		    IEEF_NCMDS * sizeof (gen_cmd_t) + (s + 1) * sizeof (rfd_t);

		/*
		 * The 596 will point the RFD to the available RBD
		 * (except 1st, see below)
		 */
		ieefp->kmem_map->ieef_rfd[s].rfd_rbd = (long)0xffffffff;

		/* Filled in by the 596 */
		ieefp->kmem_map->ieef_rfd[s].rfd_count = (long)0;
	}

	/*
	 * Special values for the 1st and last RFD
	 */
	/* First RFD points to first RBD */
	ieefp->kmem_map->ieef_rfd[0].rfd_rbd = ieefp->pmem_map +
	    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t) +
	    IEEF_NCMDS * sizeof (gen_cmd_t) + IEEF_NFRAMES * sizeof (rfd_t);

	/* Last RFD is flagged as the end of list, for now */
	ieefp->kmem_map->ieef_rfd[ieefp->ieef_nframes-1].rfd_ctlflags =
	    (FLEXIBLE_MODE | RF_EL);

	/* Last RFD points to first RFD */
	ieefp->kmem_map->ieef_rfd[ieefp->ieef_nframes-1].rfd_next =
	    ieefp->pmem_map + sizeof (scp_t) + sizeof (iscp_t) +
	    sizeof (scb_t) + IEEF_NCMDS * sizeof (gen_cmd_t);

	/* Last RFD has rfd_rbd set to 0xffffffff */
	ieefp->kmem_map->ieef_rfd[ieefp->ieef_nframes-1].rfd_rbd =
	    (long)0xffffffff;

	/* Now, set up the RBDs */
	for (s = 0; s < (ieefp->ieef_nframes); s++) {

		/* Count is filled in by the 596 */
		ieefp->kmem_map->ieef_rbd[s].rbd_count = (long)0;

		/* Each RBD points to the next (except 1st, see below) */
		ieefp->kmem_map->ieef_rbd[s].rbd_next = ieefp->pmem_map +
		    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t) +
		    IEEF_NCMDS * sizeof (gen_cmd_t) +
		    IEEF_NFRAMES * sizeof (rfd_t) + (s + 1) * sizeof (rbd_t);

		/* Put the physical address in the RBD for the 596 to use */
		ieefp->kmem_map->ieef_rbd[s].rbd_buffer = ieefp->rbufsp[s];

		/* Put the virtual address in the RBD for *us* to use */
		ieefp->kmem_map->ieef_rbd[s].rbd_v_buffer = ieefp->rbufsv[s];

		/* Store the framesize in the RBD for the 596 to use */
		ieefp->kmem_map->ieef_rbd[s].rbd_size =
		    (long)ieefp->ieef_framesize;
	}

	/* The last RBD has the EOF Bit set */
	ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes-1].rbd_size |=
	    (long)CS_EOF;

	/* The last RBD points to the first */
	ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes-1].rbd_next =
	    ieefp->pmem_map + sizeof (scp_t) + sizeof (iscp_t) +
	    sizeof (scb_t) + IEEF_NCMDS * sizeof (gen_cmd_t) +
	    IEEF_NFRAMES * sizeof (rfd_t);

	ieefp->end_rbd = &(ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes-1]);
}

/*
 * Function to return the next available TBD.  Returns -1 if none.
 */
int
ieef_avail_tbd(struct ieefinstance *ieefp)
{
	if (ieefp->tbdavail[0] == 1) {
		ieefp->tbdavail[0] = 0;
#ifdef DEBUG
		putstr("TBD\r\n");
#endif
		return (0);
	}
	Dprintf(ERR, ("no avail tbd: scb_status 0x%x, scb_command 0x%x\n",
	    ieefp->kmem_map->ieef_scb.scb_status,
	    ieefp->kmem_map->ieef_scb.scb_command));
	return (-1);
}

/*
 * Function to add a command block to the list of commands to be processed
 */
int
ieef_add_command(struct ieefinstance *ieefp, gen_cmd_t *cmd, int len)
{
	int	newcmd;
	int	i;
	char _far *ptr1;
	char _far *ptr2;

	Dprintf(FLOW, ("(add_command)"));

	/* All commands must have the interrupt bit and point to -1 (EOL) */
	cmd->cmd_next = (long)0xffffffff;
	cmd->cmd_status |= (long)CS_INT;

	for (i = 0; i < 10000; i++) {
		if (ieefp->last_cb == 0xffff)
			break;
		milliseconds(1);
	}

	/*
	 * If our pointer to the last command block is
	 * (short)-1, it is empty
	 */
	if (ieefp->last_cb == 0xffff) {
		Dprintf(FLOW, ("(empty cmd list)"));
		/*
		 * Since the list is empty, we will use the first
		 * command buffer
		 */
		ieefp->last_cb = 0;
		ieefp->current_cb = 0;

		/* Mark it as busy */
		ieefp->cmdavail[0] = FALSE;

		/* Copy the data to the command descriptor */
		ptr1 = (char _far *)&(ieefp->kmem_map->ieef_cmds[0]);
		ptr2 = (char _far *)cmd;
		Dprintf(FLOW, ("(copy %lx->%lx)", ptr2, ptr1));

		for (i = 0; i < len; i++) {
			ptr1[i] = ptr2[i];
		}

		/* Wait for the 596 to complete whatever command is current */
		COMMAND_QUIESCE(ieefp);
		/* Point the SCB to command area */
		ieefp->kmem_map->ieef_scb.scb_cbl = ieefp->pmem_map +
		    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t);

		ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_STRT;
		CHANNEL_ATTENTION(ieefp);
	} else {
		/*
		 * Find an available command block.  If none available, wait
		 * for one.
		 */
		if ((newcmd = ieef_avail_cmd(ieefp)) == -1)
			return (-1);

		/* Copy the command into the next command buffer */
		ptr1 = (char _far *)&(ieefp->kmem_map->ieef_cmds[newcmd]);
		ptr2 = (char _far *)cmd;
		for (i = 0; i < len; i++) {
			ptr1[i] = ptr2[i];
		}

		/* Point the currently last command to this one */
		ieefp->kmem_map->ieef_cmds[ieefp->last_cb].cmd_next =
		    ieefp->pmem_map + sizeof (scp_t) + sizeof (iscp_t) +
		    sizeof (scb_t) + newcmd * sizeof (gen_cmd_t);

		/* This is the new last command */
		ieefp->last_cb = newcmd;
	}
	Dprintf(FLOW, ("(add_command OK)"));
	return (0);
}

/*
 * Function called at interrupt time when the interrupt handler determines
 * that the 596 has signaled that a command has completed.  This could
 * be due to a transmit command, or any other command.  If it is a
 * transmit command completion, we must do other processing.
 */
void
ieef_cmd_complete(struct ieefinstance *ieefp, short scb_status)
{
	long	cmd;

	/* Is there even a command pending? */
	if (ieefp->current_cb == 0xffff) {
#ifdef DEBUG4
		if (DebugFlag) {
			putstr("cmd interrupt with no cmd pending, stat 0x");
			puthex(scb_status);
			putstr("\r\n");
		}
#endif
		return;
	}

	/* Ignore this interrupt if the current command not marked complete */
	if (!(ieefp->kmem_map->ieef_cmds[ieefp->current_cb].cmd_status &
	    (long)CS_CMPLT)) {
#ifdef DEBUG4
		if (DebugFlag)
			putstr("cmd not complete\r\n");
#endif
		return;
	}

	/* Get the command type from the actual command block */
	cmd = ieefp->kmem_map->ieef_cmds[ieefp->current_cb].cmd_status;

#ifdef DEBUG3
	if (DebugFlag) {
		putstr("ieefintr: command complete, command=");
		puthex((ushort)(cmd >> 16));
		puthex((ushort)cmd);
		putstr("\r\n");
	}
#endif

	/* Check if it is a transmit command */
	if (cmd & CS_CMD_XMIT)
		ieef_xmit_complete(ieefp, scb_status);

	/*
	 * Done processing command.  We now re-arrange the command buffers
	 * and pointers to show that this one is free
	 */
	/* Mark it as available */
	ieefp->cmdavail[ieefp->current_cb] = TRUE;

	/* Is this the last block on the chain? */
	if (ieefp->current_cb == ieefp->last_cb)
		ieefp->last_cb = ieefp->current_cb = 0xffff;

	/* If there are still more blocks... */
	if (ieefp->current_cb != 0xffff) {
		long	cur;	/* was paddr_t */
		long	tmp;
		ushort	dif;

		/* Make the next one the current one */
		cur = ieefp->kmem_map->ieef_cmds[ieefp->current_cb].cmd_next;

		tmp = ieefp->pmem_map + sizeof (scp_t) + sizeof (iscp_t) +
		    sizeof (scb_t);
		dif = (ushort)(cur - tmp);
		ieefp->current_cb = dif / sizeof (gen_cmd_t);

		/* Start the command unit again */
		COMMAND_QUIESCE(ieefp);
		ieefp->kmem_map->ieef_scb.scb_cbl = (long)cur;
		ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_STRT;
		CHANNEL_ATTENTION(ieefp);
#ifdef DEBUG3
		putstr("start next cmd\r\n");
#endif
	}

	/* Signal add_command in case someone is waiting for a free buffer */
}

/*
 * A transmit command has completed, mark the buffer as available,
 * and keep statistics.
 */
void
ieef_xmit_complete(struct ieefinstance *ieefp, short scb_status)
{
#ifdef DEBUG
	putstr("XMIT\r\n");
#endif
	ieefp->tbdavail[0] = 1;
}


void
ieef_re_q(struct ieefinstance *ieefp, volatile rfd_t *rfd)
{
	volatile rfd_t	*lrfd;
	rbd_t _far *rbd;
	long fsize = ieefp->ieef_framesize;

	if (rfd->rfd_rbd == 0xffffffffL) {
#ifdef DEBUG5
		putstr("ieef_re_q: no RBD\r\n");
#endif
		goto no_rbd;
	}

	rbd = (rbd_t _far *)IEEF_PTOV(ieefp, rfd->rfd_rbd);
	while (1) {
		if ((rbd->rbd_size & CS_EOF) || (rbd->rbd_count & CS_EOF))
			break;

		/* Mark this RBD as empty and available */
		rbd->rbd_size = fsize;
		rbd->rbd_count = (long)0;

		/* Get next RBD */
		rbd = (rbd_t _far *)IEEF_PTOV(ieefp, rbd->rbd_next);
	}
	/* Make this RBD the new last one */
	rbd->rbd_size = fsize | CS_EOF;
	rbd->rbd_count = (long)0;

	/* Clear the EOF bit from the former 'last' rbd */
	ieefp->end_rbd->rbd_size = fsize;
	ieefp->end_rbd = rbd;

	/*
	 * Done processing this frame, make it the new last frame
	 */
no_rbd:

	rfd->rfd_status = 0;
	rfd->rfd_ctlflags = (FLEXIBLE_MODE | RF_EL);
	rfd->rfd_rbd = (long)0xffffffff;

	/* get ptr to the RFD which was the last in list */
	lrfd = &(ieefp->kmem_map->ieef_rfd[ieefp->last_frame]);
#ifdef DEBUG5
	if (DebugFlag) {
		putstr("not last, rfd=0x");
		putptr((char _far *)rfd);
		putstr(", lrfd=0x");
		putptr((char _far *)lrfd);
		putstr("\r\n");
	}
#endif

	/* Set the former last frame to not be last */
	lrfd->rfd_ctlflags &= ~(RF_EL);
}


/*
 * Function called from interrupt level when a frame has been received.
 */
void
ieef_process_recv(struct ieefinstance *ieefp, short scb_status)
{
	rbd_t _far *rbd;
	volatile rfd_t	*rfd;
	ushort	count;
	ushort	offset;

#ifdef DEBUG5
	if (DebugFlag) {
		putstr("process_recv(), current_frame = ");
		putnum(ieefp->current_frame);
		putstr(", last = ");
		putnum(ieefp->last_frame);
		putstr("\r\n");
	}
#endif
	/* Start at the current frame */
	rfd = &(ieefp->kmem_map->ieef_rfd[ieefp->current_frame]);
#ifdef DEBUG5
	if (DebugFlag) {
		putstr("rfd = 0x");
		putptr((char _far *)rfd);
		putstr(" --- ");
	}
#endif

	/* If it is not marked as complete, ignore it and just return */
	if (!(rfd->rfd_status & CS_CMPLT)) {
#ifdef DEBUG5
		if (DebugFlag) {
			putstr("receive not complete, ctlflags=0x");
			puthex(rfd->rfd_ctlflags);
			putstr(" status=0x");
			puthex(rfd->rfd_status);
			putstr("\r\n");
		}
#endif
		return;
	}

	while (rfd->rfd_status & (long)CS_CMPLT) {
		offset = 0;
		if ((rfd->rfd_status & CS_OK) == 0) {
			goto frame_done;
		}
		if (rfd->rfd_rbd == (long)0xffffffff) {
			goto frame_done;
		}
		if (ieefp->ieef_type == IEEF_HW_EE100_EISA ||
				ieefp->ieef_type == IEEF_HW_EE100_PCI) {
			if (rfd->rfd_status & (RFD_CRC_ERROR |
					RFD_ALIGNMENT_ERROR |
					RFD_NO_RESOURCES | RFD_DMA_OVERRUN |
					RFD_FRAME_TOO_SHORT | RFD_NO_EOP_FLAG |
					RFD_RECEIVE_COLLISION |
					RFD_LENGTH_ERROR)) {
#ifdef DEBUG5
				putstr("receive error, ctlflags=0x");
				puthex(rfd->rfd_ctlflags);
				putstr(" status=0x");
				puthex(rfd->rfd_status);
				putstr("\r\n");
#endif
				goto frame_done;
			}
		}
		rbd = (rbd_t _far *)IEEF_PTOV(ieefp, rfd->rfd_rbd);
#ifdef DEBUG5
		if (DebugFlag) {
			putstr("rbd = 0x");
			putptr((char _far *)rbd);
			putstr("\r\n");
		}
#endif
		/* Get the first RBD associated with this frame */
		rbd = (rbd_t _far *)IEEF_PTOV(ieefp, rfd->rfd_rbd);
		while (1) {
			int	i;

			/* Copy this RBD to the receive buffer */
			count = (ushort)(rbd->rbd_count & (long)CS_RBD_CNT_MSK);
#ifdef DEBUG5
			if (DebugFlag) {
				putstr(" current count = ");
				putnum(count);
				putstr("\r\n");
			}
#endif
			/* copy the bytes if the buffer isn't already full */
			if (Listen_Posted && offset < Rx_Buf_Len) {
				bcopy((char _far *)&Rx_Buffer[offset],
					(char _far *)rbd->rbd_v_buffer, count);
				offset += count;
			}

			/*
			 * If CS_EL is set, then we have reached the end
			 * of the list.  If CS_EOF is set, then we have
			 * reached the end of the frame
			 */
			if ((rbd->rbd_size & (long)CS_EOF) ||
			    (rbd->rbd_count & (long)CS_EOF)) {
#ifdef DEBUG5
				if (DebugFlag)
					putstr(
					    "end of current received list\r\n");
#endif
				break;
			}

			/* Get next RBD */
			rbd = (rbd_t _far *)IEEF_PTOV(ieefp, rbd->rbd_next);
		}
	frame_done:
		/* add this rfd and its rbd's to the free list */
		ieef_re_q(ieefp, rfd);

		/* Go on to next frame */
		ieefp->last_frame = ieefp->current_frame;
		if (ieefp->current_frame == (ieefp->ieef_nframes - 1))
			ieefp->current_frame = 0;
		else
			ieefp->current_frame++;
		rfd = &(ieefp->kmem_map->ieef_rfd[ieefp->current_frame]);

		if (Listen_Posted == TRUE && offset != 0) {
			Listen_Posted = FALSE;
			(void) (*Call_Back)(offset);
		}

	} /* while */
}

/* Set up a configure command */
void
ieef_configure(struct ieefinstance *ieefp, int promisc)
{
	milliseconds(100);
	if ((ieefp->ieef_type == IEEF_HW_EE100_EISA) ||
	    (ieefp->ieef_type == IEEF_HW_EE100_PCI)) {
		ieef_configure_ee100(ieefp, promisc);
	} else {
		ieef_configure_flash32(ieefp, promisc);
	}
	lock_data_rate(ieefp);
	milliseconds(100);
}

void
ieef_configure_flash32(struct ieefinstance *ieefp, int promisc)
{
	conf32_cmd_t	ccmd;

#ifdef DEBUG
	if (DebugFlag)
		putstr("configure()\r\n");
#endif
	ccmd.conf_status = CS_EL | CS_INT | CS_CMD_CONF;

	/* Default configuration */
	ccmd.conf_conf.cnf_fifo_byte = 0xc80e;
	ccmd.conf_conf.cnf_add_mode = 0x2e40;
	ccmd.conf_conf.cnf_pri_data = 0x6000;
	ccmd.conf_conf.cnf_slot = 0xf200;

	/* Set promiscuous mode as per argument */
	ccmd.conf_conf.cnf_hrdwr = 0 | promisc;
	ccmd.conf_conf.cnf_min_len = 0xc040;
	ccmd.conf_conf.cnf_more = 0x3f00;

#ifdef DEBUG
	if (DebugFlag) {
		putstr("cmd = 0x");
		putptr((char _far *)&ccmd);
		putstr("\r\n");
	}
#endif

	if (ieef_add_command(ieefp, (gen_cmd_t *)&ccmd, sizeof (ccmd)) == -1) {
#ifdef DEBUG4
		if (DebugFlag)
			putstr("Could not add configure command.\r\n");
#endif
	}
}

long    ieef_program_burst = 0;

void
ieef_configure_ee100(struct ieefinstance *ieefp, int promisc)
{

	conf100_cmd_t ccmd;

	Dprintf(FLOW, ("(ieef_configure_ee100)"));
	if (ieef_program_burst) {
		ieefp->kmem_map->ieef_scb.scb_timer = ieef_program_burst;
		COMMAND_QUIESCE(ieefp);
		ieefp->kmem_map->ieef_scb.scb_command = 0x500;
		CHANNEL_ATTENTION(ieefp);
	}

	ccmd.conf_status = CS_EL | CS_INT | CS_CMD_CONF;

	ccmd.conf_conf.conf_bytes01 = CB_556_CFIG_DEFAULT_PARM0_1;
	ccmd.conf_conf.conf_bytes23 = CB_556_CFIG_DEFAULT_PARM2_3;
	ccmd.conf_conf.conf_bytes45 = CB_556_CFIG_DEFAULT_PARM4_5;
	ccmd.conf_conf.conf_bytes67 = CB_556_CFIG_DEFAULT_PARM6_7;
	ccmd.conf_conf.conf_bytes89 = CB_556_CFIG_DEFAULT_PARM8_9;
	ccmd.conf_conf.conf_bytes1011 = (CB_556_CFIG_DEFAULT_PARM10_11 |
	    (promisc << 8));
	ccmd.conf_conf.conf_bytes1213 = CB_556_CFIG_DEFAULT_PARM12_13;
	ccmd.conf_conf.conf_bytes1415 = CB_556_CFIG_DEFAULT_PARM14_15;
	ccmd.conf_conf.conf_bytes1617 = CB_556_CFIG_DEFAULT_PARM16_17;
	ccmd.conf_conf.conf_bytes1819 = CB_556_CFIG_DEFAULT_PARM18_19;

	if (ieefp->ieef_speed == 10)
		ccmd.conf_conf.conf_bytes45 &= 0xfe;
	else
		ccmd.conf_conf.conf_bytes45 |= 1;

	if (ieef_add_command(ieefp, (gen_cmd_t *)&ccmd, sizeof (ccmd)) == -1) {
#ifdef DEBUG4
		if (DebugFlag)
			putstr("Could not add configure command.\r\n");
#endif
	}
}

/*
 * Function to find an available command buffer.  Returns -1 if none.
 * Assumes cmdlock already taken.
 */
int
ieef_avail_cmd(struct ieefinstance *ieefp)
{
	int	s;

	for (s = 0; s < IEEF_NCMDS; s++)
		if (ieefp->cmdavail[s] == TRUE)
			break;

	if (s == IEEF_NCMDS) {
#ifdef DEBUG3
		putstr("no avail cmds\r\n");
#endif
		return (-1);
	}

	ieefp->cmdavail[s] = FALSE;

	return (s);
}

void
ieef_rfa_fix(struct ieefinstance *ieefp)
{
#ifdef DEBUG5
	if (DebugFlag) {
		putstr("rfa_fix\r\n");
	}
#endif
	/* Stop the receive unit (should be already stopped) */
	COMMAND_QUIESCE(ieefp);
	ieefp->kmem_map->ieef_scb.scb_command = SCB_RUC_ABRT;
	CHANNEL_ATTENTION(ieefp);
	COMMAND_QUIESCE(ieefp);

	/*
	 * Reset the RFA.  Since we have at least one RBD per RFD,
	 * we cannot run out of RFDs and still have RBDs.  If we simply
	 * reset the beginning and end pointers, and set the first
	 * RFD to point to the first RBD, we should be ready to start
	 * again.  We do not have to re-set-up the whole RFA.
	 */
	ieefp->end_rbd->rbd_size = (long)ieefp->ieef_framesize;
	ieefp->kmem_map->ieef_rfd[ieefp->last_frame].rfd_status = 0;
	ieefp->kmem_map->ieef_rfd[ieefp->last_frame].rfd_ctlflags =
	    FLEXIBLE_MODE;

	ieefp->current_frame = 0;
	ieefp->begin_frame = 0;
	ieefp->last_frame = ieefp->ieef_nframes - 1;
	ieefp->end_rbd = (rbd_t _far *)
	    &(ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes-1]);

	ieefp->kmem_map->ieef_rfd[ieefp->ieef_nframes-1].rfd_ctlflags =
	    (FLEXIBLE_MODE | RF_EL);
	ieefp->kmem_map->ieef_rfd[0].rfd_rbd = ieefp->pmem_map +
	    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t) +
	    IEEF_NCMDS * sizeof (gen_cmd_t) + IEEF_NFRAMES * sizeof (rfd_t);
	ieefp->kmem_map->ieef_rbd[ieefp->ieef_nframes-1].rbd_size =
	    (long)(ieefp->ieef_framesize | CS_EOF);
	ieefp->kmem_map->ieef_scb.scb_rfa = ieefp->pmem_map +
	    sizeof (scp_t) + sizeof (iscp_t) + sizeof (scb_t) +
	    IEEF_NCMDS * sizeof (gen_cmd_t);
}

void
AdapterOpen(void)
{
	int	old_spl = splhi();

#ifdef DEBUG6
	if (DebugFlag) {
		putstr("AdapterOpen() ");
		PAK();
	}
#endif

	if (!(ieefp->kmem_map->ieef_scb.scb_status & SCB_RUS_READY)) {
		ieef_start_ru(ieefp);
	}

	splx(old_spl);
}

void
AdapterClose(void)
{
	int	old_spl = splhi();

#ifdef DEBUG6
	putstr("AdapterClose\r\n");
	PAK();
#endif
	COMMAND_QUIESCE(ieefp);
	ieefp->kmem_map->ieef_scb.scb_command = SCB_CUC_SUSPND | SCB_RUC_ABRT;
	CHANNEL_ATTENTION(ieefp);
	COMMAND_QUIESCE(ieefp);
	splx(old_spl);
}


int
eisaIdMatch(unchar *id, ushort port)
{
	return ((inb(port) != id[0] || inb(port + 1) != id[1] ||
	    inb(port + 2) != id[2] || inb(port + 3) != id[3]) ? 0 : 1);
}


void
ieef_set_media(struct ieefinstance *ieefp, unchar bits)
{
		unchar portbyte;

#ifdef DEBUG
		switch (bits) {
			case IEEF_MEDIA_AUI :
			    Dprintf(MEDIA, ("ieef_set_media : AUI \r\n"));
				break;
			case IEEF_MEDIA_BNC :
				Dprintf(MEDIA, ("ieef_set_media : BNC \r\n"));
				break;
			case IEEF_MEDIA_TP  :
				Dprintf(MEDIA, ("ieef_set_media : TP \r\n"));
				break;
			default :
				Dprintf(MEDIA,
				    ("ieef_set_media : Unknown Media \r\n"));
				break;
		}
#endif
		portbyte = inb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins);
		portbyte &= 0xFC;
		portbyte |= (bits & 3);
		outb(ieefp->ieef_ioaddr + ieefp->ieef_user_pins, portbyte);
		microseconds(2000);
}

int
ieef_send_test(struct ieefinstance *ieefp)
{
	int	old_spl;
	int	i;
	int	s, status;

	/* Form the XMIT command */
	xcmd.xmit_status = CS_EL | CS_CMD_XMIT;
	xcmd.xmit_tbd = 0xffffffffL;
	xcmd.xmit_tcb_cnt = 14 | CS_EOF;
	xcmd.xmit_length = 0;
	for (i = 0; i < ETHERADDRL; i++)
		xcmd.xmit_dest[i] = MAC_Addr[i];

	for (i = 0; i < 100; i++) {
		old_spl = splhi();
		if (ieefp->current_cb == 0xffff)
			break;
		splx(old_spl);
		milliseconds(1);
	}

	if (i == 100) {
		Dprintf(MEDIA, ("ieef_send_test : Timeout-Failure\r\n"));
		return (-1);
	}

	/* Add it to the command chain */
	if (ieef_add_command(ieefp, (gen_cmd_t *)&xcmd, sizeof (xcmd)) == -1) {
		splx(old_spl);
		Dprintf(MEDIA, ("ieef_send_test : Add Command-Failure\r\n"));
		return (-1);
	}
	i = 50;
	while (--i) {
		status = ieefp->kmem_map->ieef_cmds[0].cmd_status;
		if (status & CS_CMPLT)
			break;
		microseconds(100);
	}

	if (status & CS_OK) {
		splx(old_spl);
		Dprintf(MEDIA, ("ieef_send_test : Transmit-Success \r\n"));
		return (SUCCESS);
	}
	splx(old_spl);
	Dprintf(MEDIA, ("ieef_send_test : Transmit-Failure \r\n"));
	return (FAILURE);
}

void
ieef_detect_media(struct ieefinstance *ieefp)
{
		Dprintf(MEDIA, ("ieef_detect_media(0x%p)\r\n", ieefp));
		ieef_set_media(ieefp, IEEF_MEDIA_BNC);
		if (ieef_send_test(ieefp) == SUCCESS)
				return;
		milliseconds(2000);
		ieef_set_media(ieefp, IEEF_MEDIA_AUI);
		if (ieef_send_test(ieefp) == SUCCESS)
				return;
		ieef_set_media(ieefp, IEEF_MEDIA_TP);
}
