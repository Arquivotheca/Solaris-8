 /*
	PCnet  Realmode Driver
 */

/*
 * Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ident  "@(#)pcn.c 1.21	97/07/07 SMI\n"
/*
 * Modification history
 *
 */

#include <types.h>
#include <common.h>
#include <bef.h>
#include <befext.h>
#include <stdio.h>
#include "rplagent.h"
#include "pcn.h"

/*
 * External PCI access functions
 */

ushort  Inetboot_IO_Addr,
	Inetboot_Mem_Base,
	Inetboot_IRQ_no;
unchar  MAC_Addr[6];
InstallSpace(Name, 1);
InstallSpace(Slot, 1);
InstallSpace(Port, 1);
InstallSpace(Irq, 1);
InstallSpace(Mem, 1);
InstallSpace(Dma, 1);

/*
 *  Declarations and Module Linkage
 */

char    ident[9] = "PCnet   ",
	driver_name[8] = "pcn";

#if defined(PCNDEBUG)
int DebugFlag = 1;
#else
int DebugFlag;
#endif  /* defined(PCNDEBUG) */

/*
 * This is an experimental feature not required in the
 * realmode boot; leave it undefined.
 */
#define	PCN_GET_CHIP_ID

/*
 * []----------------------------------------------------------[]
 * |		Start of Prototype Area				|
 * []----------------------------------------------------------[]
 */
static	void	pcn_SetupAdapter(void);
static	int	pcn_ProcessReceive(int);
static	void	pcn_ReceiveISR(void);
static	void	pcn_TransmitISR(void);
static	int	pcn_get_pic_irr(ushort);
static	void	pcn_StopLANCE(void);
static	void	pcn_InitData(void);
static	void	pcn_goose_LANCE(void);
static	int	pcn_ProbeDMA(void);
static	int	pcn_iob_find(ushort);
static	void	pci_pcn(void);
static	void	pcn_ResetLANCE(void);
static	ushort	pcn_InCSR(ushort);
static	void	pcn_OutCSR(ushort, ushort);
static	ulong	pcn_OffToLin(void *ptr); /* ... was far pointer */
static	void	pcn_MakeDesc(struct PCN_MsgDesc *, ulong, ushort, ushort);
static void 	scan_pci_pcn(void);
#ifdef	PCN_GET_CHIP_ID
static	ulong	pcn_GetChipID();
#endif
static	void	pcn_ResetRXDesc(struct PCN_MsgDesc *); /* was a far pointer */
#if	defined(PCNDEBUG)
static	void	pcn_dumpframe(unchar *, ushort);
#endif  /* defined(PCNDEBUG) */
/*
 * []----------------------------------------------------------[]
 * |		End of Prototype Area				|
 * []----------------------------------------------------------[]
 */

/*
 * []----------------------------------------------------------[]
 * | Start of Debug Area					|
 * []----------------------------------------------------------[]
 */
/* #define DEBUG /* */
#ifdef DEBUG
# pragma	message (__FILE__ ": << WARNING! DEBUG MODE >>")

# define	Dprintf(f, x) if (DebugPcn & PCN_##f) printf x
# define	PCN_FLOW	0x0001	/* ... general flow of program */
# define	PCN_ERR		0x0002	/* ... error conditions */
# define	PCN_BIOS	0x0004	/* ... data on bios calls */
# define	PCN_FLOW_IRQ	0x0008	/* ... info. during interrupts */
# define	PCN_PROBE	0x0010	/* ... info. during isa probes */
# define	PCN_ALL		-1	/* ... enable all flags */
int	DebugPcn = ~PCN_FLOW_IRQ;
#else
# define	Dprintf(f, x)
#endif
/*
 * []----------------------------------------------------------[]
 * | End of Debug Area						|
 * []----------------------------------------------------------[]
 */
/*
 * Default entries in this array are for ISA bus devices.  If a PCI bus
 * device is discovered with the same iobase as one of the ISA entries,
 * then the ISA entry is re-written to describe a PCI entry. The
 * PCN_BUS_NONE slots can be used by devices that are autoconfigured, and have
 * InstallConfig() called for them.  Note that this allows PnP ISA devices
 * to have any IO address , and they are not limited as the non-PnP devices
 * are 
 */
struct pcnIOBase pcn_iobase[PCN_IOBASE_ARRAY_SIZE] = {
	{ 0x300, PCN_BUS_ISA, 0 },
	{ 0x320, PCN_BUS_ISA, 0 },
	{ 0x340, PCN_BUS_ISA, 0 },
	{ 0x360, PCN_BUS_ISA, 0 },
	{ 0, PCN_BUS_NONE, 0 },
	{ 0, PCN_BUS_NONE, 0 },
	{ 0, PCN_BUS_NONE, 0 },
	{ 0, PCN_BUS_NONE, 0 },
	{ 0, PCN_BUS_NONE, 0 },
	{ 0, PCN_BUS_NONE, 0 },
	{ 0, PCN_BUS_NONE, 0 },
};

