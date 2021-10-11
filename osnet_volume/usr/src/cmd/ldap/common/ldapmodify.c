/*
 *
 * Portions Copyright 11/13/97 Sun Microsystems, Inc. All Rights Reserved
 *
 */
#ident   "@(#)ldapmodify.c 1.5     99/03/17 SMI"
/* ldapmodify.c - generic program to modify or add entries using LDAP */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#ifndef VMS
#include <unistd.h>
#endif /* VMS */
#include <locale.h>
#include <lber.h>
#include <ldap.h>


static char	*prog;
static char	*binddn = NULL;
static char	*passwd = NULL;
static char	*ldaphost = NULL;

#ifdef LDAP_SSL
static int	ldapport = 0;	/* 0 means the default port (LDAP_PORT or SSL_LDAP_PORT) */
static	int			usessl = 0;
#else
static int	ldapport = LDAP_PORT;
#endif

static int	new, replace, not, verbose, contoper, force;

int referrals = 1;


static int cram_md5 = 0;

static LDAP	**ld;
static int nbthreads = 1;
static int authmethod;
static int error = 0;
static void * process(void * arg);
static pthread_mutex_t read_mutex = {0};
static pthread_mutex_t wait_mutex = {0};
static pthread_cond_t  wait_cond  = {0};
static	FILE		*fp;

#ifdef LDAP_DEBUG
extern int ldap_debug, lber_debug;
#endif 

#define safe_realloc( ptr, size )	( ptr == NULL ? malloc( size ) : \
					 realloc( ptr, size ))

#define LDAPMOD_MAXLINE		4096

/* strings found in replog/LDIF entries (mostly lifted from slurpd/slurp.h) */
#define T_REPLICA_STR		"replica"
#define T_DN_STR		"dn"
#define T_CHANGETYPESTR		 "changetype"
#define T_ADDCTSTR		"add"
#define T_MODIFYCTSTR		"modify"
#define T_DELETECTSTR		"delete"
#define T_MODRDNCTSTR		"modrdn"
#define T_MODDNCTSTR			"moddn"
#define T_MODOPADDSTR		"add"
#define T_MODOPREPLACESTR	"replace"
#define T_MODOPDELETESTR	"delete"
#define T_MODSEPSTR		"-"
#define T_NEWRDNSTR		"newrdn"
#define T_DELETEOLDRDNSTR	"deleteoldrdn"
#define T_NEWPARENTSTR	"newsuperior"
#define T_VERSION_STR	"version"


#ifdef NEEDPROTOS
static int process_ldapmod_rec( char *rbuf );
static int process_ldif_rec( char *rbuf );
static void addmodifyop( LDAPMod ***pmodsp, int modop, char *attr,
	char *value, int vlen );
static int domodify( char *dn, LDAPMod **pmods, int newentry );
static int dodelete( char *dn );
static int domodrdn( char *dn, char *newrdn, char * newparent, int deleteoldrdn );
static void freepmods( LDAPMod **pmods );
static char *read_one_record( FILE *fp );
#else /* NEEDPROTOS */
static int process_ldapmod_rec();
static int process_ldif_rec();
static void addmodifyop();
static int domodify();
static int dodelete();
static int domodrdn();
static void freepmods();
static char *read_one_record();
#endif /* NEEDPROTOS */

char * str_getline( char **next );

