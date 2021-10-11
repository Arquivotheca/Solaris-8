/* general.c -- Stuff that is used by all files. */

/* Copyright (C) 1987, 1988, 1989, 1990, 1991, 1992
   Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   Bash is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with Bash; see the file COPYING.  If not, write to the Free Software
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

/*
 * mike_s@Sun.COM 8/5/1999 - cast real_limit to 'int' in comparison to avoid
 * compiler warning (though the test is useless on machines where
 * RLIMTYPE is unsigned).
 */

#include "config.h"

#include "bashtypes.h"
#ifndef _MINIX
#  include <sys/param.h>
#endif
#include "posixstat.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "filecntl.h"
#include "bashansi.h"
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "shell.h"
#include <tilde/tilde.h>

#if defined (TIME_WITH_SYS_TIME)
#  include <sys/time.h>
#  include <time.h>
#else
#  if defined (HAVE_SYS_TIME_H)
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif

#include <sys/times.h>
#include "maxpath.h"

#if !defined (errno)
extern int errno;
#endif /* !errno */

#ifndef to_upper
#  define to_upper(c) (islower(c) ? toupper(c) : (c))
#  define to_lower(c) (isupper(c) ? tolower(c) : (c))
#endif

extern int interrupt_immediately;
extern int interactive_comments;

/* A standard error message to use when getcwd() returns NULL. */
char *bash_getcwd_errstr = "getcwd: cannot access parent directories";

/* Do whatever is necessary to initialize `Posix mode'. */
void
posix_initialize (on)
     int on;
{
  interactive_comments = on != 0;
}

/* **************************************************************** */
/*								    */
/*  Functions to convert to and from and display non-standard types */
/*								    */
/* **************************************************************** */

#if defined (RLIMTYPE)
RLIMTYPE
string_to_rlimtype (s)
     char *s;
{
  RLIMTYPE ret;
  int neg;

  ret = 0;
  neg = 0;
  while (s && *s && whitespace (*s))
    s++;
  if (*s == '-' || *s == '+')
    {
      neg = *s == '-';
      s++;
    }
  for ( ; s && *s && digit (*s); s++)
    ret = (ret * 10) + digit_value (*s);
  return (neg ? -ret : ret);
}

void
print_rlimtype (n, addnl)
     RLIMTYPE n;
     int addnl;
{
  char s[sizeof (RLIMTYPE) * 3 + 1];
  int len;

  if (n == 0)
    {
      printf ("0%s", addnl ? "\n" : "");
      return;
    }

  if ((int)n < 0)
    {
      putchar ('-');
      n = -n;
    }

  len = sizeof (RLIMTYPE) * 3 + 1;
  s[--len] = '\0';
  for ( ; n != 0; n /= 10)
    s[--len] = n % 10 + '0';
  printf ("%s%s", s + len, addnl ? "\n" : "");
}
#endif /* RLIMTYPE */

#if defined (HAVE_TIMEVAL)
/* Convert a pointer to a struct timeval to seconds and thousandths of a
   second, returning the values in *SP and *SFP, respectively.  This does
   rounding on the fractional part, not just truncation to three places. */
void
timeval_to_secs (tvp, sp, sfp)
     struct timeval *tvp;
     long *sp;
     int *sfp;
{
  int rest;

  *sp = tvp->tv_sec;

  *sfp = tvp->tv_usec % 1000000;	/* pretty much a no-op */
  rest = *sfp % 1000;
  *sfp = (*sfp * 1000) / 1000000;
  if (rest >= 500)
    *sfp += 1;

  /* Sanity check */
  if (*sfp >= 1000)
    {
      *sp += 1;
      *sfp -= 1000;
    }
}
  
/* Print the contents of a struct timeval * in a standard way to stdio
   stream FP.  */
void
print_timeval (fp, tvp)
     FILE *fp;
     struct timeval *tvp;
{
  int minutes, seconds_fraction;
  long seconds;

  timeval_to_secs (tvp, &seconds, &seconds_fraction);

  minutes = seconds / 60;
  seconds %= 60;

