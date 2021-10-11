/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Realmode sound-blaster detection driver.
 * Probes for the following types of Sound Blaster:-
 *
 *	Sound Blaster Pro
 * 	Sound Blaster 16
 *	AWE 32 (treated as Sound Blaster 16)
 *
 * This bef is misnamed as it obviously handles more than the
 * sbpro board. However, the name was kept the same to match the kernel
 * driver name.
 *
 * Status/Issues:-
 *	1. sbpro Tested on all dma channels (0, 1, 3), at irs 2(9), 7, and 10
 *	   - didn't both with 5 as this was used by a n/w board.
 *	2. sb 16 w/jumpers and awe-32 also tested ok
 *	3. Order for registers (agreed with David Butterfield) is
 *		A. DSP/Mixer (ie 0x200, 220, 240, 260)
 *		B. FM music (388-38B)
 *		C. MPU ports (SB-16 only)
 */

#ident "@(#)sbpro.c   1.11   97/05/12 SMI"

#include <befext.h>
#include <biosmap.h>
#include <stdio.h>
#include <string.h>

#include "sbpro.h"

/* #define DEBUG 1 */

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;

static int probe_sb(u_short ioaddr);
static int set_sbpro_irq_and_dma(u_short ioaddr);
static void setmixer(u_short ioaddr, int port, int value);
static u_char getmixer(u_short ioaddr, int port);
static u_char dsp_getdata(u_short ioaddr);
static void dsp_command(u_short ioaddr, int cmd);
static int dsp_reset(u_short ioaddr);
static void generate_test_cmd(u_short ioaddr);
static u_short read_irrs(void);
static u_short read_irr(u_short pic);
static void setup_dma(int dma);
static void clear_dma(int dma);
static void check_irqs(void);
static void check_dmas(void);
static void free_dmas(int dma);
static void free_irqs(int irq);
static int res(int type, int set, long value, unsigned int flags);
static void add_mpu(void);
static int mpu_present(u_short ioaddr);

extern void drv_usecwait(u_long usecs);

static int irqs[] = {10, 5, 9, 7}; /* ordered by preference */
static int dmas_8[] = {1, 3, 0}; /* ordered by preference */
static int dmas_16[] = {5, 6, 7}; /* ordered by preference */

#define	NUM_DMAS_8 (sizeof (dmas_8) / sizeof (dmas_8[0]))
#define	NUM_DMAS_16 (sizeof (dmas_16) / sizeof (dmas_16[0]))

#define	IRQ 1
#define	DMA 2
#define	NUM_ITER 3
#define	MAX_RETRIES 5

static char tmpbuf[2];
static int num_dmas = 0;
static int dmas_8_free[3];
static u_char pic1_mask, pic2_mask;

/*
 * Probe for a Sound blaster at the specified port. If sucessful return 1
 * else, return zero.  Also reserves the irq and dma(s) for the board if
 * the probe was successful.
 */
