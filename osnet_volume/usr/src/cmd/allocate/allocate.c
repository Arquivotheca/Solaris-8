#if !defined(lint) && defined(SCCSIDS)
static char	*bsm_sccsid = "@(#)allocate.c 1.11 99/04/07 SMI; SunOS BSM";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <locale.h>
#include "allocate.h"

#if !defined(TEXT_DOMAIN)
#define		TEXT_DOMAIN 	"SUNW_OST_OSCMD"
#endif

extern int	errno;
extern char	*optarg;
extern int	optind, opterr;

extern void audit_allocate_argv();
extern int audit_allocate_record();

void
usage(func)
	int func;
{
	char *use[5];

	use[0] = gettext("allocate [-s] [-U uname] [-F] device");
	use[1] = gettext("allocate [-s] [-U uname] -g dev_type");
	use[2] = gettext("deallocate [-s] [-F] device");
	use[3] = gettext("deallocate [-s] [-I]");
	use[4] = gettext("list_devices [-s] [-U uname] {-l|-n|-u} [device]");

	switch (func) {
		case 0:
			fprintf(stderr, "%s\n%s\n", use[0], use[1]);
			break;
		case 1:
			fprintf(stderr, "%s\n%s\n", use[2], use[3]);
			break;
		case 2:
			fprintf(stderr, "%s\n", use[4]);
			break;
		default:
			fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
				use[0], use[1], use[2], use[3], use[4]);
	}
}

void
print_error(error, name)
int	error;
char	*name;
{
	char *msg;

	switch (error) {
	case SYSERROR:
		msg = gettext("Unknown System error.");
		break;
	case IMPORT_ERR:
		msg = gettext(
			"User lacks authorization required for this operation.");
		break;
	case NODAENT:
		msg = gettext(
			"No device allocate file entry for specified device.");
		break;
	case NODMAPENT:
		msg = gettext(
			"No device maps file entry for specified device.");
		break;
	case DEVRNG:
		msg = gettext(
			"Operation inconsistent with device's label range.");
		break;
	case DACLCK:
		msg = gettext(
			"Can't lock the DAC file for the device specified.");
		break;
	case DACACC:
		msg = gettext(
			"Can't access DAC file for the device specified.");
		break;
	case DEVLST:
		msg = gettext(
			"Could not use device list for the device specified.");
		break;
	case NDEALLOC:
		msg = gettext("Specified device is already allocated.");
		break;
	case NALLOCU:
		msg = gettext(
		"Specified device is allocated to another user or label.");
		break;
	case HASOPNS:
		msg = gettext(
			"Can't deallocate specified device device has opens.");
		break;
	case NOMACEX:
		msg = gettext("Error while calling macexempt(2).");
		break;
	case NOTROOT:
		msg = gettext(
			"Not authorized for specified operation.");
		break;
	case NOTEROOT:
		msg = gettext("Must run set-uid root.");
		break;
	case CNTFRC:
		msg = gettext("Can't force deallocate specified device.");
		break;
	case CNTDEXEC:
		msg = gettext(
		"Can't exec device-clean program for specified device.");
		break;
	case NO_DEVICE:
		msg = gettext(
			"Can't find a device of type requested to allocate.");
		break;
	case DSPMISS:
		msg = gettext(
			"Device special file(s) missing for specified device.");
		break;
	case ALLOCERR:
		msg = gettext("Device specified is in allocate error state.");
		break;
	case FALLERR:
		msg = gettext("Device(s) could not be force deallocated.");
		break;
	case CHMOD_PERR:
		msg = gettext("Process lacks Privilege required to chmod().");
		break;
	case CHOWN_PERR:
		msg = gettext("Process lacks Privilege required to chown().");
		break;
	case ALLOC:
		msg = gettext("Device already allocated.");
		break;
	case ALLOC_OTHER:
		msg = gettext("Device allocated to another user.");
		break;
	case NALLOC:
		msg = gettext("Device not allocated.");
		break;
	case AUTHERR:
		msg = gettext("Device not allocatable.");
		break;
	case MACERR:
		msg = gettext("Process label doesn't dominate device label.");
		break;
	case CLEAN_ERR:
		msg = gettext("Unable to clean up the device.");
		break;
	default:
		msg = gettext("Unknown error code.");
		break;
	}

	(void) fprintf(stderr, "%s: %s\n", name, msg);
	(void) fflush(stderr);
}

