/*
 * Copyright (C) 1989-1995 by the Massachusetts Institute of Technology,
 * Cambridge, MA, USA.  All Rights Reserved.
 * 
 * This software is being provided to you, the LICENSEE, by the 
 * Massachusetts Institute of Technology (M.I.T.) under the following 
 * license.  By obtaining, using and/or copying this software, you agree 
 * that you have read, understood, and will comply with these terms and 
 * conditions:  
 * 
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify and distribute 
 * this software and its documentation for any purpose and without fee or 
 * royalty is hereby granted, provided that you agree to comply with the 
 * following copyright notice and statements, including the disclaimer, and 
 * that the same appear on ALL copies of the software and documentation, 
 * including modifications that you make for internal use or for 
 * distribution:
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS 
 * OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not 
 * limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF 
 * MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF 
 * THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY 
 * PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.   
 * 
 * The name of the Massachusetts Institute of Technology or M.I.T. may NOT 
 * be used in advertising or publicity pertaining to distribution of the 
 * software.  Title to copyright in this software and any associated 
 * documentation shall at all times remain with M.I.T., and USER agrees to 
 * preserve same.  
 */
/*
 * This prototype for k5-int.h (Krb5 internals include file)
 * includes the user-visible definitions from krb5.h and then
 * includes other definitions that are not user-visible but are
 * required for compiling Kerberos internal routines.
 *
 * John Gilmore, Cygnus Support, Sat Jan 21 22:45:52 PST 1995
 */

#ifndef _KRB5_INT_H
#define _KRB5_INT_H

#pragma ident	"@(#)k5-int.h	1.1	99/07/18 SMI"

#include	<sys/debug.h>

#ifndef	_KERNEL
#include "osconf.h"
#include "profile.h"
#endif


#ifdef	_KERNEL
#ifdef	DEBUG
extern void prom_printf();
#endif
#else
#define prom_printf printf 
#endif

#ifdef POSIX_TYPES
#define timetype time_t
#else
#define timetype long
#endif


/*
 * Begin "k5-config.h"
 */
#ifndef KRB5_CONFIG__
#define KRB5_CONFIG__

#if (defined(_MSDOS) || defined(_WIN32))
/* 
 * Machine-type definitions: PC Clone 386 running Microloss Windows
 */

#if defined(_MSDOS)
	/* Windows 16 specific */
#define BITS16
#define SIZEOF_INT      2
#define SIZEOF_SHORT    2
#define SIZEOF_LONG     4

#ifndef KRB5_CALLCONV
#define KRB5_CALLCONV __far __export __pascal
#define KRB5_CALLCONV_C __far __export __cdecl
#define KRB5_DLLIMP
#define INTERFACE   KRB5_CALLCONV
#define INTERFACE_C KRB5_CALLCONV_C
#endif /* !KRB5_CALLCONV */

/*
 * The following defines are needed to make <windows.h> work
 * in stdc mode (/Za flag). Winsock.h needs <windows.h>.
 */
#define FAR     _far
#define NEAR    _near
#define _far    __far
#define _near   __near
#define _pascal __pascal
#define _cdecl  __cdecl
#define _huge   __huge

#ifdef NEED_WINDOWS
#include <windows.h>
#endif
	
#else 
	/* Windows 32 specific */
#define SIZEOF_INT      4
#define SIZEOF_SHORT    2
#define SIZEOF_LONG     4

#ifndef KRB5_CALLCONV
#ifdef KRB5_DLL_FILE
#define KRB5_DECLSPEC dllexport
#else
#define KRB5_DECLSPEC dllimport
#endif
#define KRB5_DLLIMP __declspec(KRB5_DECLSPEC)
#define KRB5_CALLCONV __stdcall
#define KRB5_CALLCONV_C __cdecl
#define INTERFACE   KRB5_DLLIMP KRB5_CALLCONV
#define INTERFACE_C KRB5_DLLIMP KRB5_CALLCONV_C
#endif /* !KRB5_CALLCONV */

#include <windows.h>
#endif
	
/* Kerberos Windows initialization file */
#define KERBEROS_INI    "kerberos.ini"
#define INI_FILES       "Files"
#define INI_KRB_CCACHE  "krb5cc"       /* Location of the ccache */
#define INI_KRB5_CONF   "krb5.ini"		/* Location of krb5.conf file */

#define MAXHOSTNAMELEN  512
#define MAXPATHLEN      256            /* Also for Windows temp files */

#define KRB5_USE_INET
#define MSDOS_FILESYSTEM
#define USE_STRING_H 
#define HAVE_SRAND
#define HAVE_ERRNO
#define HAS_STRDUP
#define NO_USERID
#define NOFCHMOD
#define NOCHMOD
#define NO_PASSWORD
#define WM_KERBEROS5_CHANGED "Kerberos5 Changed"

#define HAS_ANSI_VOLATILE
#define HAS_VOID_TYPE
#define	KRB5_PROVIDE_PROTOTYPES
#define HAVE_STDARG_H
#define HAVE_SYS_TYPES_H
#define HAS_STDLIB_H

/* This controls which encryption routines libcrypto will provide */
#define PROVIDE_DES_CBC_MD5
#define PROVIDE_DES_CBC_CRC
#define PROVIDE_DES_CBC_RAW
#define PROVIDE_DES_CBC_CKSUM
/* #define PROVIDE_DES3_CBC_SHA */
/* #define PROVIDE_DES3_CBC_RAW */
#define PROVIDE_CRC32
#define PROVIDE_RSA_MD4
#define PROVIDE_RSA_MD5
/* #define PROVIDE_NIST_SHA */

#ifndef _SIZE_T_DEFINED
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif 

#ifndef KRB5_SYSTYPES__
#define KRB5_SYSTYPES__
#include <sys/types.h>
#endif /* KRB5_SYSTYPES__ */


#ifdef NEED_LOWLEVEL_IO
/* Ugly. Microsoft, in stdc mode, doesn't support the low-level i/o
 * routines directly. Rather, they only export the _<function> version.
 * The following defines works around this problem. 
 */
