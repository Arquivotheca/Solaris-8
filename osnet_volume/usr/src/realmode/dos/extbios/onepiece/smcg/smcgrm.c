/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident  "@(#)smcgrm.c 1.13	99/07/20 SMI\n"

/*
 * smcgrm -- SMC Generic Upper MAC realmode driver
 */

#include <sys/types.h>
#include <bef.h>
#include <befext.h>
#include <stdio.h>
#include <common.h>
#include "../rplagent.h"
#include SMC_INCLUDE
#include "../smcg/smcgrm.h"

/* required global variables */
char    ident[] = SMCG_IDENT;
char    driver_name[8] = SMCG_NAME;
ushort  Inetboot_IO_Addr = 0;
ushort  Inetboot_Mem_Base = 0;
unchar  MAC_Addr[ETHERADDRL] = {0};
InstallSpace(Name, 1);
InstallSpace(Slot, 1);
InstallSpace(Port, 1);
InstallSpace(Irq, 1);
InstallSpace(Mem, 1);
InstallSpace(Dma, 1);

#ifdef DEBUG
int DebugFlag = 1;			/* BOGUS spec says Debug_Flag */
#else
int DebugFlag = 0;
#endif

/* variables provided by the framework */
extern int	Network_Traffic;
extern int	Media_Type;		/* BOGUS spec says not extern */

/* variables for communication with the LM driver */
static Adapter_Struc smparam[SMNUMPORTS] = {0};
static Adapter_Struc *smp = &smparam[0];
static Data_Buff_Structure mb = {0};

#ifdef __9432__
static smhostram[SMNUMPORTS * (HOST_RAM_SIZE + HOST_RAM_ALIGNMENT)] = {0};
static smtxscratch[2048] = {0};
#endif

/* routines for communicating with the LM driver */
#ifdef __9432__
extern int LM_Initialize_Adapter(Adapter_Struc *, ulong);
#else
extern int	LM_Initialize_Adapter(Adapter_Struc *, void *);
#endif

extern int	LM_Open_Adapter(Adapter_Struc *);
extern int	LM_Close_Adapter(Adapter_Struc *);
extern int	LM_Send(Data_Buff_Structure _far *, Adapter_Struc *,
		    unsigned int);
extern int	LM_Service_Events(Adapter_Struc *);
#ifdef __9232__
extern int	LM_Receive_Copy(ulong, ushort, Data_Buff_Structure _far *,
		    Adapter_Struc *, int);
#else
extern int	LM_Receive_Copy(ushort, ushort, Data_Buff_Structure _far *,
		    Adapter_Struc *, int);
#endif

#if (defined(__8033__) || defined(__8232__))
void		UM_Receive_Packet(void *, ushort, Adapter_Struc *, int);
#endif
#if (defined(__9232__) || defined(__8416__))
void		UM_Receive_Packet(void _far *, ushort, Adapter_Struc *, ulong);
#endif
#ifdef __9432__
unsigned short UM_Receive_Packet(unchar *, ushort, Adapter_Struc *, ushort);
#endif

int		UM_Status_Change(Adapter_Struc *);
int		UM_Interrupt(Adapter_Struc *);
#ifdef __9432__
unsigned short UM_Send_Complete(ushort, Adapter_Struc *);
unsigned short UM_Receive_Copy_Complete(Adapter_Struc *);
#else
int		UM_Send_Complete(Adapter_Struc *);
#endif

#ifdef __9432__
int UM_PCI_Services(Adapter_Struc *, union RM_REGS *);
#endif

/* local variables */
static int	Listen_Posted = 0;
static unchar far *Rx_Buffer = 0;
static ushort	Rx_Buf_Len = 0;
static recv_callback_t Call_Back = 0;
static ushort	found_slot[MAX_ADAPTERS];
static ushort	found_iobase[MAX_ADAPTERS];
static ushort	found_type[MAX_ADAPTERS];
static ushort	found_irq[MAX_ADAPTERS];
static ushort	found_rambase[MAX_ADAPTERS];
static ushort	found_ramsize[MAX_ADAPTERS];
static struct {
	ushort	ioaddr;
	ushort	irq;
	ulong	ram;
} ancient[] = {	0x280, 3, 0xd0000,
		0x2a0, 5, 0xd4000,
		0x300, 5, 0xd4000,
		0x380, 7, 0xd0000,
		0x260, 5, 0xe0000 };

