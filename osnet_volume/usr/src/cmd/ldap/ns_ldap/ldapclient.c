/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ldapclient.c	1.3	99/11/11 SMI"

/*
 * ldapclient command. To make (initiailize) or uninitialize a machines as
 * and LDAP client.  This command MUST be run as root (or it will simply exit).
 *
 *	IPaddr	An IP address (dotted quad) with an optional port number.
 *		The default port number is of course 389.
 *	-v	Verbose.  For debugging tell the user what we are doing.
 *	-P X	Initialize an LDAP client from a profile on the server.
 *		EX. -P default.  It will also set an expiration date which
 *		the ldap_cachemgr(1M) can use to automatically update the
 *		file if needed.
 *	-u	Uninitialize this machine.
 *	-i	Initiialze (create) an LDAP client by hand.
 *	-m	Modify the LDAP client configuration on this machine by hand.
 *	-l	List the contents of the LDAP client cache files.
 *
 *	-a X	Authentication method for (-1) none | simple | cram_md5
 *	-b X	Search Base DN. Ex. dc=eng,dc=sun,dc=com
 *	-B X	Alternative Search DN for specific databases.
 *		See nsswitch.conf(4) for database lists like "passwd".
 *		Ex. passwd:(ou=people,dc=corp,dc=sun,dc=com),\
 *			   (ou=people,dc=lab,dc=sun,dc=com)
 *	-D X	Binding DN.  Ex. cn=client,ou=people,cd=eng,dc=sun,dc=com
 *	-w X	Client password not needed for authentication "none".
 *	-O	Only use the servers in the preference list.
 *	-p X	Server preference list. Comma ',' seperated list of IPaddr.
 *	-o X	Timeout value.
 *	-e X	Client info TTL.  If set to 0 this information will not be
 *		automatically updated by the ldap_cachemgr(1M).
 *	-r X	Search derefrence. followref or noref (default followref)
 *	-d X	Hosts lookup domain (DNS)  Ex. eng.sun.com
 *
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <fcntl.h>
#include <xti.h>
#include <strings.h>
#include <limits.h>
#include "../../../lib/libsldap/common/ns_sldap.h"
#include "../../../lib/libsldap/common/ns_internal.h"

char profile[BUFSIZ];
static char *cmd;

typedef enum { S_GEN, S_GPROF, S_HINIT, S_MOD, S_LIST, S_UINIT, S_NONE }
									state_t;

state_t  cond = S_NONE;

int verbose = 0;
int gen = 0;

void usage(void);
int uninit(void);
extern ns_ldap_error_t *__ns_ldap_print_config(int);
extern void __ns_ldap_default_config();
extern int __ns_ldap_download(char *, char *, char *);


char *findDN(char *);

void download(char *, char *);

char *searchbdn = NULL;
char *dname = NULL;
char dname_buf[BUFSIZ];
int dump_cred = 0;
int dump_file = 0;

/* files to (possibiliy) restore */
#define	DOMAINNAME_ORIG "/etc/defaultdomain.orig"
#define	NSSWITCH_ORIG	"/etc/nsswitch.conf.orig"
#define	ROOTKEY_ORIG "/etc/.rootkey.orig"

#define	NIS_COLDSTART_ORIG "/var/nis/NIS_COLD_START.orig"
#define	YP_BIND_DIR_ORIG "/var/yp/binding/%s.orig"

#define	DOMAINNAME "/etc/defaultdomain"
#define	NSSWITCH_CONF	"/etc/nsswitch.conf"
#define	ROOTKEY "/etc/.rootkey"

#define	NIS_COLDSTART "/var/nis/NIS_COLD_START"
#define	YP_BIND_DIR "/var/yp/binding/"

/* Files that need to be just removed */
#define	NIS_PRIVATE_CACHE "/var/nis/.NIS_PRIVATE_DIRCACHE"
#define	NIS_SHARED_CACHE "/var/nis/NIS_SHARED_DIRCACHE"
#define	NIS_CLIENT_INFO "/var/nis/client_info"
#define	LDAP_CACHE_LOG "/var/ldap/cachemgr.log"