#include <sys\types.h>
#include <sys\stat.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#define THREEPARAMOPEN(x,y,z) open(x,y,z)
#define O_RDONLY        _O_RDONLY
#define O_WRONLY        _O_WRONLY
#define O_RDWR          _O_RDWR
#define O_APPEND        _O_APPEND
#define O_CREAT         _O_CREAT
#define O_TRUNC         _O_TRUNC
#define O_EXCL          _O_EXCL
#define O_TEXT          _O_TEXT
#define O_BINARY        _O_BINARY
#define O_NOINHERIT     _O_NOINHERIT
#define stat            _stat
#define unlink          _unlink
#define lseek           _lseek
#define write           _write
#define open            _open
#define close           _close
#define read            _read
#define fstat           _fstat
#define mktemp          _mktemp
#define dup             _dup

#define getpid          _getpid
#endif

#ifdef NEED_SYSERROR
/* Only needed by util/et/error_message.c but let's keep the source clean */
#define sys_nerr        _sys_nerr
#define sys_errlist     _sys_errlist
#endif

#define DEFAULT_PWD_STRING1 "Enter password:"
#define DEFAULT_PWD_STRING2 "Re-enter password for verification:"

/* Functions with slightly different names on the PC
*/                   
#define strcasecmp   _stricmp
#define strncasecmp  _strnicmp
#define strdup       _strdup
#define off_t        _off_t
int sscanf(const char *str, char const *fmt0, ...);

#else 		/* Rest of include file is for non-Microloss-Windows */

#ifndef _MACINTOSH
#if defined(__MWERKS__) || defined(applec) || defined(THINK_C)
#define _MACINTOSH
#define SIZEOF_INT 4
#define SIZEOF_SHORT 2
#define HAVE_SRAND
#define NO_PASSWORD
#define HAS_LABS
#define ENOMEM -1
#define ANSI_STDIO
#ifndef _SIZET
typedef unsigned int size_t;
#define _SIZET
#endif
#include <unix.h>
#include <ctype.h>
#endif
#endif

#if defined(_MACINTOSH)

#ifdef NEED_LOWLEVEL_IO
#include <fcntl.h>
#endif

/*
 * Which encryption routines libcrypto will provide is controlled by
 * mac/libraries/KerberosHeaders.h.
 */

/* there is no <stat.h> for mpw */
typedef unsigned long	mode_t;
typedef unsigned long	ino_t;
typedef unsigned long	dev_t;
typedef short			nlink_t;
typedef unsigned long	uid_t;
typedef unsigned long	gid_t;
typedef long			off_t;

#ifndef __MWERKS__
struct stat
{
	mode_t		st_mode;		/* File mode; see #define's below */
	ino_t		st_ino;			/* File serial number */
	dev_t		st_dev;			/* ID of device containing this file */
	nlink_t		st_nlink;		/* Number of links */
	uid_t		st_uid;			/* User ID of the file's owner */
	gid_t		st_gid;			/* Group ID of the file's group */
	dev_t		st_rdev;		/* Device type */
	off_t		st_size;		/* File size in bytes */
	unsigned long		st_atime;		/* Time of last access */
	unsigned long		st_mtime;		/* Time of last data modification */
	unsigned long		st_ctime;		/* Time of last file status change */
	long		st_blksize;		/* Optimal blocksize */
	long		st_blocks;		/* blocks allocated for file */
};

int stat(const char *path, struct stat *buf);
#endif

int fstat(int fildes, struct stat *buf);

#define EFBIG 1000

#define NOFCHMOD 1
#define NOCHMOD 1
#define _MACSOCKAPI_

#define THREEPARAMOPEN(x,y,z) open(x,y)
#else /* _MACINTOSH */
#define THREEPARAMOPEN(x,y,z) open(x,y,z)
#endif /* _MACINTOSH */

#ifndef _KERNEL 
#ifndef KRB5_AUTOCONF__
#define KRB5_AUTOCONF__
#include "autoconf.h"
#endif
#endif 		/* !_KERNEL  */

#ifndef KRB5_SYSTYPES__
#define KRB5_SYSTYPES__

#ifdef HAVE_SYS_TYPES_H		/* From autoconf.h */
#include <sys/types.h>
#else /* HAVE_SYS_TYPES_H */
#endif /* HAVE_SYS_TYPES_H */
#endif /* KRB5_SYSTYPES__ */

#ifdef SYSV
/* Change srandom and random to use rand and srand */
/* Taken from the Sandia changes.  XXX  We should really just include */
/* srandom and random into Kerberos release, since rand() is a really */
/* bad random number generator.... [tytso:19920616.2231EDT] */
#define random() rand()
#define srandom(a) srand(a)
#ifndef unicos61
#define utimes(a,b) utime(a,b)
#endif  /* unicos61 */
#endif /* SYSV */

#define DEFAULT_PWD_STRING1 "Enter password:"
#define DEFAULT_PWD_STRING2 "Re-enter password for verification:"

#define	KRB5_KDB_MAX_LIFE	(60*60*24) /* one day */
#define	KRB5_KDB_MAX_RLIFE	(60*60*24*365) /* one year */
#define	KRB5_KDB_EXPIRATION	2145830400 /* Thu Jan  1 00:00:00 2038 UTC */
#define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */
#define KRB5_DEFAULT_RENEW_LIFE 7*24*60*60 /* 7 Days */
/* 
 * Windows requires a different api interface to each function. Here
 * just define it as NULL.
 */
#ifndef KRB5_CALLCONV
#define KRB5_CALLCONV
#define KRB5_CALLCONV_C
#define KRB5_DLLIMP
#define INTERFACE
#define INTERFACE_C
#define FAR
#define NEAR
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef HAS_LABS
#define labs(x) abs(x)
#endif

#endif /* _MSDOS */
#endif /* KRB5_CONFIG__ */

/*
 * End "k5-config.h"
 */

/*
 * After loading the configuration definitions, load the Kerberos definitions.
 */
#include "krb5.h"

#ifndef _KERNEL
#ifdef NEED_SOCKETS
#include "port-sockets.h"
#endif
#endif

/* krb5/krb5.h includes many other .h files in the krb5 subdirectory.
   The ones that it doesn't include, we include below.  */

/*
 * Begin "k5-errors.h"
 */
#ifndef KRB5_ERRORS__
#define KRB5_ERRORS__


/* Error codes used in KRB_ERROR protocol messages.
   Return values of library routines are based on a different error table
   (which allows non-ambiguous error codes between subsystems) */

