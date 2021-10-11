/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_nis_internal.cc	1.39	98/03/26 SMI"

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ndbm.h>
#include <synch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include <FNSP_Syntax.hh>
#include "fnsp_nis_internal.hh"
#include "fnsp_internal_common.hh"
#include "FNSP_nisImpl.hh"

/* NIS map names and directory */
static const char *FNSP_nis_map_dir = "/var/yp/";
static const char *FNSP_nis_dir = "/etc/fn/";
static const char *FNSP_org_map = FNSP_ORG_MAP;
static const char *FNSP_passwd_map = "passwd.byname";
static const char *FNSP_hosts_map = "hosts.byname";
static const char *FNSP_map_suffix = ".ctx";
static const char *FNSP_attr_suffix = ".attr";
static const char *FNSP_lock_file = "/fns.lock";
static const char *FNSP_count_file = "/fns.count";
static char *FNSP_yp_master = "YP_MASTER_NAME";
static char *FNSP_yp_last_modified = "YP_LAST_MODIFIED";

#define	DBM_MAX_SIZE	73728
#define	FNS_MIN_ENTRY	20480

// -----------------------------------------
// Routine to talk to foreign NIS domains
// -----------------------------------------
#include <dlfcn.h>

// Function defined in yp_bind.c to talk to another YP domain
// extern "C" {
// int __yp_add_binding(char * /* Domain */,
// char * /* IP address */);
// }

typedef int (*add_binding_func)(char *, char *);
typedef int (*yp_empty_cache)();

static mutex_t yp_add_binding_lock = DEFAULTMUTEX;
static void *nis_bind_function = 0;
static void *nis_empty_cache_function = 0;
static void *mh = 0;

// Function to add bindings to the foreign domain
// Returns 1 on success and 0 on failure
static int
yp_add_binding(char *domain, char *ip_address)
{
	// Check if we have cached the function
	mutex_lock(&yp_add_binding_lock);
	if (nis_bind_function != 0) {
		mutex_unlock(&yp_add_binding_lock);
		return ((*((add_binding_func) nis_bind_function))(domain,
		    ip_address));
	}

	// Try to dlsym to the function __yp_add_binding
	// if present in shared libraries
	if ((mh != 0) || ((mh = dlopen(0, RTLD_LAZY)) != 0)) {
		if (nis_bind_function = dlsym(mh, "__yp_add_binding")) {
			mutex_unlock(&yp_add_binding_lock);
			return ((*((add_binding_func) nis_bind_function))(
			    domain, ip_address));
		}
	}

	// Default behaviour, just try to bind and maybe fail
	mutex_unlock(&yp_add_binding_lock);
	return (yp_bind(domain) == 0);
}


// Function to empty YP cache
static void
FNSP_nis_empty_cache()
{
	// Check if we have cached the function
	mutex_lock(&yp_add_binding_lock);
	if (nis_empty_cache_function != 0) {
		mutex_unlock(&yp_add_binding_lock);
		(*((yp_empty_cache) nis_empty_cache_function))();
		return;
	}

	// Try to dlsym to the function __empty_yp_cache
	// if present in shared libraries
	if ((mh != 0) || ((mh = dlopen(0, RTLD_LAZY)) != 0)) {
		if (nis_empty_cache_function =
		    dlsym(mh, "__empty_yp_cache")) {
			mutex_unlock(&yp_add_binding_lock);
			(*((yp_empty_cache) nis_empty_cache_function))();
			return;
		}
	}
	mutex_unlock(&yp_add_binding_lock);
}

// Cache to store the READ ONLY NDBM files
class FNSP_nis_ndbm_files {
protected:
	char *dbm_file;
	DBM  *dbm;
	hrtime_t cache_time;
	FNSP_nis_ndbm_files *next;
	DBM *open_dbm();

public:
	FNSP_nis_ndbm_files(const char *file_name);
	virtual ~FNSP_nis_ndbm_files();
	static DBM *open_dbm(const char *file_name);
};

static mutex_t rd_only_ndbm_cache_lock = DEFAULTMUTEX;
static FNSP_nis_ndbm_files rd_only_ndbm_cache(0);
static rd_only_ndbm_cache_dirty;
#define NDBM_MAX_CACHE_TIME	((hrtime_t) (120000000000))

FNSP_nis_ndbm_files::FNSP_nis_ndbm_files(const char *file_name)
{
	if (file_name == 0) {
		dbm_file = strdup("DUMMY");
		dbm = 0;
	} else {
		dbm_file = strdup(file_name);
		dbm = dbm_open(file_name, O_RDONLY, 0444);
	}
	cache_time = gethrtime();
	next = 0;
}

FNSP_nis_ndbm_files::~FNSP_nis_ndbm_files()
{
	if (next)
		delete (next);
	if (dbm_file)
		free (dbm_file);
	if (dbm)
		dbm_close(dbm);
}

DBM *FNSP_nis_ndbm_files::open_dbm()
{
	hrtime_t currenttime = gethrtime();
	if ((currenttime - cache_time) > NDBM_MAX_CACHE_TIME) {
		if (dbm)
			dbm_close(dbm);
		dbm = dbm_open(dbm_file, O_RDONLY, 0444);
		cache_time = currenttime;
	}
	return (dbm);
}

DBM *FNSP_nis_ndbm_files::open_dbm(const char *file_name)
{
	FNSP_nis_ndbm_files *current;

	// Check if there has been a write operation
	if (rd_only_ndbm_cache_dirty) {
		delete rd_only_ndbm_cache.next;
		rd_only_ndbm_cache.next = 0;
		rd_only_ndbm_cache_dirty = 0;
	}

	// Check in cache
	mutex_lock(&rd_only_ndbm_cache_lock);
	current = &rd_only_ndbm_cache;
	while (current) {
		if (strcmp(current->dbm_file, file_name) == 0) {
			mutex_unlock(&rd_only_ndbm_cache_lock);
			return (current->open_dbm());
		}
		current = current->next;
	}
	mutex_unlock(&rd_only_ndbm_cache_lock);

	// Create a new entry
	current = new FNSP_nis_ndbm_files(file_name);
	mutex_lock(&rd_only_ndbm_cache_lock);
	current->next = rd_only_ndbm_cache.next;
	rd_only_ndbm_cache.next = current;
	mutex_unlock(&rd_only_ndbm_cache_lock);
	return (current->open_dbm());
}


// Struct to hold the domainname and operations to
// add and check domainnames. yp_unbind is done at exit

class FNSP_nis_domainname {
protected:
	char *domainname;
	FNSP_nis_domainname *next;
public:
	FNSP_nis_domainname();
	~FNSP_nis_domainname();
	int add_domain(char *domain);
	int check_domain(char *domain);
};

