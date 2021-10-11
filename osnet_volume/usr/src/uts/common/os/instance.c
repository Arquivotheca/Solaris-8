/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)instance.c	1.32	99/05/31 SMI"

/*
 * Instance number assignment code
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/kobj.h>
#include <sys/t_lock.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/autoconf.h>
#include <sys/systeminfo.h>
#include <sys/dc_ki.h>
#include <sys/clconf.h>
#include <sys/cladm.h>
#include <sys/hwconf.h>
#include <sys/reboot.h>
#include <sys/ddi_impldefs.h>
#include <sys/instance.h>
#include <sys/debug.h>
#include <sys/devfs_log_event.h>
#include <sys/modctl.h>

static void i_log_devfs_instance_mod(void);
static int in_get_infile(int);
static uint_t in_next_instance(major_t major, in_drv_t *dp, uint_t inst);
static void in_removenode(struct devnames *dnp, in_node_t *mp, in_node_t *ap);
static in_node_t *in_alloc_node(char *name, char *addr);
static int in_eqstr(char *a, char *b);
static char *in_name_addr(char **cpp, char **addrp);
static in_node_t *in_devwalk(dev_info_t *dip, in_node_t **ap);
static void in_dealloc_node(in_node_t *np);
static void in_hash_walk(in_node_t *np);
static void in_pathin(char *cp, uint_t instance, char *bname);
static in_node_t *in_make_path(char *path);
static void in_enlist(in_node_t *ap, in_node_t *np);
static int in_inuse(uint_t instance, char *name);
static void in_hashdrv(in_drv_t *dp);
static int in_ask_rebuild(void);
static in_drv_t *in_drvwalk(in_node_t *np, char *binding_name);
static in_drv_t *in_alloc_drv(char *bindingname);
static void in_endrv(in_node_t *np, in_drv_t *dp);
static void in_dq_drv(in_drv_t *np);
void in_removedrv(struct devnames *dnp, in_drv_t *mp);

/* external functions */
extern char *i_binding_to_drv_name(char *bname);

in_softstate_t e_ddi_inst_state;

/*
 * State transition information:
 * e_ddi_inst_state contains, among other things, the root of a tree of
 * nodes used to track instance number assignment.
 * Each node can be in one of 3 states, indicated by ind_state:
 * IN_UNKNOWN:	Each node is created in this state.  The instance number of
 *	this node is not known.  ind_instance is set to -1.
 * IN_PROVISIONAL:  When a node is assigned an instance number in
 *	e_ddi_assign_instance(), its state is set to IN_PROVISIONAL.
 *	Subsequently, the framework will always call either
 *	e_ddi_keep_instance() which makes the node IN_PERMANENT,
 *	or e_ddi_free_instance(), which deletes the node.
 * IN_PERMANENT:
 *	If e_ddi_keep_instance() is called on an IN_PROVISIONAL node,
 *	its state is set to IN_PERMANENT.
 *
 *	During the processing of the /etc/path_to_inst file in
 *	e_ddi_instance_init(), after all nodes that have been explicitly
 *	assigned instance numbers in path_to_inst have been processed,
 *	all inferred nodes (nexus nodes that have only been encountered in
 *	the path to an explicitly assigned node) are assigned instance
 *	numbers in in_hashdrv() and their state changed from IN_UNKNOWN
 *	to IN_PERMANENT.
 *
 *
 * Solaris Clustering:
 *	The contents of /etc/path_to_inst refer to instance
 *	numbers that may not refer to devices on this node. The instance
 *	numbers are handed out on a global basis. In other words if you have
 *	an instance for driver, it shall have be unique
 *	across the cluster. It is not clear if the unique instance number
 *	assignment is of any value for single instance devices, such
 *	as the mm driver. There is exactly one instance of such a device
 *	and the encoded major/minor number does have any hint of instance
 *	number in them.
 *
 */

/*
 * This can be undefined (and the code removed as appropriate)
 * when the real devfs is done.
 */
#define	INSTANCE_TRANSITION_MODE

#ifdef	INSTANCE_TRANSITION_MODE
/*
 * This flag generates behaviour identical to that before persistent
 * instance numbers, allowing us to boot without having installed a
 * correct /etc/path_to_inst file.
 */
static int in_transition_mode;

#else	/* ! INSTANCE_TRANSITION_MODE */

#define	in_transition_mode	0

#endif	/* INSTANCE_TRANSITION_MODE */

static char *instance_file = INSTANCE_FILE;

/*
 * Return values for in_get_infile().
 */
#define	PTI_FOUND	0
#define	PTI_NOT_FOUND	1
#define	PTI_REBUILD	2

/*
 * Path to instance file magic string used for first time boot after
 * an install.  If this is the first string in the file we will
 * automatically rebuild the file.
 */
#define	PTI_MAGIC_STR		"#path_to_inst_bootstrap_1"
#define	PTI_MAGIC_STR_LEN	25

