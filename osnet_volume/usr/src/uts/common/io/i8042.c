/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)i8042.c	1.19	99/05/20 SMI"

#include <sys/types.h>
#include <sys/inline.h>
#include <sys/conf.h>
#include <sys/sunddi.h>
#include <sys/i8042.h>
#include <sys/kmem.h>
#include <sys/promif.h>	/* for prom_printf */
#include <sys/note.h>

/*
 * Unfortunately, soft interrupts are implemented poorly.  Each additional
 * soft interrupt user impacts the performance of all existing soft interrupt
 * users.
 */
#undef	USE_SOFT_INTRS

#define	BUFSIZ	64

enum i8042_ports {MAIN_PORT = 0, AUX_PORT, NUM_PORTS};

/*
 * One of these for each port - main (keyboard) and aux (mouse).
 */
struct i8042_port {
	boolean_t		initialized;
	dev_info_t		*dip;
	int			inumber;
	enum i8042_ports	which;		/* main or aux port */
#if	defined(USE_SOFT_INTRS)
	ddi_softintr_t		softid;
	boolean_t		soft_intr_enabled;
#else
	unsigned int		(*intr_func)(caddr_t arg);
	caddr_t			intr_arg;
	kmutex_t		intr_mutex;
#endif
	struct i8042		*i8042_global;
	/*
	 * wptr is next byte to write
	 */
	int			wptr;
	/*
	 * rptr is next byte to read, == wptr means empty
	 * NB:  At full, one byte is unused.
	 */
	int			rptr;
	int			overruns;
	unsigned char		buf[BUFSIZ];
};

/*
 * Describes entire 8042 device.
 */
struct i8042 {
	struct i8042_port	i8042_ports[NUM_PORTS];
	kmutex_t		i8042_mutex;
	kmutex_t		i8042_out_mutex;
	boolean_t		initialized;
	ddi_acc_handle_t	io_handle;
	uint8_t			*io_addr;
	ddi_iblock_cookie_t	iblock_cookie_0;
	ddi_iblock_cookie_t	iblock_cookie_1;
};

/*
 * i8042 hardware register definitions
 */

/*
 * These are I/O registers, relative to the device's base (normally 0x60).
 */
#define	I8042_DATA	0x00	/* read/write data here */
#define	I8042_STAT	0x04	/* read status here */
#define	I8042_CMD	0x04	/* write commands here */

/*
 * These are bits in I8042_STAT.
 */
#define	I8042_STAT_OUTBF	0x01	/* Output (to host) buffer full */
#define	I8042_STAT_INBF		0x02	/* Input (from host) buffer full */
#define	I8042_STAT_AUXBF	0x20	/* Output buffer data is from aux */

/*
 * These are commands to the i8042 itself (as distinct from the devices
 * attached to it).
 */
#define	I8042_CMD_RCB		0x20	/* Read command byte (we don't use) */
#define	I8042_CMD_WCB		0x60	/* Write command byte */
#define	I8042_CMD_WRITE_AUX	0xD4	/* Send next data byte to aux port */

/*
 * function prototypes for bus ops routines:
 */
static int i8042_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *addrp);
static int i8042_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result);

/*
 * function prototypes for dev ops routines:
 */
static int i8042_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int i8042_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static ddi_intrspec_t i8042_get_intrspec(dev_info_t *dip,
	dev_info_t *rdip, uint_t inumber);
static int i8042_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	uint_t (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind);
static void i8042_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie);

/*
 * bus ops and dev ops structures:
 */
static struct bus_ops i8042_bus_ops = {
	BUSO_REV,
	i8042_map,
	i8042_get_intrspec,
	i8042_add_intrspec,
	i8042_remove_intrspec,
	NULL,	/* ddi_map_fault */
	NULL,	/* ddi_dma_map */
	NULL,	/* ddi_dma_allochdl */
	NULL,	/* ddi_dma_freehdl */
	NULL,	/* ddi_dma_bindhdl */
	NULL,	/* ddi_dma_unbindhdl */
	NULL,	/* ddi_dma_flush */
	NULL,	/* ddi_dma_win */
	NULL,	/* ddi_dma_mctl */
	i8042_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static struct dev_ops i8042_ops = {
	DEVO_REV,
	0,
	ddi_no_info,
	nulldev,
	0,
	i8042_attach,
	i8042_detach,
	nodev,
	(struct cb_ops *)0,
	&i8042_bus_ops
};


/*
 * module definitions:
 */
#include <sys/modctl.h>
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, 	/* Type of module.  This one is a driver */
	"i8042 nexus driver",	/* Name of module. */
	&i8042_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	int e;