/*
 * ISAAddr -- return next valid I/O configuation for an 'smc' adapter
 *		ADATER_TABLE_OK = okay, ADAPTER_TABLE_END = stop
 */
ISAAddr(short Idx, short *Port, short *Portsize)
{
#if (defined(__8416__))
	if (LM_Nextcard(smp) != SUCCESS)
		return (ADAPTER_TABLE_END);

	*Port = smp->io_base;
	*Portsize = 0x20;

	return (ADAPTER_TABLE_OK);
#else
	return (ADAPTER_TABLE_END);
#endif
}

/*
 * ISAProbe -- given the port see if a non-Plug-and-Play card exists
 *		ADAPTER_FOUND = okay, ADAPTER_NOT_FOUND = fail
 */
ISAProbe(short port)
{
	int	rc;

#if (defined(__8416__))

	smp->io_base = port;
	smp->pc_bus = is_eisa() ? SMCG_EISA_BUS : SMCG_AT_BUS;
	rc = LM_GetCnfg(smp);
	if (rc == ADAPTER_AND_CONFIG)
		return (ADAPTER_FOUND);
	if (rc == ADAPTER_NO_CONFIG) {
		int	i;

		for (i = 0; i < sizeof (ancient) / sizeof (ancient[0]); i++)
			if (smp->io_base == ancient[i].ioaddr)
				return (ADAPTER_FOUND);
	}
#endif
	return (ADAPTER_NOT_FOUND);
}

/*
 * ISAIdentify -- given the port return as much info as possible
 */
void
ISAIdentify(short Port, short *Irq, short *Mem, short *Memsize)
{
	int	rc, i;

#if (defined(__8416__))

	smp->io_base = Port;
	smp->pc_bus = is_eisa() ? SMCG_EISA_BUS : SMCG_AT_BUS;
	rc = LM_GetCnfg(smp);
	if (rc == ADAPTER_AND_CONFIG) {
		*Irq = smp->irq_value;
		*Mem = smp->ram_base >> 4;
		*Memsize = smp->ram_usable;
		return;
	}
	if (rc == ADAPTER_NO_CONFIG) {
		/* ADAPTER_NO_CONFIG (ancient ISA 8003 cards) */
		for (i = 0;
		    i < sizeof (ancient) / sizeof (ancient[0]); i++) {
			if (smp->io_base == ancient[i].ioaddr) {
				*Irq = ancient[i].irq;
				*Mem = ancient[i].ram >> 4;
				*Memsize = 8;		/* 8K */
				return;
			}
		}
	}
#endif
}

/*
 * InstallConfig -- setup internal structures for card at given resources
 *			0 = okay, !0 = failure
 */
InstallConfig(void)
{
	ushort	rc, i;

#if (defined(__8033__) || defined(__9232__) || defined(__8232__))
	/* Eisa boards */
	smp->pc_bus = SMCG_EISA_BUS;
	smp->slot_num = Slot_Space[0];
	smp->io_base = (ushort)Slot_Space[0] * 0x1000;
#endif

#if (defined(__8416__))
	smp->pc_bus = (is_eisa() ? SMCG_EISA_BUS : SMCG_AT_BUS);
	smp->io_base = Port_Space[0];
#endif

#ifdef __9432__
	smp->pc_bus = SMCG_PCI_BUS;

	/*
	 * Call LM_GetCnfg for each possible PCI adapter, until we find the
	 * board which matches the IO base of the board we want to boot off
	 */
	while (LM_Nextcard(smp) == SUCCESS)
		if ((rc = LM_GetCnfg(smp)) == ADAPTER_AND_CONFIG &&
		    smp->io_base == Port_Space[0])
			break;

	if (i == MAX_ADAPTERS)
		return (1);
#else
	rc = LM_GetCnfg(smp);
#endif
	found_iobase[0] = smp->io_base;
	found_type[0] = rc;

	if (rc == ADAPTER_NO_CONFIG) {
		for (i = 0;
		    i < sizeof (ancient) / sizeof (ancient[0]); i++) {
			if (smp->io_base == ancient[i].ioaddr)
				found_slot[0] = i;
		}
	} else
		found_slot[0] = smp->slot_num;

	return ((rc == ADAPTER_AND_CONFIG ||
	    rc == ADAPTER_NO_CONFIG) ? 0 : 1);
}