void
e_ddi_instance_init(void)
{
	int hshndx;
	struct bind *bp;
	struct bind **in_hashtab = NULL;
	struct in_drv *dp, *ndp;
	char *name;

	mutex_init(&e_ddi_inst_state.ins_serial, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&e_ddi_inst_state.ins_serial_cv, NULL, CV_DEFAULT, NULL);

	/*
	 * Only one thread is allowed to change the state of the instance
	 * number assignments on the system at any given time.
	 * Note that this is not really necessary, as we are single-threaded
	 * here, but it won't hurt, and it allows us to keep ASSERTS for
	 * our assumptions in the code.
	 */
	mutex_enter(&e_ddi_inst_state.ins_serial);
	ASSERT(!e_ddi_inst_state.ins_busy);	/* too early to be busy! */
	while (e_ddi_inst_state.ins_busy)
		cv_wait(&e_ddi_inst_state.ins_serial_cv,
		    &e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 1;
	mutex_exit(&e_ddi_inst_state.ins_serial);

	/*
	 * Create the root node, instance zallocs to 0.
	 * The name and address of this node never get examined, we always
	 * start searching with its first child.
	 */
	ASSERT(e_ddi_inst_state.ins_root == NULL);
	e_ddi_inst_state.ins_root = in_alloc_node(NULL, NULL);
	/*
	 * XXX ddi_get_name() returns binding name, but not this early in
	 * XXX the kernel.   THis is  cribbed from  create_root_class.
	 */
	dp = in_alloc_drv(i_binding_to_drv_name("rootnex"));
	in_endrv(e_ddi_inst_state.ins_root, dp);

	switch (in_get_infile((boothowto & RB_ASKNAME))) {
	case PTI_REBUILD:
		/*
		 * The file does not exist, but user booted -a and wants
		 * to rebuild the file, or this is the first boot after
		 * an install and the magic string is all that is in the
		 * file.
		 */
#ifdef	INSTANCE_TRANSITION_MODE
		in_transition_mode = 1;
#endif	/* INSTANCE_TRANSITION_MODE */

		/*
		 * Need to set the reconfigure flag so that the /devices
		 * and /dev directories gets rebuild.
		 */
		boothowto |= RB_RECONFIG;

		cmn_err(CE_CONT,
			"?Using default device instance data\n");
		break;

	case PTI_FOUND:
		/*
		 * Assertion:
		 *	We've got a readable file
		 */
		in_hashtab = kmem_zalloc(
		    sizeof (struct bind) * MOD_BIND_HASHSIZE, KM_SLEEP);

		(void) read_binding_file(instance_file, in_hashtab);
		/*
		 * This really needs to be a call into the mod code
		 *
		 * b_bind_name may be in one of three states...
		 * 1. it may be NULL if we are dealing with an
		 * old version of the path_to_inst file that has just
		 * 2 fields per line.
		 * 2. it may contain a valid binding name/driver - in
		 * which case we pass the driver name to in_pathin.
		 * 3. it may contain a binding/driver name that no longer
		 * corresponds to a valid driver on the system.  In this
		 * case we pass it to in_pathin since we cannot convert
		 * it - in_pathin does the right thing and keeps the old
		 * instance around.
		 */
		for (hshndx = 0; hshndx < MOD_BIND_HASHSIZE; hshndx++) {
			bp = in_hashtab[hshndx];
			while (bp) {
				if (*bp->b_name == '/') {
					if (bp->b_bind_name != NULL) {
						name = i_binding_to_drv_name
						    (bp->b_bind_name);
						if (name == NULL)
							name = bp->b_bind_name;
					} else {
						name = NULL;
					}
					in_pathin(bp->b_name, bp->b_num, name);
				} else {
					cmn_err(CE_WARN,
					    "invalid instance file entry %s %d",
					    bp->b_name, bp->b_num);
				}
				bp = bp->b_next;
			}
		}

		clear_binding_hash(in_hashtab);

		kmem_free(in_hashtab, sizeof (struct bind) * MOD_BIND_HASHSIZE);

		/*
		 * Walk the tree, calling in_hashdrv() on every node that
		 * already has an instance number assigned
		 */
		in_hash_walk(e_ddi_inst_state.ins_root->in_child);

		/*
		 * Now do the ones that did not
		 */
		dp = e_ddi_inst_state.ins_no_instance;
		e_ddi_inst_state.ins_no_instance = NULL;
		while (dp) {
			ndp = dp->ind_next;
			dp->ind_next = NULL;
			in_hashdrv(dp);
			dp = ndp;
		}
		break;

	default:
	case PTI_NOT_FOUND:
		/*
		 * .. else something is terribly wrong.
		 *
		 * We're paranoid here because the loss of this file
		 * is potentially damaging to user data e.g. a
		 * controller slips and we swap on someones database..
		 * Oops.
		 *
		 * This is the rather vicious and cruel version
		 * favoured by some.  If you can't find path_to_inst
		 * and you're not booting with '-a' then just halt
		 * the system.
		 *
		 */
		cmn_err(CE_CONT, "Cannot open '%s'\n", instance_file);
		halt((char *)NULL);
		/*NOTREACHED*/
		break;

	}

	mutex_enter(&e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 0;
	cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
	mutex_exit(&e_ddi_inst_state.ins_serial);
}

/*
 * Checks to see if the /etc/path_to_inst file exists and whether or not
 * it has the magic string in it.
 *
 * Returns one of the following:
 *
 *	PTI_FOUND	- We have found the /etc/path_to_inst file
 *	PTI_REBUILD	- We did not find the /etc/path_to_inst file, but
 *			  the user has booted with -a and wants to rebuild it
 *			  or we have found the /etc/path_to_inst file and the
 *			  first line was PTI_MAGIC_STR.
 *	PTI_NOT_FOUND	- We did not find the /etc/path_to_inst file
 *
 */
static int
in_get_infile(int ask_rebuild)
{
	intptr_t file;
	int return_val;
	char buf[PTI_MAGIC_STR_LEN];

	/*
	 * Try to open the file.  If the user booted -a (ask_rebuild == TRUE if
	 * the user booted -a) and the file was not found, then ask the
	 * user if they want to rebuild the path_to_inst file.  If not,
	 * then return file not found, else return rebuild.
	 */
	if ((file = kobj_open(instance_file)) == -1) {
		if (ask_rebuild && in_ask_rebuild())
			return (PTI_REBUILD);
		else
			return (PTI_NOT_FOUND);
	} else {
		return_val = PTI_FOUND;
	}

	/*
	 * Read the first PTI_MAGIC_STR_LEN bytes from the file to see if
	 * it contains the magic string.  If there aren't that many bytes
	 * in the file, then assume file is correct and no magic string
	 * and move on.
	 */
	switch (kobj_read(file, buf, PTI_MAGIC_STR_LEN, 0)) {

	case PTI_MAGIC_STR_LEN:
		/*
		 * If the first PTI_MAGIC_STR_LEN bytes are the magic string
		 * then return PTI_REBUILD.
		 */
		if (strncmp(PTI_MAGIC_STR, buf, PTI_MAGIC_STR_LEN) == 0)
			return_val = PTI_REBUILD;
		break;

	case 0:
		/*
		 * If the file is zero bytes in length, then consider the
		 * file to not be found and ask the user if they want to
		 * rebuild it (if ask_rebuild is true).
		 */
		if (ask_rebuild && in_ask_rebuild())
			return_val = PTI_REBUILD;
		else
			return_val = PTI_NOT_FOUND;
		break;

	default: /* Do nothing we have a good file */
		break;
	}

	kobj_close(file);
	return (return_val);
}

static int
in_ask_rebuild(void)
{
	char answer[32];

	extern void gets(char *);

	do {
		answer[0] = '\0';
		printf(
		    "\nThe %s on your system does not exist or is empty.\n"
		    "Do you want to rebuild this file [n]? ",
		    instance_file);
		gets(answer);
		if ((answer[0] == 'y') || (answer[0] == 'Y'))
			return (1);
	} while ((answer[0] != 'n') && (answer[0] != 'N') &&
	    (answer[0] != '\0'));
	return (0);
}

static void
in_hash_walk(in_node_t *np)
{
	in_drv_t *dp;

	while (np) {
		for (dp = np->in_drivers; dp; dp = dp->ind_next_drv) {
			if (dp->ind_state == IN_UNKNOWN) {
				dp->ind_next = e_ddi_inst_state.ins_no_instance;
				e_ddi_inst_state.ins_no_instance = dp;
			} else
				in_hashdrv(dp);
		}
		if (np->in_child)
			in_hash_walk(np->in_child);
		np = np->in_sibling;
	}
}

int
is_pseudo_device(dev_info_t *dip)
{
	dev_info_t	*pdip;

	for (pdip = ddi_get_parent(dip); pdip && pdip != ddi_root_node();
	    pdip = ddi_get_parent(pdip)) {
		if (strcmp(ddi_get_name(pdip), DEVI_PSEUDO_NEXNAME) == 0)
			return (1);
	}
	return (0);
}


static void
in_set_instance(dev_info_t *dip, in_drv_t *dp, major_t major, int transition)
{
#ifdef INSTANCE_TRANSITION_MODE
	int cinstance;

	if (transition) {
		/*
		 * If a hint is provided by .conf file
		 * processing and it does not conflict with
		 * a previous assignment, use it
		 */
		if ((cinstance = ddi_get_instance(dip)) < 0)
			cinstance = 0;
		dp->ind_instance = in_next_instance(major, dp, cinstance);
	} else {
		dp->ind_instance = in_next_instance(major, dp, 0);
	}
#else	/* INSTANCE_TRANSITION_MODE */
	dp->ind_instance = in_next_instance(major, dp, 0);
#endif	/* INSTANCE_TRANSITION_MODE */
}

/*
 * Look up an instance number for a dev_info node, and assign one if it does
 * not have one (the dev_info node has devi_name and devi_addr already set).
 */
uint_t
e_ddi_assign_instance(dev_info_t *dip)
{
	char *name;
	in_node_t *ap, *np;
	in_drv_t *dp;
	major_t major;
	uint_t ret;
	char *bname;

	/*
	 * Allow implementation to override
	 */
	if ((ret = impl_assign_instance(dip)) != (uint_t)-1)
		return (ret);

	/*
	 * If this is a pseudo-device, use the instance number
	 * assigned by the pseudo nexus driver. The mutex is
	 * not needed since the instance tree is not used.
	 */
	if (is_pseudo_device(dip)) {
		return (ddi_get_instance(dip));
	}

	/*
	 * Only one thread is allowed to change the state of the instance
	 * number assignments on the system at any given time.
	 */
	mutex_enter(&e_ddi_inst_state.ins_serial);
	while (e_ddi_inst_state.ins_busy)
		cv_wait(&e_ddi_inst_state.ins_serial_cv,
		    &e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 1;
	mutex_exit(&e_ddi_inst_state.ins_serial);

	/*
	 * could use ddi_get_name, but this is more explicit
	 */
	bname = i_binding_to_drv_name(ddi_binding_name(dip));

	np = in_devwalk(dip, &ap);
	if (np) {
		/*
		 * found a matching instance node, we're half done
		 */
		if (dp = in_drvwalk(np, bname)) {
			ret = dp->ind_instance;
			mutex_enter(&e_ddi_inst_state.ins_serial);
			e_ddi_inst_state.ins_busy = 0;
			cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
			mutex_exit(&e_ddi_inst_state.ins_serial);
			return (ret);
		} else {
			/*
			 * Need to create a new in_drv struct
			 */
			dp = in_alloc_drv(bname);
			major = ddi_name_to_major(bname);
			ASSERT(major != (major_t)-1);
			in_endrv(np, dp);
			in_set_instance(dip, dp, major, in_transition_mode);
			mutex_enter(&e_ddi_inst_state.ins_serial);
			e_ddi_inst_state.ins_busy = 0;
			cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
			mutex_exit(&e_ddi_inst_state.ins_serial);
			return (dp->ind_instance);
		}
	}
	name = ddi_node_name(dip);
	major = ddi_name_to_major(bname);
	ASSERT(major != (major_t)-1);

	np = in_alloc_node(name, ddi_get_name_addr(dip));
	if (np == NULL)
		cmn_err(CE_PANIC, "instance initialization");
	in_enlist(ap, np);	/* insert into tree */
	dp = in_alloc_drv(bname);
	if (dp == NULL)
		cmn_err(CE_PANIC, "instance initialization");

	in_endrv(np, dp);
	in_set_instance(dip, dp, major, in_transition_mode);
	dp->ind_state = IN_PROVISIONAL;

	in_hashdrv(dp);

	ASSERT(np == in_devwalk(dip, &ap));

	ret = dp->ind_instance;

	mutex_enter(&e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 0;
	cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
	mutex_exit(&e_ddi_inst_state.ins_serial);
	return (ret);
}

static
mkpathname(char *path, in_node_t *np, int len)
{
	int len_needed;

	if (np == e_ddi_inst_state.ins_root)
		return (0);

	if (mkpathname(path, np->in_parent, len) == -1)
		return (-1);

	len_needed = strlen(path);
	len_needed += strlen(np->in_node_name) + 1;	/* for '/' */
	if (np->in_unit_addr) {
		len_needed += strlen(np->in_unit_addr) + 1;  /* for '@' */
	}
	len_needed += 1; /* for '\0' */

	/*
	 * XX complain
	 */
	if (len_needed > len)
		return (-1);

	if (np->in_unit_addr[0] == '\0')
		(void) sprintf(path+strlen(path), "/%s", np->in_node_name);
	else
		(void) sprintf(path+strlen(path), "/%s@%s", np->in_node_name,
		    np->in_unit_addr);

	return (0);
}

static char *
get_instpath(in_drv_t *dp, char *path)
{
	/*
	 * Solaris Clustering: The node uniquifier is added to the path.
	 * For non-clustered systems it is just a null string.
	 */
	(void) strcpy(path, i_ddi_get_dpath_prefix());

	(void) mkpathname(path, dp->ind_node, MAXPATHLEN);
	return (path);
}

/*
 * This depends on the list being sorted in ascending instance number
 * sequence.  dn_instance contains the next available instance no.
 * or IN_SEARCHME, indicating (a) hole(s) in the sequence.
 */
static uint_t
in_next_instance(major_t major, in_drv_t *ndp, uint_t inst)
{
	unsigned int prev = inst;
	char	*path;

	path = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	(void) DC_GET_INSTANCE(&dcops, major, get_instpath(ndp, path), &prev);
	kmem_free(path, MAXPATHLEN);

	return (prev);
}

int
in_next_local_instance(major_t major)
{
	unsigned int prev;
	struct devnames *dnp;
	in_drv_t *dp;

	dnp = &devnamesp[major];

	ASSERT(major != (major_t)-1);
	ASSERT(e_ddi_inst_state.ins_busy);
	if (dnp->dn_instance != IN_SEARCHME)
		return (dnp->dn_instance++);
	dp = dnp->dn_inlist;
	if (dp == NULL) {
		dnp->dn_instance = 1;
		return (0);
	}
	if (dp->ind_next == NULL) {
		if (dp->ind_instance != 0)
			return (0);
		else {
			dnp->dn_instance = 2;
			return (1);
		}
	}
	prev = dp->ind_instance;
	if (prev != 0)	/* hole at beginning of list */
		return (0);
	/* search the list for a hole in the sequence */
	for (dp = dp->ind_next; dp; dp = dp->ind_next) {
		if (dp->ind_instance != prev + 1)
			return (prev + 1);
		else
			prev++;
	}
	/*
	 * If we got here, then the hole has been patched
	 */
	dnp->dn_instance = ++prev + 1;

	return (prev);
}

/*
 * This call causes us to *forget* the instance number we've generated
 * for a given device if it was not permanent.
 */
void
e_ddi_free_instance(dev_info_t *dip)
{
	char *name;
	in_node_t *np;
	in_node_t *ap;	/* ancestor node */
	major_t major;
	struct devnames *dnp;
	in_drv_t *dp;	/* in_drv entry */

	/*
	 * Allow implementation override
	 */
	if (impl_free_instance(dip) == DDI_SUCCESS)
		return;

	/*
	 * If this is a pseudo-device, no instance number
	 * was assigned.
	 */
	if (is_pseudo_device(dip)) {
		return;
	}

	/* gets driver name */
	name = i_binding_to_drv_name(ddi_binding_name(dip));
	major = ddi_name_to_major(name);
	ASSERT(major != (major_t)-1);
	dnp = &devnamesp[major];
	/*
	 * Only one thread is allowed to change the state of the instance
	 * number assignments on the system at any given time.
	 */
	mutex_enter(&e_ddi_inst_state.ins_serial);
	while (e_ddi_inst_state.ins_busy)
		cv_wait(&e_ddi_inst_state.ins_serial_cv,
		    &e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 1;
	mutex_exit(&e_ddi_inst_state.ins_serial);
	np = in_devwalk(dip, &ap);
	ASSERT(np);
	dp = in_drvwalk(np, name);
	ASSERT(dp);
	if (dp->ind_state == IN_PROVISIONAL) {
		in_removedrv(dnp, dp);
	}
	if (np->in_drivers == NULL) {
		in_removenode(dnp, np, ap);
	}
	mutex_enter(&e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 0;
	cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
	mutex_exit(&e_ddi_inst_state.ins_serial);
}

/*
 * This makes our memory of an instance assignment permanent
 */
void
e_ddi_keep_instance(dev_info_t *dip)
{
	in_node_t *np, *ap;
	in_drv_t *dp;

	/*
	 * Allow implementation override
	 */
	if (impl_keep_instance(dip) == DDI_SUCCESS)
		return;

	/*
	 * Nothing to do for pseudo devices.
	 */
	if (is_pseudo_device(dip))
		return;

	/*
	 * Only one thread is allowed to change the state of the instance
	 * number assignments on the system at any given time.
	 */
	mutex_enter(&e_ddi_inst_state.ins_serial);
	while (e_ddi_inst_state.ins_busy)
		cv_wait(&e_ddi_inst_state.ins_serial_cv,
		    &e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 1;
	mutex_exit(&e_ddi_inst_state.ins_serial);
	np = in_devwalk(dip, &ap);
	ASSERT(np);
	dp = in_drvwalk(np, i_binding_to_drv_name(ddi_binding_name(dip)));
	ASSERT(dp);

	mutex_enter(&e_ddi_inst_state.ins_serial);
	if (dp->ind_state == IN_PROVISIONAL) {
		dp->ind_state = IN_PERMANENT;
		i_log_devfs_instance_mod();
	}
	e_ddi_inst_state.ins_busy = 0;
	cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
	mutex_exit(&e_ddi_inst_state.ins_serial);
}

/*
 * The devnames struct for this driver is about to vanish.
 * Put the instance tracking nodes on the orphan list
 */
void
e_ddi_orphan_instance_nos(in_drv_t *dp)
{
	in_drv_t *ndp;

	/*
	 * Only one thread is allowed to change the state of the instance
	 * number assignments on the system at any given time.
	 */
	mutex_enter(&e_ddi_inst_state.ins_serial);
	while (e_ddi_inst_state.ins_busy)
		cv_wait(&e_ddi_inst_state.ins_serial_cv,
		    &e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 1;
	mutex_exit(&e_ddi_inst_state.ins_serial);
	while (dp) {
		ndp = dp->ind_next;
		dp->ind_next = e_ddi_inst_state.ins_no_major;
		e_ddi_inst_state.ins_no_major = dp;
		dp = ndp;
	}
	mutex_enter(&e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 0;
	cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
	mutex_exit(&e_ddi_inst_state.ins_serial);
}

/*
 * A new major has been added to the system.  Run through the orphan list
 * and try to attach each one to a driver's list.
 */
void
e_ddi_unorphan_instance_nos()
{
	in_drv_t *dp, *ndp;

	/*
	 * disconnect the orphan list, and call in_hashdrv for each item
	 * on it
	 */

	/*
	 * Only one thread is allowed to change the state of the instance
	 * number assignments on the system at any given time.
	 */
	mutex_enter(&e_ddi_inst_state.ins_serial);
	while (e_ddi_inst_state.ins_busy)
		cv_wait(&e_ddi_inst_state.ins_serial_cv,
		    &e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 1;
	mutex_exit(&e_ddi_inst_state.ins_serial);
	if (e_ddi_inst_state.ins_no_major == NULL) {
		mutex_enter(&e_ddi_inst_state.ins_serial);
		e_ddi_inst_state.ins_busy = 0;
		cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
		mutex_exit(&e_ddi_inst_state.ins_serial);
		return;
	}
	/*
	 * Make two passes through the data, skipping over those without
	 * instance no assignments the first time, then making the
	 * assignments the second time.  List should be shorter second
	 * time.  Note that if there is not a valid major number for the
	 * node, in_hashdrv will put it back on the no_major list without
	 * assigning an instance number.
	 */
	dp = e_ddi_inst_state.ins_no_major;
	e_ddi_inst_state.ins_no_major = NULL;
	while (dp) {
		ndp = dp->ind_next;
		if (dp->ind_state == IN_UNKNOWN) {
			dp->ind_next = e_ddi_inst_state.ins_no_instance;
			e_ddi_inst_state.ins_no_instance = dp;
		} else {
			dp->ind_next = NULL;
			in_hashdrv(dp);
		}
		dp = ndp;
	}
	dp = e_ddi_inst_state.ins_no_instance;
	e_ddi_inst_state.ins_no_instance = NULL;
	while (dp) {
		ndp = dp->ind_next;
		dp->ind_next = NULL;
		in_hashdrv(dp);
		dp = ndp;
	}
	mutex_enter(&e_ddi_inst_state.ins_serial);
	e_ddi_inst_state.ins_busy = 0;
	cv_broadcast(&e_ddi_inst_state.ins_serial_cv);
	mutex_exit(&e_ddi_inst_state.ins_serial);
}

static void
in_removenode(struct devnames *dnp, in_node_t *mp, in_node_t *ap)
{
	in_node_t *np;

	ASSERT(e_ddi_inst_state.ins_busy);
	/*
	 * Assertion: parents are always instantiated by the framework
	 * before their children, destroyed after them
	 */
	ASSERT(mp->in_child == NULL);
	/*
	 * Assertion: drv entries are always removed before their owning nodes
	 */
	ASSERT(mp->in_drivers == NULL);
	/*
	 * Take the node out of the tree
	 */
	if (ap->in_child == mp) {
		ap->in_child = mp->in_sibling;
		in_dealloc_node(mp);
		return;
	} else {
		for (np = ap->in_child; np; np = np->in_sibling) {
			if (np->in_sibling == mp) {
				np->in_sibling = mp->in_sibling;
				in_dealloc_node(mp);
				return;
			}
		}
	}
	cmn_err(CE_PANIC, "in_removenode dnp %p mp %p", (void *)dnp,
	    (void *)mp);
}

/*
 * Recursive ascent
 *
 * This now only does half the job.  It finds the node, then the caller
 * has to search the node for the binding name
 */
static in_node_t *
in_devwalk(dev_info_t *dip, in_node_t **ap)
{
	in_node_t *np;
	char *name;
	char *addr;

	ASSERT(dip);
	ASSERT(e_ddi_inst_state.ins_busy);
	if (dip == ddi_root_node()) {
		*ap = NULL;
		return (e_ddi_inst_state.ins_root);
	}
	/*
	 * call up to find parent, then look through the list of kids
	 * for a match
	 */
	np = in_devwalk(ddi_get_parent(dip), ap);
	if (np == NULL)
		return (np);
	*ap = np;
	np = np->in_child;
	name = ddi_node_name(dip);
	addr = ddi_get_name_addr(dip);

	while (np) {
		if (in_eqstr(np->in_node_name, name) &&
		    in_eqstr(np->in_unit_addr, addr)) {
			return (np);
		}
		np = np->in_sibling;
	}
	return (np);
}

/*
 * Create a node specified by cp and assign it the given instance no.
 */
static void
in_pathin(char *cp, uint_t instance, char *bname)
{
	in_node_t *np;
	in_drv_t *dp;

	ASSERT(e_ddi_inst_state.ins_busy);

#define	IGNORE_STORED_PSEUDO_INSTANCES
#ifdef IGNORE_STORED_PSEUDO_INSTANCES
	/*
	 * We should prevent pseudo devices from being placed in the
	 * instance tree by omitting all names beginning with /pseudo/.
	 * If they aren't removed, a new kernel with an old path_to_inst
	 * file will contain unnecessary entries in the instance tree,
	 * wasting memory, and they will be written out when the
	 * system is reconfigured (thus never going away). Upgrading the
	 * system might cause this to happen. Functionally, this isn't
	 * a problem, since the instance tree is never examined for
	 * pseudo devices.
	 *
	 * This uses ANSI C string concatenation to add the slashes
	 * to the nexus name, so DEVI_PSEUDO_NEXNAME better always be
	 * a macro for a constant string until this is no longer needed.
	 */
	if (strncmp(cp, "/" DEVI_PSEUDO_NEXNAME "/",
	    strlen(DEVI_PSEUDO_NEXNAME) + 2) == 0) {
		return;
	}
#endif

	np = in_make_path(cp);
	/*
	 * Solaris Clustering: This is needed to keep instances for
	 * different hosts in the same path_to_inst file.
	 * If the device node belongs to another host (its node number
	 * is different from ours), we do not want to create a path for
	 * it.  It would be a waste of space and it could never be used.
	 */
	if (np == NULL) {
		return;
	}
	if (bname == NULL) {
		if ((bname = i_binding_to_drv_name(np->in_node_name))
		    == NULL) {
			bname = np->in_node_name;
		}
	}
	if (in_inuse(instance, bname)) {
		if (bname != np->in_node_name) {
			cmn_err(CE_WARN,
			    "instance %d already in use, cannot be assigned "
			    "to '%s' (driver %s)", instance, cp, bname);
		} else {
			cmn_err(CE_WARN,
			    "instance %d already in use, cannot be assigned "
			    "to '%s'", instance, cp);
		}
		return;
	}
	dp = in_drvwalk(np, bname);
	if (dp == NULL) {
		dp = in_alloc_drv(bname);
		if (dp == NULL)
			cmn_err(CE_PANIC, "instance initialization");
		in_endrv(np, dp);
	}
	if (dp->ind_state == IN_PERMANENT) {
		if (bname != np->in_node_name) {	/* assignment above */
			cmn_err(CE_WARN,
			    "multiple instance number assignments for "
			    "'%s' (driver %s), %d used",
			    cp, bname, dp->ind_instance);
		} else {
			cmn_err(CE_WARN,
			    "multiple instance number assignments for "
			    "'%s', %d used", cp, dp->ind_instance);
		}
	} else {
		dp->ind_instance = instance;
		dp->ind_state = IN_PERMANENT;
	}
}

/*
 * Create (or find) the node named by path by recursively decending from the
 * root's first child (we ignore the root, which is never named)
 */
static in_node_t *
in_make_path(char *path)
{
	in_node_t *ap;		/* ancestor pointer */
	in_node_t *np;		/* working node pointer */
	in_node_t *rp;		/* return node pointer */
	char buf[MAXPATHLEN];	/* copy of string so we can change it */
	char *cp, *name, *addr;
	const char *pathprefix, *pathbase;

	ASSERT(e_ddi_inst_state.ins_busy);
	if (path == NULL || path[0] != '/')
		return (NULL);
	(void) strcpy(buf, path);
	cp = buf + 1;	/* skip over initial '/' in path */
	name = in_name_addr(&cp, &addr);

	/*
	 * Solaris Clustering: For non clustered system the code
	 * does nothing at all, in i_ddi_get_dpath_prefix() returns a
	 * null string.
	 *
	 * We want to make sure this node belongs to this host.
	 * Check to see if the host matches, if not just return NULL, or
	 * go into cluster install mode if there are no /node entries in
	 * the path_to_inst file at all.
	 */

	pathprefix = i_ddi_get_dpath_prefix();

	if (pathprefix && *pathprefix) {
		if (strncmp(path, pathprefix, strlen(pathprefix)) == 0 &&
		    path[strlen(pathprefix)] == '/') {
			/*
			 * Skip the node number, do not pollute the
			 * dev tree with the node number.
			 */
			cp = buf + strlen(pathprefix) + 1;
			name = in_name_addr(&cp, &addr);
		} else {
			pathbase = i_ddi_get_dpath_base();
			if (strncmp(path, pathbase, strlen(pathbase)) == 0) {

				/*
				 * Clustering: We need a method to be able to
				 * skip over the cluster-specific component of
				 * the device path, even when we can't get the
				 * full prefix due to the cluster configuration
				 * being corrupted. So, just skip over
				 * everything from the initial, constant,
				 * prefix, through the next "/".
				 */

				if ((cluster_bootflags & (CLUSTER_CONFIGURED |
				    CLUSTER_BOOTED)) == CLUSTER_CONFIGURED) {
					cp = buf + strlen(pathbase) + 1;
					while (cp[0] != '/') {
						cp = cp + 1;
					}
					cp = cp + 1;
					name = in_name_addr(&cp, &addr);
				} else {
					return (NULL);
				}

			} else {
				cluster_bootflags |= CLUSTER_INSTALLING;

				/*
				 * Set the reconfigure flag so that the /devices
				 * and /dev directories get rebuilt.
				 */
				boothowto |= RB_RECONFIG;
			}
		}
	}

	ap = e_ddi_inst_state.ins_root;
	rp = np = e_ddi_inst_state.ins_root->in_child;
	while (name) {
		while (name && np) {
			if (in_eqstr(name, np->in_node_name) &&
			    in_eqstr(addr, np->in_unit_addr)) {
				name = in_name_addr(&cp, &addr);
				if (name == NULL)
					return (np);
				ap = np;
				np = np->in_child;
				continue;
			} else {
				np = np->in_sibling;
			}
		}
		np = in_alloc_node(name, addr);
		in_enlist(ap, np);	/* insert into tree */
		rp = np;	/* value to return if we quit */
		ap = np;	/* new parent */
		np = NULL;	/* can have no children */
		name = in_name_addr(&cp, &addr);
	}
	return (rp);
}

/*
 * Insert node np into the tree as one of ap's children.
 */
static void
in_enlist(in_node_t *ap, in_node_t *np)
{
	in_node_t *mp;
	ASSERT(e_ddi_inst_state.ins_busy);
	/*
	 * Make this node some other node's child or child's sibling
	 */
	ASSERT(ap && np);
	if (ap->in_child == NULL) {
		ap->in_child = np;
	} else {
		for (mp = ap->in_child; mp; mp = mp->in_sibling)
			if (mp->in_sibling == NULL) {
				mp->in_sibling = np;
				break;
			}
	}
	np->in_parent = ap;
}

/*
 * Insert drv entry dp onto a node's driver list
 */
static void
in_endrv(in_node_t *np, in_drv_t *dp)
{
	in_drv_t *mp;
	ASSERT(e_ddi_inst_state.ins_busy);
	ASSERT(np && dp);
	mp = np->in_drivers;
	np->in_drivers = dp;
	dp->ind_next_drv = mp;
	dp->ind_node = np;
}

/*
 * Parse the next name out of the path, null terminate it and update cp.
 * caller has copied string so we can mess with it.
 * Upon return *cpp points to the next section to be parsed, *addrp points
 * to the current address substring (or NULL if none) and we return the
 * current name substring (or NULL if none).  name and address substrings
 * are null terminated in place.
 */

static char *
in_name_addr(char **cpp, char **addrp)
{
	char *namep;	/* return value holder */
	char *ap;	/* pointer to '@' in string */
	char *sp;	/* pointer to '/' in string */

	if (*cpp == NULL || **cpp == '\0') {
		*addrp = NULL;
		return (NULL);
	}
	namep = *cpp;
	sp = strchr(*cpp, '/');
	if (sp != NULL) {	/* more to follow */
		*sp = '\0';
		*cpp = sp + 1;
	} else {		/* this is last component. */
		*cpp = NULL;
	}
	ap = strchr(namep, '@');
	if (ap == NULL) {
		*addrp = NULL;
	} else {
		*ap = '\0';		/* terminate the name */
		*addrp = ap + 1;
	}
	return (namep);
}

/*
 * Allocate a node and storage for name and addr strings, and fill them in.
 */
static in_node_t *
in_alloc_node(char *name, char *addr)
{
	in_node_t *np;
	char *cp;
	size_t namelen;

	ASSERT(e_ddi_inst_state.ins_busy);
	/*
	 * Has name or will become root
	 */
	ASSERT(name || e_ddi_inst_state.ins_root == NULL);
	if (addr == NULL)
		addr = "";
	if (name == NULL)
		namelen = 0;
	else
		namelen = strlen(name) + 1;
	cp = kmem_zalloc(sizeof (in_node_t) + namelen + strlen(addr) + 1,
	    KM_SLEEP);
	np = (in_node_t *)cp;
	if (name) {
		np->in_node_name = cp + sizeof (in_node_t);
		(void) strcpy(np->in_node_name, name);
	}
	np->in_unit_addr = cp + sizeof (in_node_t) + namelen;
	(void) strcpy(np->in_unit_addr, addr);
	return (np);
}

/*
 * Allocate a drv entry and storage for binding name string, and fill it in.
 */
static in_drv_t *
in_alloc_drv(char *bindingname)
{
	in_drv_t *dp;
	char *cp;
	size_t namelen;

	ASSERT(e_ddi_inst_state.ins_busy);
	/*
	 * Has name or will become root
	 */
	ASSERT(bindingname || e_ddi_inst_state.ins_root == NULL);
	if (bindingname == NULL)
		namelen = 0;
	else
		namelen = strlen(bindingname) + 1;
	cp = kmem_zalloc(sizeof (in_drv_t) + namelen, KM_SLEEP);
	dp = (in_drv_t *)cp;
	if (bindingname) {
		dp->ind_driver_name = cp + sizeof (in_drv_t);
		(void) strcpy(dp->ind_driver_name, bindingname);
	}
	dp->ind_state = IN_UNKNOWN;
	dp->ind_instance = -1;
	return (dp);
}

static void
in_dealloc_node(in_node_t *np)
{
	/*
	 * The root node can never be de-allocated
	 */
	ASSERT(np->in_node_name && np->in_unit_addr);
	ASSERT(e_ddi_inst_state.ins_busy);
	kmem_free(np, sizeof (in_node_t) + strlen(np->in_node_name)
	    + strlen(np->in_unit_addr) + 2);
}

static void
in_dealloc_drv(in_drv_t *dp)
{
	ASSERT(dp->ind_driver_name);
	ASSERT(e_ddi_inst_state.ins_busy);
	kmem_free(dp, sizeof (in_drv_t) + strlen(dp->ind_driver_name)
	    + 1);
}

/*
 * Handle the various possible versions of "no address"
 */
static int
in_eqstr(char *a, char *b)
{
	if (a == b)	/* covers case where both are nulls */
		return (1);
	if (a == NULL && *b == 0)
		return (1);
	if (b == NULL && *a == 0)
		return (1);
	if (a == NULL || b == NULL)
		return (0);
	return (strcmp(a, b) == 0);
}

/*
 * Returns true if instance no. is already in use by named driver
 */
static int
in_inuse(uint_t instance, char *name)
{
	major_t major;
	in_drv_t *dp;
	struct devnames *dnp;

	ASSERT(e_ddi_inst_state.ins_busy);
	/*
	 * For now, if we've never heard of this device we assume it is not
	 * in use, since we can't tell
	 * XXX could do the weaker search through the nomajor list checking
	 * XXX for the same name
	 */
	if ((major = ddi_name_to_major(name)) == (major_t)-1)
		return (0);
	dnp = &devnamesp[major];

	dp = dnp->dn_inlist;
	while (dp) {
		if (dp->ind_instance == instance)
			return (1);
		dp = dp->ind_next;
	}
	return (0);
}

void
in_hashdrv(in_drv_t *dp)
{
	struct devnames *dnp;
	in_drv_t *mp, *pp;
	major_t major;

	if ((major = ddi_name_to_major(dp->ind_driver_name)) == (major_t)-1) {
		dp->ind_next = e_ddi_inst_state.ins_no_major;
		e_ddi_inst_state.ins_no_major = dp;
		return;
	}
	dnp = &devnamesp[major];

	if (dp->ind_state == IN_UNKNOWN) {
		dp->ind_instance = in_next_instance(major, dp, 0);
		dp->ind_state = IN_PERMANENT;
	}

	dnp->dn_instance = IN_SEARCHME;
	pp = mp = dnp->dn_inlist;
	if (mp == NULL || dp->ind_instance < mp->ind_instance) {
		dp->ind_next = mp;
		dnp->dn_inlist = dp;
	} else {
		ASSERT(mp->ind_instance != dp->ind_instance);
		while (mp->ind_instance < dp->ind_instance && mp->ind_next) {
			pp = mp;
			mp = mp->ind_next;
			ASSERT(mp->ind_instance != dp->ind_instance);
		}
		if (mp->ind_instance < dp->ind_instance) { /* end of list */
			dp->ind_next = NULL;
			mp->ind_next = dp;
		} else {
			dp->ind_next = pp->ind_next;
			pp->ind_next = dp;
		}
	}
}

/*
 * Remove a driver entry from the list, given a previous pointer
 */
void
in_removedrv(struct devnames *dnp, in_drv_t *mp)
{
	in_drv_t *dp;
	in_drv_t *prevp;

	if (dnp->dn_inlist == mp) {	/* head of list */
		dnp->dn_inlist = mp->ind_next;
		dnp->dn_instance = IN_SEARCHME;
		in_dq_drv(mp);
		in_dealloc_drv(mp);
		return;
	}
	prevp = dnp->dn_inlist;
	for (dp = prevp->ind_next; dp; dp = dp->ind_next) {
		if (dp == mp) {		/* found it */
			dnp->dn_instance = IN_SEARCHME;
			prevp->ind_next = mp->ind_next;
			in_dq_drv(mp);
			in_dealloc_drv(mp);
			return;
		}
		prevp = dp;
	}
	cmn_err(CE_PANIC, "in_removedrv dnp %p mp %p", (void *)dnp, (void *)mp);
}

static void
in_dq_drv(in_drv_t *mp)
{
	struct in_node *node = mp->ind_node;
	in_drv_t *ptr, *prev;

	if (mp == node->in_drivers) {
		node->in_drivers = mp->ind_next_drv;
		return;
	}
	prev = node->in_drivers;
	for (ptr = prev->ind_next_drv; ptr != (struct in_drv *)NULL;
	    ptr = ptr->ind_next_drv) {
		if (ptr == mp) {
			prev->ind_next_drv = ptr->ind_next_drv;
			return;
		}
	}
	cmn_err(CE_PANIC, "in_dq_drv: in_drv not found on node driver list");
}


in_drv_t *
in_drvwalk(in_node_t *np, char *binding_name)
{
	char *name;
	in_drv_t *dp = np->in_drivers;
	while (dp) {
		if ((name = i_binding_to_drv_name(dp->ind_driver_name))
		    == NULL) {
			name = dp->ind_driver_name;
		}
		if (strcmp(binding_name, name) == 0) {
			break;
		}
		dp = dp->ind_next_drv;
	}
	return (dp);
}



static void
i_log_devfs_instance_mod(void)
{
	log_event_tuple_t	tuples[7];
	int			argc = 0;

	tuples[argc].attr = LOGEVENT_CLASS;
	tuples[argc++].val = EC_DEVFS;
	tuples[argc].attr = LOGEVENT_TYPE;
	tuples[argc++].val = ET_DEVFS_INSTANCE_MOD;

	if (i_ddi_log_event(argc, tuples, KM_NOSLEEP) != DDI_SUCCESS)
		cmn_err(CE_WARN, "i_log_devfs_instance_mod: failed log_event");
}
