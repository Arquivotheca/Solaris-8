#pragma ident	"@(#)autoconf.h	1.1	99/07/18 SMI"
/* autoconf.h.  Generated automatically by configure.  */
/* autoconf.h.in.  Generated automatically from configure.in by autoheader.  */
/* Edited to remove KRB4 compatible and SIZEOF_LONG
 */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

#define ANSI_STDIO 1
#define HAS_SETVBUF 1
#define HAS_STDLIB_H 1
#define HAS_STRDUP 1
#define HAS_LABS 1
#define HAS_VOID_TYPE 1
/* #undef KRB5_NO_PROTOTYPES */
#define KRB5_PROVIDE_PROTOTYPES 1
/* #undef KRB5_NO_NESTED_PROTOTYPES */
/* #undef NO_STDLIB_H */

/* #undef NO_YYLINENO */
#define POSIX_FILE_LOCKS 1
#define POSIX_SIGTYPE 1
#define POSIX_TERMIOS 1
#define POSIX_TYPES 1
#define USE_DIRENT_H 1
#define USE_STRING_H 1
#define WAIT_USES_INT 1
#define krb5_sigtype void
#define HAS_UNISTD_H 1
#define KRB5_USE_INET 1

#define HAVE_STDARG_H 1
/* #undef HAVE_VARARGS_H */

/* Define if MIT Project Athena default configuration should be used */
/* #undef KRB5_ATHENA_COMPAT */

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a short.  */
#define SIZEOF_SHORT 2

/* Define if you have the <dbm.h> header file.  */
/* #undef HAVE_DBM_H */

/* Define if you have the <macsock.h> header file.  */
/* #undef HAVE_MACSOCK_H */

/* Define if you have the <ndbm.h> header file.  */
#define HAVE_NDBM_H 1

/* Define if you have the <stddef.h> header file.  */
#define HAVE_STDDEF_H 1

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <xom.h> header file.  */
/* #undef HAVE_XOM_H */

/* Define if you have the dbm library (-ldbm).  */
/* #undef HAVE_LIBDBM */

/* Define if you have the ndbm library (-lndbm).  */
/* #undef HAVE_LIBNDBM */

/* Define if you have the nsl library (-lnsl).  */
#define HAVE_LIBNSL 1

/* Define if you have the socket library (-lsocket).  */
#define HAVE_LIBSOCKET 1
