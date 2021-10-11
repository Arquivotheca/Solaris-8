/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consmsconf.c	1.43	99/06/05 SMI" 	/* SVr4 */

/*
 * Console and mouse configuration
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/vm.h>
#include <sys/file.h>
#include <sys/klwp.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/ttold.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/mman.h>
#undef NFS
#include <sys/mount.h>
#include <sys/bootconf.h>
#include <sys/fs/snode.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>

#include <sys/kmem.h>
#include <sys/cpu.h>
#include <sys/consdev.h>
#include <sys/kbio.h>
#include <sys/debug.h>
#include <sys/reboot.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>		/* XXX: DDI_CF2 */
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/promif.h>

#define	NOUNIT		-1

/* #define	PATH_DEBUG	1 */
#ifdef	PATH_DEBUG
static int path_debug = PATH_DEBUG;
#endif	PATH_DEBUG

#define	KBDMINOR	0
#define	MOUSEMINOR	1

extern struct fileops vnodefops;

static int mouseconfig(dev_t);
static void get_consinfo(int *);

/*
 * This is the loadable module wrapper.
 */

extern struct mod_ops mod_miscops;

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "console configuration"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * Find the zs device's major number via lookup.
 * Assumes that the driver is pre-loaded.
 * caches the result, so the lookup is only done once.
 */

static major_t
zsmajor(void)
{
	static major_t zsm;

	return (zsm ? zsm : (zsm = ddi_name_to_major("zs")));
}

/*
 * Configure keyboard and mouse.
 */

static const char discmsg[] = "%s: can't push %s line discipline: error %d";
static char devname[512];

#ifndef	MPSAS

