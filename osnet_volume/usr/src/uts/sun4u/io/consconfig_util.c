/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consconfig_util.c	1.3	99/10/22 SMI"

/*
 * The console configuration process for the sun4u platforms is divided between
 * the consconfig misc module and the consconfig_dacf dacf module.
 * consconfig_util.c contains utility functions used by both the consconfig misc
 * module and the consconfig_dacf dacf module. Each of these modules has
 * this file statically linked into it.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/klwp.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>

#include <sys/consdev.h>
#include <sys/kbio.h>
#include <sys/debug.h>
#include <sys/reboot.h>
#include <sys/termios.h>
#include <sys/clock.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/modctl.h>
#include <sys/ddi_impldefs.h>

#include <sys/errno.h>
#include <sys/devops.h>
#include <sys/note.h>

#include <sys/polled_io.h>
#include <sys/kmem.h>
#include <sys/consconfig.h>

/*LINTLIBRARY*/

int consconfig_errlevel = DPRINT_L3;

/*
 * External global variables
 */
extern dev_t	rwsconsdev;

/*
 * consconfig_dprintf
 * 	Print string to the console.
 */
    void
consconfig_dprintf(int l, const char *fmt, ...)
{
	va_list ap;

#ifndef DEBUG
	if (!l) {
		return;
	}
#endif
	if ((l) < consconfig_errlevel) {

		return;
	}

	va_start(ap, fmt);
	(void) vprintf(fmt, ap);
	va_end(ap);
}

/*
 * consconfig_util_getdev:
 *      Return the dev_t given the name of the driver and the
 *      minor number
 */
dev_t
consconfig_util_getdev(char *driver_name, int minor)
{
	int	major;
	dev_t	dev;

	major = ddi_name_to_major(driver_name);

	if (major == (major_t)-1) {

		cmn_err(CE_WARN, "consconfig: could not get major for %s",
			driver_name);

		return (NODEV);
	}

	dev = makedevice(major, minor);

	return (dev);
}

/*
 * consconfig_util_createvp:
 *      This routine is a convenience routine that is passed a dev and returns
 *      a vnode.
 */
consconfig_vnode_t *
consconfig_util_createvp(dev_t dev)
{
	consconfig_vnode_t	*avp;

	avp = (consconfig_vnode_t *)kmem_alloc(
			sizeof (consconfig_vnode_t),
			KM_SLEEP);

	/*
	 * makespecvp is kind of a back door routine that returns a vnode
	 * that can be used for VOP_OPEN.  makespecvp calls into the specfs
	 * driver to get the vnode.  Normally filesystem drivers will return
	 * vnodes through VOP_LOOKUP, or VFS_ROOT, but the specfs filesystem
	 * does not have either of these routines.
	 */
	avp->consconfig_vp = makespecvp(dev, VCHR);

	avp->consconfig_dev = dev;

	consconfig_dprintf(DPRINT_L0, "opening vnode = 0x%llx - dev 0x%llx\n",
		avp->consconfig_vp, dev);

	return (avp);
}

/*
 * consconfig_util_freevp:
 *	This routine is a convenience routine to destroy the
 *	consconfig_vnode_t created by consconfig_util_createvp()
 */
void
consconfig_util_freevp(consconfig_vnode_t *avp)
{
	kmem_free(avp, sizeof (consconfig_vnode_t));
}

/*
 * consconfig_util_openvp:
 *
 *      This routine is a convenience routine that is passed a vnode and
 *      opens it.
 */
int
consconfig_util_openvp(consconfig_vnode_t *avp)
{
	int	error;

	/*
	 * open the vnode.  This routine is going to call spec_open() to open
	 * up the vnode that was initialized by makespecvp.
	 */
	error = VOP_OPEN(&avp->consconfig_vp, FREAD+FWRITE, kcred);

	if (error) {
		cmn_err(CE_WARN,
		"consconfig: consconfig_util_openvp failed: err %d vnode 0x%x",
			error, (int)avp->consconfig_vp);
	}

	return (error);
}