FNSP_nis_domainname::FNSP_nis_domainname()
{
	domainname = 0;
	next = 0;
}

FNSP_nis_domainname::~FNSP_nis_domainname()
{
	if (domainname) {
		yp_unbind(domainname);
		delete[] domainname;
	}
	delete next;
}

int
FNSP_nis_domainname::add_domain(char *domain)
{
	if (!domainname) {
		domainname = new char[strlen(domain) + 1];
		if (domainname == 0)
			return (0);
		strcpy(domainname, domain);
		return (1);
	}

	if (!next) {
		next = new FNSP_nis_domainname();
		if (next == 0)
			return (0);
	}
	return (next->add_domain(domain));
}

int
FNSP_nis_domainname::check_domain(char *domain)
{
	if (domainname)
		if (strcmp(domain, domainname) == 0)
			return (1);
	if (next)
		return (next->check_domain(domain));
	return (0);
}

// Lock for the domain name the current process is
// bound to.
static mutex_t domainname_lock = DEFAULTMUTEX;
static FNSP_nis_domainname nis_domainname;

unsigned
FNSP_nis_bind(const char *in_buffer)
{
	char *domain, *machine_name, *ip_address = 0;
	char ip_addr_buf[FNS_NIS_INDEX];
	int ret_error, fns_error = FN_SUCCESS;
	char *temp, buffer[FNS_NIS_SIZE];

	strcpy(buffer, in_buffer);
	domain = strparse(buffer, " ", &temp);
	machine_name = strparse(0, " ", &temp);
	if (machine_name) {
		if (inet_addr(machine_name) == (in_addr_t) -1)
			ip_address = strparse(0, " ", &temp);
		else {
			ip_address = machine_name;
			machine_name = 0;
		}
	}

	if ((machine_name) && (ip_address == 0)) {
		// Get the IP address from machine name
		struct hostent host_result;
		char host_buffer[FNS_NIS_SIZE];
		int host_error;
		struct hostent *hostentry = gethostbyname_r(
		    machine_name, &host_result, host_buffer,
		    FNS_NIS_SIZE, &host_error);
		if (hostentry == NULL)
			return (FN_E_CONFIGURATION_ERROR);

		// Get the IP address
		char **p;
		struct in_addr in;
		p = hostentry->h_addr_list;
		memcpy(&in.s_addr, *p, sizeof (in.s_addr));
		strcpy(ip_addr_buf, inet_ntoa(in));
		ip_address = ip_addr_buf;
	}


	// Obtain mutex lock
	mutex_lock(&domainname_lock);
	if (nis_domainname.check_domain(domain)) {
		mutex_unlock(&domainname_lock);
		return (FN_SUCCESS);
	}

	// If ip address is provided, then exteral binding has to be done
	if (ip_address) {
		if ((ret_error = yp_add_binding(domain, ip_address)) == 1) {
			if (nis_domainname.add_domain(domain) == 0)
				fns_error = FN_E_INSUFFICIENT_RESOURCES;
			mutex_unlock(&domainname_lock);
			return (fns_error);
		}
	} else if ((ret_error = yp_bind(domain)) == 0) {
		if (nis_domainname.add_domain(domain) == 0)
			fns_error = FN_E_INSUFFICIENT_RESOURCES;
		mutex_unlock(&domainname_lock);
		return (fns_error);
	}
	fns_error = FNSP_nis_map_status(ret_error);
	mutex_unlock(&domainname_lock);
	return (fns_error);
}

// Function to bind to NIS master server
static mutex_t master_domainname_lock = DEFAULTMUTEX;
static FNSP_nis_domainname nis_master_domainname;

static int
FNSP_nis_bind_master_server(char *domain)
{
	char *nis_master;
	char ip_addr_buf[FNS_NIS_INDEX];
	int yperr;

	// Check if already bound
	mutex_lock(&master_domainname_lock);
	if (nis_master_domainname.check_domain(domain)) {
		mutex_unlock(&master_domainname_lock);
		return (FN_SUCCESS);
	}

	// Get the master server
	if ((yperr = yp_master(domain, "fns_org.ctx",
	    &nis_master)) != 0) {
		mutex_unlock(&master_domainname_lock);
		return (FN_E_COMMUNICATION_FAILURE);
	}

	struct hostent host_result;
	char host_buffer[FNS_NIS_SIZE];
	int host_error;
	struct hostent *hostentry = gethostbyname_r(
	    nis_master, &host_result, host_buffer,
	    FNS_NIS_SIZE, &host_error);
	if (hostentry == NULL) {
		mutex_unlock(&master_domainname_lock);
		return (FN_E_COMMUNICATION_FAILURE);
	}

	// Get the IP address
	char **p, *ip_address;
	struct in_addr in;
	p = hostentry->h_addr_list;
	memcpy(&in.s_addr, *p, sizeof (in.s_addr));
	strcpy(ip_addr_buf, inet_ntoa(in));
	ip_address = ip_addr_buf;

	// unbind for the `domainname`
	yp_unbind(domain);
	// Bind to the domain with the master server
	if (yp_add_binding(domain, ip_address) == 0) {
		mutex_unlock(&master_domainname_lock);
		return (FN_E_COMMUNICATION_FAILURE);
	}

	// Add it to the cache
	if (nis_master_domainname.add_domain(domain) == 0) {
		mutex_unlock(&master_domainname_lock);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}
	mutex_unlock(&master_domainname_lock);
	return (FN_SUCCESS);
}

// -----------------------------------
// Mapping YP errors to FNS errors
// -----------------------------------
unsigned
FNSP_nis_map_status(int yp_error)
{
#ifdef DEBUG
	if (yp_error != 0)
		fprintf(stderr, "YP Error: %s\n",
		    yperr_string(ypprot_err(yp_error)));
#endif
	switch (yp_error) {
	case 0:
		return (FN_SUCCESS);
	case YPERR_ACCESS:
		return (FN_E_CTX_NO_PERMISSION);
	case YPERR_BADARGS:
		return (FN_E_ILLEGAL_NAME);
	case YPERR_BUSY:
		return (FN_E_CTX_UNAVAILABLE);
	case YPERR_KEY:
	case YPERR_MAP:
	case YPERR_NOMORE:
		return (FN_E_NAME_NOT_FOUND);
	case YPERR_DOMAIN:
	case YPERR_NODOM:
	case YPERR_BADDB:
		return (FN_E_CONFIGURATION_ERROR);
	case YPERR_RESRC:
		return (FN_E_INSUFFICIENT_RESOURCES);
	case YPERR_PMAP:
	case YPERR_RPC:
	case YPERR_YPBIND:
	case YPERR_YPERR:
	case YPERR_YPSERV:
	case YPERR_VERS:
		return (FN_E_COMMUNICATION_FAILURE);
	}
}