void
consconfig(void)
{
	static char *emsg = "consconfig";
	static char *nokset = "%s: can't set up keyboard";
	static char *nowset = "%s: can't set up workstation console";
	int error;
	int zeropgrp = 0;
	int kmunit;		/* keyboard/mouse unit */
	char *stdiopath;
	struct termios termios;
	int kbdtranslatable = TR_CANNOT;
	int kbdspeed = B9600;
	struct vnode *kbdvp, *conskbdvp;
	struct vnode *stdinvp = NULL;
	struct file *fp;
	int rval, fd;
	dev_t wsconsdev;
	klwp_t *lwp = ttolwp(curthread);
	int major;
	extern dev_t rwsconsdev;
	extern int cn_conf;
	extern void mon_clock_stop(void);

	(void) ddi_install_driver("zs");

	/*
	 * Examine system configuration information to determine which device
	 * is to act as the console.  Set that device up and record its
	 * identity in rconsdev and rconsvp for the benefit of the (indirect)
	 * console driver.
	 */

	kmunit = NOUNIT;

	/*
	 * check for console on same ascii port to allow full speed
	 * output by using the UNIX driver and avoiding the monitor.
	 *
	 * XXX - How do we know that the name of the keyboard/mouse hardware
	 *	 driver is "zs"?
	 */

#ifdef	PATH_DEBUG
	if (path_debug != 0)  {
		prom_printf("consconfig:\n");
		prom_printf("stdin is <%s>, stdout is <%s>\n",
		    prom_stdinpath(), prom_stdoutpath());
		prom_printf("stripped stdout is ...");
		prom_strip_options(prom_stdoutpath(), devname);
		prom_printf("<%s>\n", devname);
		prom_printf("stdin-stdout-equivalence? <%s>\n",
		    prom_stdin_stdout_equivalence() ? "yes" : "no");
		(void) strcpy(devname, "unknown!");
		prom_stdin_devname(devname);
		prom_printf("stdin-devname <%s> zsmajor is %d\n",
		    devname, zsmajor());
	}
#endif	PATH_DEBUG

	if (prom_stdin_stdout_equivalence() == 0)
		goto not_on_same_zs;

	if ((stdiopath = prom_stdinpath()) == 0)
		goto not_on_same_zs;

	if ((rconsdev = ddi_pathname_to_dev_t(stdiopath)) == (dev_t)-1) {
		rconsdev = 0;
		goto not_on_same_zs;
	}

	if (getmajor(rconsdev) != zsmajor()) {
		rconsdev = 0;
		goto not_on_same_zs;
	}

	if ((rconsdev = ddi_pathname_to_dev_t(stdiopath)) == (dev_t)-1)
		rconsdev = 0;

#ifdef	PATH_DEBUG
	if (path_debug && rconsdev)
		prom_printf("rconsdev (%x)  is %d,%d\n", rconsdev,
			getemajor(rconsdev), geteminor(rconsdev));
#endif	PATH_DEBUG

not_on_same_zs:

	cn_conf = 1;		/* Don't really use rconsvp yet... */

	/*
	 * NON-DDI COMPLIANT CALL
	 */
	mon_clock_stop();	/* turn off monitor polling clock */

	if (rconsdev) {
		/*
		 * Console is a CPU serial port.
		 */
		rconsvp = makespecvp(rconsdev, VCHR);

		/*
		 * Opening causes interrupts, etc. to be initialized.
		 * Console device drivers must be able to do output
		 * after being closed!
		 */

		if (error = VOP_OPEN(&rconsvp, FREAD+FWRITE+FNOCTTY, kcred)) {
			cmn_err(CE_WARN,
			    "%s: console open failed: error %d", emsg, error);
		}

		/*
		 * "Undo undesired ttyopen side effects" (not needed anymore
		 * in 5.0 -verified by commenting this out and running anyway.
		 * This zeroed u.u_ttyp and u.u_ttyd and u.u_procp->p_pgrp).
		 */

		(void) strioctl(rconsvp, TIOCSPGRP, (intptr_t)&zeropgrp,
				FREAD + FNOCTTY, K_TO_K, kcred, &rval);

		/* now we must close it to make console logins happy */
		(void) ddi_hold_installed_driver(getmajor(rconsdev));
		VOP_CLOSE(rconsvp, FREAD+FWRITE, 1, (offset_t)0, kcred);
	} else if (prom_stdout_is_framebuffer()) {
		/*
		 * Console is a framebuffer plus keyboard and mouse.
		 * Find the framebuffer driver if we can, and make
		 * ourselves a shadow vnode to track it with.
		 */
		prom_strip_options(prom_stdoutpath(), devname);
		if ((fbdev = ddi_pathname_to_dev_t(devname)) == NODEV) {
			/*
			 * Look at 1097995 if you want to know why this
			 * might be a problem ..
			 */
			cmn_err(CE_NOTE,
			    "Can't find driver for console framebuffer");
		} else
			fbvp = makespecvp(fbdev, VCHR);
	}

	/*
	 * Look for the [last] kbd/ms and matching fbtype.
	 *
	 * In the sun4c (OpenBoot Prom) version of this routine
	 * there are three things that were looked for here:
	 *
	 *	1. We wanted to find the dev_info_t for a device
	 *	that has the property "keyboard" attached to it.
	 *	This was then assumed to be a zs with a keyboard
	 *	on port-A and a mouse on port-B of that zs device.
	 *
	 *	2. We wanted to find the root node's "fb" property
	 *	value, and from the value of that find the node
	 *	for the system's frame buffer.
	 *
	 * In the 5.0/SVr4 framework, this specific method
	 * is a little hard to manage (we must allow, for instance,
	 * for non-OBP machines where you may not necessarily
	 * know the 'node id' for the console frame buffer).
	 *
	 * The specific methodology for #2 is only applicable for
	 * full OBP systems (where the hardware will truly identify
	 * itself enough to have a node exist for the console frame
	 * buffer). As I said above, this won't work to well for
	 * non-OBP systems. What we'll do instead is force each
	 * implementation to initialize a string which will be the
	 * XXidentify() matcher for the driver for the console frame
	 * buffer (if any such device exists), and also the unit
	 * number (instantiation) for the device.  #2 is now handled
	 * completely in promif interfaces.
	 *
	 * #1 can be found be repeated lookups of "zs" until the
	 * associated property of "keyboard" is found. This could
	 * be done more generally. XXX Later, babe, later XXX
	 */

	get_consinfo(&kmunit);

	/*
	 * Use serial keyboard and mouse if found flagged uart
	 * XXX - How do we know how the "zs" driver allocated minor numbers?
	 */

	if (kmunit != NOUNIT) {
		mousedev = makedevice(zsmajor(), kmunit * 2 + MOUSEMINOR);
		kbddev = makedevice(zsmajor(), kmunit * 2 + KBDMINOR);
		kbdtranslatable = TR_CAN;
#ifdef	PATH_DEBUG
		if (path_debug)  {
			prom_printf("mousedev (%x) is %d,%d\n",
				mousedev, zsmajor(), kmunit * 2 + MOUSEMINOR);
			prom_printf("kbddev (%x) is %d,%d\n",
				kbddev, zsmajor(), kmunit * 2 + KBDMINOR);
		}
#endif	PATH_DEBUG
	}

#ifdef	PATH_DEBUG
	else if (path_debug)
		prom_printf("consconfig: no kmunit found!\n");
#endif	PATH_DEBUG

	/*
	 * If rconsvp is still NULL, the only remaining possibility for the
	 * console is the combination of the redirection driver with the
	 * workstation console device.  The code below attempts to set the
	 * workstation console device up unconditionally (so that it's usable
	 * when the console is a serial port), but panics if the attempt fails
	 * with no console already established.
	 *
	 * XXX:	Need to modify this code to handle multiple workstation
	 *	console instances.
	 */

	if (kbddev != NODEV)
		kbdspeed = zsgetspeed(kbddev);		/* XXX: Assuming "zs" */
	else {
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, "%s: no keyboard found", emsg);
			/* NOTREACHED */
		}
		return;
	}

	/*
	 * Try to configure mouse.
	 */

	if (mousedev != NODEV && mouseconfig(mousedev)) {
		cmn_err(CE_NOTE, "%s: No mouse found", emsg);
		mousedev = NODEV;
		lwp->lwp_error = 0;
	}

	/*
	 * Open the keyboard device.
	 */

	kbdvp = makespecvp(kbddev, VCHR);
	if (error = VOP_OPEN(&kbdvp, FREAD + FNOCTTY, kcred)) {
		cmn_err(CE_WARN, "%s: keyboard open failed: error %d",
		    emsg, error);
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, "can't open keyboard");
			/* NOTREACHED */
		}
	}
	(void) strioctl(kbdvp, I_FLUSH, FLUSHRW, FREAD + FNOCTTY, K_TO_K,
			kcred, &rval);
	/*
	 * It would be nice if we could configure the device to autopush this
	 * module, but unfortunately this code executes before the necessary
	 * user-level administrative code has run.
	 */
	if (error = strioctl(kbdvp, I_PUSH, (intptr_t)"kb",
			FREAD + FNOCTTY, K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, (char *)discmsg, emsg, "keyboard", error);
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, nokset, emsg);
			/* NOTREACHED */
		}
	}

	/*
	 * "Undo undesired ttyopen side effects" (not needed anymore
	 * in 5.0 -verified by commenting this out and running anyway.
	 * This zeroed u.u_ttyp and u.u_ttyd and u.u_procp->p_pgrp).
	 */

	(void) strioctl(kbdvp, TIOCSPGRP, (intptr_t)&zeropgrp, FREAD + FNOCTTY,
			K_TO_K, kcred, &rval);

	if (error = strioctl(kbdvp, KIOCTRANSABLE, (intptr_t)&kbdtranslatable,
			FREAD + FNOCTTY, K_TO_K, kcred, &rval))
		cmn_err(CE_WARN, "%s: KIOCTRANSABLE failed error: %d",
		    emsg, error);

	if (kbdtranslatable == TR_CANNOT) {
		/*
		 * For benefit of serial port keyboard.
		 * XXX: Assuming "zs"
		 */
		(void) strioctl(kbdvp, TCGETS, (intptr_t)&termios,
			FREAD + FNOCTTY, K_TO_K, kcred, &rval);

		termios.c_cflag &= ~(CBAUD|CIBAUD);
		termios.c_cflag |= kbdspeed;

		if (error = strioctl(kbdvp, TCSETSF, (intptr_t)&termios,
				FREAD + FNOCTTY, K_TO_K, kcred, &rval)) {
			cmn_err(CE_WARN, "%s: TCSETSF error %d", emsg,
			    error);
			lwp->lwp_error = 0;
		}
	}

	/*
	 * Open the "console keyboard" device, and link the keyboard
	 * device under it.
	 */

	major = ddi_name_to_major("conskbd");
	conskbdvp = makespecvp(makedevice(major, 1), VCHR);
	if (error = VOP_OPEN(&conskbdvp, FREAD+FWRITE+FNOCTTY, kcred)) {
		cmn_err(CE_WARN,
		    "%s: console keyboard device open failed: error %d", emsg,
		    error);
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, "%s: can't open keyboard", emsg);
			/* NOTREACHED */
		}
	}
	if (error = falloc(kbdvp, FREAD, &fp, &fd)) {
		cmn_err(CE_WARN,
		    "%s: can't get file descriptor for keyboard: error %d",
		    emsg, error);
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, nokset, emsg);
			/* NOTREACHED */
		}
	}
	setf(fd, fp);
	/* single theaded - no  close will occur here */
	mutex_exit(&fp->f_tlock);
	if (error = strioctl(conskbdvp, I_PLINK, (intptr_t)fd,
			FREAD + FWRITE + FNOCTTY, K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, "%s: I_PLINK failed: error %d", emsg, error);
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, nokset, emsg);
			/* NOTREACHED */
		}
	}
	(void) closeandsetf(fd, NULL);

	/*
	 * Configure and open the stdin device. (If a different device)
	 *
	 * Since kbddev refers to the physical keyboard, if the prom
	 * is using some other device as the standard input device,
	 * we'd like to respect the PROM and use the same device when
	 * we construct the workstation console stream...
	 */

	if (prom_stdin_is_keyboard())  {
		stdindev = kbddev;		/* Using physical keyboard */
		stdinvp = conskbdvp;
	} else if (rconsdev) {
		stdindev = rconsdev;		/* Console is tty[abcd] */
		stdinvp = conskbdvp;		/* input/output are same dev */
	} else {
		int translatable = TR_CANNOT;
		int stdinspeed;
		int is_a_zs;

		if ((stdiopath = prom_stdinpath()) == NULL)
			goto stdin_conf_done;

		if ((stdindev = ddi_pathname_to_dev_t(stdiopath)) == NODEV)  {
			cmn_err(CE_WARN,
			    "%s: Can't decode standard-input pathname <%s>",
			    emsg, stdiopath);
			goto stdin_conf_done;
		}

		is_a_zs = (getmajor(stdindev) == zsmajor());
		if (is_a_zs)
			stdinspeed = zsgetspeed(stdindev);

		stdinvp = makespecvp(stdindev, VCHR);
		if (error = VOP_OPEN(&stdinvp, FREAD+FNOCTTY, kcred)) {
			cmn_err(CE_WARN,
			    "%s: Error %d opening standard-input device",
			    emsg, error);
			stdindev = NODEV;	/* Flag error */
			stdinvp = NULL;		/* for later */
			goto stdin_conf_done;
		}


		(void) strioctl(stdinvp, I_FLUSH, FLUSHRW,
			FREAD + FNOCTTY, K_TO_K, kcred, &rval);

		if (error = strioctl(stdinvp, I_PUSH, (intptr_t)"kb",
				FREAD + FNOCTTY, K_TO_K, kcred, &rval))  {
			cmn_err(CE_WARN, (char *)discmsg, emsg,
			    "keyboard", error);
		}

		(void) strioctl(stdinvp, TIOCSPGRP, (intptr_t)&zeropgrp,
			FREAD + FNOCTTY, K_TO_K, kcred, &rval);

		if (error = strioctl(stdinvp, KIOCTRANSABLE,
				(intptr_t)&translatable, FREAD + FNOCTTY,
				K_TO_K, kcred, &rval))
			cmn_err(CE_WARN, "%s: KIOCTRANSABLE failed error: %d",
			    emsg, error);

		/*
		 * Get and set the speed of the serial port keyboard.
		 * XXX: We only know how to do this if it's a "zs" device.
		 */
		if (is_a_zs)  {

			(void) strioctl(stdinvp, TCGETS, (intptr_t)&termios,
					FREAD + FNOCTTY, K_TO_K, kcred, &rval);

			termios.c_cflag &= ~(CBAUD|CIBAUD);
			termios.c_cflag |= stdinspeed;

			if (error = strioctl(stdinvp, TCSETSF,
					(intptr_t)&termios, FREAD + FNOCTTY,
					K_TO_K, kcred, &rval)) {

				cmn_err(CE_WARN, "%s: TCSETSF error %d",
				    emsg, error);
				lwp->lwp_error = 0;
			}
		}
	}