  fprintf (fp, "%0dm%0ld.%03ds",  minutes, seconds, seconds_fraction);
}
#endif /* HAVE_TIMEVAL */

#if defined (HAVE_TIMES)
void
clock_t_to_secs (t, sp, sfp)
     clock_t t;
     long *sp;
     int *sfp;
{
  static long clk_tck = 0;

  if (clk_tck == 0)
    clk_tck = get_clk_tck ();

  *sfp = t % clk_tck;
  *sfp = (*sfp * 1000) / clk_tck;

  *sp = t / clk_tck;

  /* Sanity check */
  if (*sfp >= 1000)
    {
      *sp += 1;
      *sfp -= 1000;
    }
}

/* Print the time defined by a time_t (returned by the `times' and `time'
   system calls) in a standard way to stdion stream FP.  This is scaled in
   terms of HZ, which is what is returned by the `times' call. */
void
print_time_in_hz (fp, t)
     FILE *fp;
     clock_t t;
{
  int minutes, seconds_fraction;
  long seconds;

  clock_t_to_secs (t, &seconds, &seconds_fraction);

  minutes = seconds / 60;
  seconds %= 60;

  fprintf (fp, "%0dm%0ld.%03ds",  minutes, seconds, seconds_fraction);
}
#endif /* HAVE_TIMES */

/* **************************************************************** */
/*								    */
/*		       Input Validation Functions		    */
/*								    */
/* **************************************************************** */

/* Return non-zero if all of the characters in STRING are digits. */
int
all_digits (string)
     char *string;
{
  while (*string)
    {
      if (!digit (*string))
	return (0);
      else
	string++;
    }
  return (1);
}

/* Return non-zero if the characters pointed to by STRING constitute a
   valid number.  Stuff the converted number into RESULT if RESULT is
   a non-null pointer to a long. */
int
legal_number (string, result)
     char *string;
     long *result;
{
  long value;
  char *ep;

  if (result)
    *result = 0;

  value = strtol (string, &ep, 10);

  /* If *string is not '\0' but *ep is '\0' on return, the entire string
     is valid. */
  if (string && *string && *ep == '\0')
    {
      if (result)
	*result = value;
      /* The SunOS4 implementation of strtol() will happily ignore
	 overflow conditions, so this cannot do overflow correctly
	 on those systems. */
      return 1;
    }
    
  return (0);
}

/* Return 1 if this token is a legal shell `identifier'; that is, it consists
   solely of letters, digits, and underscores, and does not begin with a
   digit. */
int
legal_identifier (name)
     char *name;
{
  register char *s;

  if (!name || !*name || (legal_variable_starter (*name) == 0))
    return (0);

  for (s = name + 1; *s; s++)
    {
      if (legal_variable_char (*s) == 0)
        return (0);
    }
  return (1);
}

/* Make sure that WORD is a valid shell identifier, i.e.
   does not contain a dollar sign, nor is quoted in any way.  Nor
   does it consist of all digits.  If CHECK_WORD is non-zero,
   the word is checked to ensure that it consists of only letters,
   digits, and underscores. */
int
check_identifier (word, check_word)
     WORD_DESC *word;
     int check_word;
{
  if ((word->flags & (W_HASDOLLAR|W_QUOTED)) || all_digits (word->word))
    {
      internal_error ("`%s': not a valid identifier", word->word);
      return (0);
    }
  else if (check_word && legal_identifier (word->word) == 0)
    {
      internal_error ("`%s': not a valid identifier", word->word);
      return (0);
    }
  else
    return (1);
}

/* **************************************************************** */
/*								    */
/*	     Functions to manage files and file descriptors	    */
/*								    */
/* **************************************************************** */

/* A function to unset no-delay mode on a file descriptor.  Used in shell.c
   to unset it on the fd passed as stdin.  Should be called on stdin if
   readline gets an EAGAIN or EWOULDBLOCK when trying to read input. */

#if !defined (O_NDELAY)
#  if defined (FNDELAY)
#    define O_NDELAY FNDELAY
#  endif
#endif /* O_NDELAY */

