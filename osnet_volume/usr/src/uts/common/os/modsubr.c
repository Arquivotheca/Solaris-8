/*
 * Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)modsubr.c	1.82	99/08/23 SMI"

#include <sys/param.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include <sys/pathname.h>
#include <sys/vnode.h>
#include <sys/ddi_impldefs.h>
#include <sys/esunddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/systeminfo.h>
#include <sys/hwconf.h>
#include <sys/file.h>
#include <sys/varargs.h>
#include <sys/thread.h>
#include <sys/cred.h>
#include <sys/autoconf.h>
#include <sys/kobj.h>
#include <sys/consdev.h>
#include <sys/dc_ki.h>
#include <sys/cladm.h>
#include <sys/systm.h>
#include <sys/debug.h>

extern struct dev_ops nodev_ops;
extern struct dev_ops mod_nodev_ops;

extern int servicing_interrupt(void);

struct mod_noload {
	struct mod_noload *mn_next;
	char *mn_name;
};

/*
 * Function prototypes
 */
int match_parent(dev_info_t *, char *, char *);

static int init_stubs(struct modctl *, struct mod_modinfo *);
static void append(struct hwc_spec *, struct par_list *);
static void add_spec(struct hwc_spec *, struct par_list **);
static void impl_free_parlist(struct devnames *);
static void make_children(dev_info_t *, char *, struct par_list *, int *,
    mta_handle_t *);
static int nm_hash(char *);
static void impl_attach_child_devinfos(struct par_list *, int *,
    mta_handle_t *);
static void make_syscallname(char *, int);

struct dev_ops *
mod_hold_dev_by_major(major_t major)
{
	struct dev_ops **devopspp, *ops;
	int loaded;
	char *drvname;

	ASSERT((unsigned)major < devcnt);
	LOCK_DEV_OPS(&(devnamesp[major].dn_lock));
	devopspp = &devopsp[major];
	loaded = 1;
	while (loaded && !CB_DRV_INSTALLED(*devopspp)) {
		UNLOCK_DEV_OPS(&(devnamesp[major].dn_lock));
		drvname = mod_major_to_name(major);
		if (drvname == NULL)
			return (NULL);
		loaded = (modload("drv", drvname) != -1);
		LOCK_DEV_OPS(&(devnamesp[major].dn_lock));
	}
	if (loaded) {
		INCR_DEV_OPS_REF(*devopspp);
		ops = *devopspp;
	} else {
		ops = NULL;
	}
	UNLOCK_DEV_OPS(&(devnamesp[major].dn_lock));
	return (ops);
}

#ifdef	DEBUG_RELE
static int mod_rele_pause = DEBUG_RELE;
#endif	DEBUG_RELE

void
mod_rele_dev_by_major(major_t major)
{
	struct dev_ops *ops;
	struct devnames *dnp;

	ASSERT((unsigned)major < devcnt);
	dnp = &devnamesp[major];
	LOCK_DEV_OPS(&dnp->dn_lock);
	ops = devopsp[major];
	ASSERT(CB_DRV_INSTALLED(ops));

#ifdef	DEBUG_RELE
	if (!DEV_OPS_HELD(ops))  {
		char *s;
		static char *msg = "mod_rele_dev_by_major: unheld driver!";

		printf("mod_rele_dev_by_major: Major dev <%u>, name <%s>\n",
		    (uint_t)major,
		    (s = mod_major_to_name(major)) ? s : "unknown");
		if (mod_rele_pause)
			debug_enter(msg);
		else
			printf("%s\n", msg);
		UNLOCK_DEV_OPS(&dnp->dn_lock);
		return;			/* XXX: Note changed behaviour */
	}

#endif	DEBUG_RELE

	if (!DEV_OPS_HELD(ops)) {
		cmn_err(CE_PANIC,
		    "mod_rele_dev_by_major: Unheld driver: major number <%u>",
		    (uint_t)major);
	}
	DECR_DEV_OPS_REF(ops);
	UNLOCK_DEV_OPS(&dnp->dn_lock);
}

struct dev_ops *
mod_hold_dev_by_devi(dev_info_t *devi)
{
	major_t major;
	char *name;

	name = ddi_get_name(devi);
	if ((major = mod_name_to_major(name)) == (major_t)-1)
		return (NULL);
	return (mod_hold_dev_by_major(major));
}

void
mod_rele_dev_by_devi(dev_info_t *devi)
{
	major_t major;
	char *name;

	name = ddi_get_name(devi);
	if ((major = mod_name_to_major(name)) == (major_t)-1)
		return;
	mod_rele_dev_by_major(major);
}