/*
 * AdapterProbe - Make sure a device is really there (old-style boot).
 *
 * Probe the hardware at relative index "index".  If found, return 1
 * else return 0.
 */
int
AdapterProbe(short index)
{
	int i;
	ushort rc;

	if (index < 0 || index >= MAX_ADAPTERS) {
#ifdef DEBUG
		putstr(SMCG_NAME);
		putstr(" AdapterProbe: unexpected index 0x");
		puthex(index);
		putstr("\r\n");
#endif
		return (ADAPTER_TABLE_END);
	}

	/*
	 * Prepare the wrapper structure before calling.  The LMAC expects
	 * these to have been initialized.
	 */
#ifndef __9432__
	if (is_eisa()) {
		smp->pc_bus = SMCG_EISA_BUS;
	} else {
		smp->pc_bus = SMCG_AT_BUS;
	}
#else
	smp->pc_bus = SMCG_PCI_BUS;
#endif

	while (LM_Nextcard(smp) == SUCCESS) {
		rc = LM_GetCnfg(smp);
		if (rc == ADAPTER_AND_CONFIG || rc == ADAPTER_NO_CONFIG) {
#ifdef DEBUG
			if (DebugFlag) {
				putstr(SMCG_NAME);
				putstr(" AdapterProbe index 0x");
				puthex(index);
				putstr(" found adapter at 0x");
				puthex(smp->io_base);
				if (rc == ADAPTER_NO_CONFIG)
					putstr(" (no config)");
				putstr("\r\n");
			}
#endif
			found_type[index] = rc;
			found_iobase[index] = smp->io_base;
			if (rc == ADAPTER_AND_CONFIG) {
				found_slot[index] = smp->slot_num;
				found_irq[index] = smp->irq_value;
				found_rambase[index] = smp->ram_base>>4;
				found_ramsize[index] = smp->ram_usable;
#ifdef DEBUG
				putstr("Found Adapter: slot=0x");
				puthex(smp->slot_num);
				putstr(" iobase=0x");
				puthex(smp->io_base);
				putstr(" irq=0x");
				puthex(smp->irq_value);
				putstr("\r\n");
#endif
				return (ADAPTER_FOUND);
			}
			/* ADAPTER_NO_CONFIG (ancient ISA 8003 cards) */
			for (i = 0; i < sizeof (ancient) / sizeof (ancient[0]);
			    i++) {
				if (smp->io_base == ancient[i].ioaddr) {
					found_slot[index] = i;
					found_irq[index] = ancient[i].irq;
					found_rambase[index] =
					    ancient[i].ram>>4;
					found_ramsize[index] = 8; /* 8K */
					return (ADAPTER_FOUND);
				}
			}
		}
	}

	return (ADAPTER_TABLE_END);
}

void
AdapterIdentify(short index, ushort *ioaddr, ushort *irq,
    ushort *rambase, ushort *ramsize)
{
#ifdef __9432__
	ushort		i;
	static ushort	subven, subsys, id;
	static ulong	iobase;

	/* Get the pci id for this device from the system */
	for (i = 0; i < MAX_ADAPTERS; i++)
		if (pci_find_device(EPC_ID_SMC, EPC_ID_EPIC_100, i, &id)) {
			pci_read_config_dword(id, IOBASE_REG, &iobase);
			if ((iobase & 0xfffe) == found_iobase[index])
				break;
		}

	if (i == MAX_ADAPTERS) {
		putstr("Failed to identify device\r\n");
		return;
	}

	pci_read_config_word(id, SUBVENID, &subven);
	pci_read_config_word(id, SUBSYSID, &subsys);

	/* Announce ourselves to the framework */
	net_dev_pci(PCI_COOKIE_TO_BUS(id), subven, subsys,
	    PCI_COOKIE_TO_DEV(id), PCI_COOKIE_TO_FUNC(id));
#endif

	*ioaddr = found_iobase[index];
	*irq = found_irq[index];
	*rambase = found_rambase[index];
	*ramsize = found_ramsize[index];

#ifdef DEBUG
	if (DebugFlag) {
		putstr(SMCG_NAME);
		putstr(" AdapterIdentify index=0x");
		puthex(index);
		putstr(" ioaddr=0x");
		puthex(*ioaddr);
		putstr(" irq=0x");
		puthex(*irq);
		putstr("\r\n");
	}
#endif
}