/* KDC errors */
#define	KDC_ERR_NONE			0 /* No error */
#define	KDC_ERR_NAME_EXP		1 /* Client's entry in DB expired */
#define	KDC_ERR_SERVICE_EXP		2 /* Server's entry in DB expired */
#define	KDC_ERR_BAD_PVNO		3 /* Requested pvno not supported */
#define	KDC_ERR_C_OLD_MAST_KVNO		4 /* C's key encrypted in old master */
#define	KDC_ERR_S_OLD_MAST_KVNO		5 /* S's key encrypted in old master */
#define	KDC_ERR_C_PRINCIPAL_UNKNOWN	6 /* Client not found in Kerberos DB */
#define	KDC_ERR_S_PRINCIPAL_UNKNOWN	7 /* Server not found in Kerberos DB */
#define	KDC_ERR_PRINCIPAL_NOT_UNIQUE	8 /* Multiple entries in Kerberos DB */
#define	KDC_ERR_NULL_KEY		9 /* The C or S has a null key */
#define	KDC_ERR_CANNOT_POSTDATE		10 /* Tkt ineligible for postdating */
#define	KDC_ERR_NEVER_VALID		11 /* Requested starttime > endtime */
#define	KDC_ERR_POLICY			12 /* KDC policy rejects request */
#define	KDC_ERR_BADOPTION		13 /* KDC can't do requested opt. */
#define	KDC_ERR_ENCTYPE_NOSUPP		14 /* No support for encryption type */
#define KDC_ERR_SUMTYPE_NOSUPP		15 /* No support for checksum type */
#define KDC_ERR_PADATA_TYPE_NOSUPP	16 /* No support for padata type */
#define KDC_ERR_TRTYPE_NOSUPP		17 /* No support for transited type */
#define KDC_ERR_CLIENT_REVOKED		18 /* C's creds have been revoked */
#define KDC_ERR_SERVICE_REVOKED		19 /* S's creds have been revoked */
#define KDC_ERR_TGT_REVOKED		20 /* TGT has been revoked */
#define KDC_ERR_CLIENT_NOTYET		21 /* C not yet valid */
#define KDC_ERR_SERVICE_NOTYET		22 /* S not yet valid */
#define KDC_ERR_KEY_EXP			23 /* Password has expired */
#define KDC_ERR_PREAUTH_FAILED		24 /* Preauthentication failed */
#define KDC_ERR_PREAUTH_REQUIRED	25 /* Additional preauthentication */
					   /* required */
#define KDC_ERR_SERVER_NOMATCH		26 /* Requested server and */
					   /* ticket don't match*/
/* Application errors */
#define	KRB_AP_ERR_BAD_INTEGRITY 31	/* Decrypt integrity check failed */
#define	KRB_AP_ERR_TKT_EXPIRED	32	/* Ticket expired */
#define	KRB_AP_ERR_TKT_NYV	33	/* Ticket not yet valid */
#define	KRB_AP_ERR_REPEAT	34	/* Request is a replay */
#define	KRB_AP_ERR_NOT_US	35	/* The ticket isn't for us */
#define	KRB_AP_ERR_BADMATCH	36	/* Ticket/authenticator don't match */
#define	KRB_AP_ERR_SKEW		37	/* Clock skew too great */
#define	KRB_AP_ERR_BADADDR	38	/* Incorrect net address */
#define	KRB_AP_ERR_BADVERSION	39	/* Protocol version mismatch */
#define	KRB_AP_ERR_MSG_TYPE	40	/* Invalid message type */
#define	KRB_AP_ERR_MODIFIED	41	/* Message stream modified */
#define	KRB_AP_ERR_BADORDER	42	/* Message out of order */
#define	KRB_AP_ERR_BADKEYVER	44	/* Key version is not available */
#define	KRB_AP_ERR_NOKEY	45	/* Service key not available */
#define	KRB_AP_ERR_MUT_FAIL	46	/* Mutual authentication failed */
#define KRB_AP_ERR_BADDIRECTION	47 	/* Incorrect message direction */
#define KRB_AP_ERR_METHOD	48 	/* Alternative authentication */
					/* method required */
#define KRB_AP_ERR_BADSEQ	49 	/* Incorrect sequence numnber */
					/* in message */
#define KRB_AP_ERR_INAPP_CKSUM	50	/* Inappropriate type of */
					/* checksum in message */

/* other errors */
#define KRB_ERR_GENERIC		60 	/* Generic error (description */
					/* in e-text) */
#define	KRB_ERR_FIELD_TOOLONG	61	/* Field is too long for impl. */

#endif /* KRB5_ERRORS__ */
/*
 * End "k5-errors.h"
 */

/*
 * This structure is returned in the e-data field of the KRB-ERROR
 * message when the error calling for an alternative form of
 * authentication is returned, KRB_AP_METHOD.
 */
typedef struct _krb5_alt_method {
	krb5_magic	magic;
	krb5_int32	method;
	int		length;
	krb5_octet	*data;
} krb5_alt_method;

/*
 * A null-terminated array of this structure is returned by the KDC as
 * the data part of the ETYPE_INFO preauth type.  It informs the
 * client which encryption types are supported.
 */
typedef struct _krb5_etype_info_entry {
	krb5_magic	magic;
	krb5_enctype	etype;
	int		length;
	krb5_octet	*salt;
} krb5_etype_info_entry;

typedef krb5_etype_info_entry ** krb5_etype_info;

/*
 * a sam_challenge is returned for alternate preauth 
 */
/*
          SAMFlags ::= BIT STRING {
              use-sad-as-key[0],
              send-encrypted-sad[1],
              must-pk-encrypt-sad[2]
          }
 */
/*
          PA-SAM-CHALLENGE ::= SEQUENCE {
              sam-type[0]                 INTEGER,
              sam-flags[1]                SAMFlags,
              sam-type-name[2]            GeneralString OPTIONAL,
              sam-track-id[3]             GeneralString OPTIONAL,
              sam-challenge-label[4]      GeneralString OPTIONAL,
              sam-challenge[5]            GeneralString OPTIONAL,
              sam-response-prompt[6]      GeneralString OPTIONAL,
              sam-pk-for-sad[7]           EncryptionKey OPTIONAL,
              sam-nonce[8]                INTEGER OPTIONAL,
              sam-cksum[9]                Checksum OPTIONAL
          }
*/
/* sam_type values -- informational only */
#define PA_SAM_TYPE_ENIGMA     1   /*  Enigma Logic */
#define PA_SAM_TYPE_DIGI_PATH  2   /*  Digital Pathways */
#define PA_SAM_TYPE_SKEY_K0    3   /*  S/key where  KDC has key 0 */
#define PA_SAM_TYPE_SKEY       4   /*  Traditional S/Key */
#define PA_SAM_TYPE_SECURID    5   /*  Security Dynamics */
#define PA_SAM_TYPE_GRAIL    128 /* experimental */