int
nomod_zero()
{
	return (0);
}

int
nomod_minus_one()
{
	return (-1);
}

int
nomod_einval()
{
	return (EINVAL);
}

/*
 * Install all the stubs for a module.
 * Return zero if there were no errors or an errno value.
 */
int
install_stubs_by_name(struct modctl *modp, char *name)
{
	char *p;
	char *filenamep;
	char namebuf[MODMAXNAMELEN + 12];
	struct mod_modinfo *mp;

	p = name;
	filenamep = name;

	while (*p)
		if (*p++ == '/')
			filenamep = p;

	/*
	 * Concatenate "name" with "_modname" then look up this symbol
	 * in the kernel.  If not found, we're done.
	 * If found, then find the "mod" info structure and call init_stubs().
	 */
	p = namebuf;

	while (*filenamep && *filenamep != '.')
		*p++ = *filenamep++;

	(void) strcpy(p, "_modinfo");

	if ((mp = (struct mod_modinfo *)modgetsymvalue(namebuf, 1)) != 0)
		return (init_stubs(modp, mp));
	else
		return (0);
}

static int
init_stubs(struct modctl *modp, struct mod_modinfo *mp)
{
	struct mod_stub_info *sp;
	int i;
	ulong_t offset;
	uintptr_t funcadr;
	char *funcname;

	modp->mod_modinfo = mp;

	/*
	 * Fill in all stubs for this module.  We can't be lazy, since
	 * some calls could come in from interrupt level, and we
	 * can't modlookup then (symbols may be paged out).
	 */
	sp = mp->modm_stubs;
	for (i = 0; sp->mods_func_adr; i++, sp++) {
		funcname = modgetsymname(sp->mods_stub_adr, &offset);
		if (funcname == NULL) {
		    printf("init_stubs: couldn't find symbol in module %s\n",
			mp->modm_module_name);
			return (EFAULT);
		}
		funcadr = kobj_lookup(modp->mod_mp, funcname);

		if (kobj_addrcheck(modp->mod_mp, (caddr_t)funcadr)) {
			printf("%s:%s() not defined properly\n",
				mp->modm_module_name, funcname);
			return (EFAULT);
		}
		sp->mods_func_adr = funcadr;
	}
	mp->mp = modp;
	return (0);
}

void
reset_stubs(struct modctl *modp)
{
	struct mod_stub_info *stub;

	if (modp->mod_modinfo) {
		for (stub = modp->mod_modinfo->modm_stubs;
		    stub->mods_func_adr; stub++) {
			if (stub->mods_flag & (MODS_WEAK | MODS_NOUNLOAD))
				stub->mods_func_adr =
				    (uintptr_t)stub->mods_errfcn;
			else
				stub->mods_func_adr =
				    (uintptr_t)mod_hold_stub;
		}
		modp->mod_modinfo->mp = NULL;
	}
}


struct modctl *
mod_getctl(struct modlinkage *modlp)
{
	struct modctl *modp;

	mutex_enter(&mod_lock);
	for (modp = modules.mod_next; modp != &modules;
	    modp = modp->mod_next) {
		if (modp->mod_linkage == modlp) {
			ASSERT(modp->mod_busy);
			break;
		}
	}
	mutex_exit(&mod_lock);
	if (modp == &modules)
		modp = NULL;
	return (modp);
}

static void
append(struct hwc_spec *spec, struct par_list *par)
{
	struct hwc_spec *hwc, *last;

	ASSERT(par->par_specs);
	for (hwc = par->par_specs; hwc; hwc = hwc->hwc_next) {
		last = hwc;
	}
	last->hwc_next = spec;
}

/*
 * Chain together specs whose parent's module name is the same.
 */

