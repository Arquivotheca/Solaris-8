/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consconfig.c	1.29	99/10/22 SMI"

/*
 * The console configuration process for the sun4u platforms is divided between
 * the consconfig misc module and the consconfig_dacf dacf module.  The
 * consconfig portion is responsible for determining the stdin, stdout, kb,
 * and ms paths, causing the drivers to be loaded through
 * ddi_pathname_to_dev_t, and initializing various kernel global variables
 * associated with the console.  consconfig is also responsible for setting
 * up the upper portions for the keyboard and mouse console streams.
 *
 * The consconfig_dacf portion is based upon the Extensions To Device
 * Autoconfigurations Framework (PSARC/1998/212). consconfig_dacf is
 * responsible for building and tearing down the lower portions of the keyboard
 * and mouse console streams.  See PSARC/1998/176 for a description of the
 * console streams and the effects of hotplugging.
 *
 * consconfig() is called by kern_setup2().  kern_setup() is in genunix and
 * it is called by main. Since consconfig isn't defined in genuix, a stub
 * mechanism is used in order for genunix to call consconfig at run time.
 * sparc/ml/modstubs.s defines consconfig as a "misc" module.
 *
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

#include <sys/strsubr.h>
#include <sys/errno.h>
#include <sys/devops.h>
#include <sys/note.h>
#include <sys/kmem.h>
#include <sys/consconfig.h>

/*
 * This is a useful variable for debugging the polled input code.  This
 * flag allows us to exercise the polled callbacks without switching the
 * prom's notion of the input device.  It allows us to do the debugging
 * over a tip line.
 *
 * This variable is defined in polled_io.c which is resident in unix.
 *
 */
extern int polled_debug;

/*
 * Variable for adjusting retries
 */
int consconfig_retries = 6;

/*
 * On supported configurations, the firmware defines the keyboard and mouse
 * paths.  However, during USB development, it is useful to be able to use
 * the USB keyboard and mouse on machines without full USB firmware support.
 * These variables may be set in /etc/system according to a machine's
 * USB configuration.  This module will override the firmware's values
 * with these.
 */
static char *usb_kb_path = NULL;
static char *usb_ms_path = NULL;
static char *beep_path = NULL;

/*
 * External functions
 */
extern void	printf(const char *fmt, ...);
extern major_t	ddi_name_to_major(char *name);
extern int	i_stdin_is_keyboard(void);
extern int	i_stdout_is_framebuffer(void);
extern int	i_setmodes(dev_t, struct termios *);
extern char    *i_kbdpath(void);
extern char    *i_mousepath(void);
extern char    *i_beeppath(void);
extern char    *i_stdinpath(void);
extern char    *i_stdoutpath(void);
extern int	space_store(char *key, uintptr_t ptr);

/*
 * External functions from consconfig_util.o
 */
extern dev_t	consconfig_util_getdev(char *driver_name, int minor);
extern consconfig_vnode_t *consconfig_util_createvp(dev_t dev);
extern int 	consconfig_util_openvp(consconfig_vnode_t *avp);
extern int 	consconfig_util_getfile(consconfig_vnode_t *avp);
extern int 	consconfig_util_getfd(consconfig_vnode_t *avp);
extern void	consconfig_util_closevp(consconfig_vnode_t *avp);
extern int	consconfig_util_link(consconfig_vnode_t *avp, int fd,
					int *muxid);
extern int	consconfig_util_unlink(consconfig_vnode_t *avp, int mux_id);
extern void 	consconfig_util_kbconfig(cons_state_t *sp,
					consconfig_vnode_t *avp,
					int kbdtranslatable,
					char *module_name);
extern void	consconfig_util_link_wc(cons_state_t *sp,
						consconfig_vnode_t *avp);
extern int 	consconfig_util_abort(consconfig_vnode_t *vp);
extern void 	consconfig_util_setup_polledio(consconfig_vnode_t *avp);
extern void 	consconfig_dprintf(int l, const char *fmt, ...);
extern int 	consconfig_errlevel;

/*
 * External global variables
 * 	consconfig is responsible for initializing these
 *	kernel global variables
 */