typedef struct _krb5_predicted_sam_response {
	krb5_magic	magic;
	krb5_keyblock	sam_key;
} krb5_predicted_sam_response;

typedef struct _krb5_sam_challenge {
	krb5_magic	magic;
	krb5_int32	sam_type; /* information */
	krb5_flags	sam_flags; /* KRB5_SAM_* values */
	krb5_data	sam_type_name;
	krb5_data	sam_track_id;
	krb5_data	sam_challenge_label;
	krb5_data	sam_challenge;
	krb5_data	sam_response_prompt;
	krb5_data	sam_pk_for_sad;
	krb5_int32	sam_nonce;
	krb5_checksum	sam_cksum;
} krb5_sam_challenge;

typedef struct _krb5_sam_key {	/* reserved for future use */
	krb5_magic	magic;
	krb5_keyblock	sam_key;
} krb5_sam_key;

typedef struct _krb5_enc_sam_response_enc {
	krb5_magic	magic;
	krb5_int32	sam_nonce;
	krb5_timestamp	sam_timestamp;
	krb5_int32	sam_usec;
	krb5_data	sam_passcode;
} krb5_enc_sam_response_enc;

typedef struct _krb5_sam_response {
	krb5_magic	magic;
	krb5_int32	sam_type; /* informational */
	krb5_flags	sam_flags; /* KRB5_SAM_* values */
	krb5_data	sam_track_id; /* copied */
	krb5_enc_data	sam_enc_key; /* krb5_sam_key - future use */
	krb5_enc_data	sam_enc_nonce_or_ts; /* krb5_enc_sam_response_enc */
	krb5_int32	sam_nonce;
	krb5_timestamp	sam_patimestamp;
} krb5_sam_response;


/*
 * Begin "dbm.h"
 */
#ifndef _KERNEL 

/*
 * Since we are always using db, use the db-ndbm include header file.
 */	

#include "db-ndbm.h"
	
#endif /* !KERNEL */
/*
 * End "dbm.h"
 */

/*
 * Begin "ext-proto.h"
 */
#ifndef KRB5_EXT_PROTO__
#define KRB5_EXT_PROTO__

#ifndef _KERNEL 
#include <stdlib.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#endif /* !_KERNEL */

#ifndef HAS_STRDUP
extern char *strdup KRB5_PROTOTYPE((const char *));
#endif

#ifndef _KERNEL
#ifdef HAS_UNISTD_H
#include <unistd.h>
#endif
#endif /* !_KERNEL */

#endif /* KRB5_EXT_PROTO__ */
/*
 * End "ext-proto.h"
 */

/*
 * Begin "sysincl.h"
 */
#ifndef KRB5_SYSINCL__
#define KRB5_SYSINCL__

#ifndef KRB5_SYSTYPES__
#define KRB5_SYSTYPES__
/* needed for much of the rest -- but already handled in krb5.h? */
/* #include <sys/types.h> */
#endif /* KRB5_SYSTYPES__ */

#ifdef	_KERNEL
#include <sys/time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#endif /* _KERNEL */

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>			/* struct stat, stat() */
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>			/* MAXPATHLEN */
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>			/* prototypes for file-related
					   syscalls; flags for open &
					   friends */
#endif

#ifdef _KERNEL
#include <sys/fcntl.h>
#else
#include <fcntl.h>
#endif

#endif /* KRB5_SYSINCL__ */
/*
 * End "sysincl.h"
 */

/*
 * Begin "los-proto.h"
 */
#ifndef KRB5_LIBOS_PROTO__
#define KRB5_LIBOS_PROTO__

#ifndef	_KERNEL
#include <stdio.h>
#endif

/* libos.spec */
krb5_error_code krb5_lock_file
	KRB5_PROTOTYPE((krb5_context,
		        int,
		        int));
krb5_error_code krb5_unlock_file
	KRB5_PROTOTYPE((krb5_context,
			int));
int krb5_net_read
	KRB5_PROTOTYPE((krb5_context,
		   int ,
		   char *,
		   int  ));
int krb5_net_write
	KRB5_PROTOTYPE((krb5_context,
		   int ,
		   const char *,
		   int  ));
krb5_error_code krb5_sendto_kdc
	KRB5_PROTOTYPE((krb5_context,
		   const krb5_data *,
		   const krb5_data *,
		   krb5_data * ));
krb5_error_code krb5_get_krbhst
	KRB5_PROTOTYPE((krb5_context,
		   const krb5_data *,
		   char *** ));
krb5_error_code krb5_free_krbhst
	KRB5_PROTOTYPE((krb5_context,
		   char * const * ));
krb5_error_code krb5_random_confounder
	KRB5_PROTOTYPE((int,
		   krb5_pointer ));
krb5_error_code krb5_gen_replay_name
    KRB5_PROTOTYPE((krb5_context,
		   const krb5_address *,
	       const char *,
	       char **));
krb5_error_code krb5_create_secure_file
	KRB5_PROTOTYPE((krb5_context,
		   const char * pathname));
#ifndef	_KERNEL
krb5_error_code krb5_sync_disk_file
	KRB5_PROTOTYPE((krb5_context,
		   FILE *fp));
#endif


krb5_error_code krb5_read_message 
	KRB5_PROTOTYPE((krb5_context,
		   krb5_pointer, 
		   krb5_data *));
krb5_error_code krb5_write_message 
	KRB5_PROTOTYPE((krb5_context,
		   krb5_pointer, 
		   krb5_data *));

krb5_error_code krb5_os_init_context
        KRB5_PROTOTYPE((krb5_context));

void krb5_os_free_context
        KRB5_PROTOTYPE((krb5_context));

krb5_error_code krb5_find_config_files
        KRB5_PROTOTYPE(());


/* in here to deal with stuff from lib/crypto/os */

krb5_error_code krb5_crypto_os_localaddr
    KRB5_PROTOTYPE((krb5_address ***));

