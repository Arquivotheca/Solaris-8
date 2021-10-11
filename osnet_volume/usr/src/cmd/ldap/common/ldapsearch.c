/*
 *
 * Portions Copyright 07/23/97 Sun Microsystems, Inc. All Rights Reserved
 *
 */
#ident   "@(#)ldapsearch.c 1.5     99/03/17 SMI"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <lber.h>
#include <ldap.h>
#include <locale.h>


#define DEFSEP		"="

#ifdef LDAP_DEBUG
extern int ldap_debug, lber_debug;
#endif /* LDAP_DEBUG */


usage( s )
char	*s;
{
	fprintf( stderr, catgets(slapdcat, 1, 33, "usage: %s [options] filter [attributes...]\nwhere:\n"), s );
	fprintf( stderr, catgets(slapdcat, 1, 1238, "	filter\t\tRFC-1558 compliant LDAP search filter\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1239, "	attributes\twhitespace-separated list of attributes to retrieve\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1240, "\t\t\t(if no attribute list is given, all are retrieved)\n") );
	fprintf( stderr, catgets(slapdcat, 1, 37, "options:\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1241, "	-n\t\tshow what would be done but don't actually search\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1242, "	-v\t\trun in verbose mode (diagnostics to standard output)\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1243, "	-t\t\twrite values to files in /tmp\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1244, "	-u\t\tinclude User Friendly entry names in the output\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1245, "	-A\t\tretrieve attribute names only (no values)\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1246, "	-B\t\tdo not suppress printing of non-ASCII values\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1247, "	-L\t\tprint entries in LDIF format (-B is implied)\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1248, "	-R\t\tdo not automatically follow referrals\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1249, "	-d level\tset LDAP debugging level to `level'\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1250, "	-F sep\t\tprint `sep' instead of `=' between attribute names and values\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1251, "	-f file\t\tperform sequence of searches listed in `file'\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1252, "	-b basedn\tbase dn for search\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1253, "	-s scope\tone of base, one, or sub (search scope)\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1254, "	-a deref\tone of never, always, search, or find (alias dereferencing)\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1255, "	-l time lim\ttime limit (in seconds) for search\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1256, "	-z size lim\tsize limit (in entries) for search\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1257, "	-h host\t\tldap server\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1258, "	-p port\t\tport on ldap server\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1259, "	-D binddn\tbind dn\n") );
	fprintf( stderr, catgets(slapdcat, 1, 1260, "	-w passwd\tbind passwd (for authentication)\n") );
	fprintf( stderr, catgets(slapdcat, -1, -1, "	-M authentication-mechanism\tCRAM-MD5\n") );
#ifdef LDAP_SSL
    fprintf( stderr, catgets(slapdcat, 1, 1213, "    -Z \t\tmake an SSL-encrypted connection\n") );
#endif
}

static char	*binddn = NULL;
static char	*passwd = NULL;
static char	*base = NULL;
static char	*ldaphost = NULL;

#ifdef LDAP_SSL
static int	ldapport = 0;	/* 0 means the default port (LDAP_PORT or SSL_LDAP_PORT) */
#else
static int	ldapport = LDAP_PORT;
#endif

static char	*sep = DEFSEP;
static int	verbose, not, includeufn, allow_binary, vals2tmp, ldif;

static int matches = 0;
static int protoVersion = LDAP_VERSION3;

main( argc, argv )
int	argc;
char	**argv;
{
	char		*infile, *filtpattern, **attrs, line[ BUFSIZ ];
	FILE		*fp;
	int			rc, i, first, scope, deref, attrsonly, err;
	int			L_referrals, timelimit, sizelimit, authmethod;
	
#ifdef LDAP_SSL
	int			usessl = 0;
#endif

	LDAP		*ld;

	int cram_md5 = 0;
	struct berval cred;
	char  *authMech = NULL;

	extern char		*optarg;
	extern int		optind;

	int L_res = LDAP_SUCCESS;
	int L_derefOption = LDAP_DEREF_NEVER;
	
	char * locale = setlocale(LC_ALL, "");
	i18n_catopen("sdserver");
   ldaplogconfigf(NULL); 

	infile = NULL;
	deref = verbose = allow_binary = not = vals2tmp =
	attrsonly = ldif = 0;

	L_referrals = 1;

	sizelimit = timelimit = 0;
	scope = LDAP_SCOPE_SUBTREE;

	while (( i = getopt( argc, argv,
#ifdef LDAP_SSL
		"nuvtABLD:s:f:h:b:d:p:F:a:w:l:z:Z"
#endif
		"nuvtRABLD:s:f:h:b:d:p:F:a:w:l:z:M:"
		)) != EOF ) 
	{
		switch( i ) 
		{
         case 'M':   /* authentication mechanism */
            authMech = strdup( optarg );
            if ( strcmp(authMech, LDAP_SASL_CRAM_MD5) == 0 )
               cram_md5 = 1;
            break;
			case 'n':	/* do Not do any searches */
				++not;
				break;
			case 'v':	/* verbose mode */
				++verbose;
				break;
			case 'd':
#ifdef LDAP_DEBUG
				ldap_debug = lber_debug = atoi( optarg );	/* */
#else
				fprintf( stderr, "compile with -DLDAP_DEBUG for debugging\n" );
#endif
				break;
			case 'u':	/* include UFN */
				++includeufn;
				break;
			case 't':	/* write attribute values to /tmp files */
				++vals2tmp;
				break;
			case 'R':	/* don't automatically chase referrals */
				L_referrals = 0;
				break;
			case 'A':	/* retrieve attribute names only -- no values */
				++attrsonly;
				break;
			case 'L':	/* print entries in LDIF format */
				++ldif;
				/* fall through -- always allow binary when outputting LDIF */
			case 'B':	/* allow binary values to be printed */
				++allow_binary;
				break;
			case 's':	/* search scope */
				if ( strncasecmp( optarg, "base", 4 ) == 0 )
					scope = LDAP_SCOPE_BASE;
				else if ( strncasecmp( optarg, "one", 3 ) == 0 )
					scope = LDAP_SCOPE_ONELEVEL;
				else if ( strncasecmp( optarg, "sub", 3 ) == 0 )
					scope = LDAP_SCOPE_SUBTREE;
				else {
					fprintf( stderr, catgets(slapdcat, 1, 61, "scope should be base, one, or sub\n") );
					usage( argv[ 0 ] );
					exit(1);
				}
			break;

			case 'a':	/* set alias deref option */
				if ( strncasecmp( optarg, "never", 5 ) == 0 )
					deref = LDAP_DEREF_NEVER;
				else if ( strncasecmp( optarg, "search", 6 ) == 0 )
					deref = LDAP_DEREF_SEARCHING;
				else if ( strncasecmp( optarg, "find", 4 ) == 0 )
					deref = LDAP_DEREF_FINDING;
				else if ( strncasecmp( optarg, "always", 6 ) == 0 )
					deref = LDAP_DEREF_ALWAYS;
				else 
				{
					fprintf( stderr, catgets(slapdcat, 1, 62, "alias deref should be never, search, find, or always\n") );
					usage( argv[ 0 ] );
					exit(1);
				}
				break;
				
			case 'F':	/* field separator */
				sep = strdup( optarg );
				break;
			case 'f':	/* input file */
				infile = strdup( optarg );
				break;
			case 'h':	/* ldap host */
				ldaphost = strdup( optarg );
				break;
			case 'b':	/* searchbase */
				base = strdup( optarg );
				break;
			case 'D':	/* bind DN */
				binddn = strdup( optarg );
				break;
			case 'p':	/* ldap port */
				ldapport = atoi( optarg );
				break;
			case 'w':	/* bind password */
				passwd = strdup( optarg );
				break;
			case 'l':	/* time limit */
				timelimit = atoi( optarg );
				break;
			case 'z':	/* size limit */
				sizelimit = atoi( optarg );
				break;
#ifdef LDAP_SSL
			case 'Z':	/* use SSL */
				usessl = 1;
				break;
#endif
			default:
				usage( argv[0] );
				exit(1);
		}
	}

	if ( argc - optind < 1 )
	{
		usage( argv[ 0 ] );
		exit(1);
	}

	filtpattern = strdup( argv[ optind ] );
	if ( argv[ optind + 1 ] == NULL )
		attrs = NULL;
	else 
		attrs = &argv[ optind + 1 ];

	if ( infile != NULL ) 
	{
		if ( infile[0] == '-' && infile[1] == '\0' ) 
		{
			fp = stdin;
		} 
		else if (( fp = fopen( infile, "r" )) == NULL ) 
		{
			perror( infile );
			exit( 1 );
		}
	}

	if ( verbose ) {
		printf( catgets(slapdcat, 1, 63, "ldap_init( %1$s, %2$d )\n"), ldaphost, ldapport );
	}

    /*
     * ask user for password
     */
    if ( binddn && !passwd ) {
	    passwd = getpassphrase( catgets(slapdcat, 1, 1214, "Bind Password:"));
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
			perror( ldaphost );
			exit( 1 );
		}
	} 
	else 
	{
		if (( ld = ldap_init( ldaphost, ldapport )) == NULL ) 
		{
			perror( ldaphost );
			exit( 1 );
		}
	}
#else 
	if (( ld = ldap_init( ldaphost, ldapport )) == NULL ) 
	{
		perror( ldaphost );
		exit( 1 );
	}
#endif

	ldap_set_option(ld, LDAP_OPT_DEREF, &L_derefOption);
	ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &protoVersion);
	ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &timelimit);
	ldap_set_option(ld, LDAP_OPT_SIZELIMIT, &sizelimit);
	ldap_set_option(ld, LDAP_OPT_REFERRALS, &L_referrals);
	/* ldap_set_option(ld, LDAP_OPT_REFERRALS, L_referrals ? LDAP_OPT_ON : LDAP_OPT_OFF);
	printf ("OPT_REFERRALS: Success\n"); */

	/* 
	 * If no authentication information is given, try without binding
	 */
	if ( binddn && passwd )
	{
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

				protoVersion = LDAP_VERSION;
				ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &protoVersion);

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
	}

	if ( verbose ) 
	{
		printf( catgets(slapdcat, 1, 64, "filter pattern: %s\nreturning: "), filtpattern );
		if ( attrs == NULL ) 
		{
			printf( catgets(slapdcat, 1, 65, "ALL") );
		} 
		else 
		{
			for ( i = 0; attrs[ i ] != NULL; ++i ) 
			{
				printf( "%s ", attrs[ i ] );
			}
		}
		putchar( '\n' );
	}

	if ( infile == NULL ) 
	{
		rc = dosearch( ld, base, scope, attrs, attrsonly, filtpattern, "" );
	} 
	else 
	{
		rc = 0;
		first = 1;
		while ( 	(rc == 0) && 
					fgets( line, sizeof( line ), fp ) != NULL ) 
		{
			line[ strlen( line ) - 1 ] = '\0';
			if ( !first ) 
			{
				putchar( '\n' );
			} 
			else 
			{
				first = 0;
			}
			rc = dosearch( ld, base, scope, attrs, attrsonly, filtpattern,
				line );
		}
		if ( fp != stdin ) 
		{
			fclose( fp );
		}
	}

	ldap_unbind( ld );
	exit( rc );
}


dosearch( ld, base, scope, attrs, attrsonly, filtpatt, value )
	LDAP	*ld;
	char	*base;
	int		scope;
	char	**attrs;
	int		attrsonly;
	char	*filtpatt;
	char	*value;
{
	char		filter[ BUFSIZ ], **val;
	int			rc, first, err;
	LDAPMessage		*res, *e;

	int L_errcodep;
	char *L_closerDn;
	char *L_errText;

	sprintf( filter, filtpatt, value );

	if ( verbose ) 
	{
		printf( catgets(slapdcat, 1, 66, "filter is: (%s)\n"), filter );
	}

	if ( not ) 
	{
		return( LDAP_SUCCESS );
	}

	if ( (rc=ldap_search( ld, base, scope, filter, attrs, attrsonly)) == -1 ) 
	{
		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &err);
		fprintf(stderr, "ldap_search: %s\n", ldap_err2string(err) );
		return( err );
	}

	matches = 0;
	first = 1;
	while ( (rc = ldap_result( ld, LDAP_RES_ANY, 0, NULL, &res ))
		== LDAP_RES_SEARCH_ENTRY ) 
	{
		matches++;
		e = ldap_first_entry( ld, res );
		if ( !first ) 
		{
			putchar( '\n' );
		} 
		else 
		{
			first = 0;
		}
		print_entry( ld, e, attrsonly );
		ldap_msgfree( res );
	}


	if ( rc == -1 ) 
	{
		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &err);
		fprintf(stderr, "ldap_result: %s\n", ldap_err2string(err) );
		return( 1 );
	}

	L_closerDn = (char *) malloc(50);
	L_errText = (char *) malloc(50);

	if ( (rc=ldap_parse_result(ld, res, 
				&L_errcodep, &L_closerDn, 
				&L_errText, NULL, NULL, 0) ) != LDAP_SUCCESS ) 
	{
		fprintf(stderr, "ldap_parse_result: %s\n", ldap_err2string(rc) );
		ldap_msgfree( res );
		return( 1 );
	}

	if ( L_errcodep != LDAP_SUCCESS )
	{
		/* 
			Do not use ldap_perror, because ld has been updated by 
			ldap_parse_result and now contains the result of this 
			call and no more the result of ldapsearch
		*/
		if ( (L_errcodep == LDAP_PROTOCOL_ERROR) && (protoVersion == LDAP_VERSION3) )
		{  
			printf( catgets(slapdcat, 1, 1216, "LDAP Server is V2: execute command with LDAP V2...\n") );

			protoVersion = LDAP_VERSION;
			ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &protoVersion);
			
			/* rebind with the version 2 */
			if ( (rc=ldap_simple_bind_s( ld, binddn, passwd)) != LDAP_SUCCESS )
			{
				fprintf(stderr, "ldap_simple_bind_s: %s\n", ldap_err2string(rc));
				exit(L_errcodep);
			}
			rc = redo_dosearch(ld, base, scope, attrs, attrsonly, filtpatt, value );
			ldap_msgfree( res );
			return( 1 );
		}   
		else
		{   
			fprintf(stderr, "ldap_search: %s\n", ldap_err2string(L_errcodep));
			if ( L_errText && *L_errText )
				fprintf(stderr, "Additional info: %s\n", L_errText);
			ldap_memfree(L_errText);
			ldap_msgfree( res );
			return( 1 );
		}
	}

	if ( verbose ) 
	{
		printf( catgets(slapdcat, 1, 67, "%d matches\n"), matches );
	}

	ldap_msgfree( res );
	return( rc );
}

int redo_dosearch(ld, base, scope, attrs, attrsonly, filtpatt, value)
	LDAP	*ld;
	char	*base;
	int		scope;
	char	**attrs;
	int		attrsonly;
	char	*filtpatt;
	char	*value;
{
	return dosearch(ld, base, scope, attrs, attrsonly, filtpatt, value );
}


print_entry( ld, entry, attrsonly )
	LDAP	*ld;
	LDAPMessage	*entry;
	int		attrsonly;
{
	char		*a, *dn, *ufn, tmpfname[ 64 ];
	int			i, j, notascii;
	BerElement		*ber;
	struct berval	**bvals;
	FILE		*tmpfp;
	extern char		*mktemp();

	dn = ldap_get_dn( ld, entry );
	if ( ldif ) 
	{
		write_ldif_value( "dn", dn, strlen( dn ));
	} 
	else 
	{
		printf( "%s\n", dn );
	}
	if ( includeufn ) 
	{
		ufn = ldap_dn2ufn( dn );
		if ( ldif ) 
		{
			write_ldif_value( "ufn", ufn, strlen( ufn ));
		} 
		else 
		{
			printf( "%s\n", ufn );
		}
		free( ufn );
	}
	free( dn );

	for ( a = ldap_first_attribute( ld, entry, &ber ); a != NULL;
		a = ldap_next_attribute( ld, entry, ber ) ) 
	{
		if ( attrsonly ) 
		{
			if ( ldif ) 
			{
				write_ldif_value( a, "", 0 );
			} 
			else 
			{
				printf( "%s\n", a );
			}
		} 
		else if (( bvals = ldap_get_values_len( ld, entry, a )) != NULL ) 
		{
			for ( i = 0; bvals[i] != NULL; i++ ) 
			{
				if ( vals2tmp ) 
				{
					sprintf( tmpfname, "/tmp/ldapsearch-%s-XXXXXX", a );
					tmpfp = NULL;

					if ( mktemp( tmpfname ) == NULL ) 
					{
						perror( tmpfname );
					} 
					else if (( tmpfp = fopen( tmpfname, "w")) == NULL ) 
					{
						perror( tmpfname );
					} 
					else if ( fwrite( bvals[ i ]->bv_val,
						bvals[ i ]->bv_len, 1, tmpfp ) == 0 ) 
					{
						perror( tmpfname );
					} 
					else if ( ldif ) 
					{
						write_ldif_value( a, tmpfname, strlen( tmpfname ));
					} 
					else 
					{
						printf( "%s%s%s\n", a, sep, tmpfname );
					}

					if ( tmpfp != NULL ) 
					{
						fclose( tmpfp );
					}
				} 
				else 
				{
					notascii = 0;
					if ( !allow_binary ) 
					{
						for ( j = 0; j < bvals[ i ]->bv_len; ++j ) 
						{
							if ( !isascii( bvals[ i ]->bv_val[ j ] )) 
							{
								notascii = 1;
								break;
							}
						}
					}

					if ( ldif ) 
					{
						write_ldif_value( a, bvals[ i ]->bv_val, bvals[ i ]->bv_len );
					} 
					else 
					{
						printf( "%s%s%s\n", a, sep,
						notascii ? catgets(slapdcat, 1, 68, "NOT ASCII") : bvals[ i ]->bv_val );
					}
				}
			}
			ber_bvecfree( bvals );
		}
	}
}


int
write_ldif_value( char *type, char *value, unsigned long vallen )
{
	char	*ldif;
	char *ldif_type_and_value( char *, char *, int );

	if (( ldif = ldif_type_and_value( type, value, (int)vallen )) == NULL ) 
	{
		return( -1 );
	}

	fputs( ldif, stdout );
	free( ldif );

	return( 0 );
}