/* Make sure no-delay mode is not set on file descriptor FD. */
void
unset_nodelay_mode (fd)
     int fd;
{
  int flags, set;

  if ((flags = fcntl (fd, F_GETFL, 0)) < 0)
    return;

  set = 0;

  /* This is defined to O_NDELAY in filecntl.h if O_NONBLOCK is not present
     and O_NDELAY is defined. */
  if (flags & O_NONBLOCK)
    {
      flags &= ~O_NONBLOCK;
      set++;
    }

  if (set)
    fcntl (fd, F_SETFL, flags);
}

/* There is a bug in the NeXT 2.1 rlogind that causes opens
   of /dev/tty to fail. */

#if defined (__BEOS__)
/* On BeOS, opening in non-blocking mode exposes a bug in BeOS, so turn it
   into a no-op.  This should probably go away in the future. */
#  undef O_NONBLOCK
#  define O_NONBLOCK 0
#endif /* __BEOS__ */

void
check_dev_tty ()
{
  int tty_fd;
  char *tty;

  tty_fd = open ("/dev/tty", O_RDWR|O_NONBLOCK);

  if (tty_fd < 0)
    {
      tty = (char *)ttyname (fileno (stdin));
      if (tty == 0)
	return;
      tty_fd = open (tty, O_RDWR|O_NONBLOCK);
    }
  close (tty_fd);
}

/* Return 1 if PATH1 and PATH2 are the same file.  This is kind of
   expensive.  If non-NULL STP1 and STP2 point to stat structures
   corresponding to PATH1 and PATH2, respectively. */
int
same_file (path1, path2, stp1, stp2)
     char *path1, *path2;
     struct stat *stp1, *stp2;
{
  struct stat st1, st2;

  if (stp1 == NULL)
    {
      if (stat (path1, &st1) != 0)
	return (0);
      stp1 = &st1;
    }

  if (stp2 == NULL)
    {
      if (stat (path2, &st2) != 0)
	return (0);
      stp2 = &st2;
    }

  return ((stp1->st_dev == stp2->st_dev) && (stp1->st_ino == stp2->st_ino));
}

/* Move FD to a number close to the maximum number of file descriptors
   allowed in the shell process, to avoid the user stepping on it with
   redirection and causing us extra work.  If CHECK_NEW is non-zero,
   we check whether or not the file descriptors are in use before
   duplicating FD onto them.  MAXFD says where to start checking the
   file descriptors.  If it's less than 20, we get the maximum value
   available from getdtablesize(2). */
int
move_to_high_fd (fd, check_new, maxfd)
     int fd, check_new, maxfd;
{
  int script_fd, nfds, ignore;

  if (maxfd < 20)
    {
      nfds = getdtablesize ();
      if (nfds <= 0)
	nfds = 20;
      if (nfds > 256)
	nfds = 256;
    }
  else
    nfds = maxfd;

  for (nfds--; check_new && nfds > 3; nfds--)
    if (fcntl (nfds, F_GETFD, &ignore) == -1)
      break;

  if (nfds && fd != nfds && (script_fd = dup2 (fd, nfds)) != -1)
    {
      if (check_new == 0 || fd != fileno (stderr))	/* don't close stderr */
	close (fd);
      return (script_fd);
    }

  return (fd);
}
 
/* Return non-zero if the characters from SAMPLE are not all valid
   characters to be found in the first line of a shell script.  We
   check up to the first newline, or SAMPLE_LEN, whichever comes first.
   All of the characters must be printable or whitespace. */

#if !defined (isspace)
#define isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\f')
#endif

#if !defined (isprint)
#define isprint(c) (isletter(c) || digit(c) || ispunct(c))
#endif

int
check_binary_file (sample, sample_len)
     unsigned char *sample;
     int sample_len;
{
  register int i;

  for (i = 0; i < sample_len; i++)
    {
      if (sample[i] == '\n')
	return (0);

      if (isspace (sample[i]) == 0 && isprint (sample[i]) == 0)
	return (1);
    }

  return (0);
}

/* **************************************************************** */
/*								    */
/*		    Functions to manipulate pathnames		    */
/*								    */
/* **************************************************************** */

