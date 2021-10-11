/*

 Copyright (C) 1990-1997 Mark Adler, Richard B. Wales, Jean-loup Gailly,
 Kai Uwe Rommel, Onno van der Linden and Igor Mandrichenko.
 Permission is granted to any individual or institution to use, copy, or
 redistribute this software so long as all of the original files are included,
 that it is not sold for profit, and that this copyright notice is retained.

*/

/*
 *  fileio.c by Mark Adler.
 *
 */

#include "zip.h"

#include <time.h>

#ifdef QDOS
# ifndef MATCH
#  define MATCH shmatch
# endif
#endif

#ifdef OSF
#define EXDEV 18   /* avoid a bug in the DEC OSF/1 header files. */
#else
#include <errno.h>
#endif

#ifdef NO_ERRNO
extern int errno;
#endif

#if (defined(DOS) && !defined(__GO32__) && !defined(__EMX__)) || defined(WIN32)
#define MATCH dosmatch
#else
#define MATCH shmatch
#endif

#if defined(VMS) || defined(TOPS20)
#  define PAD 5
#else
#  define PAD 0
#endif

#ifdef NO_RENAME
int rename OF((ZCONST char *, ZCONST char *));
#endif


#ifndef UTIL    /* the companion #endif is a bit of ways down ... */

/* Local functions */
local int fqcmp  OF((ZCONST zvoid *, ZCONST zvoid *));
local int fqcmpz OF((ZCONST zvoid *, ZCONST zvoid *));

/* Local module level variables. */
char *label = NULL;                /* global, but only used in `system'.c */
local struct stat zipstatb;
#ifndef WINDLL
local int zipstate = -1;
#else
int zipstate;
#endif
/* -1 unknown, 0 old zip file exists, 1 new zip file */

char *getnam(n, fp)
char *n;                /* where to put name (must have >=FNMAX+1 bytes) */
FILE *fp;
/* Read a \n or \r delimited name from stdin into n, and return
   n.  If EOF, then return NULL.  Also, if the name read is too big, return
   NULL. */
{
  int c;                /* last character read */
  char *p;              /* pointer into name area */

  p = n;
  while ((c = getc(fp)) == '\n' || c == '\r')
    ;
  if (c == EOF)
    return NULL;
  do {
    if (p - n >= FNMAX)
      return NULL;
    *p++ = (char) c;
    c = getc(fp);
  } while (c != EOF && (c != '\n' && c != '\r'));
  *p = 0;
  return n;
}

struct flist far *fexpel(f)
struct flist far *f;    /* entry to delete */
/* Delete the entry *f in the doubly-linked found list.  Return pointer to
   next entry to allow stepping through list. */
{
  struct flist far *t;  /* temporary variable */

  t = f->nxt;
  *(f->lst) = t;                        /* point last to next, */
  if (t != NULL)
    t->lst = f->lst;                    /* and next to last */
  if (f->name != NULL)                  /* free memory used */
    free((zvoid *)(f->name));
  if (f->zname != NULL)
    free((zvoid *)(f->zname));
  if (f->iname != NULL)
    free((zvoid *)(f->iname));
  farfree((zvoid far *)f);
  fcount--;                             /* decrement count */
  return t;                             /* return pointer to next */
}


local int fqcmp(a, b)
ZCONST zvoid *a, *b;          /* pointers to pointers to found entries */
/* Used by qsort() to compare entries in the found list by name. */
{
  return strcmp((*(struct flist far **)a)->name,
                (*(struct flist far **)b)->name);
}


local int fqcmpz(a, b)
ZCONST zvoid *a, *b;          /* pointers to pointers to found entries */
/* Used by qsort() to compare entries in the found list by zname. */
{
  return strcmp((*(struct flist far **)a)->zname,
                (*(struct flist far **)b)->zname);
}


char *last(p, c)
char *p;                /* sequence of path components */
int c;                  /* path components separator character */
/* Return a pointer to the start of the last path component. For a directory
 * name terminated by the character in c, the return value is an empty string.
 */
{
  char *t;              /* temporary variable */

  if ((t = strrchr(p, c)) != NULL)
    return t + 1;
  else
#ifndef AOS_VS
    return p;
#else
/* We want to allow finding of end of path in either AOS/VS-style pathnames
 * or Unix-style pathnames.  This presents a few little problems ...
 */
  {
    if (*p == '='  ||  *p == '^')      /* like ./ and ../ respectively */
      return p + 1;
    else
      return p;
  }
#endif
}