/*
 * These are here for the benefit of ISA bus
 */
static ushort   pcn_irq_map[] = { 3, 4, 5, 9, 10, 11, 14, 15 };
static ushort   pcn_dma_map[] = { 5, 3, 6, 7};

static volatile int	pcn_init_done;
extern int	Media_Type;

struct pcnInstance	pcns,
			*pcnp;

/*
 * Receiver variables
 */
unchar far	*Rx_Buf;
ushort		Rx_Len;
recv_callback_t	Rx_Callback;
int		Rx_Enable;

/*
 * ISAAddr -- given instance 'idx' return port/size pair
 *		ADATER_TABLE_OK = okay, ADAPTER_TABLE_END = stop
 */
ISAAddr(short idx, short *Port, short *Portsize)
{
	*Port = pcn_iobase[idx].iobase;
	*Portsize = 0x10;
	return (pcn_iobase[idx].iobase ? ADAPTER_TABLE_OK : ADAPTER_TABLE_END);
}

/*
 * ISAProbe -- given the port see if a non-Plug-and-Play card exists
 *		ADAPTER_FOUND = okay, ADAPTER_NOT_FOUND = fail
 */
ISAProbe(short Port)
{
	int	csum, j, i;
	ushort  iobase;

	Dprintf(PROBE, ("(pcn probe 0x%x)", Port));
	if (pcnp == 0) {
		(void *) iobase = (void *) &pcns;
		iobase = (iobase+7) & 0xfff8;   /* this can't segment wrap */
		pcnp = (void *) iobase;		/* if small model is used */
	}
	pcnp->iobase = Port;    /* so calls to pcn_[In,Out]CSR() work */

	csum = 0;

	pcn_ResetLANCE();

	if (inb(Port + 14) != 'W' || inb(Port +15) != 'W') {
		Dprintf(PROBE, ("(pcn probe: bad value at 14/15)"));
		return (ADAPTER_NOT_FOUND);
	}

	for (j = 0; j < 12; j++)
		csum += inb(Port+j);

	csum += inb(Port+14);
	csum += inb(Port+15);

	if ((csum == 0) || (csum == inw(Port + 12))) {
		Dprintf(PROBE, ("(pcn found card)"));
	} else {
		Dprintf(PROBE, ("(pcn probe: invalid csum)"));
		return (ADAPTER_NOT_FOUND);
	}
	if (!pcn_ProbeDMA())
		return ADAPTER_NOT_FOUND;

	return (ADAPTER_FOUND);
}

/*
 * ISAIdentify -- given the port return as much info as possible
 *		routine doesn't get called because ISAProbe always
 *		returns ADAPTER_NOT_FOUND;
 */
void
ISAIdentify(short Port, short *Irq, short *Mem, short *Memsize)
{
	int	len,
		i,
		interrupt_status;
	unchar  oldpicmask1, oldpicmask2,
		picmask1, picmask2,
		temp;

	/*
	 * pcn_goose_LANCE() needs us to fill in iobase in the
	 * pcnInstance structure.  Kinda ugly, sorry.
	 */
	pcnp->iobase = Port;

	*Mem = 0;
	*Memsize = 0;

	/*
	 * Determine which IRQ is being used
	 */

	/*
	 * Mask off all interrupts the LANCE can generate
	 */
	len = sizeof (pcn_irq_map) / sizeof (pcn_irq_map[0]);

	picmask1 = oldpicmask1 = inb(0x21);
	picmask2 = oldpicmask2 = inb(0xa1);

	for (i = 0; i < len; i++) {
		if (pcn_irq_map[i] > 7)
			picmask2 |= (1<<(pcn_irq_map[i]-8));
		else
			picmask1 |= (1<<pcn_irq_map[i]);
	}

	outb(0x21, picmask1);
	outb(0xa1, picmask2);

	/*
	 * Goose the LANCE to inspire an interrupt
	 */
	pcn_goose_LANCE();

	/*
	 * Now look to see if the IRR is set
	 */

	for (i = 0; i < len; i++) {
		if (pcn_get_pic_irr(pcn_irq_map[i])) {
			/*
			 * Disable Interrupt enable and make sure the
			 * interrupt goes away.
			 */
			pcn_OutCSR(CSR0, 0);
			if (!pcn_get_pic_irr(pcn_irq_map[i])) {
				break;
			}
			/*
			 * If the interrupt didn't go away, it wasn't ours so
			 * re-enable Interrupt Enable and keep looking.
			 */
			pcn_OutCSR(CSR0, CSR0_INEA);
		}
	}

	/*
	 * Shut the LANCE up, now that we're done
	 */
	pcn_ResetLANCE();

	outb(0x21, oldpicmask1);
	outb(0xa1, oldpicmask2);

	if (i >= len) {
		/*
		 * Couldn't find IRQ. Is there anything that we can
		 * do here?
		 */
		*Irq = 0;
		Dprintf(ERR, ("(pcn identify: failed to find irq)"));
	} else {
		*Irq = (ushort) pcn_irq_map[i];
	}
	return;
}