/* Return 1 if PATH corresponds to a directory. */
static int
canon_stat (path)
     char *path;
{
  int l;
  char *s;
  struct stat sb;

  l = strlen (path);
  s = xmalloc (l + 3);
  strcpy (s, path);
  s[l] = '/';
  s[l+1] = '.';
  s[l+2] = '\0';
  l = stat (s, &sb) == 0 && S_ISDIR (sb.st_mode);
  free (s);
  return l;
}

/* Canonicalize PATH, and return a new path.  The new path differs from PATH
   in that:
	Multple `/'s are collapsed to a single `/'.
	Leading `./'s and trailing `/.'s are removed.
	Trailing `/'s are removed.
	Non-leading `../'s and trailing `..'s are handled by removing
	portions of the path. */
char *
canonicalize_pathname (path)
     char *path;
{
  register int i, start;
  char stub_char;
  char *result;

  /* The result cannot be larger than the input PATH. */
  result = savestring (path);

  stub_char = (*path == '/') ? '/' : '.';

  /* Walk along RESULT looking for things to compact. */
  i = 0;
  while (1)
    {
      if (!result[i])
	break;

      while (result[i] && result[i] != '/')
	i++;

      start = i++;

      /* If we didn't find any slashes, then there is nothing left to do. */
      if (!result[start])
	break;

      /* Handle multiple `/'s in a row. */
      while (result[i] == '/')
	i++;

#if 0
      if ((start + 1) != i)
#else
      /* Leave a leading `//' alone, as POSIX requires. */
      if ((start + 1) != i && (start != 0 || i != 2))
#endif
	{
	  strcpy (result + start + 1, result + i);
	  i = start + 1;
	  /* Make sure that what we have so far corresponds to a directory.
	     If it does not, just punt. */
	  if (*result)
	    {
	      char c;
	      c = result[start];
	      result[start] = '\0';
	      if (canon_stat (result) == 0)
		{
		  free (result);
		  return ((char *)NULL);
		}
	      result[start] = c;
	    }
	}
#if 0
      /* Handle backslash-quoted `/'. */
      if (start > 0 && result[start - 1] == '\\')
	continue;
#endif

      /* Check for trailing `/'. */
      if (start && !result[i])
	{
	zero_last:
	  result[--i] = '\0';
	  break;
	}

      /* Check for `../', `./' or trailing `.' by itself. */
      if (result[i] == '.')
	{
	  /* Handle trailing `.' by itself. */
	  if (!result[i + 1])
	    goto zero_last;

	  /* Handle `./'. */
	  if (result[i + 1] == '/')
	    {
	      strcpy (result + i, result + i + 1);
	      i = (start < 0) ? 0 : start;
	      continue;
	    }

	  /* Handle `../' or trailing `..' by itself. */
	  if (result[i + 1] == '.' &&
	      (result[i + 2] == '/' || !result[i + 2]))
	    {
	      /* Make sure that the last component corresponds to a directory
		 before blindly chopping it off. */
	      if (i)
		{
		  result[i] = '\0';
		  if (canon_stat (result) == 0)
		    {
		      free (result);
		      return ((char *)NULL);
		    }
		  result[i] = '.';
		}
	      while (--start > -1 && result[start] != '/');
	      strcpy (result + start + 1, result + i + 2);
#if 0	/* Unnecessary */
	      if (*result && canon_stat (result) == 0)
		{
		  free (result);
		  return ((char *)NULL);
		}
#endif
	      i = (start < 0) ? 0 : start;
	      continue;
	    }
	}
    }

  if (!*result)
    {
      *result = stub_char;
      result[1] = '\0';
    }

  /* If the result starts with `//', but the original path does not, we
     can turn the // into /. */
  if ((result[0] == '/' && result[1] == '/' && result[2] != '/') &&
      (path[0] != '/' || path[1] != '/' || path[2] == '/'))
    {
      char *r2;
      if (result[2] == '\0')	/* short-circuit for bare `//' */
	result[1] = '\0';
      else
	{
	  r2 = savestring (result + 1);
	  free (result);
	  result = r2;
	}
    }

  return (result);
}

/* Turn STRING (a pathname) into an absolute pathname, assuming that
   DOT_PATH contains the symbolic location of `.'.  This always
   returns a new string, even if STRING was an absolute pathname to
   begin with. */