krb5_error_code krb5_crypto_us_timeofday
    KRB5_PROTOTYPE((krb5_int32  *,
		    krb5_int32  *));

#ifndef _KERNEL
time_t gmt_mktime KRB5_PROTOTYPE((struct tm *));
#endif

#endif /* KRB5_LIBOS_PROTO__ */
/*
 * End "los-proto.h"
 */

/*
 * Include the KDB definitions.
 */
#ifndef _KERNEL
#include <krb5/kdb.h>
#include <krb5/kdb_dbm.h>
#endif /* !_KERNEL */
/*
 * Begin "libos.h"
 */
#ifndef KRB5_LIBOS__
#define KRB5_LIBOS__

typedef struct _krb5_os_context {
	krb5_magic	magic;
	krb5_int32	time_offset;
	krb5_int32	usec_offset;
	krb5_int32	os_flags;
} *krb5_os_context;

/*
 * Flags for the os_flags field
 *
 * KRB5_OS_TOFFSET_VALID means that the time offset fields are valid.
 * The intention is that this facility to correct the system clocks so
 * that they reflect the "real" time, for systems where for some
 * reason we can't set the system clock.  Instead we calculate the
 * offset between the system time and real time, and store the offset
 * in the os context so that we can correct the system clock as necessary.
 *
 * KRB5_OS_TOFFSET_TIME means that the time offset fields should be
 * returned as the time by the krb5 time routines.  This should only
 * be used for testing purposes (obviously!)
 */
#define KRB5_OS_TOFFSET_VALID	1
#define KRB5_OS_TOFFSET_TIME	2

/* lock mode flags */
#define	KRB5_LOCKMODE_SHARED	0x0001
#define	KRB5_LOCKMODE_EXCLUSIVE	0x0002
#define	KRB5_LOCKMODE_DONTBLOCK	0x0004
#define	KRB5_LOCKMODE_UNLOCK	0x0008

#endif /* KRB5_LIBOS__ */
/*
 * End "libos.h"
 */

/*
 * Define our view of the size of a DES key.
 */
#define	KRB5_MIT_DES_KEYSIZE		8
/*
 * Check if des_int.h has been included before us.  If so, then check to see
 * that our view of the DES key size is the same as des_int.h's.
 */
#ifdef	MIT_DES_KEYSIZE
#if	MIT_DES_KEYSIZE != KRB5_MIT_DES_KEYSIZE
error(MIT_DES_KEYSIZE does not equal KRB5_MIT_DES_KEYSIZE)
#endif	/* MIT_DES_KEYSIZE != KRB5_MIT_DES_KEYSIZE */
#endif	/* MIT_DES_KEYSIZE */

/*
 * Begin "preauth.h"
 *
 * (Originally written by Glen Machin at Sandia Labs.)
 */
/*
 * Sandia National Laboratories also makes no representations about the 
 * suitability of the modifications, or additions to this software for 
 * any purpose.  It is provided "as is" without express or implied warranty.
 * 
 */
#ifndef KRB5_PREAUTH__
#define KRB5_PREAUTH__

typedef struct _krb5_pa_enc_ts {
    krb5_timestamp	patimestamp;
    krb5_int32		pausec;
} krb5_pa_enc_ts;

typedef krb5_error_code (*krb5_preauth_obtain_proc)
    KRB5_NPROTOTYPE((krb5_context,
		    krb5_pa_data *,
		    krb5_etype_info,
		    krb5_keyblock *, 
		    krb5_error_code ( * )(krb5_context,
					  krb5_const krb5_enctype,
					  krb5_data *,
					  krb5_const_pointer,
					  krb5_keyblock **),
		    krb5_const_pointer,
		    krb5_creds *,
		    krb5_kdc_req *,
		    krb5_pa_data **));

typedef krb5_error_code (*krb5_preauth_process_proc)
    KRB5_NPROTOTYPE((krb5_context,
		    krb5_pa_data *,
		    krb5_kdc_req *,
		    krb5_kdc_rep *,
		    krb5_error_code ( * )(krb5_context,
					  krb5_const krb5_enctype,
					  krb5_data *,
					  krb5_const_pointer,
					  krb5_keyblock **),
		    krb5_const_pointer,
		    krb5_error_code ( * )(krb5_context,
					  krb5_const krb5_keyblock *,
					  krb5_const_pointer,
					  krb5_kdc_rep * ),
		    krb5_keyblock **,
		    krb5_creds *, 
		    krb5_int32 *,
		    krb5_int32 *));

typedef struct _krb5_preauth_ops {
    krb5_magic magic;
    int     type;
    int	flags;
    krb5_preauth_obtain_proc	obtain;
    krb5_preauth_process_proc	process;
} krb5_preauth_ops;

krb5_error_code krb5_obtain_padata
    	KRB5_PROTOTYPE((krb5_context,
		krb5_pa_data **,
		krb5_error_code ( * )KRB5_NPROTOTYPE((krb5_context,
						      krb5_const krb5_enctype,
						      krb5_data *,
						      krb5_const_pointer,
						      krb5_keyblock **)),
		krb5_const_pointer, 
		krb5_creds *,
		krb5_kdc_req *));

krb5_error_code krb5_process_padata
	KRB5_PROTOTYPE((krb5_context,
		krb5_kdc_req *,
		krb5_kdc_rep *,
		krb5_error_code ( * )KRB5_NPROTOTYPE((krb5_context,
						      krb5_const krb5_enctype,
						      krb5_data *,
						      krb5_const_pointer,
						      krb5_keyblock **)),
		krb5_const_pointer,
		krb5_error_code ( * )KRB5_NPROTOTYPE((krb5_context,
						      krb5_const krb5_keyblock *,
						      krb5_const_pointer,
						      krb5_kdc_rep * )),
		krb5_keyblock **, 	
		krb5_creds *, 
		krb5_int32 *));		

void krb5_free_etype_info
    KRB5_PROTOTYPE((krb5_context, krb5_etype_info));

/*
 * Preauthentication property flags
 */
#define KRB5_PREAUTH_FLAGS_ENCRYPT	0x00000001
#define KRB5_PREAUTH_FLAGS_HARDWARE	0x00000002

#endif /* KRB5_PREAUTH__ */
/*
 * End "preauth.h"
 */

/* #include "krb5/wordsize.h" -- comes in through base-defs.h. */
 
