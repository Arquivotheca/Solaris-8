/*

 Copyright (C) 1990-1997 Mark Adler, Richard B. Wales, Jean-loup Gailly,
 Kai Uwe Rommel, Onno van der Linden, Igor Mandrichenko and Chris Herborth.
 Permission is granted to any individual or institution to use, copy, or
 redistribute this software so long as all of the original files are included,
 that it is not sold for profit, and that this copyright notice is retained.

 This BeOS-specific file is based on unix.c in the unix directory; changes
 by Chris Herborth (chrish@qnx.com).

*/

#include "zip.h"

#ifndef UTIL    /* the companion #endif is a bit of ways down ... */

#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MATCH shmatch

#include <dirent.h>

#ifdef __GNUC__
#warning NOTE: Expect two warnings for Mime.h...
#endif
#include <kernel/fs_attr.h>
#include <storage/Mime.h>

int get_attr_dir( const char *, char **, off_t * );
int add_UT_ef( struct zlist far * );
int add_Ux_ef( struct zlist far * );
int add_Be_ef( struct zlist far * );

#define PAD 0
#define PATH_END '/'

/* Library functions not in (most) header files */

#ifdef _POSIX_VERSION
#  include <utime.h>
#else
   int utime OF((char *, time_t *));
#endif

extern char *label;
local ulg label_time = 0;
local ulg label_mode = 0;
local time_t label_utim = 0;

/* Local functions */
local char *readd OF((DIR *));


#ifdef NO_DIR                    /* for AT&T 3B1 */
#include <sys/dir.h>
#ifndef dirent
#  define dirent direct
#endif
typedef FILE DIR;
/*
**  Apparently originally by Rich Salz.
**  Cleaned up and modified by James W. Birdsall.
*/

#define opendir(path) fopen(path, "r")

struct dirent *readdir(dirp)
DIR *dirp;
{
  static struct dirent entry;

  if (dirp == NULL)
    return NULL;
  for (;;)
    if (fread (&entry, sizeof (struct dirent), 1, dirp) == 0)
      return NULL;
    else if (entry.d_ino)
      return (&entry);
} /* end of readdir() */

#define closedir(dirp) fclose(dirp)
#endif /* NO_DIR */


local char *readd(d)
DIR *d;                 /* directory stream to read from */
/* Return a pointer to the next name in the directory stream d, or NULL if
   no more entries or an error occurs. */
{
  struct dirent *e;

  e = readdir(d);
  return e == NULL ? (char *) NULL : e->d_name;
}

int procname(n)
char *n;                /* name to process */
/* Process a name or sh expression to operate on (or exclude).  Return
   an error code in the ZE_ class. */
{
  char *a;              /* path and name for recursion */
  DIR *d;               /* directory stream from opendir() */
  char *e;              /* pointer to name from readd() */
  int m;                /* matched flag */
  char *p;              /* path for recursion */
  struct stat s;        /* result of stat() */
  struct zlist far *z;  /* steps through zfiles list */

  if (strcmp(n, "-") == 0)   /* if compressing stdin */
    return newname(n, 0);
  else if (LSSTAT(n, &s))
  {
    /* Not a file or directory--search for shell expression in zip file */
    p = ex2in(n, 0, (int *)NULL);       /* shouldn't affect matching chars */
    m = 1;
    for (z = zfiles; z != NULL; z = z->nxt) {
      if (MATCH(p, z->iname))
      {
        z->mark = pcount ? filter(z->zname) : 1;
        if (verbose)
            fprintf(mesg, "zip diagnostic: %scluding %s\n",
               z->mark ? "in" : "ex", z->name);
        m = 0;
      }
    }
    free((zvoid *)p);
    return m ? ZE_MISS : ZE_OK;
  }

  /* Live name--use if file, recurse if directory */
  if ((s.st_mode & S_IFDIR) == 0)
  {
    /* add or remove name of file */
    if ((m = newname(n, 0)) != ZE_OK)
      return m;
  } else {
    /* Add trailing / to the directory name */
    if ((p = malloc(strlen(n)+2)) == NULL)
      return ZE_MEM;
    if (strcmp(n, ".") == 0) {
      *p = '\0';  /* avoid "./" prefix and do not create zip entry */
    } else {
      strcpy(p, n);
      a = p + strlen(p);
      if (a[-1] != '/')
        strcpy(a, "/");
      if (dirnames && (m = newname(p, 1)) != ZE_OK) {
        free((zvoid *)p);
        return m;
      }
    }
    /* recurse into directory */
    if (recurse && (d = opendir(n)) != NULL)
    {
      while ((e = readd(d)) != NULL) {
        if (strcmp(e, ".") && strcmp(e, ".."))
        {
          if ((a = malloc(strlen(p) + strlen(e) + 1)) == NULL)
          {
            closedir(d);
            free((zvoid *)p);
            return ZE_MEM;
          }
          strcat(strcpy(a, p), e);
          if ((m = procname(a)) != ZE_OK)   /* recurse on name */
          {
            if (m == ZE_MISS)
              zipwarn("name not matched: ", a);
            else
              ziperr(m, a);
          }
          free((zvoid *)a);
        }
      }
      closedir(d);
    }
    free((zvoid *)p);
  } /* (s.st_mode & S_IFDIR) == 0) */
  return ZE_OK;
}