usage(s)
   char * s;
{
#ifdef LDAP_SSL
	fprintf( stderr, catgets(slapdcat, 1, 1217, "usage: %s [-acnrvF] [-d debug-level] [-h ldaphost] [-p ldapport] [-D binddn] [-w passwd] [ -f file | < entryfile ] [-Z] [-l number of ldap connections]\n"), s);
#endif
	fprintf(stderr, catgets(slapdcat, 1, 1218, "usage: %s [-acnrvFR] [-d debug-level] [-h ldaphost] [-p ldapport] [-D binddn] [-w passwd] [ -f file | [-l number of ldap connections] < entryfile ]\n"), s);

   fprintf( stderr, catgets(slapdcat, 1, 37, "options:\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1220, "  -a\t\tadd entries\n"));
   fprintf( stderr, catgets(slapdcat, 1, 1203, "  -c\t\tcontinuous mode\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1221, "  -n\t\tshow what would be done but don't actually do\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1222, "  -r\t\treplace values\n"));
   fprintf( stderr, catgets(slapdcat, 1, 1205, "  -v\t\trun in verbose mode (diagnostics to standard output)\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1223, "  -F\t\tforce application of all changes\n"));
   fprintf( stderr, catgets(slapdcat, 1, 1206, "  -R\t\tdo not automatically follow referrals\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1207, "  -d level\tset LDAP debugging level to `level'\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1209, "  -h host\tldap server\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1210, "  -p port\tport on ldap server\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1211, "  -D binddn\tbind dn\n") );
   fprintf( stderr, catgets(slapdcat, 1, 1212, "  -w passwd\tbind passwd (for authentication)\n") );
	fprintf( stderr, catgets(slapdcat, -1, -1, "  -M authentication-mechanism\tCRAM-MD5\n") );
#ifdef LDAP_SSL
	fprintf( stderr, catgets(slapdcat, 1, 1224, "  -Z \t\tmake an SSL-encrypted connection\n") );
#endif
   fprintf( stderr, catgets(slapdcat, 1, 1225, "  -f file\tread the entry modification information from file instead of from standard input\n"));
   fprintf( stderr, catgets(slapdcat, 1, 1226, "  -l nb-connections\tnumber of LDAP connections\n"));
}

main( argc, argv )
	int		argc;
	char	**argv;
{
	int			rc, i;
	char		*infile;
	extern char	*optarg;
	extern int	optind;

   char  *authMech = NULL;

	char * locale = setlocale(LC_ALL, "");
	i18n_catopen("sdserver");
   ldaplogconfigf(NULL); 

	if (( prog = strrchr( argv[ 0 ], '/' )) == NULL ) 
	{
		prog = argv[ 0 ];
	} 
	else 
	{
		++prog;
	}
	new = ( strcmp( prog, "ldapadd" ) == 0 );

	infile = NULL;
	not = verbose = 0;

#ifdef LDAP_SSL
	while (( i = getopt( argc, argv, "Fabcnrvh:p:D:w:d:f:Zl:" )) != EOF ) 
#endif
	while (( i = getopt( argc, argv, "Fabcnrvh:p:D:w:d:f:l:R:M:" )) != EOF )
	{
		switch( i ) 
		{
			case 'M':
            authMech = strdup( optarg );
            if ( strcmp(authMech, LDAP_SASL_CRAM_MD5) == 0 )
               cram_md5 = 1;
            break;
			case 'R':   /* don't automatically chase referrals */
				referrals = 0;
            break;
			case 'a':	/* add */
				new = 1;
				break;
			case 'c':	/* continuous operation */
				contoper = 1;
				break;
			case 'r':	/* default is to replace rather than add values */
				replace = 1;
				break;
			case 'F':	/* force all changes records to be used */
				force = 1;
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
				fprintf( stderr, "%s: compile with -DLDAP_DEBUG for debugging\n",
					prog );
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
#ifdef LDAP_SSL
			case 'Z':	/* use SSL */
				usessl = 1;
				break;
#endif /* LDAP_SSL */
			case 'l':
				nbthreads = atoi(optarg);
				break;
			default:
				usage(prog);
				exit( 1 );
		}
	}

	if ( argc - optind != 0 ) 
	{
		usage(prog);
		exit( 1 );
	}

	if ( infile != NULL ) 
	{
		if (( fp = fopen( infile, "r" )) == NULL ) 
		{
			perror( infile );
		exit( 1 );
	}
	} else 
	{
		fp = stdin;
	}

	/*
	 * ask user for password
	 */
	if ( binddn && !passwd ) 
	{
		passwd = getpassphrase( catgets(slapdcat, 1, 1214, "Bind Password:"));
	}

	/*
	 * dn and password are mandatory
	 */
	if ( !binddn && !passwd ) 
	{
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


	ld = (LDAP **) calloc (nbthreads+100, sizeof(LDAP *));
	for ( i=0; i<nbthreads; ++i ) 
	{
	  pthread_t tid;
	  thr_create(NULL, 0, process, NULL, NULL, &tid);
	} 
	while ( thr_join(0, NULL, NULL) == 0 );
	exit(error);
}

#define ld ld[thr_self()]
#define exit(a) error|=a;thr_exit(a)

int protoVersion = LDAP_VERSION3;

static void * process(void * arg)
{
	char		*rbuf, *start, *p, *q;
	int rc, use_ldif, err;

	int L_res = LDAP_SUCCESS;
	int L_derefOption = LDAP_DEREF_NEVER;
	int L_ref = (int) LDAP_OPT_ON;

	struct berval cred;

	if ( !not ) 
	{
#ifdef LDAP_SSL
		if (usessl) 
		{
			if (( ld = ldap_ssl_open( ldaphost, ldapport, NULL )) == NULL ) 
			{
				perror( "ldap_ssl_open" );
				exit( 1 );
			}
		} 
		else 
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
		ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &protoVersion);
		ldap_set_option(ld, LDAP_OPT_REFERRALS, &referrals);


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
   	else if ( (L_res = ldap_simple_bind_s( ld, binddn, passwd ) ) != LDAP_SUCCESS ) 
		{
			if ( L_res == LDAP_PROTOCOL_ERROR )
			{
				printf( catgets(slapdcat, 1, 1216, "LDAP Server is V2: execute command with LDAP V2...\n") );

				protoVersion = LDAP_VERSION;
				ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &protoVersion);

				if ( (L_res = ldap_simple_bind_s( ld, binddn, passwd ) ) != LDAP_SUCCESS ) 
				{
					fprintf(stderr, "ldap_simple_bind_s: %s\n", ldap_err2string(L_res) );
					exit( 1 );
				}
			}
			else
			{
				fprintf(stderr, "ldap_simple_bind_s: %s\n", ldap_err2string(L_res) );
				exit( 1 );
			}
		}
	}

	rc = 0;

	while (	(rc == 0 || contoper) &&
				(rbuf = read_one_record( fp )) != NULL) 
	{
		/*
		 * we assume record is ldif/slapd.replog if the first line
		 * has a colon that appears to the left of any equal signs, OR
		 * if the first line consists entirely of digits (an entry id)
		 */
		use_ldif = ( p = strchr( rbuf, ':' )) != NULL &&
			( q = strchr( rbuf, '\n' )) != NULL && p < q &&
			(( q = strchr( rbuf, '=' )) == NULL || p < q );

		start = rbuf;

		if ( !use_ldif && ( q = strchr( rbuf, '\n' )) != NULL ) 
		{
			for ( p = rbuf; p < q; ++p ) 
			{
				if ( !isdigit( *p )) 
				{
					break;
				}
			}
			if ( p >= q ) 
			{
				use_ldif = 1;
				start = q + 1;
			}
		}

		if ( use_ldif ) 
		{
			rc = process_ldif_rec( start );
		} 
		else 
		{
			rc = process_ldapmod_rec( start );
		}

		free( rbuf );
	}

	if ( !not ) 
	{
		ldap_unbind( ld );
	}

	exit( rc );
}


static int
process_ldif_rec( char *rbuf )
{
	char	*line, *dn, *type, *value, *newrdn, *p;
	int		rc, linenum, vlen, modop, replicaport;
	int		expect_modop, expect_sep, expect_ct, expect_newrdn;
	int		expect_deleteoldrdn, deleteoldrdn;
	int		saw_replica, use_record, new_entry, delete_entry, got_all;
	LDAPMod	**pmods;

	int expect_newparent = 0;
	char * newparent = NULL;

	new_entry = new;

	rc = got_all = saw_replica = delete_entry = expect_modop = 0;
	expect_deleteoldrdn = expect_newrdn = expect_sep = expect_ct = 0;
	linenum = 0;
	deleteoldrdn = 1;
	use_record = force;
	pmods = NULL;
	dn = newrdn = NULL;

	while ( (rc == 0) && ( line = str_getline( &rbuf )) != NULL ) 
	{
		++linenum;
		if ( expect_sep && strcasecmp( line, T_MODSEPSTR ) == 0 ) 
		{
			expect_sep = 0;
			expect_ct = 1;
			continue;
		}

		if ( str_parse_line( line, &type, &value, &vlen ) < 0 ) 
		{
			fprintf( stderr, catgets(slapdcat, 1, 7, "%1$s: invalid format (line %2$d of entry: %s)\n"),
				prog, linenum, dn == NULL ? "" : dn );
			rc = LDAP_PARAM_ERROR;
			break;
		}
		if ( (linenum==1) && (strcasecmp(type, T_VERSION_STR) == 0) )
		{
			if ( strcmp(value, "1") != 1 )
			{
				/* LDIF version error */
				fprintf( stderr, catgets(slapdcat, 1, 1227, "%1$s: bad LDIF version number (version: 1 is expected as line %2d)\n"),  prog, linenum );
				rc = LDAP_PARAM_ERROR;
			}
			else
				continue;
		}

		if ( dn == NULL ) 
		{
			if ( !use_record && strcasecmp( type, T_REPLICA_STR ) == 0 ) 
			{
				++saw_replica;
				if (( p = strchr( value, ':' )) == NULL ) 
				{
					replicaport = LDAP_PORT;
				} 
				else 
				{
					*p++ = '\0';
					replicaport = atoi( p );
				}
				if (	strcasecmp( value, ldaphost ) == 0 &&
						replicaport == ldapport ) 
				{
					use_record = 1;
				}
			} 
			else if ( strcasecmp( type, T_DN_STR ) == 0 ) 
			{
				if (( dn = strdup( value )) == NULL ) 
				{
					perror( "strdup" );
					exit( 1 );
				}
				expect_ct = 1;
			}
			continue;	/* skip all lines until we see "dn:" */
		}

		if ( expect_ct ) 
		{
			expect_ct = 0;
			if ( !use_record && saw_replica ) 
			{
				printf( catgets(slapdcat, 1, 8, "%1$s: skipping change record for entry: %2$s\n\t(LDAP host/port does not match replica: lines)\n"),
					prog, dn );
				free( dn );
				return( 0 );
			}

			if ( strcasecmp( type, T_CHANGETYPESTR ) == 0 ) 
			{
				if ( strcasecmp( value, T_MODIFYCTSTR ) == 0 ) 
				{
					new_entry = 0;
					expect_modop = 1;
				} 
				else if ( strcasecmp( value, T_ADDCTSTR ) == 0 ) 
				{
					new_entry = 1;
				} 
				else if ( (strcasecmp( value, T_MODRDNCTSTR ) == 0) || 
							 (strcasecmp( value, T_MODDNCTSTR ) == 0) ) 
				{
					expect_newrdn = 1;
				} 
				else if ( strcasecmp( value, T_DELETECTSTR ) == 0 ) 
				{
					got_all = delete_entry = 1;
				} 
				else 
				{
					fprintf( stderr,
						catgets(slapdcat, 1, 9, "%1$s:  unknown %2$s \"%3$s\" (line %4$d of entry: %5$s)\n"),
						prog, T_CHANGETYPESTR, value, linenum, dn );
					rc = LDAP_PARAM_ERROR;
				}
				continue;
			} 
			else if ( new_entry ) 		/*  missing changetype => add */
			{
				modop = LDAP_MOD_ADD;
			} 
			else 
			{
				expect_modop = 1;	/* missing changetype => modify */
			}
		}

		if ( expect_modop ) 
		{
			expect_modop = 0;
			expect_sep = 1;
			if ( strcasecmp( type, T_MODOPADDSTR ) == 0 ) 
			{
				modop = LDAP_MOD_ADD;
				continue;
			} 
			else if ( strcasecmp( type, T_MODOPREPLACESTR ) == 0 ) 
			{
				modop = LDAP_MOD_REPLACE;
				continue;
			} 
			else if ( strcasecmp( type, T_MODOPDELETESTR ) == 0 ) 
			{
				modop = LDAP_MOD_DELETE;
				addmodifyop( &pmods, modop, value, NULL, 0 );
				continue;
			} 	
			else 
			{	/* no modify op:  use default */
				modop = replace ? LDAP_MOD_REPLACE : LDAP_MOD_ADD;
			}
		}

		if ( expect_newrdn ) 
		{
			if ( strcasecmp( type, T_NEWRDNSTR ) == 0 ) 
			{
				if (( newrdn = strdup( value )) == NULL ) 
				{
					perror( "strdup" );
					exit( 1 );
				}
				expect_deleteoldrdn = 1;
				expect_newrdn = 0;
			} 
			else 
			{
				fprintf( stderr, catgets(slapdcat, 1, 10, "%1$s: expecting \"%2$s:\" but saw \"%3$s:\" (line %4$d of entry %5$s)\n"),
					prog, T_NEWRDNSTR, type, linenum, dn );
				rc = LDAP_PARAM_ERROR;
			}
		} 
		else if ( expect_deleteoldrdn ) 
		{
			if ( strcasecmp( type, T_DELETEOLDRDNSTR ) == 0 ) 
			{
				deleteoldrdn = ( *value == '0' ) ? 0 : 1;
				got_all = 1;
				expect_deleteoldrdn = 0;
				expect_newparent = 1;
			} 
			else 
			{
				fprintf( stderr, catgets(slapdcat, 1, 10, "%1$s: expecting \"%2$s:\" but saw \"%3$s:\" (line %4$d of entry %5$s)\n"),
				prog, T_DELETEOLDRDNSTR, type, linenum, dn );
				rc = LDAP_PARAM_ERROR;
			}
		} 
		else if ( got_all ) 
		{
			if ( expect_newparent )
			{
				if ( strcasecmp( type, T_NEWPARENTSTR ) == 0 ) 
				{
					if (( newparent = strdup( value )) == NULL ) 
					{
						perror( "strdup" );
						exit( 1 );
					}
					expect_newparent = 0;
				}
				else 
				{
					fprintf( stderr, catgets(slapdcat, 1, 10, "%1$s: expecting \"%2$s:\" but saw \"%3$s:\" (line %4$d of entry %5$s)\n"),
						prog, T_NEWPARENTSTR, type, linenum, dn );
					rc = LDAP_PARAM_ERROR;
				}
			}
			else
			{
				fprintf( stderr,
					catgets(slapdcat, 1, 11, "%1$s: extra lines at end (line %2$d of entry %3$s)\n"),
					prog, linenum, dn );
				rc = LDAP_PARAM_ERROR;
			}
		} 
		else 
		{
			addmodifyop( &pmods, modop, type, value, vlen );
		}
	}

	if ( rc == 0 ) 
	{
		if ( delete_entry ) 
		{
			rc = dodelete( dn );
		} 
		else if ( newrdn != NULL ) 
		{
			rc = domodrdn( dn, newrdn, newparent, deleteoldrdn );
		} 
		else 
		{
			rc = domodify( dn, pmods, new_entry );
		}

		if ( rc == LDAP_SUCCESS ) 
		{
			rc = 0;
		}
	}

	if ( dn != NULL ) 
	{
		free( dn );
	}
	if ( newrdn != NULL ) 
	{
		free( newrdn );
	}
	if ( pmods != NULL ) 
	{
		freepmods( pmods );
	}

	return( rc );
}


static int
process_ldapmod_rec( char *rbuf )
{
	char	*line, *dn, *p, *q, *attr, *value;
	int		rc, linenum, modop;
	LDAPMod	**pmods;

	pmods = NULL;
	dn = NULL;
	linenum = 0;
	line = rbuf;
	rc = 0;

	while ( (rc == 0) && (rbuf != NULL) && (*rbuf != '\0') ) 
	{
		++linenum;
		if (( p = strchr( rbuf, '\n' )) == NULL ) 
		{
			rbuf = NULL;
		} 
		else 
		{
			if ( *(p-1) == '\\' )  /* lines ending in '\' are continued */
			{	
				strcpy( p - 1, p );
				rbuf = p;
				continue;
			}
			*p++ = '\0';
			rbuf = p;
		}

		if ( dn == NULL ) 	/* first line contains DN */
		{
			if (( dn = strdup( line )) == NULL ) 
			{
				perror( "strdup" );
				exit( 1 );
			}
		} 
		else 
		{
			if (( p = strchr( line, '=' )) == NULL ) 
			{
				value = NULL;
				p = line + strlen( line );
			} 
			else 
			{
				*p++ = '\0';
				value = p;
			}

			for ( attr = line; *attr != '\0' && isspace( *attr ); ++attr ) 
			{
				;	/* skip attribute leading white space */
			}

			for ( q = p - 1; q > attr && isspace( *q ); --q ) 
			{
				*q = '\0';	/* remove attribute trailing white space */
			}

			if ( value != NULL ) 
			{
				while ( isspace( *value )) 
				{
					++value;		/* skip value leading white space */
				}
				for ( q = value + strlen( value ) - 1; q > value &&
					isspace( *q ); --q ) 
				{
					*q = '\0';	/* remove value trailing white space */
				}
				if ( *value == '\0' ) 
				{
					value = NULL;
				}
			}

			if ( value == NULL && new ) 
			{
				fprintf( stderr, catgets(slapdcat, 1, 12, "%1$s: missing value on line %2$d (attr is %3$s)\n"),
					prog, linenum, attr );
				rc = LDAP_PARAM_ERROR;
			} 
			else 
			{
				 switch ( *attr ) 
				{
					case '-':
						modop = LDAP_MOD_DELETE;
						++attr;
						break;
					case '+':
						modop = LDAP_MOD_ADD;
						++attr;
						break;
					default:
						modop = replace ? LDAP_MOD_REPLACE : LDAP_MOD_ADD;
				}

				addmodifyop( &pmods, modop, attr, value,
					( value == NULL ) ? 0 : strlen( value ));
			}
		}
		line = rbuf;
	}

	if ( rc == 0 ) 
	{
		if ( dn == NULL ) 
		{
			rc = LDAP_PARAM_ERROR;
		} 
		else if (( rc = domodify( dn, pmods, new )) == LDAP_SUCCESS ) 
		{
			rc = 0;
		}
	}

	if ( pmods != NULL ) 
	{
		freepmods( pmods );
	}
	if ( dn != NULL ) 
	{
		free( dn );
	}

	return( rc );
}


static void
addmodifyop( LDAPMod ***pmodsp, int modop, char *attr, char *value, int vlen )
{
	LDAPMod		**pmods;
	int			i, j;
	struct berval	*bvp;

	pmods = *pmodsp;
	modop |= LDAP_MOD_BVALUES;

	i = 0;
	if ( pmods != NULL ) 
	{
		for ( ; pmods[ i ] != NULL; ++i ) 
		{
			if ( strcasecmp( pmods[ i ]->mod_type, attr ) == 0 &&
				pmods[ i ]->mod_op == modop ) 
			{
				break;
			}
		}
	}

	if ( pmods == NULL || pmods[ i ] == NULL ) 
	{
		if (( pmods = (LDAPMod **)safe_realloc( pmods, (i + 2) *
			sizeof( LDAPMod * ))) == NULL ) 
		{
			perror( "safe_realloc" );
			exit( 1 );
		}
		*pmodsp = pmods;
		pmods[ i + 1 ] = NULL;
		if (( pmods[ i ] = (LDAPMod *)calloc( 1, sizeof( LDAPMod )))
			== NULL ) 
		{
			perror( "calloc" );
			exit( 1 );
		}
		pmods[ i ]->mod_op = modop;
		if (( pmods[ i ]->mod_type = strdup( attr )) == NULL ) 
		{
			perror( "strdup" );
			exit( 1 );
		}
	}

	if ( value != NULL ) 
	{
		j = 0;
		if ( pmods[ i ]->mod_bvalues != NULL ) 
		{
			for ( ; pmods[ i ]->mod_bvalues[ j ] != NULL; ++j ) 
			{
				;
			}
		}
		if (( pmods[ i ]->mod_bvalues =
			(struct berval **)safe_realloc( pmods[ i ]->mod_bvalues,
			(j + 2) * sizeof( struct berval * ))) == NULL ) 
		{
			perror( "safe_realloc" );
			exit( 1 );
		}
		pmods[ i ]->mod_bvalues[ j + 1 ] = NULL;
		if (( bvp = (struct berval *)malloc( sizeof( struct berval )))
			== NULL ) 
		{
			perror( "malloc" );
			exit( 1 );
		}
		pmods[ i ]->mod_bvalues[ j ] = bvp;

		bvp->bv_len = vlen;
		if (( bvp->bv_val = (char *)malloc( vlen + 1 )) == NULL ) 
		{
			perror( "malloc" );
			exit( 1 );
		}
		SAFEMEMCPY( bvp->bv_val, value, vlen );
		bvp->bv_val[ vlen ] = '\0';
	}
}


static int
domodify( char *dn, LDAPMod **pmods, int newentry )
{
	int			i, j, k, notascii, op;
	struct berval	*bvp;

	if ( pmods == NULL ) 
	{
		fprintf( stderr, catgets(slapdcat, 1, 13, "%1$s: no attributes to change or add (entry %2$s)\n"),
			prog, dn?dn:"(nil)" );
		return( LDAP_PARAM_ERROR );
	}

	if ( verbose ) 
	{
		for ( i = 0; pmods[ i ] != NULL; ++i ) 
		{
			op = pmods[ i ]->mod_op & ~LDAP_MOD_BVALUES;
			printf( "%s %s:\n", op == LDAP_MOD_REPLACE ?
				catgets(slapdcat, 1, 14, "replace") : op == LDAP_MOD_ADD ?
				catgets(slapdcat, 1, 15, "add") :
					catgets(slapdcat, 1, 16, "delete"), pmods[ i ]->mod_type );
			if ( pmods[ i ]->mod_bvalues != NULL ) 
			{
				for ( j = 0; pmods[ i ]->mod_bvalues[ j ] != NULL; ++j ) 
				{
					bvp = pmods[ i ]->mod_bvalues[ j ];
					notascii = 0;
					for ( k = 0; k < bvp->bv_len; ++k ) 
					{
						if ( !isascii( bvp->bv_val[ k ] )) 
						{
							notascii = 1;
							break;
						}
					}
					if ( notascii ) 
					{
						printf( catgets(slapdcat, 1, 17, "\tNOT ASCII (%ld bytes)\n"), bvp->bv_len );
					} 
					else 
					{
						printf( "\t%s\n", bvp->bv_val );
					}
				}
			}
		}
	}

	if ( newentry ) 
	{
		printf( catgets(slapdcat, 1, 18, "%1$sadding new entry %2$s\n"), not ? "!" : "", dn );
	}
	else 
	{
		printf( catgets(slapdcat, 1, 19, "%1$smodifying entry %2$s\n"), not ? "!" : "", dn );
	}

	if ( !not ) 
	{
		if ( newentry ) 
		{
			int nb=0;
			i = LDAP_NO_SUCH_OBJECT;
			while (i == LDAP_NO_SUCH_OBJECT && nb < nbthreads ) 
			{
				/*
				* may be the parent needs to be created by another thread
				* before, so wait for the parent to be created
				* don't retry more than the maximum of threads
				*
				* do that only for the add as for the modify the entry
				* shall be created before and normally we do only one
				* modification record per entry when we are in multi
				* threaded mode
				*/
				if (nb != 0) 
				{
					timestruc_t to;
					mutex_lock(&wait_mutex);
					to.tv_sec = time(NULL) + 5;
					to.tv_nsec = 0;
					if (cond_timedwait(&wait_cond, &wait_mutex, &to) == ETIME) 
					{
						 nb = nbthreads; /* last chance */
					}
					mutex_unlock(&wait_mutex);
				}
				++nb;
				i = ldap_add_s( ld, dn, pmods );
			} /* end while */
		  cond_broadcast(&wait_cond);
		} 
		else 
		{
			i = ldap_modify_s( ld, dn, pmods );
		}
		if ( i != LDAP_SUCCESS ) 
		{
			if ( newentry )
				fprintf(stderr, "ldap_add_s: %s\n", ldap_err2string(i) );
			else
				fprintf(stderr, "ldap_modify_s: %s\n", ldap_err2string(i) );
		} 
		else if ( verbose ) 
		{
			printf( catgets(slapdcat, 1, 20, "modify complete\n") );
		}
	} 
	else 
	{
		i = LDAP_SUCCESS;
	}

	putchar( '\n' );

	return( i );
}


static int
dodelete( char *dn )
{
	int	rc;

	printf( catgets(slapdcat, 1, 3, "%1$sdeleting entry %2$s\n"), not ? "!" : "", dn );

	if ( !not ) 
	{
		if (( rc = ldap_delete_s( ld, dn )) != LDAP_SUCCESS ) 
		{
			fprintf(stderr, "ldap_delete_s: %s\n", ldap_err2string(rc) );
		} 
		else if ( verbose ) 
		{
			printf( catgets(slapdcat, 1, 21, "delete completed\n") );
		}
	} 
	else 
	{
		rc = LDAP_SUCCESS;
	}

	putchar( '\n' );

	return( rc );
}


static int
domodrdn( char *dn, char *newrdn, char * newparent, int deleteoldrdn )
{
	int rc = LDAP_SUCCESS;

	if ( verbose ) 
	{
	printf( catgets(slapdcat, 1, 22, "new RDN: %1$s (%2$skeep existing values)\n"),
		newrdn, deleteoldrdn ? "do not " : "" );
	}

	printf( catgets(slapdcat, 1, 23, "%1$srenaming entry %2$s\n"), not ? "!" : "", dn );
	if ( !not ) 
	{
		if ( (protoVersion == LDAP_VERSION) && newparent )
		{
				printf(catgets(slapdcat, 1, 1228, "LDAP Server is V2: newsuperior: %x line is ignored ...\n"), newparent);
				newparent = NULL;
		}

		if ( (rc=ldap_rename_s( ld, dn, newrdn, newparent, deleteoldrdn, NULL, NULL )) != LDAP_SUCCESS ) 
		{
			fprintf(stderr, "ldap_rename_s: %s\n", ldap_err2string(rc) );
		} 
		else 
		{
			printf( catgets(slapdcat, 1, 24, "rename completed\n") );
		}
	} 

	putchar( '\n' );

	return( rc );
}



static void
freepmods( LDAPMod **pmods )
{
	int	i;

	for ( i = 0; pmods[ i ] != NULL; ++i ) 
	{
		if ( pmods[ i ]->mod_bvalues != NULL ) 
		{
			ber_bvecfree( pmods[ i ]->mod_bvalues );
		}
		if ( pmods[ i ]->mod_type != NULL ) 
		{
			free( pmods[ i ]->mod_type );
		}
		free( pmods[ i ] );
	}
	free( pmods );
}


static char *
read_one_record( FILE *fp )
{
	/*
	 * one or more blank lines are now allowed as record separator.
	 * so when a blank line is found:
	 * 	if the data is given through stdin, means that the end of the record
	 *		else (data read from file):
	 *			if nothing has been read: skip the blank line and read the next input string
	 * 		else means the end of a record
	 */

	int		 len;
	char		*buf, line[ LDAPMOD_MAXLINE ];
	int		lcur, lmax;

	lcur = lmax = 0;
	buf = NULL;

	mutex_lock(&read_mutex);

	while ( fgets( line, sizeof(line), fp ) != NULL )
	{
		len = strlen( line );
		if ( len == 1 )
		{
			if ( fp==stdin ) 			/* end of stdin input */
				break;
			else if ( buf==NULL )	/* just a new line in file */
				continue;
			else
				break;
		}

		if ( lcur + len + 1 > lmax ) 
		{
			lmax = LDAPMOD_MAXLINE * (( lcur + len + 1 ) / LDAPMOD_MAXLINE + 1 );
			if (( buf = (char *)safe_realloc( buf, lmax )) == NULL ) 
			{
				perror( "safe_realloc" );
				exit( 1 );
			}
		}
		strcpy( buf + lcur, line );
		lcur += len;
	}

	mutex_unlock(&read_mutex);

	return( buf );
}