int
AdapterInit(ushort ioaddr, ushort irq, ushort rambase, ushort ramsize)
{
	unsigned char c;
	int i;

	/* So that the boot-path can be constructed correctly */
	Inetboot_IO_Addr = 0;
	Inetboot_Mem_Base = 0;
	Media_Type = MEDIA_ETHERNET;

	/*
	 * Prepare the wrapper structure before calling.  The LMAC expects
	 * these to have been initialized.
	 */
#ifndef __9432__
	if (is_eisa()) {
		smp->pc_bus = SMCG_EISA_BUS;
	} else {
		smp->pc_bus = SMCG_AT_BUS;
	}
#else
	smp->pc_bus = SMCG_PCI_BUS;
#endif

	smp->io_base = ioaddr;
	for (i = 0; i < MAX_ADAPTERS; i++) {
		if (ioaddr == found_iobase[i]) {
			smp->slot_num = found_slot[i];
			break;
		}
	}
#ifdef DEBUG
	if (i == MAX_ADAPTERS) {
		putstr(SMCG_NAME);
		putstr(" AdapterInit cannot find ioaddr in table! 0x");
		puthex(ioaddr);
		putstr("\r\n");
	}
#endif

	LM_GetCnfg(smp);

	/*
	 * We can't read the configuration on ADAPTER_NO_CONFIG cards,
	 * so we support some documented permissible configurations.
	 */
	if (found_type[i] == ADAPTER_NO_CONFIG) {
		smp->irq_value = ancient[found_slot[i]].irq;
		smp->ram_base = ancient[found_slot[i]].ram;
		smp->ram_size = smp->ram_usable = 8; /* 8K */
	}

	smp->ram_access = (ulong)MK_FP(smp->ram_base >> 4, 0);
	/* 9432 mishandles having only one element in tx descriptor list */
#ifndef __9432__
	smp->num_of_tx_buffs = 1;
#endif
	smp->receive_mask |= ACCEPT_BROADCAST;
	smp->max_packet_size = SMMAXPKT;

#ifdef __9432__
	smp->host_ram_virt_addr = (unsigned char *)smhostram +
	    HOST_RAM_ALIGNMENT - (FP_OFF(smhostram) % HOST_RAM_ALIGNMENT);
	smp->host_ram_phy_addr = PTR2PHYSADDR(smhostram) +
	    HOST_RAM_ALIGNMENT -
	    (PTR2PHYSADDR(smhostram) % HOST_RAM_ALIGNMENT);
	smp->tx_scratch = PTR2PHYSADDR(smtxscratch);
#endif

	for (i = 1; i < SMNUMPORTS; i++) {
		smparam[i] = smparam[0];	/* init secondary port struct */
	}

	/*
	 * The UMAC/LMAC spec says we have to set up the interrupt vector
	 * and enable the system interrupt for the adapter before calling
	 * LM_Initialize_Adapter().  However this causes a hang on a soft
	 * reset on some cards when interrupts are generated before the LM
	 * code is ready to handle them correctly.  Issue to be resolved
	 * with SMC.
	 */
	i = LM_Initialize_Adapter(smp, 0);
	intr_enable();

	if (i != SUCCESS) {
		putstr(SMCG_NAME);
		putstr(" LM_Initialize_Adapter failed 0x");
		puthex(i);
		putstr("\r\n");
		return (-1);
	}

	/*
	 * We have no way to address secondary ports via the framework,
	 * or to tell the Solaris driver to use a secondary port, so we
	 * must always use the first port for now.  If this ever changes,
	 * we can modify smp to point at the selected smparam element.
	 * This must be done after the call to LM_Initialize_Adapter,
	 * because LM_Initialize_Adapter expects to be passed a pointer
	 * to the first contiguous smparam struct.
	 *
	 * smp = &smparam[0];
	 */

#ifdef DEBUG
	putstr("Adapter Init: slot=0x");
	puthex(smp->slot_num);
	putstr(" iobase=0x");
	puthex(smp->io_base);
	putstr(" irq=0x");
	puthex(smp->irq_value);
	putstr("\r\n");
	if (DebugFlag) {
		putstr(SMCG_NAME);
		putstr(" AdapterInit: Ethernet address");
		for (i = 0; i < ETHERADDRL; i++) {
			putstr(":");
			put2hex(smp->node_address[i]);
		}
		putstr("\r\n");
	}
#endif
	for (i = 0; i < ETHERADDRL; i++)
		MAC_Addr[i] = smp->node_address[i];

	return (0);
}