char *ex2in(x, isdir, pdosflag)
char *x;                /* external file name */
int isdir;              /* input: x is a directory */
int *pdosflag;          /* output: force MSDOS file attributes? */
/* Convert the external file name to a zip file name, returning the malloc'ed
   string or NULL if not enough memory. */
{
  char *n;              /* internal file name (malloc'ed) */
  char *t;              /* shortened name */
  int dosflag;

  dosflag = dosify;  /* default for non-DOS and non-OS/2 */

  /* Find starting point in name before doing malloc */
  for (t = x; *t == '/'; t++)
    ;

  /* Make changes, if any, to the copied name (leave original intact) */
  if (!pathput)
    t = last(t, PATH_END);

  /* Malloc space for internal name and copy it */
  if ((n = malloc(strlen(t) + 1)) == NULL)
    return NULL;
  strcpy(n, t);

  if (isdir == 42) return n;      /* avoid warning on unused variable */

  if (dosify)
    msname(n);

  /* Returned malloc'ed name */
  if (pdosflag)
    *pdosflag = dosflag;
  return n;
}


char *in2ex(n)
char *n;                /* internal file name */
/* Convert the zip file name to an external file name, returning the malloc'ed
   string or NULL if not enough memory. */
{
  char *x;              /* external file name */

  if ((x = malloc(strlen(n) + 1 + PAD)) == NULL)
    return NULL;
  strcpy(x, n);
  return x;
}

/*
 * use ztimbuf in both POSIX and non POSIX cases ?
 */
void stamp(f, d)
char *f;                /* name of file to change */
ulg d;                  /* dos-style time to change it to */
/* Set last updated and accessed time of file f to the DOS time d. */
{
#ifdef _POSIX_VERSION
  struct utimbuf u;     /* argument for utime()  const ?? */
#else
  time_t u[2];          /* argument for utime() */
#endif

  /* Convert DOS time to time_t format in u */
#ifdef _POSIX_VERSION
  u.actime = u.modtime = dos2unixtime(d);
  utime(f, &u);
#else
  u[0] = u[1] = dos2unixtime(d);
  utime(f, u);
#endif

}

