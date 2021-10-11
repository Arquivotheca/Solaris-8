/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnutils.cc	1.6	96/09/24 SMI"

#include <rpcsvc/nis.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libintl.h>

#define	HOST_TEMP_FILE "/tmp/hx_nis_hosts"
#define	USER_TEMP_FILE "/tmp/hx_nis_users"
#define	FNS_NIS_INDEX 256

// Return 1 if error encountered; 0 if OK
static int
print_user_entry(char *, nis_object *ent, void *udata)
{
	FILE *outfile = (FILE *)udata;
	uint32_t entry_type, t;

	// extract user name from entry
	t = *(long *)
	    (ent->EN_data.en_cols.en_cols_val[0].ec_value.ec_value_val);
	entry_type = ntohl(t);

	if (entry_type == ENTRY_OBJ) {
		fprintf(stderr, "%s\n",
			gettext("Encountered object that is not an entry"));
		return (1);
	}

	// print out user name
	fprintf(outfile, "%s\n", ENTRY_VAL(ent, 0));
	return (0);
}


// Return 1 if error encountered; 0 if OK
static int
print_host_entry(char *, nis_object *ent, void *udata)
{
	FILE *outfile = (FILE *) udata;
	uint32_t entry_type, t;

	// extract host name from entry
	t = *(long *)
	    (ent->EN_data.en_cols.en_cols_val[0].ec_value.ec_value_val);
	entry_type = ntohl(t);

	if (entry_type == ENTRY_OBJ) {
		fprintf(stderr, "%s\n",
			gettext("Encountered object that is not an entry"));
		return (1);
	}

	// print out canonical host name, host name pair
	fprintf(outfile, "%s %s\n", ENTRY_VAL(ent, 0), ENTRY_VAL(ent, 1));
	return (0);
}

FILE *
get_user_file(const char *program_name, const char *domainname)
{
	unsigned nisflags = 0;   // FOLLOW_LINKS?
	nis_result* res = 0;
	char tname[NIS_MAXNAMELEN+1];
	FILE *userfile;

	unlink(USER_TEMP_FILE);
	userfile = fopen(USER_TEMP_FILE, "w");
	if (userfile == NULL) {
		fprintf(stderr, "%s: %s %s %s\n",
			program_name, gettext("could not open file"),
			USER_TEMP_FILE, gettext("for write"));
		return (NULL);
	}

	sprintf(tname, "passwd.org_dir.%s", domainname);
	if (tname[strlen(tname)-1] != '.')
		strcat(tname, ".");

	res = nis_list(tname, nisflags, print_user_entry, (void *)userfile);
	if ((res->status != NIS_CBRESULTS) &&
	    (res->status != NIS_NOTFOUND)) {
		nis_perror(res->status, "can't list table");
	}
	nis_freeresult(res);
	fclose(userfile);

	// open file for read
	if ((userfile = fopen(USER_TEMP_FILE, "r")) == NULL) {
		fprintf(stderr, "%s: %s %s %s\n",
			program_name, gettext("could not open file"),
			USER_TEMP_FILE, gettext("for read"));
		return (NULL);
	}
	return (userfile);
}

FILE *
get_host_file(const char *program_name, const char *domainname)
{
	FILE *hostfile;
	char tname[NIS_MAXNAMELEN+1];
	unsigned nisflags = 0;   // FOLLOW_LINKS?
	nis_result* res = 0;

	unlink(HOST_TEMP_FILE);
	hostfile = fopen(HOST_TEMP_FILE, "w");
	if (hostfile == NULL) {
		fprintf(stderr, "%s: %s %s %s\n",
			program_name, gettext("could not open file"),
			HOST_TEMP_FILE, gettext("for write"));
		return (NULL);
	}

	sprintf(tname, "hosts.org_dir.%s", domainname);
	if (tname[strlen(tname)-1] != '.')
		strcat(tname, ".");

	res = nis_list(tname, nisflags, print_host_entry, (void *)hostfile);
	if ((res->status != NIS_CBRESULTS) &&
	    (res->status != NIS_NOTFOUND)) {
		nis_perror(res->status, "can't list table");
	}
	nis_freeresult(res);
	fclose(hostfile);

	// open file for read
	if ((hostfile = fopen(HOST_TEMP_FILE, "r")) == NULL) {
		fprintf(stderr, "%s: %s %s %s\n",
			program_name, gettext("could not open file"),
			HOST_TEMP_FILE, gettext("for read"));
		return (NULL);
	}
	return (hostfile);
}

void
free_host_file(FILE *hostfile)
{
	fclose(hostfile);
	unlink(HOST_TEMP_FILE);
}

void
free_user_file(FILE *userfile)
{
	fclose(userfile);
	unlink(USER_TEMP_FILE);
}