/*
 * InstallConfig -- setup internal structures for card at given resources
 *		0 = okay, !0 = failure
 */
InstallConfig(void)
{
	int	Idx,		/* ... used to find matching iobase */
		iobase,		/* ... setup */
		Cookie,		/* ... pci id value */
		Reg;		/* ... command reg for test Master Enable */
	DWORD   val[1], len;

	/*
	 * First time through, set up the pcnp ptr on a
	 * q-word aligned boundary.
	 */
	if (pcnp == 0) {
		(void *) iobase = (void *) &pcns;
		iobase = (iobase+7) & 0xfff8;
		pcnp = (void *) iobase;
	}

	if ((Idx = pcn_iob_find((ushort)Port_Space[0])) == -1)
		return (1);

	pcn_iobase[Idx].iobase = (ushort)Port_Space[0];

	switch (Name_Space[1]) {
	case RES_BUS_PCI:

		len = 1;
		if (get_res("addr", val, &len) != RES_OK) return (1);
		Cookie = (int)val[0];

		if (!pci_read_config_word(Cookie, 0x04, &Reg)) return (1);
		if ((Reg & 5) == 1) {
			/*
			 * If the BIOS didn't set the Bus Master Enable
			 * (bit 2) yet the I/O Enable is set (bit 0) enable
			 * the BME bit.
			 */

			Dprintf(FLOW, ("Enabling the BME bit\n"));
			pci_write_config_word(Cookie, 0x04, Reg | 4);

		} else {
			Dprintf(FLOW, ("Cmd bit 0x%x\n", Reg));
		}

		pcn_iobase[Idx].bustype = PCN_BUS_PCI;
		pcn_iobase[Idx].cookie = Cookie;

		net_dev_pci(PCI_COOKIE_TO_BUS(Cookie), PCI_AMD_VENDOR_ID,
			    PCI_PCNET_ID, PCI_COOKIE_TO_DEV(Cookie),
			    PCI_COOKIE_TO_FUNC(Cookie));
		break;

	case RES_BUS_PNPISA: case RES_BUS_ISA:

		pcn_iobase[Idx].bustype = PCN_BUS_ISA; 
		break;
	}
	return (0);
}

void
AdapterIdentify(short index, ushort *ioaddr, ushort *irq,
		ushort *rambase, ushort *ramsize)
{
	unchar  irqline;

	if ((index > PCN_IOBASE_ARRAY_SIZE) ||
	    (pcn_iobase[index].bustype == PCN_BUS_NONE))
		return;

	*ioaddr = pcn_iobase[index].iobase;
	*rambase = 0;
	*ramsize = 0;

	if (pcn_iobase[index].bustype == PCN_BUS_PCI) {
		if (pci_read_config_byte((ushort)pcn_iobase[index].cookie,
		    0x3c, &irqline)) {
			*irq = irqline;
			net_dev_pci(
				PCI_COOKIE_TO_BUS(pcn_iobase[index].cookie),
				PCI_AMD_VENDOR_ID,
				PCI_PCNET_ID,
				PCI_COOKIE_TO_DEV(pcn_iobase[index].cookie),
				PCI_COOKIE_TO_FUNC(pcn_iobase[index].cookie));
		}
	}
	else
		ISAIdentify(*ioaddr, irq, rambase, ramsize);
}

/*
 * probe -- Determine if a device is present
 */
AdapterProbe(short index)
{
	ulong   chip_id;
	ushort  iobase;

	/*
	 * First time through, set up the pcnp ptr on a
	 * q-word aligned boundary.
	 */
	if (pcnp == 0) {
		/*
		 * This can't segment wrap as long as small mem model is used
		 */

		(void *) iobase = (void *) &pcns;
		iobase = (iobase+7) & 0xfff8;
		pcnp = (void *) iobase;
	}

	/*
	 * Look for any PCI devices and add them to the
	 * IOBase table.
	 */
	scan_pci_pcn();

	if ((index > PCN_IOBASE_ARRAY_SIZE) ||
	    (pcn_iobase[index].bustype == PCN_BUS_NONE))
		return (ADAPTER_TABLE_END);

	if ((pcn_iobase[index].bustype == PCN_BUS_ISA) &&
	    (ISAProbe(pcn_iobase[index].iobase) == ADAPTER_NOT_FOUND)) {
		return (ADAPTER_NOT_FOUND);
	}

	/*
	 * Check to see if our adapter is a PC-Net/ISA
	 */
adapter_found:
#if defined(PCN_GET_CHIP_ID)
	chip_id = pcn_GetChipID();
#endif  /* defined(PCN_GET_CHIP_ID) */

	return (ADAPTER_FOUND);
}

