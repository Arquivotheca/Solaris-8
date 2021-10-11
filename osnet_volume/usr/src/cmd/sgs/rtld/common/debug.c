/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)debug.c	1.38	99/05/18 SMI"

#include	"_synonyms.h"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<stdio.h>
#include	<fcntl.h>
#include	<stdarg.h>
#include	<dlfcn.h>
#include	<unistd.h>
#include	<string.h>
#include	<thread.h>
#include	"debug.h"
#include	"_rtld.h"
#include	"_elf.h"
#include	"profile.h"
#include	"msg.h"


static int	dbg_fd;		/* debugging output file descriptor */
static dev_t	dbg_dev;
static ino_t	dbg_ino;
static pid_t	pid;

/*
 * Enable diagnostic output.  All debugging functions reside in the linker
 * debugging library liblddbg.so which is lazy loaded when required.
 */
int
dbg_setup(const char *options)
{
	Rt_map		*rlmp = lml_rtld.lm_head;
	int		error;
	struct stat	status;

	PRF_MCOUNT(103, dbg_setup);

	/*
	 * If we're running secure only allow debugging if ld.so.1 itself is
	 * owned by root and has its mode setuid.  Fail silently.
	 */
	if (rtld_flags & RT_FL_SECURE) {
		struct stat	status;

		if (stat(NAME(rlmp), &status) == 0) {
			if ((status.st_uid != 0) ||
			    (!(status.st_mode & S_ISUID)))
				return (0);
		} else
			return (0);
	}

	/*
	 * Call the debugging setup routine (which will lazyload the debugging
	 * library).  This function verifies the debugging tokens provided and
	 * return a masks indicating the debugging categories selected.  The
	 * mask effectively enables calls to the debugging library.
	 */
	if (elf_rtld_load(rlmp) == 0)
		return (0);
	if ((error = Dbg_setup(options)) == S_ERROR)
		exit(0);
	if (error != 0) {
		Rel *		_reladd, * reladd = (Rel *)JMPREL(rlmp);
		Rt_map *	dlmp = (Rt_map *)NEXT(rlmp);

		/*
		 * Loop through ld.so.1's plt relocations and bind all debugging
		 * functions.  This avoids possible recursion, and prevents the
		 * user from having to see the debugging bindings when they are
		 * trying to investigate their own bindings.
		 *
		 * NOTE: we assume liblddbg is the next object loaded on
		 * ld.so.1's link-map (setup() insures this).
		 */
		for (_reladd = reladd + (PLTRELSZ(rlmp) / RELENT(rlmp));
		    reladd < _reladd;
		    reladd = (Rel *)((uintptr_t)reladd +
		    (uintptr_t)RELENT(rlmp))) {
			unsigned long	addr;
			Sym		*sym;
			char		*name;
			Rt_map		*_dlmp;

			sym = (Sym *)((unsigned long)SYMTAB(rlmp) +
				(ELF_R_SYM(reladd->r_info) * SYMENT(rlmp)));
			name = (char *)(STRTAB(rlmp) + sym->st_name);

			/*
			 * Optimization: All debugging references start `Dbg'.
			 */
			if ((name[0] != 'D') || (name[1] != 'b') ||
			    (name[2] != 'g'))
				continue;

			addr = reladd->r_offset;
			addr += ADDR(rlmp);

			/*
			 * Find the symbol definition and perform the required
			 * relocation.
			 */
			if ((sym = SYMINTP(dlmp)(name, dlmp, &_dlmp,
			    (LKUP_DEFT | LKUP_FIRST), elf_hash(name))) != 0) {
				unsigned long	value = sym->st_value;

				if (!(FLAGS(_dlmp) & FLG_RT_FIXED))
					value += ADDR(_dlmp);

				elf_plt_write((unsigned long *)addr,
				    (unsigned long *)value,
				    (unsigned long *)PLTGOT(_dlmp));
			}
		}
	}


	if (dbg_file) {
		/*
		 * If an LD_DEBUG_OUTPUT file was specified then we need
		 * to direct all diagnostics to the specified file.  Add
		 * the process id as a file suffix so that multiple
		 * processes that inherit the same debugging environment
		 * variable don't fight over the same file.
		 */
		char 	file[MAXPATHLEN];

		(void) snprintf(file, MAXPATHLEN, MSG_ORIG(MSG_DBG_FMT_FILE),
			dbg_file, getpid());
		if ((dbg_fd = open(file, (O_RDWR | O_CREAT), 0666)) == -1) {
			int	err = errno;

			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), file,
			    strerror(err));
			dbg_mask = 0;
			return (0);
		}
	} else {
		/*
		 * The default is to direct debugging to the stderr.
		 */
		dbg_fd = 2;
	}

	/*
	 * Initialize the dev/inode pair to enable us to determine if
	 * the debugging file descriptor is still available once the
	 * application has been entered.
	 */
	(void) fstat(dbg_fd, &status);
	dbg_dev = status.st_dev;
	dbg_ino = status.st_ino;
	pid = getpid();

	return (error);
}

/*
 * All diagnostic requests are funneled to this routine.
 */
/*PRINTFLIKE1*/
void
dbg_print(const char *format, ...)
{
	va_list			args;
	char			buffer[ERRSIZE + 1];
	pid_t			_pid;
	struct stat		status;
	Prfbuf			prf;

	PRF_MCOUNT(104, dbg_print);

	/*
	 * If we're in the application make sure the debugging file descriptor
	 * is still available (ie, the user hasn't closed and/or reused the
	 * same descriptor).
	 */
	if (rtld_flags & RT_FL_APPLIC) {
		if ((fstat(dbg_fd, &status) == -1) ||
		    (status.st_dev != dbg_dev) ||
		    (status.st_ino != dbg_ino)) {
			if (dbg_file) {
				/*
				 * If the user specified output file has been
				 * disconnected try and reconnect to it.
				 */
				char 	file[MAXPATHLEN];

				(void) snprintf(file, MAXPATHLEN,
					MSG_ORIG(MSG_DBG_FMT_FILE),
					dbg_file, pid);
				if ((dbg_fd = open(file, (O_RDWR | O_APPEND),
				    0)) == -1) {
					dbg_mask = 0;
					return;
				}
				(void) fstat(dbg_fd, &status);
				dbg_dev = status.st_dev;
				dbg_ino = status.st_ino;
			} else {
				/*
				 * If stderr has been stolen from us simply
				 * turn debugging off.
				 */
				dbg_mask = 0;
				return;
			}
		}
	}

	/*
	 * The getpid() call is a 'special' interface between ld.so.1
	 * and dbx, because of this getpid() can't be called freely
	 * until after control has been given to the user program.
	 * Once the control has been given to the user program
	 * we know that the r_debug structure has been properly
	 * initialized for the debugger.
	 */
	if (rtld_flags & RT_FL_APPLIC)
		_pid = getpid();
	else
		_pid = pid;

	prf.pr_buf = prf.pr_cur = buffer;
	prf.pr_len = ERRSIZE;
	prf.pr_fd = dbg_fd;

	if (rtld_flags & RT_FL_THREADS)
		(void) bufprint(&prf, MSG_ORIG(MSG_DBG_FMT_THREAD), _pid,
			thr_self());
	else
		(void) bufprint(&prf, MSG_ORIG(MSG_DBG_FMT_DIAG), _pid);

	/*
	 * Format the message and print it.
	 */
	va_start(args, format);
	prf.pr_cur--;
	(void) doprf(format, args, &prf);
	*(prf.pr_cur - 1) = '\n';
	(void) dowrite(&prf);
	va_end(args);
}