	/*
	 * Install the module.
	 */
	e = mod_install(&modlinkage);
	return (e);
}

int
_fini(void)
{
	int e;

	/*
	 * Remove the module.
	 */
	e = mod_remove(&modlinkage);
	if (e != 0)
		return (e);

	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

#define	DRIVER_NAME(dip)	ddi_major_to_name(ddi_name_to_major(	\
					ddi_get_name(dip)))

static unsigned int i8042_intr(caddr_t arg);
static void i8042_write_command_byte(struct i8042_port *, unsigned char);
static uint8_t i8042_get8(ddi_acc_impl_t *handlep, uint8_t *addr);
static void i8042_put8(ddi_acc_impl_t *handlep, uint8_t *addr,
	uint8_t value);
static void i8042_send(struct i8042 *global, int reg, unsigned char cmd);

unsigned int i8042_unclaimed_interrupts = 0;

static int
i8042_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct i8042_port	*port;
	enum i8042_ports	which_port;
	int			rc;
	unsigned char		stat;
	static ddi_device_acc_attr_t attr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};
	struct i8042 *global;

	if (cmd != DDI_ATTACH) {
		/*
		 * We don't support anything but DDI_ATTACH.  Eventually
		 * we probably should.
		 */
		return (DDI_FAILURE);
	}

	global = (struct i8042 *)kmem_zalloc(sizeof (*global), KM_SLEEP);
	ddi_set_driver_private(dip, (caddr_t)global);

	/*
	 * We're evil - we never release this.
	 *
	 * Well, we will when we have a detach routine.
	 */
	rc = ddi_regs_map_setup(dip, 0, (caddr_t *)&global->io_addr,
		(offset_t)0, (offset_t)0, &attr, &global->io_handle);
	if (rc != DDI_SUCCESS)
		goto fail_1;

	rc = ddi_get_iblock_cookie(dip, 0, &global->iblock_cookie_0);
	if (rc != DDI_SUCCESS)
		goto fail_2;

	mutex_init(&global->i8042_mutex, NULL, MUTEX_DRIVER,
		global->iblock_cookie_0);

	mutex_init(&global->i8042_out_mutex, NULL, MUTEX_DRIVER, NULL);

	for (which_port = 0; which_port < NUM_PORTS; ++which_port) {
		port = &global->i8042_ports[which_port];
		port->initialized = B_FALSE;
		port->i8042_global = global;
		port->which = which_port;
		mutex_init(&port->intr_mutex, NULL, MUTEX_DRIVER,
		    global->iblock_cookie_0);
	}

	/*
	 * Disable input and interrupts from both the main and aux ports.
	 *
	 * It is difficult if not impossible to read the command byte in
	 * a completely clean way.  Reading the command byte may cause
	 * an interrupt, and there is no way to suppress interrupts without
	 * writing the command byte.  On a PC we might rely on the fact
	 * that IRQ 1 is disabled and guaranteed not shared, but on
	 * other platforms the interrupt line might be shared and so
	 * causing an interrupt could be bad.
	 *
	 * Since we can't read the command byte and update it, we
	 * just set it to static values:
	 *
	 * 0x80:  0 = Reserved, must be zero.
	 * 0x40:  0 = Do not translate to XT codes.
	 *		Doesn't make a difference right here, but we turn
	 *		off translation because we're going to turn it off
	 *		later anyway.
	 * 0x20:  1 = Disable aux (mouse) port.
	 * 0x10:  1 = Disable main (keyboard) port.
	 * 0x08:  0 = Reserved, must be zero.
	 * 0x04:  1 = System flag, 1 means passed self-test.
	 *		Caution:  setting this bit to zero causes some
	 *		systems (HP Kayak XA) to fail to reboot without
	 *		a hard reset.
	 * 0x02:  0 = Disable aux port interrupts.
	 * 0x01:  0 = Disable main port interrupts.
	 */
	i8042_write_command_byte(&global->i8042_ports[0], 0x34);

	global->initialized = B_TRUE;

	rc = ddi_add_intr(dip, 0,
		(ddi_iblock_cookie_t *)NULL, (ddi_idevice_cookie_t *)NULL,
		i8042_intr, (caddr_t)global);
	if (rc != DDI_SUCCESS)
		goto fail_2;

	/*
	 * Some systems (SPARCengine-6) have both interrupts wired to
	 * a single interrupt line.  We should try to detect this case
	 * and not call ddi_add_intr twice.
	 */
	rc = ddi_add_intr(dip, 1,
		&global->iblock_cookie_1, (ddi_idevice_cookie_t *)NULL,
		i8042_intr, (caddr_t)global);
	if (rc != DDI_SUCCESS)
		goto fail_3;

	/* Discard any junk data that may have been left around */
	for (;;) {
		stat = ddi_get8(global->io_handle,
			global->io_addr + I8042_STAT);
		if (! (stat & I8042_STAT_OUTBF))
			break;
		(void) ddi_get8(global->io_handle,
			global->io_addr + I8042_DATA);

	}

	/*
	 * As noted above, we simply set the command byte to the
	 * desired value.  For normal operation, that value is:
	 *
	 * 0x80:  0 = Reserved, must be zero.
	 * 0x40:  0 = Do not translate to XT codes.
	 * 0x20:  0 = Enable aux (mouse) port.
	 * 0x10:  0 = Enable main (keyboard) port.
	 * 0x08:  0 = Reserved, must be zero.
	 * 0x04:  1 = System flag, 1 means passed self-test.
	 *		Caution:  setting this bit to zero causes some
	 *		systems (HP Kayak XA) to fail to reboot without
	 *		a hard reset.
	 * 0x02:  1 = Enable aux port interrupts.
	 * 0x01:  1 = Enable main port interrupts.
	 */