char *
make_absolute (string, dot_path)
     char *string, *dot_path;
{
  char *result;
  int result_len;

  if (dot_path == 0 || *string == '/')
    result = savestring (string);
  else
    {
      if (dot_path[0])
	{
	  result_len = strlen (dot_path);
	  result = xmalloc (2 + result_len + strlen (string));
	  strcpy (result, dot_path);
	  if (result[result_len - 1] != '/')
	    {
	      result[result_len++] = '/';
	      result[result_len] = '\0';
	    }
	}
      else
	{
	  result = xmalloc (3 + strlen (string));
	  result[0] = '.'; result[1] = '/'; result[2] = '\0';
	  result_len = 2;
	}

      strcpy (result + result_len, string);
    }

  return (result);
}

/* Return 1 if STRING contains an absolute pathname, else 0. */
int
absolute_pathname (string)
     char *string;
{
  if (!string || !*string)
    return (0);

  if (*string == '/')
    return (1);

  if (*string++ == '.')
    {
      if (!*string || *string == '/' ||
	   (*string == '.' && (string[1] == '\0' || string[1] == '/')))
	return (1);
    }
  return (0);
}

/* Return 1 if STRING is an absolute program name; it is absolute if it
   contains any slashes.  This is used to decide whether or not to look
   up through $PATH. */
int
absolute_program (string)
     char *string;
{
  return ((char *)strchr (string, '/') != (char *)NULL);
}

/* Return the `basename' of the pathname in STRING (the stuff after the
   last '/').  If STRING is not a full pathname, simply return it. */
char *
base_pathname (string)
     char *string;
{
  char *p;

  if (!absolute_pathname (string))
    return (string);

  p = (char *)strrchr (string, '/');
  return (p ? ++p : string);
}

/* Return the full pathname of FILE.  Easy.  Filenames that begin
   with a '/' are returned as themselves.  Other filenames have
   the current working directory prepended.  A new string is
   returned in either case. */
char *
full_pathname (file)
     char *file;
{
  char *disposer;
  char *current_dir;
  int dlen;

  file = (*file == '~') ? bash_tilde_expand (file) : savestring (file);

  if ((*file == '/') && absolute_pathname (file))
    return (file);

  disposer = file;

  /* XXX - this should probably be just PATH_MAX or PATH_MAX + 1 */
  current_dir = xmalloc (2 + PATH_MAX + strlen (file));
  if (getcwd (current_dir, PATH_MAX) == 0)
    {
      sys_error (bash_getcwd_errstr);
      free (disposer);
      free (current_dir);
      return ((char *)NULL);
    }
  dlen = strlen (current_dir);
  if (current_dir[0] == '/' && dlen > 1)
    current_dir[dlen++] = '/';

  /* Turn /foo/./bar into /foo/bar. */
  if (file[0] == '.' && file[1] == '/')
    file += 2;

  strcpy (current_dir + dlen, file);
  free (disposer);
  return (current_dir);
}

/* A slightly related function.  Get the prettiest name of this
   directory possible. */
static char tdir[PATH_MAX];

/* Return a pretty pathname.  If the first part of the pathname is
   the same as $HOME, then replace that with `~'.  */
char *
polite_directory_format (name)
     char *name;
{
  char *home;
  int l;

  home = get_string_value ("HOME");
  l = home ? strlen (home) : 0;
  if (l > 1 && strncmp (home, name, l) == 0 && (!name[l] || name[l] == '/'))
    {
      strncpy (tdir + 1, name + l, sizeof(tdir) - 2);
      tdir[0] = '~';
      tdir[sizeof(tdir) - 1] = '\0';
      return (tdir);
    }
  else
    return (name);
}

/* Given a string containing units of information separated by colons,
   return the next one pointed to by (P_INDEX), or NULL if there are no more.
   Advance (P_INDEX) to the character after the colon. */