ulg filetime(f, a, n, t)
char *f;                /* name of file to get info on */
ulg *a;                 /* return value: file attributes */
long *n;                /* return value: file size */
iztimes *t;             /* return value: access, modific. and creation times */
/* If file *f does not exist, return 0.  Else, return the file's last
   modified date and time as an MSDOS date and time.  The date and
   time is returned in a long with the date most significant to allow
   unsigned integer comparison of absolute times.  Also, if a is not
   a NULL pointer, store the file attributes there, with the high two
   bytes being the Unix attributes, and the low byte being a mapping
   of that to DOS attributes.  If n is not NULL, store the file size
   there.  If t is not NULL, the file's access, modification and creation
   times are stored there as UNIX time_t values.
   If f is "-", use standard input as the file. If f is a device, return
   a file size of -1 */
{
  struct stat s;        /* results of stat() */
  char name[FNMAX];
  int len = strlen(f);

  if (f == label) {
    if (a != NULL)
      *a = label_mode;
    if (n != NULL)
      *n = -2L; /* convention for a label name */
    if (t != NULL)
      t->atime = t->mtime = t->ctime = label_utim;
    return label_time;
  }
  strcpy(name, f);
  if (name[len - 1] == '/')
    name[len - 1] = '\0';
  /* not all systems allow stat'ing a file with / appended */
  if (strcmp(f, "-") == 0) {
    if (fstat(fileno(stdin), &s) != 0)
      error("fstat(stdin)");
  } else if (LSSTAT(name, &s) != 0)
             /* Accept about any file kind including directories
              * (stored with trailing / with -r option)
              */
    return 0;

  if (a != NULL) {
    *a = ((ulg)s.st_mode << 16) | !(s.st_mode & S_IWRITE);
    if ((s.st_mode & S_IFMT) == S_IFDIR) {
      *a |= MSDOS_DIR_ATTR;
    }
  }
  if (n != NULL)
    *n = (s.st_mode & S_IFMT) == S_IFREG ? s.st_size : -1L;
  if (t != NULL) {
    t->atime = s.st_atime;
    t->mtime = s.st_mtime;
    t->ctime = s.st_mtime;   /* best guess (s.st_ctime: last status change!) */
  }

  return unix2dostime(&s.st_mtime);
}

/* ----------------------------------------------------------------------

Return a malloc()'d buffer containing all of the attributes and their names
for the file specified in name.  You have to free() this yourself.  The length
of the buffer is also returned.

If get_attr_dir() fails, the buffer will be NULL, total_size will be 0,
and an error will be returned:

    EOK    - no errors occurred
    EINVAL - attr_buff was pointing at a buffer
    ENOMEM - insufficient memory for attribute buffer

Other errors are possible (whatever is returned by the fs_attr.h functions).

PROBLEMS:

- pointers are 32-bits; attributes are limited to off_t in size so it's
  possible to overflow... in practice, this isn't too likely... your
  machine will thrash like hell before that happens

*/

#define INITIAL_BUFF_SIZE 65536

int get_attr_dir( const char *name, char **attr_buff, off_t *total_size )
{
    int               retval = EOK;
    int               fd;
    DIR              *fa_dir;
    struct dirent    *fa_ent;
#ifdef __GNUC__
#warning Expect a warning for attrs_size... gcc is lying.
#endif
    off_t             attrs_size;
    off_t             this_size;
    char             *ptr;
    struct attr_info  fa_info;

    retval      = EOK;
    attrs_size  = 0;    /* gcc still says this is used uninitialized... */
    *total_size = 0;

    /* ----------------------------------------------------------------- */
    /* Sanity-check.                                                     */
    if( *attr_buff != NULL ) {
        return EINVAL;
    }

    /* ----------------------------------------------------------------- */
    /* Can we open the file/directory?                                   */
    fd = open( name, O_RDONLY );
    if( fd < 0 ) {
        return errno;
    }

    /* ----------------------------------------------------------------- */
    /* Allocate an initial buffer; 64k should usually be enough.         */
    *attr_buff = (char *)malloc( INITIAL_BUFF_SIZE );
    ptr        = *attr_buff;
    if( ptr == NULL ) {
        close( fd );

        return ENOMEM;
    }

    /* ----------------------------------------------------------------- */
    /* Open the attributes directory for this file.                      */
    fa_dir = fs_fopen_attr_dir( fd );
    if( fa_dir == NULL ) {
        close( fd );

        free( ptr );
        *attr_buff = NULL;

        return retval;
    }

    /* ----------------------------------------------------------------- */
    /* Read all the attributes; the buffer could grow > 64K if there are */
    /* many and/or they are large.                                       */
    fa_ent = fs_read_attr_dir( fa_dir );
    while( fa_ent != NULL ) {
        retval = fs_stat_attr( fd, fa_ent->d_name, &fa_info );
        /* TODO: check retval != EOK */

        this_size  = strlen( fa_ent->d_name ) + 1;
        this_size += sizeof( struct attr_info );
        this_size += fa_info.size;

        attrs_size += this_size;

        if( attrs_size > INITIAL_BUFF_SIZE ) {
            unsigned long offset = ptr - *attr_buff;

            *attr_buff = (char *)realloc( *attr_buff, attrs_size );
            if( *attr_buff == NULL ) {
                retval = fs_close_attr_dir( fa_dir );
                /* TODO: check retval != EOK */
                close( fd );

                return ENOMEM;
            }

            ptr = *attr_buff + offset;
        }

        /* Now copy the data for this attribute into the buffer. */
        strcpy( ptr, fa_ent->d_name );
        ptr += strlen( fa_ent->d_name );
        *ptr++ = '\0';

        memcpy( ptr, &fa_info, sizeof( struct attr_info ) );
        ptr += sizeof( struct attr_info );

        if( fa_info.size > 0 ) {
            ssize_t read_bytes;

            read_bytes = fs_read_attr( fd, fa_ent->d_name, fa_info.type, 0,
                                       ptr, fa_info.size );
            if( read_bytes != fa_info.size ) {
                /* print a warning about mismatched sizes */
                char buff[80];

                sprintf( buff, "read %ld, expected %ld",
                         (ssize_t)read_bytes, (ssize_t)fa_info.size );
                zipwarn( "attribute size mismatch: ", buff );
            }
            ptr += fa_info.size;
        }

        fa_ent = fs_read_attr_dir( fa_dir );
    }

    /* ----------------------------------------------------------------- */
    /* Close the attribute directory.                                    */
    retval = fs_close_attr_dir( fa_dir );
    /* TODO: check retval != EOK */

    /* ----------------------------------------------------------------- */
    /* If the buffer is too big, shrink it.                              */
    if( attrs_size < INITIAL_BUFF_SIZE ) {
        *attr_buff = (char *)realloc( *attr_buff, attrs_size );
        if( *attr_buff == NULL ) {
            /* This really shouldn't happen... */
            close( fd );

            return ENOMEM;
        }
    }

    *total_size = attrs_size;

    close( fd );

    return EOK;
}

