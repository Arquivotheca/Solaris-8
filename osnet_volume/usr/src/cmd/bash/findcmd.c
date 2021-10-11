/* findcmd.c -- Functions to search for commands by name. */

/* Copyright (C) 1997 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   Bash is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Bash; see the file COPYING.  If not, write to the
   Free Software Foundation Inc.,
   59 Temple Place, Suite 330, Boston, MA 02111-1307, USA. */

#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include "bashtypes.h"
#ifndef _MINIX
#  include <sys/file.h>
#endif
#include "filecntl.h"
#include "posixstat.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_LIMITS_H)
#  include <limits.h>
#endif

#include "bashansi.h"

#include "memalloc.h"
#include "shell.h"
#include "flags.h"
#include "hashlib.h"
#include "pathexp.h"
#include "hashcmd.h"

extern int posixly_correct;

/* Static functions defined and used in this file. */
static char *find_user_command_internal (), *find_user_command_in_path ();
static char *find_in_path_element (), *find_absolute_program ();

/* The file name which we would try to execute, except that it isn't
   possible to execute it.  This is the first file that matches the
   name that we are looking for while we are searching $PATH for a
   suitable one to execute.  If we cannot find a suitable executable
   file, then we use this one. */
static char *file_to_lose_on;

/* Non-zero if we should stat every command found in the hash table to
   make sure it still exists. */
int check_hashed_filenames;

/* DOT_FOUND_IN_SEARCH becomes non-zero when find_user_command ()
   encounters a `.' as the directory pathname while scanning the
   list of possible pathnames; i.e., if `.' comes before the directory
   containing the file of interest. */
int dot_found_in_search = 0;

#define u_mode_bits(x) (((x) & 0000700) >> 6)
#define g_mode_bits(x) (((x) & 0000070) >> 3)
#define o_mode_bits(x) (((x) & 0000007) >> 0)
#define X_BIT(x) ((x) & 1)

/* Return some flags based on information about this file.
   The EXISTS bit is non-zero if the file is found.
   The EXECABLE bit is non-zero the file is executble.
   Zero is returned if the file is not found. */
int
file_status (name)
     char *name;
{
  struct stat finfo;

  /* Determine whether this file exists or not. */
  if (stat (name, &finfo) < 0)
    return (0);

  /* If the file is a directory, then it is not "executable" in the
     sense of the shell. */
  if (S_ISDIR (finfo.st_mode))
    return (FS_EXISTS|FS_DIRECTORY);

#if defined (AFS)
  /* We have to use access(2) to determine access because AFS does not
     support Unix file system semantics.  This may produce wrong
     answers for non-AFS files when ruid != euid.  I hate AFS. */
  if (access (name, X_OK) == 0)
    return (FS_EXISTS | FS_EXECABLE);
  else
    return (FS_EXISTS);
#else /* !AFS */

  /* Find out if the file is actually executable.  By definition, the
     only other criteria is that the file has an execute bit set that
     we can use. */

  /* Root only requires execute permission for any of owner, group or
     others to be able to exec a file. */
  if (current_user.euid == (uid_t)0)
    {
      int bits;

      bits = (u_mode_bits (finfo.st_mode) |
	      g_mode_bits (finfo.st_mode) |
	      o_mode_bits (finfo.st_mode));

      if (X_BIT (bits))
	return (FS_EXISTS | FS_EXECABLE);
    }

  /* If we are the owner of the file, the owner execute bit applies. */
  if (current_user.euid == finfo.st_uid && X_BIT (u_mode_bits (finfo.st_mode)))
    return (FS_EXISTS | FS_EXECABLE);

  /* If we are in the owning group, the group permissions apply. */
  if (group_member (finfo.st_gid) && X_BIT (g_mode_bits (finfo.st_mode)))
    return (FS_EXISTS | FS_EXECABLE);

  /* If `others' have execute permission to the file, then so do we,
     since we are also `others'. */
  if (X_BIT (o_mode_bits (finfo.st_mode)))
    return (FS_EXISTS | FS_EXECABLE);

  return (FS_EXISTS);
#endif /* !AFS */
}

