/*
 *
 * Portions Copyright 07/23/97 Sun Microsystems, Inc. All Rights Reserved
 *
 */
#ident   "@(#)ldapdelete.c 1.5     99/03/17 SMI"
/* ldapdelete.c - simple program to delete an entry using LDAP */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <ctype.h>
#include <lber.h>
#include <ldap.h>


static char	*binddn = NULL;
static char	*base = NULL;
static char	*passwd = NULL;
static char	*ldaphost = NULL;

static int	not, verbose, contoper;
static LDAP	*ld;

#ifdef LDAP_SSL
static int	ldapport = 0;	/* 0 means the default port (LDAP_PORT or SSL_LDAP_PORT) */
#else
static int	ldapport = LDAP_PORT;
#endif

#ifdef LDAP_DEBUG
extern int ldap_debug, lber_debug;
#endif

#define safe_realloc( ptr, size )	( ptr == NULL ? malloc( size ) : \
					 realloc( ptr, size ))

usage(s)
	char * s;
{
#ifdef LDAP_SSL
   fprintf(stderr, catgets(slapdcat, 1, 1199, "usage: %s [-cnv] [-d debug-level] [-f file] [-h ldaphost] [-p ldapport] [-D binddn] [-w passwd] [-Z] [dn...]\n"), s);
#endif
   fprintf(stderr, catgets(slapdcat, 1, 1200, "usage: %s [-cnvR] [-d debug-level] [-f file] [-h ldaphost] [-p ldapport] [-D binddn] [-w passwd] [dn...]\n"), s);

   fprintf( stderr, catgets(slapdcat, 1, 733, "where:\n"));
   fprintf( stderr, catgets(slapdcat, 1, 1202, "  dn\t\t one or several distinguished names of entries to delete\n") );
   fprintf( stderr, catgets(slapdcat, 1, 37, "options:\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1203, "  -c\t\tcontinuous mode\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1204, "  -n\t\tshow what would be done but don't actually delete\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1205, "  -v\t\trun in verbose mode (diagnostics to standard output)\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1206, "  -R\t\tdo not automatically follow referrals\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1207, "  -d level\tset LDAP debugging level to `level'\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1208, "  -f file\tperform sequence of deletion listed in `file'\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1209, "  -h host\tldap server\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1210, "  -p port\tport on ldap server\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1211, "  -D binddn\tbind dn\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1212, "  -w passwd\tbind passwd (for authentication) \n") );
	fprintf( stderr, catgets(slapdcat, -1, -1, "  -M authentication-mechanism\tCRAM-MD5\n") );
#ifdef LDAP_SSL
    fprintf( stderr, catgets(slapdcat, 1, 1213, "    -Z \t\tmake an SSL-encrypted connection\n") );
#endif
}

main( argc, argv )
	int		argc;
	char	**argv;
{
	extern char	*optarg;
	extern int	optind;

	FILE		*fp;

	char		*p, buf[ 4096 ];
	int		i, rc, err;

#ifdef LDAP_SSL
	int			usessl = 0;
#endif

	int L_derefOption = LDAP_DEREF_NEVER;
	int L_protoVersion = LDAP_VERSION3;
	int L_referrals = 1;

	int cram_md5 = 0;
	struct berval cred;
	char	*authMech = NULL;

	char *locale = setlocale(LC_ALL, "");
	i18n_catopen("sdserver");

	ldaplogconfigf(NULL);

	not = verbose = contoper = 0;
	fp = NULL;

#ifdef LDAP_SSL
	while (( i = getopt( argc, argv, "nvch:p:D:w:d:f:Z" )) != EOF ) 
#endif
	while (( i = getopt( argc, argv, "nvch:p:D:w:d:f:R:M:" )) != EOF ) 
	{
		switch( i ) 
		{
			case 'M':	/* authentication mechanism */
				authMech = strdup( optarg );
				if ( strcmp(authMech, LDAP_SASL_CRAM_MD5) == 0 )
					cram_md5 = 1;
				break;
			case 'R':   /* don't automatically chase referrals */
				L_referrals = 0;
				break;
			case 'c':	/* continuous operation mode */
				++contoper;
				break;
			case 'h':	/* ldap host */
				ldaphost = strdup( optarg );
				break;
			case 'D':	/* bind DN */
				binddn = strdup( optarg );
				break;
			case 'w':	/* password */
				passwd = strdup( optarg );
				break;
			case 'f':	/* read DNs from a file */
				if (( fp = fopen( optarg, "r" )) == NULL ) 
				{
					perror( optarg );
					exit( 1 );
				}
				break;
			case 'd':
#ifdef LDAP_DEBUG
				ldap_debug = lber_debug = atoi( optarg );	/* */
#else
				fprintf( stderr, "compile with -DLDAP_DEBUG for debugging\n" );
#endif
				break;
			case 'p':
				ldapport = atoi( optarg );
				break;
			case 'n':	/* print deletes, don't actually do them */
				++not;
				break;
			case 'v':	/* verbose mode */
				verbose++;
				break;
#ifdef LDAP_SSL
			case 'Z':	/* use SSL */
				usessl = 1;
				break;
#endif
			default:
				usage(argv[0]);
				exit( 1 );
		}
	}

	if ( fp == NULL ) 
	{
		if ( optind >= argc ) 
			fp = stdin;
	}

	/* 
	 * check coherency of remaining arguments
	 * verify  that no arg starting with a '-' exists 
	 */
	for ( i=optind; i < argc; ++i ) 
	{
		if (argv[i][0] == '-' ) 
		{
			usage(argv[0]);
			exit( 1 );
		}
	}

	/*
	 * ask user for password
	 */
	if ( binddn && !passwd ) {
		passwd = getpassphrase( catgets(slapdcat, 1, 1214, "Bind Password:"));
	}

	/*
	 * dn and password are mandatory
	 */
	if ( !binddn && !passwd ) {
		fprintf(stderr, catgets(slapdcat, 1, 1215, "DN and Bind Password are required.\n"));
		exit(1);
	}
	/*
	 * dn and password are mandatory for cram-md5 authentication
	 */
	if ( cram_md5 && (!binddn || !passwd) ) {
		fprintf(stderr, catgets(slapdcat, 1, 1215, "DN and Bind Password are required.\n"));
		exit(1);
	}
	

#ifdef LDAP_SSL
	if (usessl) 
	{
		if (( ld = ldap_ssl_open( ldaphost, ldapport, NULL )) == NULL ) 
		{
			perror( "ldap_ssl_open" );
			exit( 1 );
		}
	} else 
	{
		if (( ld = ldap_init( ldaphost, ldapport )) == NULL ) 
		{
			perror( "ldap_init" );
			exit( 1 );
		}
	}
#else
	if (( ld = ldap_init( ldaphost, ldapport )) == NULL ) 
	{
		perror( "ldap_init" );
		exit( 1 );
	}
#endif
	
	ldap_set_option(ld, LDAP_OPT_DEREF, &L_derefOption);
	ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &L_protoVersion);
	ldap_set_option(ld, LDAP_OPT_REFERRALS, &L_referrals);

	if ( cram_md5 ) 
	{
		cred.bv_val = passwd;
		cred.bv_len = strlen(passwd);

		if ( ldap_sasl_cram_md5_bind_s(ld, binddn, &cred, NULL, NULL) != LDAP_SUCCESS ){
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &err);
			fprintf(stderr, "ldap_sasl_cram_md5_bind_s: %s\n", ldap_err2string(err));
			exit( 1 );
		}
	}
	else if ( (rc=ldap_simple_bind_s( ld, binddn, passwd)) != LDAP_SUCCESS ) 
	{
		if ( rc == LDAP_PROTOCOL_ERROR )
		{
			printf( catgets(slapdcat, 1, 1216, "LDAP Server is V2: execute command with LDAP V2...\n") );

			L_protoVersion = LDAP_VERSION;
			ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &L_protoVersion);

			if ( (rc=ldap_simple_bind_s( ld, binddn, passwd)) != LDAP_SUCCESS ) 
			{
				fprintf(stderr, "ldap_simple_bind_s: %s\n", ldap_err2string(rc));
				exit( 1 );
			}
		}
		else
		{
			fprintf(stderr, "ldap_simple_bind_s: %s\n", ldap_err2string(rc));
			exit( 1 );
		}
	}

	if ( fp == NULL ) 
	{
		for ( ; optind < argc; ++optind )
			rc = dodelete( ld, argv[ optind ] );
	} 
	else 
	{
		rc = 0;
		while (	(rc == 0 || contoper) && 
					(fgets(buf, (int) sizeof(buf), fp) != NULL) )
		{
			if ( (strlen(buf) == 1) && (fp == stdin) )	/* to stop when \n\n */
				break;
			buf[ strlen( buf ) - 1 ] = '\0';	/* remove trailing newline */
			if ( *buf != '\0' )
			{
				rc = dodelete( ld, buf );
			}
		}
	}

	ldap_unbind( ld );

	exit( rc );
}


dodelete( ld, dn )
	LDAP	*ld;
	char	*dn;
{
	int	rc = LDAP_SUCCESS;

	if ( verbose )
		printf( catgets(slapdcat, 1, 3, "%1$sdeleting entry %2$s\n"), not ? "!" : "", dn?dn:"(nil)" );

	if ( !not ) 
	{
		if (( rc = ldap_delete_s( ld, dn )) != LDAP_SUCCESS ) 
      	fprintf(stderr, "ldap_delete_s: %s\n", ldap_err2string(rc));
		else if ( verbose )
			printf( catgets(slapdcat, 1, 4, "delete completed\n") );
	}

	putchar('\n');

	return( rc );
}