/* ---------------------------------------------------------------------- */
/* Add a 'UT' extra field to the zlist data pointed to by z.              */

#define EB_L_UT_SIZE    (EB_HEADSIZE + EB_UT_LEN(2))
#define EB_C_UT_SIZE    (EB_HEADSIZE + EB_UT_LEN(1))

int add_UT_ef( struct zlist far *z )
{
    char        *l_ef = NULL;
    char        *c_ef = NULL;
    struct stat  s;

    /* We can't work if there's no entry to work on. */
    if( z == NULL ) {
        return ZE_LOGIC;
    }

    /* Check to make sure we've got enough room in the extra fields. */
    if( z->ext + EB_L_UT_SIZE > USHRT_MAX ||
        z->cext + EB_C_UT_SIZE > USHRT_MAX ) {
        return ZE_MEM;
    }

    /* stat() the file (or the symlink) to get the data; if we can't get */
    /* the data, there's no point in trying to fill out the fields.      */
    if(LSSTAT( z->name, &s ) ) {
        return ZE_OPEN;
    }

    /* Allocate memory for the local and central extra fields. */
    if( z->extra && z->ext != 0 ) {
        z->extra = (char *)realloc( z->extra, z->ext + EB_L_UT_SIZE );
        l_ef = z->extra + z->ext;
    } else {
        z->extra = (char *)malloc( EB_L_UT_SIZE );
        z->ext = 0;
        l_ef = z->extra;
    }
    if( l_ef == NULL ) {
        return ZE_MEM;
    }

    if( z->cextra && z->cext != 0 ) {
        z->cextra = (char *)realloc( z->cextra, z->cext + EB_C_UT_SIZE );
        c_ef = z->cextra + z->cext;
    } else {
        z->cextra = (char *)malloc( EB_C_UT_SIZE );
        z->cext = 0;
        c_ef = z->cextra;
    }
    if( c_ef == NULL ) {
        return ZE_MEM;
    }

    /* Now add the local version of the field. */
    *l_ef++ = 'U';
    *l_ef++ = 'T';
    *l_ef++ = (char)(EB_UT_LEN(2)); /* length of data in local EF */
    *l_ef++ = (char)(EB_UT_LEN(2) >> 8);
    *l_ef++ = (char)(EB_UT_FL_MTIME | EB_UT_FL_ATIME);
    *l_ef++ = (char)(s.st_mtime);
    *l_ef++ = (char)(s.st_mtime >> 8);
    *l_ef++ = (char)(s.st_mtime >> 16);
    *l_ef++ = (char)(s.st_mtime >> 24);
    *l_ef++ = (char)(s.st_atime);
    *l_ef++ = (char)(s.st_atime >> 8);
    *l_ef++ = (char)(s.st_atime >> 16);
    *l_ef++ = (char)(s.st_atime >> 24);

    z->ext += EB_L_UT_SIZE;

    /* Now add the central version. */
    *c_ef++ = 'U';
    *c_ef++ = 'T';
    *c_ef++ = (char)(EB_UT_LEN(1)); /* length of data in central EF */
    *c_ef++ = (char)(EB_UT_LEN(1) >> 8);
    *c_ef++ = (char)(EB_UT_FL_MTIME);
    *c_ef++ = (char)(s.st_mtime);
    *c_ef++ = (char)(s.st_mtime >> 8);
    *c_ef++ = (char)(s.st_mtime >> 16);
    *c_ef++ = (char)(s.st_mtime >> 24);

    z->cext += EB_C_UT_SIZE;

    return ZE_OK;
}