#ifndef __ia64
	i8042_write_command_byte(&global->i8042_ports[0], 0x07);
#else
	/* disable mouse port for now - XXX FIX ME MERCED */
	i8042_write_command_byte(&global->i8042_ports[0], 0x25);
#endif

	return (rc);

fail_3:
	ddi_remove_intr(dip, 0, global->iblock_cookie_0);
fail_2:
	ddi_regs_map_free(&global->io_handle);
fail_1:
	kmem_free(global, sizeof (*global));
	ddi_set_driver_private(dip, NULL);
	return (rc);
}

/*ARGSUSED*/
static int
i8042_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	/*
	 * We do not support detach.  Eventually we probably should, but
	 * (a) detach of nexus drivers is iffy at present, and (b)
	 * realistically, the keyboard never detaches.  This assumption
	 * might come into question when USB keyboards are introduced.
	 */
	cmn_err(CE_WARN, "i8042_detach:  detach not supported");
	return (DDI_FAILURE);
}

/*
 * The primary interface to us from our children is via virtual registers.
 * This is the entry point that allows our children to "map" these
 * virtual registers.
 */
static int
i8042_map(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_map_req_t *mp,
	off_t offset,
	off_t len,
	caddr_t *addrp)
{
	struct i8042_port	*port;
	struct i8042		*global;
	enum i8042_ports	which_port;
	int			*iprop;
	unsigned int		iprop_len;
	int			rnumber;
	ddi_acc_hdl_t		*handle;
	ddi_acc_impl_t		*ap;

	global = (struct i8042 *)ddi_get_driver_private(dip);

	switch (mp->map_type) {
	case DDI_MT_REGSPEC:
		which_port = *(int *)mp->map_obj.rp;
		break;

	case DDI_MT_RNUMBER:
		rnumber = mp->map_obj.rnumber;
		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
		    DDI_PROP_DONTPASS, "reg", &iprop, &iprop_len) !=
		    DDI_SUCCESS) {
#if	defined(DEBUG)
			cmn_err(CE_WARN, "%s #%d:  Missing 'reg' on %s@%s",
			    DRIVER_NAME(dip), ddi_get_instance(dip),
			    ddi_node_name(rdip), ddi_get_name_addr(rdip));
#endif
			return (DDI_FAILURE);
		}