static int
probe_sb(u_short ioaddr)
{
	int board_irq, board_dma;
	int board_dmachan8 = -1, board_dmachan16 = -1;
	int jumpers = 1;
	int i;
	long ver_hi, ver_low;

	if (dsp_reset(ioaddr)) {
		return (NODE_FREE); /* No sb here */
	}

	/* check for the type of the board */
	dsp_command(ioaddr, GET_DSP_VER);
	ver_hi = dsp_getdata(ioaddr);
	ver_low = dsp_getdata(ioaddr);

	if (ver_hi >= 4) { /* SB16 */
		board_dma = getmixer(ioaddr, MIXER_DMA);
		board_irq = getmixer(ioaddr, MIXER_IRQ);
#ifdef DEBUG
		printf("SB16: board_dma 0x%x, board_irq 0x%x\n",
		    board_dma, board_irq);
#endif
		/* Is this a non-jumpered SB16 card? */
		if (!(board_irq & 0x0F)) {
#ifdef DEBUG
			printf("sb: non-jumpered SB16 detected");
#endif
			jumpers = 0;
		}
	}

	add_mpu();

	/* Jumpered SB16 card */
	if (ver_hi >= 4 && jumpers) {
		/* Is the board IRQ valid? */
		switch (board_irq & 0x0F) {
			case 1:
				board_irq = 02;
				break;
			case 2:
				board_irq = 05;
				break;
			case 4:
				board_irq = 07;
				break;
			case 8:
				board_irq = 10;
				break;
			default:
				return (NODE_FREE);
		}
		/*
		 * check 8 bit dma channel. Mask out
		 * both bit 4 and 2, which may be on, and which are unused.
		 */
		switch (board_dma & 0x0B) {
			case DMA_CHAN_0:
				board_dmachan8 = 0;
				break;
			case DMA_CHAN_1:
				board_dmachan8 = 1;
				break;
			case DMA_CHAN_3:
				board_dmachan8 = 3;
				break;
			default:
				return (NODE_FREE);

		}

		/* check 16 bit dma channel */
		switch (board_dma & 0xE0) {
			case DMA_CHAN_5:
				board_dmachan16 = 5;
				break;
			case DMA_CHAN_6:
				board_dmachan16 = 6;
				break;
			case DMA_CHAN_7:
				board_dmachan16 = 7;
				break;
			default:
				return (NODE_FREE);
		}
		/*
		 * Reserve the irq and dma channels.
		 * Return if not available - implying a conflict!
		 */
		if (res(IRQ, 1, board_irq, 0) != RES_OK ||
		    res(DMA, 1, board_dmachan8, 0) != RES_OK ||
		    res(DMA, 1, board_dmachan16, 0) != RES_OK) {
			return (NODE_INCOMPLETE);
		}
#ifdef DEBUG
		printf("board_irq %d, board_dmachan8 %d, board_dmachan16 %d\n",
		    board_irq, board_dmachan8, board_dmachan16);
#endif
	}

	/* Non-jumpered SB16 Card */
	if (ver_hi >= 4 && !jumpers) {

		/*
		 * Search for available IRQ and DMA values for board
		 * to use.  First the irq
		 */
		for (i = 0; i < sizeof (irqs) / sizeof (irqs[0]); i++) {
			if (res(IRQ, 1, irqs[i], RES_SILENT) == RES_OK) {
				break;
			}
		}
		if (i == sizeof (irqs) / sizeof (irqs[0]))
			return (NODE_INCOMPLETE);

		/*
		 * Now the 8 bit dma channel
		 */
		for (i = 0; i < sizeof (dmas_8) / sizeof (dmas_8[0]); i++) {
			if (res(DMA, 1, dmas_8[i], RES_SILENT) == RES_OK) {
				break;
			}
		}
		if (i == sizeof (dmas_8) / sizeof (dmas_8[0]))
			return (NODE_INCOMPLETE);

		/*
		 * Now the 16 bit dma channel
		 */
		for (i = 0; i < sizeof (dmas_16) / sizeof (dmas_16[0]); i++) {
			if (res(DMA, 1, dmas_16[i], RES_SILENT) == RES_OK) {
				break;
			}
		}
		if (i == sizeof (dmas_16) / sizeof (dmas_16[0]))
			return (NODE_INCOMPLETE);
	}

	if (ver_hi < 4) {
		return (set_sbpro_irq_and_dma(ioaddr));
	}
	return (NODE_DONE); /* success */
}


/*
 * For the sb-pro get the irq and dma.
 * This can't be read from the board, so we set up each possible dma
 * channel; generate a test read; and check if an interrupt arrives.
 * When we get an interrupt we use that dma/irq resource pair.
 */
