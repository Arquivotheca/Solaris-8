/*
 * Copyright (c) 1992-1993, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)power_off.c 1.2     94/04/05 SMI"

#include <sys/cpuvar.h>
#include <sys/syserr.h>
#include <sys/promif.h>

/*
 * Defines necessary for JTAG operations
 */
#define	BCT8244_LENGTH		18
#define	IR_PRELOAD 		0x02
#define	IR_SETBYP 		0x07
#define	BCT8244_IR		8
#define	SCB_8BUF		0xF2
#define	TAP_BUSY		0x100
#define	POWER_OFF		1
#define	MASTER_ENABLE 		0x80
#define	XOFF_SLOWBUS_JTAG	0x30
#define	JTAG_CMD		0x0
#define	JTAG_TIMEOUT		0x10000

/* JTAG commands */
#define	JTAG_SEL_RING		0x6000
#define	JTAG_SEL_DR		0x5050
#define	JTAG_SEL_IR		0x5068
#define	JTAG_SHIFT		0x00A0
#define	JTAG_RUNIDLE		0x50C0
#define	JTAG_IR_TO_DR		0x50E8
#define	JTAG_DR_TO_IR		0x50F4
#define	JTAG_TAP_RESET		0x50FF

#define	TAP_SHIFT(csr, data, nbits)	\
	jtag_cmd_set_ecsr(csr, (data<<16) | ((nbits-1)<<12) | JTAG_SHIFT);

/* local function declarations for JTAG operations */
static int jtag_single_IR_DR(unsigned int,
	int,
	unsigned short,
	unsigned char *,
	int);
static int jtag_single_IR(unsigned int, int, int);
static int select_ring(unsigned int, int);
static int tap_issue_cmd(unsigned int, int);
static int tap_shift_single(unsigned int, int, int);
static int tap_shift_multiple(unsigned int, unsigned char *, int);
static unsigned int find_jtag_master(void);
void power_off(void);

extern u_int bootbusses;

/*
 * power_off.c module:
 *
 * The code in this module powers down a sun4d architecture system.
 * What the software must do to trip the breaker is to force one
 * of the output lines from the BCT8244 chip on the system control
 * board. This chip is the same on all sun4d systems.
 *
 * JTAG is a serial shift protocol. The programming model is not
 * very complicated. You select a JTAG ring, then shift data
 * through that ring to access chips in that ring. The code
 * contained in this module is a very simplified version of the
 * code present in Scantool or sun4d POST. The reason for this is
 * that the ring the BCT8244 lives on is architecturally very
 * simple. Another reason for this is that this code was not required
 * to retreive and compare data from the chip after any of the
 * JTAG operations.
 *
 * In order to use JTAG on the control board, you must use the
 * JTAG port which has acquired the master enable semaphore. This
 * is a system-wide hardware semaphore controlling access to off-
 * board JTAG hardware. Therefore the first thing the software must
 * do is to look at all the CPU's bootbuses, find the one that has
 * the bootbus semaphore and the JTAG master semaphore. Then this
 * CPU's ID is used to calculate the address of the JTAG master port.
 * This port address is used during the rest of the power_off()
 * function. It gets passed to all the low-level routines.
 *
 * Once we knwo the address of the JTAG master port, the following
 * steps will power-off the system:
 *	Select the proper JTAG ring.
 *	Tap reset the ring (puts chip into known state).
 *	Set up chip for loading data.
 *	Load data into chip.
 *	Set chip into bypass mode (forces outputs).
 *
 * The circuit breaker will then be tripped.  Certain older system
 * control boards and power supplies for sun4d systems do not support
 * this functionality. This is why we drop into the PROM if we get to
 * the end of the power_off() fucntion.
 */