// ------------------------------------------
// Dynamically adding maps to NIS,
// by modifying the Makefile for FNS
// ------------------------------------------
#define	MAKE_FILE "/Makefile"
#define	MAKEFILE_0 "\n%s.time : /var/yp/%s/%s.pag\n"
#define	MAKEFILE_1 "\t-@if [ -f /var/yp/$(DOM)/%s.pag ]; then \\\n"
#define	MAKEFILE_6 "\t\tif [ ! $(NOPUSH) ]; then \\\n"
#define	MAKEFILE_7 "\t\t\t$(YPPUSH) %s; \\\n"
#define	MAKEFILE_8 "\t\t\techo \"pushed %s\"; \\\n"
#define	MAKEFILE_9 "\t\telse \\\n"
#define	MAKEFILE_10 "\t\t: ; \\\n"
#define	MAKEFILE_11 "\t\tfi \\\n"
#define	MAKEFILE_12 "\telse \\\n"
#define	MAKEFILE_13 "\t\techo \"couldn't find %s\"; \\\n"
#define	MAKEFILE_14 "\tfi\n"

unsigned
FNSP_update_makefile(const char *mapfile)
{
	char makefile[FNS_NIS_INDEX];
	char tempfile[FNS_NIS_INDEX];
	char line[FNS_NIS_SIZE], temp[FNS_NIS_SIZE], *ret, *tp;
	FILE *rf, *wf;

	// First obtain the domainname from variable "mapfile"
	// It is of the form /var/fn/'domainname'/mapfile
	char domain[FNS_NIS_INDEX];
	const char *start;
	start = mapfile + strlen(FNSP_nis_map_dir);
	size_t ptr = 0;
	while (start[ptr] != '/')
		ptr++;
	strncpy(domain, start, ptr);
	domain[ptr] = '\0';

	// Obtain map name from "mapfile".
	char map[FNS_NIS_INDEX];
	start = mapfile + strlen(FNSP_nis_map_dir) + strlen(domain) + 1;
	strcpy(map, start);

	// Construct the path for the "Makefile"
	strcpy(makefile, FNSP_nis_dir);
	strcat(makefile, domain);
	strcat(makefile, MAKE_FILE);
	strcpy(tempfile, makefile);
	strcat(tempfile, ".tmp");
	if ((rf = fopen(makefile, "r")) == NULL)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if ((wf = fopen(tempfile, "w")) == NULL)
		return (FN_E_INSUFFICIENT_RESOURCES);

	// Update the *all* line
	while (fgets(line, sizeof (line), rf)) {
		if (FNSP_match_map_index(line, "all:")) {
			// Check if the map name already exists
			strcpy(temp, line);
			ret = strparse(temp, map, &tp);
			if (strcmp(ret, line) == 0) {
				if (iscntrl(line[strlen(line) - 1]))
					line[strlen(line) - 1] = '\0';
				fprintf(wf, "%s %s.time\n",
				    line, map);
			} else {
				unlink(tempfile);
				fclose(rf);
				fclose(wf);
				return (FN_SUCCESS);
			}
		} else
			fputs(line, wf);
	}

	fprintf(wf, MAKEFILE_0
	    MAKEFILE_1
	    MAKEFILE_6
	    MAKEFILE_7
	    MAKEFILE_8
	    MAKEFILE_9
	    MAKEFILE_10
	    MAKEFILE_11
	    MAKEFILE_12
	    MAKEFILE_13
	    MAKEFILE_14,
	    map, domain, map, map, map, map, map);

	fclose(rf);
	fclose(wf);
	if (rename(tempfile, makefile) < 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	else
		return (FN_SUCCESS);
}

// --------------------------------------------
// File maipulation routines for NIS maps
// This routine can be used to selectively
// insert, delete, store and modify an entry
// in the map speficied
// --------------------------------------------
static unsigned
FNSP_is_update_valid(const char *domain, const char *map)
{
	char mapname[FNS_NIS_INDEX];

	// Check if map is valid for update ie.,
	// neither passwd not hosts table
	if ((strncasecmp(map, FNSP_passwd_map,
	    strlen(FNSP_passwd_map)) == 0) ||
	    (strncasecmp(map, FNSP_hosts_map,
	    strlen(FNSP_hosts_map)) == 0))
		return (FN_E_CTX_NO_PERMISSION);

	// Check for *root* permissions
	uid_t pid = geteuid();
	if (pid != 0)
		return (FN_E_CTX_NO_PERMISSION);

	struct stat buf;
	strcpy(mapname, FNSP_nis_dir);
	strcat(mapname, domain);
	strcat(mapname, MAKE_FILE);
	if (stat(mapname, &buf) != 0)
		return (FN_E_CTX_NO_PERMISSION);
	else
		return (FN_SUCCESS);
}

static unsigned
FNSP_can_read_from_files(const char *domain, const char *map)
{
	char mapname[FNS_NIS_INDEX];
	struct stat buf;
	strcpy(mapname, FNSP_nis_map_dir);
	strcat(mapname, domain);
	strcat(mapname, "/");
	strcat(mapname, map);
	strcat(mapname, ".pag");
	if (stat(mapname, &buf) != 0)
		return (FN_E_CTX_NO_PERMISSION);
	else if (buf.st_mode  & S_IROTH)
		return (FN_SUCCESS);
	else
		return (FN_E_CTX_NO_PERMISSION);
}

static int
FNSP_get_number_of_lines(const char *mapfile)
{
	// Get the domain name
	char domain[FNS_NIS_INDEX];
	const char *start;
	start = mapfile + strlen(FNSP_nis_map_dir);
	size_t prt = 0;
	while (start[prt] != '/')
		prt++;
	strncpy(domain, start, prt);
	domain[prt] = '\0';

	int count = 0;
	FILE *rf;
	char count_file[FNS_NIS_INDEX], *num;
	char line[FNS_NIS_SIZE];

	// Construnct the count_file name
	strcpy(count_file, FNSP_nis_dir);
	strcat(count_file, domain);
	strcat(count_file, FNSP_count_file);
	if ((rf = fopen(count_file, "r")) == NULL) {
		if ((rf = fopen(count_file, "w")) == NULL)
			return (-1);
		fclose(rf);
		return (0);
	}

	while (fgets(line, sizeof (line), rf)) {
		if (FNSP_match_map_index(line, mapfile)) {
			num = line + strlen(mapfile);
			while (isspace(*num))
				num++;
			count = atoi(num);
		}
	}
	fclose(rf);
	return (count);
}

static unsigned
FNSP_set_number_of_lines(const char *mapfile, int count)
{
	// Get the domain name
	char domain[FNS_NIS_INDEX];
	const char *start;
	start = mapfile + strlen(FNSP_nis_map_dir);
	size_t prt = 0;
	while (start[prt] != '/')
		prt++;
	strncpy(domain, start, prt);
	domain[prt] = '\0';

	FILE *wf, *rf;
	char count_file[FNS_NIS_INDEX];
	char temp_file[FNS_NIS_INDEX];
	char line[FNS_NIS_SIZE];

	// Construnct the count_file name
	strcpy(count_file, FNSP_nis_dir);
	strcat(count_file, domain);
	strcat(count_file, FNSP_count_file);
	if ((rf = fopen(count_file, "r")) == NULL) {
		if ((wf = fopen(count_file, "w")) == NULL)
			return (FN_E_INSUFFICIENT_RESOURCES);
		fprintf(wf, "%s  %d\n", mapfile, count);
		fclose(wf);
		return (FN_SUCCESS);
	}
	strcpy(temp_file, count_file);
	strcat(temp_file, ".tmp");
	if ((wf = fopen(temp_file, "w")) == NULL) {
		fclose(rf);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	int found = 0;
	while (fgets(line, sizeof (line), rf)) {
		if ((!found) &&
		    (FNSP_match_map_index(line, mapfile))) {
			found = 1;
			fprintf(wf, "%s  %d\n", mapfile, count);
		} else
			fputs(line, wf);
	}
	if (!found)
		fprintf(wf, "%s  %d\n", mapfile, count);
	fclose(rf);
	fclose(wf);
	if (rename(temp_file, count_file) < 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	return (FN_SUCCESS);
}

unsigned
FNSP_compose_next_map_name(char *map)
{
	int ctx = 0;

	// Check if the last char are of FNSP_map_suffix
	size_t length = strlen(map) - strlen(FNSP_map_suffix);
	if (strcmp(&map[length], FNSP_map_suffix) == 0) {
		map[length] = '\0';
		ctx = 1;
	} else {
		length = strlen(map) - strlen(FNSP_attr_suffix);
		map[length] = '\0';
	}

	if (!isdigit(map[strlen(map) - 1])) {
		strcat(map, "_0");
		if (ctx)
			strcat(map, FNSP_map_suffix);
		else
			strcat(map, FNSP_attr_suffix);
		return (FN_SUCCESS);
	}

	char num[FNS_NIS_INDEX];
	int map_num;
	size_t i = strlen(map) - 1;
	while (map[i] != '_') i--;
	strcpy(num, &map[i+1]);
	map_num = atoi(num) + 1;
	if (ctx)
		sprintf(&map[i+1], "%d%s", map_num, FNSP_map_suffix);
	else
		sprintf(&map[i+1], "%d%s", map_num, FNSP_attr_suffix);
	return (FN_SUCCESS);
}

static unsigned
FNSP_insert_last_modified_key(DBM *db)
{
	datum dbm_key, dbm_value;
	dbm_key.dptr = FNSP_yp_last_modified;
	dbm_key.dsize = strlen(FNSP_yp_last_modified);

	// Get time of the day
	struct timeval time;
	gettimeofday(&time, NULL);
	char timeofday[FNS_NIS_INDEX];
	sprintf(timeofday, "%ld", time.tv_sec);
	dbm_value.dptr = timeofday;
	dbm_value.dsize = strlen(timeofday);

	if (dbm_store(db, dbm_key, dbm_value, DBM_REPLACE) != 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	return (FN_SUCCESS);
}

static int
FNSP_init_dbm_file(DBM *db)
{
	int dbm_ret;
	datum dbm_key, dbm_value;

	// Insert the YP_MASTER_NAME key
	char hostname[MAXHOSTNAMELEN];
	sysinfo(SI_HOSTNAME, hostname, MAXHOSTNAMELEN);
	dbm_key.dptr = FNSP_yp_master;
	dbm_key.dsize = strlen(FNSP_yp_master);
	dbm_value.dptr = hostname;
	dbm_value.dsize = strlen(hostname);
	dbm_ret = dbm_store(db, dbm_key, dbm_value, DBM_REPLACE);
	return (dbm_ret);
}

static unsigned
FNSP_fast_update_map(const char *domain, const char *map,
    const char *index, const void *data)
{
	// Compose the map name
	char mapfile[FNS_NIS_INDEX];
	int lines;
	strcpy(mapfile, FNSP_nis_map_dir);
	strcat(mapfile, domain);
	strcat(mapfile, "/");
	strcat(mapfile, map);

	// Check memory of the dbm file.
	// If the memory is greater than DBM_MAX_SIZE
	// switch to the next dbm file
	int valid_map_file;
	char dbmfile[FNS_NIS_INDEX];
	struct stat dbm_stat;

	int dbm_ret, loop_count = 0;
	DBM *db;
	datum dbm_key, dbm_value, dbm_added;
	dbm_key.dptr = (char *) index;
	dbm_key.dsize = strlen(index);
	dbm_value.dptr = (char *) data;
	dbm_value.dsize = strlen((char *) data);

 fast_update_start_over:
	unsigned status = FN_SUCCESS;
	loop_count++;
	if (loop_count > 100)
		return (FN_E_INSUFFICIENT_RESOURCES);
	valid_map_file = 0;
	while (!valid_map_file) {
		lines = FNSP_get_number_of_lines(mapfile);
		if (lines < FNS_MIN_ENTRY)
			valid_map_file = 1;
		else if (lines >= FNS_MAX_ENTRY)
			FNSP_compose_next_map_name(mapfile);
		else {
			strcpy(dbmfile, mapfile);
			strcat(dbmfile, ".pag");
			if (stat(dbmfile, &dbm_stat) != 0)
				// File not found
				valid_map_file = 1;
			else
				if (dbm_stat.st_blocks < DBM_MAX_SIZE)
					valid_map_file = 1;
				else
					FNSP_compose_next_map_name(mapfile);
		}
	}

	if ((db = dbm_open(mapfile, O_RDWR | O_CREAT, 0644)) != 0) {
		if (lines == 0) {
			FNSP_update_makefile(mapfile);
			if (FNSP_init_dbm_file(db) != 0) {
				dbm_close(db);
				return (FN_E_CONFIGURATION_ERROR);
			}
			lines = 2;
		}
		dbm_clearerr(db);
		dbm_ret = dbm_store(db, dbm_key, dbm_value, DBM_INSERT);
		if (dbm_ret == 1)
			status = FN_E_NAME_IN_USE;
		else if ((dbm_ret != 0) || dbm_error(db)) {
			// Hashing problem, try next file
			dbm_delete(db, dbm_key);
			dbm_close(db);
			FNSP_compose_next_map_name(mapfile);
			if (dbm_error(db))
				dbm_clearerr(db);
			goto fast_update_start_over;
		} else {
			FNSP_set_number_of_lines(mapfile, lines+1);
			status = FNSP_insert_last_modified_key(db);
		}

		// Make sure the entry is present in the DBM file
		// %%% There seems to be a BUG in NDBM where, a success
		// is returned but the entry is not added to the DBM file
		dbm_clearerr(db);
		dbm_added = dbm_fetch(db, dbm_key);
		if (dbm_added.dptr == NULL) {
			// Value is not added, add to the next map
			dbm_close(db);
			FNSP_set_number_of_lines(mapfile, lines);
			FNSP_compose_next_map_name(mapfile);
			goto fast_update_start_over;
		}
		dbm_close(db);
		
	} else
		status = FN_E_INSUFFICIENT_RESOURCES;
	return (status);
}

static unsigned
fnsp_client_update_nis_database(const char *domain, const char *map,
    const char *key, const void *value, FNSP_map_operation op);

static unsigned
FNSP_local_update_map(const char *domain, const char *map,
    const char *index, const void *data,
    FNSP_map_operation op)
{
	unsigned status;

	int count, dbm_ret;
	struct stat buffer;
	char mapfile[FNS_NIS_INDEX], dbmfile[FNS_NIS_INDEX];
	DBM *db;
	datum dbm_key, dbm_value, dbm_val;

	dbm_key.dptr = (char *) index;
	dbm_key.dsize = strlen(index);

	strcpy(mapfile, FNSP_nis_map_dir);
	strcat(mapfile, domain);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	strcpy(dbmfile, mapfile);
	strcat(dbmfile, ".dir");
	dbm_ret = 1;
	while (stat(dbmfile, &buffer) == 0) {
		if ((db = dbm_open(mapfile, O_RDWR, 0644)) == 0) {
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
		switch (op) {
		case FNSP_map_store:
		case FNSP_map_modify:
			dbm_val = dbm_fetch(db, dbm_key);
			if (dbm_val.dptr != 0) {
				dbm_value.dptr = (char *) data;
				dbm_value.dsize = strlen((char *) data);
				dbm_ret = dbm_store(db, dbm_key, dbm_value,
				    DBM_REPLACE);
				if (dbm_ret != 0) {
					status = FN_E_INSUFFICIENT_RESOURCES;
					dbm_ret = 0;
				} else
					status =
					    FNSP_insert_last_modified_key(db);
			}
			break;
		case FNSP_map_delete:
			dbm_ret = dbm_delete(db, dbm_key);
			if (dbm_ret == 0) {
				count = FNSP_get_number_of_lines(mapfile);
				FNSP_set_number_of_lines(mapfile, count-1);
				status = FNSP_insert_last_modified_key(db);
			}
			break;
		case FNSP_map_insert:
			dbm_val = dbm_fetch(db, dbm_key);
			if (dbm_val.dptr != 0) {
				dbm_ret = 0;
				status = FN_E_NAME_IN_USE;
			}
			break;
		}
		dbm_close(db);
		if (!dbm_ret) {
			// Found entry
			return (status);
		}
		FNSP_compose_next_map_name(mapfile);
		strcpy(dbmfile, mapfile);
		strcat(dbmfile, ".dir");
	}
	switch (op) {
	case FNSP_map_modify:
	case FNSP_map_delete:
		return (FN_E_NAME_NOT_FOUND);
	case FNSP_map_insert:
	case FNSP_map_store:
		break;
	}

	status = FNSP_fast_update_map(domain, map, index, data);
	return (status);
}

unsigned FNSP_update_map(const char *domain, const char *map,
    const char *index, const void *data,
    FNSP_map_operation op)
{
	// If the operation is store or modify, delete first
	if (FNSP_is_update_valid((char *) domain, (char *) map)
	    == FN_SUCCESS) {
		switch (op) {
		case FNSP_map_store:
		case FNSP_map_modify:
			FNSP_update_map(domain, map, index, data,
			    FNSP_map_delete);
		default:
			break;
		}
	} else
		return (fnsp_client_update_nis_database(domain,
		    map, index, data, op));

	// Lock the operations. Lock FNS update map
	int lock_fs;
	char lock_file[FNS_NIS_INDEX];
	strcpy(lock_file, FNSP_nis_dir);
	strcat(lock_file, domain);
	strcat(lock_file, FNSP_lock_file);
	if ((lock_fs = open(lock_file, O_WRONLY)) == -1)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if (lockf(lock_fs, F_LOCK, 0L) == -1) {
		close(lock_fs);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	// Hold all the signals
	sigset_t mask_signal, org_signal;
	sigfillset(&mask_signal);
	sigprocmask(SIG_BLOCK, &mask_signal, &org_signal);
	
	// Need to split the data to fit 1024 byte limitation
	char new_index[FNS_NIS_SIZE], next_index[FNS_NIS_SIZE];
	char new_data[FNS_NIS_SIZE];
	unsigned status, stat, count = 1, delete_count = 0;
	size_t length = 0;
	for (status = FNSP_get_first_index_data(op, index, data,
	    new_index, new_data, length, next_index);
	    status == FN_SUCCESS;
	    status = FNSP_get_next_index_data(op, index, data,
	    new_index, new_data, length, next_index)) {
		stat = FNSP_local_update_map(domain, map,
		    new_index, new_data, op);
		if (stat != FN_SUCCESS) {
			if ((op == FNSP_map_delete) &&
			    (delete_count < 2) && (count != 1))
				delete_count++;
			else {
				if ((count == 1) ||
				    (op != FNSP_map_delete))
					status = stat;
				else
					status = FN_SUCCESS;
				goto update_map_out;
			}
		} else if (op == FNSP_map_delete)
			delete_count = 0;
		count++;
	}
	if (!((count == 1) && (status == FN_E_INSUFFICIENT_RESOURCES)))
		status = FN_SUCCESS;
 update_map_out:
	if (status == FN_SUCCESS)
		rd_only_ndbm_cache_dirty = 1;
	sigprocmask(SIG_SETMASK, &org_signal, 0);
	lockf(lock_fs, F_ULOCK, 0L);
	close(lock_fs);
	return (status);
}


static unsigned
FNSP_yp_map_local(char *domain, char *map, char *map_index,
    char **mapentry, int *maplen)
{
	DBM *db;
	char mapfile[FNS_NIS_INDEX];

	// Lock FNS file
	// int lock_fs;
	// char lock_file[FNS_NIS_INDEX];
	// strcpy(lock_file, FNSP_nis_dir);
	// strcat(lock_file, domain);
	// strcat(lock_file, FNSP_lock_file);

	// if ((lock_fs = open(lock_file, O_WRONLY)) == -1)
	// return (FN_E_INSUFFICIENT_RESOURCES);
	// if (lockf(lock_fs, F_LOCK, 0) == -1) {
	// perror("Unable to obtain lock\n");
	// close(lock_fs);
	// return (FN_E_INSUFFICIENT_RESOURCES);
	// }

	datum dbm_key, dbm_value;
	dbm_key.dptr = map_index;
	dbm_key.dsize = strlen(map_index);
	strcpy(mapfile, FNSP_nis_map_dir);
	strcat(mapfile, domain);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	while ((db = FNSP_nis_ndbm_files::open_dbm(mapfile))) {
		dbm_value = dbm_fetch(db, dbm_key);
		if (dbm_value.dptr != 0) {
			*maplen = dbm_value.dsize;
			*mapentry = (char *) malloc(*maplen);
			strncpy(*mapentry, dbm_value.dptr, (*maplen));
			// lockf(lock_fs, F_ULOCK, 0L); close(lock_fs);
			return (FN_SUCCESS);
		}
		FNSP_compose_next_map_name(mapfile);
	}
	// lockf(lock_fs, F_ULOCK, 0L);
	// close(lock_fs);
	return (FN_E_NAME_NOT_FOUND);
}

static unsigned
FNSP_local_yp_map_lookup(char *domain, char *map, char *map_index,
    int len, char **mapentry, int *maplen)
{
	int yperr;
	unsigned status;
	char mapfile[FNS_NIS_INDEX];

	strcpy(mapfile, map);
	if (FNSP_can_read_from_files(domain, mapfile) == FN_SUCCESS)
		return (FNSP_yp_map_local(domain, mapfile, map_index,
		    mapentry, maplen));

	mutex_lock(&domainname_lock);
	if (!nis_domainname.check_domain(domain)) {
		mutex_unlock(&domainname_lock);
		if ((status = FNSP_nis_bind(domain)) != FN_SUCCESS)
			return (status);
	} else
		mutex_unlock(&domainname_lock);

	while ((yperr = yp_match((char *) domain, mapfile, map_index,
	    len, mapentry, maplen)) == YPERR_KEY)
		FNSP_compose_next_map_name(mapfile);
	return (FNSP_nis_map_status(yperr));
}

class FNSP_nis_individual_entry {
public:
	char *mapentry;
	size_t maplen;
	FNSP_nis_individual_entry *next;
};

unsigned
FNSP_yp_map_lookup(char *domain, char *map, char *map_index,
    int /* len */, char **mapentry, int *maplen)
{
	char new_index[FNS_NIS_SIZE], next_index[FNS_NIS_SIZE];
	unsigned status, stat;
	FNSP_nis_individual_entry *prev_entry = 0, *new_entry;
	FNSP_nis_individual_entry *entries = 0;

	for (status = FNSP_get_first_lookup_index(map_index,
	    new_index, next_index); status == FN_SUCCESS;
	    status = FNSP_get_next_lookup_index(new_index, next_index,
	    *mapentry, *maplen)) {
		stat = FNSP_local_yp_map_lookup(domain, map, new_index,
		    strlen(new_index), mapentry, maplen);
		if (stat != FN_SUCCESS)
			break;
		new_entry = (FNSP_nis_individual_entry *)
		    malloc(sizeof(FNSP_nis_individual_entry));
		new_entry->mapentry = *mapentry;
		new_entry->maplen = *maplen;
		new_entry->next = 0;
		if (entries == 0)
			entries = new_entry;
		else
			prev_entry->next = new_entry;
		prev_entry = new_entry;
	}
	if (!entries)
		return (stat);

	// Construct the mapentry from the linked list
	size_t length = 0;
	new_entry = entries;
	while (new_entry) {
		length += new_entry->maplen;
		new_entry = new_entry->next;
	}
	*maplen = (int) length;
	*mapentry = (char *) malloc(sizeof(char)*(length + 1));
	memset((void *) (*mapentry), 0, sizeof(char)*(length + 1));
	new_entry = entries;
	while (new_entry) {
		if (new_entry == entries)
			strncpy((*mapentry), new_entry->mapentry,
			    new_entry->maplen);
		else
			strncat((*mapentry), new_entry->mapentry,
			    new_entry->maplen);
		new_entry = new_entry->next;
	}
	(*mapentry)[sizeof(char)*length] = '\0';

	// Destroy entries
	new_entry = entries;
	while (new_entry) {
		free (new_entry->mapentry);
		prev_entry = new_entry;
		new_entry = prev_entry->next;
		free (prev_entry);
	}
	return (FN_SUCCESS);
}
	
// Structure to hold information about FNS installation
// The value of installation is as follows
// status == 2 (unknown)
// status == 1 (fns installed)
// status == 0 (fns not installed)
#define	FNS_NIS_UNKNOWN_INSTALLTION 2
#define	FNS_NIS_INSTALLED 1
#define	FNS_NIS_NOT_INSTALLED 0

class FNSP_fns_nis_installation : public FNSP_nis_domainname {
protected:
	unsigned status;
public:
	FNSP_fns_nis_installation();
	~FNSP_fns_nis_installation();
	int add_nis_installation(char *domain, unsigned stat);
	unsigned check_nis_installation(char *domain);
};

FNSP_fns_nis_installation::FNSP_fns_nis_installation()
: FNSP_nis_domainname()
{
}

FNSP_fns_nis_installation::~FNSP_fns_nis_installation()
{
}

int
FNSP_fns_nis_installation::add_nis_installation(char *domain,
    unsigned stat)
{
	if (!domainname) {
		status = stat;
		return (add_domain(domain));
	}

	if (!next) {
		next = new FNSP_fns_nis_installation;
		if (next == 0)
			return (0);
	}
	return (((FNSP_fns_nis_installation *)
	    next)->add_nis_installation(domain, stat));
}

unsigned
FNSP_fns_nis_installation::check_nis_installation(char *domain)
{
	if (check_domain(domain))
		return (status);
	if (next)
		return (((FNSP_fns_nis_installation *)
		    next)->check_nis_installation(domain));
	return (FNS_NIS_UNKNOWN_INSTALLTION);
}

// Cache the domainname with info about FNS installation
static mutex_t fns_installation_lock = DEFAULTMUTEX;
static FNSP_fns_nis_installation install_domainnames;

int
FNSP_is_fns_installed(const FN_ref_addr *caddr)
{
	FN_string index_name, table_name;
	FN_string *map, *domain;
	unsigned status;

	FN_string *iname = FNSP_address_to_internal_name(*caddr);
	if (FNSP_decompose_nis_index_name(*iname, table_name,
	    index_name)) {
		status = FNSP_nis_split_internal_name(table_name, &map, &domain);
		if (status != FN_SUCCESS)
			return (0);
		delete map;
	} else {
		char d_name[FNS_NIS_INDEX], *chr_ptr;
		strcpy(d_name, (char *) iname->str());
		if ((chr_ptr = strchr(d_name, ' ')) == NULL)
			domain = new FN_string(*iname);
		else {
			chr_ptr[0] = '\0';
			domain = new FN_string(
			    (unsigned char *) d_name);
			FNSP_nis_bind((char *) iname->str());
		}
	}
	delete iname;

	// Check the cache for FNS installation
	mutex_lock(&fns_installation_lock);
	status = install_domainnames.check_nis_installation(
	    (char *) domain->str());
	mutex_unlock(&fns_installation_lock);
	if (status != FNS_NIS_UNKNOWN_INSTALLTION) {
		delete domain;
		return (status);
	}

	// Reset status value
	status = 0;

	char mapname[FNS_NIS_INDEX], *nis_master;
	int yperr;
	strcpy(mapname, FNSP_org_map);
	if ((yperr = yp_master((char *) domain->str(),
	    mapname, &nis_master)) == 0) {
		free(nis_master);
		status = FN_SUCCESS;
	} else {
		// Check for root ID
		if (geteuid() != 0) {
			delete domain;
			return (status);
		}

		// Check if the make file exists
		char tname[FNS_NIS_INDEX];
		strcpy(tname, FNSP_nis_dir);
		strcat(tname, (char *) domain->str());
		strcat(tname, MAKE_FILE);
		struct stat buffer;
		if (stat(tname, &buffer) == 0)
			status = FN_SUCCESS;
	}

	// Update the cache
	mutex_lock(&fns_installation_lock);
	install_domainnames.add_nis_installation((char *)
	    domain->str(), status);
	mutex_unlock(&fns_installation_lock);

	delete domain;
	return (status);
}


// Functions to update user/host contexts and attributes
// from the client using ONC RPC

#define FNSP_NIS_UPDATE_USER 1
#define FNSP_NIS_UPDATE_HOST 2

// strlen of user's map
#define FNS_USER_LEN 8UL
#define FNS_HOST_LEN 8UL
#define FNS_CTX_LEN 4UL
#define FNS_ATTR_LEN 5UL

extern "C" unsigned
fnsp_server_update_nis_database(const char *name, int user_host,
    const char *domain, const char *map,
    const char *key, const char *value,
    const char *old_value, FNSP_map_operation op)
{
	// Check if the correct maps are being updated
	switch (user_host) {
	case FNSP_NIS_UPDATE_USER:
		if (strncmp(map, "fns_user", FNS_USER_LEN)
		    != 0)
			return (FN_E_CTX_NO_PERMISSION);
		break;
	case FNSP_NIS_UPDATE_HOST:
		if (strncmp(map, "fns_host", FNS_HOST_LEN)
		    != 0)
			return (FN_E_CTX_NO_PERMISSION);
		break;
	}

	// Check permissions for "*.ctx" maps
	if (strncmp(&map[strlen(map) - FNS_CTX_LEN],
	    ".ctx", strlen(".ctx")) == 0) {
		// Changes in context information
		char entry_name[FNS_NIS_INDEX];
		strcpy(entry_name, name);
		strcat(entry_name, "_");
		// Check if the key is related to the user
		if (strncmp(key, entry_name, strlen(entry_name)) != 0)
			return (FN_E_CTX_NO_PERMISSION);
	} else if (strncmp(&map[strlen(map) - FNS_ATTR_LEN],
	    ".attr", FNS_ATTR_LEN) != 0)
		return (FN_E_CTX_NO_PERMISSION);

	// Check if the old value matches with the current values
	char *old_mapentry;
	int old_maplen, status = FN_SUCCESS;
	if (FNSP_yp_map_lookup((char *) domain, (char *) map,
	    (char *) key, strlen(key), &old_mapentry, &old_maplen)
	    == FN_SUCCESS) {
		if (strcmp(old_value, old_mapentry) != 0)
			// The correct error message should be
			// data out of sync. Due to lack of correct
			// error message, the following is returned
			status = FN_E_UNSPECIFIED_ERROR;
		free (old_mapentry);
	} else if (strcmp(old_value, "") != 0)
		status = FN_E_UNSPECIFIED_ERROR;

	if (status != FN_SUCCESS)
		return (status);

	// Update the NIS maps
	return (FNSP_update_map(domain, map, key, value, op));
}

extern "C" unsigned
fnsp_call_client_update_nis_database(const char *username,
    int user_host, const char *domain,
    const char *map, const char *key, const char *value,
    const char *old_value, int op);

static unsigned
fnsp_client_update_nis_database(const char *domain, const char *map,
    const char *key, const void *value, FNSP_map_operation op)
{
	// Get the old values from the current map for the key
	char *old_mapentry = 0;
	int old_maplen;
	FNSP_yp_map_lookup((char *) domain, (char *) map, (char *) key,
	    strlen(key), &old_mapentry, &old_maplen);

	// First check if cache can be emptied
	if (nis_empty_cache_function == 0) {
		FNSP_nis_empty_cache();
		if (nis_empty_cache_function == 0) {
			if (old_mapentry)
				free (old_mapentry);
			return (FN_E_CTX_NO_PERMISSION);
		}
	}

	int status;
	if ((status = FNSP_nis_bind_master_server((char *) domain))
	    != FN_SUCCESS) {
		if (old_mapentry)
			free (old_mapentry);
		return (status);
	}

	// Get the username/hostname
	char username[FNS_NIS_INDEX], *mapentry;
	int maplen, yperr, user_host = FNSP_NIS_UPDATE_USER;
	uid_t u = geteuid();
	if (u == 0) {
		sysinfo(SI_HOSTNAME, username, FNS_NIS_INDEX);
		user_host = FNSP_NIS_UPDATE_HOST;
	} else {
		char userid[FNS_NIS_INDEX];
		sprintf(userid, "%d", geteuid());
		yperr = yp_match((char *) domain, "passwd.byuid", userid,
		    strlen(userid), &mapentry, &maplen);
		if (yperr != 0)
			return (FNSP_nis_map_status(yperr));
		strcpy(username, strtok(mapentry, ":"));
		free (mapentry);
	}

	// Check if the user/host is allowed to perform this operation
	char entry_name[FNS_NIS_INDEX];
	strcpy(entry_name, username);
	strcat(entry_name, "_");
	if ((strncmp(key, entry_name, strlen(entry_name)) != 0) &&
	    (strncmp(&map[strlen(map) - FNS_ATTR_LEN],
	    ".attr", FNS_ATTR_LEN) != 0)) {
		if (old_mapentry)
			free (old_mapentry);
		return (FN_E_CTX_NO_PERMISSION);
	}

	switch (user_host) {
	case FNSP_NIS_UPDATE_USER:
		if (strncmp(map, "fns_user", FNS_USER_LEN) != 0) {
			if (old_mapentry)
				free (old_mapentry);
			return (FN_E_CTX_NO_PERMISSION);
		}
		break;
	case FNSP_NIS_UPDATE_HOST:
		if (strncmp(map, "fns_host", FNS_HOST_LEN) != 0) {
			if (old_mapentry)
				free (old_mapentry);
			return (FN_E_CTX_NO_PERMISSION);
		}
		break;
	}


	// Perform the ONC-RPC call
	if (op == FNSP_map_delete)
		status = fnsp_call_client_update_nis_database(username,
		    user_host, domain, map, key, "",
		    old_mapentry ? old_mapentry : "", op);
	else
		status = fnsp_call_client_update_nis_database(username,
		    user_host, domain, map, key, (char *) value,
		    old_mapentry ? old_mapentry : "", op);
	if (old_mapentry)
		free (old_mapentry);
	return (status);
}

extern "C" unsigned
get_login_name_from_distinguished_name(const char *dn_name,
    char *domainname, int *matched, char **login_name, int *user_host)
{
	unsigned status;

	char lookup_name[FNS_NIS_SIZE];
	char *mapentry;
	int maplen;

	FN_nameset *usernames = 0;
	FN_nameset *hostnames = 0;
	const FN_string *name;
	void *ip;

	// Construct the lookup name
	strcpy(lookup_name, "onc_distinguished_name_");
	strcat(lookup_name, dn_name);

	// First look in fns_user.attr map
	status = FNSP_yp_map_lookup((char *) domainname, "fns_user.attr",
	    lookup_name, strlen(lookup_name), &mapentry, &maplen);
	if (status == FN_SUCCESS)
		usernames = FNSP_nis_sub_context_deserialize(
		    mapentry, status);

	// Next look in fns_host.attr map
	status = FNSP_yp_map_lookup((char *) domainname, "fns_host.attr",
	    lookup_name, strlen(lookup_name), &mapentry, &maplen);
	if (status == FN_SUCCESS)
		hostnames = FNSP_nis_sub_context_deserialize(
		    mapentry, status);

	*matched = 0;
	*login_name = 0;
	if (usernames) {
		*matched += usernames->count();
		name = usernames->first(ip);
		*login_name = strdup((char *) name->str());
		*user_host = FNSP_NIS_UPDATE_USER;
	}
	if (hostnames) {
		*matched += hostnames->count();
		if (*login_name == 0) {
			name = hostnames->first(ip);
			*login_name = strdup((char *) name->str());
			*user_host = FNSP_NIS_UPDATE_HOST;
		}
	}
	if (*matched == 0)
		return (FN_E_NAME_NOT_FOUND);

	delete usernames;
	delete hostnames;
	return (FN_SUCCESS);
}

// Class to enumerate a map
FN_nis_map_enumerate::FN_nis_map_enumerate(const char *d, const char *m)
{
	strcpy(domain, d);
	strcpy(map, m);
	initial = 1;
	inkey = 0;
	from_file = ((FNSP_can_read_from_files(domain, map) ==
	    FN_SUCCESS) ? 1 : 0);
	dbm = 0;
	if (from_file) {
		strcpy(dbmfilename, FNSP_nis_map_dir);
		strcat(dbmfilename, domain);
		strcat(dbmfilename, "/");
		strcat(dbmfilename, map);
	}
}

FN_nis_map_enumerate::~FN_nis_map_enumerate()
{
	if (inkey)
		free (inkey);
	if (dbm)
		dbm_close(dbm);
}

int
FN_nis_map_enumerate::next_from_map(char **outkey, int *outkeylen,
    char **outval, int *outvallen)
{
	int yp_err;
	if (initial) {
		initial = 0;
		if ((yp_err = yp_first(domain, map, outkey, outkeylen,
		    outval, outvallen)) == 0) {
			inkey = (char *) malloc(*outkeylen);
			memcpy(inkey, *outkey, *outkeylen);
			inkeylen = *outkeylen;
			initial = 0;
			return (0);
		} else
			return (yp_err);
	}

	if ((yp_err = yp_next(domain, map, inkey, inkeylen,
	    outkey, outkeylen, outval, outvallen)) == 0) {
		free (inkey);
		inkey = (char *) malloc(*outkeylen);
		memcpy(inkey, *outkey, *outkeylen);
		inkeylen = *outkeylen;
		return (0);
	}
	if (inkey) {
		free (inkey);
		inkey = 0;
	}

	if (yp_err == YPERR_NOMORE) {
		FNSP_compose_next_map_name(map);
		initial = 1;
		return (next_from_map(outkey, outkeylen, outval, outvallen));
	}
	return (yp_err);
}

int
FN_nis_map_enumerate::next_from_file(char **outkey, int *outkeylen,
    char **outval, int *outvallen)
{
	datum dbm_key, dbm_value;
	if (initial) {
		initial = 0;
		dbm = dbm_open(dbmfilename, O_RDONLY, 0444);
		if (dbm == 0)
			return (YPERR_MAP);
		dbm_key = dbm_firstkey(dbm);
		if (dbm_key.dptr == NULL)
			return (YPERR_MAP);
		dbm_value = dbm_fetch(dbm, dbm_key);
		*outkey = (char *) malloc(dbm_key.dsize);
		memcpy(*outkey, dbm_key.dptr, dbm_key.dsize);
		*outkeylen = dbm_key.dsize;
		*outval = (char *) malloc(dbm_value.dsize);
		memcpy(*outval, dbm_value.dptr, dbm_value.dsize);
		*outvallen = dbm_value.dsize;
		return (0);
	}

	dbm_key = dbm_nextkey(dbm);
	if (dbm_key.dptr != NULL) {
		dbm_value = dbm_fetch(dbm, dbm_key);
		*outkey = (char *) malloc(dbm_key.dsize);
		memcpy(*outkey, dbm_key.dptr, dbm_key.dsize);
		*outkeylen = dbm_key.dsize;
		*outval = (char *) malloc(dbm_value.dsize);
		memcpy(*outval, dbm_value.dptr, dbm_value.dsize);
		*outvallen = dbm_value.dsize;
		return (0);
	}
	if (dbm) {
		dbm_close(dbm);
		dbm = 0;
	}
	FNSP_compose_next_map_name(dbmfilename);
	initial = 1;
	return (next_from_file(outkey, outkeylen, outval, outvallen));
}

int
FN_nis_map_enumerate::next(char **outkey, int *outkeylen,
    char **outval, int *outvallen)
{
	if (from_file)
		return (next_from_file(outkey, outkeylen,
		    outval, outvallen));
	return (next_from_map(outkey, outkeylen,
	    outval, outvallen));
}