/* Return non-zero if FILE exists and is executable.
   Note that this function is the definition of what an
   executable file is; do not change this unless YOU know
   what an executable file is. */
int
executable_file (file)
     char *file;
{
  int s;

  s = file_status (file);
  return ((s & FS_EXECABLE) && ((s & FS_DIRECTORY) == 0));
}

int
is_directory (file)
     char *file;
{
  return (file_status (file) & FS_DIRECTORY);
}

/* Locate the executable file referenced by NAME, searching along
   the contents of the shell PATH variable.  Return a new string
   which is the full pathname to the file, or NULL if the file
   couldn't be found.  If a file is found that isn't executable,
   and that is the only match, then return that. */
char *
find_user_command (name)
     char *name;
{
  return (find_user_command_internal (name, FS_EXEC_PREFERRED|FS_NODIRS));
}

/* Locate the file referenced by NAME, searching along the contents
   of the shell PATH variable.  Return a new string which is the full
   pathname to the file, or NULL if the file couldn't be found.  This
   returns the first file found. */
char *
find_path_file (name)
     char *name;
{
  return (find_user_command_internal (name, FS_EXISTS));
}

static char *
_find_user_command_internal (name, flags)
     char *name;
     int flags;
{
  char *path_list, *cmd;
  SHELL_VAR *var;

  /* Search for the value of PATH in both the temporary environment, and
     in the regular list of variables. */
  if (var = find_variable_internal ("PATH", 1))	/* XXX could be array? */
    path_list = value_cell (var);
  else
    path_list = (char *)NULL;

  if (path_list == 0 || *path_list == '\0')
    return (savestring (name));

  cmd = find_user_command_in_path (name, path_list, flags);

  if (var && tempvar_p (var))
    dispose_variable (var);

  return (cmd);
}

static char *
find_user_command_internal (name, flags)
     char *name;
     int flags;
{
#ifdef __WIN32__
  char *res, *dotexe;

  dotexe = xmalloc (strlen (name) + 5);
  strcpy (dotexe, name);
  strcat (dotexe, ".exe");
  res = _find_user_command_internal (dotexe, flags);
  free (dotexe);
  if (res == 0)
    res = _find_user_command_internal (name, flags);
  return res;
#else
  return (_find_user_command_internal (name, flags));
#endif
}

/* Return the next element from PATH_LIST, a colon separated list of
   paths.  PATH_INDEX_POINTER is the address of an index into PATH_LIST;
   the index is modified by this function.
   Return the next element of PATH_LIST or NULL if there are no more. */
static char *
get_next_path_element (path_list, path_index_pointer)
     char *path_list;
     int *path_index_pointer;
{
  char *path;

  path = extract_colon_unit (path_list, path_index_pointer);

  if (!path)
    return (path);

  if (!*path)
    {
      free (path);
      path = savestring (".");
    }

  return (path);
}

/* Look for PATHNAME in $PATH.  Returns either the hashed command
   corresponding to PATHNAME or the first instance of PATHNAME found
   in $PATH.  Returns a newly-allocated string. */
char *
search_for_command (pathname)
     char *pathname;
{
  char *hashed_file, *command;
  int temp_path, st;
  SHELL_VAR *path;

  hashed_file = command = (char *)NULL;

  /* If PATH is in the temporary environment for this command, don't use the
     hash table to search for the full pathname. */
  path = find_tempenv_variable ("PATH");
  temp_path = path != 0;

  /* Don't waste time trying to find hashed data for a pathname
     that is already completely specified or if we're using a command-
     specific value for PATH. */
  if (path == 0 && absolute_program (pathname) == 0)
    hashed_file = find_hashed_filename (pathname);

  /* If a command found in the hash table no longer exists, we need to
     look for it in $PATH.  Thank you Posix.2.  This forces us to stat
     every command found in the hash table. */

  if (hashed_file && (posixly_correct || check_hashed_filenames))
    {
      st = file_status (hashed_file);
      if ((st ^ (FS_EXISTS | FS_EXECABLE)) != 0)
	{
	  remove_hashed_filename (pathname);
	  free (hashed_file);
	  hashed_file = (char *)NULL;
	}
    }

  if (hashed_file)
    command = hashed_file;
  else if (absolute_program (pathname))
    /* A command containing a slash is not looked up in PATH or saved in
       the hash table. */
    command = savestring (pathname);
  else
    {
      /* If $PATH is in the temporary environment, we've already retrieved
	 it, so don't bother trying again. */
      if (temp_path)
        {
	  command = find_user_command_in_path (pathname, value_cell (path),
					       FS_EXEC_PREFERRED|FS_NODIRS);
	  if (tempvar_p (path))
	    dispose_variable (path);
        }
      else
	command = find_user_command (pathname);
      if (command && hashing_enabled && temp_path == 0)
	remember_filename (pathname, command, dot_found_in_search, 1);
    }
  return (command);
}