int AdapterInit(ushort ioaddr, ushort irq, ushort rambase, ushort ramsize)
{
	int i;

	Media_Type = MEDIA_ETHERNET;
	Inetboot_IO_Addr  = ioaddr;
	Inetboot_IRQ_no   = irq;
	Inetboot_Mem_Base = rambase;
	pcnp->iobase = ioaddr;
	pcnp->irq = irq;

	Dprintf(FLOW, ("AdapterInit: io=%x irq=%x ram=%x-%x\n", 
			ioaddr, irq, rambase, rambase + ramsize - 1));
	/* copy node address */
	for (i = 0; i < 6; i++)
		MAC_Addr[i] = inb(ioaddr+i);

	i = pcn_iob_find(ioaddr);
	if (i < 0)
		return (0);		/* don't know this device */

	/*
	 * If ISA, figure out which DMA channel is being used
	 * ...and configure it in cascade mode to allow ISA bus mastering.
	 */
	switch (pcn_iobase[i].bustype)
	{
	case PCN_BUS_ISA:

#ifdef PRESOLARIS2_6
		if(!pcn_ProbeDMA())
			return (0);
#else   /* PRESOLARIS2_6 */
		pcnp->dma = (ushort) Dma_Space[0];
#endif  /* PRESOLARIS2_6 */
		Dprintf(PROBE,("ISA Card: DMA channel:0x%X\n", pcnp->dma));
		if (pcnp->dma > 4) {
			outb(PCN_DMA_2_MODE_REGS,
				(unchar) (PCN_CASCADE | (pcnp->dma-4)));
			outb(PCN_DMA_2_MASK_REGS, (unchar) (pcnp->dma-4));
		} else {
			outb(PCN_DMA_1_MODE_REGS,
				(unchar) (PCN_CASCADE | pcnp->dma));
			outb(PCN_DMA_1_MASK_REGS, (unchar) pcnp->dma);
		}
		break;
	case PCN_BUS_PCI:
		break;
	default:
		break;

	}
	/* setup the board for the base address and IRQ configurations */
	pcn_InitData();
	pcn_SetupAdapter();

	/* 
	 * Originally the code disabled interrupts here, and waited for
	 * CSR0_INTR to toggle. It turns out that milliseconds() re-enables
	 * interrupts (by way of invoking int 15h func 86h), so the interrupt
	 * handler was triggered, and was immediately clearing the status bits.
	 * This way, we get to use CSR0's IDON bit as a status, which is a
	 * more accurate check on the adapter status
	 */

	pcn_init_done = 0;
	pcn_OutCSR(CSR0, CSR0_INIT | CSR0_INEA);	/* start the init */
	intr_enable();

	/*
	 * wait for interrupt handler to signal that the adapter generated an
	 * initialization done interrupt.
	 */
	for (i = 0; i < 1000 && !pcn_init_done; i++)
		milliseconds(1);

	if (pcn_init_done)
		return 1;

	Dprintf (ERR, ("Could not initialize PCN adapter"));
	return 0;
}

static void
pcn_SetupAdapter(void)
{
	int	len;
	int	i, j, index;
	ushort  iobase;
	ulong   linaddr = pcnp->linear_base_address;


	pcn_ResetLANCE();

	pcn_OutCSR(CSR1, (ushort) linaddr & 0xfffe);
	pcn_OutCSR(CSR2, (ushort) (linaddr>>16) & 0xff);

	return;
}

void AdapterOpen()
{
	int	i;

	/*  start the board and enable the interrupt   */
	intr_enable();
	pcn_OutCSR(CSR0, CSR0_STRT | CSR0_INEA);

	/*
	 * wait for the LANCE to start
	 */
	for (i = 0; i < 1000; i++) {
		milliseconds(1);
		if (pcn_InCSR(CSR0) & CSR0_STRT)
			return;
	}
}

void AdapterClose()
{
	pcn_StopLANCE();
}

/*
 *
 */