extern  dev_t	kbddev;
extern  dev_t	mousedev;
extern  dev_t	rconsdev;
extern  dev_t	stdindev;
extern  dev_t	fbdev;
extern  struct vnode    *fbvp;
extern  vnode_t	*rconsvp;
extern  vnode_t	*rwsconsvp;
extern struct vnode	*wsconsvp;
extern dev_t	rwsconsdev;
extern vnode_t	*stdinvp = NULL;

/*
 * Internal functions
 */
static void consconfig_print_paths();

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops, "console configuration 1.29"
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
 * This function kicks off the console configuration.
 * The driver loading and configuration starts when
 * ddi_pathname_to_dev_t() is called.
 */
void
consconfig(void)
{
	int	error;
	dev_t	wsconsdev;
	int	major;
	int	rval;
	klwp_t	*lwp;
	struct termios	termios;
	dev_t	dev;
	int	retry;
	consconfig_vnode_t 	*stdin_avp;
	consconfig_vnode_t 	*rcons_avp;
	dev_t	stdoutdev;
	cons_state_t	*sp;	/* console state pointer */
	dev_t		beepdev;

	/* Allocate space for the console state */
	sp = kmem_zalloc(sizeof (cons_state_t), KM_SLEEP);

	ASSERT(sp != NULL);

	/* Save the pointer for retrieval by the dacf functions */
	rval = space_store("consconfig", (uintptr_t)sp);

	ASSERT(rval == 0);

	/*
	 * Mutexes protect global variables from multiple threads
	 * calling the dacf functions at once
	 */
	mutex_init(&sp->cons_kb_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&sp->cons_ms_mutex, NULL, MUTEX_DRIVER, NULL);

	consconfig_print_paths();

	/* Initialize console information */
	sp->cons_keyboard_path = NULL;
	sp->cons_mouse_path = NULL;
	sp->cons_stdin_path = NULL;
	sp->cons_stdout_path = NULL;
	sp->cons_input_type = 0;
	sp->cons_keyboard_problem = 0;

	/*
	 * Find beep device
	 */
	if (beep_path == NULL) {
		beep_path = i_beeppath();
	}

	/*
	 * Find keyboard, mouse, stdin and stdout devices, if they
	 * exist on this platform.
	 */
	if (usb_kb_path != NULL) {
		sp->cons_keyboard_path = usb_kb_path;
	} else {
		sp->cons_keyboard_path = i_kbdpath();
	}

	if (usb_ms_path != NULL) {
		sp->cons_mouse_path = usb_ms_path;
	} else {
		sp->cons_mouse_path = i_mousepath();
	}

	/*
	 * The standard in device may or may not be the same as the keyboard
	 * Even if the keyboard is not the standard input, the keyboard
	 * console stream will still be built if the keyboard alias provided
	 * by the firmware exists and is valid.
	 */
	sp->cons_stdin_path = i_stdinpath();

	/* Identify the stdout driver */
	sp->cons_stdout_path = i_stdoutpath();

	/*
	 * Build the wc->conskbd portion of the keyboard console stream.
	 * Even if no keyboard is attached to the system, the upper
	 * layer of the stream will be built. If the user attaches
	 * a keyboard after the system is booted, the keyboard driver
	 * and module will be linked under conskbd.
	 *
	 * Errors are generally ignored here because conskbd and wc
	 * are pseudo drivers and should be present on the system.
	 */
	dev = consconfig_util_getdev("conskbd", 1);

	sp->cons_kbd_avp = consconfig_util_createvp(dev);

	/* conskbd doesn't fail opens for this minor number */
	(void) consconfig_util_openvp(sp->cons_kbd_avp);

	/* assume that conskbd exists on the system. */
	(void) consconfig_util_getfile(sp->cons_kbd_avp);
	(void) consconfig_util_getfd(sp->cons_kbd_avp);

	rwsconsdev = consconfig_util_getdev("wc", 0);

	sp->cons_rws_avp = consconfig_util_createvp(rwsconsdev);

	rwsconsvp = sp->cons_rws_avp->consconfig_vp;

	/* wc won't fail this open */
	(void) consconfig_util_openvp(sp->cons_rws_avp);

	/*
	 * Link conskbd under wc.
	 * consconfig_util_link() calls closeandsetf() which frees the
	 * sp->cons_kbd_avp->consconfig_fd that was allocated above.
	 *
	 * The act of linking conskbd under wc will cause wc to
	 * query the lower layers about their polled I/O routines
	 * (CONSOPENPOLLEDIO).  This will fail on this link because
	 * there is not a physical keyboard linked under conskbd.
	 *
	 * Since conskbd and wc are pseudo drivers, errors are
	 * generally ignored when linking and unlinking them.
	 */
	(void) consconfig_util_link(sp->cons_rws_avp,
				sp->cons_kbd_avp->consconfig_fd,
				&sp->cons_conskbd_muxid);

	/*
	 * At this point, we have the chain: wc->conskbd
	 */

	stdinvp = sp->cons_kbd_avp->consconfig_vp;

	/*
	 * now we must close it to make console logins happy
	 */
	(void) ddi_hold_installed_driver(getmajor(rwsconsdev));

	consconfig_util_closevp(sp->cons_rws_avp);

	/*
	 * The sp->cons_rws_avp is not destroyed at this point so that we
	 * can re-use it later.  Although we have closed the vnode,
	 * the vnode is still valid.
	 */

	/*
	 * When the machine is booting, the
	 * the keyboard minor node that is hooked into the console
	 * stream must match the keyboard that is defined by OBP
	 * as the console stream, since it is possible that the
	 * there may be multiple keyboards on the system, and it is
	 * not guarranteed that the first keyboard found will match
	 * the firmware's notion of the console keyboard.  After boot
	 * the user may hotplug the keyboard and this keyboard location
	 * may not be the same location as defined by the firmware.  So,
	 * after booting,  keyboard minor nodes are hooked into the
	 * console stream on a first come first serve basis.  The same
	 * goes for the mouse.
	 *
	 * Calling ddi_pathname_to_dev_t causes the drivers to be loaded.
	 * The attaching of the drivers will cause the creation of the
	 * keyboard and mouse minor nodes, which will in turn trigger the
	 * dacf framework to call the keyboard and mouse configuration
	 * tasks.  See PSARC/1998/212 for more details about the dacf
	 * framework.
	 *
	 * USB might be slow probing down the 6 possible hub levels and
	 * therefore we retry and delay in each iteration.
	 */

	kbddev = NODEV;
	mousedev = NODEV;
	stdindev = NODEV;
	stdoutdev = NODEV;

	sp->cons_dacf_booting = CONSCONFIG_BOOTING;

	/*
	 * Load the hardware-dependent beep driver
	 */
	if (beep_path != NULL) {

		uint_t		lkcnt;

		consconfig_dprintf(DPRINT_L2, "Beep path = %s\n", beep_path);

		i_ndi_block_device_tree_changes(&lkcnt);
		beepdev = ddi_pathname_to_dev_t(beep_path);
		i_ndi_allow_device_tree_changes(lkcnt);

		if (beepdev == NODEV) {
			consconfig_dprintf(DPRINT_L2, "Beep alias is NODEV\n");
		}
	}

	if (sp->cons_keyboard_path != NULL) {
		for (retry = 0; retry < consconfig_retries; retry++) {
			uint_t lkcnt;
			i_ndi_block_device_tree_changes(&lkcnt);
			kbddev = ddi_pathname_to_dev_t(
				sp->cons_keyboard_path);
			i_ndi_allow_device_tree_changes(lkcnt);

			if (kbddev != NODEV) {
				break;
			} else {
				delay(drv_usectohz(1000000));
			}
		}
	}

	if (kbddev == NODEV) {
		consconfig_dprintf(DPRINT_L2, "keyboard alias is NODEV\n");
	}

	if (sp->cons_mouse_path != NULL) {
		for (retry = 0; retry < consconfig_retries; retry++) {
			uint_t lkcnt;
			i_ndi_block_device_tree_changes(&lkcnt);
			mousedev =  ddi_pathname_to_dev_t(
				sp->cons_mouse_path);
			i_ndi_allow_device_tree_changes(lkcnt);

			if (mousedev != NODEV) {
				break;
			} else {
				delay(drv_usectohz(1000000));
			}
		}
	}

	if (sp->cons_stdin_path != NULL) {
		stdindev = ddi_pathname_to_dev_t(sp->cons_stdin_path);
	}

	if (sp->cons_stdout_path != NULL) {
		stdoutdev = ddi_pathname_to_dev_t(sp->cons_stdout_path);
	}

	sp->cons_dacf_booting = CONSCONFIG_DRIVERS_LOADED;

	/*
	 * At the conclusion of the ddi_pathname_to_dev_t calls, the keyboard
	 * and mouse drivers are linked into their respective console
	 * streams if the pathnames are valid.
	 */

	/*
	 * This is legacy special case code for the "cool" virtual console
	 * for the Starfire project.  It has been suggested that Starfire
	 * should have an "ssp-serial" node in the device tree and cvc should
	 * be bound to alleviate the special case.
	 */
	if (sp->cons_stdout_path
			!= NULL && stdindev == NODEV &&
			strstr(sp->cons_stdout_path, "ssp-serial")) {

		/*
		 * Setup the virtual console driver
		 * Note that console I/O will still go thru prom for now
		 * (notice we don't open the driver here). The cvc driver
		 * will be activated when /dev/console is opened by init.
		 * During that time, a cvcd daemon will be started that
		 * will open the cvcredir(ection) driver to facilitate
		 * the redirection of console I/O from cvc to cvcd.
		 */
		major = ddi_name_to_major("cvc");

		if (major == (major_t)-1) {
			return;
		}

		rconsdev = makedevice(major, 0);
		rconsvp = makespecvp(rconsdev, VCHR);
		(void) ddi_hold_installed_driver(major);

		/* No support for workstation console and its redirection */
		rwsconsvp = wsconsvp = NULL;

		return;
	}

	/*
	 * Now that we know what all the devices are, we can figure out
	 * what kind of console we have
	 */
	if (i_stdin_is_keyboard())  {
		/*
		 * Stdin is from the system keyboard
		 */
		sp->cons_input_type =
			CONSOLE_INPUT_KEYBOARD;

	} else if ((stdindev != NODEV) && (stdindev == stdoutdev)) {
		/*
		 * A reliable indicator that we are doing a remote console
		 * is that stdin and stdout are the same.  This is probably
		 * a tip line.
		 */
		sp->cons_input_type =
			CONSOLE_INPUT_TTY;
	} else {
		/*
		 * The input is from the serial port (tip line), but the
		 * output is not to the same port (or to the ffb).
		 */
		sp->cons_input_type =
			CONSOLE_INPUT_SERIAL_KEYBOARD;
	}

	lwp = ttolwp(curthread);

	/*
	 * Initialize the external global variables for the
	 * serial line case.
	 */
	if (sp->cons_input_type == CONSOLE_INPUT_TTY) {

		consconfig_dprintf(DPRINT_L0, "CONSOLE_INPUT_TTY\n");

		/*
		 * We are opening this to initialize interrupts and other
		 * things.  Console device drivers must be able to output
		 * after being closed.
		 */
		rconsdev = stdindev;

		rcons_avp = consconfig_util_createvp(rconsdev);

		rconsvp = rcons_avp->consconfig_vp;

		/*
		 * if ttya can't be opened, then the machine will
		 * panic because the console global variables won't
		 * be set up properly.  Checking the return values
		 * isn't useful.
		 */
		(void) consconfig_util_openvp(rcons_avp);

		/*
		 * Now we must close it to make console logins happy
		 */
		(void) ddi_hold_installed_driver(getmajor(rconsdev));

		consconfig_util_closevp(rcons_avp);
	}

	if (i_stdout_is_framebuffer()) {

		consconfig_dprintf(DPRINT_L0, "stdout is framebuffer\n");

		/*
		 * Console output is a framebuffer.
		 * Find the framebuffer driver if we can, and make
		 * ourselves a shadow vnode to track it with.
		 */
		fbdev = stdoutdev;

		if (fbdev == NODEV) {
			/*
			 * Look at 1097995 if you want to know why this
			 * might be a problem ..
			 */
			consconfig_dprintf(DPRINT_L3,
			    "Can't find driver for console framebuffer");
		} else {
			fbvp = makespecvp(fbdev, VCHR);
		}
	}

	consconfig_dprintf(DPRINT_L0,
		"mousedev %lx\nkbddev %lx\nfbdev %lx\nrconsdev %lx\n",
		    mousedev,  kbddev, fbdev, rconsdev);

	/*
	 * Previous versions of consconfig call debug_enter() if
	 * kbddev is NODEV.  If the keyboard is not the console
	 * input, then the user can type "go" and the machine will
	 * continue to boot.  If the keyboard is the console,
	 * then the user is out of luck.  The machine has to
	 * be power cycled.
	 *
	 * This consconfig_dacf file attempts to allow the machine
	 * to boot if kbddev equals NODEV.  kbddev may equal NODEV
	 * if the keyboard alias is invalid, the keyboard hardware
	 * is missing, or there is a problem with the keyboard hardware
	 * during the loading of the drivers.  In the case of USB,
	 * it is possible that the keyboard alias does not exist if
	 * the firmware can not locate a USB keyboard on the bus.  This
	 * is different from the serial case where the keyboard alias
	 * is hardcoded to the same value regardless of whether or not
	 * a keyboard is present.  Therefore, the consconfig_dacf
	 * allows the machine to continue booting if kbddev is
	 * NODEV.
	 *
	 * Allowing the machine to boot if kbddev equals NODEV is safe
	 * and more user friendly than doing the debug_enter().
	 * If kbddev equals NODEV and the console input device
	 * is tty, then it is fine to let the machine continue to
	 * boot.  If a user attaches a keyboard later, the keyboard
	 * will be hooked into the console stream with the dacf
	 * functions.
	 *
	 * If kbddev equals NODEV and the console input
	 * device is the keyboard, a message is printed out.  This
	 * scenario could happen if the user unplugged the keyboard
	 * during boot or the keyboard hardware had a problem while
	 * the drivers are loading.  In this case, a message is
	 * printed out to warn that their is a problem with the keyboard.
	 * If the user attaches a keyboard later, the keyboard will be
	 * hooked into the console stream with the dacf functions.
	 *
	 * Allowing kbddev to be NODEV and letting the system boot is not
	 * a problem.  The only drivers that look at kbbdev are the serial
	 * drivers.  These drivers look at this value to see if they should
	 * allow abort on a break.  If there is something wrong with the
	 * keyboard or the keyboard isn't there, these drivers won't
	 * be attached for a keyboard instance.  Also,
	 * wscons->conskbd is built up regardless of the value of kbddev.
	 * So, the upper stream will always be in place, and this
	 * is different from the previous consconfig.
	 */
	if ((sp->cons_input_type == CONSOLE_INPUT_KEYBOARD) &&
		(kbddev == NODEV)) {

		consconfig_dprintf(DPRINT_L3, "Error with console keyboard\n");

		/*
		 * If there is a problem with the keyboard during the
		 * driver loading, then the polled input won't
		 * get setup properly if polled input is needed.  This means
		 * that if the keyboard is hotplugged, the keyboard would
		 * work normally, but going down to the debugger would not
		 * work if polled input is required.  This field is set
		 * here.  The next time a keyboard is plugged in, the
		 * field is checked in order to give the next keyboard
		 * a chance at being registered for console input.
		 *
		 * Although this code will rarely be needed, USB keyboards
		 * can be flakey, so this code  will be useful on the
		 * occasion that the keyboard doesn't enumerate when the
		 * drivers are loaded.
		*/
		sp->cons_keyboard_problem =
			CONSCONFIG_KB_PROBLEM;
	}

	switch (sp->cons_input_type) {

	case CONSOLE_INPUT_KEYBOARD:
		sp->cons_final_avp = sp->cons_rws_avp;

		/* Using physical keyboard */
		stdindev = kbddev;

		break;

	case CONSOLE_INPUT_TTY:
		sp->cons_final_avp = rcons_avp;

		/* Console is tty[a-z] */
		stdindev = rconsdev;

		break;

	    case CONSOLE_INPUT_SERIAL_KEYBOARD:
		consconfig_dprintf(DPRINT_L0, "stdin is serial keyboard\n");

		/*
		 * Non-keyboard input device, but not rconsdev.
		 * This is known as the "serial keyboard" case - the
		 * most likely use is someone has a keyboard attached
		 * to a serial port (tip) and still has output on a
		 * framebuffer.
		 *
		 * In this case, the serial driver must be linked
		 * directly beneath wc.  Since conskbd was linked
		 * underneath wc above, first unlink conskbd.
		 *
		 * Note that errors are generally ignored for the
		 * serial keyboard case.
		 */
		(void) consconfig_util_openvp(sp->cons_rws_avp);

		(void) consconfig_util_unlink(sp->cons_rws_avp,
					sp->cons_conskbd_muxid);

		/*
		 * now we must close it to make console logins happy
		 */
		(void) ddi_hold_installed_driver(getmajor(rwsconsdev));

		consconfig_util_closevp(sp->cons_rws_avp);

		/*
		 * At this point, conskbd is unlinked from underneath wc.
		 */
		stdin_avp = consconfig_util_createvp(stdindev);

		/*
		 * We will now open up the serial keyboard, configure it,
		 * and link it underneath wc.
		 */
		stdinvp = stdin_avp->consconfig_vp;

		/*
		 * Do the linking if the keyboard opens
		 */
		if (consconfig_util_openvp(stdin_avp) == 0) {

			consconfig_util_kbconfig(sp,
			    stdin_avp, TR_CANNOT, "kb");

			/* Re-set baud rate */
			(void) strioctl(stdin_avp->consconfig_vp, TCGETS,
			    (intptr_t)&termios, FREAD+FNOCTTY, K_TO_K, kcred,
			    &rval);

			/* Set baud rate */
			if (i_setmodes(stdindev, &termios) == 0) {
				if (error = strioctl(stdin_avp->consconfig_vp,
				    TCSETSF, (intptr_t)&termios, FREAD+FNOCTTY,
				    K_TO_K, kcred, &rval)) {
					cmn_err(CE_WARN,
					    "consconfig: TCSETSF error %d",
					    error);
					lwp->lwp_error = 0;
				}
			}
		}

		/*
		 * Now link the serial keyboard underneath wc.
		 */
		consconfig_util_link_wc(sp, stdin_avp);

		sp->cons_final_avp = sp->cons_rws_avp;

		break;
	}

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

	if (major != (major_t)-1) {

		wsconsdev = makedevice(major, 0);

		wsconsvp = makespecvp(wsconsdev, VCHR);
	}

	/*
	 * Enable abort on the console
	 */
	(void) consconfig_util_abort(sp->cons_final_avp);

	/*
	 * Set up polled input if it is supported by the console devices
	 */
	if (polled_debug) {
		/*
		 * In the debug case, register the keyboard polled entry
		 * points, but don't throw the switch in the debugger.  This
		 * allows the polled entry points to be checked by hand
		 */
		consconfig_util_setup_polledio(sp->cons_rws_avp);
	} else {
		consconfig_util_setup_polledio(sp->cons_final_avp);
	}

	/*
	 * Use the redirection device/workstation console pair as the "real"
	 * console if the latter hasn't already been set.
	 */
	if (!rconsvp) {

		consconfig_dprintf(DPRINT_L0, "do the redirection\n");

		/*
		 * The workstation console driver needs to see rwsconsvp, but
		 * all other access should be through the redirecting driver.
		 */
		(void) ddi_hold_installed_driver(major);
		rconsdev = wsconsdev;
		rconsvp = wsconsvp;
	}

	/*
	 * OK to use rconsvp, now.
	 */
}

/*
 * consconfig_print_paths:
 *      Function to print out the various paths
 */
static void
consconfig_print_paths()
{
	char    *path;

	if (consconfig_errlevel > DPRINT_L0) {

		return;
	}

	path = usb_kb_path;
	if (path != NULL)
		consconfig_dprintf(DPRINT_L0, "usb keyboard path = %s\n", path);
	path = i_kbdpath();
	if (path != NULL)
		consconfig_dprintf(DPRINT_L0, "keyboard path = %s\n", path);

	path = usb_ms_path;
	if (path != NULL)
		consconfig_dprintf(DPRINT_L0, "usb mouse path = %s\n", path);

	path = i_mousepath();
	if (path != NULL)
		consconfig_dprintf(DPRINT_L0, "mouse path = %s\n", path);

	path = i_stdinpath();
	if (path != NULL)
		consconfig_dprintf(DPRINT_L0, "stdin path = %s\n", path);

	path = i_stdoutpath();
	if (path != NULL)
		consconfig_dprintf(DPRINT_L0, "stdout path = %s\n", path);
}