void
main(argc, argv)
	int argc;
	char **argv;
{
	char *s_time = NULL;
	char *cache_ttl = NULL;
	char *pref_only = NULL;
	char *ref = NULL;
	char *prefsrv = NULL;
	char *passwd = NULL;
	char *binddn = NULL;
	char *asbdn = NULL;
	char *temp, server_addr[BUFSIZ];
	char *auth = NULL;
	int  foundsrv = 0; /* any server addresses on the command line? */
	int  needpwd = 0; /* other then NONE need a password */
	int c, rc, ret;
	extern char *optarg;
	extern int optind;
	ns_ldap_error_t *errorp;
	int sysinfostatus;


	/*
	 * Determine command invoked (ldapclient, or ldap_gen_profile)
	 */
	if (cmd = strrchr(argv[0], '/'))
		++cmd;
	else
		cmd = argv[0];

	profile[0] = '\0';
	cond = S_NONE;
	if (0 == strcmp(cmd, "ldap_gen_profile")) {
		gen = 1;
		cond = S_GEN;
		__ns_ldap_default_config();
	} else {
		if (0 != getuid()) {
			puts(
			"You must be root (SuperUser) to run this command.");
			usage();
		}
		sysinfostatus = sysinfo(SI_SRPC_DOMAIN, dname_buf, BUFSIZ);
		if (0 < sysinfostatus)
			dname = &dname_buf[0];
	}

	__ns_ldap_setServer(TRUE);

	while ((c = getopt(argc, argv, "a:b:B:d:D:e:imlOo:p:P:r:t:uvw:")) !=
									EOF) {
		if (verbose)
			fprintf(stderr, "parsing -%c option\n", c);
		switch (c) {
		/* need profile name and ipaddr[:port#] */
		case 'P':
			if (S_NONE == cond)
				cond = S_GPROF;
			else if ((S_GEN != cond) && (S_MOD != cond))
			/* -P is OK with -m to modify profile name */
				usage();
			strcpy(profile, optarg);
			dump_file = 1;
			break;
		case 'l':
			if (S_NONE != cond)
				usage();
			cond = S_LIST;
			if (NULL != (errorp =
					__ns_ldap_print_config(verbose))) {
				fputs("Cannot get print configuration ",
								stderr);
				fputs(errorp->message, stderr);
				__ns_ldap_freeError(&errorp);
				fputc('\n', stderr);
				exit(1);
			}
			exit(0);
			break;

		/* these MUST be specified -i ipaddr[:port#] */
		case 'i':
			if (S_NONE != cond)
				usage();
			cond = S_HINIT;
			/* need to load default config */
			__ns_ldap_default_config();
			strcpy(profile, "");
			rc = __ns_ldap_setParam(NULL, NS_LDAP_PROFILE_P,
						(void *)profile, &errorp);
			if (NS_LDAP_SUCCESS != rc) {
				if (NULL != errorp)
					__ns_ldap_freeError(&errorp);
			}
			dump_cred = 1;
			dump_file = 1;
			break;

		/* these MUST be at least one option specified */
		case 'm':
			if ((S_NONE != cond) && (S_GPROF != cond))
			/* -m is OK with -P to modify profile name */
				usage();
			cond = S_MOD;
			/* need to load current config it */
			if (NULL != (errorp =
					__ns_ldap_LoadConfiguration(NULL))) {
				fputs("Cannot get load configuration ", stderr);
				fputs(errorp->message, stderr);
				__ns_ldap_freeError(&errorp);
				fputc('\n', stderr);
				exit(1);
			}
			break;
		case 'b':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();
			searchbdn = strdup(optarg);
			dump_file = 1;
			break;
		/* can be more then one */
		case 'B':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();
			asbdn = strdup(optarg);
			dump_file = 1;
			break;
		case 'D':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();
			binddn = strdup(optarg);
			dump_cred = 1;
			break;
		/* can be more then one */
		case 'a':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();
			if (0 == strcasecmp(optarg, "none"))
				auth = strdup(
					defconfig[NS_LDAP_AUTH_P].allowed[0]);
			if (0 == strcasecmp(optarg, "simple")) {
				needpwd = 1;
				auth = strdup(
					defconfig[NS_LDAP_AUTH_P].allowed[1]);
			}
			if (strcasecmp(optarg, "cram_md5") == 0) {
				needpwd = 1;
				auth = strdup(
					defconfig[NS_LDAP_AUTH_P].allowed[2]);
			}
			if (verbose)
				fprintf(stderr, "auth set to %s needpwd %d\n",
							auth, needpwd);
			dump_file = 1;
			break;
		case 'r':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();
			if (0 == strcasecmp(optarg, "followref"))
				ref = strdup("NS_LDAP_FOLLOWREF");
			if (0 == strcasecmp(optarg, "noref"))
				ref = strdup("NS_LDAP_NOREF");
			dump_file = 1;
			break;
		case 'd':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
			    (S_GEN != cond) && (S_GPROF != cond))
				usage();
			dname = strdup(optarg);
			dump_file = 1;
			break;
		case 'w':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();
			passwd = strdup(optarg);
			dump_cred = 1;
			break;
		case 'e':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();

			cache_ttl = strdup(optarg);
			dump_file = 1;
			break;
		case 'o':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();
			s_time = strdup(optarg);
			dump_file = 1;
			break;
		case 'p':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();

			prefsrv = strdup(optarg);
			dump_file = 1;
			break;
		/* always produces FALSE! */
		case 'O':
			if ((S_MOD != cond) && (S_HINIT != cond) &&
						(S_GEN != cond))
				usage();
			pref_only = (void *)strdup("NS_LDAP_TRUE");
			dump_file = 1;
			break;
		case 'u':
			if (S_NONE != cond)
				usage();
			cond = S_UINIT;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			break;
		}
	}

	if ((NULL != auth) && (1 == needpwd) && (NULL == passwd)) {
		fputs("You must specify a password with \"-w password\" "
					"for authentication methods\n", stderr);
		fputs("simple and cram_md5.  Please see the man page"
				" ldapclient(1M) for details.\n\n", stderr);
		usage();
	}

	if ((S_MOD == cond) || (S_HINIT == cond) ||
	    (S_GEN == cond) || (S_GPROF == cond)) {
		char *bdn = NULL;

		temp = &server_addr[0];

		if (optind < argc) {
		    strcpy(temp, argv[optind]);
		    foundsrv = 1;
		    do {
			/*
			 * findDN has the NASTY side effect of totally
			 * screwing up the config ptr
			*/
			if ((S_MOD != cond) && (S_GEN != cond) &&
						(NULL == bdn)) {
			    char *cp;
			    int rc;

			    bdn = findDN(argv[optind]);
			    if (NULL == bdn) {
				fputs("cannot find search base DN\n", stderr);
				exit(1);
			    }

			    cp = strdup("NS_LDAP_SCOPE_ONELEVEL");
			    rc = __ns_ldap_setParam(NULL,
				NS_LDAP_SEARCH_SCOPE_P, (void *)cp, &errorp);
			    if (NS_LDAP_SUCCESS != rc) {
				if (NULL != errorp) {
					fputs(errorp->message, stderr);
					fputc('\n', stderr);
					__ns_ldap_freeError(&errorp);
				}
			    }
			    free(cp);
			}
			optind++;
			if (optind == argc)
				break;
			strcat(temp, ",");
			strcat(temp, argv[optind]);
		    } while (optind < argc);
		    if (verbose)
			fprintf(stderr, "Servers addresses %s\n",
								server_addr);
		} else {
		/*
		 * it's OKAY not to have server addresses with -m.
		 * it's also OKAY not to have server address with -P if no
		 * other options are specified.
		 */
		    if ((S_MOD != cond) && !((S_GEN == cond) &&
			(dump_file == 0) && (dump_cred == 0)))
			usage();
		}

	}
	/* here because of findDN */
	if (NULL != auth) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_AUTH_P,
					(void *)auth, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != searchbdn) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_BASEDN_P,
						(void *)searchbdn, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != asbdn) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_DN_P,
						(void *)asbdn, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != binddn) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_BINDDN_P,
						(void *)binddn, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != ref) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_REF_P,
							(void *)ref, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != dname) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_DOMAIN_P,
						(void *)dname, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != passwd) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_BINDPASSWD_P,
						(void *)passwd, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != cache_ttl) {
		/*
		 * By setting the NS_LDAP_CACHETTL_P parameter, this implies
		 * that the NS_LDAP_EXP_P parameter will also be set.
		 */
		rc = __ns_ldap_setParam(NULL, NS_LDAP_CACHETTL_P,
						(void *)cache_ttl, &errorp);
		free(cache_ttl);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != s_time) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_TIME_P,
						(void *)s_time, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != prefsrv) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_SERVER_PREF_P,
						(void *)prefsrv, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}
	if (NULL != pref_only) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_PREF_ONLY_P,
						(void *)pref_only, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp) {
				fputs(errorp->message, stderr);
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
			}
		}
	}

	switch (cond) {
	    case S_GEN:
		if (verbose)
			fputs("Generating LDIF first set server addresses\n",
								stderr);
		if ((dump_file == 0) && (dump_cred == 0) && (0 == foundsrv) &&
				(profile[0] == NULL)) {
			/* dumps the current information from the cachefile */
			ns_ldap_error_t *ret = NULL;

			__ns_ldap_cache_destroy();
			if ((ret = __ns_ldap_LoadConfiguration(NULL)) != NULL) {
				/* failed to load the configuration */
				fputs(
				    "Failed to load the LDAP client cache file",
				    stderr);
				if (ret->message) {
					fputs(" (", stderr);
					fputs(ret->message, stderr);
					fputc(')', stderr);
				}
				fputc('\n', stderr);
				__ns_ldap_freeError(&errorp);
				exit(1);
			}
		} else {
			if (foundsrv) {
				rc = __ns_ldap_setParam(NULL, NS_LDAP_SERVERS_P,
					(void *)&server_addr[0], &errorp);
				if (NS_LDAP_SUCCESS != rc) {
					if (NULL != errorp) {
						fputs(cmd, stderr);
						fputs(": ", stderr);
						fputs(errorp->message, stderr);
						fputc('\n', stderr);
						__ns_ldap_freeError(&errorp);
					}
					break;
				}
			}
			if (profile[0] != NULL) {
				rc = __ns_ldap_setParam(NULL, NS_LDAP_PROFILE_P,
						(void *)profile, &errorp);
				if (NS_LDAP_SUCCESS != rc) {
					if (NULL != errorp) {
						fputs(cmd, stderr);
						fputs(": ", stderr);
						fputs(errorp->message, stderr);
						fputc('\n', stderr);
						__ns_ldap_freeError(&errorp);
					}
					break;
				}
			}
			if (verbose)
				fputs(
				"Generating LDIF first must set profile name\n",
								stderr);
		}
		if (verbose)
			fputs("calling __ns_ldap_DumpLdif(NULL)\n", stderr);
		errorp = __ns_ldap_DumpLdif(NULL);
		if (NULL != errorp) {
			fputs(cmd, stderr);
			fputs(": ", stderr);
			fputs(errorp->message, stderr);
			fputc('\n', stderr);
			__ns_ldap_freeError(&errorp);
		}
		break;
	    case S_MOD:
	    case S_HINIT:
		if (0 != foundsrv) {
			rc = __ns_ldap_setParam(NULL, NS_LDAP_SERVERS_P,
					(void *)&server_addr[0], &errorp);
			if (NS_LDAP_SUCCESS != rc) {
				if (NULL != errorp)
					__ns_ldap_freeError(&errorp);
			}
		}
		if (profile[0] != NULL) {
			rc = __ns_ldap_setParam(NULL, NS_LDAP_PROFILE_P,
					(void *)profile, &errorp);
			if (NS_LDAP_SUCCESS != rc) {
				if (NULL != errorp) {
					fputs(cmd, stderr);
					fputs(": ", stderr);
					fputs(errorp->message, stderr);
					fputc('\n', stderr);
					__ns_ldap_freeError(&errorp);
				}
				break;
			}
		}
		dump_file = 1;

		if (verbose)
			fputs("About to modify this machines configuration"
					" by writing the files\n", stderr);
		if (1 == dump_file) {
		    errorp = __ns_ldap_DumpConfiguration(NSCONFIGFILE);
		    if (NULL != errorp) {
			fprintf(stderr, "%s -m errorp is not NULL; %s\n",
							cmd, errorp->message);
			__ns_ldap_freeError(&errorp);
			exit(1);
		    }
		}
		if (1 == dump_cred) {
		    errorp = __ns_ldap_DumpConfiguration(NSCREDFILE);
		    if (NULL != errorp) {
			fprintf(stderr, "%s -m errorp is not NULL; %s\n",
							cmd, errorp->message);
			__ns_ldap_freeError(&errorp);
			exit(1);
		    }
		}
		if (NULL == dname) {
			fputs("Should never happen, no domainname!\n", stderr);
		} else {
			int fp;

			if (NULL == (fp = open(DOMAINNAME, O_WRONLY|O_CREAT))) {
				fprintf(stderr, "Cannot open %s\n", DOMAINNAME);
				exit(1);
			}
			write(fp, dname, strlen(dname));
			write(fp, "\n", 1);
			close(fp);
		}
		break;

	    case S_GPROF:
		if (verbose)
			fputs("About to configure machine by downloading"
						" a profile\n", stderr);
		download(profile, server_addr);
		if (NULL == dname) {
			fputs("Should never happen, no domainname!\n", stderr);
		} else {
			int fp;

			if (NULL == (fp = open(DOMAINNAME, O_WRONLY|O_CREAT))) {
				fprintf(stderr, "Cannot open %s\n", DOMAINNAME);
				exit(1);
			}
			write(fp, dname, strlen(dname));
			write(fp, "\n", 1);
			close(fp);
		}
		break;

	    case S_UINIT:
		if (verbose)
			fputs("About to restore machine to it's"
					" previous configuration\n", stderr);
		ret = uninit();
		exit(ret);
		break;

	    case S_NONE:
	    default:
		/* Need one of -P | -i | -m | -l | -u */
		usage();
		break;
	}

	exit(0);
}