void AdapterSend(char __far *pkt, ushort packet_length)
{
	int	i, index;

	/* Discard a frame larger than the Ether MTU */
	if (packet_length > ETH_MAX_TU)
		return;

	/* Pad a frame smaller than the Ether minimum */
	if (packet_length < ETH_MIN_TU)
		packet_length = ETH_MIN_TU;

	/*
	 * Look for a free buffer in TX ring.  It really
	 * doesn't matter where we start, since any buffer
	 * owned by the host (that's us) is big enough.
	 */
	for (i = 0, index = pcnp->tx_index; i < PCN_TX_RING_SIZE;
	    i++, index = NextTXIndex(index)) {
		if (!(pcnp->tx_ring[index].MD[1] & MD1_OWN))
			break;  /* got one */
	}
	if (i >= PCN_TX_RING_SIZE) {
		pcnp->tx_no_buff++;
		return;		/* couldn't find a TX descriptor! */
	}

	/*
	 * Copy the packet into the buffer
	 */
	bcopy((char __far *) &pcnp->tx_buffer[index], pkt, packet_length);
	pcnp->tx_ring[index].MD[2] = (-packet_length) | 0xf000;
	pcnp->tx_ring[index].MD[3] = 0;		/* clear the status field */
	pcnp->tx_ring[index].MD[1] |= MD1_TXFLAGS; /* must be last */

	/*
	 * Kick the LANCE into immediate action
	 */
	pcn_OutCSR(CSR0, pcn_InCSR(CSR0) | CSR0_TDMD);

	pcnp->tx_index = NextTXIndex(index);

	return;
}


/*
 *
 */