/*
 * consconfig_util_getfile:
 *
 *      This routine gets a file pointer that will be associated with the
 *      vnode that is passed in.  This routine has to be called once before
 *      we can call consconfig_util_getfd().
 *      Once we have a valid file_t, we can
 *      call consconfig_util_getfd() and consconfig_util_link() on it.
 *      The file_t exists
 *      outside of the context of the calling thread, so it can be used
 *      in another thread context.
 */
int
consconfig_util_getfile(consconfig_vnode_t *avp)
{
	int	error;

	/*
	 * Allocate a file_t and associate it with avp.
	 */
	error = falloc(avp->consconfig_vp, FREAD+FWRITE+FNOCTTY,
		&avp->consconfig_fp, NULL);

	consconfig_dprintf(DPRINT_L0, "falloc vnode = 0x%llx - fp = 0x%llx\n",
		avp->consconfig_vp, avp->consconfig_fp);

	if (error) {
		cmn_err(CE_WARN, "consconfig: can't get fp : error %d",
			error);

		return (error);
	}

	/*
	 * single threaded - no close will occur here
	 */
	mutex_exit(&avp->consconfig_fp->f_tlock);

	return (0);
}


/*
 * consconfig_util_getfd:
 *      This routine gets a free file descriptor and associates it with
 *      a file pointer that we have already allocated.  The file descriptors
 *      exist in the context of the calling thread.  This routine is called
 *      any time that we are going to I_PLINK a module under another module.
 *      We closeandsetf() in consconfig_util_link(), and that will free the fd
 *      that is allocated here.
 */
int
consconfig_util_getfd(consconfig_vnode_t *avp)
{
	/*
	 * Allocate a file descriptor that is greater than or equal to
	 * 0
	 */
	avp->consconfig_fd = ufalloc(0);

	if (avp->consconfig_fd == -1) {
		cmn_err(CE_WARN, "consconfig: can't get fd");
		return (EMFILE);
	}

	/*
	 * Associate the file pointer with the new file descriptor.  The
	 * strioctl() routine uses a file descriptor for link operations.
	 */
	setf(avp->consconfig_fd, avp->consconfig_fp);

	return (0);
}


/*
 * consconfig_util_closevp:
 *
 *      This routine is a convenience routine that is passed in a vnode.
 *      This routine calls spec_close to do the dirty work.
 */
void
consconfig_util_closevp(consconfig_vnode_t *avp)
{
	VOP_CLOSE(avp->consconfig_vp, FREAD+FWRITE, 1, (offset_t)0, kcred);
}

/*
 * consconfig_util_link:
 *
 *      This routine is a convenience routine that links a streams module
 *      underneath a multiplexor.
 */
int
consconfig_util_link(consconfig_vnode_t *avp, int fd, int *muxid)
{
	int	error;

	/*
	 * Link the device specified by "fd" under the multiplexor specified by
	 * "avp".  strioctl() will call mlink() to do most of the work.  mlink()
	 * looks up the file_t corresponding to "fd" and stores it in the "stp"
	 * that is pointed to by "avp".  mlink() will increment the f_count on
	 * the file_t.
	 */
	if (error = strioctl(avp->consconfig_vp, I_PLINK, (intptr_t)fd,
		FREAD+FWRITE+FNOCTTY, K_TO_K, kcred, muxid)) {

		cmn_err(CE_WARN,
		"consconfig: consconfig_util_link I_PLINK failed: error %d",
			error);

		return (error);
	}

	/*
	 * This call will lookup the file_t corresponding to "fd" and (among
	 * other things) call closef() on the file_t.  closef() will call
	 * VOP_CLOSE() which is a call into spec_close().  Because strioctl
	 * above incremented the f_count, this will not be the last close on
	 * this file_t, and the call to spec_close() will return without
	 * calling close on the device. closef() will not do a VN_RELE() on
	 * the vnode, because it is still open.
	 */
	(void) closeandsetf(fd, NULL);

	return (0);
}

/*
 * consconfig_util_unlink:
 *
 *      This routine is a convenience routine that unlinks a streams module
 *      from underneath a multiplexor.
 */