static int
set_sbpro_irq_and_dma(u_short ioaddr)
{
	u_char SaveMaster, SaveSlave;
	int i, j, iter;
	int ret = NODE_INCOMPLETE;
	int irq = -1, dma = -1;
	u_short omap, nmap, map, map_mask;
	int retries;

	/* Save pic masks - disabled == 1 */
	SaveMaster = inp(PIC1_PORT+1);
	SaveSlave = inp(PIC2_PORT+1);

	/*
	 * Check which irqs are available, by reserving the possibilities.
	 * Updates the irq masks: pic1_mask, pic2_mask.
	 */
	check_irqs();
	if (!pic1_mask && !pic2_mask)
		return (NODE_INCOMPLETE);

	/*
	 * Check which dmas are available, by reserving the possibilities.
	 * Leaves the list in dmas_8_free[] with the length of the array
	 * in num_dmas.
	 */
	check_dmas();
	if (!num_dmas)
		return (NODE_INCOMPLETE);

	/* Additionally disable irqs that are possibilities */
	outp(PIC1_PORT+1, SaveMaster | pic1_mask);
	outp(PIC2_PORT+1, SaveSlave | pic2_mask);
	map_mask = (((u_short) pic2_mask << 8) | pic1_mask);

	for (i = 0; i < num_dmas; i++) {
		dma = dmas_8_free[i];
		retries = 0;

retry:
		map = map_mask;
		/* Like the kernel, we let it check 3 times */
		for (iter = 0; iter < NUM_ITER; iter++) {
			setup_dma(dma);
			generate_test_cmd(ioaddr);
			omap = read_irrs() & map_mask;
			clear_dma(dma);
			(void) dsp_reset(ioaddr); /* clear interrupt */
			drv_usecwait(10);
			/*
			 * map of bits that toggled
			 */
			nmap = (read_irrs() & map_mask) ^ omap;
			/*
			 * now keep only bits that toggled before
			 */
			map &= nmap;
#ifdef DEBUG
			printf("maps 0x%x, 0x%x 0x%x, dma %d\n",
			    omap, nmap, map, dma);
#endif
			if (!map) {
				/* Nothing changed */
#ifdef DEBUG
				printf("no toggle iter %d\n", iter);
#endif
				if (++retries < MAX_RETRIES) {
					goto retry;
				}
				break;
			}
		}
		if (map) {
			/*
			 * Success! Determine the irq and release
			 * any dmas and irqs that are unused
			 */
			for (j = 0; j < 16; (j++, map >>= 1)) {
				if (map & 1) {
					irq = j;
				}
			}

#ifdef DEBUG
			printf("sb: irq %d, dma %d, retries %d\n",
			    irq, dma, retries);
#endif
			ret = NODE_DONE; /* success */
			break;
		}
	}
	outp(PIC1_PORT+1, SaveMaster);
	outp(PIC2_PORT+1, SaveSlave);

	/*
	 * If the irq/dma is undetermined, a value of -1 will be passed,
	 * causing all irqs/dmas we reserved to be freed.
	 */
	free_irqs(irq);
	free_dmas(dma);

	return (ret);
}

/*
 * Driver Function Dispatcher:
 *
 * This is the realmode driver equivalent of a "main" routine.  It
 * processes one of the four possible driver functions - the one that
 * does device probing.  The "install" functions are not supported.
 */
int
dispatch(int func)
{
	u_short port;
	long ports[3], portcnt = PortTupleSize;

	if (func != BEF_LEGACYPROBE)
		return (BEF_BADFUNC);	/* Not an operation we support! */

	for (port = SB_MIN_IO; port <= SB_MAX_IO; port += SB_IO_GAP) {
		if (node_op(NODE_START) != NODE_OK) {
			return (0);
		}

		/*
		 * Add the io ports - in numerical order.
		 */

		/*
		 * Note the joystick ports (if enabled) are detected
		 * by the joystick bef
		 */

		/*
		 * Assume no sb here if we can't reserve the ports
		 */
		ports[0] = port;
		ports[1] = SB_IO_LEN;
		ports[2] = 0;
		if (set_res("port", ports, &portcnt, 0) != RES_OK) {
			node_op(NODE_FREE);
			continue;
		}

		/*
		 * Check if we can reserve the common FM_MUSIC ports
		 * This I/O space is always fixed for Sound Blaster
		 * boards, so in order to support several boards,
		 * we reserve this port space as shared.
		 */
		ports[0] = SB_IO_FMMUSIC;
		ports[1] = SB_IO_FMMUSIC_LEN;
		ports[2] = 0;
		if (set_res("port", ports, &portcnt, RES_SHARE) != RES_OK) {
			node_op(NODE_FREE);
			return (0);
		}

		node_op(probe_sb(port));
#ifdef DEBUG
		delay(4000);
#endif
	}
	return (0);
}