void
AdapterInterrupt(void)
{
	ushort  interrupt_status;
	ulong   index;

	intr_disable();

	interrupt_status = pcn_InCSR(CSR0);
	if (!(interrupt_status & CSR0_INTR))
		return;

	Dprintf(FLOW_IRQ, ("i"));
	while (interrupt_status &
	    (CSR0_TINT | CSR0_RINT | CSR0_IDON | CSR0_MERR | CSR0_BABL |
	    CSR0_MISS)) {
		/* clear cause of interrupt */
		pcn_OutCSR(CSR0, interrupt_status);

		/* babble error interrupt */
		if (interrupt_status & CSR0_BABL) {
			Dprintf(FLOW_IRQ, ("BE,"));
			/* Do a hot reset */
			pcn_StopLANCE();
			pcn_InitData();
			pcn_SetupAdapter();
			pcn_OutCSR(CSR0, CSR0_STRT | CSR0_INEA);
		}

		/* memory error interrupt */
		if (interrupt_status & CSR0_MERR) {
			Dprintf(FLOW_IRQ, ("ME,"));
			/* Do a hot reset */
			pcn_StopLANCE();
			pcn_InitData();
			pcn_SetupAdapter();
			pcn_OutCSR(CSR0, CSR0_STRT | CSR0_INEA);
		}

		/*    receive interrupt   */
		if (interrupt_status & CSR0_RINT) {
			pcn_ReceiveISR();
		}

		/*    overflow receive the packets   */
		if (interrupt_status & CSR0_MISS) {
			Dprintf(FLOW_IRQ, ("MI,"));
			pcn_ReceiveISR();
		}
		if (interrupt_status & CSR0_IDON) {
			pcn_init_done=1;
			Dprintf(FLOW_IRQ, ("ID,"));
		}

		/*    transmit    over    */
		if (interrupt_status & CSR0_TINT) {
			pcn_TransmitISR();
		}

		/* NEEDSWORK: update stats counters for errors? */

		interrupt_status = pcn_InCSR(CSR0);
	}

	/*
	 * EOI the PIC(s)
	 */
	outb(0x20, 0x20);
	if (Inetboot_IRQ_no > 7)
		outb(0xa0, 0x20);

	intr_enable();
	return;
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
 * Starting with the buffer described by index, process received
 * frames out of the ring.  Return the index of the last descriptor
 * processed.  This assumes the STP bit is set in the descriptor
 * pointed at by index.
 */
static int
pcn_ProcessReceive(int index)
{
	struct PCN_MsgDesc *mdp;
	ushort  pktlen;


	/*
	 * Simplifying assumption: if the first descriptor isn't
	 * an entire frame, give it back to the LANCE and return.
	 */
	mdp = &pcnp->rx_ring[index];

	if (mdp->MD[1] & MD1_ERR) {
		pcn_ResetRXDesc(mdp);
		return (index);
	}

	/*
	 * If this is the first frame of a chained message,
	 * reset all the descriptors until the ENP is found set
	 */
	if (!(mdp->MD[1] & MD1_ENP)) {
		ushort i;

		for (i = 0; i < PCN_RX_RING_SIZE; i++) {
			int enp;

			enp = mdp->MD[1] & MD1_ENP;
			pcn_ResetRXDesc(mdp);
			if (enp)
				break;
			index = NextRXIndex(index);
			mdp = &pcnp->rx_ring[index];
		}
		return (index);
	}


	/*
	 * Check frame size and transfer it if receiver is enabled
	 */
	if (Rx_Enable) {
		pktlen = mdp->MD[3];
		pktlen -= LANCE_FCS_SIZE;	/* discard FCS at end */

		if (pktlen > Rx_Len) {
			goto done;
		}
		bcopy(Rx_Buf, (char __far *) &pcnp->rx_buffer[index], pktlen);

		Rx_Enable = 0;
		Rx_Callback((int) pktlen);
	}

	/*
	 * Reset the frame descriptor  and return it to the LANCE
	 */
done:
	pcn_ResetRXDesc(mdp);
	return (index);
}

/*
 * Receive interrupt handler - only called at interrupt time.
 * Look at the receiver ring and transfer a received frame if
 * an AdapterReceive() is outstanding.

 * Cleanup for error frames, record statistics and return buffers to
 * the LANCE.
 */
void
pcn_ReceiveISR(void)
{
	int	i, index;

	Dprintf(FLOW_IRQ, ("(r)"));
	for (i = 0, index = pcnp->rx_index; i < PCN_RX_RING_SIZE;
	    i++, index = NextRXIndex(index)) {
		if (!(pcnp->rx_ring[index].MD[1] & MD1_OWN)) {
			/*
			 * we own it?
			 */

			if (pcnp->rx_ring[index].MD[1] & MD1_STP) {
				/*
				 * first desc?
				 */

				index = pcn_ProcessReceive(index);
				pcnp->rx_index = index;
			}
		}
	}

	if (i >= PCN_RX_RING_SIZE) {
		return;		/* got an RINT but no frame? */
	}
}


/*
 * Transmit interrupt handler - only called at interrupt time.
 * Look for buffers in the ring which are owned by us but have
 * status indicating transmit errors in MD3, perform error recovery
 * and carry on.
 */
static void
pcn_TransmitISR(void)
{

	/*
	 * NEEDSWORK: look for errors which require recovery and
	 * do it.  May wish to record statistics.
	 */
	return;
}


void
AdapterReceive(unchar far * buffer, ushort buflen, recv_callback_t callback)
{


	Rx_Buf = buffer;
	Rx_Len = buflen + 14; /* NEEDSWORK: upper layer tells us 1500?? */
	Rx_Callback = callback;
	Rx_Enable = 1;
	intr_enable();
}


/*
 * Return 1 if specified IRQ is being requested, 0 otherwise.
 */
int
pcn_get_pic_irr(ushort n)
{
	ushort  picport;
	unchar  irr;

	if (n > 7) {
		picport = 0xa0;
		n -= 8;
	} else {
		picport = 0x20;
	}

	outb(picport, 0x0a);    /* give me the IRR please */
	irr = inb(picport);
	return ((irr & (1 << n)) != 0);
}

/*
 *
 */
static void
pcn_StopLANCE(void)
{
	pcn_OutCSR(CSR0, CSR0_STOP);	/* also resets CSR0_INEA */
	while (!(pcn_InCSR(CSR0) & CSR0_STOP))
		milliseconds(1);	/* wait for the STOP to take */
	pcn_ResetLANCE();
}

/*
 * Initialize all data structures used to communicate with the LANCE.
 * The LANCE is stopped if necessary before changing the data.
 */
static void
pcn_InitData(void)
{
	ulong	linaddr;
	int	i;
	unchar	*p;

	pcn_StopLANCE();
	pcnp->linear_base_address = pcn_OffToLin(&pcnp->initblock);

	/*
	 * Default LANCE MODE setting is 0.
	 */
	pcnp->initblock.MODE = 0;

	/*
	 * Copy in the Ethernet address
	 */
	p = (unchar *) &pcnp->initblock.PADR[0];
	for (i = 0; i < ETH_ADDR_SIZE; i++)
		*(p++) = MAC_Addr[i];

	/*
	 * Fill the Multicast array with 0
	 */
	p = (unchar *) &pcnp->initblock.LADRF[0];
	for (i = 0; i < 8; i++)
		*(p++) = 0;

	/*
	 * Create the RX Ring Ptr
	 */
	linaddr = pcn_OffToLin(pcnp->rx_ring) |
		((ulong) PCN_RX_RING_VAL) << 29;
	pcnp->initblock.RDRA[0] = (ushort) linaddr;
	pcnp->initblock.RDRA[1] = (ushort) (linaddr >> 16);

	/*
	 * Create the TX Ring Ptr
	 */
	linaddr = pcn_OffToLin(pcnp->tx_ring) |
		((ulong) PCN_TX_RING_VAL) << 29;
	pcnp->initblock.TDRA[0] = (ushort) linaddr;
	pcnp->initblock.TDRA[1] = (ushort) (linaddr >> 16);

	/*
	 * Initialize the RX Ring
	 */
	for (i = 0; i < PCN_RX_RING_SIZE; i++) {
		linaddr = pcn_OffToLin(&pcnp->rx_buffer[i]);
		pcn_MakeDesc(&pcnp->rx_ring[i], linaddr,
			-PCN_RX_BUF_SIZE, MD1_OWN);
	}

	/*
	 * Initialize the TX Ring
	 */
	for (i = 0; i < PCN_TX_RING_SIZE; i++) {
		linaddr = pcn_OffToLin(&pcnp->tx_buffer[i]);
		pcn_MakeDesc(&pcnp->tx_ring[i], linaddr,
			-PCN_TX_BUF_SIZE, 0);
	}

	/*
	 * Initialize the ring pointers
	 */
	pcnp->tx_index = 0;
	pcnp->rx_index = 0;

}


/*
 *
 */
static void
pcn_goose_LANCE(void)
{
	int	i;

	pcn_InitData();
	pcn_SetupAdapter();

	/*
	 * Start initialization, enable an interrupt at completion
	 */
	pcn_OutCSR(CSR0, CSR0_INIT | CSR0_INEA);

	/*
	 * wait for the LANCE to finish the initialization sequence
	 */
	for (i = 0; i < 1000; i++) {
		milliseconds(1);
		if (pcn_InCSR(CSR0) & CSR0_INTR)
			return;
	}

	/*
	 * NEEDSWORK: initialization never completed
	 * This should never, ever, happen.
	 */
	return;
}


/*
 * Determine the DMA channel in use
 */
static int
pcn_ProbeDMA(void)
{
	int	len, i, j, dma_ok;
#ifndef PRESOLARIS2_6
	DWORD   val[2], TupleLen;
#endif  /* PRESOLARIS2_6 */

	len = sizeof (pcn_dma_map)/sizeof (pcn_dma_map[0]);
	for (i = 0; i < len; i++) {
		/*
		 * Select a DMA channel
		 */
#ifdef PRESOLARIS2_6
		pcnp->dma = pcn_dma_map[i];
#else  /* PRESOLARIS2_6 */
		val[0] = pcnp->dma = pcn_dma_map[i];
		val[1] = 0;
		TupleLen = DmaTupleSize;
		if (set_res("dma", val, &TupleLen, 0) != RES_OK) {
			Dprintf(PROBE, ("Cannot Reserve DMA Channel 0x%x\r\n",
			   val[0]));
			continue;
		}
#endif  /* PRESOLARIS2_6 */

		if (pcnp->dma > 4) {
			outb(PCN_DMA_2_MODE_REGS,
				(unchar) (PCN_CASCADE | (pcnp->dma-4)));
			outb(PCN_DMA_2_MASK_REGS, (unchar) (pcnp->dma-4));
		} else {
			outb(PCN_DMA_1_MODE_REGS,
				(unchar) (PCN_CASCADE | pcnp->dma));
			outb(PCN_DMA_1_MASK_REGS, (unchar) pcnp->dma);
		}

		/*
		 * Attempt to initialize the LANCE
		 */
		pcn_ResetLANCE();
		pcn_InitData();
		pcn_SetupAdapter();
		pcn_OutCSR(CSR0, CSR0_INIT);
		for (j = 0; j < 1000; j++) {
			milliseconds(1);
			if (pcn_InCSR(CSR0) & CSR0_INTR)
				break;
		}
		if (j >= 1000) {
			/*
			 * If this happens the board doesn't even respond to
			 * an initialisation command, we have polled it waiting
			 * for CSR0_INTR (Interrupt Status Bit) to indicate the
			 * completion of the Initialisation command.
			 */
			Dprintf(PROBE, ("\r\npcn_ProbeDMA, board did not "
			   "become ready\r\n"));
#ifndef PRESOLARIS2_6
			rel_res("dma", val, &TupleLen);
#endif  /* PRESOLARIS2_6 */
			return (0);
		}

		/*
		 * Check to see if the initialization
		 * succeeded (i.e., correct DMA)
		 */
		if ((dma_ok = (pcn_InCSR(CSR0) & CSR0_IDON)) != 0) {
			break;
		} else {
			Dprintf(PROBE, ("\r\nDMA Channel 0x%X failed,"
			   " releasing resource ..\r\n",pcnp->dma));
#ifndef PRESOLARIS2_6
			rel_res("dma", val, &TupleLen);
#endif  /* PRESOLARIS2_6 */
		}
	}

	if (!dma_ok) {
		putstr("PC-Net: could not identify DMA channel; failed.\n\r");
	}
	return (dma_ok);
}

/*
 * Given an iobase value, look for an entry in the pcn_iobase array
 * which matches.  If one is found, return the index.  If none are
 * found, return the index of the next available entry (bustype ==
 * PCN_BUS_NONE).  If no entries are available, return -1.
 */
static int
pcn_iob_find(ushort iobase)
{
	int	i;

	for (i = 0; i < PCN_IOBASE_ARRAY_SIZE; i++) {
		if ((pcn_iobase[i].iobase == iobase) ||
		    (pcn_iobase[i].bustype == PCN_BUS_NONE))
			return (i);
	}
	return (-1);
}

/*
 * Scan PCI space for all PCNet adapters and add them to the
 * list of possible IO base addresses.
 */
static void
scan_pci_pcn(void)
{
	ulong	base_addr_reg;
	ushort	cmd_reg;
	unchar	interrupt_line;
	ushort	id, iobase;
	int	i, j, found_one;
	static int scanned = 0; /* 0 if not scanned yet, 1 otherwise */

	if (scanned)
		return;		/* only do this once */

	scanned++;

	if (!is_pci())
		return;		/* all done if nothing to scan */

	i = 0;
	do {
		found_one = pci_find_device(PCI_AMD_VENDOR_ID,
						PCI_PCNET_ID, i, &id);
		if (found_one) {
			if (!pci_read_config_dword(id, 0x10, &base_addr_reg)) {
				goto next;	/* NEEDSWORK */
			}
			if (!pci_read_config_word(id, 0x04, &cmd_reg)) {
				goto next;	/* NEEDSWORK */
			}
			/*
			 * If the BIOS didn't set Bus Master Enable, but
			 * I/O Enable is set, then set Bus Master Enable.
			 */
			if (cmd_reg & 1) {
				if (!(cmd_reg & 4)) {
					cmd_reg |= 4;
					pci_write_config_word(id, 0x04,
						cmd_reg);
				}
			} else
				goto next;	/* NEEDSWORK */
			/*
			 * Update/add the iobase record
			 */
			iobase = base_addr_reg & 0xffe0;
			if ((j = pcn_iob_find(iobase)) < 0)
				return; /* all out of entries */

			pcn_iobase[j].iobase = iobase;
			pcn_iobase[j].bustype = PCN_BUS_PCI;
			pcn_iobase[j].cookie = id;
		}
next:
		i++;
	} while (found_one);

	return;
}

/*
 *
 */
void
pcn_ResetLANCE(void)
{
	inb(PCN_IO_RESET+pcnp->iobase);
	outb(PCN_IO_RESET+pcnp->iobase, 0);
	milliseconds(10);
}


/*
 *
 */
static ushort
pcn_InCSR(ushort csr)
{
	ushort  iobase = pcnp->iobase;

	outw(PCN_IO_RAP+iobase, csr);
	return (inw(PCN_IO_RDP+iobase));
}


/*
 *
 */
static void
pcn_OutCSR(ushort csr, ushort val)
{
	ushort  iobase = pcnp->iobase;

	outw(PCN_IO_RAP+iobase, csr);
	outw(PCN_IO_RDP+iobase, val);
}

static ulong
pcn_OffToLin(void *ptr)
{
	void __far *fp;

	fp = MK_FP(myds(), (ushort)ptr);
	return ((FP_SEG(fp)*16)+FP_OFF(fp));
}

/*
 *
 */
static void
pcn_MakeDesc(struct PCN_MsgDesc *mdp, ulong addr, ushort size,
	ushort flags)
{
	mdp->MD[0] = (ushort) addr;
	mdp->MD[1] = (flags & 0xff00) | ((ushort)(addr>>16) & 0xff);
	mdp->MD[2] = size | 0xf000;
	mdp->MD[3] = 0;
}

#ifdef PCN_GET_CHIP_ID
static ulong
pcn_GetChipID()
{
	ushort  idl, idh;

	idl = pcn_InCSR(CSR88);
	idh = pcn_InCSR(CSR89);
	return ((((ulong)idh)<<16) | (ulong) idl);
}
#endif /* defined(PCN_GET_CHIP_ID) */

/*
 *
 */
static void
pcn_ResetRXDesc(struct PCN_MsgDesc *mdp)
{
	/* reset received size */
	mdp->MD[3] = 0;

	/* reset buffer size */
	mdp->MD[2] = (-PCN_RX_BUF_SIZE) | 0xf000;

	/* always do this last */
	mdp->MD[1] = (mdp->MD[1]&0xff) | MD1_OWN;
}

#if defined(PCNDEBUG)
/*
 * Dump an ether frame
 */
static void
pcn_dumpframe(unchar *pkt, ushort len)
{
	ushort  i;

	putstr("\r\nreceived frame:\r\n");
	for (i = 0; i < len; i++) {
		put2hex(*pkt++);
		if ((i > 0) && ((i % 16) == 0))
			putstr("\r\n");
		else
			putstr(".");
	}
	putstr("\r\n");
}

#endif  /* if defined(PCNDEBUG) */