struct _krb5_context {
	krb5_magic	magic;
	krb5_enctype  FAR *in_tkt_ktypes;
	int		in_tkt_ktype_count;
	krb5_enctype  FAR *tgs_ktypes;
	int		tgs_ktype_count;
	void	      FAR *os_context;
	char	      FAR *default_realm;
#ifndef _KERNEL
	profile_t     profile;
	void	      FAR *db_context;
#endif
	int		ser_ctx_count;
	void	      	FAR *ser_ctx;
	krb5_deltat 	clockskew; /* allowable clock skew */
	krb5_cksumtype	kdc_req_sumtype;
	krb5_cksumtype	default_ap_req_sumtype;
	krb5_cksumtype	default_safe_sumtype;
	krb5_flags 	kdc_default_options;
	krb5_flags	library_options;
	krb5_boolean	profile_secure;
	int		fcc_default_format;
	int		scc_default_format;
};

#define KRB5_LIBOPT_SYNC_KDCTIME	0x0001

/*
 * Begin "asn1.h"
 */
#ifndef KRB5_ASN1__
#define KRB5_ASN1__

/* ASN.1 encoding knowledge; KEEP IN SYNC WITH ASN.1 defs! */
/* here we use some knowledge of ASN.1 encodings */
/* 
  Ticket is APPLICATION 1.
  Authenticator is APPLICATION 2.
  AS_REQ is APPLICATION 10.
  AS_REP is APPLICATION 11.
  TGS_REQ is APPLICATION 12.
  TGS_REP is APPLICATION 13.
  AP_REQ is APPLICATION 14.
  AP_REP is APPLICATION 15.
  KRB_SAFE is APPLICATION 20.
  KRB_PRIV is APPLICATION 21.
  KRB_CRED is APPLICATION 22.
  EncASRepPart is APPLICATION 25.
  EncTGSRepPart is APPLICATION 26.
  EncAPRepPart is APPLICATION 27.
  EncKrbPrivPart is APPLICATION 28.
  EncKrbCredPart is APPLICATION 29.
  KRB_ERROR is APPLICATION 30.
 */
/* allow either constructed or primitive encoding, so check for bit 6
   set or reset */
#define krb5_is_krb_ticket(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x61 ||\
				    (dat)->data[0] == 0x41))
#define krb5_is_krb_authenticator(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x62 ||\
				    (dat)->data[0] == 0x42))
#define krb5_is_as_req(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x6a ||\
				    (dat)->data[0] == 0x4a))
#define krb5_is_as_rep(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x6b ||\
				    (dat)->data[0] == 0x4b))
#define krb5_is_tgs_req(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x6c ||\
				    (dat)->data[0] == 0x4c))
#define krb5_is_tgs_rep(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x6d ||\
				    (dat)->data[0] == 0x4d))
#define krb5_is_ap_req(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x6e ||\
				    (dat)->data[0] == 0x4e))
#define krb5_is_ap_rep(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x6f ||\
				    (dat)->data[0] == 0x4f))
#define krb5_is_krb_safe(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x74 ||\
				    (dat)->data[0] == 0x54))
#define krb5_is_krb_priv(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x75 ||\
				    (dat)->data[0] == 0x55))
#define krb5_is_krb_cred(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x76 ||\
				    (dat)->data[0] == 0x56))
#define krb5_is_krb_enc_as_rep_part(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x79 ||\
				    (dat)->data[0] == 0x59))
#define krb5_is_krb_enc_tgs_rep_part(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x7a ||\
				    (dat)->data[0] == 0x5a))
#define krb5_is_krb_enc_ap_rep_part(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x7b ||\
				    (dat)->data[0] == 0x5b))
#define krb5_is_krb_enc_krb_priv_part(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x7c ||\
				    (dat)->data[0] == 0x5c))
#define krb5_is_krb_enc_krb_cred_part(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x7d ||\
				    (dat)->data[0] == 0x5d))
#define krb5_is_krb_error(dat)\
	((dat) && (dat)->length && ((dat)->data[0] == 0x7e ||\
				    (dat)->data[0] == 0x5e))

/*************************************************************************
 * Prototypes for krb5_encode.c
 *************************************************************************/

/*
   krb5_error_code encode_krb5_structure(const krb5_structure *rep,
					 krb5_data **code);
   modifies  *code
   effects   Returns the ASN.1 encoding of *rep in **code.
             Returns ASN1_MISSING_FIELD if a required field is emtpy in *rep.
             Returns ENOMEM if memory runs out.
*/

krb5_error_code encode_krb5_authenticator
	KRB5_PROTOTYPE((const krb5_authenticator *rep, krb5_data **code));

krb5_error_code encode_krb5_ticket
	KRB5_PROTOTYPE((const krb5_ticket *rep, krb5_data **code));

krb5_error_code encode_krb5_encryption_key
	KRB5_PROTOTYPE((const krb5_keyblock *rep, krb5_data **code));

krb5_error_code encode_krb5_enc_tkt_part
	KRB5_PROTOTYPE((const krb5_enc_tkt_part *rep, krb5_data **code));

krb5_error_code encode_krb5_enc_kdc_rep_part
	KRB5_PROTOTYPE((const krb5_enc_kdc_rep_part *rep, krb5_data **code));

/* yes, the translation is identical to that used for KDC__REP */ 
krb5_error_code encode_krb5_as_rep
	KRB5_PROTOTYPE((const krb5_kdc_rep *rep, krb5_data **code));

/* yes, the translation is identical to that used for KDC__REP */ 
krb5_error_code encode_krb5_tgs_rep
	KRB5_PROTOTYPE((const krb5_kdc_rep *rep, krb5_data **code));

krb5_error_code encode_krb5_ap_req
	KRB5_PROTOTYPE((const krb5_ap_req *rep, krb5_data **code));

krb5_error_code encode_krb5_ap_rep
	KRB5_PROTOTYPE((const krb5_ap_rep *rep, krb5_data **code));

krb5_error_code encode_krb5_ap_rep_enc_part
	KRB5_PROTOTYPE((const krb5_ap_rep_enc_part *rep, krb5_data **code));

krb5_error_code encode_krb5_as_req
	KRB5_PROTOTYPE((const krb5_kdc_req *rep, krb5_data **code));

krb5_error_code encode_krb5_tgs_req
	KRB5_PROTOTYPE((const krb5_kdc_req *rep, krb5_data **code));

