/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DEVFSADM_IMPL_H
#define	_DEVFSADM_IMPL_H

#pragma ident	"@(#)devfsadm_impl.h	1.8	99/08/30 SMI"

#include <dlfcn.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/file.h>
#include <locale.h>
#include <libintl.h>
#include <ctype.h>
#include <signal.h>
#include <deflt.h>
#include <ftw.h>
#include <sys/instance.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/int_types.h>
#include <limits.h>
#include <strings.h>
#include <devfsadm.h>
#include <libdevinfo.h>
#include <sys/devinfo_impl.h>
#include <sys/modctl.h>
#include <libgen.h>
#include <sys/hwconf.h>
#include <sys/devfs_log_event.h>
#include <syslog.h>
#include <libdevfsevent.h>
#include <thread.h>
#include <message.h>
#include <sys/cladm.h>

#undef	DEBUG
#ifndef DEBUG
#define	NDEBUG 1
#else
#undef	NDEBUG
#endif

#include <assert.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	DEV_LOCK_FILE ".devfsadm_dev.lock"
#define	DAEMON_LOCK_FILE ".devfsadm_deamon.lock"

#define	DEV "/dev"
#define	DEV_LEN 4
#define	DEVICES "/devices"
#define	DEVICES_LEN 8
#define	MODULE_DIRS "/usr/lib/devfsadm/linkmod"
#define	PERMFILE "/etc/minor_perm"
#define	ALIASFILE "/etc/driver_aliases"
#define	NAME_TO_MAJOR "/etc/name_to_major"
#define	RECONFIG_BOOT "_INIT_RECONFIG"

#define	DEVFSADM_DEFAULT_FILE "/etc/default/devfsadm"

#define	MINOR_FINI_TIMEOUT_DEFAULT 2
#define	FORCE_CALL_MINOR_FINI	10

#define	DEFAULT_USER    "root"
#define	DEFAULT_GROUP   "sys"

#define	DRVCONFIG "drvconfig"
#define	DEVFSADM "devfsadm"
#define	DEVFSADMD "devfsadmd"
#define	DEVLINKS "devlinks"
#define	TAPES "tapes"
#define	AUDLINKS "audlinks"
#define	PORTS "ports"
#define	DISKS "disks"

#define	MAX_IDLE_DELAY 5
#define	MAX_DELAY 30
#define	NAME 0x01
#define	ADDR 0x03
#define	MINOR 0x04
#define	COUNTER 0x05
#define	CONSTANT 0x06
#define	TYPE 0x07
#define	TYPE_S "type"
#define	ADDR_S "addr"
#define	ADDR_S_LEN 4
#define	MINOR_S "minor"
#define	MINOR_S_LEN 5
#define	NAME_S "name"
#define	TAB '\t'
#define	NEWLINE '\n'
#define	MAX_DEVLINK_LINE 4028
#define	INTEGER 0
#define	LETTER 1
#define	MAX_PERM_LINE 256

#define	TYPE_LINK 0x00
#define	TYPE_DEVICES 0x01

#define	CREATE_LINK 0x01
#define	READ_LINK 0x02
#define	CREATE_NODE 0x01
#define	READ_NODE 0x02

#define	MODULE_ACTIVE 0x01

#define	MAX_SLEEP 120

#define	DEVLINKTAB_FILE "/etc/devlink.tab"

#define	MODULE_SUFFIX ".so"
#define	MINOR_INIT "minor_init"
#define	MINOR_FINI "minor_fini"
#define	_DEVFSADM_CREATE_REG "_devfsadm_create_reg"
#define	_DEVFSADM_REMOVE_REG "_devfsadm_remove_reg"

#define	NUM_EV_STR		4
#define	EV_TYPE			0
#define	EV_CLASS		1
#define	EV_PATH_NAME		2
#define	EV_MINOR_NAME		3

/* add new debug level and meanings here */
#define	DEVLINK_MID		"devfsadm:devlink"
#define	MODLOAD_MID		"devfsadm:modload"
#define	INITFINI_MID		"devfsadm:initfini"
#define	EVENT_MID		"devfsadm:event"
#define	REMOVE_MID		"devfsadm:remove"
#define	LOCK_MID		"devfsadm:lock"
#define	PATH2INST_MID		"devfsadm:path2inst"
#define	CACHE_MID		"devfsadm:cache"
#define	BUILDCACHE_MID		"devfsadm:buildcache"
#define	RECURSEDEV_MID		"devfsadm:recursedev"
#define	INSTSYNC_MID		"devfsadm:instsync"
#define	FILES_MID		"devfsadm:files"
#define	ENUM_MID		"devfsadm:enum"
#define	LINKCACHE_MID		"devfsadm:linkcache"
#define	ADDREMCACHE_MID		"devfsadm:addremcache"
#define	MALLOC_MID		"devfsadm:malloc"
#define	ALL_MID			"all"