/* ---------------------------------------------------------------------- */
/* Add a 'Ux' extra field to the zlist data pointed to by z.              */

#define EB_L_UX2_SIZE   (EB_HEADSIZE + EB_UX2_MINLEN)
#define EB_C_UX2_SIZE   (EB_HEADSIZE)

int add_Ux_ef( struct zlist far *z )
{
    char        *l_ef = NULL;
    char        *c_ef = NULL;
    struct stat  s;

    /* Check to make sure we've got enough room in the extra fields. */
    if( z->ext + EB_L_UX2_SIZE > USHRT_MAX ||
        z->cext + EB_C_UX2_SIZE > USHRT_MAX ) {
        return ZE_MEM;
    }

    /* stat() the file (or the symlink) to get the data; if we can't get */
    /* the data, there's no point in trying to fill out the fields.      */
    if(LSSTAT( z->name, &s ) ) {
        return ZE_OPEN;
    }

    /* Allocate memory for the local and central extra fields. */
    if( z->extra && z->ext != 0 ) {
        z->extra = (char *)realloc( z->extra, z->ext + EB_L_UX2_SIZE );
        l_ef = z->extra + z->ext;
    } else {
        z->extra = (char *)malloc( EB_L_UX2_SIZE );
        z->ext = 0;
        l_ef = z->extra;
    }
    if( l_ef == NULL ) {
        return ZE_MEM;
    }

    if( z->cextra && z->cext != 0 ) {
        z->cextra = (char *)realloc( z->cextra, z->cext + EB_C_UX2_SIZE );
        c_ef = z->cextra + z->cext;
    } else {
        z->cextra = (char *)malloc( EB_C_UX2_SIZE );
        z->cext = 0;
        c_ef = z->cextra;
    }
    if( c_ef == NULL ) {
        return ZE_MEM;
    }

    /* Now add the local version of the field. */
    *l_ef++ = 'U';
    *l_ef++ = 'x';
    *l_ef++ = (char)(EB_UX2_MINLEN);
    *l_ef++ = (char)(EB_UX2_MINLEN >> 8);
    *l_ef++ = (char)(s.st_uid);
    *l_ef++ = (char)(s.st_uid >> 8);
    *l_ef++ = (char)(s.st_gid);
    *l_ef++ = (char)(s.st_gid >> 8);

    z->ext += EB_L_UX2_SIZE;

    /* Now add the central version of the field. */
    *c_ef++ = 'U';
    *c_ef++ = 'x';
    *c_ef++ = 0;
    *c_ef++ = 0;

    z->cext += EB_C_UX2_SIZE;

    return ZE_OK;
}

/* ---------------------------------------------------------------------- */
/* Add a 'Be' extra field to the zlist data pointed to by z.              */

#define EB_L_BE_SIZE    (EB_HEADSIZE + EB_L_BE_LEN) /* + attr size */
#define EB_C_BE_SIZE    (EB_HEADSIZE + EB_C_BE_LEN)

#define MEMCOMPRESS_OVERHEAD    6   /* ush compression type, ulg CRC */