char *
extract_colon_unit (string, p_index)
     char *string;
     int *p_index;
{
  int i, start, len;
  char *value;

  if (string == 0)
    return (string);

  len = strlen (string);
  if (*p_index >= len)
    return ((char *)NULL);

  i = *p_index;

  /* Each call to this routine leaves the index pointing at a colon if
     there is more to the path.  If I is > 0, then increment past the
     `:'.  If I is 0, then the path has a leading colon.  Trailing colons
     are handled OK by the `else' part of the if statement; an empty
     string is returned in that case. */
  if (i && string[i] == ':')
    i++;

  for (start = i; string[i] && string[i] != ':'; i++)
    ;

  *p_index = i;

  if (i == start)
    {
      if (string[i])
	(*p_index)++;
      /* Return "" in the case of a trailing `:'. */
      value = xmalloc (1);
      value[0] = '\0';
    }
  else
    {
      len = i - start;
      value = xmalloc (1 + len);
      strncpy (value, string + start, len);
      value [len] = '\0';
    }

  return (value);
}

/* **************************************************************** */
/*								    */
/*		    Tilde Initialization and Expansion		    */
/*								    */
/* **************************************************************** */

#if defined (PUSHD_AND_POPD)
extern char *get_dirstack_from_string __P((char *));
#endif

/* If tilde_expand hasn't been able to expand the text, perhaps it
   is a special shell expansion.  This function is installed as the
   tilde_expansion_preexpansion_hook.  It knows how to expand ~- and ~+.
   If PUSHD_AND_POPD is defined, ~[+-]N expands to directories from the
   directory stack. */
static char *
bash_special_tilde_expansions (text)
     char *text;
{
  char *result;

  result = (char *)NULL;

  if (text[0] == '+' && text[1] == '\0')
    result = get_string_value ("PWD");
  else if (text[0] == '-' && text[1] == '\0')
    result = get_string_value ("OLDPWD");
#if defined (PUSHD_AND_POPD)
  else if (isdigit (*text) || ((*text == '+' || *text == '-') && isdigit (text[1])))
    result = get_dirstack_from_string (text);
#endif

  return (result ? savestring (result) : (char *)NULL);
}

/* Initialize the tilde expander.  In Bash, we handle `~-' and `~+', as
   well as handling special tilde prefixes; `:~" and `=~' are indications
   that we should do tilde expansion. */
void
tilde_initialize ()
{
  static int times_called = 0;

  /* Tell the tilde expander that we want a crack first. */
  tilde_expansion_preexpansion_hook = (CPFunction *)bash_special_tilde_expansions;

  /* Tell the tilde expander about special strings which start a tilde
     expansion, and the special strings that end one.  Only do this once.
     tilde_initialize () is called from within bashline_reinitialize (). */
  if (times_called++ == 0)
    {
      tilde_additional_prefixes = (char **)xmalloc (3 * sizeof (char *));
      tilde_additional_prefixes[0] = "=~";
      tilde_additional_prefixes[1] = ":~";
      tilde_additional_prefixes[2] = (char *)NULL;

      tilde_additional_suffixes = (char **)xmalloc (3 * sizeof (char *));
      tilde_additional_suffixes[0] = ":";
      tilde_additional_suffixes[1] = "=~";
      tilde_additional_suffixes[2] = (char *)NULL;
    }
}

char *
bash_tilde_expand (s)
     char *s;
{
  int old_immed;
  char *ret;

  old_immed = interrupt_immediately;
  interrupt_immediately = 1;
  ret = tilde_expand (s);
  interrupt_immediately = old_immed;
  return (ret);
}

/* **************************************************************** */
/*								    */
/*	  Functions to manipulate and search the group list	    */
/*								    */
/* **************************************************************** */

static int ngroups, maxgroups;

/* The set of groups that this user is a member of. */
static GETGROUPS_T *group_array = (GETGROUPS_T *)NULL;

#if !defined (NOGROUP)
#  define NOGROUP (gid_t) -1
#endif

