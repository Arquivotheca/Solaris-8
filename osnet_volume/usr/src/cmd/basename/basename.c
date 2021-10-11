#ident	"@(#)basename.c	1.4	94/08/31 SMI"	/* SVr4.0 1.8	*/

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void output(char *);
static void usage(void);

void
main(argc, argv)
int	argc;
char	**argv;
{
	char	*p;
	char	*string;
	char	*suffix;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (argc == 1)
		output(".");

	if (strcmp(argv[1], "--") == 0) {
		argv++;
		argc--;
	}

	if (argc == 1)
		output(".");

	if (argc > 3)
		usage();

	string = argv[1];
	suffix = (argc == 2) ? NULL : argv[2];

	if (*string == '\0')
		output(".");

	/* remove trailing slashes */
	p = string + strlen(string) -1;
	while ((p >= string) && (*p == '/'))
		*p-- = '\0';

	if (*string == '\0')
		output("/");

	/* skip to one past last slash */
	if ((p = strrchr(string, '/')) != NULL)
		string = p + 1;

	/*
	 * if a suffix is present and is not the same as the remaining
	 * string and is identical to the last characters in the remaining
	 * string, remove those characters from the string.
	 */
	if (suffix != NULL)
		if (strcmp(string, suffix) != NULL) {
			p = string + strlen(string) - strlen(suffix);
			if (strcmp(p, suffix) == NULL)
				*p = '\0';
		}

	output(string);
}

static void
output(char *string)
{
	(void) printf("%s\n", string);
	exit(0);
}

static void usage(void)
{
	(void) fprintf(stderr,
	    gettext("Usage: basename string [ suffix ]\n"));
	exit(1);
}