static void
add_spec(struct hwc_spec *spec, struct par_list **par)
{
	major_t maj;
	char *p;
	struct par_list *pl, *par_last = NULL;
	char *parent = spec->hwc_parent_name;

	if ((parent == NULL) && (spec->hwc_class_name == NULL)) {
		cmn_err(CE_WARN, "add_spec: No class or parent specified");
		hwc_free(spec);
		return;
	}

	maj = (major_t)-1;


	/*
	 * If given a parent=/full-pathname, see if the platform
	 * can resolve the pathname to driver, otherwise, try
	 * the leaf node name.
	 */
	if (parent) {
		if (*parent == '/') {
			if ((p = i_path_to_drv(parent)) != 0) {
				maj = ddi_name_to_major(p);
			}
		}

		/*
		 * If that didn't resolve the driver name, the component we
		 * want is in between the last '/' (or beginning of string,
		 * if there is no '/') and the first '@' in hwc_parent_name.
		 */
		if (maj == (major_t)-1) {
			if (*parent == '/') {
				parent = strrchr(parent, '/') + 1;
			}
			if ((p = strchr(parent, '@')) != 0) {
				/* temporarily, end the string here */
				*p = '\0';
			}
			maj = ddi_name_to_major(parent);
			if (p) {
				/* restore the string we changed */
				*p = '@';
			}
		}

		if (maj == (major_t)-1) {
			cmn_err(CE_WARN,
			    "add_spec: No major number for %s", parent);
			hwc_free(spec);
			return;
		}
	}

	/*
	 * Scan the list looking for a matching parent.
	 */
	for (pl = *par; pl; pl = pl->par_next) {
		if (maj == pl->par_major) {
			append(spec, pl);
			return;
		}
		par_last = pl;
	}

	/*
	 * Didn't find a match on the list.  Make a new parent list.
	 */
	pl = kmem_zalloc(sizeof (*pl), KM_SLEEP);
	pl->par_major = maj;
	if (maj == (major_t)-1 && par_last != NULL) {
		/* put "class=" entries last (lower pri if dups) */
		par_last->par_next = pl;
	} else {
		pl->par_next = *par;
		*par = pl;
	}
	pl->par_specs = spec;
}

/*
 * Sort a list of hardware conf specifications by parent module name.
 */
struct par_list *
sort_hwc(struct hwc_spec *hwc)
{
	struct hwc_spec *spec, *list = hwc;
	struct par_list *pl = NULL;

	while (list) {
		spec = list;
		list = list->hwc_next;
		spec->hwc_next = NULL;
		add_spec(spec, &pl);
	}
	return (pl);
}

#ifdef HWC_DEBUG
static void
print_hwc(struct hwc_spec *hwc)
{
	struct hwc_spec *spec;

	printf("parent %s\n", hwc->hwc_parent_name);
	while (hwc) {
		printf("\tchild %s\n",
			hwc->hwc_proto->proto_devi_name));
		hwc = hwc->hwc_next;
	}
}

static void
print_par(struct par_list *par)
{
	while (par) {
		print_hwc(par->par_specs);
		par = par->par_next;
	}
}
#endif /* HWC_DEBUG */

/*
 * gather_globalprops is passed a list of entries created from the
 * driver.conf(4) file of a driver. If an entry does not have
 * a name, then it does not identify a possible instance of a device,
 * and instead holds global driver properties. The properties on such
 * nodes are moved to the devnames structure of the driver, and the
 * original property node is removed. gather_globalprops returns the list
 * of entries with all driver global property nodes removed.
 */
static struct hwc_spec *
gather_globalprops(struct devnames *dnp, struct hwc_spec *hwc)
{
	struct hwc_spec *tmp = NULL;
	struct hwc_spec *devinfo_hwc = NULL;	/* returned list */
	struct hwc_spec *devinfo_hwc_tail = NULL;
	struct hwc_spec *global_props_hwc;	/* working property entry */
	ddi_prop_t	*global_propp_tail;	/* end of property list */
	ddi_prop_t	*node_propp;		/* node system property list */

	global_props_hwc = NULL;
	global_propp_tail = dnp->dn_global_prop_ptr;
	tmp = hwc;
	while (tmp != NULL) {
		/*
		 * The presence of a name means that this entry in the
		 * driver.conf(4) file refers to a possible device
		 * instance.
		 *
		 * The absence of a name indicates that this entry
		 * is a list of driver global properties.
		 */
		if (tmp->hwc_proto->proto_devi_name != NULL) {
			/*
			 * This is a prototype devinfo node, not a
			 * property node. Move it to the list
			 * of prototype devinfo nodes that will be
			 * returned, preserving order.
			 */
			if (devinfo_hwc == NULL) {
				devinfo_hwc = tmp;
				tmp = tmp->hwc_next;
				devinfo_hwc_tail = devinfo_hwc;
				devinfo_hwc_tail->hwc_next = NULL;
			} else {
				devinfo_hwc_tail->hwc_next = tmp;
				tmp = tmp->hwc_next;
				devinfo_hwc_tail->hwc_next->hwc_next = NULL;
				devinfo_hwc_tail = devinfo_hwc_tail->hwc_next;
			}
			continue;
		}
		/*
		 * This node contains driver global properties.
		 * It will not be added to the list of entries
		 * which will be returned.
		 */
		global_props_hwc = tmp;
		tmp = tmp->hwc_next;
		global_props_hwc->hwc_next = NULL;

		/*
		 * The global properties are created as normal system
		 * properties by the parser. They are on the system property
		 * list of the prototype devinfo node for this entry.
		 *
		 * Move the entire list to the end of the current
		 * global property list. This also happens to preserve the
		 * property order that was listed in the driver.conf(4) file,
		 * though this is not documented.
		 */
		node_propp =
			global_props_hwc->hwc_proto->proto_devi_sys_prop_ptr;
		if (node_propp) {
			/*
			 * Move the nodes list to the end of the driver
			 * global property list on the devnames structure.
			 */
			if (global_propp_tail == NULL) {
				dnp->dn_global_prop_ptr = node_propp;
				global_propp_tail = node_propp;
			} else {
				global_propp_tail->prop_next = node_propp;
			}
			/* Find the end of the list for the next set. */
			while (global_propp_tail->prop_next != NULL)
				global_propp_tail =
				    global_propp_tail->prop_next;

			/* Finally, remove these properties from the node */
			global_props_hwc->hwc_proto->proto_devi_sys_prop_ptr =
			    NULL;
		}

		hwc_free(global_props_hwc);
	}

	return (devinfo_hwc);
}