char *msname(n)
char *n;
/* Reduce all path components to MSDOS upper case 8.3 style names.  Probably
   should also check for invalid characters, but I don't know which ones
   those are.  We at least remove spaces and colons. */
{
  int c;                /* current character */
  int f;                /* characters in current component */
  char *p;              /* source pointer */
  char *q;              /* destination pointer */

  p = q = n;
  f = 0;
  while ((c = (unsigned char)*p++) != 0)
    if (c == ' ' || c == ':' /* or anything else illegal */ )
      q++;                              /* char is discarded */
    else if (c == '/')
    {
      *q++ = (char)c;
      f = 0;                            /* new component */
    }
#ifdef __human68k__
    else if (iskanji(c))
    {
      if (f == 7 || f == 11)
        f++;
      else if (*p != '\0' && f < 12 && f != 8)
      {
        *q++ = c;
        *q++ = *p++;
        f += 2;
      }
    }
#endif /* __human68k__ */
    else if (c == '.')
      if (f < 9)
      {
        *q++ = (char)c;
        f = 9;                          /* now in file type */
      }
      else
        f = 12;                         /* now just excess characters */
    else
      if (f < 12 && f != 8)
      {
        *q++ = (char)(to_up(c));
        f++;                            /* do until end of name or type */
      }
  *q = 0;
  return n;
}

int check_dup()
/* Sort the found list and remove duplicates.
   Return an error code in the ZE_ class. */
{
  struct flist far *f;          /* steps through found linked list */
  extent j, k;                  /* indices for s */
  struct flist far **s;         /* sorted table */
  struct flist far **nodup;     /* sorted table without duplicates */

  /* sort found list, remove duplicates */
  if (fcount)
  {
    if ((s = (struct flist far **)malloc(
         fcount * sizeof(struct flist far *))) == NULL)
      return ZE_MEM;
    for (j = 0, f = found; f != NULL; f = f->nxt)
      s[j++] = f;
    /* Check names as given (f->name) */
    qsort((char *)s, fcount, sizeof(struct flist far *), fqcmp);
    for (k = j = fcount - 1; j > 0; j--)
      if (strcmp(s[j - 1]->name, s[j]->name) == 0)
        /* remove duplicate entry from list */
        fexpel(s[j]);           /* fexpel() changes fcount */
      else
        /* copy valid entry into destination position */
        s[k--] = s[j];
    s[k] = s[0];                /* First entry is always valid */
    nodup = &s[k];              /* Valid entries are at end of array s */

    /* sort only valid items and check for unique internal names */
    qsort((char *)nodup, fcount, sizeof(struct flist far *), fqcmpz);
    for (j = 1; j < fcount; j++)
      if (strcmp(nodup[j - 1]->zname, nodup[j]->zname) == 0)
      {
        zipwarn("  first full name: ", nodup[j - 1]->name);
        zipwarn(" second full name: ", nodup[j]->name);
        zipwarn("name in zip file repeated: ", nodup[j]->zname);
        return ZE_PARMS;
      }
    free((zvoid *)s);
  }
  return ZE_OK;
}

int filter(name)
  char *name;
  /* Scan the -i and -x lists for matches to the given name.
     Return true if the name must be included, false otherwise.
     Give precedence to -x over -i.
   */
{
   int n, slashes;
   int include = icount ? 0 : 1;
   char *p, *q;

   if (pcount == 0) return 1;

   for (n = 0; n < pcount; n++) {
      if (!patterns[n].zname[0])        /* it can happen... */
          continue;
      p = name;
      if (patterns[n].select == 'R') {
         /* With -R patterns, if the pattern has N path components (that is, */
         /* N-1 slashes), then we test only the last N components of name.   */
         slashes = 0;
         for (q = patterns[n].zname; (q = strchr(q, '/')) != NULL; q++)
            slashes++;
         for (q = p + strlen(p); q > p; q--)
            if (q[-1] == '/')
               if (!slashes--) {
                  p = q;
                  break;
               }
      }
      if (MATCH(patterns[n].zname, p)) {
         if (patterns[n].select == 'x') return 0;
         include = 1;
      }
   }
   return include;
}