krb5_error_code encode_krb5_kdc_req_body
	KRB5_PROTOTYPE((const krb5_kdc_req *rep, krb5_data **code));

krb5_error_code encode_krb5_safe
	KRB5_PROTOTYPE((const krb5_safe *rep, krb5_data **code));

krb5_error_code encode_krb5_priv
	KRB5_PROTOTYPE((const krb5_priv *rep, krb5_data **code));

krb5_error_code encode_krb5_enc_priv_part
	KRB5_PROTOTYPE((const krb5_priv_enc_part *rep, krb5_data **code));

krb5_error_code encode_krb5_cred
	KRB5_PROTOTYPE((const krb5_cred *rep, krb5_data **code));

krb5_error_code encode_krb5_enc_cred_part
	KRB5_PROTOTYPE((const krb5_cred_enc_part *rep, krb5_data **code));

krb5_error_code encode_krb5_error
	KRB5_PROTOTYPE((const krb5_error *rep, krb5_data **code));

krb5_error_code encode_krb5_authdata
	KRB5_PROTOTYPE((const krb5_authdata **rep, krb5_data **code));

krb5_error_code encode_krb5_pwd_sequence
	KRB5_PROTOTYPE((const passwd_phrase_element *rep, krb5_data **code));

krb5_error_code encode_krb5_pwd_data
	KRB5_PROTOTYPE((const krb5_pwd_data *rep, krb5_data **code));

krb5_error_code encode_krb5_padata_sequence
	KRB5_PROTOTYPE((const krb5_pa_data ** rep, krb5_data **code));

krb5_error_code encode_krb5_alt_method
	KRB5_PROTOTYPE((const krb5_alt_method *, krb5_data **code));

krb5_error_code encode_krb5_etype_info
	KRB5_PROTOTYPE((const krb5_etype_info_entry **, krb5_data **code));

krb5_error_code encode_krb5_enc_data
    	KRB5_PROTOTYPE((const krb5_enc_data *, krb5_data **));

krb5_error_code encode_krb5_pa_enc_ts
    	KRB5_PROTOTYPE((const krb5_pa_enc_ts *, krb5_data **));

krb5_error_code encode_krb5_sam_challenge
	KRB5_PROTOTYPE((const krb5_sam_challenge * , krb5_data **));

krb5_error_code encode_krb5_sam_key
	KRB5_PROTOTYPE((const krb5_sam_key * , krb5_data **));

krb5_error_code encode_krb5_enc_sam_response_enc
	KRB5_PROTOTYPE((const krb5_enc_sam_response_enc * , krb5_data **));

krb5_error_code encode_krb5_sam_response
	KRB5_PROTOTYPE((const krb5_sam_response * , krb5_data **));

krb5_error_code encode_krb5_predicted_sam_response
	KRB5_PROTOTYPE((const krb5_predicted_sam_response * , krb5_data **));

/*************************************************************************
 * End of prototypes for krb5_encode.c
 *************************************************************************/


/*************************************************************************
 * Prototypes for krb5_decode.c
 *************************************************************************/

/*
   krb5_error_code decode_krb5_structure(const krb5_data *code,
                                         krb5_structure **rep);
                                         
   requires  Expects **rep to not have been allocated;
              a new *rep is allocated regardless of the old value.
   effects   Decodes *code into **rep.
	     Returns ENOMEM if memory is exhausted.
             Returns asn1 and krb5 errors.
*/

krb5_error_code decode_krb5_authenticator
	KRB5_PROTOTYPE((const krb5_data *code, krb5_authenticator **rep));

krb5_error_code decode_krb5_ticket
	KRB5_PROTOTYPE((const krb5_data *code, krb5_ticket **rep));

krb5_error_code decode_krb5_encryption_key
	KRB5_PROTOTYPE((const krb5_data *output, krb5_keyblock **rep));

krb5_error_code decode_krb5_enc_tkt_part
	KRB5_PROTOTYPE((const krb5_data *output, krb5_enc_tkt_part **rep));

krb5_error_code decode_krb5_enc_kdc_rep_part
	KRB5_PROTOTYPE((const krb5_data *output, krb5_enc_kdc_rep_part **rep));

krb5_error_code decode_krb5_as_rep
	KRB5_PROTOTYPE((const krb5_data *output, krb5_kdc_rep **rep));

krb5_error_code decode_krb5_tgs_rep
	KRB5_PROTOTYPE((const krb5_data *output, krb5_kdc_rep **rep));

krb5_error_code decode_krb5_ap_req
	KRB5_PROTOTYPE((const krb5_data *output, krb5_ap_req **rep));

krb5_error_code decode_krb5_ap_rep
	KRB5_PROTOTYPE((const krb5_data *output, krb5_ap_rep **rep));

krb5_error_code decode_krb5_ap_rep_enc_part
	KRB5_PROTOTYPE((const krb5_data *output, krb5_ap_rep_enc_part **rep));

krb5_error_code decode_krb5_as_req
	KRB5_PROTOTYPE((const krb5_data *output, krb5_kdc_req **rep));

krb5_error_code decode_krb5_tgs_req
	KRB5_PROTOTYPE((const krb5_data *output, krb5_kdc_req **rep));

krb5_error_code decode_krb5_kdc_req_body
	KRB5_PROTOTYPE((const krb5_data *output, krb5_kdc_req **rep));

krb5_error_code decode_krb5_safe
	KRB5_PROTOTYPE((const krb5_data *output, krb5_safe **rep));

krb5_error_code decode_krb5_priv
	KRB5_PROTOTYPE((const krb5_data *output, krb5_priv **rep));

krb5_error_code decode_krb5_enc_priv_part
	KRB5_PROTOTYPE((const krb5_data *output, krb5_priv_enc_part **rep));

krb5_error_code decode_krb5_cred
	KRB5_PROTOTYPE((const krb5_data *output, krb5_cred **rep));

krb5_error_code decode_krb5_enc_cred_part
	KRB5_PROTOTYPE((const krb5_data *output, krb5_cred_enc_part **rep));

krb5_error_code decode_krb5_error
	KRB5_PROTOTYPE((const krb5_data *output, krb5_error **rep));

krb5_error_code decode_krb5_authdata
	KRB5_PROTOTYPE((const krb5_data *output, krb5_authdata ***rep));

krb5_error_code decode_krb5_pwd_sequence
	KRB5_PROTOTYPE((const krb5_data *output, passwd_phrase_element **rep));