#if	defined(DEBUG)
		if (iprop_len != 1) {
			cmn_err(CE_WARN, "%s #%d:  Malformed 'reg' on %s@%s",
			    DRIVER_NAME(dip), ddi_get_instance(dip),
			    ddi_node_name(rdip), ddi_get_name_addr(rdip));
			return (DDI_FAILURE);
		}
		if (rnumber < 0 || rnumber >= iprop_len) {
			cmn_err(CE_WARN, "%s #%d:  bad map request for %s@%s",
				DRIVER_NAME(dip), ddi_get_instance(dip),
				ddi_node_name(rdip), ddi_get_name_addr(rdip));
			return (DDI_FAILURE);
		}
#endif
		which_port = iprop[rnumber];
		ddi_prop_free((void *)iprop);
#if	defined(DEBUG)
		if (which_port != MAIN_PORT && which_port != AUX_PORT) {
			cmn_err(CE_WARN,
			    "%s #%d:  bad 'reg' value %d on %s@%s",
			    DRIVER_NAME(dip), ddi_get_instance(dip),
			    which_port,
			    ddi_node_name(rdip), ddi_get_name_addr(rdip));
			return (DDI_FAILURE);
		}
#endif
		break;

	default:
#if	defined(DEBUG)
		cmn_err(CE_WARN, "%s #%d:  unknown map type %d for %s@%s",
			DRIVER_NAME(dip), ddi_get_instance(dip),
			mp->map_type,
			ddi_node_name(rdip), ddi_get_name_addr(rdip));
#endif
		return (DDI_FAILURE);
	}

#if	defined(DEBUG)
	if (offset != 0 || len != 0) {
		cmn_err(CE_WARN,
			"%s #%d:  partial mapping attempt for %s@%s ignored",
				DRIVER_NAME(dip), ddi_get_instance(dip),
				ddi_node_name(rdip), ddi_get_name_addr(rdip));
	}
#endif

	port = &global->i8042_ports[which_port];

	switch (mp->map_op) {
	case DDI_MO_MAP_LOCKED:
#if	defined(USE_SOFT_INTRS)
		port->soft_intr_enabled = B_FALSE;
#else
		port->intr_func = NULL;
#endif
		port->wptr = 0;
		port->rptr = 0;
		port->dip = dip;
		port->inumber = 0;
		port->initialized = B_TRUE;

		handle = mp->map_handlep;
		handle->ah_bus_private = port;
		handle->ah_addr = 0;
		ap = (ddi_acc_impl_t *)handle->ah_platform_private;
		/*
		 * Only single get/put 8 is supported on this "bus".
		 */
		ap->ahi_put8 = i8042_put8;
		ap->ahi_get8 = i8042_get8;
		ap->ahi_put16 = NULL;
		ap->ahi_get16 = NULL;
		ap->ahi_put32 = NULL;
		ap->ahi_get32 = NULL;
		ap->ahi_put64 = NULL;
		ap->ahi_get64 = NULL;
		ap->ahi_rep_put8 = NULL;
		ap->ahi_rep_get8 = NULL;
		ap->ahi_rep_put16 = NULL;
		ap->ahi_rep_get16 = NULL;
		ap->ahi_rep_put32 = NULL;
		ap->ahi_rep_get32 = NULL;
		ap->ahi_rep_put64 = NULL;
		ap->ahi_rep_get64 = NULL;
		*addrp = 0;

		return (DDI_SUCCESS);

	default:
		cmn_err(CE_WARN, "%s:  map operation %d not supported",
			DRIVER_NAME(dip), mp->map_op);
		return (DDI_FAILURE);
	}
}

/*
 * i8042 hardware interrupt routine.  Called for both main and aux port
 * interrupts.
 */