int newname(name, isdir)
char *name;                /* name to add (or exclude) */
int  isdir;             /* true for a directory */
/* Add (or exclude) the name of an existing disk file.  Return an error
   code in the ZE_ class. */
{
  char *iname, *zname;  /* Internal name, external version of iname */
  char *undosm;
  struct flist far *f;  /* where in found, or new found entry */
  struct zlist far *z;  /* where in zfiles (if found) */
  int dosflag;

  /* Search for name in zip file.  If there, mark it, else add to
     list of new names to do (or remove from that list). */
  if ((iname = ex2in(name, isdir, &dosflag)) == NULL)
    return ZE_MEM;

  /* Discard directory names with zip -rj */
  if (*iname == '\0') {
#ifndef AMIGA
/* A null string is a legitimate external directory name in AmigaDOS; also,
 * a command like "zip -r zipfile FOO:" produces an empty internal name.
 */
# ifndef RISCOS
 /* If extensions needs to be swapped, we will have empty directory names
    instead of the original directory. For example, zipping 'c.', 'c.main'
    should zip only 'main.c' while 'c.' will be converted to '\0' by ex2in. */

    if (pathput && !recurse) error("empty name without -j or -r");

# endif /* !RISCOS */
#endif /* !AMIGA */
    free((zvoid *)iname);
    return ZE_OK;
  }
  undosm = iname;
  if (dosflag || !pathput) {
    int save_dosify = dosify, save_pathput = pathput;
    dosify = 0;
    pathput = 1;
    if ((undosm = ex2in(name, isdir, NULL)) == NULL)
      undosm = iname;
    dosify = save_dosify;
    pathput = save_pathput;
  }
  if ((zname = in2ex(iname)) == NULL)
    return ZE_MEM;
  if ((z = zsearch(zname)) != NULL) {
    if (pcount && !filter(undosm)) {
      /* Do not clear z->mark if "exclude", because, when "dosify || !pathput"
       * is in effect, two files with different filter options may hit the
       * same z entry.
       */
      if (verbose)
        fprintf(mesg, "excluding %s\n", zname);
      free((zvoid *)iname);
      free((zvoid *)zname);
    } else {
      z->mark = 1;
      if ((z->name = malloc(strlen(name) + 1 + PAD)) == NULL) {
        if (undosm != iname)
          free((zvoid *)undosm);
        free((zvoid *)iname);
        free((zvoid *)zname);
        return ZE_MEM;
      }
      strcpy(z->name, name);
      z->dosflag = dosflag;

#ifdef FORCE_NEWNAME
      free((zvoid *)(z->iname));
      z->iname = iname;
#else
      /* Better keep the old name. Useful when updating on MSDOS a zip file
       * made on Unix.
       */
      free((zvoid *)iname);
      free((zvoid *)zname);
#endif /* ? FORCE_NEWNAME */
    }
    if (name == label) {
       label = z->name;
    }
  } else if (pcount == 0 || filter(undosm)) {

    /* Check that we are not adding the zip file to itself. This
     * catches cases like "zip -m foo ../dir/foo.zip".
     */
#ifndef CMS_MVS
/* Version of stat() for CMS/MVS isn't complete enough to see if       */
/* files match.  Just let ZIP.C compare the filenames.  That's good    */
/* enough for CMS anyway since there aren't paths to worry about.      */
    struct stat statb;

    if (zipstate == -1)
       zipstate = strcmp(zipfile, "-") != 0 &&
                   stat(zipfile, &zipstatb) == 0;

    if (zipstate == 1 && (statb = zipstatb, stat(name, &statb) == 0
      && zipstatb.st_mode  == statb.st_mode
      && zipstatb.st_ino   == statb.st_ino
      && zipstatb.st_dev   == statb.st_dev
      && zipstatb.st_uid   == statb.st_uid
      && zipstatb.st_gid   == statb.st_gid
      && zipstatb.st_size  == statb.st_size
      && zipstatb.st_mtime == statb.st_mtime
      && zipstatb.st_ctime == statb.st_ctime)) {
      /* Don't compare a_time since we are reading the file */
         if (verbose)
           fprintf(mesg, "file matches zip file -- skipping\n");
         if (undosm != iname)
           free((zvoid *)undosm);
         free((zvoid *)iname);
         free((zvoid *)zname);
         return ZE_OK;
    }
#endif  /* CMS_MVS */

    /* allocate space and add to list */
    if ((f = (struct flist far *)farmalloc(sizeof(struct flist))) == NULL ||
        (f->name = malloc(strlen(name) + 1 + PAD)) == NULL)
    {
      if (f != NULL)
        farfree((zvoid far *)f);
      if (undosm != iname)
        free((zvoid *)undosm);
      free((zvoid *)iname);
      free((zvoid *)zname);
      return ZE_MEM;
    }
    strcpy(f->name, name);
    f->iname = iname;
    f->zname = zname;
    f->dosflag = dosflag;
    *fnxt = f;
    f->lst = fnxt;
    f->nxt = NULL;
    fnxt = &f->nxt;
    fcount++;
    if (name == label) {
      label = f->name;
    }
  }
  if (undosm != iname)
    free((zvoid *)undosm);
  return ZE_OK;
}