#if defined (HAVE_SYSCONF) && defined (_SC_NGROUPS_MAX)
#  define getmaxgroups() sysconf(_SC_NGROUPS_MAX)
#else
#  if defined (NGROUPS_MAX)
#    define getmaxgroups() NGROUPS_MAX
#  else /* !NGROUPS_MAX */
#    if defined (NGROUPS)
#      define getmaxgroups() NGROUPS
#    else /* !NGROUPS */
#      define getmaxgroups() 64
#    endif /* !NGROUPS */
#  endif /* !NGROUPS_MAX */
#endif /* !HAVE_SYSCONF || !SC_NGROUPS_MAX */

static void
initialize_group_array ()
{
  register int i;

  if (maxgroups == 0)
    maxgroups = getmaxgroups ();

  ngroups = 0;
  group_array = (GETGROUPS_T *)xrealloc (group_array, maxgroups * sizeof (GETGROUPS_T));

#if defined (HAVE_GETGROUPS)
  ngroups = getgroups (maxgroups, group_array);
#endif

  /* If getgroups returns nothing, or the OS does not support getgroups(),
     make sure the groups array includes at least the current gid. */
  if (ngroups == 0)
    {
      group_array[0] = current_user.gid;
      ngroups = 1;
    }

  /* If the primary group is not in the groups array, add it as group_array[0]
     and shuffle everything else up 1, if there's room. */
  for (i = 0; i < ngroups; i++)
    if (current_user.gid == (gid_t)group_array[i])
      break;
  if (i == ngroups && ngroups < maxgroups)
    {
      for (i = ngroups; i > 0; i--)
        group_array[i] = group_array[i - 1];
      group_array[0] = current_user.gid;
      ngroups++;
    }

  /* If the primary group is not group_array[0], swap group_array[0] and
     whatever the current group is.  The vast majority of systems should
     not need this; a notable exception is Linux. */
  if (group_array[0] != current_user.gid)
    {
      for (i = 0; i < ngroups; i++)
        if (group_array[i] == current_user.gid)
          break;
      if (i < ngroups)
	{
	  group_array[i] = group_array[0];
	  group_array[0] = current_user.gid;
	}
    }
}

/* Return non-zero if GID is one that we have in our groups list. */
int
#if defined (__STDC__) || defined ( _MINIX)
group_member (gid_t gid)
#else
group_member (gid)
     gid_t gid;
#endif /* !__STDC__ && !_MINIX */
{
#if defined (HAVE_GETGROUPS)
  register int i;
#endif

  /* Short-circuit if possible, maybe saving a call to getgroups(). */
  if (gid == current_user.gid || gid == current_user.egid)
    return (1);

#if defined (HAVE_GETGROUPS)
  if (ngroups == 0)
    initialize_group_array ();

  /* In case of error, the user loses. */
  if (ngroups <= 0)
    return (0);

  /* Search through the list looking for GID. */
  for (i = 0; i < ngroups; i++)
    if (gid == (gid_t)group_array[i])
      return (1);
#endif

  return (0);
}

char **
get_group_list (ngp)
     int *ngp;
{
  static char **group_vector = (char **)NULL;
  register int i;
  char *nbuf;

  if (group_vector)
    {
      if (ngp)
	*ngp = ngroups;
      return group_vector;
    }

  if (ngroups == 0)
    initialize_group_array ();

  if (ngroups <= 0)
    {
      if (ngp)
	*ngp = 0;
      return (char **)NULL;
    }

  group_vector = (char **)xmalloc (ngroups * sizeof (char *));
  for (i = 0; i < ngroups; i++)
    {
      nbuf = itos ((int)group_array[i]);
      group_vector[i] = nbuf;
    }

  if (ngp)
    *ngp = ngroups;
  return group_vector;
}

int *
get_group_array (ngp)
     int *ngp;
{
  int i;
  static int *group_iarray = (int *)NULL;

  if (group_iarray)
    {
      if (ngp)
	*ngp = ngroups;
      return (group_iarray);
    }

  if (ngroups == 0)
    initialize_group_array ();    

  if (ngroups <= 0)
    {
      if (ngp)
	*ngp = 0;
      return (int *)NULL;
    }

  group_iarray = (int *)xmalloc (ngroups * sizeof (int));
  for (i = 0; i < ngroups; i++)
    group_iarray[i] = (int)group_array[i];

  if (ngp)
    *ngp = ngroups;
  return group_iarray;
}
