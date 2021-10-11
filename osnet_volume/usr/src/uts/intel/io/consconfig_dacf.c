#define	WORK_AROUND_4235048

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consconfig_dacf.c	1.35	99/08/19 SMI"

/*
 * This is a dacf module based upon the Extensions to Device Autoconfiguration
 * project.  See PSARC/1998/212 for more details.
 *
 * This module performs two functions.  First, it kicks off the driver loading
 * of the console devices during boot in dynamic_console_config().
 * The loading of the drivers for the console devices triggers the
 * additional device autoconfiguration to link the drivers into the keyboard
 * and mouse console streams.
 *
 * The second function of this module is to provide the dacf functions
 * to be called after a driver has attached and before it detaches.  For
 * example, a driver associated with the keyboard will have kb_config called
 * after the driver attaches and kb_unconfig before it detaches.  Similar
 * configuration actions are performed on behalf of minor nodes representing
 * mice.  The configuration functions for the attach case take a module
 * name as a paramater.  The module is pushed on top of the driver during
 * the configuration.
 *
 * Although the dacf framework is used to configure all keyboards and mice,
 * their primary function is to allow keyboard and mouse hotplugging.  The
 * state of the console stream when a USB keyboard or mouse is hotplugged
 * is described in PSARC/1998/176.
 *
 * This dacf module currently does not support multiple keyboards or multiple
 * mice. See PSARC/1998/176 for the issues with supporting multipe keyboars
 * and multiple mice.
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
#include <sys/consconfig_dacf.h>
#include <sys/dacf.h>

/*
 * Tasks
 */
static int	kb_config(dacf_infohdl_t, dacf_arghdl_t, int);
static int	kb_unconfig(dacf_infohdl_t, dacf_arghdl_t, int);
static int	ms_config(dacf_infohdl_t, dacf_arghdl_t, int);
static int	ms_unconfig(dacf_infohdl_t, dacf_arghdl_t, int);

/*
 * External functions
 */
extern major_t	ddi_name_to_major(char *name);
extern int	i_stdin_is_keyboard(void);
extern int	i_stdout_is_framebuffer(void);
extern	char	*i_kbdpath(void);
extern	char	*i_fbpath(void);
extern	char	*i_mousepath(void);
extern	char	*i_stdinpath(void);
extern	char	*i_stdoutpath(void);
extern void	printf(const char *fmt, ...);
extern major_t  ddi_name_to_major(char *name);

/*
 * External global variables
 */
extern  dev_t		kbddev;
extern  dev_t		mousedev;
extern  dev_t		rconsdev;
extern  dev_t		stdindev;
extern  dev_t		fbdev;
extern  struct vnode	*fbvp;
extern  vnode_t		*rconsvp;
extern  vnode_t		*rwsconsvp;
extern struct vnode	*wsconsvp;
extern dev_t		rwsconsdev;
extern vnode_t		*stdinvp = NULL;

/*
 * Internal functions
 */
static void consconfig_dacf_mouselink(consconfig_dacf_vnode_t *mouse_avp,
					const char *module_name);
static void consconfig_dacf_mouseunlink(void);
static void consconfig_dacf_kblink(consconfig_dacf_vnode_t *kbd_avp,
					    const char *module_name);
static void consconfig_dacf_kbunlink(void);
static void consconfig_dacf_link_conskbd(consconfig_dacf_vnode_t *kbd_avp,
							int *);
static void consconfig_dacf_unlink_conskbd(int mux_id);
static void consconfig_dacf_link_consms(consconfig_dacf_vnode_t *mouse_avp,
					int *);
static int consconfig_dacf_kbconfig(consconfig_dacf_vnode_t *avp,
					int kbdtranslatable,
					const char *module_name);
static void consconfig_dacf_dprintf(int l, const char *fmt, ...);
static dev_t consconfig_dacf_getdev(char *driver_name, int minor);
static consconfig_dacf_vnode_t *consconfig_dacf_createvp(dev_t dev);
static int consconfig_dacf_openvp(consconfig_dacf_vnode_t *avp);
static void consconfig_dacf_closevp(consconfig_dacf_vnode_t *avp);
static void consconfig_dacf_destroyvp(consconfig_dacf_vnode_t *avp);
static int consconfig_dacf_link(consconfig_dacf_vnode_t *avp, int fd,
				int *muxid);
static int consconfig_dacf_unlink(consconfig_dacf_vnode_t *avp, int mux_id);
static int consconfig_dacf_getfd(consconfig_dacf_vnode_t *avp);
static int consconfig_dacf_getfile(consconfig_dacf_vnode_t *avp);
static void consconfig_dacf_push(consconfig_dacf_vnode_t *avp,
					const char *module_name);
static cons_polledio_t *consconfig_get_polledio(consconfig_dacf_vnode_t *avp);
static int consconfig_dacf_abort(consconfig_dacf_vnode_t *vp);
static void consconfig_dacf_setup_polledio(consconfig_dacf_vnode_t *avp);
static void consconfig_holdvp(consconfig_dacf_vnode_t *avp);
static void consconfig_dacf_print_paths(void);
static dev_t consconfig_pathname_to_dev_t(char *name, char *path);
static void consconfig_find_fb(char **ppath, dev_t *pdev);
static int consconfig_setup_wc_output(consconfig_dacf_vnode_t *avp,
	char *outpath);
static consconfig_dacf_vnode_t	*consconfig_open_conskbd(void);
static void consconfig_setup_wc(consconfig_dacf_vnode_t *in_avp,
	char *out_path, dev_t *pdev, consconfig_dacf_vnode_t **pavp,
	int *pmuxid);
static void consconfig_dacf_unconfig_current_console_keyboard(void);
static void consconfig_dacf_unconfig_current_console_mouse(void);

/*
 * Internal variables
 *	These globals are unavoidable because they must be shared between
 *	the dynamic_console_config() function which only runs during boot
 *	and the dacf functions which run dynamically.
 */
static consconfig_dacf_vnode_t	*conskbd_avp;	/* conskbd file pointers */
static consconfig_dacf_vnode_t	*kbd_avp;	/* kbd device file pointers */
static consconfig_dacf_vnode_t	*mouse_avp;	/* mouse device file pointers */
static consconfig_dacf_vnode_t 	*rwscons_avp;	/* wc file pointers */
static kmutex_t	consconfig_kb_attach_sync; /* protects kb configuration */
static kmutex_t	consconfig_ms_attach_sync; /* protects mouse configuration */
static consconfig_dacf_info_t	consconfig_info;  /* console information */
static consconfig_dacf_vnode_t  *final_consavp;

/* ??? get rid of these with dacf interfaces */
static int		conskbd_muxid;	/* conskbd->wc muxid */
static int		mouse_muxid;	/* mouse device->consms muxid */
static int		kb_muxid;	/* kbd device->conskbd muxid */

/*
 * Debug variables
 */

/*
 * consconfig_dacf_errlevel:  debug verbosity; smaller numbers are more
 * verbose.
 */
static int consconfig_dacf_errlevel = DPRINT_L3;

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

static dacf_op_t kbconfig_op[] = {
	{ DACF_OPID_POSTATTACH,	kb_config },
	{ DACF_OPID_PREDETACH,	kb_unconfig },
	{ DACF_OPID_END,	NULL },
};

static dacf_op_t msconfig_op[] = {
	{ DACF_OPID_POSTATTACH,	ms_config },
	{ DACF_OPID_PREDETACH,	ms_unconfig },
	{ DACF_OPID_END,	NULL },
};

static dacf_opset_t opsets[] = {
	{ "kb_config",	kbconfig_op },
	{ "ms_config",	msconfig_op },
	{ NULL,		NULL }
};

struct dacfsw dacfsw = {
	DACF_MODREV_1,
	opsets,
};

struct modldacf modldacf = {
	&mod_dacfops,   /* Type of module */
	"Consconfig DACF 1.35",
	&dacfsw
};

struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldacf, NULL
};