int add_Be_ef( struct zlist far *z )
{
    char *l_ef       = NULL;
    char *c_ef       = NULL;
    char *attrbuff   = NULL;
    off_t attrsize   = 0;
    char *compbuff   = NULL;
    ush   compsize   = 0;
    uch   flags      = 0;

#ifdef BE_NO_SYMLINK_ATTRS
    /* See osdep.h; there's currently no way to save/restore a symbolic     */
    /* link's attributes from C, and I'm loathe to introduce any C++. [cjh] */
    {
        struct stat s;

        if( SSTAT( z->name, &s ) ) {
            return ZE_OPEN;
        }

        if( S_ISLNK( s.st_mode ) ) {
            return ZE_OK;
        }
    }
#endif

    /* Check to make sure we've got enough room in the extra fields. */
    if( z->ext + EB_L_BE_SIZE > USHRT_MAX ||
        z->cext + EB_C_BE_SIZE > USHRT_MAX ) {
        return ZE_MEM;
    }

    /* Attempt to load up a buffer full of the file's attributes. */
    {
        int retval;

        retval = get_attr_dir( z->name, &attrbuff, &attrsize );
        if( retval != EOK ) {
            return ZE_OPEN;
        }
        if( attrsize == 0 ) {
            return ZE_OK;
        }
        if( attrbuff == NULL ) {
            return ZE_LOGIC;
        }

        /* Check for way too much data. */
        if( attrsize > (off_t)ULONG_MAX ) {
            zipwarn( "uncompressed attributes truncated", "" );
            attrsize = (off_t)(ULONG_MAX - MEMCOMPRESS_OVERHEAD);
        }
    }

    if( verbose ) {
        printf( "\t[in=%ld]", (unsigned long)attrsize );
    }

    /* Try compressing the data */
    compbuff = (char *)malloc( (size_t)attrsize + MEMCOMPRESS_OVERHEAD );
    if( compbuff == NULL ) {
        return ZE_MEM;
    }
    compsize = memcompress( compbuff,
                            (size_t)attrsize + MEMCOMPRESS_OVERHEAD,
                            attrbuff,
                            (size_t)attrsize );
    if( verbose ) {
        printf( " [out=%d]", compsize );
    }

    /* Attempt to optimise very small attributes. */
    if( compsize > attrsize ) {
        free( compbuff );
        compsize = (ush)attrsize;
        compbuff = attrbuff;

        flags = EB_BE_FL_NATURAL;
    }

    /* Check to see if we really have enough room in the EF for the data. */
    if( ( z->ext + compsize + EB_L_BE_LEN ) > USHRT_MAX ) {
        compsize = USHRT_MAX - EB_L_BE_LEN - z->ext;
    }

    /* Allocate memory for the local and central extra fields. */
    if( z->extra && z->ext != 0 ) {
        z->extra = (char *)realloc( z->extra, z->ext + EB_L_BE_SIZE + compsize );
        l_ef = z->extra + z->ext;
    } else {
        z->extra = (char *)malloc( EB_L_BE_SIZE + compsize );
        z->ext = 0;
        l_ef = z->extra;
    }
    if( l_ef == NULL ) {
        return ZE_MEM;
    }

    if( z->cextra && z->cext != 0 ) {
        z->cextra = (char *)realloc( z->cextra, z->cext + EB_C_BE_SIZE );
        c_ef = z->cextra + z->cext;
    } else {
        z->cextra = (char *)malloc( EB_C_BE_SIZE );
        z->cext = 0;
        c_ef = z->cextra;
    }
    if( c_ef == NULL ) {
        return ZE_MEM;
    }

    /* Now add the local version of the field. */
    *l_ef++ = 'B';
    *l_ef++ = 'e';
    *l_ef++ = (char)(compsize + EB_L_BE_LEN);
    *l_ef++ = (char)((compsize + EB_L_BE_LEN) >> 8);
    *l_ef++ = (char)((unsigned long)attrsize);
    *l_ef++ = (char)((unsigned long)attrsize >> 8);
    *l_ef++ = (char)((unsigned long)attrsize >> 16);
    *l_ef++ = (char)((unsigned long)attrsize >> 24);
    *l_ef++ = flags;
    memcpy( l_ef, compbuff, (size_t)compsize );

    z->ext += EB_L_BE_SIZE + compsize;

    /* And the central version. */
    *c_ef++ = 'B';
    *c_ef++ = 'e';
    *c_ef++ = (char)(EB_C_BE_LEN);
    *c_ef++ = (char)(EB_C_BE_LEN >> 8);
    *c_ef++ = (char)compsize;
    *c_ef++ = (char)(compsize >> 8);
    *c_ef++ = (char)(compsize >> 16);
    *c_ef++ = (char)(compsize >> 24);
    *c_ef++ = flags;

    z->cext += EB_C_BE_SIZE;

    return ZE_OK;
}