char *
user_command_matches (name, flags, state)
     char *name;
     int flags, state;
{
  register int i;
  int  path_index, name_len;
  char *path_list, *path_element, *match;
  struct stat dotinfo;
  static char **match_list = NULL;
  static int match_list_size = 0;
  static int match_index = 0;

  if (state == 0)
    {
      /* Create the list of matches. */
      if (match_list == 0)
	{
	  match_list_size = 5;
	  match_list = (char **)xmalloc (match_list_size * sizeof(char *));
	}

      /* Clear out the old match list. */
      for (i = 0; i < match_list_size; i++)
	match_list[i] = 0;

      /* We haven't found any files yet. */
      match_index = 0;

      if (absolute_program (name))
	{
	  match_list[0] = find_absolute_program (name, flags);
	  match_list[1] = (char *)NULL;
	  path_list = (char *)NULL;
	}
      else
	{
	  name_len = strlen (name);
	  file_to_lose_on = (char *)NULL;
	  dot_found_in_search = 0;
      	  stat (".", &dotinfo);
	  path_list = get_string_value ("PATH");
      	  path_index = 0;
	}

      while (path_list && path_list[path_index])
	{
	  path_element = get_next_path_element (path_list, &path_index);

	  if (path_element == 0)
	    break;

	  match = find_in_path_element (name, path_element, flags, name_len, &dotinfo);

	  free (path_element);

	  if (match == 0)
	    continue;

	  if (match_index + 1 == match_list_size)
	    {
	      match_list_size += 10;
	      match_list = (char **)xrealloc (match_list, (match_list_size + 1) * sizeof (char *));
	    }

	  match_list[match_index++] = match;
	  match_list[match_index] = (char *)NULL;
	  FREE (file_to_lose_on);
	  file_to_lose_on = (char *)NULL;
	}

      /* We haven't returned any strings yet. */
      match_index = 0;
    }

  match = match_list[match_index];

  if (match)
    match_index++;

  return (match);
}

/* Turn PATH, a directory, and NAME, a filename, into a full pathname.
   This allocates new memory and returns it. */
static char *
make_full_pathname (path, name, name_len)
     char *path, *name;
     int name_len;
{
  char *full_path;
  int path_len;

  path_len = strlen (path);
  full_path = xmalloc (2 + path_len + name_len);
  strcpy (full_path, path);
  full_path[path_len] = '/';
  strcpy (full_path + path_len + 1, name);
  return (full_path);
}

static char *
find_absolute_program (name, flags)
     char *name;
     int flags;
{
  int st;

  st = file_status (name);

  /* If the file doesn't exist, quit now. */
  if ((st & FS_EXISTS) == 0)
    return ((char *)NULL);

  /* If we only care about whether the file exists or not, return
     this filename.  Otherwise, maybe we care about whether this
     file is executable.  If it is, and that is what we want, return it. */
  if ((flags & FS_EXISTS) || ((flags & FS_EXEC_ONLY) && (st & FS_EXECABLE)))
    return (savestring (name));

  return ((char *)NULL);
}