// NIS functions
static int
print_host_map_entry(int instatus, char /* *inkey */, int /* inkeylen */,
    char *inval, int invallen, char *indata)
{
	FILE *wf = (FILE *) indata;
	char line[FNS_NIS_INDEX], *host_name;
	if (instatus == YP_TRUE) {
		strncpy(line, inval, invallen);
		line[invallen] = '\0';
		// Get over the IP address
		host_name = strpbrk(line, " \t");
		if (host_name != 0) {
			while (((*host_name) == ' ') ||
			    ((*host_name) == '\t'))
				host_name++;
			// Copy the hostnames
			if (host_name[strlen(host_name)-1] == '\n')
				fprintf(wf, "%s", host_name);
			else
				fprintf(wf, "%s\n", host_name);
		}
		return (0);
	} else
		return (1);
}

static int
print_user_map_entry(int instatus, char *inkey, int inkeylen,
    char * /* inval */, int /* invallen */, char *indata)
{
	FILE *wf;
	char key_char[FNS_NIS_INDEX];
	if (instatus == YP_TRUE) {
		wf = (FILE *) indata;
		strncpy(key_char, inkey, inkeylen);
		key_char[inkeylen] = '\0';
		fprintf(wf, "%s\n", key_char);
		return (0);
	} else
		return (1);
}

FILE *
get_nis_host_file(const char *program_name, const char *domain)
{
	FILE *hostfile;
	struct ypall_callback yp_callback;

	hostfile = fopen(HOST_TEMP_FILE, "w");
	if (hostfile == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for write\n"),
		    program_name, HOST_TEMP_FILE);
		return (NULL);
	}
	yp_callback.data = (char *) hostfile;
	yp_callback.foreach = (int (*) ()) print_host_map_entry;

	if (yp_all((char *) domain, "hosts.byaddr", &yp_callback) != 0) {
		fclose(hostfile);
		return (NULL);
	}

	fclose(hostfile);
	// open file for read
	if ((hostfile = fopen(HOST_TEMP_FILE, "r")) == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for read\n"),
		    program_name, HOST_TEMP_FILE);
		return (NULL);
	}
	return (hostfile);
}

FILE *
get_nis_user_file(const char *program_name, const char *domain)
{
	FILE *userfile;
	struct ypall_callback yp_callback;

	userfile = fopen(USER_TEMP_FILE, "w");
	if (userfile == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for write\n"),
		    program_name, USER_TEMP_FILE);
		return (NULL);
	}
	yp_callback.data = (char *) userfile;
	yp_callback.foreach = (int (*) ()) print_user_map_entry;

	if (yp_all((char *) domain, "passwd.byname", &yp_callback) != 0) {
		fclose(userfile);
		return (NULL);
	}

	fclose(userfile);
	// open file for read
	if ((userfile = fopen(USER_TEMP_FILE, "r")) == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for read\n"),
		    program_name, USER_TEMP_FILE);
		return (NULL);
	}
	return (userfile);
}

// Files routines
FILE *
get_files_host_file(const char *program_name, const char * /* domain */)
{
	FILE *hostfile;
	FILE *hostsfile;

	hostfile = fopen(HOST_TEMP_FILE, "w");
	hostsfile = fopen("/etc/hosts", "r");
	if ((hostfile == NULL) || (hostsfile == NULL)) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for write\n"),
		    program_name, HOST_TEMP_FILE);
		return (NULL);
	}

	char line[FNS_NIS_INDEX];
	char *host_name;
	while (fgets(line, FNS_NIS_INDEX, hostsfile)) {
		// Get over any comment lines
		if (line[0] == '#')
			continue;
		// Get over the IP address
		host_name = strpbrk(line, " \t");
		if (host_name == 0)
			continue;
		while (((*host_name) == ' ') ||
		    ((*host_name) == '\t'))
			host_name++;
		// Copy the hostnames
		if (host_name[strlen(host_name)-1] == '\n')
			fprintf(hostfile, "%s", host_name);
		else
			fprintf(hostfile, "%s\n", host_name);
	}

	fclose(hostfile);
	fclose(hostsfile);
	// open file for read
	if ((hostfile = fopen(HOST_TEMP_FILE, "r")) == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for read\n"),
		    program_name, HOST_TEMP_FILE);
		return (NULL);
	}
	return (hostfile);
}

FILE *
get_files_user_file(const char *program_name, const char * /* domain */)
{
	FILE *userfile;
	FILE *passwdfile;

	userfile = fopen(USER_TEMP_FILE, "w");
	passwdfile = fopen("/etc/passwd", "r");
	if ((userfile == NULL) || (passwdfile == NULL)) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for write\n"),
		    program_name, USER_TEMP_FILE);
		return (NULL);
	}

	char line[FNS_NIS_INDEX];
	char username[FNS_NIS_INDEX];
	char *ptr;
	while (fgets(line, FNS_NIS_INDEX, passwdfile)) {
		if (line[0] == '#')
			continue;
		ptr = strchr(line, ':');
		if (ptr == NULL)
			continue;
		strncpy(username, line, (ptr-line));
		username[ptr-line] = '\0';
		fprintf(userfile, "%s\n", username);
	}

	fclose(userfile);
	fclose(passwdfile);
	// open file for read
	if ((userfile = fopen(USER_TEMP_FILE, "r")) == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for read\n"),
		    program_name, USER_TEMP_FILE);
		return (NULL);
	}
	return (userfile);
}
