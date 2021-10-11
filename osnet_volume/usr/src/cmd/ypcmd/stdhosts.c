
/*
 * Copyright (c) 1985, 1986, 1987, 1988, 1989, 1990 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ident	"@(#)stdhosts.c	1.5	99/03/21 SMI"	/* SMI4.1 1.7  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

/*
 * Filter to convert both IPv4 and IPv6 addresses from /etc/hosts or
 * /etc/inet/ipnodes files.
 */

static int ipv4 = -1;
static char *cmd;
static char *any();

void
usage()
{
	fprintf(stderr, "stdhosts [-w] [-n] [in-file]\n");
	fprintf(stderr, "\t-w\tprint malformed warning messages.\n");
	exit(1);
}

main(argc, argv)
	char **argv;
{
	char line[256];
	char adr[256];
	char *trailer;
	int c;
	char dst[INET6_ADDRSTRLEN + 1];
	int warn = 0;
	FILE *fp;

	if (cmd = strrchr(argv[0], '/'))
		++cmd;
	else
		cmd = argv[0];


	while ((c = getopt(argc, argv, "v:wn")) != -1) {
		switch (c) {
		case 'w':
			warn = 1;
			break;
		case 'n':
			ipv4 = 0;
			break;
		default:
			usage();
			exit(1);
		}
	}

	if (optind < argc) {
		fp = fopen(argv[optind], "r");
		if (fp == NULL) {
			fprintf(stderr, "stdhosts: can't open %s\n", argv[1]);
			exit(1);
		}
	} else
		fp = stdin;

	while (fgets(line, sizeof (line), fp)) {
		if (line[0] == '#')
			continue;
		if ((trailer = any(line, " \t")) == NULL)
			continue;
		sscanf(line, "%s", adr);
		/* check for valid addresses */
		if (ipv4) {
			if ((c = inet_pton(AF_INET, adr, (void *)dst)) != 1) {
				if (warn)
					fprintf(stderr,
					"Warning: malformed line ignored:\n%s",
					line);
				continue;
			}
		} else { /* v4 or v6 for ipnodes */
			if ((inet_pton(AF_INET, adr, (void *)dst) != 1) &&
				(inet_pton(AF_INET6, adr, (void *)dst) != 1)) {
				if (warn)
					fprintf(stderr,
					"Warning: malformed line ignored:\n%s",
					line);
				continue;
			}
		}
		while (isspace(*trailer))
			trailer++;
		fprintf(stdout, "%s\t%s",  adr, trailer);
	}	/* while */
	exit(0);
	/* NOTREACHED */
}

/*
 * scans cp, looking for a match with any character
 * in match.  Returns pointer to place in cp that matched
 * (or NULL if no match)
 */
static char *
any(cp, match)
	register char *cp;
	char *match;
{
	register char *mp, c;

	while (c = *cp) {
		for (mp = match; *mp; mp++)
			if (*mp == c)
				return (cp);
		cp++;
	}
	return (NULL);
}