int
consconfig_util_unlink(consconfig_vnode_t *avp, int mux_id)
{
	int	error, rval;

	/*
	 * Unlink the driver specified by the "mux_id" from underneath the vnode
	 * specified by "vp".  strioctl() will call munlink() to do most of the
	 * dirty work.  Because we did a closeandsetf() above, the f_count on
	 * the file_t that is saved in the "linkp" will be 1.  munlink() calls
	 * closef() on the file_t, causing VOP_CLOSE() an VN_RELE() to be
	 * called on the vnode corresponding to "mux_id".  The closef() will
	 * cause any modules that have been pushed on top of the vnode
	 * corresponding to the "mux_id" to be popped.
	 */
	error = strioctl(avp->consconfig_vp, I_PUNLINK,
				(intptr_t)mux_id, 0,
				K_TO_K, CRED(), &rval);

	if (error) {
		cmn_err(CE_WARN, "consconfig: unlink failed: error %d", error);
	}

	return (error);
}

/*
 * consconfig_util_push:
 *
 *      This is a convenience routine that pushes a "modulename" on
 *      the driver represented by avp.
 */
int
consconfig_util_push(consconfig_vnode_t *avp, char *module_name)
{
	int	error, rval;

	/*
	 * Push the module named module_name on top of the driver corresponding
	 *  to "vp".
	 */
	error = strioctl(avp->consconfig_vp, I_PUSH,
			(intptr_t)module_name,
			FREAD+FNOCTTY, K_TO_K, kcred, &rval);

	if (error) {
		cmn_err(CE_WARN,
			"consconfig: can't push line discipline: error %d",
			    error);
	}

	return (error);
}


/*
 * consconfig_util_abort:
 *      Send the CONSSETABORTENABLE ioctl to the lower layers.  This ioctl
 *      will only be sent to the device if it is the console device.
 *      This ioctl tells the device to pay attention to abort sequences.
 *      In the case of kbtrans, this would tell the driver to pay attention
 *      to the two key abort sequences like STOP-A.  In the case of the
 *      serial keyboard, it would be an abort sequence like a break.
 */
int
consconfig_util_abort(consconfig_vnode_t *avp)
{
	int	flag, rval, error;

	consconfig_dprintf(DPRINT_L0, "consconfig_util_abort\n");

	if ((error = consconfig_util_openvp(avp)) != 0) {

		return (error);
	}

	flag = FREAD+FWRITE+FNOCTTY;

	error = VOP_IOCTL(avp->consconfig_vp, CONSSETABORTENABLE,
		(intptr_t)B_TRUE, (flag|FKIOCTL), kcred, &rval);

	consconfig_util_closevp(avp);

	return (error);
}

/*
 * consconfig_util_kbconfig:
 *      Push "module_name" on top of the driver represented by avp
 *      issue the KIOCTRANSABLE ioctl
 */
void
consconfig_util_kbconfig(cons_state_t *sp, consconfig_vnode_t *avp,
				int kbdtranslatable,
				char *module_name)
{
	int	error, rval;

	/*
	 * Send a flush down the stream to the keyboard.
	 */
	(void) strioctl(avp->consconfig_vp, I_FLUSH, (intptr_t)FLUSHRW,
		FREAD+FNOCTTY, K_TO_K, kcred, &rval);

	/* If there's an error with the push, just return */
	if (consconfig_util_push(avp, module_name) != 0) {
		return;
	}

	error = strioctl(avp->consconfig_vp, KIOCTRANSABLE,
		(intptr_t)&kbdtranslatable, FREAD+FNOCTTY, K_TO_K, kcred,
		&rval);

	if (error) {
		cmn_err(CE_WARN,
		"consconfig_util_kbconfig: KIOCTRANSABLE failed error: %d",
			error);

		return;
	}

	/*
	 * During boot, dynamic_console_config() will call the
	 * function to enable abort on the console.  If the
	 * keyboard is hotplugged after boot, check to see if
	 * the keyboard is the console input.  If it is
	 * enable abort on it.
	 */
	if (sp->cons_input_type == CONSOLE_INPUT_KEYBOARD) {

		(void) consconfig_util_abort(avp);
	}
}

/*
 * consconfig_util_link_wc:
 *      link parameter avp underneath wc
 *      Only called in the serial keyboard case where the user
 *      attached a keyboard to a serial port.
 */