static unsigned int
i8042_intr(caddr_t arg)
{
	struct i8042		*global = (struct i8042 *)arg;
	enum i8042_ports	which_port;
	unsigned char		stat;
	unsigned char		byte;
	int			new_wptr;
	struct i8042_port	*port;

	mutex_enter(&global->i8042_mutex);

	stat = ddi_get8(global->io_handle, global->io_addr + I8042_STAT);

	if (! (stat & I8042_STAT_OUTBF)) {
		++i8042_unclaimed_interrupts;
		mutex_exit(&global->i8042_mutex);
		return (DDI_INTR_UNCLAIMED);
	}

	byte = ddi_get8(global->io_handle, global->io_addr + I8042_DATA);

	which_port = (stat & I8042_STAT_AUXBF) ? AUX_PORT : MAIN_PORT;

	port = &global->i8042_ports[which_port];

	if (! port->initialized) {
		mutex_exit(&global->i8042_mutex);
		return (DDI_INTR_CLAIMED);
	}

	new_wptr = (port->wptr + 1) % BUFSIZ;
	if (new_wptr == port->rptr) {
		port->overruns++;
#if	defined(DEBUG)
		if (port->overruns % 50 == 1) {
			cmn_err(CE_WARN, "i8042/%d: %d overruns\n",
				which_port, port->overruns);
		}
#endif
		mutex_exit(&global->i8042_mutex);
		return (DDI_INTR_CLAIMED);
	}

	port->buf[port->wptr] = byte;
	port->wptr = new_wptr;

#if	defined(USE_SOFT_INTRS)
	if (port->soft_intr_enabled)
		ddi_trigger_softintr(port->softid);
#endif

	mutex_exit(&global->i8042_mutex);

#if	!defined(USE_SOFT_INTRS)
	mutex_enter(&port->intr_mutex);
	if (port->intr_func != NULL)
		port->intr_func(port->intr_arg);
	mutex_exit(&port->intr_mutex);
#endif

	return (DDI_INTR_CLAIMED);
}

static void
i8042_write_command_byte(struct i8042_port *port, unsigned char cb)
{
	struct i8042 *global;

	global = port->i8042_global;

	mutex_enter(&global->i8042_mutex);
	i8042_send(global, I8042_CMD, I8042_CMD_WCB);
	i8042_send(global, I8042_DATA, cb);
	mutex_exit(&global->i8042_mutex);
}

/*
 * Send a byte to either the i8042 command or data register, depending on
 * the argument.
 */
static void
i8042_send(struct i8042 *global, int reg, unsigned char val)
{
	uint8_t stat;

	/*
	 * First, wait for the i8042 to be ready to accept data.
	 */
	do {
		stat = ddi_get8(global->io_handle,
			global->io_addr + I8042_STAT);
	} while (stat & I8042_STAT_INBF);

	/*
	 * ... and then send the data.
	 */
	ddi_put8(global->io_handle, global->io_addr+reg, val);
}

/*
 * Here's the interface to the virtual registers on the device.
 *
 * Normal interrupt-driven I/O:
 *
 * I8042_INT_INPUT_AVAIL	(r/o)
 *	Interrupt mode input bytes available?  Zero = No.
 * I8042_INT_INPUT_DATA		(r/o)
 *	Fetch interrupt mode input byte.
 * I8042_INT_OUTPUT_DATA	(w/o)
 *	Interrupt mode output byte.
 *
 * Polled I/O, used by (e.g.) kadb, when normal system services are
 * unavailable:
 *
 * I8042_POLL_INPUT_AVAIL	(r/o)
 *	Polled mode input bytes available?  Zero = No.
 * I8042_POLL_INPUT_DATA	(r/o)
 *	Polled mode input byte.
 * I8042_POLL_OUTPUT_DATA	(w/o)
 *	Polled mode output byte.
 *
 * Note that in polled mode we cannot use cmn_err; only prom_printf is safe.
 */