stdin_conf_done:

	if (stdindev == NODEV)  {
		/*
		 * After all this, if we can't deal with the actual stdin
		 * device, just use the keyboard and complain about it.
		 */
		cmn_err(CE_WARN, "%s: Unable to configure input device", emsg);
		cmn_err(CE_WARN, "%s: Using keyboard as input device", emsg);
		stdindev = kbddev;
		stdinvp = conskbdvp;
	}

	/*
	 * Open the "workstation console" device, and link the
	 * standard input device under it. (Which may or may not be the
	 * physical console keyboard.)
	 */
	major = ddi_name_to_major("wc");
	rwsconsdev = makedevice(major, 0);
	rwsconsvp = makespecvp(rwsconsdev, VCHR);
	if (error = VOP_OPEN(&rwsconsvp, FREAD+FWRITE, kcred)) {
		cmn_err(CE_WARN,
		    "%s: workstation console open failed: error %d",
		    emsg, error);
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, "%s: can't open console", emsg);
			/* NOTREACHED */
		}
	}

	if (error = falloc(stdinvp, FREAD+FWRITE+FNOCTTY, &fp, &fd)) {
		cmn_err(CE_WARN,
		    "%s: can't get fd for console keyboard: error %d",
		    emsg, error);
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, nowset, emsg);
			/* NOTREACHED */
		}
	}
	setf(fd, fp);
	/* single theaded - no  close will occur here */
	mutex_exit(&fp->f_tlock);
	if (error = strioctl(rwsconsvp, I_PLINK, (intptr_t)fd,
			FREAD + FWRITE + FNOCTTY, K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, "%s: I_PLINK failed: error %d", emsg, error);
		if (rconsvp == NULL) {
			cmn_err(CE_PANIC, nowset, emsg);
			/* NOTREACHED */
		}
	}
	(void) closeandsetf(fd, NULL);

	/* now we must close it to make console logins happy */
	(void) ddi_hold_installed_driver(getmajor(rwsconsdev));
	VOP_CLOSE(rwsconsvp, FREAD+FWRITE, 1, (offset_t)0, kcred);

	/*
	 * Get a vnode for the redirection device.  (It has the
	 * connection to the workstation console device wired into it,
	 * so that it's not necessary to establish the connection
	 * here.  If the redirection device is ever generalized to
	 * handle multiple client devices, it won't be able to
	 * establish the connection itself, and we'll have to do it
	 * here.)
	 */
	major = ddi_name_to_major("iwscn");
	wsconsdev = makedevice(major, 0);
	wsconsvp = makespecvp(wsconsdev, VCHR);

	/*
	 * Use the redirection device/workstation console pair as the "real"
	 * console if the latter hasn't already been set.
	 */
	if (rconsvp == NULL) {
		/*
		 * The workstation console driver needs to see rwsconsvp, but
		 * all other access should be through the redirecting driver.
		 */
		(void) ddi_hold_installed_driver(major);
		rconsdev = wsconsdev;
		rconsvp = wsconsvp;
	}

	cn_conf = 0;		/* OK to use rconsvp, now. */

}