/* Extra field info:
   - 'UT' - UNIX time extra field
   - 'Ux' - UNIX uid/gid extra field
   - 'Be' - BeOS file attributes extra field

   This is done the same way ../unix/unix.c stores the 'UT'/'Ux' fields
   (full data in local header, only modification time in central header),
   with the 'Be' field added to the end and the size of the 'Be' field
   in the central header.

   See the end of beos/osdep.h for a simple explanation of the 'Be' EF
   layout.
 */
int set_extra_field(z, z_utim)
  struct zlist far *z;
  iztimes *z_utim;
  /* store full data in local header but just modification time stamp info
     in central header */
{
    int retval;

    /* Tell picky compilers to shut up about unused variables. */
    z_utim = z_utim;

    /* Check to make sure z is valid. */
    if( z == NULL ) {
        return ZE_LOGIC;
    }

    /* This function is much simpler now that I've moved the extra fields */
    /* out... it simplified the 'Be' code, too.                           */
    retval = add_UT_ef( z );
    if( retval != ZE_OK ) {
        return retval;
    }

    retval = add_Ux_ef( z );
    if( retval != ZE_OK ) {
        return retval;
    }

    retval = add_Be_ef( z );
    if( retval != ZE_OK ) {
        return retval;
    }

    return ZE_OK;
}

/* ---------------------------------------------------------------------- */
/* Set a file's MIME type.                                                */
void setfiletype( const char *file, const char *type )
{
    int fd;
    attr_info fa;
    ssize_t wrote_bytes;

    fd = open( file, O_RDWR );
    if( fd < 0 ) {
        zipwarn( "can't open zipfile to write file type", "" );
        return;
    }

    fa.type = B_MIME_STRING_TYPE;
    fa.size = (off_t)(strlen( type ) + 1);

    wrote_bytes = fs_write_attr( fd, BE_FILE_TYPE_NAME, fa.type, 0,
                                 type, fa.size );
    if( wrote_bytes != (ssize_t)fa.size ) {
        zipwarn( "couldn't write complete file type", "" );
    }

    close( fd );
}

int deletedir(d)
char *d;                /* directory to delete */
/* Delete the directory *d if it is empty, do nothing otherwise.
   Return the result of rmdir(), delete(), or system().
   For VMS, d must be in format [x.y]z.dir;1  (not [x.y.z]).
 */
{
# ifdef NO_RMDIR
    /* code from Greg Roelofs, who horked it from Mark Edwards (unzip) */
    int r, len;
    char *s;              /* malloc'd string for system command */

    len = strlen(d);
    if ((s = malloc(len + 34)) == NULL)
      return 127;

    sprintf(s, "IFS=\" \t\n\" /bin/rmdir %s 2>/dev/null", d);
    r = system(s);
    free(s);
    return r;
# else /* !NO_RMDIR */
    return rmdir(d);
# endif /* ?NO_RMDIR */
}

#endif /* !UTIL */


/******************************/
/*  Function version_local()  */
/******************************/

void version_local()
{
    static ZCONST char CompiledWith[] = "Compiled with %s%s for %s%s%s%s.\n\n";

    printf(CompiledWith,

#ifdef __MWERKS__
      "Metrowerks CodeWarrior", "",
#else
#  ifdef __GNUC__
      "gcc ", __VERSION__,
#  endif
#endif

      "BeOS",

#ifdef __POWERPC__
      " (PowerPC)",
#endif

#ifdef __DATE__
      " on ", __DATE__
#else
      "", ""
#endif
    );

} /* end function version_local() */