void
AdapterOpen(void)
{
	ushort rc;
#ifdef DEBUG
	if (DebugFlag) {
		putstr(SMCG_NAME);
		putstr(" AdapterOpen: smp=");
		putptr((unsigned char *)smp);
		putstr("\r\n");
	}
#endif
	rc = LM_Open_Adapter(smp);
#ifdef DEBUG
	if (rc != SUCCESS) {
		putstr(SMCG_NAME);
		putstr(" LM_Open_Adapter() returned 0x");
		puthex(rc);
		putstr("\r\n");
	}
#endif
}

void
AdapterClose(void)
{
#ifdef DEBUG
	if (DebugFlag) {
		putstr(SMCG_NAME);
		putstr(" AdapterClose\r\n");
	}
#endif
	LM_Close_Adapter(smp);
}

void
AdapterSend(char far *packetbuf, ushort pktlength)
{
#ifdef DEBUG_SEND
	if (DebugFlag) {
		putstr(SMCG_NAME);
		putstr(" AdapterSend: length=0x");
		puthex(pktlength);
		putstr(" Buffer is at 0x");
		putptr(packetbuf);
		putstr("\r\n");
	}
#endif
	mb.fragment_count = 1;
#ifdef __9432__
	mb.fragment_list[0].fragment_ptr = PTR2PHYSADDR(packetbuf);
	mb.fragment_list[0].fragment_length = pktlength | PHYSICAL_ADDR;
#else
	mb.fragment_list[0].fragment_ptr = (ulong)packetbuf;
	mb.fragment_list[0].fragment_length = pktlength;
#endif

	if (pktlength > SMMAXPKT) {
#ifdef DEBUG
		putstr(SMCG_NAME);
		putstr(" AdapterSend: length=0x");
		puthex(pktlength);
		putstr(" exceeds maximum -- dropping\r\n");
#endif
		return;
	}

	if (pktlength < ETHERMIN) {
#ifdef DEBUG
		putstr(SMCG_NAME);
		putstr(" AdapterSend: length=0x");
		puthex(pktlength);
		putstr(" not large enough -- padding\r\n");
#endif
		pktlength = ETHERMIN;
	}

	pktlength = (pktlength+1) & ~1; /* even size, please */

#ifdef DEBUG_SEND_LONG
	if (DebugFlag) {
		int i;
		putstr(SMCG_NAME);
		putstr(" AdapterSend: sending length=0x");
		puthex(pktlength);
		putstr("\r\nS>>> ");
		for (i = 0; i < 30; i++) {
			put2hex(*(((char far *)packetbuf+i)));
			putstr(" ");
		}
		putstr("\r\n");
/*
		putstr("[PRESS KEY]");
		kbchar();
*/
	}
#endif

	LM_Send((Data_Buff_Structure _far *)&mb, smp, pktlength);
}