/*
 * Delete all the global properties of the driver.
 * Called when the driver is unloaded.
 */
static void
ddi_drv_remove_globalprops(struct devnames *dnp)
{
	ddi_prop_t	*freep;
	ddi_prop_t	**list_head = &(dnp->dn_global_prop_ptr);
	ddi_prop_t	*propp = *list_head;

	/* remove default properties */
	while (propp != NULL)  {
		freep = propp;
		propp = propp->prop_next;
		kmem_free(freep->prop_name,
		    (size_t)(strlen(freep->prop_name) + 1));
		if (freep->prop_len != 0)
			kmem_free(freep->prop_val, (size_t)(freep->prop_len));
		kmem_free(freep, sizeof (ddi_prop_t));
	}

	*list_head = NULL;
}

/*
 * Assumes major is valid.
 */
struct par_list *
impl_make_parlist(major_t major)
{
	struct hwc_spec *hp;
	struct par_list *pl = NULL;
	char *confname;
	struct devnames *dnp;

	dnp = &devnamesp[major];

	ASSERT((DN_BUSY_CHANGING(dnp->dn_flags)) &&
	    (dnp->dn_busy_thread == curthread));

	/*
	 * If we have already looked at the .conf file, return the result
	 * stored in dn_pl.  May return NULL.
	 */
	if (dnp->dn_flags & DN_CONF_PARSED)
		return (dnp->dn_pl);

	confname = kmem_alloc(MAXNAMELEN, KM_SLEEP);
	(void) sprintf(confname, "drv/%s.conf", mod_major_to_name(major));
	hp = hwc_parse(confname);
	kmem_free(confname, MAXNAMELEN);
	if (hp != NULL) {
		/*
		 * hwc_parse has the side-effect of possibly creating nodes
		 * containing driver global properties. These properties must
		 * be moved to the devnames structure and the extra entries
		 * removed from the hwc_spec list.
		 */
		hp = gather_globalprops(dnp, hp);

		/*
		 * Sort the specs by parent.
		 */
		pl = sort_hwc(hp);

		dnp->dn_flags |= DN_CONF_PARSED;
	}
	dnp->dn_pl = pl;
	return (pl);
}

/*
 * Free up the memory from the lists.
 */
static void
impl_free_parlist(struct devnames *dnp)
{
	impl_delete_par_list(dnp->dn_pl);
	dnp->dn_pl = NULL;
	dnp->dn_flags &= ~DN_CONF_PARSED;
}

/*
 * Make dev_info nodes for the named driver.
 * This is an internal function only called from ddi_hold_installed_driver.
 * This is effectively part of ddi_hold_installed_driver.  Do not call
 * this directly, call ddi_hold_installed_driver instead.
 */
int
impl_make_devinfos(major_t major)
{
	struct par_list *pl, *saved_pl;
	struct devnames *dnp;
	int circular;
	int unit = 0;
	mta_handle_t *mta;

	ASSERT((unsigned)major < devcnt);
	dnp = &devnamesp[major];
	circular = dnp->dn_circular;

	/*
	 * Read the .conf file, if it has not already been done.
	 * Note that impl_make_parlist will only make the list once.
	 */
	saved_pl = impl_make_parlist(major);

	/*
	 * Get mta handle. If NULL, no mt_attach.
	 */
	mta = mta_get_handle(major);

	/*
	 * Walk the tree making children wherever they belong, and
	 * probe/attach them.  (That's the only way to tell they're real)
	 */
	for (pl = saved_pl; pl != NULL; pl = pl->par_next)  {
		if (pl->par_major == (major_t)-1) {
			struct par_list *ppl;
			struct par_list *new_pl = impl_replicate_class_spec(pl);

			/*
			 * this is a par list with class spec's.
			 * expand it
			 */
			for (ppl = new_pl; ppl != NULL; ppl = ppl->par_next) {
				impl_attach_child_devinfos(ppl, &unit, mta);
			}
			impl_delete_par_list(new_pl);

		} else {
			impl_attach_child_devinfos(pl, &unit, mta);
		}
	}

	if (mta) {
		mta_attach_devi_list(mta);
	}

	if ((saved_pl) && (!circular))
		dnp->dn_flags |= DN_DEVI_MADE;

	return (DDI_SUCCESS);
}