/*
 * Set a mixer internal register to a particular value.
 */
static void
setmixer(u_short ioaddr, int port, int value)
{
	outp(ioaddr + MIXER_ADDR, port);
	drv_usecwait(10);
	outp(ioaddr + MIXER_DATA, value);
	drv_usecwait(25); /* from kernel */
}

/*
 * getmixer -- get a mixer internal register
 */
static u_char
getmixer(u_short ioaddr, int port)
{
	u_char retval;

	outp(ioaddr + MIXER_ADDR, port);
	drv_usecwait(10);
	retval = inp(ioaddr + MIXER_DATA);
	drv_usecwait(25); /* from kernel */
	return (retval);
}

static u_char
dsp_getdata(u_short ioaddr)
{
	u_char data;
	int cnt;

	for (cnt = 0; cnt < 10; cnt++) {	/* try a few times */
		if (inp(ioaddr + DSP_DATAAVAIL) & DATA_READY) {
			drv_usecwait(10);		/* delay a while */
			data = inp(ioaddr + DSP_RDDATA);
			return (data);
		}
		drv_usecwait(10);		/* delay a while */
	}

	return (0);
}

/*
 *	Send a command to the DSP, waiting for it to become non-busy first.
 */
static void
dsp_command(u_short ioaddr, int cmd)
{
	int	cnt = 100;

	/*
	 *	Check that the "Write Status" port shows the DSP is ready
	 *	to receive the command (or data) before we send it.
	 */
	while ((inp(ioaddr + DSP_WRSTATUS) & WR_BUSY) && --cnt > 0)
		drv_usecwait(10);

#ifdef DEBUG
	if (cnt <= 0)
		printf("sbpro: dsp_command(%X): Sound Blaster is hung.", cmd);
#endif

	/* now send the command over */
	outp(ioaddr + DSP_WRDATA_CMD, cmd);
	drv_usecwait(25); /* from kernel */
}

/*
 *	Reset the DSP chip.  This will cause the chip to initialize itself,
 *	then become idle with the value 0xAA in the read data port.
 */
static int
dsp_reset(u_short ioaddr)
{
	u_char	data = 0x00;
	int	cnt;

	outp(ioaddr + DSP_RESET, RESET_CMD);	/* set the "reset" bit */
	drv_usecwait(5);			/* need at least 5 usec delay */
	outp(ioaddr + DSP_RESET, 0x00); /* clear the "reset" bit */
	drv_usecwait(10);		/* delay a while */

	/*
	 * now poll for READY in the read data port, checking the status
	 * port for DATA AVAILABLE before doing so.
	 * (Note that the DSP is expected to take 100 usec to initialize)
	 */
	for (cnt = 0; cnt < 100; cnt++) {	/* try a few times */
		if (inp(ioaddr + DSP_DATAAVAIL) & DATA_READY) {
			drv_usecwait(10);		/* delay a while */
			data = inp(ioaddr + DSP_RDDATA);
			if (data == READY) {	/* we got it */
				return (0);
			}
		}
		drv_usecwait(10);		/* delay a while */
	}
	return (-1);
}

static void
generate_test_cmd(u_short ioaddr)
{
	dsp_command(ioaddr, RECORD_MONO);
	dsp_command(ioaddr, SPEAKER_OFF);
	dsp_command(ioaddr, SET_CONSTANT);
	dsp_command(ioaddr, 0x83); /* 0x83 = 8000 Hz */
	dsp_command(ioaddr, ADC_DMA);
	dsp_command(ioaddr, 1); /* low byte */
	dsp_command(ioaddr, 0); /* high byte */

	/*
	 * We have programmed the card to read 2 samples of audio
	 * data at a sampling rate of 8000Hz. We need to wait for this
	 * operation to complete so that the interrupt will have been
	 * generated i.e. 250usec. We will wait 400usec to be sure the
	 * interrupt is generated.
	*/
	drv_usecwait(400);
}