void
power_off(void)
{
	unsigned char data[4];
	int i;
	unsigned int jtag_csr;

	if ((jtag_csr = find_jtag_master()) == 0) {
		prom_printf("JTAG Power-off failed\n\n");
		prom_exit_to_mon();
		/*NOTREACHED*/
	}

	/* the 8 lsb of the scan chain force the 8 outputs */
	for (i = 0; i < 4; i++)
		data[i] = 0;

	data[0] = POWER_OFF;

	if (select_ring(jtag_csr, SCB_8BUF) == -1) {
		prom_printf("JTAG Power-off failed\n\n");
		prom_exit_to_mon();
		/*NOTREACHED*/
	}


	if (jtag_single_IR_DR(jtag_csr,
			BCT8244_IR,
			IR_PRELOAD,
			data,
			BCT8244_LENGTH) == -1) {
		prom_printf("JTAG Power-off failed\n\n");
		prom_exit_to_mon();
		/*NOTREACHED*/
	}

	if (jtag_single_IR(jtag_csr, BCT8244_IR, IR_SETBYP) == -1) {
		prom_printf("JTAG Power-off failed\n\n");
		prom_exit_to_mon();
		/*NOTREACHED*/
	}

	/*
	 * Systems which have old rev sun4d control boards will not
	 * power-off. There will be no failures either. So we must
	 * drop into the PROM here in case we encounter one of these
	 * control boards.
	 */
	prom_exit_to_mon();
	/*NOTREACHED*/
}


/*
 * Yes. Sun4d supports software power off.
 */
int
power_off_is_supported(void)
{
	return (1);
}


/*
 * Shift the specified instruction into the component, then
 * shift the required data in.
 */

static int
jtag_single_IR_DR(unsigned int csr,
	int ir_len,
	unsigned short instr,
	unsigned char *in,
	int nbits)
{
	if (tap_issue_cmd(csr, JTAG_SEL_IR) == -1) {
		return (-1);
	}

	if (tap_shift_single(csr, instr, ir_len) == -1) {
		return (-1);
	}

	if (tap_issue_cmd(csr, JTAG_IR_TO_DR) == -1) {
		return (-1);
	}

	if (tap_shift_multiple(csr, in, nbits) == -1) {
		return (-1);
	}

	if (tap_issue_cmd(csr, JTAG_RUNIDLE) == -1) {
		return (-1);
	}

	return (0);
}

static int
jtag_single_IR(unsigned int csr,
	int ir_len,
	int instr)
{
	if (tap_issue_cmd(csr, JTAG_SEL_IR) == -1) {
		return (-1);
	}

	if (tap_shift_single(csr, instr, ir_len) == -1) {
		return (-1);
	}

	if (tap_issue_cmd(csr, JTAG_RUNIDLE) == -1) {
		return (-1);
	}

	return (0);
}

static int
select_ring(unsigned int csr, int ring)
{
	if (tap_issue_cmd(csr, (ring << 16) | JTAG_SEL_RING) == -1) {
		return (-1);
	}

	if (tap_issue_cmd(csr, JTAG_TAP_RESET) == -1) {
		return (-1);
	}

	return (0);
}

static int
tap_issue_cmd(unsigned int csr, int command)
{
	int wait = JTAG_TIMEOUT;

	while ((jtag_cmd_get_ecsr(csr) & TAP_BUSY) != 0) {
		if (wait-- < 0)
			return (-1);
	}

	jtag_cmd_set_ecsr(csr, command);

	return (0);
}

static int
tap_shift_single(unsigned int csr, int instr, int nbits)
{
	int wait = JTAG_TIMEOUT;

	while ((jtag_cmd_get_ecsr(csr) & TAP_BUSY) != 0) {
		if (wait-- < 0)
			return (-1);
	}

	TAP_SHIFT(csr, instr, nbits);

	return (0);
}

static int
tap_shift_multiple(unsigned int csr, unsigned char *data_in, int nbits)
{
	for (; nbits > 0; nbits = nbits - 8) {
		int bits_this_pass = nbits > 8 ? 8 : nbits;
		int wait = JTAG_TIMEOUT;

		while ((jtag_cmd_get_ecsr(csr) & TAP_BUSY) != 0) {
			if (wait-- < 0)
				return (-1);
		}
		TAP_SHIFT(csr, *data_in++, bits_this_pass);
	}

	return (0);
}

static unsigned int
find_jtag_master(void)
{
	int i;
	unsigned int jtag_cmd;

	for (i = 0; i < NCPU; i++) {

		if (cpu[i] == NULL)
			continue;

		/* check if this CPU has the Bootbus semaphore */
		if (!CPU_IN_SET(bootbusses, cpu[i]->cpu_id))
			continue;

		/* now check if it is the JTAG master */
		if (jtag_ctl_get_ecsr(cpu[i]->cpu_id) & MASTER_ENABLE) {
			jtag_cmd = (cpu[i]->cpu_id << 27) +
				(XOFF_SLOWBUS_JTAG << 16) + JTAG_CMD;

			/* return ECSR address of JTAG master command */
			return (jtag_cmd);
		}

	}
	return (0);
}