static int
mouseconfig(dev_t msdev)
{
	static char *emsg = "mouseconfig";
	int error;
	struct vnode *mousevp;
	struct vnode *consmousevp;
	struct file *fp;
	int fd, rval;
	int major;

	/* Open the mouse device. */
	mousevp = makespecvp(msdev, VCHR);
	if (error = VOP_OPEN(&mousevp, FREAD + FNOCTTY, kcred))
		return (error);
	(void) strioctl(mousevp, I_FLUSH, FLUSHRW, FREAD + FNOCTTY,
			K_TO_K, kcred, &rval);
	if (error = strioctl(mousevp, I_PUSH, (intptr_t)"ms", FREAD + FNOCTTY,
			K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, (char *)discmsg, emsg, "mouse", error);
		(void) VOP_CLOSE(mousevp, FREAD+FNOCTTY, 1, (offset_t)0,
		    kcred);
		return (error);
	}

	/*
	 * Open the "console mouse" device, and link the mouse device
	 * under it.
	 */
	major = ddi_name_to_major("consms");
	consmousevp = makespecvp(makedevice(major, 0), VCHR);
	if (error = VOP_OPEN(&consmousevp, FREAD+FWRITE+FNOCTTY, kcred)) {
		(void) VOP_CLOSE(mousevp, FREAD+FNOCTTY, 1, (offset_t)0, kcred);
		return (error);
	}
	if (error = falloc(mousevp, FREAD+FNOCTTY, &fp, &fd)) {
		cmn_err(CE_WARN,
		    "%s: can't get file descriptor for mouse: error %d",
		    emsg, error);
		(void) VOP_CLOSE(consmousevp, FREAD+FWRITE+FNOCTTY, 1,
		    (offset_t)0, kcred);
		(void) VOP_CLOSE(mousevp, FREAD+FNOCTTY, 1, (offset_t)0,
		    kcred);
		return (error);
	}
	setf(fd, fp);
	/* single theaded - no  close will occur here */
	mutex_exit(&fp->f_tlock);
	if (error = strioctl(consmousevp, I_PLINK, (intptr_t)fd,
			FREAD + FWRITE + FNOCTTY, K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, "%s: I_PLINK failed: error %d",
		    emsg, error);
		(void) VOP_CLOSE(consmousevp, FREAD+FWRITE+FNOCTTY, 1,
		    (offset_t)0, kcred);
		(void) VOP_CLOSE(mousevp, FREAD+FNOCTTY, 1, (offset_t)0,
		    kcred);
		return (error);
	}
	(void) closeandsetf(fd, NULL);
	(void) ddi_hold_installed_driver(major);
	(void) VOP_CLOSE(consmousevp, FREAD+FWRITE+FNOCTTY, 1,
	    (offset_t)0, kcred);

	return (error);
}