#define	DEVFSADM_DEBUG_ON	(verbose == NULL) ? FALSE : TRUE

typedef struct recurse_dev {
	void (*fcn)(char *, void *);
	void *data;
} recurse_dev_t;

typedef struct link {
	char *devlink; /* without ".../dev/"   prefix */
	char *contents; /* without "../devices" prefix */
	struct link *next;
} link_t;

typedef struct linkhead {
	regex_t dir_re_compiled;
	char *dir_re;
	link_t *link;
	struct linkhead *next;
} linkhead_t;

typedef struct link_list  {
	int type;
	char *constant;
	int arg;
	struct link_list *next;
} link_list_t;

typedef struct selector_list {
	int key;
	char *val;
	int arg;
	struct selector_list *next;
} selector_list_t;

typedef struct devlinktab_list {
	int line_number;
	char *selector_pattern;
	char *p_link_pattern;
	char *s_link_pattern;
	selector_list_t *selector;
	link_list_t *p_link;
	link_list_t *s_link;
	struct devlinktab_list *next;
} devlinktab_list_t;

typedef struct module {
	char *name;
	void *dlhandle;
	int (*minor_init)();
	int (*minor_fini)();
	int flags;
	struct module *next;
} module_t;

typedef struct create_list {
	devfsadm_create_t *create;
	module_t *modptr;
	regex_t node_type_comp;
	regex_t drv_name_comp;
	struct create_list *next;
} create_list_t;


typedef struct defer_minor {
	di_node_t node;
	di_minor_t minor;
	struct defer_minor *next;
} defer_minor_t;

typedef struct defer_list {
	defer_minor_t *head;
	defer_minor_t *tail;
} defer_list_t;

typedef struct remove_list {
	devfsadm_remove_t *remove;
	module_t *modptr;
	struct remove_list *next;
} remove_list_t;

typedef struct cleanup_data {
	int flags;
	char *phypath;
	remove_list_t *rm;
} cleanup_data_t;

typedef struct n2m {
	major_t major;
	char *driver;
	struct n2m *next;
} n2m_t;

/* structures for devfsadm_enumerate() */
typedef struct numeral {
	char *id;
	char *full_path;
	int rule_index;
	char *cmp_str;
	struct numeral *next;
} numeral_t;

typedef struct numeral_set {
	int re_count;
	char **re;
	numeral_t *headnumeral;
	struct numeral_set *next;
} numeral_set_t;

typedef struct temp {
	int integer;
	struct temp *next;
} temp_t;

typedef struct driver_alias {
	char *driver_name;
	char *alias_name;
	struct driver_alias *next;
} driver_alias_t;

static int devfsadm_enumerate_int_start(char *devfs_path,
	int index, char **buf, devfsadm_enumerate_t rules[],
	int nrules, char *start);
static char *s_di_devfs_path(di_node_t node, di_minor_t minor);
static void deferred_call_minor_fini();
static void set_root_devices_dev_dir(char *dir);
static void pre_and_post_cleanup(int flags);
static void hot_cleanup(char *node_path, char *minor_path);
static void catch_sigs(void);
static void devfsadm_exit(int status);
static void rm_link_from_cache(char *devlink);
static void rm_all_links_from_cache();
static void add_link_to_cache(char *devlink, char *physpath);
static link_t *get_cached_links(char *dir_re);
static void build_devlink_list(char *check_link, void *data);
static void instance_flush_thread(void);
static void rm_parent_dir_if_empty(char *path);
static void free_link_list(link_list_t *head);
static void free_selector_list(selector_list_t *head);
void devfsadm_err_print(char *message, ...);
void defvsadm_print(int level, char *message, ...);
static int call_minor_init(module_t *module);
static void load_module(char *module, char *cdir);
extern int modctl(int, ...);
static void invalidate_enumerate_cache(void);
static pid_t enter_dev_lock(void);
static void exit_dev_lock(void);
static pid_t enter_daemon_lock(void);
static void exit_daemon_lock(void);
static int process_devlink_compat(di_minor_t minor, di_node_t node);
static int alias(char *drvname, char *name);
static int devfsadm_copy(void);
static void flush_path_to_inst(void);
static void detachfromtty(void);
static void minor_process(di_node_t node, di_minor_t minor,
    defer_list_t *deferp);