static uint8_t
i8042_get8(ddi_acc_impl_t *handlep, uint8_t *addr)
{
	struct i8042_port *port;
	struct i8042 *global;
	uint8_t	ret;
	ddi_acc_hdl_t	*h;
	uint8_t stat;

	h = (ddi_acc_hdl_t *)handlep;

	port = (struct i8042_port *)h->ah_bus_private;
	global = port->i8042_global;

	switch ((unsigned)addr) {
	case I8042_INT_INPUT_AVAIL:
		mutex_enter(&global->i8042_mutex);
		ret = port->rptr != port->wptr;
		mutex_exit(&global->i8042_mutex);
		return (ret);

	case I8042_INT_INPUT_DATA:
		mutex_enter(&global->i8042_mutex);

		if (port->rptr != port->wptr) {
			ret = port->buf[port->rptr];
			port->rptr = (port->rptr + 1) % BUFSIZ;
		} else {
#if	defined(DEBUG)
			cmn_err(CE_WARN,
				"i8042:  Tried to read from empty buffer");
#endif
			ret = 0;
		}


		mutex_exit(&global->i8042_mutex);

		break;

#if	defined(DEBUG)
	case I8042_INT_OUTPUT_DATA:
	case I8042_POLL_OUTPUT_DATA:
		cmn_err(CE_WARN, "i8042:  read of write-only register 0x%x",
			(unsigned)addr);
		ret = 0;
		break;
#endif

	case I8042_POLL_INPUT_AVAIL:
		if (port->rptr != port->wptr)
			return (B_TRUE);
		for (;;) {
			stat = ddi_get8(global->io_handle,
				global->io_addr + I8042_STAT);
			if ((stat & I8042_STAT_OUTBF) == 0)
				return (B_FALSE);
			switch (port->which) {
			case MAIN_PORT:
				if ((stat & I8042_STAT_AUXBF) == 0)
					return (B_TRUE);
				break;
			case AUX_PORT:
				if ((stat & I8042_STAT_AUXBF) != 0)
					return (B_TRUE);
				break;
			}
			/*
			 * Data for wrong port pending; discard it.
			 */
			(void) ddi_get8(global->io_handle,
					global->io_addr + I8042_DATA);
		}

		/* NOTREACHED */

	case I8042_POLL_INPUT_DATA:
		if (port->rptr != port->wptr) {
			ret = port->buf[port->rptr];
			port->rptr = (port->rptr + 1) % BUFSIZ;
			return (ret);
		}

		stat = ddi_get8(global->io_handle,
			    global->io_addr + I8042_STAT);
		if ((stat & I8042_STAT_OUTBF) == 0) {
#if	defined(DEBUG)
			prom_printf("I8042_POLL_INPUT_DATA:  no data!\n");
#endif
			return (0);
		}
		ret = ddi_get8(global->io_handle,
			    global->io_addr + I8042_DATA);
		switch (port->which) {
		case MAIN_PORT:
			if ((stat & I8042_STAT_AUXBF) == 0)
				return (ret);
			break;
		case AUX_PORT:
			if ((stat & I8042_STAT_AUXBF) != 0)
				return (ret);
			break;
		}
#if	defined(DEBUG)
		prom_printf("I8042_POLL_INPUT_DATA:  data for wrong port!\n");
#endif
		return (0);

	default:
#if	defined(DEBUG)
		cmn_err(CE_WARN, "i8042:  read of undefined register 0x%x",
			(unsigned)addr);
#endif
		ret = 0;
		break;
	}
	return (ret);
}

static void
i8042_put8(ddi_acc_impl_t *handlep, uint8_t *addr, uint8_t value)
{
	struct i8042_port *port;
	struct i8042 *global;
	ddi_acc_hdl_t	*h;

	h = (ddi_acc_hdl_t *)handlep;

	port = (struct i8042_port *)h->ah_bus_private;
	global = port->i8042_global;

	switch ((unsigned)addr) {
	case I8042_INT_OUTPUT_DATA:
	case I8042_POLL_OUTPUT_DATA:

		if ((unsigned)addr == I8042_INT_OUTPUT_DATA)
			mutex_enter(&global->i8042_out_mutex);

		if (port->which == AUX_PORT)
			i8042_send(global, I8042_CMD, I8042_CMD_WRITE_AUX);

		i8042_send(global, I8042_DATA, value);

		if ((unsigned)addr == I8042_INT_OUTPUT_DATA)
			mutex_exit(&global->i8042_out_mutex);
		break;


#if	defined(DEBUG)
	case I8042_INT_INPUT_AVAIL:
	case I8042_INT_INPUT_DATA:
	case I8042_POLL_INPUT_AVAIL:
	case I8042_POLL_INPUT_DATA:
		cmn_err(CE_WARN, "i8042:  write of read-only register 0x%x",
			(unsigned)addr);
		break;

	default:
		cmn_err(CE_WARN, "i8042:  read of undefined register 0x%x",
			(unsigned)addr);
		break;
#endif
	}
}