#if !defined(OS2) && !defined(VMS)

time_t dos2unixtime(dostime)
ulg dostime;            /* DOS time to convert */
/* Return the Unix time_t value (GMT/UTC time) for the DOS format (local)
 * time dostime, where dostime is a four byte value (date in most significant
 * word, time in least significant word), see dostime() function.
 */
{
  struct tm *t;         /* argument for mktime() */
  ZCONST time_t clock = time(NULL);

  t = localtime(&clock);
  t->tm_isdst = -1;     /* let mktime() determine if DST is in effect */
  /* Convert DOS time to UNIX time_t format */
  t->tm_sec  = (((int)dostime) <<  1) & 0x3e;
  t->tm_min  = (((int)dostime) >>  5) & 0x3f;
  t->tm_hour = (((int)dostime) >> 11) & 0x1f;
  t->tm_mday = (int)(dostime >> 16) & 0x1f;
  t->tm_mon  = ((int)(dostime >> 21) & 0x0f) - 1;
  t->tm_year = ((int)(dostime >> 25) & 0x7f) + 80;

  return mktime(t);
}

#endif /* !OS2 && !VMS */


ulg dostime(y, n, d, h, m, s)
int y;                  /* year */
int n;                  /* month */
int d;                  /* day */
int h;                  /* hour */
int m;                  /* minute */
int s;                  /* second */
/* Convert the date y/n/d and time h:m:s to a four byte DOS date and
   time (date in high two bytes, time in low two bytes allowing magnitude
   comparison). */
{
  return y < 1980 ? dostime(1980, 1, 1, 0, 0, 0) :
        (((ulg)y - 1980) << 25) | ((ulg)n << 21) | ((ulg)d << 16) |
        ((ulg)h << 11) | ((ulg)m << 5) | ((ulg)s >> 1);
}


ulg unix2dostime(t)
time_t *t;             /* unix time to convert */
/* Return the Unix time t in DOS format, rounded up to the next two
   second boundary. */
{
  time_t t_even;
  struct tm *s;         /* result of localtime() */

  t_even = (*t + 1) & (~1);     /* Round up to even seconds. */
  s = localtime(&t_even);       /* Use local time since MSDOS does. */
  return dostime(s->tm_year + 1900, s->tm_mon + 1, s->tm_mday,
                 s->tm_hour, s->tm_min, s->tm_sec);
}

int issymlnk(a)
ulg a;                  /* Attributes returned by filetime() */
/* Return true if the attributes are those of a symbolic link */
{
#ifndef QDOS
#ifdef S_IFLNK
  return ((a >> 16) & S_IFMT) == S_IFLNK;
#else /* !S_IFLNK */
  return (int)a & 0;    /* avoid warning on unused parameter */
#endif /* ?S_IFLNK */
#else
  return 0;
#endif
}

#endif /* !UTIL */

int destroy(f)
char *f;                /* file to delete */
/* Delete the file *f, returning non-zero on failure. */
{
  return unlink(f);
}