int
_init(void)
{
	int rval;

	rval = mod_install(&modlinkage);

	return (rval);
}

_fini()
{
	/*
	 * This module contains state that must be retained across
	 * keyboard and mouse hotplugs.  So, the module can not
	 * be unloaded.
	 */
	return (EBUSY);
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Configure keyboard and mouse. Main entry here.
 */
void
dynamic_console_config(void)
{
	dev_t			wsconsdev;
	consconfig_dacf_vnode_t	*rcons_avp;
	dev_t			stdoutdev;
	extern void		kadb_uses_kernel(void);

	/*
	 * Mutexes protect global variables from multiple threads
	 * calling the dacf functions at once
	 */
	mutex_init(&consconfig_kb_attach_sync, "KB Attach Sync Lock",
		MUTEX_DRIVER, NULL);

	mutex_init(&consconfig_ms_attach_sync, "MS Attach Sync Lock",
		MUTEX_DRIVER, NULL);

	consconfig_dacf_print_paths();


	/* Initialize console information */
	consconfig_info.consconfig_dacf_keyboard_path = NULL;
	consconfig_info.consconfig_dacf_mouse_path = NULL;
	consconfig_info.consconfig_dacf_stdin_path = NULL;
	consconfig_info.consconfig_dacf_stdout_path = NULL;
	consconfig_info.consconfig_dacf_console_input_type = 0;
	consconfig_info.consconfig_dacf_keyboard_problem = B_FALSE;

	/*
	 * Find keyboard, mouse, stdin and stdout devices, if they
	 * exist on this platform.
	 */
	if (usb_kb_path != NULL) {
		consconfig_info.consconfig_dacf_keyboard_path = usb_kb_path;
	} else {
		consconfig_info.consconfig_dacf_keyboard_path = i_kbdpath();
	}

	if (usb_ms_path != NULL) {
		consconfig_info.consconfig_dacf_mouse_path = usb_ms_path;
	} else {
		consconfig_info.consconfig_dacf_mouse_path = i_mousepath();
	}

	consconfig_find_fb(&consconfig_info.consconfig_dacf_fb_path,
		&fbdev);

	if (i_stdin_is_keyboard() && usb_kb_path != NULL)  {
		consconfig_info.consconfig_dacf_stdin_path =
			consconfig_info.consconfig_dacf_keyboard_path;
	} else {
		/*
		 * The standard in device may or may not be the same as
		 * the keyboard. Even if the keyboard is not the
		 * standard input, the keyboard console stream will
		 * still be built if the keyboard alias provided by the
		 * firmware exists and is valid.
		 */
		consconfig_info.consconfig_dacf_stdin_path = i_stdinpath();
	}

	/* Identify the stdout driver */
	consconfig_info.consconfig_dacf_stdout_path = i_stdoutpath();

	/*
	 * Build the wc->conskbd portion of the keyboard console stream.
	 * Even if no keyboard is attached to the system, the upper
	 * layer of the stream will be built. If the user attaches
	 * a keyboard after the system is booted, the keyboard driver
	 * and module will be linked under conskbd.
	 */
	conskbd_avp = consconfig_open_conskbd();
	DPRINTF(DPRINT_L0, "conskbd_avp %p\n", conskbd_avp);

	consconfig_setup_wc(conskbd_avp,
			consconfig_info.consconfig_dacf_fb_path,
			&rwsconsdev, &rwscons_avp, &conskbd_muxid);
	rwsconsvp = CONSCONFIG_VNODE(rwscons_avp);

	/*
	 * At this point, we have the chain: wc->conskbd
	 */

	/*
	 * NEEDSWORK:  On its face, this seems incorrect.  The keyboard,
	 * in whatever guise, should be "standard in" only if the keyboard
	 * plus frame buffer is the console; it's wrong if the console is
	 * a serial line.
	 */
	if (conskbd_avp != NULL)
		stdinvp = CONSCONFIG_VNODE(conskbd_avp);

	/*
	 * Calling ddi_pathname_to_dev_t causes the drivers to be loaded.
	 * The attaching of the drivers will cause the creation of the
	 * keyboard and mouse minor nodes, which will in turn trigger the
	 * dacf framework to call the keyboard and mouse configuration
	 * tasks.  See PSARC/1998/212 for more details about the dacf
	 * framework.
	 *
	 * USB might be slow probing down the 6 possible hub levels and
	 * therefore we rety and delay in each iteration.
	 */

	kbddev = NODEV;
	mousedev = NODEV;
	stdindev = NODEV;
	stdoutdev = NODEV;

	kbddev = consconfig_pathname_to_dev_t("keyboard",
		consconfig_info.consconfig_dacf_keyboard_path);
	mousedev = consconfig_pathname_to_dev_t("mouse",
		consconfig_info.consconfig_dacf_mouse_path);
	stdindev = consconfig_pathname_to_dev_t("stdin",
		consconfig_info.consconfig_dacf_stdin_path);
	stdoutdev = consconfig_pathname_to_dev_t("stdout",
		consconfig_info.consconfig_dacf_stdout_path);

	/*
	 * At the conclusion of the ddi_pathname_to_dev_t calls, the keyboard
	 * and mouse drivers are linked into their respective console
	 * streams if the pathnames are valid.
	 */

	/*
	 * Now that we know what all the devices are, we can figure out
	 * what kind of console we have
	 */
	if (i_stdin_is_keyboard())  {

		/*
		 * Stdin is from the system keyboard
		 */
		consconfig_info.consconfig_dacf_console_input_type =
					    CONSOLE_INPUT_KEYBOARD;

	} else if ((stdindev != NODEV) && (stdindev == stdoutdev)) {

		/*
		 * A reliable indicator that we are doing a remote console
		 * is that stdin and stdout are the same.  This is probably
		 * a tip line.
		 */
		consconfig_info.consconfig_dacf_console_input_type =
							    CONSOLE_INPUT_TTY;
	} else {
		cmn_err(CE_WARN,
		    "consconfig_dacf:  "
		    "unsupported console input/output combination");
		consconfig_info.consconfig_dacf_console_input_type =
						    CONSOLE_INPUT_UNSUPPORTED;
	}


	/*
	 * Initialize the external global variables for the
	 * serial line case.
	 */
	if (consconfig_info.consconfig_dacf_console_input_type
						== CONSOLE_INPUT_TTY) {

		DPRINTF(DPRINT_L0, "CONSOLE_INPUT_TTY\n");

		/*
		 * We are opening this to initialize interrupts and other
		 * things.  Console device drivers must be able to output
		 * after being closed.
		 */
		rconsdev = stdindev;

		if (rconsdev != NODEV) {
			rcons_avp = consconfig_dacf_createvp(rconsdev);

			if (rcons_avp != NULL) {
				rconsvp = CONSCONFIG_VNODE(rcons_avp);

				/*
				 * NEEDSWORK:  ignore errors.  is this right?
				 */
				(void) consconfig_dacf_openvp(rcons_avp);

				/*
				 * Now we must close it to make console logins
				 * happy
				 */
				(void) ddi_hold_installed_driver(
							getmajor(rconsdev));

				consconfig_dacf_closevp(rcons_avp);
			}
		}
	}

	if (i_stdout_is_framebuffer()) {

		DPRINTF(DPRINT_L0, "stdout is framebuffer\n");

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
			DPRINTF(DPRINT_L3,
			    "Can't find driver for console framebuffer");
		} else {
			fbvp = makespecvp(fbdev, VCHR);
		}
	}

	DPRINTF(DPRINT_L0,
		"mousedev %lx\nkbddev %lx\nfbdev %lx\nrconsdev %lx\n",
		    mousedev,  kbddev, fbdev, rconsdev);


	switch (consconfig_info.consconfig_dacf_console_input_type) {

	case CONSOLE_INPUT_KEYBOARD:

		/*
		 * Previous versions of consconfig call debug_enter()
		 * if kbddev is NODEV.  If the keyboard is not the
		 * console input, then the user can type "go" and the
		 * machine will continue to boot.  If the keyboard is
		 * the console, then the user is out of luck.  The
		 * machine has to be power cycled.

		 * This consconfig_dacf file attempts to allow the
		 * machine to boot if kbddev equals NODEV.  kbddev may
		 * equal NODEV if the keyboard alias is invalid, the
		 * keyboard hardware is missing, or there is a problem
		 * with the keyboard hardware during the loading of the
		 * drivers.  In the case of USB, it is possible that
		 * the keyboard alias does not exist if the firmware
		 * can not locate a USB keyboard on the bus.  This is
		 * different from the serial case where the keyboard
		 * alias is hardcoded to the same value regardless of
		 * whether or not a keyboard is present.  Therefore,
		 * the consconfig_dacf allows the machine to continue
		 * booting if kbddev is NODEV.

		 * Allowing the machine to boot if kbddev equals NODEV
		 * is safe and more user friendly than doing the
		 * debug_enter().  If kbddev equals NODEV and the
		 * console input device is tty, then it is fine to let
		 * the machine continue to boot.  If a user attaches a
		 * keyboard later, the keyboard will be hooked into the
		 * console stream with the dacf functions.

		 * If kbddev equals NODEV and the console input device
		 * is the keyboard, a message is printed out.  This
		 * scenario could happen if the user unplugged the
		 * keyboard during boot or the keyboard hardware had a
		 * problem while the drivers are loading.  In this
		 * case, a message is printed out to warn that their is
		 * a problem with the keyboard.  If the user attaches a
		 * keyboard later, the keyboard will be hooked into the
		 * console stream with the dacf functions.

		 * Allowing kbddev to be NODEV and letting the system
		 * boot is not a problem.  The only drivers that look
		 * at kbbdev are the serial drivers.  These drivers
		 * look at this value to see if they should allow abort
		 * on a break.  If there is something wrong with the
		 * keyboard or the keyboard isn't there, these drivers
		 * won't be attached for a keyboard instance.  Also,
		 * wscons->conskbd is built up regardless of the value
		 * of kbddev.  So, the upper stream will always be in
		 * place, and this is different from the previous
		 * consconfig.
		 */

		if (kbddev == NODEV) {

			DPRINTF(DPRINT_L3, "Error with console keyboard\n");

			/*
			 * If there is a problem with the keyboard
			 * during the driver loading, then the polled
			 * input won't get setup properly if polled
			 * input is needed.  This means that if the
			 * keyboard is hotplugged, the keyboard would
			 * work normally, but going down to the
			 * debugger would not work if polled input is
			 * required.  This field is set here.  The next
			 * time a keyboard is plugged in, the field is
			 * checked in order to give the next keyboard a
			 * chance at being registered for console
			 * input.
			 *
			 * Although this code will rarely be needed,
			 * USB keyboards can be flakey, so this code
			 * will be useful on the occasion that the
			 * keyboard doesn't enumerate when the drivers
			 * are loaded.
			 */
			consconfig_info.consconfig_dacf_keyboard_problem =
				B_TRUE;

		}
		final_consavp = rwscons_avp;

		/* Using physical keyboard */
		stdindev = kbddev;

		break;

	case CONSOLE_INPUT_TTY:

		final_consavp = rcons_avp;

		/* Console is tty[a-z] */
		stdindev = rconsdev;

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
	wsconsdev = consconfig_dacf_getdev("iwscn", 0);
	if (wsconsdev != NODEV)
		wsconsvp = makespecvp(wsconsdev, VCHR);
	else
		wsconsvp = NULL;

	/*
	 * Enable abort on the console
	 */
	(void) consconfig_dacf_abort(final_consavp);

	/*
	 * Set up polled input if it is supported by the console devices
	 */
	consconfig_dacf_setup_polledio(final_consavp);
	kadb_uses_kernel();

	/*
	 * Use the redirection device/workstation console pair as the "real"
	 * console if the latter hasn't already been set.
	 */
	if (!rconsvp) {

		DPRINTF(DPRINT_L0, "do the redirection\n");

		/*
		 * The workstation console driver needs to see rwsconsvp, but
		 * all other access should be through the redirecting driver.
		 */
		(void) ddi_hold_installed_driver(getmajor(wsconsdev));
		rconsdev = wsconsdev;
		rconsvp = wsconsvp;
	}

	/*
	 * OK to use rconsvp, now.
	 */
	/*
	 * NEEDSWORK:  TOTAL HACK
	 * Kick-start the USB subsystem by loading the host controller
	 * driver(s).
	 */
	(void) ddi_install_driver("uhci");
	(void) ddi_install_driver("ohci");
}

/*
 * This is the post-attach / pre-detach action function for the keyboard.
 * This function is associated with a node type in /etc/actions.
 *
 * Arguments:
 *	dev	- device that is being attached.
 *	arg	- this is the argument that was setup in /etc/actions
 *	dir	- specifies if this action function is being called as
 *		  a result of a post-attach or a pre-detach.
 */

static int
kb_config(dacf_infohdl_t minor_hdl, dacf_arghdl_t arg_hdl, int flags)
{
_NOTE(ARGUNUSED(flags))
	major_t		major;
	minor_t		minor;
	const char	*pushmod;
	dev_t		dev;
	int		res;
	int		error;
#if	defined(WORK_AROUND_4235048)
	static boolean_t running = B_FALSE;
	static kmutex_t running_mutex = { 0 };
#endif

	DPRINTF(DPRINT_L0, "kb_config callback\n");

	pushmod = dacf_get_arg(arg_hdl, "pushmod");

	major = ddi_name_to_major((char *)dacf_driver_name(minor_hdl));
	if (major == (major_t)-1) {
		DPRINTF(DPRINT_L0, "invalid major #\n");

		return (DDI_FAILURE);
	}

	minor = dacf_minor_number(minor_hdl);

	dev = makedevice(major, minor);

	DPRINTF(DPRINT_L0, "dev = 0x%lx\n", dev);

	DPRINTF(DPRINT_L0, "major 0x%lx\n", major);

	DPRINTF(DPRINT_L2, "kb_config: driver name %s\n",
			(char *)dacf_driver_name(minor_hdl));

#if	defined(WORK_AROUND_4235048)
	mutex_enter(&running_mutex);
	if (running) {
		DPRINTF(DPRINT_L2, "kb_config: recursive call, bailing out\n");
		mutex_exit(&running_mutex);
		return (DDI_SUCCESS);
	}
	running = B_TRUE;
	mutex_exit(&running_mutex);
#endif

	/*
	 * Access to the global variables is synchronized
	 */
	mutex_enter(&consconfig_kb_attach_sync);

	/*
	 * If we already have a kbd_avp then there is already a keyboard
	 * plugged into the system.  Unlink it, preferring the newly added
	 * keyboard.
	 */
	if (kbd_avp != NULL) {
		DPRINTF(DPRINT_L0, "Replacing the console keyboard\n");
		consconfig_dacf_unconfig_current_console_keyboard();
	}

	/*
	 * Create a vnode pointer so that we can perform pushing or
	 * popping functions on the keyboard device.
	 */
	kbd_avp = consconfig_dacf_createvp(dev);

	error = consconfig_dacf_openvp(kbd_avp);
	if (error) {
		res = DDI_FAILURE;
		goto done;
	}

	/*
	 * create the keyboard stream.
	 */
	consconfig_dacf_kblink(kbd_avp, pushmod);

	/*
	 * See if there was a problem with the console keyboard during boot.  If
	 * so, try to register polled input for this keyboard.
	 */
	if (consconfig_info.consconfig_dacf_keyboard_problem) {
		consconfig_dacf_setup_polledio(final_consavp);
		consconfig_info.consconfig_dacf_keyboard_problem = B_FALSE;
	}

	/*
	 * Increment the module reference count so that the driver
	 * can't be unloaded.
	 */
	(void) ddi_hold_installed_driver(major);

	res = DDI_SUCCESS;

done:
	mutex_exit(&consconfig_kb_attach_sync);

#if	defined(WORK_AROUND_4235048)
	running = B_FALSE;
#endif

	return (res);
}

static int
kb_unconfig(dacf_infohdl_t minor_hdl, dacf_arghdl_t arg_hdl, int flags)
{
_NOTE(ARGUNUSED(arg_hdl, flags))
	major_t		major;
	minor_t		minor;
	dev_t		dev;

	DPRINTF(DPRINT_L0, "kb_unconfig callback\n");

	major = ddi_name_to_major((char *)dacf_driver_name(minor_hdl));
	if (major == (major_t)-1) {
		DPRINTF(DPRINT_L0, "invalid major #\n");

		return (DDI_FAILURE);
	}

	minor = dacf_minor_number(minor_hdl);

	dev = makedevice(major, minor);

	mutex_enter(&consconfig_kb_attach_sync);

	/*
	 * Check if the keyboard that is being detached
	 * is the console keyboard or not
	 */
	if (kbd_avp == NULL || dev != CONSCONFIG_DEV(kbd_avp)) {
		DPRINTF(DPRINT_L0, "Unplug of non-console keyboard\n");
		mutex_exit(&consconfig_kb_attach_sync);
		return (DDI_SUCCESS);
	}

	DPRINTF(DPRINT_L0, "Unplug of console keyboard\n");

	consconfig_dacf_unconfig_current_console_keyboard();

	mutex_exit(&consconfig_kb_attach_sync);

	return (DDI_SUCCESS);
}

/*
 * This is the post-attach / pre-detach action function for the mouse.
 * This function is associated with a node type in /etc/actions.
 *
 * Arguments:
 *	dev	- device that is being attached
 *	arg	- this is the argument that was setup in /etc/actions
 *	dir	- specifies if this action function is being called as
 *		  a result of a post-attach or a pre-detach.
 */
static int
ms_config(dacf_infohdl_t minor_hdl, dacf_arghdl_t arg_hdl, int flags)
{
_NOTE(ARGUNUSED(flags))
	major_t		major;
	minor_t		minor;
	const char	*pushmod;
	dev_t		dev;
	int		res;
	int		error;
#if	defined(WORK_AROUND_4235048)
	static boolean_t running = B_FALSE;
	static kmutex_t running_mutex = { 0 };
#endif

	DPRINTF(DPRINT_L0, "ms_config callback!\n");

	pushmod = dacf_get_arg(arg_hdl, "pushmod");

	major = ddi_name_to_major((char *)dacf_driver_name(minor_hdl));
	if (major == (major_t)-1) {
		DPRINTF(DPRINT_L0, "invalid major #\n");

		return (DDI_FAILURE);
	}

	minor = dacf_minor_number(minor_hdl);

	dev = makedevice(major, minor);

	DPRINTF(DPRINT_L2, "ms_config: driver name %s\n",
		(char *)dacf_driver_name(minor_hdl));

#if	defined(WORK_AROUND_4235048)
	mutex_enter(&running_mutex);
	if (running) {
		DPRINTF(DPRINT_L2, "ms_config: recursive call, bailing out\n");
		mutex_exit(&running_mutex);
		return (DDI_SUCCESS);
	}
	running = B_TRUE;
	mutex_exit(&running_mutex);
#endif

	/*
	 * Access to the global variables is synchronized
	 */
	mutex_enter(&consconfig_ms_attach_sync);

	/*
	 * If we already have a mouse_avp then there is already a mouse
	 * plugged into the system.  Unhook it and replace it.
	 */
	if (mouse_avp != NULL) {
		DPRINTF(DPRINT_L0, "Replacing the console mouse\n");
		consconfig_dacf_unconfig_current_console_mouse();
	}

	/*
	 * Create a vnode pointer so that we can perform pushing or
	 * popping functions on the mouse device.
	 */
	mouse_avp = consconfig_dacf_createvp(dev);

	error = consconfig_dacf_openvp(mouse_avp);
	if (error) {
		res = DDI_FAILURE;
		goto done;
	}

	/*
	 * Link consms on top of the mouse device
	 *
	 * NEEDSWORK:  Ignores errors.  Is this right?
	 */
	consconfig_dacf_mouselink(mouse_avp, pushmod);

	/*
	 * Increment the module reference count so that the driver
	 * can't be unloaded.
	 */
	(void) ddi_hold_installed_driver(major);

	res = DDI_SUCCESS;

done:
	mutex_exit(&consconfig_ms_attach_sync);

#if	defined(WORK_AROUND_4235048)
	running = B_FALSE;
#endif
	return (res);

}

static int
ms_unconfig(dacf_infohdl_t minor_hdl, dacf_arghdl_t arg_hdl, int flags)
{
_NOTE(ARGUNUSED(arg_hdl, flags))
	major_t		major;
	minor_t		minor;
	dev_t		dev;

	DPRINTF(DPRINT_L0, "ms_unconfig callback!\n");

	major = ddi_name_to_major((char *)dacf_driver_name(minor_hdl));
	if (major == (major_t)-1) {
		DPRINTF(DPRINT_L0, "invalid major #\n");

		return (DDI_FAILURE);
	}

	minor = dacf_minor_number(minor_hdl);

	dev = makedevice(major, minor);

	major = getmajor(dev);

	mutex_enter(&consconfig_kb_attach_sync);

	/*
	 * Ignore unplugs of mice that are not the console mouse.
	 */
	if (mouse_avp == NULL || dev != CONSCONFIG_DEV(mouse_avp)) {

		DPRINTF(DPRINT_L0, "Unplug of non-console mouse\n");
		mutex_exit(&consconfig_kb_attach_sync);

		return (DDI_SUCCESS);
	}

	DPRINTF(DPRINT_L0, "Unplug of console mouse\n");

	consconfig_dacf_unconfig_current_console_mouse();

	mutex_exit(&consconfig_kb_attach_sync);

	return (DDI_SUCCESS);
}

/*
 * consconfig_dacf_kblink:
 * 	Link the keyboard streams driver under the keyboard mux (conskbd).
 * 	This routine is not called for the keyboard.
 */
static void
consconfig_dacf_kblink(
	consconfig_dacf_vnode_t *kbd_avp,
	const char *module_name)
{
	DPRINTF(DPRINT_L0, "consconfig_dacf_kblink: start\n");

	/*
	 * Push the module_name on top of the vnode that we have been given,
	 * and configure the module.
	 */
	(void) consconfig_dacf_kbconfig(kbd_avp, TR_CAN, module_name);

	/*
	 * Link the stream underneath conskbd
	 */
	consconfig_dacf_link_conskbd(kbd_avp, &kb_muxid);

	/*
	 * At this point, the stream is:
	 *	wc->conskbd->["module_name"->]"kbd_avp driver"
	 */

	DPRINTF(DPRINT_L0, "consconfig_dacf_kblink: end\n");
}

/*
 * consconfig_dacf_kbunlink:
 * 	Unlink the keyboard module and underlying driver from the keyboard
 * 	console stream
 */
static void
consconfig_dacf_kbunlink()
{
	DPRINTF(DPRINT_L0, "consconfig_dacf_kbunlink: start\n");

	/*
	 * Unlink the stream from underneath the wc->conskbd multiplexor.
	 */
	consconfig_dacf_unlink_conskbd(kb_muxid);

	DPRINTF(DPRINT_L0, "consconfig_dacf_kbunlink: end\n");
}

/*
 * consconfig_dacf_mouselink:
 * 	Push the module called "module_name" on top of the
 *	driver specified in the mouse_avp argument.  Link
 * 	the driver specified by the mouse_avp argument under
 *	consms.
 */
static void
consconfig_dacf_mouselink(
	consconfig_dacf_vnode_t *mouse_avp,
	const char *module_name)
{
	int		rval;

	DPRINTF(DPRINT_L0, "consconfig_dacf_mouselink: start\n");

	/*
	 * Send a flush to the mouse driver.
	 */
	(void) strioctl(CONSCONFIG_VNODE(mouse_avp), I_FLUSH, (intptr_t)FLUSHRW,
		FREAD+FNOCTTY, K_TO_K, kcred, &rval);

	if (module_name != NULL)
		consconfig_dacf_push(mouse_avp, module_name);

	/*
	 * Link the serial module underneath consms
	 */
	consconfig_dacf_link_consms(mouse_avp, &mouse_muxid);

	/*
	 * At this point, the stream is:
	 *		consms->["module_name"->]"mouse_avp driver"
	 */
}

/*
 * consconfig_dacf_mouseunlink:
 * 	Unlink the mouse module and driver from underneath
 * 	consms.
 */
static void
consconfig_dacf_mouseunlink()
{
	consconfig_dacf_vnode_t	*consmouse_avp;
	dev_t		dev;
	int		error;

	DPRINTF(DPRINT_L0, "consconfig_dacf_mouseunlink: start\n");

	/*
	 * Open the "console mouse" device, to unlink the mouse device
	 * under it.
	 */
	dev = consconfig_dacf_getdev("consms", 0);

	consmouse_avp = consconfig_dacf_createvp(dev);

	error = consconfig_dacf_openvp(consmouse_avp);
	if (error) {
		consmouse_avp = NULL;
		return;
	}

	/*
	 * Unlink the mouse stream represented by mouse_avp
	 * from consms.
	 */
	error = consconfig_dacf_unlink(consmouse_avp, mouse_muxid);
	if (error != 0) {
		cmn_err(CE_WARN,
		    "consconfig_dacf_mouseunlink:  unlink failed, error %d",
		    error);
		/*
		 * NEEDSWORK:  Just continue anyway.  Is this right?
		 */
	}

	consconfig_dacf_closevp(consmouse_avp);

	/*
	 * Completely destroy the vnode that was created above
	 */
	consconfig_dacf_destroyvp(consmouse_avp);

	DPRINTF(DPRINT_L0, "consconfig_dacf_mouseunlink: end\n");
}


/*
 * consconfig_dacf_link_conskbd:
 * 	kbd_avp represents a driver with a keyboard module pushed on top of
 *	it. The driver is linked underneath conskbd.
 */
static void
consconfig_dacf_link_conskbd(consconfig_dacf_vnode_t *kbd_avp, int *mux_id)
{
	int error;

	DPRINTF(DPRINT_L0,
	    "consconfig_dacf_link_conskbd(kbd_avp %p mux_id %p)\n",
		kbd_avp, mux_id);
	DPRINTF(DPRINT_L0,
	    "consconfig_dacf_link_conskbd:  conskbd_avp %p\n", conskbd_avp);

	/*
	 * Hold the file_t so that the unlink doesn't cause the file_t to
	 * be closed.  Closing the file_t would cause conskbd to be detached
	 * when we want it to remain in place.
	 */
	consconfig_holdvp(conskbd_avp);

	error = consconfig_dacf_getfd(conskbd_avp);
	if (error)
		return;

	error = consconfig_dacf_openvp(rwscons_avp);
	if (error)
		return;

	/*
	 * Temporarily unlink conskbd from wc so that the kbd_avp
	 * stream may be linked under conskbd.  This has to be done
	 * because streams are built bottom up and linking a stream
	 * under conskbd isn't allowed when conskbd is linked under
	 * wc.
	 */
	error = consconfig_dacf_unlink(rwscons_avp, conskbd_muxid);
	if (error != 0) {
		cmn_err(CE_WARN,
		    "consconfig_dacf_link_conskbd:  unlink failed, error %d",
		    error);
		/*
		 * NEEDSWORK:  Just continue anyway.  Is this right?
		 */
	}

	error = consconfig_dacf_getfile(kbd_avp);
	if (error)
		return;

	error = consconfig_dacf_getfd(kbd_avp);
	if (error)
		return;

	DPRINTF(DPRINT_L0, "linking keyboard under conskbd\n");

	/*
	 * Link the stream represented by kbd_avp under conskbd
	 */
	error = consconfig_dacf_link(conskbd_avp, CONSCONFIG_FD(kbd_avp),
		mux_id);
	if (error != 0) {
		cmn_err(CE_WARN,
		    "consconfig_dacf_link_conskbd:  kb link failed, error %d",
		    error);
		/*
		 * NEEDSWORK:  Just continue anyway.  Is this right?
		 */
	}

	/*
	 * Link consbkd back under wc.
	 *
	 * The act of linking conskbd back under wc will cause wc
	 * to query the lower lower layers about their polled I/O
	 * routines.  This time the request will succeed because there
	 * is a physical keyboard linked under conskbd.
	 */
	error = consconfig_dacf_link(rwscons_avp, CONSCONFIG_FD(conskbd_avp),
		&conskbd_muxid);
	if (error != 0) {
		cmn_err(CE_WARN,
	    "consconfig_dacf_link_conskbd:  conskbd link failed, error %d",
		    error);
		/*
		 * NEEDSWORK:  Just continue anyway.  Is this right?
		 */
	}

	consconfig_dacf_closevp(rwscons_avp);

	DPRINTF(DPRINT_L0, "consconfig_dacf_link_conskbd: end\n");
}

/*
 * consconfig_dacf_unlink_conskbd:
 * 	Unlink the driver with the keyboard module pushed on top from
 *	beneath conskbd.  The function will leave just wc->conskbd
 */
static void
consconfig_dacf_unlink_conskbd(int mux_id)
{
	int error;

	DPRINTF(DPRINT_L0, "consconfig_dacf_unlink_conskbd: start\n");

	/*
	 * Hold the file_t so that the unlink doesn't cause the file_t to be
	 * closed.  Closing the file_t would cause conskbd to be detached
	 * when we want it to remain in place.
	 */
	consconfig_holdvp(conskbd_avp);

	error = consconfig_dacf_getfd(conskbd_avp);
	if (error)
		return;

	error = consconfig_dacf_openvp(rwscons_avp);
	if (error)
		return;

	/*
	 * Temporarily unlink conskbd from wc so that the kbd_avp
	 * stream may be linked under conskbd.  This has to be done
	 * because streams are built bottom up and unlinking a stream
	 * under conskbd isn't allowed when conskbd is linked under
	 * wc.
	 */
	error = consconfig_dacf_unlink(rwscons_avp, conskbd_muxid);
	if (error != 0) {
		cmn_err(CE_WARN,
	"consconfig_dacf_unlink_conskbd:  unlink of conskbd failed, error %d",
		    error);
		/*
		 * NEEDSWORK:  Just continue anyway.  Is this right?
		 */
	}

	/*
	 * This will cause the keyboard driver to be closed, all modules to be
	 * popped, and the keyboard vnode released.
	 */
	error = consconfig_dacf_unlink(conskbd_avp, mux_id);
	if (error != 0) {
		cmn_err(CE_WARN,
	"consconfig_dacf_unlink_conskbd:  unlink of kb failed, error %d",
		    error);
		/*
		 * NEEDSWORK:  Just continue anyway.  Is this right?
		 */
	}

	/*
	 * Link consbkd back under wc
	 *
	 * NEEDSWORK:  Ignore error.  Is this right?
	 */
	error = consconfig_dacf_link(rwscons_avp, CONSCONFIG_FD(conskbd_avp),
		&conskbd_muxid);
	if (error != 0) {
		cmn_err(CE_WARN,
	    "consconfig_dacf_unlink_conskbd:  conskbd link failed, error %d",
		    error);
		/*
		 * NEEDSWORK:  Just continue anyway.  Is this right?
		 */
	}

	consconfig_dacf_closevp(rwscons_avp);

	DPRINTF(DPRINT_L0, "consconfig_dacf_unlink_conskbd: end\n");
}


/*
 * consconfig_dacf_link_consms:
 *	mouse_avp is a driver with a mouse module pushed on top.
 *	The driver is linked underneath consms
 */
static void
consconfig_dacf_link_consms(consconfig_dacf_vnode_t *mouse_avp, int *mux_id)
{
	consconfig_dacf_vnode_t	*consmouse_avp;
	dev_t		dev;
	int		error;

	DPRINTF(DPRINT_L0, "consconfig_dacf_link_cons: start\n");

	/*
	 * Open the "console mouse" device, and link the mouse device
	 * under it. The mouse_avp stream will look like ms->"serial-driver"
	 * or usbms->hid at this point.
	 */
	dev = consconfig_dacf_getdev("consms", 0);

	/*
	 * We will open up a new stream for the consms device and link our
	 * ms->"serial-driver" stream underneath it.  consms is a multiplexor.
	 */
	consmouse_avp = consconfig_dacf_createvp(dev);

	error = consconfig_dacf_openvp(consmouse_avp);
	if (error)
		return;

	error = consconfig_dacf_getfile(mouse_avp);
	if (error)
		return;

	error = consconfig_dacf_getfd(mouse_avp);
	if (error)
		return;

	/*
	 * Link ms/usbms stream underneath consms multiplexor.
	 *
	 * NEEDSWORK:  Ignore error.  Is this right?
	 */
	error = consconfig_dacf_link(consmouse_avp, CONSCONFIG_FD(mouse_avp),
		mux_id);
	if (error != 0) {
		cmn_err(CE_WARN,
	    "consconfig_dacf_link_consms:  mouse link failed, error %d",
		    error);
		/*
		 * NEEDSWORK:  Just continue anyway.  Is this right?
		 */
	}

	/*
	 * Increment the module reference count so that the driver can't
	 * be unloaded.
	 */
	(void) ddi_hold_installed_driver(getmajor(dev));

	consconfig_dacf_closevp(consmouse_avp);

	/*
	 * Completely destroy the vnode that was created above.
	 */
	consconfig_dacf_destroyvp(consmouse_avp);

	DPRINTF(DPRINT_L0, "consconfig_dacf_link_cons: end\n");
}

/*
 * consconfig_dacf_kbconfig:
 *	Push "module_name" on top of the driver represented by avp
 * 	issue the KIOCTRANSABLE ioctl
 */
static int
consconfig_dacf_kbconfig(
	consconfig_dacf_vnode_t *avp,
	int kbdtranslatable,
	const char *module_name)
{
	int error, rval;

	/*
	 * Send a flush down the stream to the keyboard.
	 */
	(void) strioctl(CONSCONFIG_VNODE(avp), I_FLUSH, (intptr_t)FLUSHRW,
		FREAD+FNOCTTY, K_TO_K, kcred, &rval);

	if (module_name != NULL)
		consconfig_dacf_push(avp, module_name);

	error = strioctl(CONSCONFIG_VNODE(avp), KIOCTRANSABLE,
		(intptr_t)&kbdtranslatable, FREAD+FNOCTTY, K_TO_K, kcred,
		&rval);

	if (error) {
		cmn_err(CE_WARN,
		"consconfig_dacf_kbconfig: KIOCTRANSABLE failed error: %d",
			error);
	}

	/*
	 * During boot, dynamic_console_config() will call the
	 * function to enable abort on the console.  If the
	 * keyboard is hotplugged after boot, check to see if
	 * the keyboard is the console input.  If it is
	 * enable abort on it.
	 */
	if (consconfig_info.consconfig_dacf_console_input_type
			== CONSOLE_INPUT_KEYBOARD) {

		(void) consconfig_dacf_abort(avp);
	}

	return (0);
}

/*
 * consconfig_get_polledio:
 * 	Query the console with the CONSPOLLEDIO ioctl.
 * 	The polled I/O routines are used by debuggers to perform I/O while
 * 	interrupts and normal kernel services are disabled.
 */
static cons_polledio_t *
consconfig_get_polledio(consconfig_dacf_vnode_t *avp)
{
	int		error;
	int		flag;
	int		rval;
	struct strioctl	strioc;
	cons_polledio_t	*polled_io;

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
	error = VOP_IOCTL(CONSCONFIG_VNODE(avp), I_STR, (intptr_t)&strioc,
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
 * consconfig_dacf_abort:
 * 	Send the CONSSETABORTENABLE ioctl to the lower layers.  This ioctl
 * 	will only be sent to the device if it is the console device.
 * 	This ioctl tells the device to pay attention to abort sequences.
 * 	In the case of kbtrans, this would tell the driver to pay attention
 * 	to the two key abort sequences like STOP-A.  In the case of the
 * 	serial keyboard, it would be an abort sequence like a break.
 */
static int
consconfig_dacf_abort(consconfig_dacf_vnode_t *avp)
{
	int flag;
	int rval;
	int error;

	DPRINTF(DPRINT_L0, "consconfig_dacf_abort\n");

	error = consconfig_dacf_openvp(avp);
	if (error)
		return (error);

	flag = FREAD+FWRITE+FNOCTTY;

	error = VOP_IOCTL(CONSCONFIG_VNODE(avp), CONSSETABORTENABLE,
		(intptr_t)B_TRUE, (flag|FKIOCTL), kcred, &rval);

	consconfig_dacf_closevp(avp);

	return (error);
}

/*
 * consconfig_dacf_setup_polledio:
 * 	This routine does the setup work for polled I/O.  First we get
 * 	the polled_io structure from the lower layers
 * 	and then we register the polled I/O
 * 	callbacks with the debugger that will be using them.
 */
static void
consconfig_dacf_setup_polledio(consconfig_dacf_vnode_t *avp)
{
	cons_polledio_t	*polled_io;
	int error;

	DPRINTF(DPRINT_L0, "consconfig_dacf_setup_polledio: start\n");

	error = consconfig_dacf_openvp(avp);
	if (error)
		return;

	/*
	 * Get the polled io routines so that we can use this
	 * device with the debuggers.
	 */
	polled_io = consconfig_get_polledio(avp);

	/*
	 * If the get polledio failed, then we do not want to throw
	 * the polled I/O switch.
	 */
	if (polled_io == NULL) {

		DPRINTF(DPRINT_L0,
		"consconfig_dacf_setup_polledio: get_polledio failed\n");

		consconfig_dacf_closevp(avp);

		return;
	}

	/*
	 * Initialize the polled input.
	 */
	polled_io_init();

	DPRINTF(DPRINT_L0,
	"consconfig_dacf_setup_polledio: registering callbacks\n");

	/*
	 * Register the callbacks.
	 */
	(void) polled_io_register_callbacks(polled_io, 0);

	consconfig_dacf_closevp(avp);

	DPRINTF(DPRINT_L0, "consconfig_dacf_setup_polledio: end\n");
}

/*
 * consconfig_dacf_getdev:
 * 	Return the dev_t given the name of the driver and the
 * 	minor number
 */
dev_t
consconfig_dacf_getdev(char *driver_name, int minor)
{
	int		major;
	dev_t		dev;

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
 * consconfig_dacf_createvp:
 *	This routine is a convenience routine that is passed a dev and returns
 *	a vnode.
 */
static consconfig_dacf_vnode_t *
consconfig_dacf_createvp(dev_t dev)
{
	consconfig_dacf_vnode_t	*avp;

	avp = (consconfig_dacf_vnode_t *)kmem_alloc(
			sizeof (consconfig_dacf_vnode_t),
			KM_SLEEP);

	/*
	 * makespecvp is kind of a back door routine that returns a vnode
	 * that can be used for VOP_OPEN.  makespecvp calls into the specfs
	 * driver to get the vnode.  Normally filesystem drivers will return
	 * vnodes through VOP_LOOKUP, or VFS_ROOT, but the specfs filesystem
	 * does not have either of these routines.
	 */
	CONSCONFIG_VNODE(avp) = makespecvp(dev, VCHR);

	if (CONSCONFIG_VNODE(avp) == NULL) {
		cmn_err(CE_WARN,
		    "consconfig: consconfig_dacf_createvp failed: dev 0x%x",
			(int)dev);
		kmem_free((void *)avp, sizeof (*avp));
		return (NULL);
	}

	CONSCONFIG_DEV(avp) = dev;

	DPRINTF(DPRINT_L0, "opening vnode = 0x%llx - dev 0x%llx\n",
		CONSCONFIG_VNODE(avp), dev);

	return (avp);
}


/*
 * consconfig_dacf_openvp:
 *
 *	This routine is a convenience routine that is passed a vnode and
 *	opens it.
 */
static int
consconfig_dacf_openvp(consconfig_dacf_vnode_t *avp)
{
	int		error;

	/*
	 * open the vnode.  This routine is going to call spec_open() to open
	 * up the vnode that was initialized by makespecvp.
	 */
	error = VOP_OPEN(&CONSCONFIG_VNODE(avp), FREAD+FWRITE, kcred);

	if (error) {
		cmn_err(CE_WARN,
		"consconfig: consconfig_dacf_openvp failed: err %d vnode 0x%x",
			error, (int)CONSCONFIG_VNODE(avp));
	}
	return (error);
}

/*
 * consconfig_dacf_getfd:
 *	This routine gets a free file descriptor and associates it with
 *	a file pointer that we have already allocated.  The file descriptors
 * 	exist in the context of the calling thread.  This routine is called
 *	any time that we are going to I_PLINK a module under another module.
 *	We closeandsetf() in consconfig_dacf_link(), and that will free the fd
 * 	that is allocated here.
 */
static int
consconfig_dacf_getfd(consconfig_dacf_vnode_t *avp)
{
	/*
	 * Allocated a file descriptor that is greater than or equal to 0.
	 */
	if ((CONSCONFIG_FD(avp) = ufalloc(0)) == -1) {
		cmn_err(CE_WARN, "consconfig: can't get fd");
		return (EMFILE);
	}

	/*
	 * Associate the file pointer with the new file descriptor.  The
	 * strioctl() routine uses a file descriptor for link operations.
	 */
	setf(CONSCONFIG_FD(avp), CONSCONFIG_FILE(avp));
	return (0);
}

/*
 * consconfig_dacf_getfile:
 *
 *	This routine gets a file pointer that will be associated with the
 *	vnode that is passed in.  This routine has to be called once before
 *	we can call consconfig_dacf_getfd().
 *	Once we have a valid file_t, we can
 *	call consconfig_dacf_getfd() and consconfig_dacf_link() on it.
 *	The file_t exists
 *	outside of the context of the calling thread, so it can be used
 *	in another thread context.
 */
static int
consconfig_dacf_getfile(consconfig_dacf_vnode_t *avp)
{
	int		error;

	/*
	 * Allocate a file_t and associate it with avp.
	 */
	error = falloc(CONSCONFIG_VNODE(avp), FREAD+FWRITE+FNOCTTY,
		&CONSCONFIG_FILE(avp), NULL);

	DPRINTF(DPRINT_L0, "falloc vnode = 0x%llx - fp = 0x%llx\n",
		CONSCONFIG_VNODE(avp), CONSCONFIG_FILE(avp));

	if (error) {
		cmn_err(CE_WARN, "consconfig: can't get fp : error %d",
			error);
		return (error);
	}

	/*
	 * single theaded - no close will occur here
	 */
	mutex_exit(&CONSCONFIG_FILE(avp)->f_tlock);

	return (0);
}

/*
 * consconfig_holdvp:
 *
 *	This routine keeps the vnode from being destroyed by bumping the
 *	f_count on the file that this vnode is associated with.  When we
 *	do an I_PLINK, mlink() will increment the f_count and closef() the
 *	file_t.  We then close the file_t with closeandsetf(), and the
 *	the f_count gets decremented back to 1.  If do not increment the
 *	f_count again in this routine before doing a I_PUNLINK, munlink()
 *	will decrement the f_count and call closef().  This will cause
 *	our stream to be torn down, which is not what we want.
 */
static void
consconfig_holdvp(consconfig_dacf_vnode_t *avp)
{
	mutex_enter(&CONSCONFIG_FILE(avp)->f_tlock);

	CONSCONFIG_FILE(avp)->f_count++;

	mutex_exit(&CONSCONFIG_FILE(avp)->f_tlock);
}

/*
 * consconfig_dacf_closevp:
 *
 *	This routine is a convenience routine that is passed in a vnode.
 */
static void
consconfig_dacf_closevp(consconfig_dacf_vnode_t *avp)
{
	/*
	 * This routine closes the vnode, but does not get rid of it.  This
	 * routine calls spec_close() to do the dirty work.
	 */
	VOP_CLOSE(CONSCONFIG_VNODE(avp), FREAD+FWRITE, 1, (offset_t)0, kcred);
}

/*
 * consconfig_dacf_destroyvp:
 *
 *	This routine is a convenience routine that releases a vnode that has
 *	been allocated by makespecvp()
 */
static void
consconfig_dacf_destroyvp(consconfig_dacf_vnode_t *avp)
{
	/*
	 * Release the vnode.  VOP_CLOSE() calls the close routine, but does not
	 * get rid of the vnode.  VN_RELE() will call through vn_rele()->
	 * VOP_INACTIVE()->spec_inactive() to release the vnode.  Normally
	 * this happens on a closef, but we need to call this routine if
	 * we have allocated a vnode, but have not allocated a file_t to go
	 * with it.
	 */
	VN_RELE(CONSCONFIG_VNODE(avp));

	kmem_free(avp, sizeof (consconfig_dacf_vnode_t));
}

/*
 * consconfig_dacf_link:
 *
 *	This routine is a convenience routine that links a streams module
 * 	underneath a multiplexor.
 */
static int
consconfig_dacf_link(consconfig_dacf_vnode_t *avp, int fd, int *muxid)
{
	int	error;

	/*
	 * Link the device specified by "fd" under the multiplexor specified by
	 * "vp".  strioctl() will call mlink() to do most of the work.  mlink()
	 * looks up the file_t corresponding to "fd" and stores it in the "stp"
	 * that is pointed to by "vp".  mlink() will increment the f_count on
	 * the file_t.
	 */
	if (error = strioctl(CONSCONFIG_VNODE(avp), I_PLINK, (intptr_t)fd,
		FREAD+FWRITE+FNOCTTY, K_TO_K, kcred, muxid)) {

		cmn_err(CE_WARN,
		"consconfig: consconfig_dacf_link I_PLINK failed: error %d",
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
 * consconfig_dacf_unlink:
 *
 *	This routine is a convenience routine that unlinks a streams module
 * 	from underneath a multiplexor.
 */
static int
consconfig_dacf_unlink(consconfig_dacf_vnode_t *avp, int mux_id)
{
	int	error;
	int	rval;

	/*
	 * Unlink the driver specified by the "mux_id" from underneath the vnode
	 * specified by "vp".  strioctl() will call munlink() to do most of the
	 * dirty work.  Because we did a closeandsetf() above, the f_count on
	 * the file_t that is saved in the "linkp" will be 1.  munlink() calls
	 * closef() on the file_t, causing VOP_CLOSE() an VN_RELE() to be
	 * called on the vnode corresponding to "mux_id".  The closef() will
	 * cause any modules that have been pushed on top of the vnode
	 * corresponding to the "mux_id" to be * popped.
	 */
	error = strioctl(CONSCONFIG_VNODE(avp), I_PUNLINK, (intptr_t)mux_id, 0,
		K_TO_K, CRED(), &rval);

	if (error) {
		cmn_err(CE_WARN, "consconfig: unlink failed: error %d", error);
	}
	return (error);
}

/*
 * consconfig_dacf_push:
 *
 *	This is a convenience routine that pushes a "modulename" on
 * 	the driver representned by avp.
 */
static void
consconfig_dacf_push(consconfig_dacf_vnode_t *avp, const char *module_name)
{
	int	error;
	int	rval;

	/*
	 * Push the module named module_name on top of the driver corresponding
	 *  to "vp".
	 */
	error = strioctl(CONSCONFIG_VNODE(avp), I_PUSH, (intptr_t)module_name,
		FREAD+FNOCTTY, K_TO_K, kcred, &rval);

	if (error) {
		cmn_err(CE_WARN,
			"consconfig: can't push line discipline: error %d",
			    error);
	}
}

/*
 * consconfig_dacf_print_paths:
 *	Function to print out the various paths
 */
static void
consconfig_dacf_print_paths(void)
{
	char    *path;

	if (consconfig_dacf_errlevel > DPRINT_L0) {

		return;
	}

	path = usb_kb_path;
	if (path != NULL)
		DPRINTF(DPRINT_L0, "usb keyboard path = %s\n", path);

	path = i_kbdpath();
	if (path != NULL)
		DPRINTF(DPRINT_L0, "keyboard path = %s\n", path);

	path = usb_ms_path;
	if (path != NULL)
		DPRINTF(DPRINT_L0, "usb mouse path = %s\n", path);

	path = i_mousepath();
	if (path != NULL)
		DPRINTF(DPRINT_L0, "mouse path = %s\n", path);

	path = i_stdinpath();
	if (path != NULL)
		DPRINTF(DPRINT_L0, "stdin path = %s\n", path);

	path = i_stdoutpath();
	if (path != NULL)
		DPRINTF(DPRINT_L0, "stdout path = %s\n", path);

	path = i_fbpath();
	if (path != NULL)
		DPRINTF(DPRINT_L0, "fb path = %s\n", path);
}

static void
consconfig_dacf_dprintf(int l, const char *fmt, ...)
{
	va_list ap;

#ifndef DEBUG
	if (!l) {
		return;
	}
#endif
	if ((l) < consconfig_dacf_errlevel) {

		return;
	}

	va_start(ap, fmt);
	(void) vprintf(fmt, ap);
	va_end(ap);
}

static int
consconfig_setup_wc_output(consconfig_dacf_vnode_t *avp, char *outpath)
{
	int error;
	int rval;
	struct strioctl strioc;
	int flag;

	flag = FREAD+FWRITE+FNOCTTY;

	strioc.ic_cmd = WC_OPEN_FB;
	strioc.ic_timout = INFTIM;
	strioc.ic_len = strlen(outpath)+1;
	strioc.ic_dp = outpath;
	error = VOP_IOCTL(CONSCONFIG_VNODE(avp), I_STR, (intptr_t)&strioc,
		(flag|FKIOCTL), kcred, &rval);
	if (error != 0) {
		cmn_err(CE_WARN,
		    "Could not attach frame buffer to wscons, error %d",
		    error);
	}
	return (error);
}

static void
consconfig_find_fb(char **ppath, dev_t *pdev)
{
	*ppath = i_fbpath();
	if (*ppath == NULL) {
		cmn_err(CE_WARN, "consconfig:  no screen found");
		*pdev = NODEV;
	} else {
		*pdev = ddi_pathname_to_dev_t(*ppath);
		if (*pdev == NODEV) {
			cmn_err(CE_WARN,
		"consconfig:  could not find driver for screen device %s",
				*ppath);
		}
	}
}

static consconfig_dacf_vnode_t	*
consconfig_open_conskbd(void)
{
	dev_t dev;
	int error;
	consconfig_dacf_vnode_t	*avp;

	DPRINTF(DPRINT_L0, "consconfig_open_conskbd()\n");

	dev = consconfig_dacf_getdev("conskbd", 1);
	if (dev == NODEV)
		return (NULL);

	avp = consconfig_dacf_createvp(dev);
	if (avp == NULL)
		return (NULL);

	error = consconfig_dacf_openvp(avp);
	if (error)
		return (NULL);

	error = consconfig_dacf_getfile(avp);
	if (error)
		return (NULL);

	error = consconfig_dacf_getfd(avp);
	if (error)
		return (NULL);

	DPRINTF(DPRINT_L0, "consconfig_open_conskbd returns %p\n", avp);

	return (avp);
}

static void
consconfig_setup_wc(
    consconfig_dacf_vnode_t	*in_avp,
    char			*out_path,
    dev_t			*pdev,
    consconfig_dacf_vnode_t	**pavp,
    int				*pmuxid)
{
	consconfig_dacf_vnode_t	*avp;
	dev_t			dev;
	int			muxid;
	int			error;

	*pavp = NULL;
	*pdev = NODEV;
	*pmuxid = 0;

	dev = consconfig_dacf_getdev("wc", 0);
	if (dev == NODEV)
		return;

	avp = consconfig_dacf_createvp(dev);
	if (avp == NULL)
		return;

	error = consconfig_dacf_openvp(avp);
	if (error)
		return;

	if (in_avp != NULL) {
		/*
		 * Link conskbd under wc.
		 * consconfig_dacf_link() calls
		 * closeandsetf() which frees the
		 * CONSCONFIG_FD(conskbd_avp) that was
		 * allocated above.

		 * The act of linking conskbd under wc
		 * will cause wc to query the lower
		 * layers about their polled I/O
		 * routines using CONSOPENPOLLEDIO.  This
		 * will fail on this link because there
		 * is not a physical keyboard linked
		 * under conskbd.
		 */
		error = consconfig_dacf_link(avp, CONSCONFIG_FD(in_avp),
			&muxid);
		if (error != 0) {
			cmn_err(CE_WARN,
		    "consconfig_setup_wc:  in_avp link failed, error %d",
			    error);
			/*
			 * NEEDSWORK:  Just continue anyway.  Is this right?
			 */
		}
	}

	if (out_path != NULL) {
		(void) consconfig_setup_wc_output(avp, out_path);
	}

	/*
	 * now we must close it to make console logins happy
	 */
	(void) ddi_hold_installed_driver(getmajor(dev));

	consconfig_dacf_closevp(avp);

	/*
	 * The rwscons_avp is not destroyed at this
	 * point so that we can re-use it later.
	 * Although we have closed the vnode, the vnode
	 * is still valid.
	 */
	*pavp = avp;
	*pdev = dev;
	*pmuxid = muxid;
}

dev_t
consconfig_pathname_to_dev_t(char *name, char *path)
{
	dev_t ret;
	int			retry;

	if (path == NULL) {
		DPRINTF(DPRINT_L2, "consconfig_dacf:  no %s path\n", name);
		return (NODEV);
	}

	DPRINTF(DPRINT_L0,
		"consconfig_dacf:  trying to find %s %s...\n",
		name, path);

	for (retry = 0; retry < consconfig_retries; retry++) {
		uint_t lkcnt;
		i_ndi_block_device_tree_changes(&lkcnt);
		ret = ddi_pathname_to_dev_t(path);
		i_ndi_allow_device_tree_changes(lkcnt);
		if (ret != NODEV) {
			DPRINTF(DPRINT_L0, "%s %d,%d\n", name,
				getmajor(ret), getminor(ret));
			return (ret);
		} else {
			delay(drv_usectohz(1000000));
		}
	}

	DPRINTF(DPRINT_L2, "%s is NODEV\n", name);
	return (NODEV);
}

static void
consconfig_dacf_unconfig_current_console_keyboard(void)
{
	/*
	 * Un-do everything that we did in consconfig_dacf_kblink above.
	 */
	consconfig_dacf_kbunlink();

	/*
	 * We are no longer using this driver, so allow it to be
	 * unloaded.
	 */
	ddi_rele_driver(getmajor(CONSCONFIG_DEV(kbd_avp)));

	kbd_avp = NULL;
}

static void
consconfig_dacf_unconfig_current_console_mouse(void)
{
	/*
	 * Tear down the mouse stream
	 */
	consconfig_dacf_mouseunlink();

	/*
	 * We are no longer using this driver, so allow it to be
	 * unloaded.
	 */
	ddi_rele_driver(getmajor(CONSCONFIG_DEV(mouse_avp)));

	mouse_avp = NULL;
}