static u_short
read_irrs(void)
{
	return ((read_irr(PIC2_PORT) << 8) | read_irr(PIC1_PORT));
}

static u_short
read_irr(u_short pic)
{
	u_short map;

	/* set up to read the Interrupt request register */
	outp(pic, READ_IRR);
	map = inp(pic);
	/* revert to read Interrupt Service Request register */
	outp(pic, READ_ISR);
	return (map);
}

static int page_regs[4] = {DMA_0PAGE, DMA_1PAGE, DMA_2PAGE, DMA_3PAGE};

static void
setup_dma(int dma)
{
	union {
		char far *bufp;
		u_long phys; /* long */
		u_short s[2]; /* shorts */
		u_char b[4]; /* bytes */
	} u;

	_asm { cli }; /* disable interrupts */

	/* disable specified dma channel */
	outp(DMAC1_MASK, DMA_SETMSK | dma);
	drv_usecwait(10);

	/*
	 * Set dma mode: single; addr increment; no autoinit; read;
	 * on the specified dma channel
	 */
	outp(DMAC1_MODE, DMAMODE_SINGLE | DMAMODE_READ | dma);
	drv_usecwait(10);

	/*
	 * Setup address & page registers
	 */
	outp(DMAC1_CLFF, 0); /* initialise flipflop */
	drv_usecwait(10);
	u.bufp = (char far *) tmpbuf;
	u.phys = (((u_long) u.s[1]) << 4) + u.s[0]; /* convert to physical */
	outp(DMA1_ADDR_BASE_REG + (dma << 1), u.b[0]);
	drv_usecwait(10);
	outp(DMA1_ADDR_BASE_REG + (dma << 1), u.b[1]);
	drv_usecwait(10);
	outp(page_regs[dma], u.b[2]);
	drv_usecwait(10);

	/*
	 * Set up 16 bit count - just 2 bytes, which is 0 based
	 */
	outp(DMAC1_CLFF, 0); /* initialise flipflop */
	drv_usecwait(10);
	outp(DMA1_CNT_BASE_REG + (dma << 1), 1);
	drv_usecwait(10);
	outp(DMA1_CNT_BASE_REG + (dma << 1), 0);
	drv_usecwait(10);

	/* enable specified dma channel */
	outp(DMAC1_MASK, DMA_CLRMSK | dma);
	drv_usecwait(10);

	_asm { sti }; /* enable interrupts */
}

static void
clear_dma(int dma)
{
	_asm { cli }; /* disable interrupts */

	/* disable specified dma channel */
	outp(DMAC1_MASK, DMA_SETMSK | dma);
	drv_usecwait(10);

	_asm { sti }; /* enable interrupts */
}

static void
check_irqs(void)
{
	pic1_mask = 0;
	pic2_mask = 0;

	if (res(IRQ, 1, 5, RES_SILENT) == RES_OK) {
		pic1_mask |= IRQ5_MASK;
	}
	if (res(IRQ, 1, 7, RES_SILENT) == RES_OK) {
		pic1_mask |= IRQ7_MASK;
	}
	if (res(IRQ, 1, 9, RES_SILENT) == RES_OK) {
		pic2_mask |= IRQ9_MASK;
	}
	if (res(IRQ, 1, 10, RES_SILENT) == RES_OK) {
		pic2_mask |= IRQ10_MASK;
	}
	if (!pic1_mask && !pic2_mask)
		/*
		 * No valid IRQs are available; generate a conflict.
		 * Unfortunately, we have not yet determined which IRQ
		 * the board is jumpered to, so the value with which the
		 * conflict is reported probably won't match the jumpered
		 * setting.
		 */
		res(IRQ, 1, 5, 0);
}