void
AdapterReceive(unchar _far *buffer, ushort buflen, recv_callback_t func)
{
#ifdef DEBUG_RECV
	if (DebugFlag) {
		putstr(SMCG_NAME);
		putstr(" AdapterReceive: ");
	}
#endif
	Listen_Posted = 1;
	Call_Back = func;
	Rx_Buffer = buffer;
	Rx_Buf_Len = buflen;

	/* bagbiting realmode driver framework passes 1500 */
	if (Rx_Buf_Len < ETHERMAX)
		Rx_Buf_Len = ETHERMAX;
}

void
AdapterInterrupt(void)
{
	intr_disable();
	set_ds();

#ifdef DEBUG_INTR
	if (DebugFlag) {
		putstr(SMCG_NAME);
		putstr(" ISR: ");
	}
#endif
	LM_Service_Events(smp);

	outb(0x20, 0x20);
	if (smp->irq_value > 7) {
		outb(0xA0, 0x20);
	}
	intr_enable();
}

void interrupt
AdapterISR()
{
	AdapterInterrupt();
}

/*
 * Following are routines to interface with the LMAC
 */

/* Called from LM_Service_Events() to receive a packet */
#if (defined(__8033__) || defined(__8232__))
void
UM_Receive_Packet(void * look, ushort length, Adapter_Struc * smp, int stat)
#endif
#if (defined(__9232__) || defined(__8416__))
void
UM_Receive_Packet(void _far * look, ushort length, Adapter_Struc * smp,
    ulong stat)
#endif
#ifdef __9432__
unsigned short UM_Receive_Packet(unchar *look, ushort length,
    Adapter_Struc * smp, ushort stat)
#endif
{
#ifdef DEBUG_RECV
	if (DebugFlag) {
		putstr(SMCG_NAME);
		putstr(" UM_Receive_Packet: length=0x");
		puthex(length);
		putstr(" Rx_Buf_Len=0x");
		puthex(Rx_Buf_Len);
		putstr(" Listen_Posted=0x");
		puthex(Listen_Posted);
	}
#endif

	if (Listen_Posted && length <= Rx_Buf_Len) {
		Listen_Posted = 0;
		mb.fragment_count = 1;
#ifdef __9432__
		mb.fragment_list[0].fragment_ptr = PTR2PHYSADDR(Rx_Buffer);
		mb.fragment_list[0].fragment_length = length | PHYSICAL_ADDR;
#else
		mb.fragment_list[0].fragment_ptr = (ulong)Rx_Buffer;
		mb.fragment_list[0].fragment_length = length;
#endif
		LM_Receive_Copy(length, 0, (Data_Buff_Structure _far *)&mb,
		    smp, 2);
#ifdef DEBUG_RECV_LONG
	if (DebugFlag) {
		int i;
		putstr("\r\nR>>> ");
		for (i = 0; i < 20; i++) {
			put2hex(*(Rx_Buffer+i));
			putstr(" ");
		}
		putstr("\r\n");
		putstr("[PRESS KEY]");
		kbchar();
	}
#endif
		(void) (*Call_Back)(length);
	}
#ifdef DEBUG_RECV
	else if (DebugFlag)
		putstr(" -- dropping");
	if (DebugFlag)
		putstr("\r\n");
#endif
}

UM_Interrupt(Adapter_Struc * pAd)
{
	return (SUCCESS);
}

UM_Status_Change(Adapter_Struc * pAd)
{
	return (SUCCESS);
}

#ifdef __9432__
unsigned short
UM_Send_Complete(ushort stat, Adapter_Struc *pAd)
#else
int
UM_Send_Complete(Adapter_Struc * pAd)
#endif
{
	return (SUCCESS);
}

cmn_err(int level, char *s)
{
	putstr("LM:");
	putstr(s);
}

#ifdef DEBUG
port_dump(unsigned int port, unsigned int len)
{
	int	i;

	printf("IO port range 0x%x - 0x%x\n", port, port + len);
	for (i = 0; i < len; ) {
		printf("%2x ", inb(port + i));
		if (!(++i % 16))
			putstr("\r\n");
	}
	putstr("\r\nHit a key");
	kbchar();
	putstr("\r\n");
}
#endif /* DEBUG */

