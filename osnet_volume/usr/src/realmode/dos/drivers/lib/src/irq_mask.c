/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)irq_mask.c	1.3	97/03/10 SMI"
 
/*
 *  IRQ channel mask/unmask routine for Solaris real mode drivers.
 */

typedef unsigned short ushort;

extern unsigned char inb(ushort);
extern void outb(ushort, unsigned char);
extern ushort splhi(void);
extern void splx(ushort);

#define	MPIC_MASK	0x21	/* Master PIC mask register */
#define	SPIC_MASK	0xa1	/* Slave PIC mask register */

ushort irq_mask(ushort irq, ushort mask_bit)
{
	ushort old_spl = splhi();
	ushort mask_port = (irq < 8) ? MPIC_MASK : SPIC_MASK;
	ushort answer;
	unsigned char mask = inb(mask_port);
	
	/* If given an unknown interrupt channel, just say it was masked */
	if (irq > 15)
		return (1);
		
	answer = ((mask >> (irq & 7)) & 1);
	mask &= ~(1 << (irq & 7));
	mask |= (mask_bit << (irq & 7));
	outb(mask_port, mask);
	splx(old_spl);
	return (answer);
}
