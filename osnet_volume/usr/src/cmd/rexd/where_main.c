

#ident	"@(#)where_main.c	1.7	92/07/14 SMI"

/*	@(#)where_main.c 1.5 89/10/03 SMI	*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/param.h>

static char **Argv;		/* saved argument vector (for ps) */
static char *LastArgv;		/* saved end-of-argument vector */

int child = 0;			/* pid of the executed process */
int ChildDied = 0;		/* true when above is valid */
int HasHelper = 0;		/* must kill helpers (interactive mode) */


int	Debug = 0;


void
main(argc, argv)
	char **argv;
{
	char host[MAXPATHLEN];
	char fsname[MAXPATHLEN];
	char within[MAXPATHLEN];
	char *pn;
	int many;

	/*
	 * argv start and extent for setproctitle()
	 */
	Argv = argv;
	if (argc > 0)
		LastArgv = argv[argc-1] + strlen(argv[argc-1]);
	else
		LastArgv = NULL;

	many = argc > 2;
	while (--argc > 0) {

		if ( strcmp( "-d", *argv ) == 0 )
		{
			Debug = 1;
			argv++;
			continue;
		}
		pn = *++argv;
		where(pn, host, fsname, within);
		if (many)
			printf("%s:\t", pn);
		printf("%s:%s%s\n", host, fsname, within);
	}
	exit(0);
	/* NOTREACHED */
}

/*
 *  SETPROCTITLE -- set the title of this process for "ps"
 *
 *	Does nothing if there were not enough arguments on the command
 * 	line for the information.
 *
 *	Side Effects:
 *		Clobbers argv[] of our main procedure.
 */
void
setproctitle(user, host)
	char *user, *host;
{
	register char *tohere;

	tohere = Argv[0];
	if (LastArgv == NULL || 
			strlen(user)+strlen(host)+3 > (LastArgv - tohere))
		return;
	*tohere++ = '-';		/* So ps prints (rpc.rexd)	*/
	sprintf(tohere, "%s@%s", user, host);
	while (*tohere++)		/* Skip to end of printf output	*/
		;
	while (tohere < LastArgv)	/* Avoid confusing ps		*/
		*tohere++ = ' ';
}