#ifdef __9432__
UM_PCI_Services(Adapter_Struc *pAd, union RM_REGS *pregs)
{
	int func = (int)pregs->h.al;

	switch (func) {
		case PCI_BIOS_PRESENT:
			/* return PCI present with rev 2.1 */
			if (is_pci()) {
				pregs->h.ah = 0;
				pregs->h.al = 0;
				pregs->h.bh = 2;
				pregs->h.bl = 1;
				pregs->h.cl = 1;
				pregs->e.edx = 0x20494350;
				pregs->x.cflag = 0;
			} else {
				pregs->x.cflag = 1;
				pregs->h.ah = PCI_FUNC_NOT_SUPPORTED;
			}
			break;
		case FIND_PCI_DEVICE:
			if (pci_find_device(pregs->x.dx, pregs->x.cx,
			    pregs->x.si, (ushort *) &pregs->x.bx)) {
				pregs->h.ah = PCI_SUCCESSFUL;
				pregs->x.cflag = 0;
			} else {
				pregs->h.ah = PCI_DEVICE_NOT_FOUND;
				pregs->x.cflag = 1;
			}
			break;
		case PCI_READ_CONFIG_BYTE:
			if (pci_read_config_byte(pregs->x.bx, pregs->x.di,
			    (unchar *) &pregs->h.cl)) {
				pregs->x.cflag = 0;
				pregs->h.ah = PCI_SUCCESSFUL;
			} else {
				pregs->x.cflag = 1;
				pregs->h.ah = PCI_BAD_REGISTER_NUMBER;
			}
			break;
		case PCI_READ_CONFIG_WORD:
			if (pci_read_config_word(pregs->x.bx, pregs->x.di,
			    (ushort *)&pregs->x.cx)) {
				pregs->x.cflag = 0;
				pregs->h.ah = PCI_SUCCESSFUL;
			} else {
				pregs->x.cflag = 1;
				pregs->h.ah = PCI_BAD_REGISTER_NUMBER;
			}
			break;
		case PCI_READ_CONFIG_DWORD:
			if (pci_read_config_dword(pregs->x.bx, pregs->x.di,
			    (ulong *)&pregs->e.ecx)) {
				pregs->x.cflag = 0;
				pregs->h.ah = PCI_SUCCESSFUL;
			} else {
				pregs->x.cflag = 1;
				pregs->h.ah = PCI_BAD_REGISTER_NUMBER;
			}
			break;
		case PCI_WRITE_CONFIG_BYTE:
			if (pci_write_config_byte(pregs->x.bx, pregs->x.di,
			    pregs->h.cl)) {
				pregs->x.cflag = 0;
				pregs->h.ah = PCI_SUCCESSFUL;
			} else {
				pregs->x.cflag = 1;
				pregs->h.ah = PCI_BAD_REGISTER_NUMBER;
			}
			break;
		case PCI_WRITE_CONFIG_WORD:
			if (pci_write_config_word(pregs->x.bx, pregs->x.di,
			    pregs->x.cx)) {
				pregs->x.cflag = 0;
				pregs->h.ah = PCI_SUCCESSFUL;
			} else {
				pregs->x.cflag = 1;
				pregs->h.ah = PCI_BAD_REGISTER_NUMBER;
			}
			break;
		case PCI_WRITE_CONFIG_DWORD:
			if (pci_write_config_dword(pregs->x.bx, pregs->x.di,
			    pregs->e.ecx)) {
				pregs->x.cflag = 0;
				pregs->h.ah = PCI_SUCCESSFUL;
			} else {
				pregs->x.cflag = 1;
				pregs->h.ah = PCI_BAD_REGISTER_NUMBER;
			}
			break;
		default:
			pregs->x.cflag = 1;	/* set error */
			pregs->h.ah = PCI_FUNC_NOT_SUPPORTED;
			break;
	}
} 

/*
 * UM_Receive_Copy_Complete() -- LM has completed a receive copy
 */
unsigned short
UM_Receive_Copy_Complete(Adapter_Struc *pAd)
{
	/*
	 * This completion mechanism is not used by the UMAC to
	 * determine if the copy has completed, because all LMACs
	 * complete the copy prior to returning.
	 */
	return (SUCCESS);
}
#endif