/*
 * Fill in console related information
 */
static void
get_consinfo(int *kmunit_p)
{
	dev_info_t *dev;

	/*
	 * Find the "zs" that has the property "keyboard"
	 * associated with it.  For machines with multiple keyboards,
	 * we use the first keyboard/mouse device as the system kbd/mouse.
	 * The following loop assumes the list is in probe order w.r.t.
	 * machines that have multiple kbd/mouse devices.
	 */

	for (dev = devnamesp[zsmajor()].dn_head; dev != NULL;
	    dev = ddi_get_next(dev))  {

		if (!DDI_CF2(dev))		/* XXX: Need framework fcn */
			continue;
		/*
		 * XXX: Should be using the e_ddi_getprop interface, which
		 * XXX: takes care of the install and hold of the driver.
		 * XXX: But we don't know which dev_t we want, and the
		 * XXX: interface requires a dev_t.
		 */

		if (ddi_getprop(DDI_DEV_T_ANY, dev, DDI_PROP_DONTPASS,
		    "keyboard", 0) != 0) {
			*kmunit_p = ddi_get_instance(dev);
#ifdef	PATH_DEBUG
			if (path_debug)
				prom_printf("get_consinfo: kmunit is zs%d\n",
				    ddi_get_instance(dev));
#endif	PATH_DEBUG
			break;
		}
	}
}

