/*
 *
 * Portions Copyright 11/07/97 Sun Microsystems, Inc. All Rights Reserved
 *
 */
#ident   "@(#)ldapmodrdn.c 1.4     99/03/17 SMI"
/* ldapmodrdn.c - generic program to modify an entry's RDN using LDAP */

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
	fprintf(stderr, catgets(slapdcat, 1, 1229, "usage: %s [-cnrv] [-d debug-level] [-h ldaphost] [-p ldapport] [-D binddn] [-w passwd] [ -f file | < entryfile | dn newrdn [newsuperior]] [-Z]\n"), s);
#endif
	fprintf(stderr, catgets(slapdcat, 1, 1230, "usage: %s [-cnrvR] [-d debug-level] [-h ldaphost] [-p ldapport] [-D binddn] [-w passwd] [ -f file | < entryfile | dn newrdn [newsuperior]]\n"), s);

	fprintf( stderr, catgets(slapdcat, 1, 733, "where:\n"));
	fprintf( stderr, catgets(slapdcat, 1, 1232, "  dn\t\t DN of the entry whose DN is to be changed\n"));
	fprintf( stderr, catgets(slapdcat, 1, 1233, "  newrdn\t new RDN to give the entry\n"));
	fprintf( stderr, catgets(slapdcat, 1, 1234, "  newsuperior\t DN of the new parent\n"));
	fprintf( stderr, catgets(slapdcat, 1, 37, "options:\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1203, "  -c\t\tcontinuous mode\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1204, "  -n\t\tshow what would be done but don't actually delete\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1235, "  -r\t\tremove old RDN from the entries\n"));
	fprintf( stderr, catgets(slapdcat, 1, 1205, "  -v\t\trun in verbose mode (diagnostics to standard output)\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1206, "  -R\t\tdo not automatically follow referrals\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1207, "  -d level\tset LDAP debugging level to `level'\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1209, "  -h host\tldap server\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1210, "  -p port\tport on ldap server\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1211, "  -D binddn\tbind dn\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1212, "  -w passwd\tbind passwd (for authentication)\n") );
	fprintf( stderr, catgets(slapdcat, -1, -1, "  -M authentication-mechanism\tCRAM-MD5\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1208, "  -f file\tperform sequence of deletion listed in `file'\n") );
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

	char *myname,*infile, *p, *entrydn, *rdn, buf[ 4096 ];
	FILE *fp;
	int rc, i, remove, havedn, authmethod, err;
	LDAPMod **pmods;

	char * L_newParent = NULL;
	int haverdn = 0;

#ifdef LDAP_SSL
	int			usessl = 0;
#endif


	int cram_md5 = 0;
	struct berval cred;
	char  *authMech = NULL;

	int L_derefOption = LDAP_DEREF_NEVER;
	int L_protoVersion = LDAP_VERSION3;
	int L_referrals = 1;

	char * locale = setlocale(LC_ALL, "");
	i18n_catopen("sdserver");
	ldaplogconfigf(NULL); 


	infile = NULL;
	not = contoper = verbose = remove = 0;

	myname = (myname = strrchr(argv[0], '/')) == NULL ? argv[0] : ++myname;

#ifdef LDAP_SSL
	while (( i = getopt( argc, argv, "cnvrh:p:D:w:d:f:Z" )) != EOF ) 
#endif
		while (( i = getopt( argc, argv, "cnvrh:p:D:w:d:f:RM:" )) != EOF )
		{
			switch( i ) 
			{
			case 'M':   /* authentication mechanism */
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
			case 'd':
#ifdef LDAP_DEBUG
				ldap_debug = lber_debug = atoi( optarg );	/* */
#else
				fprintf( stderr, "compile with -DLDAP_DEBUG for debugging\n" );
#endif
				break;
			case 'f':	/* read from file */
				infile = strdup( optarg );
				break;
			case 'p':
				ldapport = atoi( optarg );
				break;
			case 'n':	/* print adds, don't actually do them */
				++not;
				break;
			case 'v':	/* verbose mode */
				verbose++;
				break;
			case 'r':	/* remove old RDN */
				remove++;
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

	havedn = 0;
	if (argc - optind == 3) 		/* accept as arguments: dn rdn newsuperior */
	{
		if (( L_newParent = strdup( argv[argc - 1] )) == NULL ) 
		{
			perror( "strdup" );
			exit( 1 );
		}

		if (( rdn = strdup( argv[argc - 2] )) == NULL ) 
		{
			perror( "strdup" );
			exit( 1 );
		}

		if (( entrydn = strdup( argv[argc - 3] )) == NULL ) 
		{
			perror( "strdup" );
			exit( 1 );
		}
		++havedn;
	} 
	else if (argc - optind == 2) 		/* accept as arguments: dn rdn */
	{
		if (( rdn = strdup( argv[argc - 1] )) == NULL ) 
		{
			perror( "strdup" );
			exit( 1 );
		}

		if (( entrydn = strdup( argv[argc - 2] )) == NULL ) 
		{
			perror( "strdup" );
			exit( 1 );
		}
		++havedn;
	} 
	else if ( argc - optind != 0 ) 
	{
		fprintf( stderr, catgets(slapdcat, 1, 27, "%s: invalid number of arguments, only two or three allowed\n"), myname);
		usage(argv[0]);
		exit( 1 );
	}

	if ( infile != NULL ) 
	{
		if (( fp = fopen( infile, "r" )) == NULL ) 
		{
			perror( infile );
			exit( 1 );
		}
	} else {
		fp = stdin;
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
	else if ( (rc = ldap_simple_bind_s( ld, binddn, passwd ) ) != LDAP_SUCCESS )
	{  
		/* 
		 * if we switch into V2, may be some options are not implemented in V2
		 */
			  
		if ( rc == LDAP_PROTOCOL_ERROR )
		{  
			printf( catgets(slapdcat, 1, 1236, "LDAP Server is V2: execute command with LDAP V2...\n\n") );
			if ( L_newParent )
			{
				printf( catgets(slapdcat, 1, 1237, "LDAP Server is V2: <newsuperior> argument is ignored...\n") );
				L_newParent = NULL;
			}

			L_protoVersion = LDAP_VERSION;
			ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &L_protoVersion);

			if ( (rc = ldap_simple_bind_s( ld, binddn, passwd ) ) != LDAP_SUCCESS )
			{
				fprintf(stderr, "ldap_simple_bind_s: %s\n", ldap_err2string(rc) );
				exit( 1 );
			} 
		}  
		else 
		{ 
			fprintf(stderr, "ldap_simple_bind_s: %s\n", ldap_err2string(rc) );
			exit( 1 );
		}  
	}  

	rc = 0;
	if (havedn)
	{
		rc = domodrdn(ld, entrydn, rdn, L_newParent, remove);
	}
	else while (	(rc == 0 || contoper) && 
					(fgets(buf, sizeof(buf), fp) != NULL) )

	{
		/* 
		 * The format of the file is one of the following:
		 * 	dn
		 * 	rdn
		 * 	newsuperior
		 * 	<blank lines...>
		 * OR
		 * 	dn
		 * 	rdn
		 * 	<blank lines...>
		 * both types of sequences can be found in the file
		 */
		
		if ( (strlen(buf) == 1) && (fp == stdin) )
			break;

		buf[ strlen( buf ) - 1 ] = '\0';	/* remove nl */
		if ( *buf != '\0' ) 		/* blank lines optional, skip */
		{
			if ( haverdn )		/* first type of sequence */
			{
				if (( L_newParent = strdup( buf )) == NULL ) 
				{
					perror( "strdup" );
					exit( 1 );
				}
				if ( L_newParent && (L_protoVersion == LDAP_VERSION) )
				{
					printf( catgets(slapdcat, 1, 1237, "LDAP Server is V2: <newsuperior> argument is ignored...\n") );
					L_newParent = NULL;
				}
				rc = domodrdn(ld, entrydn, rdn, L_newParent, remove);
				haverdn = 0;
			}
			else if ( havedn ) 		/* have DN, get RDN */
			{
				if (( rdn = strdup( buf )) == NULL ) 
				{
					perror( "strdup" );
					exit( 1 );
				}
				havedn = 0;
				++haverdn;
			}
			else if ( !havedn ) 		/* don't have DN yet */
			{
				if (( entrydn = strdup( buf )) == NULL)
				{
					perror( "strdup" );
					exit( 1 );
				}
				++havedn;
			}
		}
		else
		{
			printf("kex: new line %d\n", rc);
			if ( haverdn )		/* second type of sequence */
			{
				rc = domodrdn(ld, entrydn, rdn, NULL, remove);
				haverdn = 0;
			}
		}
	}
	if ( (rc == 0 || contoper) && haverdn )		/* second type of sequence */
	{
		rc = domodrdn(ld, entrydn, rdn, NULL, remove);
		haverdn = 0;
	}

	ldap_unbind( ld );

	exit( rc );
}

domodrdn( ld, dn, rdn, newsuperior, remove )
	LDAP	*ld;
	char	*dn;
	char	*rdn;
	char	*newsuperior;
	int	remove;	/* flag: remove old RDN */
{
	int	rc = LDAP_SUCCESS;

	if ( verbose )
		printf( catgets(slapdcat, 1, 22, "new RDN: %1$s (%2$skeep existing values)\n"),
						rdn, remove ? "do not " : "" );
 
	printf( catgets(slapdcat, 1, 23, "%1$srenaming entry %2$s\n"), not ? "!" : "", dn );

	if ( !not ) 
	{
		rc = ldap_rename_s( ld, dn, rdn, newsuperior, remove, NULL, NULL );
		if ( rc != LDAP_SUCCESS )
			fprintf(stderr, "ldap_rename_s: %s\n", ldap_err2string(rc));
		else if ( verbose )
			printf( catgets(slapdcat, 1, 32, "rename completed\n") );
	}

	putchar('\n');

	return( rc );
}