krb5_error_code decode_krb5_pwd_data
	KRB5_PROTOTYPE((const krb5_data *output, krb5_pwd_data **rep));

krb5_error_code decode_krb5_padata_sequence
	KRB5_PROTOTYPE((const krb5_data *output, krb5_pa_data ***rep));

krb5_error_code decode_krb5_alt_method
	KRB5_PROTOTYPE((const krb5_data *output, krb5_alt_method **rep));

krb5_error_code decode_krb5_etype_info
	KRB5_PROTOTYPE((const krb5_data *output, krb5_etype_info_entry ***rep));

krb5_error_code decode_krb5_enc_data
	KRB5_PROTOTYPE((const krb5_data *output, krb5_enc_data **rep));

krb5_error_code decode_krb5_pa_enc_ts
	KRB5_PROTOTYPE((const krb5_data *output, krb5_pa_enc_ts **rep));

krb5_error_code decode_krb5_sam_challenge
	KRB5_PROTOTYPE((const krb5_data *, krb5_sam_challenge **));

krb5_error_code decode_krb5_sam_key
	KRB5_PROTOTYPE((const krb5_data *, krb5_sam_key **));

krb5_error_code decode_krb5_enc_sam_response_enc
	KRB5_PROTOTYPE((const krb5_data *, krb5_enc_sam_response_enc **));

krb5_error_code decode_krb5_sam_response
	KRB5_PROTOTYPE((const krb5_data *, krb5_sam_response **));

krb5_error_code decode_krb5_predicted_sam_response
	KRB5_PROTOTYPE((const krb5_data *, krb5_predicted_sam_response **));

/*************************************************************************
 * End of prototypes for krb5_decode.c
 *************************************************************************/

#endif /* KRB5_ASN1__ */
/*
 * End "asn1.h"
 */


/*
 * Internal krb5 library routines
 */
krb5_error_code krb5_encrypt_tkt_part
	KRB5_PROTOTYPE((krb5_context,
		   krb5_const krb5_keyblock *,
		   krb5_ticket * ));


krb5_error_code krb5_encode_kdc_rep
	KRB5_PROTOTYPE((krb5_context,
		   krb5_const krb5_msgtype,
		   krb5_const krb5_enc_kdc_rep_part *,
		   krb5_const krb5_keyblock *,
		   krb5_kdc_rep *,
		   krb5_data ** ));

krb5_error_code krb5_validate_times
	KRB5_PROTOTYPE((krb5_context, 
			krb5_ticket_times *));
/*
 * [De]Serialization Handle and operations.
 */
struct __krb5_serializer {
    krb5_magic		odtype;
    krb5_error_code	(*sizer) KRB5_NPROTOTYPE((krb5_context,
						  krb5_pointer,
						  size_t *));
    krb5_error_code	(*externalizer) KRB5_NPROTOTYPE((krb5_context,
							 krb5_pointer,
							 krb5_octet **,
							 size_t *));
    krb5_error_code	(*internalizer) KRB5_NPROTOTYPE((krb5_context,
							 krb5_pointer *,
							 krb5_octet **,
							 size_t *));
};
typedef struct __krb5_serializer * krb5_ser_handle;
typedef struct __krb5_serializer krb5_ser_entry;

krb5_ser_handle krb5_find_serializer KRB5_PROTOTYPE((krb5_context,
						     krb5_magic));
krb5_error_code krb5_register_serializer
	KRB5_PROTOTYPE((krb5_context,
			const krb5_ser_entry *));

/* Determine the external size of a particular opaque structure */
krb5_error_code krb5_size_opaque KRB5_PROTOTYPE((krb5_context,
						 krb5_magic,
						 krb5_pointer,
						 size_t *));
/* Serialize the structure into a buffer */
krb5_error_code krb5_externalize_opaque KRB5_PROTOTYPE((krb5_context,
							krb5_magic,
							krb5_pointer,
							krb5_octet **,
							size_t *));
/* Deserialize the structure from a buffer */
krb5_error_code krb5_internalize_opaque KRB5_PROTOTYPE((krb5_context,
							krb5_magic,
							krb5_pointer *,
							krb5_octet **,
							size_t *));

/* Serialize data into a buffer */
krb5_error_code krb5_externalize_data KRB5_PROTOTYPE((krb5_context,
						      krb5_pointer,
						      krb5_octet **,
						      size_t *));
/*
 * Initialization routines.
 */

/* Initialize serialization for krb5_[os_]context */
krb5_error_code krb5_ser_context_init KRB5_PROTOTYPE((krb5_context));

/* Initialize serialization for krb5_auth_context */
krb5_error_code krb5_ser_auth_context_init KRB5_PROTOTYPE((krb5_context));

/* Initialize serialization for krb5_keytab */
krb5_error_code krb5_ser_keytab_init KRB5_PROTOTYPE((krb5_context));

/* Initialize serialization for krb5_ccache */
krb5_error_code krb5_ser_ccache_init KRB5_PROTOTYPE((krb5_context));

/* Initialize serialization for krb5_rcache */
krb5_error_code krb5_ser_rcache_init KRB5_PROTOTYPE((krb5_context));

/* [De]serialize 4-byte integer */
krb5_error_code krb5_ser_pack_int32 KRB5_PROTOTYPE((krb5_int32,
						    krb5_octet **,
						    size_t *));
krb5_error_code krb5_ser_unpack_int32 KRB5_PROTOTYPE((krb5_int32 *,
						      krb5_octet **,
						      size_t *));
/* [De]serialize byte string */
krb5_error_code krb5_ser_pack_bytes KRB5_PROTOTYPE((krb5_octet *,
						    size_t,
						    krb5_octet **,
						    size_t *));
krb5_error_code krb5_ser_unpack_bytes KRB5_PROTOTYPE((krb5_octet *,
						      size_t,
						      krb5_octet **,
						      size_t *));
/*
 * Convenience function for structure magic number
 */
#define KRB5_VERIFY_MAGIC(structure,magic_number) \
    if ((structure)->magic != (magic_number)) return (magic_number);

int krb5_seteuid  KRB5_PROTOTYPE((int));

int krb5_setenv  KRB5_PROTOTYPE((const char *, const char *, int));

void krb5_unsetenv  KRB5_PROTOTYPE((const char *));
#endif /* _KRB5_INT_H */