void
usage(void)
{
	int c;

	if (0 == gen)
	    fprintf(stderr, "Usage: %s -v -P profile addr | -i [ options ]"
					" | -m [ options ] | -u | -l\n", cmd);
	else
	    fprintf(stderr,
		"Usage: %s -P profile -b baseDN [options] <server_addr>\n",
									cmd);
	fputs("\tWould you like a more detailed error message(y/n)? ", stderr);
	c = getchar();
	if (('y' != c) && ('Y' != c)) {
		exit(1);
	}
	if (0 == gen) {
	fputs("\nThis command converts a machine into a"
			" client of an LDAP namespace.  This\n", stderr);
	fputs("must be done by a user with root (SuperUser)"
				" access obviously, and will\n", stderr);
	fputs("fail otherwise.  It can also attempt (given"
				" the -u argument) to restore\n", stderr);
	fputs("this machine to a previous configuration"
				" (using NIS(YP), NIS+ or even\n", stderr);
	fputs("another LDAP name space) so long as the"
				" previous configuration\n", stderr);
	fputs("information (stored in *.orig files) has not"
				" been deleted.\n\n", stderr);
	fputs("The -v option makes everything quite verbose"
				" and the command well tell\n", stderr);
	fputs("you (sometimes in excruciating detail) exactly"
				" what it is doing.\n\n", stderr);

	fputs("One of the five options in the Example's below is required:\n\n",
								stderr);
	fputs("Example 1: Make this machine an LDAP client"
				" by downloading the profile\n", stderr);
	fputs("\t   name from the server at the specified"
				" IP address given.  The port #\n", stderr);
	fputs("\t   is optional and is assumed to be 389"
				" (the standard) if missing.\n\n", stderr);
	fprintf(stderr, "# %s [ -v ] -P profile IP_address[:port#]\n\n", cmd);
	fputs("Example 2: Uninitialize (return) this machine"
				" to it's previous name service\n", stderr);
	fputs("\t   (NIS/NIS+/LDAP).  This command assumes"
				" this machine was made into\n", stderr);
	fputs("\t   an LDAP client by this command.  If that"
				" assumption is incorrect,\n", stderr);
	fputs("\t   this command will fail (and tell you why).\n\n", stderr);
	fprintf(stderr, "# %s [ -v ] -u\n\n", cmd);
	fputs("Example 3: Make this machine an LDAP client"
				" by constructing a client file\n", stderr);
	fputs("\t   from the information given on the command line."
				"  The options are\n", stderr);
	fputs("\t   many but at the very least a server address"
				" is required.  Every-\n", stderr);
	fputs("\t   thing else has a default value or is retrieved"
				" from the server.\n\n", stderr);
	fprintf(stderr, "# %s [ -v ] -i -b baseDN IP_address[:port#]\n\n", cmd);
	fputs("Example 4: Modify this machine's LDAP configuration"
				" by modifying the files\n", stderr);
	fputs("\t   in /var/ldap using the options given on"
				" the command line.\n", stderr);
	fputs("\t   At least one of the many options possible is required.\n\n",
									stderr);
	fprintf(stderr, "# %s [ -v ] -m -a simple -w password\n\n", cmd);
	fputs("Example 5: List this machine LDAP configuration."
				"  Note that although this\n", stderr);
	fputs("\t   information is retrieved from the /var/ldap/* files,"
				" it is not\n", stderr);
	fputs("\t   supported to simply cat(1) the files as they"
					" are not necessarily\n", stderr);
	fputs("\t   in a humanly comprehensible format.\n\n", stderr);
	fprintf(stderr, "# %s -l\n\n", cmd);
	fputs("For all the details read the man page ldapclient(1M).\n",
								stderr);
	} else {	/* ldap_gen_profile */

	fputs("\nThis command generates a profile for the client"
					" of an LDAP namespace.\n", stderr);
	fprintf(stderr, "This profile is meant to be loaded into an LDAP"
				" server at ou=%s\n", _PROFILE_CONTAINER);
	fputs("to be downloaded by LDAP clients using the"
				" ldapclient(1M) -P argument.\n", stderr);
	fputs("This profile will be in LDIF format and sent"
					" to the standard output.\n", stderr);
	fputs("This command may be run by any user (but they "
					"obviously) need permission\n", stderr);
	fputs("to write to the LDAP servers to make this profile"
					" useful for clients.\n", stderr);
	fputs("Example 1: Make a \"default\" profile with only server"
					" at the specified IP", stderr);
	fputs("\t   address given.  The port # is optional and"
					" is assumed to be 389\n", stderr);
	fprintf(stderr, "%% %s -P profile -b baseDN IP_address[:port#]\n\n",
								cmd);
	fputs("Example 2: Make a profile called \"eng\" with simple"
					" authentication.\n", stderr);
	fprintf(stderr, "%% %s -P eng -b baseDN -a simple -w secret"
				" 192.100.200.1 192.200.123.45\n\n", cmd);
	fputs("Example 3: The profile name will default to \"default\"\n",
								stderr);
	fprintf(stderr, "# %s [ -v ] -b baseDN IP_address[:port#]\n\n",
								cmd);
	fprintf(stderr, "For all the details read the man page %s(1M).\n",
								cmd);
	}
	exit(1);
}