static void
check_dmas(void)
{
	int i, j;

	for (i = 0, j = 0; i < NUM_DMAS_8; i++) {
		if (res(DMA, 1, dmas_8[i], RES_SILENT) == RES_OK) {
			dmas_8_free[j++] = dmas_8[i];
			num_dmas++;
		}
	}
	if (!num_dmas)
		/*
		 * No DMAs are free; generate a conflict.  Unfortunately,
		 * we have not yet determined which DMA the board is
		 * jumpered to, so the value with which the conflict is
		 * reported probably won't match the jumpered setting.
		 */
		res(DMA, 1, dmas_8[0], 0);
}

static void
free_dmas(int dma)
{
	int j;

	for (j = 0; j < num_dmas; j++) {
		if (dma != dmas_8_free[j]) {
			(void) res(DMA, 0, dmas_8_free[j], 0);
		}
	}
}

static void
free_irqs(int irq)
{
	if ((irq != 5) && (pic1_mask & IRQ5_MASK)) {
		(void) res(IRQ, 0, 5, 0);
	}
	if ((irq != 7) && (pic1_mask & IRQ7_MASK)) {
		(void) res(IRQ, 0, 7, 0);
	}
	if ((irq != 9) && (pic2_mask & IRQ9_MASK)) {
		(void) res(IRQ, 0, 9, 0);
	}
	if ((irq != 10) && (pic2_mask & IRQ10_MASK)) {
		(void) res(IRQ, 0, 10, 0);
	}
}

static int
res(int type, int set, long value, unsigned int flags)
{
	long val[2], len;

	val[0] = value;
	val[1] = 0;

	if (type == IRQ) {
		len = IrqTupleSize;
		if (set) {
			return (set_res("irq", val, &len, flags));
		} else {
			return (rel_res("irq", val, &len));
		}
	} else {
		len = DmaTupleSize;
		if (set) {
			return (set_res("dma", val, &len, flags));
		} else {
			return (rel_res("dma", val, &len));
		}
	}
}

u_short mpu_io_bases[] = {MPU_PORT_A, MPU_PORT_B}; /* 0x330 comes 1st */

#define	MPU_NUM_IO_BASES (sizeof (mpu_io_bases) / sizeof (mpu_io_bases[0]))

/*
 * the sb-16 has a mpu-401 uart port at either 0x300-0x301 or 0x330-0x331
 * which the default at 0x330. It uses the same irq as the dsp/mixer
 * We simply attempt to reserve both of the valid ranges, so we don't
 * need to know which DSP address is associated with which MPU address
 * Also, we reserve the address range as shared, so if two SB cards
 * are configured to use the same MPU address range, it won't matter,
 * as Solaris does not support the mpu-401.
 */
static void
add_mpu(void)
{
	int i;
	long ports[3], portcnt = PortTupleSize;

	for (i = 0; i < MPU_NUM_IO_BASES; i++) {
		ports[0] = mpu_io_bases[i];
		ports[1] = MPU_PORT_LEN;
		ports[2] = 0;
		if (set_res("port", ports, &portcnt, RES_SHARE) == RES_OK) {
			if (mpu_present(mpu_io_bases[i])) {
				continue;
			} else {
				(void) rel_res("port", ports, &portcnt);
			}
		}
	}
}

static int
mpu_present(u_short ioaddr)
{
	u_int i;

	/*
	 * poll for status port ready
	 */
	for (i = 0; i < MPU_MAX_RETRIES; i++) {
		if (!(inp(ioaddr + MPU_STATUS_PORT) & MPU_STAT_OUTPUT_READY)) {
			break;
		}
	}
	if (i == MPU_MAX_RETRIES) {
		return (0);
	}

	outp(ioaddr + MPU_CMD_PORT, MPU_RESET_CMD);

	for (i = 0; i < MPU_MAX_RETRIES; i++) {
		if (!(inp(ioaddr + MPU_STATUS_PORT) & MPU_STAT_INPUT_DATA) &&
		    (inp(ioaddr + MPU_DATA_PORT) == MPU_RESET_SUCCESS)) {
			return (1); /* success */
		}
	}
	return (0);
}