static void read_minor_perm_file(void);
static void read_driver_aliases_file(void);
static void load_modules(void);
static void unload_modules(void);
static void *s_malloc(const size_t size);
static void *s_zalloc(const size_t size);
static char *s_strdup(const char *ptr);
static void devfs_instance_mod(void);
static void add_minor_pathname(char *pathname, char *minorname);
static int check_minor_type(di_node_t node, di_minor_t minor, void *arg);
static void cache_deferred_minor(defer_list_t *deferp, di_node_t node,
    di_minor_t minor);
static int compare_field(char *full_name, char *field_item, int field);
static int component_cat(char *link, char *name, int field);
static void recurse_dev_re(char *current_dir, char *path_re, recurse_dev_t *rd);
static void matching_dev(char *devpath, void *data);
static int dangling(char *devpath);
static int clean_ok(devfsadm_remove_t *remove);
static int translate_major(dev_t old_dev, dev_t *new_dev);
static int get_major_no(char *driver, major_t *major);
static int load_n2m_table(char *filename);
static int get_stat_info(char *, struct stat *);
static char *new_id(numeral_t *, int, char *);
static int find_enum_id(devfsadm_enumerate_t rules[], int nrules,
    char *devfs_path, int index, char *min, int type, char **buf);
static void daemon_update(void);
static void call_event_handler(char *sp);
static void usage(void);
static int getnexttoken(char *next, char **nextp, char **tokenpp, char *tchar);
static int getvalue(char *token, int *valuep);
static int class_ok(char *class);
static void create_devices_node(di_node_t node, di_minor_t minor);
static int create_link_common(char *devlink, char *contents);
static char *dequote(char *src);
static void parse_args(int argc, char *argv[]);
static void process_devinfo_tree(void);
static void call_minor_fini_thread(void *arg);
static void *s_realloc(void *ptr, const size_t size);
static void read_devlinktab_file(void);
static selector_list_t *create_selector_list(char *selector);
static int parse_selector(char **selector, char **key, char **val);
int devfsadm_noupdate(void);
const char *devfsadm_root_path(void);
static link_list_t *create_link_list(char *link);
static void s_unlink(const char *file);
static void s_closedir(DIR *dirp);
static void s_mkdirp(const char *path, const mode_t mode);
static int s_fclose(FILE *fp);
static int is_minor_node(char *contents, char **mn_root);
static int construct_devlink(char *link, link_list_t *link_build,
				char *contents, di_minor_t minor,
				di_node_t node, char *pattern);
static int split_devlinktab_entry(char *entry, char **selector, char **p_link,
	    char **s_link);
static int devlink_matches(devlinktab_list_t *entry, di_minor_t minor,
			    di_node_t node);
static int build_links(devlinktab_list_t *entry, di_minor_t minor,
			di_node_t node);
static numeral_set_t *get_enum_cache(devfsadm_enumerate_t rules[],
				    int nrules);
static void enumerate_recurse(char *current_dir, char *path_left,
    numeral_set_t *setp, devfsadm_enumerate_t rules[], int index);

static int match_path_component(char *file_re, char *file, char **id,
				int subexp);
static void create_cached_numeral(char *path, numeral_set_t *setp,
    char *numeral_id, devfsadm_enumerate_t rules[], int index);
static int devfsadm_copy_file(const char *file, const struct stat *stat,
			    int flags, struct FTW *ftw);
static void getattr(char *devname, int spectype, dev_t dev, mode_t *mode,
		uid_t *uid, gid_t *gid);
static int minor_matches_rule(di_node_t node, di_minor_t minor,
				create_list_t *create);
static char *get_dpath_prefix();
static void add_verbose_id(char *mid);
static char *get_component(char *str, const char *comp_num);
static char *alloc_cmp_str(const char *devfs_path, devfsadm_enumerate_t *dep);
static int lookup_enum_cache(numeral_set_t *set, char *cmp_str,
    devfsadm_enumerate_t rules[], int index, numeral_t **matchnpp);


/* convenient short hands */
#define	vprint		devfsadm_print
#define	err_print	devfsadm_errprint
#define	TRUE	1
#define	FALSE	0

#ifdef	__cplusplus
}
#endif

#endif /* _DEVFSADM_IMPL_H */