/*
 * try to restore the previous name space on this machine
 */
int
uninit(void)
{
	int ret;

	ret = recover();
	if (0 != ret) {
		fputs("Cannot recover the configuration on this machine!\n",
								stderr);
	} else {
		unlink(NSCREDFILE);
		unlink(NSCONFIGFILE);
		system("/usr/lib/ldap/ldap_cachemgr -K >"
						" /dev/null 2> /dev/null");
		unlink(LDAP_CACHE_LOG);
		puts("\n----> You will now need to reboot your machine.");
	}
	return (ret);
}

/*
 * try to restore the previous name space on this machine
 */
int
recover(void)
{
	struct stat buf;
	int l, n, y, rstat, fd;
	char yp_dir[BUFSIZ], yp_dir_orig[BUFSIZ];
	char name[BUFSIZ];
	char *ldapf, *ldapc;

	if (verbose)
		fprintf(stderr, "recover stat(%s, )\n", DOMAINNAME_ORIG);
	if (stat(DOMAINNAME_ORIG, &buf) == -1) {
		fprintf(stderr,
	"%s: can not recover, either this machine was not initialized\n", cmd);
		fputs("\t    by this command or original files were removed\n",
					stderr);
		return (1);
	}
	if (verbose)
		fprintf(stderr, "recover stat(%s, )\n", NSSWITCH_ORIG);
	if (stat(NSSWITCH_ORIG, &buf) == -1) {
		fprintf(stderr, "%s: can not recover, either this machine"
						" was not initialized\n", cmd);
		fputs("\t    by this command or original files were removed\n",
					stderr);
		return (1);
	}
	if (verbose)
		fprintf(stderr, "recover open(%s, )\n", DOMAINNAME_ORIG);
	fd = open(DOMAINNAME_ORIG, O_RDONLY);
	if (verbose)
		fprintf(stderr, "recover read(%s, )\n", DOMAINNAME_ORIG);
	rstat = read(fd, &(name[0]), BUFSIZ);
	if (rstat < 1) {
		fputs("Cannot determine previous domain name\n", stderr);
		return (2);
	} else 	{
		char *ptr;

		ptr = strchr(&(name[0]), '\n');
		*ptr = '\0';
		if (verbose)
			fprintf(stderr, "recover old domainname %s\n", name);
	}

	/* test whether this used to be a LDAP client */
	if (verbose)
		fprintf(stderr, "recover stat(%s.orig, )\n", NSCONFIGFILE);

	ldapf = (char *)malloc(strlen(NSCONFIGFILE) + 6);
	strcpy(ldapf, NSCONFIGFILE);
	ldapf = strcat(ldapf, ".orig");

	/* test whether this used to be a LDAP client */
	if (verbose)
		fprintf(stderr, "recover stat(%s, )\n", ldapf);
	l = stat(ldapf, &buf);

	/* test whether this used to be a NIS+ client */
	if (verbose)
		fprintf(stderr, "recover stat(%s, )\n", NIS_COLDSTART_ORIG);
	n = stat(NIS_COLDSTART_ORIG, &buf);

	/* test whether this used to be a NIS(YP) client */
	sprintf(yp_dir_orig, YP_BIND_DIR_ORIG, name);
	if (verbose)
		fprintf(stderr, "recover stat(%s, )\n", yp_dir_orig);
	y = stat(yp_dir_orig, &buf);

	if (verbose)
		fprintf(stderr, "recover ldap %d nis+ %d yp %d\n", l, n, y);
	if (((0 == l) && (0 == y)) ||
	    ((0 == l) && (0 == n)) ||
	    ((0 == y) && (0 == n))) {
		fputs(
"Cannot determine exactly what configuration this used to be, failed!\n",
									stderr);
		free(ldapf);
		return (4);
	}

#ifdef notdef
	if ((-1 == l) && (-1 == n) && (-1 == y)) {
		/* used to be "files" client nothing to do */
	}
#endif
	if ((0 == l) && (-1 == n) && (-1 == y)) {
		/* used to be LDAP client */
		if (verbose)
			fprintf(stderr, "recover rename(%s, %s)\n",
				ldapf, NSCONFIGFILE);
		rstat = rename(ldapf, NSCONFIGFILE);
		if (0 != rstat)
			fprintf(stderr, "recover rename(%s, %s) failed!\n",
					ldapf, NSCONFIGFILE);

		ldapc = (char *)malloc(strlen(NSCREDFILE) + 6);
		strcpy(ldapc, NSCREDFILE);
		ldapc = strcat(ldapc, ".orig");
		l = stat(ldapc, &buf);
		if (0 != l) {
			rstat = rename(ldapc, NSCREDFILE);
			if (0 != rstat) {
				fprintf(stderr,
					"recover rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
			}
		}
		free(ldapc);
		if (verbose)
			fputs("recover: ldapcachemgr -K\n", stderr);
		system("/usr/lib/ldap/ldap_cachemgr -K >"
						" /dev/null 2> /dev/null");
		if (verbose)
			fputs("recover: pkill -9 ldapcachemgr\n", stderr);
		system("/usr/bin/pkill -9 ldapcachemgr");
		if (verbose)
			fputs("recover: unlink(\"/var/ldap/cachemgr.log\"\n",
								stderr);
		(void) unlink("/var/ldap/cachemgr.log");
	}
	if ((-1 == l) && (0 == n) && (-1 == y)) {
		/* used to be NIS+ client */
		if (verbose)
			fprintf(stderr, "recover rename(%s, %s)\n",
				NIS_COLDSTART_ORIG, NIS_COLDSTART);
		rstat = rename(NIS_COLDSTART_ORIG, NIS_COLDSTART);
		if (0 != rstat)
			fprintf(stderr, "recover rename(%s, %s) failed!\n",
					NIS_COLDSTART_ORIG, NIS_COLDSTART);
		if (verbose)
			fprintf(stderr, "/usr/sbin/nscd -K\n");
		system("/usr/sbin/nscd -K");
		if (verbose)
			fputs("recover: pkill -9 nscd\n", stderr);
		system("/usr/bin/pkill -9 nscd");
	}
	if ((-1 == l) && (-1 == n) && (0 == y)) {
		/* used to be NIS(YP) client */
		strcpy(yp_dir, YP_BIND_DIR);
		strcat(yp_dir, name);
		if (verbose)
			fprintf(stderr, "recover rename(%s, %s)\n",
					yp_dir_orig, yp_dir);
		rstat = rename(yp_dir_orig, yp_dir);
		if (0 != rstat)
			fprintf(stderr, "recover rename(%s, %s) failed!\n",
					yp_dir_orig, yp_dir);
	}

	/* now restore machine configuration */
	if (verbose)
		fprintf(stderr, "recover rename(%s, %s)\n",
					NSSWITCH_ORIG, NSSWITCH_CONF);
	rstat = rename(NSSWITCH_ORIG, NSSWITCH_CONF);
	if (0 != rstat)
		fprintf(stderr, "recover rename(%s, %s) failed\n",
					NSSWITCH_ORIG, NSSWITCH_CONF);
	if (verbose)
		fprintf(stderr, "recover rename(%s, %s)\n",
					DOMAINNAME_ORIG, DOMAINNAME);
	rstat = rename(DOMAINNAME_ORIG, DOMAINNAME);
	if (0 != rstat)
		fprintf(stderr, "recover rename(%s, %s) failed!\n",
					DOMAINNAME_ORIG, DOMAINNAME);

#ifdef LATER
	/* XXX not sure how to do this "right" so comment out right now.  */

	if (verbose)
		fprintf(stderr, "recover stat(%s, )\n", "/var/nis/data.orig");
	n = stat("/var/nis/data.orig", &buf);
	if (-1 != n) {
		l = rename("/var/nis/data.orig", "/var/nis/data");
		l = rename("/var/nis/data.dict.orig", "/var/nis/data.dict");
	}
	sprintf(yp_dir_orig, "/var/yp/%s.orig", name);
	y = stat(yp_dir_orig, &buf);
	if (-1 != y) {
		sprintf(yp_dir, "/var/yp/%s", name);
		if (verbose)
			fprintf(stderr, "recover rename(%s, %s)\n",
							yp_dir_orig, yp_dir);
		l = rename(yp_dir_orig, yp_dir);
	}
#endif
	free(ldapf);
	return (0);
}

/*
 * try to save the current state of this machine.
 * this just overwrites any old saved configration files.
 *
 * Returns 0 or successful save
 * Otherwise returns -1
 */
int
save(void)
{
	struct stat buf;
	int r, d, c, l, n, y, rstat, namelen, ret;
	char yp_dir[BUFSIZ], yp_dir_orig[BUFSIZ];
	char name[BUFSIZ];
	char *ldapf, *ldapc;

	ret = 0;
	if (verbose)
		fprintf(stderr, "save sysinfo\n");
	namelen = BUFSIZ;
	(void) sysinfo(SI_SRPC_DOMAIN, &(name[0]), namelen);
	namelen = strlen(name);
	if (verbose)
		fprintf(stderr, "save stat(%s, \n", NSSWITCH_CONF);
	c =  stat(NSSWITCH_CONF, &buf);
	if (0 == c) {
		if (verbose)
			fprintf(stderr, "save /usr/sbin/nscd -K\n");
		system("/usr/sbin/nscd -K");
		if (verbose)
			fprintf(stderr, "save /usr/bin/pkill -9 nscd\n");
		system("/usr/bin/pkill -9 nscd");
		if (verbose)
			fprintf(stderr, "save rename(%s, %s)\n",
					NSSWITCH_CONF, NSSWITCH_ORIG);
		rstat = rename(NSSWITCH_CONF, NSSWITCH_ORIG);
		if (0 != rstat) {
			fprintf(stderr, "save rename(%s, %s) failed!\n",
					NSSWITCH_CONF, NSSWITCH_ORIG);
			ret = 1;
		}
	} else {
		if (verbose)
			fprintf(stderr, "No %s file!\n", NSSWITCH_CONF);
	}
	if (verbose)
		fprintf(stderr, "save stat(%s, \n", DOMAINNAME);
	d =  stat(DOMAINNAME, &buf);
	if (0 == d) {
		if (verbose)
			fprintf(stderr, "save rename(%s, %s)\n",
						DOMAINNAME, DOMAINNAME_ORIG);
#ifdef notdef
		rstat = rename(DOMAINNAME, DOMAINNAME_ORIG);
		if (0 != rstat) {
			fprintf(stderr, "save rename(%s, %s) failed\n",
						DOMAINNAME, DOMAINNAME_ORIG);
			ret = 1;
		}
#else
		/* to make -u happy make an orig file */
		copy("/etc/defaultdomain", "/etc/defaultdomain.orig");
#endif
	}

	if (verbose)
		fprintf(stderr, "save stat(%s, \n", "/etc/.rootkey");
	r =  stat("/etc/.rootkey", &buf);
	if (0 == r) {
		if (verbose)
			fprintf(stderr, "save rename(%s, %s)\n",
					ROOTKEY, ROOTKEY_ORIG);
		rstat = rename(ROOTKEY, ROOTKEY_ORIG);
		if (0 != rstat) {
			fprintf(stderr, "save rename(%s, %s) failed!\n",
					ROOTKEY, ROOTKEY_ORIG);
			ret = 1;
		}
		if (verbose)
			fputs("save: kill -9 keyserv\n", stderr);
		system("/usr/bin/pkill -9 keyserv");
	} else {
		if (verbose)
			fprintf(stderr, "No %s file!\n", "/etc/.rootkey");
	}

	if (verbose)
		fprintf(stderr, "save stat(%s, \n", NIS_COLDSTART);
	n = stat(NIS_COLDSTART, &buf);
	if (0 == n) {
		if (verbose)
			fputs("save: pkill -9 nis_cachemgr\n", stderr);
		system("/usr/bin/pkill -9 nis_cachemgr");
		if (verbose)
			fprintf(stderr, "save rename(%s, %s)\n",
					NIS_COLDSTART, NIS_COLDSTART_ORIG);
		rstat = rename(NIS_COLDSTART, NIS_COLDSTART_ORIG);
		if (0 != rstat) {
			fprintf(stderr, "save rename(%s, %s) failed!\n",
					NIS_COLDSTART, NIS_COLDSTART_ORIG);
			ret = 1;
		}
		if (verbose)
			fprintf(stderr, "save unlink(%s)\n",
							NIS_SHARED_CACHE);
		unlink(NIS_SHARED_CACHE);
		if (verbose)
			fprintf(stderr, "save unlink(%s)\n",
							NIS_PRIVATE_CACHE);
		unlink(NIS_PRIVATE_CACHE);
	} else {
		if (verbose)
			fprintf(stderr, "No %s file!\n", NIS_COLDSTART);
	}

#ifdef LATER
	/* XXX not sure how to do this "right" so comment out right now.  */

	if (verbose)
		fprintf(stderr, "save stat(%s, \n", "/var/nis/data");
	n = stat("/var/nis/data", &buf);
	if (0 == n) {
		if (verbose)
			fputs("save: pkill -9 rpc.nisd_resolv\n", stderr);
		system("/usr/bin/pkill -9 rpc.nisd_resolv");
		if (verbose)
			fputs("save: pkill -9 rpc.nisd\n", stderr);
		system("/usr/bin/pkill -9 rpc.nisd");
		if (verbose)
			fprintf(stderr, "save rename(%s, %s)\n",
					"/var/nis/data", "/var/nis/data.orig");
		rstat = rename("/var/nis/data", "/var/nis/data.orig");
		if (0 != rstat) {
			fprintf(stderr, "save rename(%s, %s) failed!\n",
					"/var/nis/data", "/var/nis/data.orig");
			ret = 1;
		}
		if (verbose)
			fprintf(stderr, "save rename(%s, %s)\n",
			    "/var/nis/data.dict", "/var/nis/data.dict.orig");
		rstat = rename("/var/nis/data.dict", "/var/nis/data.dict.orig");
		if (0 != rstat) {
			fprintf(stderr, "save rename(%s, %s) failed!\n",
			"/var/nis/data.dict", "/var/nis/data.dict.orig");
			ret = 1;
		}
	}
#endif

	if (verbose)
		fprintf(stderr, "namelen %d\n", namelen);
	/* check for domain name if not set cannot save NIS(YP) state */
	if (0 < namelen) {
		/* moving /var/yp/binding will cause ypbind to core dump */
		strcpy(yp_dir, YP_BIND_DIR);
		strcat(yp_dir, name);
		if (verbose)
			fprintf(stderr, "save stat(%s, \n", yp_dir);
		y = stat(yp_dir, &buf);
		if (0 == y) {
			if (verbose)
				fputs("save: pkill -9 ypbind\n", stderr);
			system("/usr/bin/pkill -9 ypbind");
			system("/usr/bin/sh /etc/init.d/sendmail stop");
			strcpy(yp_dir_orig, yp_dir);
			strcat(yp_dir_orig, ".orig");
			if (verbose)
				fprintf(stderr, "save rename(%s, %s)\n",
							yp_dir, yp_dir_orig);
			rstat = rename(yp_dir, yp_dir_orig);
			if (0 != rstat) {
				fprintf(stderr, "save rename(%s, %s) failed!\n",
							yp_dir, yp_dir_orig);
				ret = 1;
			}
		} else {
			if (verbose)
				fprintf(stderr, "No %s directory!\n", yp_dir);
		}

#ifdef LATER
	/* XXX not sure how to do this "right" so comment out right now.  */

		sprintf(yp_dir, "/var/yp/%s", name);
		if (verbose)
			fprintf(stderr, "save stat(%s, \n", yp_dir);
		y = stat(yp_dir, &buf);
		if (0 == y) {
			if (verbose)
				fputs("save: pkill -9 ypserv\n", stderr);
			system("/usr/bin/pkill -9 ypserv");
			strcpy(yp_dir_orig, yp_dir);
			strcat(yp_dir_orig, ".orig");
			if (verbose)
				fprintf(stderr, "save rename(%s, %s)\n",
							yp_dir, yp_dir_orig);
			rstat = rename(yp_dir, yp_dir_orig);
			if (0 != rstat) {
				fprintf(stderr, "save rename(%s, %s) failed!\n",
							yp_dir, yp_dir_orig);
				ret = 1;
			}
		}
#endif
	}

	if ((0 == y) && (0 == n)) {
		if (verbose)
			fprintf(stderr, "save stat(%s, \n", NSCONFIGFILE);
		l = stat(NSCONFIGFILE, &buf);
		if (0 == l) {

			if (verbose)
				fputs("/usr/lib/ldap/ldap_cachemgr -K\n",
								stderr);
			system("/usr/lib/ldap/ldap_cachemgr -K >"
						" /dev/null 2>/dev/null");
			if (verbose)
				fprintf(stderr,
					"/usr/bin/pkill -9 ldap_cachemgr\n");
			system("/usr/bin/pkill -9 ldap_cachemgr");
			if (verbose)
				fprintf(stderr, "save unlink(%s, \n",
						"/var/ldap/cachemgr.log");
			unlink("/var/ldap/cachemgr.log");
			ldapf = (char *)malloc(strlen(NSCONFIGFILE) + 6);
			strcpy(ldapf, NSCONFIGFILE);
			ldapf = strcat(ldapf, ".orig");
			if (verbose)
				fprintf(stderr, "save rename(%s, %s)\n",
						NSCONFIGFILE, ldapf);
			rstat = rename(NSCONFIGFILE, ldapf);
			if (0 != rstat) {
				fprintf(stderr, "save rename(%s, %s) failed!\n",
						NSCONFIGFILE, ldapf);
				ret = 1;
			}
			ldapc = (char *)malloc(strlen(NSCREDFILE) + 6);
			strcpy(ldapf, NSCREDFILE);
			ldapc = strcat(ldapc, ".orig");
			if (verbose)
				fprintf(stderr, "save rename(%s, %s)\n",
							NSCREDFILE, ldapc);
			rstat = rename(NSCREDFILE, ldapc);
			if (0 != rstat) {
				fprintf(stderr, "save rename(%s, %s) failed!\n",
							NSCREDFILE, ldapc);
				ret = 1;
			}
		} else {
			if (verbose)
				fprintf(stderr, "No %s file!\n", NSCONFIGFILE);
		}
	}

	return (ret);
}


void
download(char *profile, char *addr)
{
	int ret;
	char *p;
	char errmsg[BUFSIZ];

	ret = save();

	if (0 != ret) {
		fputs(
	"Save of system configuration failed!  Attempting recovery.\n",
								stderr);
		ret = recover();
		if (0 != ret) {
			fputs(
"Recovery of systems configuration failed!  Machine needs to be fixed!\n",
								stderr);
		}

		exit(1);
	}
	if (verbose)
		fputs("download save() of systems configuration suceeded.\n",
								stderr);

	if (NULL == searchbdn) {
		fputs("download: failed with no search base DN,"
			" bad server configuration\n", stderr);
		exit(1);
	}
	ret = __ns_ldap_download(profile, addr, searchbdn);
	if (verbose)
		fprintf(stderr, "download ret %d\n", ret);

	if (0 == ret) {
		if (verbose)
			fprintf(stderr,
		"download /bin/cp /etc/nsswitch.ldap /etc/nsswitch.conf");
		copy("/etc/nsswitch.ldap", "/etc/nsswitch.conf");
	} else {
		p = (char *)&(errmsg[0]);
		if (verbose)
			fprintf(stderr, "download p 0x%x\n", p);
		(void) __ns_ldap_err2str(ret, &p);
		if (verbose)
			fprintf(stderr, "download p %s\n", p);
		fputs(
"The download of the profile failed, recovering system state.\n", stderr);
		ret = recover();
		if (0 != ret) {
			fputs(
"Recovery of systems configuration failed!  Machine needs to be fixed!.\n",
								stderr);
		}
	}
	fputs("\n----> You will now need to reboot your machine.\n", stdout);
}


char *
findDN(char *server)
{
	struct stat sbuf;
	int ret;
	ns_ldap_entry_t *entry;
	ns_ldap_result_t *resultp;
	ns_ldap_error_t *errorp;
	char filter[BUFSIZ], *rootDN[100], *R;
	int root_cnt, found_cxt;
	int i, j, k, rc, rstat;
	char *cp, *ldapf, *ldapc;

	if (verbose)
		fputs("findDN: begins\n", stderr);
	system("/usr/lib/ldap/ldap_cachemgr -K > /dev/null 2> /dev/null");
	unlink(LDAP_CACHE_LOG);

	ldapc = (char *)malloc(strlen(NSCREDFILE) + 6);
	strcpy(ldapc, NSCREDFILE);
	ldapc = strcat(ldapc, ".orig");
	ret = stat(NSCREDFILE, &sbuf);
	if (0 == ret) {
		rstat = rename(NSCREDFILE, ldapc);
		if (0 != rstat) {
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							NSCREDFILE, ldapc);
		    return (NULL);
		}
	}

	ldapf = (char *)malloc(strlen(NSCONFIGFILE) + 6);
	strcpy(ldapf, NSCONFIGFILE);
	ldapf = strcat(ldapf, ".orig");
	ret = stat(NSCONFIGFILE, &sbuf);
	if (0 == ret) {
		rstat = rename(NSCONFIGFILE, ldapf);
		if (0 != rstat) {
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							NSCONFIGFILE, ldapf);
		    rename(ldapc, NSCREDFILE);
		    return (NULL);
		}
	}

	if (verbose)
		fputs("findDN: calling __ns_ldap_default_config()\n", stderr);
	__ns_ldap_default_config();

	rc = __ns_ldap_setParam(NULL, NS_LDAP_SERVERS_P, (void *)server,
								&errorp);
	if (NS_LDAP_SUCCESS != rc) {
		if (NULL != errorp)
			__ns_ldap_freeError(&errorp);
		rstat = rename(ldapf, NSCONFIGFILE);
		if (0 != rstat)
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
		free(ldapf);
		rstat = rename(ldapc, NSCREDFILE);
		if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
		free(ldapc);
		return (NULL);
	}

	cp = strdup("NS_LDAP_AUTH_NONE");
	rc = __ns_ldap_setParam(NULL, NS_LDAP_AUTH_P, (void *)cp, &errorp);
	free(cp);
	if (NS_LDAP_SUCCESS != rc) {
		if (NULL != errorp)
			__ns_ldap_freeError(&errorp);
		rstat = rename(ldapf, NSCONFIGFILE);
		if (0 != rstat)
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
		free(ldapf);
		rstat = rename(ldapc, NSCREDFILE);
		if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
		free(ldapc);
		return (NULL);
	}

	cp = strdup("NS_LDAP_SEC_NONE");
	rc = __ns_ldap_setParam(NULL, NS_LDAP_TRANSPORT_SEC_P, (void *)cp,
								&errorp);
	free(cp);
	if (NS_LDAP_SUCCESS != rc) {
		if (NULL != errorp)
			__ns_ldap_freeError(&errorp);
		rstat = rename(ldapf, NSCONFIGFILE);
		if (0 != rstat)
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
		free(ldapf);
		rstat = rename(ldapc, NSCREDFILE);
		if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
		free(ldapc);
		return (NULL);
	}

	cp = strdup("");
	rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_BASEDN_P, (void *)cp,
								&errorp);
	free(cp);
	if (NS_LDAP_SUCCESS != rc) {
		if (NULL != errorp)
			__ns_ldap_freeError(&errorp);
		rstat = rename(ldapf, NSCONFIGFILE);
		if (0 != rstat)
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
		free(ldapf);
		rstat = rename(ldapc, NSCREDFILE);
		if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
		free(ldapc);
		return (NULL);
	}

	cp = strdup("NS_LDAP_SCOPE_BASE");
	rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_SCOPE_P, (void *)cp,
								&errorp);
	free(cp);
	if (NS_LDAP_SUCCESS != rc) {
		if (NULL != errorp)
			__ns_ldap_freeError(&errorp);
		rstat = rename(ldapf, NSCONFIGFILE);
		if (0 != rstat)
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
		free(ldapf);
		rstat = rename(ldapc, NSCREDFILE);
		if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
		free(ldapc);
		return (NULL);
	}

	if (NULL == dname)
		return (NULL);
	strcpy(&filter[0], "(objectclass=*)");
	ret = __ns_ldap_list(NULL, filter, (const char **)NULL, NULL, NULL, 0,
						&resultp, &errorp, NULL, NULL);
	if (NULL == resultp) {
		if (NULL != errorp) {
			fprintf(stderr,
			"error looking up baseDN/domainname: stat %d msg %s\n",
					errorp->status, errorp->message);
		} else {
		    if (verbose)
			fputs("__ns_ldap_list return NULL resultp\n", stderr);
		}
		rstat = rename(ldapf, NSCONFIGFILE);
		if (0 != rstat)
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
		free(ldapf);
		rstat = rename(ldapc, NSCREDFILE);
		if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
		free(ldapc);
		exit(1);
		/* restore too; return(NULL); */
	}
	for (i = 0; i < 100; i++)
		rootDN[i] = NULL;
	root_cnt = 0;
	entry = resultp->entry;
	for (i = 0; i < resultp->entries_count; i++) {
	    for (j = 0; j < entry->attr_count; j++) {
		char *cp;

		cp = entry->attr_pair[j]->attrname;
		if (0 != j) {
		    for (k = 0; entry->attr_pair[j]->attrvalue[k]; k++)
			if (0 == strcasecmp(cp, "namingcontexts")) {
			    if (NULL == rootDN[root_cnt])
			    rootDN[root_cnt++] = strdup(
					entry->attr_pair[j]->attrvalue[k]);
			}
		}
	    }
	    entry = entry->next;
	}
	__ns_ldap_freeResult(&resultp);
	if (verbose)
		fprintf(stderr, "found %d namingcontexts\n", root_cnt);
	if (0 == root_cnt) {
		fputs("Cannot find the rootDN\n", stderr);
		rstat = rename(ldapf, NSCONFIGFILE);
		if (0 != rstat)
		    fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
		free(ldapf);
		rstat = rename(ldapc, NSCREDFILE);
		if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
		free(ldapc);
		exit(1);
	}
	found_cxt = -1;
	for (i = 0; i < root_cnt; i++) {
		rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_BASEDN_P,
						(void *)rootDN[i], &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp)
				__ns_ldap_freeError(&errorp);
			rstat = rename(ldapf, NSCONFIGFILE);
			if (0 != rstat)
				fprintf(stderr,
					"findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
			free(ldapf);
			rstat = rename(ldapc, NSCREDFILE);
			if (0 != rstat)
				fprintf(stderr,
					"findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
			free(ldapc);
			return (NULL);
		}
		cp = strdup("NS_LDAP_SCOPE_SUBTREE");
		rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_SCOPE_P,
							(void *)cp, &errorp);
		free(cp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp)
				__ns_ldap_freeError(&errorp);
			rstat = rename(ldapf, NSCONFIGFILE);
			if (0 != rstat)
				fprintf(stderr,
					"findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
			free(ldapf);
			rstat = rename(ldapc, NSCREDFILE);
			if (0 != rstat)
				fprintf(stderr,
					"findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
			free(ldapc);
			return (NULL);
		}
		snprintf(&filter[0], BUFSIZ,
			"(&(objectclass=nisDomainObject)(nisdomain=%s))",
								dname);
		if (verbose) {
		    fprintf(stderr, "findDN: __ns_ldap_list(NULL, \"%s\"\n",
								filter);
		    fprintf(stderr, "rootDN[%d] %s\n", i, rootDN[i]);
		}
		ret = __ns_ldap_list(NULL, filter, (const char **)NULL, NULL,
					NULL, 0, &resultp, &errorp, NULL, NULL);
		if (NS_LDAP_SUCCESS == ret) {
			found_cxt = i;
			break;
		} else {
		    if (verbose)
			fprintf(stderr,
		"NOTFOUND:Could not find the nisDomainObject for DN %s\n",
								rootDN[i]);
		}
	}
	if (-1 == found_cxt) {
	    rstat = rename(ldapf, NSCONFIGFILE);
	    if (0 != rstat)
		fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
	    free(ldapf);
	    rstat = rename(ldapc, NSCREDFILE);
	    if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
	    free(ldapc);
	    return (NULL);
	}
	if (NULL == resultp) {
	    rstat = rename(ldapf, NSCONFIGFILE);
	    if (0 != rstat)
		fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
	    free(ldapf);
	    rstat = rename(ldapc, NSCREDFILE);
	    if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
	    free(ldapc);
	    return (NULL);
	}
	entry = resultp->entry;
	if (NULL == entry) {
	    rstat = rename(ldapf, NSCONFIGFILE);
	    if (0 != rstat)
		fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
	    free(ldapf);
	    rstat = rename(ldapc, NSCREDFILE);
	    if (0 != rstat)
			fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
	    free(ldapc);
	    return (NULL);
	}
	R = strdup(entry->attr_pair[0]->attrvalue[0]);
	__ns_ldap_freeResult(&resultp);
	/* if -b baseDN was not specified set it from the servers */
	if (NULL == searchbdn) {
		searchbdn = R;
		rc = __ns_ldap_setParam(NULL, NS_LDAP_SEARCH_BASEDN_P,
					(void *)searchbdn, &errorp);
		if (NS_LDAP_SUCCESS != rc) {
			if (NULL != errorp)
				__ns_ldap_freeError(&errorp);
			rstat = rename(ldapf, NSCONFIGFILE);
			if (0 != rstat)
				fprintf(stderr,
					"findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
			free(ldapf);
			rstat = rename(ldapc, NSCREDFILE);
			if (0 != rstat)
				fprintf(stderr,
					"findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
			free(ldapc);
			return (NULL);
		}
		dump_file = 1;
	}

	if (verbose)
		fprintf(stderr, "found baseDN %s for domain %s\n", R, dname);

	/*
	 * The checking for access() after the rename() is to prevent
	 * printing the error message when rename failed because the file
	 * did not exist.
	 */
	rstat = rename(ldapf, NSCONFIGFILE);
	if ((0 != rstat) && (access(ldapf, F_OK) == 0))
		fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapf, NSCONFIGFILE);
	free(ldapf);
	rstat = rename(ldapc, NSCREDFILE);
	if ((0 != rstat) && (access(ldapc, F_OK) == 0))
		fprintf(stderr, "findDN rename(%s, %s) failed!\n",
							ldapc, NSCREDFILE);
	free(ldapc);
	return (R);
}