/*
 * For each parent, make sure it exists, and if it does, make child devinfos
 * for it and attach drivers using make_children().
 *
 * When mta != NULL, child devinfos are not "attached" upon return from this
 * function. impl_make_devinfos() will lauch multiple threads to attach
 * devinfos to make the process faster.
 */
static void
impl_attach_child_devinfos(struct par_list *pl, int *unit, mta_handle_t *mta)
{
	major_t major = pl->par_major;
	dev_info_t *pdevi;

	if (ddi_hold_installed_driver(major) == NULL) {
		return;
	}

	for (pdevi = devnamesp[major].dn_head; pdevi != NULL;
	    pdevi = ddi_get_next(pdevi))  {
		mutex_enter(&DEVI(pdevi)->devi_lock);
		if ((DDI_CF2(pdevi)) && (DEVI_IS_DEVICE_OFFLINE(pdevi) == 0)) {
			mutex_exit(&DEVI(pdevi)->devi_lock);
			make_children(pdevi, devnamesp[major].dn_name,
			    pl, unit, mta);
		} else {
			mutex_exit(&DEVI(pdevi)->devi_lock);
		}
	}
	ddi_rele_driver(pl->par_major);
}

void
copy_prop(ddi_prop_t *propp, ddi_prop_t **cpropp)
{
	ddi_prop_t *dpp, *cdpp, *cdpp_prev, *pp, *ppprev;

	cdpp_prev = NULL;
	for (dpp = propp; dpp != NULL; dpp = dpp->prop_next) {
		cdpp = kmem_zalloc(sizeof (struct ddi_prop), KM_SLEEP);
		if (cdpp_prev != NULL)
			cdpp_prev->prop_next = cdpp;
		else {
			if (*cpropp == NULL)
				*cpropp = cdpp;
			else {
				pp = *cpropp;
				while (pp != NULL) {
					ppprev = pp;
					pp = pp->prop_next;
				}
				ppprev->prop_next = cdpp;
			}
		}
		cdpp->prop_dev = dpp->prop_dev;
		cdpp->prop_flags = dpp->prop_flags;
		if (dpp->prop_name != NULL) {
			cdpp->prop_name = kmem_zalloc(strlen(dpp->prop_name) +
				1, KM_SLEEP);
			(void) strcpy(cdpp->prop_name, dpp->prop_name);
		}
		if ((cdpp->prop_len = dpp->prop_len) != 0) {
			cdpp->prop_val = kmem_zalloc(dpp->prop_len, KM_SLEEP);
			bcopy(dpp->prop_val, cdpp->prop_val, dpp->prop_len);
		}
		cdpp_prev = cdpp;
	}
}

/*
 * match_parent
 *
 * See if the parent name matches the dev_info node.
 * The parent name can be NULL -> rootnexus,
 * a full OBP pathname ( eg. /sbus@x,y/foo@a,b/...),
 * a dev_info node name (eg. foo@a,b),
 * or a simple name (eg. foo).
 *
 * We assert that the parent must be properly held so it is safe to
 * search this branch of the tree.
 */

int
match_parent(dev_info_t *devi, char *devi_driver_name, char *par_name)
{
	char pn[MAXNAMELEN], *ch, *devi_node_name;

	ASSERT(ddi_get_driver(devi));
	ASSERT(DEV_OPS_HELD(ddi_get_driver(devi)));

	/*
	 * If the parent name starts with '/',
	 * the entire pathname must be an exact match.
	 */
	if (*par_name == '/')
		return (strcmp(par_name, ddi_pathname(devi, pn)) == 0);
	/*
	 * If the parent name contains an '@', the devinfo name must match.
	 * We match the parent nodename@address or drivername@address,
	 * (the parent can be specified as either nodename@address or
	 * drivername@address)
	 */
	for (ch = par_name; *ch != '\0'; ch++) {
		if (*ch == '@') {
			extern char *i_ddi_parname(dev_info_t *, char *);

			/*
			 * Both ddi_deviname and i_ddi_parname return the
			 * name with a preceding '/' which we need to skip
			 * over before comparing.
			 */
			devi_node_name = ddi_deviname(devi, pn);
			if (strcmp(par_name, ++devi_node_name) == 0)
				return (1);

			devi_node_name = i_ddi_parname(devi, pn);
			return (strcmp(par_name, ++devi_node_name) == 0);
		}
	}
	/*
	 * it's a match if it matches the driver name field.
	 */
	if (strcmp(par_name, ddi_binding_name(devi)) == 0)
		return (1);

	/*
	 * handle aliases
	 */
	return (strcmp(par_name, devi_driver_name) == 0);
}