char *newenv[] = {"PATH=/usr/bin:/usr/sbin",
			NULL,			/* for NLSPATH		*/
			NULL,			/* for LC_ALL		*/
			NULL,			/* for LC_MESSAGES	*/
			NULL,			/* for LANG		*/
			NULL
};
extern char **environ;

static char *get_env_by_name(name)
char	*name;
{
	char **env =environ;
	for (; *env != NULL; env++) {
		if (strncmp(*env, name, strlen(name)) == 0)
			return *env;
	}
	return NULL;
}

main(argc, argv)
int	argc;
char	*argv[];
{
	char	*name, *env;
	int	func = -1, optflg = 0, errflg = 0, error = 0, uid, c;
	char	*uname = NULL, *label = NULL, *device = NULL;
	struct passwd *pw_ent;
	int env_num = 1;

	/*	for internationalization	*/
	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	/*	get all enviroment variables
	 *	which affect on internationalization.
	 */
	env = get_env_by_name("NLSPATH");
	if (env != NULL)
		newenv[env_num++] = env;
	env = get_env_by_name("LC_ALL");
	if (env != NULL)
		newenv[env_num++] = env;
	env = get_env_by_name("LC_MESSAGES");
	if (env != NULL)
		newenv[env_num++] = env;
	env = get_env_by_name("LANG");
	if (env != NULL)
		newenv[env_num] = env;

	environ = newenv;

	if ((name = strrchr(argv[0], '/')) == NULL)
		name = argv[0];
		else
		name++;

	if (!strcmp(name, "allocate"))
		func = 0;
	else if (!strcmp(name, "deallocate"))
		func = 1;
	else if (!strcmp(name, "list_devices"))
		func = 2;
	else {
		usage(ALL);
		exit(1);
	}

	audit_allocate_argv(func, argc, argv);	/* BSM */

		while ((c = getopt(argc, argv, "slnugIU:F")) != -1)
			switch (c) {
			case 's':
				optflg |= SILENT;
				break;
			case 'U':
				optflg |= USER;
				uname = optarg;
				break;
			case 'g':
				optflg |= TYPE;
				break;
			case 'l':
				optflg |= LIST;
				break;
			case 'n':
				optflg |= FREE;
				break;
			case 'u':
				optflg |= CURRENT;
				break;
			case 'F':
				optflg |= FORCE;
				break;
			case 'I':
				optflg |= FORCE_ALL;
				break;
			case '?':
				errflg++;
				break;
			default :
				(void) fprintf(stderr, gettext(
					"Bad option '%c'\n"), c);
			}

	if (optind < argc) {
		device = argv[optind];
	}

	if (device == NULL && !(optflg & (LIST | FREE | CURRENT | FORCE_ALL)))
		errflg++;

	if (errflg) {
		usage(func);
		exit(2);
	}
	if (optflg & USER) {
		if ((pw_ent = getpwnam(uname)) == NULL) {
			(void) fprintf(stderr, gettext(
				"Invalid user name -- %s -- \n"),
				uname);
			exit(4);
		}
		uid = pw_ent->pw_uid;
	}


	if (func == 0) {
		if (optflg & ~ALLOC_OPTS) {
			usage(func);
			exit(3);
		} else
			error = allocate(optflg, uid, device, label);
	} else if (func == 1) {
		if (optflg & ~DEALLOC_OPTS) {
			usage(func);
			exit(3);
		} else
			error = deallocate(optflg, device);
	} else if (func == 2) {
		if (optflg & ~LIST_OPTS) {
			usage(func);
			exit(3);
		} else
			error = list_devices(optflg, uid, device);
	}
	(void) audit_allocate_record(error);	/* BSM */

	if (error) {
		if (!(optflg & SILENT))
			print_error(error, name);
		exit(error);
	} else
		exit(0);
}