static char *
find_in_path_element (name, path, flags, name_len, dotinfop)
     char *name, *path;
     int flags, name_len;
     struct stat *dotinfop;
{
  int status;
  char *full_path, *xpath;

  xpath = (*path == '~') ? bash_tilde_expand (path) : path;

  /* Remember the location of "." in the path, in all its forms
     (as long as they begin with a `.', e.g. `./.') */
  if (dot_found_in_search == 0 && *xpath == '.')
    dot_found_in_search = same_file (".", xpath, dotinfop, (struct stat *)NULL);

  full_path = make_full_pathname (xpath, name, name_len);

  status = file_status (full_path);

  if (xpath != path)
    free (xpath);

  if ((status & FS_EXISTS) == 0)
    {
      free (full_path);
      return ((char *)NULL);
    }

  /* The file exists.  If the caller simply wants the first file, here it is. */
  if (flags & FS_EXISTS)
    return (full_path);

  /* If the file is executable, then it satisfies the cases of
      EXEC_ONLY and EXEC_PREFERRED.  Return this file unconditionally. */
  if ((status & FS_EXECABLE) &&
      (((flags & FS_NODIRS) == 0) || ((status & FS_DIRECTORY) == 0)))
    {
      FREE (file_to_lose_on);
      file_to_lose_on = (char *)NULL;
      return (full_path);
    }

  /* The file is not executable, but it does exist.  If we prefer
     an executable, then remember this one if it is the first one
     we have found. */
  if ((flags & FS_EXEC_PREFERRED) && file_to_lose_on == 0)
    file_to_lose_on = savestring (full_path);

  /* If we want only executable files, or we don't want directories and
     this file is a directory, fail. */
  if ((flags & FS_EXEC_ONLY) || (flags & FS_EXEC_PREFERRED) ||
      ((flags & FS_NODIRS) && (status & FS_DIRECTORY)))
    {
      free (full_path);
      return ((char *)NULL);
    }
  else
    return (full_path);
}

/* This does the dirty work for find_user_command_internal () and
   user_command_matches ().
   NAME is the name of the file to search for.
   PATH_LIST is a colon separated list of directories to search.
   FLAGS contains bit fields which control the files which are eligible.
   Some values are:
      FS_EXEC_ONLY:		The file must be an executable to be found.
      FS_EXEC_PREFERRED:	If we can't find an executable, then the
				the first file matching NAME will do.
      FS_EXISTS:		The first file found will do.
      FS_NODIRS:		Don't find any directories.
*/
static char *
find_user_command_in_path (name, path_list, flags)
     char *name;
     char *path_list;
     int flags;
{
  char *full_path, *path;
  int path_index, name_len;
  struct stat dotinfo;

  /* We haven't started looking, so we certainly haven't seen
     a `.' as the directory path yet. */
  dot_found_in_search = 0;

  if (absolute_program (name))
    {
      full_path = find_absolute_program (name, flags);
      return (full_path);
    }

  if (path_list == 0 || *path_list == '\0')
    return (savestring (name));		/* XXX */

  file_to_lose_on = (char *)NULL;
  name_len = strlen (name);
  stat (".", &dotinfo);
  path_index = 0;

  while (path_list[path_index])
    {
      /* Allow the user to interrupt out of a lengthy path search. */
      QUIT;

      path = get_next_path_element (path_list, &path_index);
      if (path == 0)
	break;

      /* Side effects: sets dot_found_in_search, possibly sets
	 file_to_lose_on. */
      full_path = find_in_path_element (name, path, flags, name_len, &dotinfo);
      free (path);

      /* This should really be in find_in_path_element, but there isn't the
	 right combination of flags. */
      if (full_path && is_directory (full_path))
	{
	  free (full_path);
	  continue;
	}

      if (full_path)
	{
	  FREE (file_to_lose_on);
	  return (full_path);
	}
    }

  /* We didn't find exactly what the user was looking for.  Return
     the contents of FILE_TO_LOSE_ON which is NULL when the search
     required an executable, or non-NULL if a file was found and the
     search would accept a non-executable as a last resort. */
  return (file_to_lose_on);
}