/*
 * Scan the list of parents looking for a match with this dev_info node.
 * If there's a match, then for every spec on the parent list, make a
 * child of this node based on the spec.
 *
 * When multi-threaded attach is enabled (mta != NULL), child nodes are
 * initialized with instance assigned. impl_make_devinfos() will launch
 * threads to probe/attach the child nodes.
 */
static void
make_children(dev_info_t *devi, char *devi_driver_name,
    struct par_list *pl, int *unitp, mta_handle_t *mta)
{
	struct hwc_spec *hp;
	dev_info_t *cdip;

	/* class specs should have been replicated by now */
	ASSERT(pl->par_major != (major_t)-1);

	/*
	 * make a child for every spec whose parent name matches
	 */
	for (hp = pl->par_specs; hp != NULL; hp = hp->hwc_next) {
		if (match_parent(devi, devi_driver_name,
		    hp->hwc_parent_name)) {
			cdip = ddi_add_child(devi,
				hp->hwc_proto->proto_devi_name,
				DEVI_PSEUDO_NODEID, (*unitp)++);
			/*
			 * grab devi_lock before copying properties
			 */
			mutex_enter(&(DEVI(cdip)->devi_lock));
			copy_prop(hp->hwc_proto->proto_devi_sys_prop_ptr,
			    &(DEVI(cdip)->devi_sys_prop_ptr));
			mutex_exit(&(DEVI(cdip)->devi_lock));

			if (impl_check_cpu(cdip) != DDI_SUCCESS) {
				(void) ddi_remove_child(cdip, 0);
				(*unitp)--;	/* rescind hint */
				continue;
			}

			/*
			 * Initialize node and assign driver instance.
			 *
			 * This must be done in serial to keep device
			 * configuration the same with and without
			 * multi-threaded driver attachment.
			 */
			if (impl_initnode(cdip) != DDI_SUCCESS)
				continue;

			if (mta == NULL) {
				(void) impl_initdev(cdip);
				continue;
			}

			/*
			 * Queue the dip for probe-attach
			 */
			mta_add_dip(mta, cdip);
		}
	}
}

/*
 * Probe/attach all (hw) devinfo nodes belonging to this driver.
 */

void
attach_driver_to_hw_nodes(major_t major, struct dev_ops *ops)
{
	dev_info_t *devi, *ndevi;
	struct devnames *dnp = &(devnamesp[major]);

#ifdef lint
	ndevi = NULL;	/* See 1094364 */
#endif

	/* Assure the dev_ops has been prevented from being unloaded */
	ASSERT((unsigned)major < devcnt);
	ops = devopsp[major];
	ASSERT(DEV_OPS_HELD(ops));

	for (devi = dnp->dn_head; devi != NULL; devi = ndevi) {
		ndevi = ddi_get_next(devi);
		/*
		 * Added clause to skip s/w nodes, which was redundant
		 */
		mutex_enter(&(DEVI(devi)->devi_lock));
		if (((!DDI_CF2(devi)) || DDI_DRV_UNLOADED(devi)) &&
		    (DEVI_IS_DEVICE_OFFLINE(devi) == 0) &&
		    ndi_dev_is_persistent_node(devi)) {
			mutex_exit(&(DEVI(devi)->devi_lock));
			if (impl_proto_to_cf2(devi) == DDI_SUCCESS)
				mod_rele_dev_by_devi(devi);
		} else {
			mutex_exit(&(DEVI(devi)->devi_lock));
		}
	}
}

/*
 * This function cleans up any driver specific data from hw protoype
 * devinfo nodes. We never delete these nodes from the device tree
 * so we do our best to reset them here.
 */
void
impl_unattach_driver(major_t major)
{
	dev_info_t *devi;

	for (devi = devnamesp[major].dn_head; devi != NULL;
	    devi = ddi_get_next(devi))
		ddi_set_driver(devi, NULL);
	ddi_drv_remove_globalprops(&devnamesp[major]);
	impl_free_parlist(&devnamesp[major]);
}