/*ARGSUSED*/
static ddi_intrspec_t
i8042_get_intrspec(dev_info_t *dip, dev_info_t *rdip, uint_t inumber)
{
	/*
	 * Each device on this "bus" has exactly one interrupt and they
	 * are all at the same priority, so we don't need an intrspec;
	 * the dip tells us all.  Still, we have to return non-NULL to
	 * indicate success.
	 */
	return ((ddi_intrspec_t)1);
}

/*ARGSUSED*/
static int
i8042_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	uint_t (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind)
{
	struct i8042_port *port;
#if	defined(USE_SOFT_INTRS)
	struct i8042 *global;
	int ret;
#endif

	port = (struct i8042_port *)ddi_get_parent_data(rdip);

#if	defined(USE_SOFT_INTRS)
	global = port->i8042_global;

	ret = ddi_add_softintr(rdip, DDI_SOFTINT_MED, &port->softid,
	    iblock_cookiep, idevice_cookiep,
	    int_handler, int_handler_arg);
	if (ret != DDI_SUCCESS) {
#if	defined(DEBUG)
		cmn_err(CE_WARN,
		    "%s #%d:  Cannot add soft interrupt for %s #%d, ret=%d.",
			DRIVER_NAME(dip), ddi_get_instance(dip),
			DRIVER_NAME(rdip), ddi_get_instance(rdip),
			ret);
#endif
		return (ret);
	}

	mutex_enter(&global->i8042_mutex);

	port->soft_intr_enabled = B_TRUE;
	if (port->wptr != port->rptr)
		ddi_trigger_softintr(port->softid);

	mutex_exit(&global->i8042_mutex);
#else
	if (iblock_cookiep != NULL)
		*iblock_cookiep = NULL;
	mutex_enter(&port->intr_mutex);
	port->intr_func = int_handler;
	port->intr_arg = int_handler_arg;
	if (port->wptr != port->rptr)
		port->intr_func(port->intr_arg);
	mutex_exit(&port->intr_mutex);
#endif

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static void
i8042_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie)
{
	struct i8042_port *port;
#if	defined(USE_SOFT_INTRS)
	struct i8042	*global;
#endif

	port = (struct i8042_port *)ddi_get_parent_data(rdip);

#if	defined(USE_SOFT_INTRS)
	global = port->i8042_global;

	mutex_enter(&global->i8042_mutex);
	port->soft_intr_enabled = B_FALSE;
	ddi_remove_softintr(port->softid);
	port->softid = 0;
	mutex_exit(&global->i8042_mutex);
#else
	mutex_enter(&port->intr_mutex);
	port->intr_func = NULL;
	mutex_exit(&port->intr_mutex);
#endif
}

static int
i8042_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result)
{
	int	*iprop;
	unsigned int	iprop_len;
	int	which_port;
	char	name[16];
	struct i8042	*global;
	dev_info_t	*child;

	global = (struct i8042 *)ddi_get_driver_private(dip);

	switch (op) {
	case DDI_CTLOPS_INITCHILD:
		child = (dev_info_t *)arg;
		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, child,
		    DDI_PROP_DONTPASS, "reg", &iprop, &iprop_len) !=
		    DDI_SUCCESS) {
#if	defined(DEBUG)
			cmn_err(CE_WARN, "%s #%d:  Missing 'reg' on %s@???",
			    DRIVER_NAME(dip), ddi_get_instance(dip),
			    ddi_node_name(child));
#endif
			return (DDI_FAILURE);
		}
		which_port = iprop[0];
		ddi_prop_free((void *)iprop);

		(void) sprintf(name, "%d", which_port);
		ddi_set_name_addr(child, name);
		ddi_set_parent_data(child,
			(caddr_t)&global->i8042_ports[which_port]);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_UNINITCHILD:
		child = (dev_info_t *)arg;
		ddi_set_name_addr(child, NULL);
		ddi_set_parent_data(child, NULL);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REPORTDEV:
		cmn_err(CE_CONT, "?8042 device:  %s@%s, %s # %d\n",
			ddi_node_name(rdip), ddi_get_name_addr(rdip),
			DRIVER_NAME(rdip), ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	default:
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}
	/* NOTREACHED */
}