void
consconfig_util_link_wc(cons_state_t *sp, consconfig_vnode_t *avp)
{
	int	rval;

	consconfig_dprintf(DPRINT_L0, "consconfig_util_link_wc: start\n");

	/* wc won't fail the open */
	(void) consconfig_util_openvp(sp->cons_rws_avp);

	/* if there's an error with the file, just return */
	if (consconfig_util_getfile(avp)) {

		return;
	}

	/* if there's an error with the file descriptor, just return */
	if (consconfig_util_getfd(avp)) {

		return;
	}

	/*
	 * Link the stream represented by avp under wc.
	 * consconfig_util_link() calls closeandsetf() to destroy the
	 * avp->consconfig_util_fd that was allocated above.
	 */
	(void) consconfig_util_link(sp->cons_rws_avp,
				    avp->consconfig_fd, &rval);

	/* now we must close it to make console logins happy */
	(void) ddi_hold_installed_driver(getmajor(rwsconsdev));

	consconfig_util_closevp(sp->cons_rws_avp);

	/*
	 * The sp->cons_rws_avp is not destroyed at this point so that we
	 * can re-use it later.  Although we have closed the vnode,
	 * the vnode is still valid.
	 */

	consconfig_dprintf(DPRINT_L0, "consconfig_util_link_wc: end\n");
}

/*
 * consconfig_get_polledio:
 *      Query the console with the CONSPOLLEDIO ioctl.
 *      The polled I/O routines are used by debuggers to perform I/O while
 *      interrupts and normal kernel services are disabled.
 */
static cons_polledio_t *
consconfig_get_polledio(consconfig_vnode_t *avp)
{
	int	error, flag, rval;
	struct strioctl strioc;
	cons_polledio_t *polled_io;

	/*
	 * Setup the ioctl to be sent down to the lower driver.
	 */
	strioc.ic_cmd = CONSOPENPOLLEDIO;
	strioc.ic_timout = INFTIM;
	strioc.ic_len = sizeof (polled_io);
	strioc.ic_dp = (char *)&polled_io;

	flag = FREAD+FWRITE+FNOCTTY;

	/*
	 * Send the ioctl to the driver.  The ioctl will wait for
	 * the response to come back from wc.  wc has already issued
	 * the CONSOPENPOLLEDIO to the lower layer driver.
	 */
	error = VOP_IOCTL(avp->consconfig_vp, I_STR, (intptr_t)&strioc,
		(flag|FKIOCTL), kcred, &rval);

	if (error != 0) {
		/*
		 * If the lower driver does not support polled I/O, then
		 * return NULL.  This will be the case if the driver does
		 * not handle polled I/O, or OBP is going to handle polled
		 * I/O for the device.
		 */

		return (NULL);
	}

	/*
	 * Return the polled I/O structure.
	 */
	return (polled_io);
}

/*
 * consconfig_util_setup_polledio:
 *      This routine does the setup work for polled I/O.  First we get
 *      the polled_io structure from the lower layers
 *      and then we register the polled I/O
 *      callbacks with the debugger that will be using them.
 */
void
consconfig_util_setup_polledio(consconfig_vnode_t *avp)
{
	cons_polledio_t	*polled_io;

	consconfig_dprintf(DPRINT_L0,
		"consconfig_util_setup_polledio: start\n");

	/* if the device won't open, just return */
	if (consconfig_util_openvp(avp) != 0) {

		return;
	}

	/*
	 * Get the polled io routines so that we can use this
	 * device with the debuggers.
	 */
	polled_io = consconfig_get_polledio(avp);

	/*
	 * If polled input isn't supported, then just return.
	 */
	if (polled_io == NULL) {

		consconfig_dprintf(DPRINT_L0,
		"consconfig_util_setup_polledio: get_polledio failed\n");

		consconfig_util_closevp(avp);

		return;
	}

	/*
	 * Initialize the polled input
	 */
	polled_io_init();

	consconfig_dprintf(DPRINT_L0,
	"consconfig_util_setup_polledio: registering callbacks\n");

	/*
	 * Register the callbacks
	 */
	(void) polled_io_register_callbacks(polled_io, 0);

	consconfig_util_closevp(avp);

	consconfig_dprintf(DPRINT_L0, "consconfig_util_setup_polledio: end\n");
}
