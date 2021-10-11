/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consconfig_dacf.c	1.12	99/10/22 SMI"

/*
 * This is a dacf module based upon the Extensions to Device Autoconfiguration
 * project.  See PSARC/1998/212 for more details.
 *
 * This module provides the dacf functions
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
 * mice. See PSARC/1998/176 for the issues with supporting multiple keyboards
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
#include <sys/clock.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/modctl.h>
#include <sys/ddi_impldefs.h>

#include <sys/errno.h>
#include <sys/devops.h>
#include <sys/note.h>

#include <sys/polled_io.h>
#include <sys/kmem.h>
#include <sys/consconfig.h>
#include <sys/dacf.h>

/*
 * Dacf entry points
 */
static int	kb_config(dacf_infohdl_t, dacf_arghdl_t, int);
static int	kb_unconfig(dacf_infohdl_t, dacf_arghdl_t, int);
static int	ms_config(dacf_infohdl_t, dacf_arghdl_t, int);
static int	ms_unconfig(dacf_infohdl_t, dacf_arghdl_t, int);

/*
 * External functions
 */
extern major_t	ddi_name_to_major(char *name);
extern void	printf(const char *fmt, ...);
extern uintptr_t space_fetch(char *key);

/*
 * Utility functions from consconfig_util.o
 */
extern int consconfig_util_link(consconfig_vnode_t *avp, int fd,
				int *muxid);
extern void consconfig_util_closevp(consconfig_vnode_t *avp);
extern void consconfig_util_link_wc(cons_state_t *sp,
					consconfig_vnode_t *avp);
extern void consconfig_util_setup_polledio(consconfig_vnode_t *avp);
extern void consconfig_dprintf(int l, const char *fmt, ...);
extern int  consconfig_util_unlink(consconfig_vnode_t *avp, int mux_id);
extern void consconfig_util_kbconfig(cons_state_t *sp,
					consconfig_vnode_t *avp,
					int kbdtranslatable,
					char *module_name);
extern consconfig_vnode_t *consconfig_util_createvp(dev_t dev);
extern void consconfig_util_freevp(consconfig_vnode_t *avp);
extern dev_t consconfig_util_getdev(char *driver_name, int minor);
extern int consconfig_util_abort(consconfig_vnode_t *vp);
extern int consconfig_util_openvp(consconfig_vnode_t *avp);
extern int consconfig_util_getfd(consconfig_vnode_t *avp);
extern int consconfig_util_getfile(consconfig_vnode_t *avp);
extern int consconfig_util_push(consconfig_vnode_t *avp,
					char *module_name);
/*
 * Internal functions
 */
static void consconfig_dacf_mouselink(cons_state_t *sp,
					consconfig_vnode_t *mouse_avp,
					char *module_name);
static void consconfig_dacf_mouseunlink(cons_state_t *sp);
static void consconfig_dacf_kblink(cons_state_t *sp,
					consconfig_vnode_t *kbd_avp,
					char *module_name);
static void consconfig_dacf_link_conskbd(cons_state_t *sp,
					consconfig_vnode_t *kbd_avp);
static void consconfig_dacf_unlink_conskbd(cons_state_t *sp);
static void consconfig_dacf_link_consms(cons_state_t *sp,
					consconfig_vnode_t *mouse_avp);
static void consconfig_dacf_destroyvp(consconfig_vnode_t *avp);
static void consconfig_holdvp(consconfig_vnode_t *avp);

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
	"Consconfig DACF 1.12",
	&dacfsw
};

struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldacf, NULL
};