void
impl_unattach_devs(major_t major)
{
	struct devnames *dnp = &(devnamesp[major]);
	dev_info_t *dip, *ndip;

#ifdef lint
	ndip = NULL;	/* See 1094364 */
#endif

	/*
	 * The per-driver lock is held and all instances of this
	 * driver are already detached.  Here we restore the state
	 * of an unloaded, unattached driver by destroying pseudo
	 * devinfo nodes and unaming prom devinfo nodes, so they are
	 * back in prototype form.
	 */
	for (dip = dnp->dn_head; dip != NULL; dip = ndip)  {
		ndip = ddi_get_next(dip);
		/*
		 * Undo ddi_initchild...
		 */
		(void) ddi_uninitchild(dip);
		/*
		 * Remove pseudo nodeids, letting ddi_remove_child
		 * `know' that we already hold the per-driver lock.
		 */
		if (ndi_dev_is_persistent_node(dip) == 0)
			(void) ddi_remove_child(dip, 1);
	}
	dnp->dn_flags &= ~DN_DEVI_MADE;
}

struct bind *mb_hashtab[MOD_BIND_HASHSIZE];
struct bind *sb_hashtab[MOD_BIND_HASHSIZE];

static int
nm_hash(char *name)
{
	char c;
	int hash = 0;

	for (c = *name++; c; c = *name++)
		hash ^= c;

	return (hash & MOD_BIND_HASHMASK);
}

void
clear_binding_hash(struct bind **bhash)
{
	int i;
	struct bind *bp, *bp1;

	for (i = 0; i < MOD_BIND_HASHSIZE; i++) {
		bp = bhash[i];
		while (bp != NULL) {
			kmem_free(bp->b_name, strlen(bp->b_name) + 1);
			if (bp->b_bind_name) {
				kmem_free(bp->b_bind_name,
				    strlen(bp->b_bind_name) + 1);
			}
			bp1 = bp;
			bp = bp->b_next;
			kmem_free(bp1, sizeof (struct bind));
		}
		bhash[i] = NULL;
	}
}

static struct bind *
find_mbind(char *name, struct bind **hashtab)
{
	int hashndx;
	struct bind *mb;

	hashndx = nm_hash(name);
	for (mb = hashtab[hashndx]; mb; mb = mb->b_next) {
		if (strcmp(name, mb->b_name) == 0)
			break;
	}

	return (mb);
}

/*
 * Create an entry for the given (name, major, bind_name) tuple in the
 * hash table supplied.  Reject the attempt to do so if 'name' is already
 * in the hash table.
 *
 * Does not provide synchronization, so use only during boot or with
 * externally provided locking.
 */
int
make_mbind(char *name, int major, char *bind_name, struct bind **hashtab)
{
	struct bind *bp;
	int hashndx;

	/*
	 * Fail if the key being added is already in the hash table
	 */
	if (find_mbind(name, hashtab) != NULL)
		return (-1);

	bp = kmem_zalloc(sizeof (struct bind), KM_SLEEP);
	bp->b_name = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(bp->b_name, name);
	bp->b_num = major;
	if (bind_name != NULL) {
		bp->b_bind_name = kmem_alloc(strlen(bind_name) + 1, KM_SLEEP);
		(void) strcpy(bp->b_bind_name, bind_name);
	}
	hashndx = nm_hash(name);
	bp->b_next = hashtab[hashndx];
	hashtab[hashndx] = bp;

	return (0);
}

/*
 * Delete a binding from a binding-hash.
 *
 * Does not provide synchronization, so use only during boot or with
 * externally provided locking.
 */
void
delete_mbind(char *name, struct bind **hashtab)
{
	int hashndx;
	struct bind *b, *bparent = NULL;
	struct bind *t = NULL;		/* target to delete */

	hashndx = nm_hash(name);

	if (hashtab[hashndx] == NULL)
		return;

	b = hashtab[hashndx];
	if (strcmp(name, b->b_name) == 0) {	/* special case first elem. */
		hashtab[hashndx] = b->b_next;
		t = b;
	} else {
		for (b = hashtab[hashndx]; b; b = b->b_next) {
			if (strcmp(name, b->b_name) == 0) {
				ASSERT(bparent);
				t = b;
				bparent->b_next = b->b_next;
				break;
			}
			bparent = b;
		}
	}

	if (t != NULL) {	/* delete the target */
		ASSERT(t->b_name);
		kmem_free(t->b_name, strlen(t->b_name) + 1);
		if (t->b_bind_name)
			kmem_free(t->b_bind_name, strlen(t->b_bind_name) + 1);
		kmem_free(t, sizeof (struct bind));
	}
}