int replace(d, s)
char *d, *s;            /* destination and source file names */
/* Replace file *d by file *s, removing the old *s.  Return an error code
   in the ZE_ class. This function need not preserve the file attributes,
   this will be done by setfileattr() later.
 */
{
  struct stat t;        /* results of stat() */
  int copy = 0;
  int d_exists;

#if defined(VMS) || defined(CMS_MVS)
  /* stat() is broken on VMS remote files (accessed through Decnet).
   * This patch allows creation of remote zip files, but is not sufficient
   * to update them or compress remote files */
  unlink(d);
#else /* !(VMS || CMS_MVS) */
  d_exists = (LSTAT(d, &t) == 0);
  if (d_exists)
  {
    /*
     * respect existing soft and hard links!
     */
    if (t.st_nlink > 1
# ifdef S_IFLNK
        || (t.st_mode & S_IFMT) == S_IFLNK
# endif
        )
       copy = 1;
    else if (unlink(d))
       return ZE_CREAT;                 /* Can't erase zip file--give up */
  }
#endif /* ?(VMS || CMS_MVS) */
  if (!copy) {
      if (rename(s, d)) {               /* Just move s on top of d */
          copy = 1;                     /* failed ? */
#if !defined(VMS) && !defined(ATARI) && !defined(AZTEC_C)
#if !defined(CMS_MVS) && !defined(RISCOS) && !defined(QDOS)
    /* For VMS, ATARI, AMIGA Aztec, VM_CMS, MVS, RISCOS,
       always assume that failure is EXDEV */
          if (errno != EXDEV
#  ifdef ENOTSAM
           && errno != ENOTSAM /* Used at least on Turbo C */
#  endif
              ) return ZE_CREAT;
#endif /* !CMS_MVS && !RISCOS */
#endif /* !VMS && !ATARI && !AZTEC_C */
      }
  }

  if (copy) {
    FILE *f, *g;      /* source and destination files */
    int r;            /* temporary variable */

#ifdef RISCOS
    if (SWI_OS_FSControl_26(s,d,0xA1)!=NULL) {
#endif

    if ((f = fopen(s, FOPR)) == NULL) {
      fprintf(stderr," replace: can't open %s\n", s);
      return ZE_TEMP;
    }
    if ((g = fopen(d, FOPW)) == NULL)
    {
      fclose(f);
      return ZE_CREAT;
    }
    r = fcopy(f, g, (ulg)-1L);
    fclose(f);
    if (fclose(g) || r != ZE_OK)
    {
      unlink(d);
      return r ? (r == ZE_TEMP ? ZE_WRITE : r) : ZE_WRITE;
    }
    unlink(s);
#ifdef RISCOS
    }
#endif
  }
  return ZE_OK;
}


int getfileattr(f)
char *f;                /* file path */
/* Return the file attributes for file f or 0 if failure */
{
  struct stat s;

  return SSTAT(f, &s) == 0 ? (int) s.st_mode : 0;
}


int setfileattr(f, a)
char *f;                /* file path */
int a;                  /* attributes returned by getfileattr() */
/* Give the file f the attributes a, return non-zero on failure */
{
#if defined(TOPS20) || defined (CMS_MVS)
  return 0;
#else
  return chmod(f, a);
#endif
}


char *tempname(zip)
char *zip;              /* path name of zip file to generate temp name for */