/*
 * copy contents of file "from" to file "to"
 * return code:
 *	0	Success
 *	1	Failure "to" exists
 *	2	Cannot stat "from" file.
 *	3	Failure could not open "from" for reading
 *	4	Failure could not open "to" for writing?
 *	5	Failure while trying to read "from"
 *	6	Failure in writing to "to.
 *	7	Close of "to" failed.
 */
int
copy(char *from, char *to)
{
	int i, j, n, r;
	struct stat sbuf;
	char mine[BUFSIZ];
	void *buf;

	/* this should fail as the "to" target should not exist */
	if ((j = stat(to, &sbuf)) != -1) {
		return (1);
	}
	if ((i = stat(from, &sbuf)) == -1) {
		return (2);
	}
	if ((i = open(from, O_RDONLY)) == -1) {
		return (3);
	}
	if ((j = open(to, O_WRONLY|O_CREAT, sbuf.st_mode)) == -1) {
		return (4);
	}
	buf = (void *)&mine[0];
	do {
		n = read(i, buf, BUFSIZ);
		if (-1 == n) {
			close(i);
			close(j);
			unlink(to);
			return (5);
		}
		r = write(j, buf, n);
		if (r < n) {
			close(i);
			close(j);
			unlink(to);
			return (6);
		}
	} while (BUFSIZ == n);
	close(i);
	r = close(j);
	if (-1 == r) {
		unlink(to);
		return (7);
	}

	return (0);
}