#else	/* !MPSAS */

extern char *get_sim_console_name();

void
consconfig(void)
{
	int error, rval;
	int zeropgrp = 0;
	int major;
	char *simc = get_sim_console_name();

	mon_clock_stop();	/* turn off monitor polling clock */

	(void) ddi_install_driver(simc);
	major = ddi_name_to_major(simc);
	rconsdev = makedevice(major, 0);
	/*
	 * Console is a CPU serial port.
	 */

	rconsvp = makespecvp(rconsdev, VCHR);

	/*
	 * Opening causes interrupts, etc. to be initialized.
	 * Console device drivers must be able to do output
	 * after being closed!
	 */

	if (error = VOP_OPEN(&rconsvp, FREAD+FWRITE+FNOCTTY, kcred))
		printf("console open failed: error %d\n", error);

	/*
	 * "Undo undesired ttyopen side effects" (not needed anymore
	 * in 5.0 -verified by commenting this out and running anyway.
	 * This zereod u.u_ttyp and u.u_ttyd and u.u_procp->p_pgrp).
	 */

	(void) strioctl(rconsvp, TIOCSPGRP, (intptr_t)&zeropgrp,
			FREAD + FNOCTTY, K_TO_K, kcred, &rval);

	/* now we must close it to make console logins happy */
	(void) ddi_hold_installed_driver(major);
	VOP_CLOSE(rconsvp, FREAD+FWRITE, 1, (offset_t)0, kcred);
	rwsconsvp = rconsvp;
}

#endif /* !SAS && !MPSAS */