/* Return a temporary file name in its own malloc'ed space, using tempath. */
{
  char *t = zip;   /* malloc'ed space for name (use zip to avoid warning) */

#ifdef CMS_MVS
  if ((t = malloc(strlen(tempath)+L_tmpnam+2)) == NULL)
    return NULL;
  tmpnam(t);
#  ifdef VM_CMS
  /* Remove filemode and replace with tempath, if any. */
  /* Otherwise A-disk is used by default */
  *(strrchr(t, ' ')+1) = '\0';
  if (tempath!=NULL)
     strcat(t, tempath);
  return t;
#  else   /* !VM_CMS */
  /* For MVS */
  if (tempath != NULL)
  {
    if ((t = malloc(strlen(tempath)+L_tmpnam+2)) == NULL)
      return NULL;
    strcpy(t, tempath);
    if (t[strlen(t)]!='.')
      strcat(t, ".");
    strcat(t,tmpnam(NULL));
    return t;
#  endif  /* !VM_CMS */
#else /* !CMS_MVS */
#ifdef TANDEM
  char cur_subvol [FILENAME_MAX];
  char temp_subvol [FILENAME_MAX];
  char *zptr;
  char *ptr;
  char *cptr = &cur_subvol[0];
  char *tptr = &temp_subvol[0];
  short err;

  t = (char *)malloc(NAMELEN); /* malloc here as you cannot free */
                               /* tmpnam allocated storage later */

  zptr = strrchr(zip, TANDEM_DELIMITER);

  if (zptr != NULL) {
    /* ZIP file specifies a Subvol so make temp file there so it can just
       be renamed at end */

    *tptr = *cptr = '\0';
    strcat(cptr, getenv("DEFAULTS"));

    strncat(tptr, zip, _min(FILENAME_MAX, (zptr - zip)) ); /* temp subvol */
    strncat(t,zip, _min(NAMELEN, ((zptr - zip) + 1)) );    /* temp stem   */

    err = chvol(tptr);
    ptr = t + strlen(t);  /* point to end of stem */
    tmpnam(ptr);          /* Add filename part to temp subvol */
    err = chvol(cptr);
  }
  else
    t = tmpnam(t);

  return t;

#else /* !CMS_MVS && !TANDEM */
/*
 * Do something with TMPDIR, TMP, TEMP ????
 */
  if (tempath != NULL)
  {
    if ((t = malloc(strlen(tempath)+12)) == NULL)
      return NULL;
    strcpy(t, tempath);
#if (!defined(VMS) && !defined(TOPS20))
#  ifdef AMIGA
    {
          char c = t[strlen(t)-1];
          if (c != '/' && c != ':')
            strcat(t, "/");
    }
#  else /* !AMIGA */
#    ifdef RISCOS
       if (t[strlen(t)-1] != '.')
         strcat(t, ".");
#    else /* !RISCOS */
#      ifdef QDOS
         if (t[strlen(t)-1] != '_')
           strcat(t, "_");
#      else
       if (t[strlen(t)-1] != '/')
         strcat(t, "/");
#      endif /* QDOS */
#    endif /* RISCOS */
#  endif  /* ?AMIGA */
#endif /* !VMS && !TOPS20 */
  }
  else
  {
    if ((t = malloc(12)) == NULL)
      return NULL;
    *t = 0;
  }
#ifdef NO_MKTEMP
  {
    char *p = t + strlen(t);
    sprintf(p, "%08lx", (ulg)time(NULL));
    return t;
  }
#else
  strcat(t, "ziXXXXXX"); /* must use lowercase for Linux dos file system */
  return mktemp(t);
#endif /* NO_MKTEMP */
#endif /* TANDEM */
#endif /* CMS_MVS */
}


int fcopy(f, g, n)
FILE *f, *g;            /* source and destination files */
ulg n;                  /* number of bytes to copy or -1 for all */
/* Copy n bytes from file *f to file *g, or until EOF if n == -1.  Return
   an error code in the ZE_ class. */
{
  char *b;              /* malloc'ed buffer for copying */
  extent k;             /* result of fread() */
  ulg m;                /* bytes copied so far */

  if ((b = malloc(CBSZ)) == NULL)
    return ZE_MEM;
  m = 0;
  while (n == (ulg)(-1L) || m < n)
  {
    if ((k = fread(b, 1, n == (ulg)(-1) ?
                   CBSZ : (n - m < CBSZ ? (extent)(n - m) : CBSZ), f)) == 0)
      if (ferror(f))
      {
        free((zvoid *)b);
        return ZE_READ;
      }
      else
        break;
    if (fwrite(b, 1, k, g) != k)
    {
      free((zvoid *)b);
      fprintf(stderr," fcopy: write error\n");
      return ZE_TEMP;
    }
    m += k;
  }
  free((zvoid *)b);
  return ZE_OK;
}

#ifdef NO_RENAME
int rename(from, to)
ZCONST char *from;
ZCONST char *to;
{
    unlink(to);
    if (link(from, to) == -1)
        return -1;
    if (unlink(from) == -1)
        return -1;
    return 0;
}

#endif /* NO_RENAME */


#ifdef ZMEM

/************************/
/*  Function memset()  */
/************************/

/*
 * memset - for systems without it
 *  bill davidsen - March 1990
 */

char *
memset(buf, init, len)
register char *buf;     /* buffer loc */
register int init;      /* initializer */
register unsigned int len;   /* length of the buffer */
{
    char *start;

    start = buf;
    while (len--) *(buf++) = init;
    return(start);
}


/************************/
/*  Function memcpy()  */
/************************/

char *
memcpy(dst,src,len)           /* v2.0f */
register char *dst, *src;
register unsigned int len;
{
    char *start;

    start = dst;
    while (len--)
        *dst++ = *src++;
    return(start);
}


/************************/
/*  Function memcmp()  */
/************************/

int
memcmp(b1,b2,len)                     /* jpd@usl.edu -- 11/16/90 */
register char *b1, *b2;
register unsigned int len;
{

    if (len) do {             /* examine each byte (if any) */
      if (*b1++ != *b2++)
        return (*((uch *)b1-1) - *((uch *)b2-1));  /* exit when miscompare */
       } while (--len);

    return(0);        /* no miscompares, yield 0 result */
}

#endif  /* ZMEM */