int
_init(void)
{


	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * This is the post-attach / pre-detach action function for the keyboard.
 * This function is associated with a node type in /etc/dacf.conf.
 */
/*ARGSUSED*/
static int
kb_config(dacf_infohdl_t info_hdl, dacf_arghdl_t arg_hdl, int flags)
{
	major_t				major;
	minor_t				minor;
	const char			*pushmod;
	dev_t				dev;
	char				*kb_path;
	int				res;
	consconfig_vnode_t		*kbd_avp;
	cons_state_t			*sp;

	consconfig_dprintf(DPRINT_L0, "kb_config callback\n");

	/* Retrieve the state information */
	sp = (cons_state_t *)space_fetch("consconfig");

	/*
	 * Some platforms may use the old-style "consconfig" to configure
	 * console stream modules but may also support devices that happen
	 * to match a rule in /etc/dacf.conf.  This will cause a problem
	 * since the console state structure will not be initialized.
	 * In that case, these entry points should silently succeed.
	 */
	if (sp == NULL) {
		return (DACF_SUCCESS);
	}

	pushmod = dacf_get_arg(arg_hdl, "pushmod");

	/*
	 * For now, if no argument is supplied to push on top of the
	 * the node, return an error.
	 */
	if (pushmod == NULL) {

		consconfig_dprintf(DPRINT_L0, "No module to push\n");

		return (DACF_FAILURE);
	}

	major = ddi_name_to_major((char *)dacf_driver_name(info_hdl));
	if (major == (major_t)-1) {
		consconfig_dprintf(DPRINT_L0, "invalid major #\n");

		return (DACF_FAILURE);
	}

	minor = dacf_minor_number(info_hdl);
	dev = makedevice(major, minor);

	if (dev == NODEV) {

		return (DACF_FAILURE);
	}

	consconfig_dprintf(DPRINT_L0, "dev = 0x%lx\n", dev);
	consconfig_dprintf(DPRINT_L0, "major 0x%lx\n", major);
	consconfig_dprintf(DPRINT_L2, "kb_config: driver name %s\n",
			(char *)dacf_driver_name(info_hdl));

	/* Access to the global variables is synchronized */
	mutex_enter(&sp->cons_kb_mutex);

	/*
	 * If we already have a keyboard major number assigned, then a
	 * keyboard is already plugged into the system.
	 */
	if (sp->cons_kb_major != 0) {

		consconfig_dprintf(DPRINT_L3,
				"Multiple keyboards not supported.\n");

		mutex_exit(&sp->cons_kb_mutex);

		return (DACF_SUCCESS);
	}

	/*
	 * If ddi_pathname_to_dev_t is in the process of being called on
	 * the keyboard device alias, then make sure this dev_t corresponds
	 * to the appropriate pathname.
	 */
	if (sp->cons_dacf_booting == CONSCONFIG_BOOTING) {

		kb_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

		(void) ddi_pathname(dacf_devinfo_node(info_hdl), kb_path);

		consconfig_dprintf(DPRINT_L2, "ddi kb path %s\n", kb_path);
		consconfig_dprintf(DPRINT_L2, "obp kb path %s\n",
			sp->cons_keyboard_path);

		res = strcmp(sp->cons_keyboard_path,
					    kb_path);

		kmem_free(kb_path, MAXPATHLEN);

		if (res != 0) {

			consconfig_dprintf(DPRINT_L2,
				"Not the console keyboard\n");

			/* This is not the console keyboard */
			mutex_exit(&sp->cons_kb_mutex);

			return (DACF_SUCCESS);
		} else {
			consconfig_dprintf(DPRINT_L2,
				"Found the console keyboard\n");
		}
	}

	/*
	 * Keep track of the major and minor number of console keyboard
	 */
	sp->cons_kb_major = major;
	sp->cons_kb_minor = minor;

	/*
	 * The mutex must be released over calls to the file system.
	 * Only one node is allowed to be the console keyboard and this
	 * is enforced by the cons_kb_major and cons_kb_minor fields.
	 * So, only one thread will reach this point at a time.
	 */
	mutex_exit(&sp->cons_kb_mutex);

	/*
	 * Create a vnode pointer so that we can perform pushing or
	 * popping funtions on the keyboard device.
	 */
	kbd_avp = consconfig_util_createvp(dev);

	if (consconfig_util_openvp(kbd_avp) != 0) {
		/*
		 * The keyboard node failed to open.
		 * Set the major and minor numbers to 0 so
		 * kb_unconfig won't unconfigure this node if it
		 * is detached.
		 */
		mutex_enter(&sp->cons_kb_mutex);
		sp->cons_kb_major = 0;
		sp->cons_kb_minor = 0;
		mutex_exit(&sp->cons_kb_mutex);
		consconfig_util_freevp(kbd_avp);

		return (DACF_SUCCESS);
	}

	/* create the keyboard stream. */
	consconfig_dacf_kblink(sp, kbd_avp, (char *)pushmod);

	/*
	 * See if there was a problem with the console keyboard during boot. If
	 * so, try to register polled input for this keyboard.
	 */
	if (sp->cons_keyboard_problem == CONSCONFIG_KB_PROBLEM) {

		consconfig_util_setup_polledio(sp->cons_final_avp);
		sp->cons_keyboard_problem = 0;
	}

	/*
	 * Increment the module reference count so that the driver
	 * can't be unloaded.
	 */
	(void) ddi_hold_installed_driver(major);

	consconfig_util_freevp(kbd_avp);

	return (DACF_SUCCESS);
}

/*ARGSUSED*/
static int
kb_unconfig(dacf_infohdl_t info_hdl, dacf_arghdl_t arg_hdl, int flags)
{
	major_t				major;
	minor_t				minor;
	cons_state_t			*sp;

	consconfig_dprintf(DPRINT_L0, "kb_unconfig callback\n");

	/* Retrieve the state information */
	sp = (cons_state_t *)space_fetch("consconfig");

	/*
	 * So if there isn't a state available, then this entry point just
	 * returns.  See note in kb_config().
	 */
	if (sp == NULL)
		return (DACF_SUCCESS);

	major = ddi_name_to_major((char *)dacf_driver_name(info_hdl));
	if (major == (major_t)-1) {
		consconfig_dprintf(DPRINT_L0, "invalid major #\n");

		return (DACF_FAILURE);
	}

	minor = dacf_minor_number(info_hdl);

	/*
	 * Check if the keyboard that is being detached
	 * is the console keyboard or not
	 */
	mutex_enter(&sp->cons_kb_mutex);
	if ((major != sp->cons_kb_major) || (minor != sp->cons_kb_minor)) {

		consconfig_dprintf(DPRINT_L0,
		    "Unplug of non-console keyboard\n");

		mutex_exit(&sp->cons_kb_mutex);

		return (DACF_SUCCESS);
	}

	/*
	 * Release the mutex over calls into the file system.
	 * It is safe to release the mutex because the cons_kb_major
	 * and cons_kb_minor fields are still set.  So, it won't
	 * be the case that kb_config will be linking a new node
	 * into the console stream while kb_unconfig
	 * is unlinking a node.
	 */
	mutex_exit(&sp->cons_kb_mutex);

	consconfig_dprintf(DPRINT_L0, "Unplug of console keyboard\n");

	/* Unlink the node from conskbd */
	consconfig_dacf_unlink_conskbd(sp);

	/*
	 * We are no longer using this driver, so allow it to be
	 * unloaded.
	 */
	ddi_rele_driver(major);

	/*
	 * Reset sp->cons_kb_major and sp->cons_kb_minor
	 * A new keyboard node may now be linked in.
	 */
	mutex_enter(&sp->cons_kb_mutex);
	sp->cons_kb_major = 0;
	sp->cons_kb_minor = 0;
	mutex_exit(&sp->cons_kb_mutex);

	return (DACF_SUCCESS);
}

/*
 * This is the post-attach action function for the mouse.
 * This function is associated with a node type in /etc/dacf.conf.
 */
/*ARGSUSED*/
static int
ms_config(dacf_infohdl_t info_hdl, dacf_arghdl_t arg_hdl, int flags)
{
	major_t				major;
	minor_t				minor;
	const char			*pushmod;
	dev_t				dev;
	char				*ms_path;
	int				res;
	char				*p, *q;
	consconfig_vnode_t		*mouse_avp;
	cons_state_t			*sp;

	consconfig_dprintf(DPRINT_L0, "ms_config callback!\n");

	/* Retrieve the state information */
	sp = (cons_state_t *)space_fetch("consconfig");

	/*
	 * So if there isn't a state available, then this entry point just
	 * returns.  See note in kb_config().
	 */
	if (sp == NULL)
		return (DACF_SUCCESS);

	pushmod = dacf_get_arg(arg_hdl, "pushmod");

	if (pushmod == NULL) {

		consconfig_dprintf(DPRINT_L0, "No module to push\n");

		return (DACF_FAILURE);
	}

	major = ddi_name_to_major((char *)dacf_driver_name(info_hdl));
	if (major == (major_t)-1) {
		consconfig_dprintf(DPRINT_L0, "invalid major #\n");

		return (DACF_FAILURE);
	}

	minor = dacf_minor_number(info_hdl);
	dev = makedevice(major, minor);

	/* if the dev_t is NODEV, then return failure */
	if (dev == NODEV) {

		return (DACF_FAILURE);
	}

	consconfig_dprintf(DPRINT_L2, "ms_config: driver name %s\n",
		(char *)dacf_driver_name(info_hdl));

	/* Access to the global variables is synchronized  */
	mutex_enter(&sp->cons_ms_mutex);

	/*
	 * If we already have a mouse major number then there is already a mouse
	 * plugged into the system.
	 */
	if (sp->cons_ms_major != 0) {

		consconfig_dprintf(DPRINT_L3, "Multiple mice not supported.\n");

		mutex_exit(&sp->cons_ms_mutex);

		return (DACF_SUCCESS);
	}

	if (sp->cons_dacf_booting == CONSCONFIG_BOOTING) {

		ms_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

		(void) ddi_pathname(dacf_devinfo_node(info_hdl), ms_path);

		/*
		 * The following code strips off the minor name
		 * from the obp pathname.
		 *
		 * sun4u/sbus machines do not have a mouse alias, so
		 * consplat.c takes the keyboard alias and appends a :b
		 * to it.  This is done so that the ddi_pathname_to_dev_t
		 * will return a dev_t on the mouse minor node.
		 * So, the value for sp->cons_mouse_path
		 * for sun4u/sbus machines will have a :b appended on the end
		 *
		 * Since the minor name is not needed for this check,
		 * it is stripped off the obp pathname.  If the minor
		 * name isn't present, this code does nothing.
		 *
		 * The goal of this dacf module is to be as generic
		 * across platforms as possible.  This piece of code
		 * doesn't make the code platform specific.
		 */
		p = (strrchr(sp->cons_mouse_path, '/'));

		if (p != NULL) {
			q = strchr(p, ':');

			/* truncate string if minor name is found */
			if (q != 0)
				*q = (char)0;
		}

		consconfig_dprintf(DPRINT_L2, "ddi ms path %s\n", ms_path);
		consconfig_dprintf(DPRINT_L2, "obp ms path %s\n",
				sp->cons_mouse_path);

		res = strcmp(sp->cons_mouse_path, ms_path);

		kmem_free(ms_path, MAXPATHLEN);

		if (res != 0) {

			consconfig_dprintf(DPRINT_L2,
				"Not the console mouse\n");

			/* This is not the console mouse */
			mutex_exit(&sp->cons_ms_mutex);

			return (DACF_SUCCESS);
		} else {

			consconfig_dprintf(DPRINT_L2,
				"Found the console mouse\n");
		}
	}

	/*
	 * Keep track of the major and minor number of the console mouse
	 */
	sp->cons_ms_major = major;
	sp->cons_ms_minor = minor;

	/*
	 * The mutex must be released over calls to the file system.
	 * Only one node is allowed to be the console mouse and this
	 * is enforced by the cons_ms_major and cons_ms_minor fields.
	 * So, only one thread will reach this point at a time.
	 */
	mutex_exit(&sp->cons_ms_mutex);

	/*
	 * Create a vnode pointer so that we can perform pushing or
	 * popping funtions on the mouse device.
	 */
	mouse_avp = consconfig_util_createvp(dev);

	if (consconfig_util_openvp(mouse_avp) != 0) {
		/*
		 * The mouse node failed to open.
		 * Set the major and minor numbers to 0 so
		 * that ms_unconfig won't unconfigure the
		 * node if it is detached.
		 */
		mutex_enter(&sp->cons_ms_mutex);
		sp->cons_ms_major = 0;
		sp->cons_ms_minor = 0;
		mutex_exit(&sp->cons_ms_mutex);
		consconfig_util_freevp(mouse_avp);

		return (DACF_SUCCESS);
	}

	/* Link consms on top of the mouse device */
	consconfig_dacf_mouselink(sp, mouse_avp, (char *)pushmod);

	/*
	 * Increment the module reference count so that the driver
	 * can't be unloaded.
	 */
	(void) ddi_hold_installed_driver(major);

	consconfig_util_freevp(mouse_avp);

	return (DACF_SUCCESS);
}

/*ARGSUSED*/
static int
ms_unconfig(dacf_infohdl_t info_hdl, dacf_arghdl_t arg_hdl, int flags)
{
	major_t				major;
	minor_t				minor;
	cons_state_t			*sp;

	consconfig_dprintf(DPRINT_L0, "ms_unconfig callback!\n");

	/* Retrieve the state information */
	sp = (cons_state_t *)space_fetch("consconfig");

	/*
	 * So if there isn't a state available, then this entry point just
	 * returns.  See note in kb_config().
	 */
	if (sp == NULL)
		return (DACF_SUCCESS);

	major = ddi_name_to_major((char *)dacf_driver_name(info_hdl));
	if (major == (major_t)-1) {
		consconfig_dprintf(DPRINT_L0, "invalid major #\n");

		return (DACF_FAILURE);
	}

	minor = dacf_minor_number(info_hdl);

	/*
	 * Check if the mouse that is being detached
	 * is the console mouse or not
	 */
	mutex_enter(&sp->cons_ms_mutex);
	if ((major != sp->cons_ms_major) ||
		(minor != sp->cons_ms_minor)) {

		consconfig_dprintf(DPRINT_L0, "Unplug of non-console mouse\n");

		mutex_exit(&sp->cons_ms_mutex);

		return (DACF_SUCCESS);
	}

	/*
	 * Release the mutex over calls into the file system.
	 * It is safe to release the mutex because the cons_ms_major
	 * and cons_ms_minor fields are still set.  So, it won't
	 * be the case that ms_config will be linking a new node
	 * into the console stream while ms_unconfig
	 * is unlinking a node.
	 */
	mutex_exit(&sp->cons_ms_mutex);

	consconfig_dprintf(DPRINT_L0, "Unplug of console mouse\n");

	/* Tear down the mouse stream */
	consconfig_dacf_mouseunlink(sp);

	/*
	 * We are no longer using this driver, so allow it to be
	 * unloaded.
	 */
	ddi_rele_driver(major);

	/*
	 * Reset sp->cons_ms_major and sp->cons_ms_minor
	 * A new mouse may now be linked in.
	 */
	mutex_enter(&sp->cons_ms_mutex);
	sp->cons_ms_major = 0;
	sp->cons_ms_minor = 0;
	mutex_exit(&sp->cons_ms_mutex);

	return (DACF_SUCCESS);
}

/*
 * consconfig_dacf_kblink:
 * 	Link the keyboard streams driver under the keyboard mux (conskbd).
 * 	This routine is called for the keyboard.
 */
static void
consconfig_dacf_kblink(cons_state_t *sp, consconfig_vnode_t *kbd_avp,
				char *module_name)
{
	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_kblink: start\n");

	/*
	 * Push the module_name on top of the vnode that we have been given,
	 * and configure the module.
	 */
	consconfig_util_kbconfig(sp, kbd_avp, TR_CAN, module_name);

	/*
	 * Link the stream underneath conskbd
	 * Note that errors aren't checked here.
	 */
	consconfig_dacf_link_conskbd(sp, kbd_avp);

	/*
	 * At this point, the stream is:
	 *	wc->conskbd->"module_name"->"kbd_avp driver"
	 */

	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_kblink: end\n");
}

/*
 * consconfig_dacf_mouselink:
 * 	Push the module called "module_name" on top of the
 *	driver specified in the mouse_avp argument.  Link
 * 	the driver specified by the mouse_avp argument under
 *	consms.
 */
static void
consconfig_dacf_mouselink(cons_state_t *sp, consconfig_vnode_t *mouse_avp,
				char *module_name)
{
	int	rval;

	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_mouselink: start\n");

	/*
	 * Send a flush to the mouse driver.
	 */
	(void) strioctl(mouse_avp->consconfig_vp,
		I_FLUSH, (intptr_t)FLUSHRW,
		FREAD+FNOCTTY, K_TO_K, kcred, &rval);

	/* if there's an error with the push, just return */
	if (consconfig_util_push(mouse_avp, module_name) != 0) {

		return;
	}

	/*
	 * Link the serial module underneath consms
	 */
	consconfig_dacf_link_consms(sp, mouse_avp);

	/*
	 * At this point, the stream is:
	 *		consms->"module_name"->"mouse_avp driver"
	 */
}

/*
 * consconfig_dacf_mouseunlink:
 * 	Unlink the mouse module and driver from underneath consms
 */
static void
consconfig_dacf_mouseunlink(cons_state_t *sp)
{
	consconfig_vnode_t	*consmouse_avp;
	dev_t		dev;
	int		error;

	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_mouseunlink: start\n");

	/*
	 * Open the "console mouse" device, to unlink the mouse device
	 * under it.
	 */
	dev = consconfig_util_getdev("consms", 0);

	consmouse_avp = consconfig_util_createvp(dev);

	error = consconfig_util_openvp(consmouse_avp);

	/* If there is an error with opening, just return */
	if (error) {
		consconfig_util_freevp(consmouse_avp);
		return;
	}

	/*
	 * Unlink the mouse stream represented by mouse_avp
	 * from consms.
	 */
	error = consconfig_util_unlink(consmouse_avp, sp->cons_ms_muxid);

	if (error != 0) {
		cmn_err(CE_WARN,
		    "consconfig_dacf_mouseunlink:  unlink failed, error %d",
		    error);
	}

	consconfig_util_closevp(consmouse_avp);

	/*
	 * Completely destroy the vnode that was created above
	 */
	consconfig_dacf_destroyvp(consmouse_avp);

	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_mouseunlink: end\n");
}


/*
 * consconfig_dacf_link_conskbd:
 * 	kbd_avp represents a driver with a keyboard module pushed on top of
 *	it. The driver is linked underneath conskbd.
 */
static void
consconfig_dacf_link_conskbd(cons_state_t *sp,
				consconfig_vnode_t *kbd_avp)
{
	int error;

	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_link_conskbd: start\n");

	/*
	 * Hold the file_t so that the unlink doesn't cause the file_t to
	 * be closed.  Closing the file_t would cause conskbd to be detached
	 * when we want it to remain in place.
	 */
	consconfig_holdvp(sp->cons_kbd_avp);

	/* if there's an error with the file descriptor, just return */
	if (consconfig_util_getfd(sp->cons_kbd_avp)) {
		return;
	}

	/* wc won't fail this open, so ignore return value */
	(void) consconfig_util_openvp(sp->cons_rws_avp);

	/*
	 * Temporarily unlink conskbd from wc so that the kbd_avp
	 * stream may be linked under conskbd.  This has to be done
	 * because streams are built bottom up and linking a stream
	 * under conskbd isn't allowed when conskbd is linked under
	 * wc.
	 */
	error = consconfig_util_unlink(sp->cons_rws_avp,
					    sp->cons_conskbd_muxid);

	if (error != 0) {
		cmn_err(CE_WARN,
		    "consconfig_dacf_link_conskbd:  unlink failed, error %d",
		    error);
	}

	/* if there's an error with the file, just return */
	if (consconfig_util_getfile(kbd_avp)) {

		return;
	}

	/* if there's an error with the file descriptor, just return */
	if (consconfig_util_getfd(kbd_avp)) {

		return;
	};

	consconfig_dprintf(DPRINT_L0, "linking keyboard under conskbd\n");

	/*
	 * Link the stream represented by kbd_avp under conskbd
	 */
	error = consconfig_util_link(sp->cons_kbd_avp,
				kbd_avp->consconfig_fd,
				&sp->cons_kb_muxid);

	if (error != 0) {
		cmn_err(CE_WARN,
		    "consconfig_dacf_link_conskbd:  kb link failed, error %d",
		    error);
	}

	/*
	 * Link consbkd back under wc.
	 *
	 * The act of linking conskbd back under wc will cause wc
	 * to query the lower lower layers about their polled I/O
	 * routines.  This time the request will succeed because there
	 * is a physical keyboard linked under conskbd.
	 */
	error = consconfig_util_link(sp->cons_rws_avp,
		sp->cons_kbd_avp->consconfig_fd,
		&sp->cons_conskbd_muxid);

	if (error != 0) {
		cmn_err(CE_WARN,
	"consconfig_dacf_unlink_conskbd:  conskbd link failed, error %d",
		error);
	}

	consconfig_util_closevp(sp->cons_rws_avp);

	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_link_conskbd: end\n");
}

/*
 * consconfig_dacf_unlink_conskbd:
 * 	Unlink the driver with the keyboard module pushed on top from
 *	beneath conskbd.  The function will leave just wc->conskbd
 */
static void
consconfig_dacf_unlink_conskbd(cons_state_t *sp)
{
	int err;
	consconfig_dprintf(DPRINT_L0,
				"consconfig_dacf_unlink_conskbd: start\n");

	/*
	 * Hold the file_t so that the unlink doesn't cause the file_t to be
	 * closed.  Closing the file_t would cause conskbd to be detached
	 * when we want it to remain in place.
	 */
	consconfig_holdvp(sp->cons_kbd_avp);

	/* if there's an error with the file descriptor, just return */
	if (consconfig_util_getfd(sp->cons_kbd_avp)) {

		return;
	}

	/* wc won't fail this open, so ignore return value. */
	(void) consconfig_util_openvp(sp->cons_rws_avp);

	/*
	 * Temporarily unlink conskbd from wc so that the kbd_avp
	 * stream may be linked under conskbd.  This has to be done
	 * because streams are built bottom up and unlinking a stream
	 * under conskbd isn't allowed when conskbd is linked under
	 * wc.
	 */
	err = consconfig_util_unlink(sp->cons_rws_avp, sp->cons_conskbd_muxid);

	if (err != 0) {
		cmn_err(CE_WARN,
	"consconfig_dacf_unlink_conskbd:  unlink of conskbd failed, error %d",
		err);
	}

	/*
	 * This will cause the keyboard driver to be closed, all modules to be
	 * popped, and the keyboard vnode released.
	 */
	err  = consconfig_util_unlink(sp->cons_kbd_avp, sp->cons_kb_muxid);

	if (err != 0) {
		cmn_err(CE_WARN,
	"consconfig_dacf_unlink_conskbd:  unlink of kb failed, error %d",
		err);
	}

	/*
	 * Link consbkd back under wc
	 */
	err = consconfig_util_link(sp->cons_rws_avp,
		sp->cons_kbd_avp->consconfig_fd,
		&sp->cons_conskbd_muxid);

	if (err != 0) {
		cmn_err(CE_WARN,
	"consconfig_dacf_unlink_conskbd:  conskbd link failed, error %d",
		err);
	}

	consconfig_util_closevp(sp->cons_rws_avp);

	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_unlink_conskbd: end\n");
}


/*
 * consconfig_dacf_link_consms:
 *	mouse_avp is a driver with a mouse module pushed on top.
 *	The driver is linked underneath consms
 */
static void
consconfig_dacf_link_consms(cons_state_t *sp,
				consconfig_vnode_t *mouse_avp)
{
	consconfig_vnode_t	*consmouse_avp;
	dev_t			dev;
	int			error;


	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_link_cons: start\n");

	/*
	 * Open the "console mouse" device, and link the mouse device
	 * under it.  The mouse_avp stream will look like ms->"serial-driver"
	 * or usbms->hid at this point.
	 */
	dev = consconfig_util_getdev("consms", 0);

	/*
	 * We will open up a new stream for the consms device and link our
	 * ms->"serial-driver" stream underneath it.  consms is a multiplexor.
	 */
	consmouse_avp = consconfig_util_createvp(dev);

	error = consconfig_util_openvp(consmouse_avp);

	/* if there is an error with opening, just return */
	if (error) {
		consconfig_util_freevp(consmouse_avp);
		return;
	}

	/* if there's an error with the file, just return */
	if (consconfig_util_getfile(mouse_avp))  {

		return;
	}

	/* if there's an error with the file descriptor, just return */
	if (consconfig_util_getfd(mouse_avp)) {

		return;
	}

	/*
	 * Link ms stream underneath consms multiplexor.
	 */
	error = consconfig_util_link(consmouse_avp, mouse_avp->consconfig_fd,
				&sp->cons_ms_muxid);

	if (error != 0) {
		cmn_err(CE_WARN,
		"consconfig_dacf_link_consms:  mouse link failed, error %d",
		error);
	}

	/*
	 * Increment the module reference count so that the driver can't
	 * be unloaded.
	 */
	(void) ddi_hold_installed_driver(getmajor(dev));

	consconfig_util_closevp(consmouse_avp);

	/* Completely destroy the vnode that was created above. */
	consconfig_dacf_destroyvp(consmouse_avp);

	consconfig_dprintf(DPRINT_L0, "consconfig_dacf_link_cons: end\n");
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
consconfig_holdvp(consconfig_vnode_t *avp)
{
	mutex_enter(&avp->consconfig_fp->f_tlock);

	avp->consconfig_fp->f_count++;

	mutex_exit(&avp->consconfig_fp->f_tlock);
}

/*
 * consconfig_dacf_destroyvp:
 *
 *	This routine is a convenience routine that releases a vnode that has
 *	been allocated by makespecvp()
 */
static void
consconfig_dacf_destroyvp(consconfig_vnode_t *avp)
{
	/*
	 * Release the vnode.  VOP_CLOSE() calls the close routine, but does not
	 * get rid of the vnode.  VN_RELE() will call through vn_rele()->
	 * VOP_INACTIVE()->spec_inactive() to release the vnode.  Normally
	 * this happens on a closef, but we need to call this routine if
	 * we have allocated a vnode, but have not allocated a file_t to go
	 * with it.
	 */
	VN_RELE(avp->consconfig_vp);

	consconfig_util_freevp(avp);
}