major_t
mod_name_to_major(char *name)
{
	struct bind *mbind;

	if ((mbind = find_mbind(name, mb_hashtab)) != NULL)
		return ((major_t)mbind->b_num);

	return ((major_t)-1);
}

char *
mod_major_to_name(major_t major)
{
	if (major >= devcnt)
		return (NULL);
	return ((&devnamesp[major])->dn_name);
}

/*
 * Set up the devnames array.  Error check for duplicate entries.
 */
void
init_devnamesp(int size)
{
	int hshndx;
	struct bind *bp;
	static char dupwarn[] =
	    "!Device entry \"%s %d\" conflicts with previous entry \"%s %d\" "
	    "in /etc/name_to_major.";
	static char badmaj[] = "The major number %u is invalid.";

	ASSERT(size <= L_MAXMAJ32 && size > 0);

	/*
	 * Allocate the devnames array.  All mutexes and cv's will be
	 * automagically initialized.
	 */
	devnamesp = kobj_zalloc(size * sizeof (struct devnames), KM_SLEEP);

	/*
	 * Stick the contents of mb_hashtab into the devnames array.  Warn if
	 * two hash entries correspond to the same major number, or if a
	 * major number is out of range.
	 */
	for (hshndx = 0; hshndx < MOD_BIND_HASHSIZE; hshndx++) {
		for (bp = mb_hashtab[hshndx]; bp; bp = bp->b_next) {
			if (make_devname(bp->b_name, (major_t)bp->b_num) != 0) {
				/*
				 * If there is not an entry at b_num already,
				 * then this must be a bad major number.
				 */
				char *nm = mod_major_to_name(bp->b_num);
				if (nm == NULL) {
					cmn_err(CE_WARN, badmaj,
					    (uint_t)bp->b_num);
				} else {
					cmn_err(CE_WARN, dupwarn, bp->b_name,
					    bp->b_num, nm, bp->b_num);
				}
			}
		}
	}
}

int
make_devname(char *name, major_t major)
{
	struct devnames *dn;

	/*
	 * Until on-disk support for major nums > 14 bits arrives, fail
	 * any major numbers that are too big.
	 */
	if (major > L_MAXMAJ32)
		return (EINVAL);

	dn = &devnamesp[major];

	if (dn->dn_name && strcmp(dn->dn_name, name) == 0)
		return (0);		/* Adding same driver */

	/*
	 * dn_flags *can* be nonzero when dn->dn_name == NULL.  See getudev()
	 */
	if (dn->dn_name || (dn->dn_flags != 0))
		return (EINVAL);	/* Another driver already here */

	dn->dn_name = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(dn->dn_name, name);
	dn->dn_flags = 0;
	return (0);
}

/*
 * Set up the syscallnames array.
 */
void
init_syscallnames(int size)
{
	int hshndx;
	struct bind *bp;

	syscallnames = kobj_zalloc(size * sizeof (char *), KM_SLEEP);

	for (hshndx = 0; hshndx < MOD_BIND_HASHSIZE; hshndx++) {
		for (bp = sb_hashtab[hshndx]; bp; bp = bp->b_next) {
			make_syscallname(bp->b_name, bp->b_num);
		}
	}
}

static void
make_syscallname(char *name, int sysno)
{
	char **cp = &syscallnames[sysno];

	if (*cp != NULL) {
		cmn_err(CE_WARN, "!Couldn't add system call \"%s %d\". "
		    "It conflicts with \"%s %d\" in /etc/name_to_sysnum.",
		    name, sysno, *cp, sysno);
		return;
	}
	*cp = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(*cp, name);
}

/*
 * Given a system call name, get its number.
 */
int
mod_getsysnum(char *name)
{
	struct bind *mbind;

	if ((mbind = find_mbind(name, sb_hashtab)) != NULL)
		return (mbind->b_num);

	return (-1);
}

/*
 * Given a system call number, get the system call name.
 */
char *
mod_getsysname(int sysnum)
{
	return (syscallnames[sysnum]);
}

/*
 * Find the name of the module containing the specified pc.
 * Returns the name on success, "<unknown>" on failure.
 * No locking is required because modctls are persistent.
 */
char *
mod_containing_pc(caddr_t pc)
{
	struct modctl *mcp = &modules;
	do {
		if (mcp->mod_mp != NULL &&
		    (size_t)pc - (size_t)mcp->mod_text < mcp->mod_text_size)
			return (mcp->mod_modname);
	} while ((mcp = mcp->mod_next) != &modules);
	return ("<unknown>");
}
