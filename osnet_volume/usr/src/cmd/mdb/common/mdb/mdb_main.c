/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_main.c	1.2	99/09/06 SMI"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/priocntl.h>
#include <sys/rtpriocntl.h>
#include <sys/resource.h>
#include <sys/termios.h>
#include <sys/param.h>
#include <sys/regset.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/reg.h>

#include <libproc.h>
#include <alloca.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>

#include <mdb/mdb_lex.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_signal.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_target.h>
#include <mdb/mdb_gelf.h>
#include <mdb/mdb_conf.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_set.h>
#include <mdb/mdb.h>

#ifndef STACK_BIAS
#define	STACK_BIAS	0
#endif

#if defined(i386) || defined(__i386)
#define	STACK_REGISTER	EBP
#else
#define	STACK_REGISTER	SP
#endif


/*
 * Similar to the panic_* variables in the kernel, we keep some relevant
 * information stored in a set of global _mdb_abort_* variables; in the
 * event that the debugger dumps core, these will aid core dump analysis.
 */
const char *volatile _mdb_abort_str;	/* reason for failure */
siginfo_t _mdb_abort_info;		/* signal info for fatal signal */
ucontext_t _mdb_abort_ctx;		/* context fatal signal interrupted */
int _mdb_abort_rcount;			/* number of times resume requested */
int _mdb_self_fd = -1;			/* fd for self as for valid_frame */

static void
terminate(int status)
{
	mdb_destroy();
	exit(status);
}

static void
print_frame(uintptr_t pc, int fnum)
{
	Dl_info dli;

	if (dladdr((void *)pc, &dli)) {
		mdb_iob_printf(mdb.m_err, "    [%d] %s`%s+0x%lx()\n", fnum,
		    strbasename(dli.dli_fname), dli.dli_sname,
		    pc - (uintptr_t)dli.dli_saddr);
	} else
		mdb_iob_printf(mdb.m_err, "    [%d] %p()\n", fnum, pc);
}

static int
valid_frame(struct frame *fr)
{
	static struct frame fake;
	uintptr_t addr = (uintptr_t)fr;

	if (pread(_mdb_self_fd, &fake, sizeof (fake), addr) != sizeof (fake)) {
		mdb_iob_printf(mdb.m_err, "    invalid frame (%p)\n", fr);
		return (0);
	}

	if (addr & (STACK_ALIGN - 1)) {
		mdb_iob_printf(mdb.m_err, "    mis-aligned frame (%p)\n", fr);
		return (0);
	}

	return (1);
}

/*ARGSUSED*/
static void
flt_handler(int sig, siginfo_t *sip, ucontext_t *ucp, void *data)
{
	static const struct rlimit rl = {
		(rlim_t)RLIM_INFINITY, (rlim_t)RLIM_INFINITY
	};

	const mdb_idcmd_t *idcp = NULL;

	if (mdb.m_frame != NULL && mdb.m_frame->f_cp != NULL)
		idcp = mdb.m_frame->f_cp->c_dcmd;

	if (sip != NULL)
		bcopy(sip, &_mdb_abort_info, sizeof (_mdb_abort_info));
	if (ucp != NULL)
		bcopy(ucp, &_mdb_abort_ctx, sizeof (_mdb_abort_ctx));

	_mdb_abort_info.si_signo = sig;
	(void) mdb_signal_sethandler(sig, SIG_DFL, NULL);

	/*
	 * If there is no current dcmd, or the current dcmd comes from a
	 * builtin module, we don't allow resume and always core dump.
	 */
	if (idcp == NULL || idcp->idc_modp == NULL ||
	    idcp->idc_modp == &mdb.m_rmod || idcp->idc_modp->mod_hdl == NULL)
		goto dump;

	if (mdb.m_term != NULL) {
		struct frame *fr = (struct frame *)
		    (ucp->uc_mcontext.gregs[STACK_REGISTER] + STACK_BIAS);

		char signame[SIG2STR_MAX];
		int i = 1;
		char c;

		if (sig2str(sig, signame) == -1) {
			mdb_iob_printf(mdb.m_err,
			    "\n*** %s: received signal %d at:\n",
			    mdb.m_pname, sig);
		} else {
			mdb_iob_printf(mdb.m_err,
			    "\n*** %s: received signal %s at:\n",
			    mdb.m_pname, signame);
		}

		if (ucp->uc_mcontext.gregs[PC] != 0)
			print_frame(ucp->uc_mcontext.gregs[PC], i++);

		while (fr != NULL && valid_frame(fr) && fr->fr_savpc != 0) {
			print_frame(fr->fr_savpc, i++);
			fr = (struct frame *)
			    ((uintptr_t)fr->fr_savfp + STACK_BIAS);
		}

query:
		mdb_iob_printf(mdb.m_err, "\n%s: (c)ore dump, (q)uit, "
		    "(r)ecover, or (s)top for debugger [cqrs]? ", mdb.m_pname);

		mdb_iob_flush(mdb.m_err);

		for (;;) {
			if (IOP_READ(mdb.m_term, &c, sizeof (c)) != sizeof (c))
				goto dump;

			switch (c) {
			case 'c':
			case 'C':
				(void) setrlimit(RLIMIT_CORE, &rl);
				mdb_iob_printf(mdb.m_err, "\n%s: attempting "
				    "to dump core ...\n", mdb.m_pname);
				goto dump;

			case 'q':
			case 'Q':
				(void) mdb_signal_unblockall();
				terminate(1);
				/*NOTREACHED*/

			case 'r':
			case 'R':
				mdb_iob_printf(mdb.m_err, "\n%s: unloading "
				    "module '%s' ...\n", mdb.m_pname,
				    idcp->idc_modp->mod_name);

				(void) mdb_module_unload(
				    idcp->idc_modp->mod_name);

				(void) mdb_signal_sethandler(sig,
				    flt_handler, NULL);

				_mdb_abort_rcount++;
				mdb.m_intr = 0;
				mdb.m_pend = 0;

				(void) mdb_signal_unblockall();
				longjmp(mdb.m_frame->f_pcb, MDB_ERR_ABORT);
				/*NOTREACHED*/

			case 's':
			case 'S':
				mdb_iob_printf(mdb.m_err, "\n%s: "
				    "attempting to stop pid %d ...\n",
				    mdb.m_pname, (int)getpid());

				/*
				 * Stop ourself; if this fails or we are
				 * subsequently continued, ask again.
				 */
				(void) mdb_signal_raise(SIGSTOP);
				(void) mdb_signal_unblockall();
				goto query;
			}
		}
	}

dump:
	if (SI_FROMUSER(sip)) {
		(void) mdb_signal_block(sig);
		(void) mdb_signal_raise(sig);
	}

	(void) sigfillset(&ucp->uc_sigmask);
	(void) sigdelset(&ucp->uc_sigmask, sig);

	if (_mdb_abort_str == NULL)
		_mdb_abort_str = "fatal signal received";

	ucp->uc_flags |= UC_SIGMASK;
	(void) setcontext(ucp);
}

/*ARGSUSED*/
static void
int_handler(int sig, siginfo_t *sip, ucontext_t *ucp, void *data)
{
	if (mdb.m_intr == 0)
		longjmp(mdb.m_frame->f_pcb, MDB_ERR_SIGINT);
	else
		mdb.m_pend++;
}

static void
usage(void)
{
	mdb_iob_printf(mdb.m_err, "Usage: %s [-kmuwyAFMS] [+/-o option] "
	    "[-p pid] [-s distance] [-I path] [-L path]\n\t[-P prompt] "
	    "[-R root] [-V dis-version] [object [core] | core | suffix]\n\n",
	    mdb.m_pname);

	mdb_iob_puts(mdb.m_err,
	    "\t-k force kernel debugging mode\n"
	    "\t-m disable demand-loading of module symbols\n"
	    "\t-o set specified debugger option (+o to unset)\n"
	    "\t-p attach to specified process-id\n"
	    "\t-s set symbol matching distance\n"
	    "\t-u force user program debugging mode\n"
	    "\t-w enable write mode\n"
	    "\t-y send terminal initialization sequences for tty mode\n"
	    "\t-A disable automatic loading of mdb modules\n"
	    "\t-F enable forcible takeover mode\n"
	    "\t-M preload all module symbols\n"
	    "\t-I set initial path for macro files\n"
	    "\t-L set initial path for module libs\n"
	    "\t-P set command-line prompt\n"
	    "\t-R set root directory for pathname expansion\n"
	    "\t-S suppress processing of ~/.mdbrc file\n"
	    "\t-V set disassembler version\n");

	terminate(2);
}

int
main(int argc, char *argv[])
{
	mdb_tgt_ctor_f *tgt_ctor = mdb_proc_tgt_create;
	const char **tgt_argv = alloca(argc * sizeof (char *));
	int tgt_argc = 0;
	mdb_tgt_t *tgt;

	char object[MAXPATHLEN], execname[MAXPATHLEN];
	mdb_io_t *in_io, *out_io, *err_io;
	struct termios tios;
	int status, c;
	char *p;

	const char *Vflag = NULL, *pidarg = NULL;
	int Rflag = 0, Sflag = 0, Oflag = 0;

	if (realpath(getexecname(), execname) == NULL) {
		(void) strncpy(execname, argv[0], MAXPATHLEN);
		execname[MAXPATHLEN - 1] = '\0';
	}

	mdb_create(execname, argv[0]);
	bzero(tgt_argv, argc * sizeof (char *));
	argv[0] = (char *)mdb.m_pname;
	_mdb_self_fd = open("/proc/self/as", O_RDONLY);

	(void) mdb_signal_sethandler(SIGPIPE, SIG_IGN, NULL);
	(void) mdb_signal_sethandler(SIGQUIT, SIG_IGN, NULL);

	(void) mdb_signal_sethandler(SIGILL, flt_handler, NULL);
	(void) mdb_signal_sethandler(SIGTRAP, flt_handler, NULL);
	(void) mdb_signal_sethandler(SIGIOT, flt_handler, NULL);
	(void) mdb_signal_sethandler(SIGEMT, flt_handler, NULL);
	(void) mdb_signal_sethandler(SIGFPE, flt_handler, NULL);
	(void) mdb_signal_sethandler(SIGBUS, flt_handler, NULL);
	(void) mdb_signal_sethandler(SIGSEGV, flt_handler, NULL);

	out_io = mdb_fdio_create(STDOUT_FILENO);
	mdb.m_out = mdb_iob_create(out_io, MDB_IOB_WRONLY);

	err_io = mdb_fdio_create(STDERR_FILENO);
	mdb.m_err = mdb_iob_create(err_io, MDB_IOB_WRONLY);

	in_io = mdb_fdio_create(STDIN_FILENO);
	mdb.m_term = NULL;

	mdb_dmode(mdb_dstr2mode(getenv("MDB_DEBUG")));

	if ((p = getenv("HISTSIZE")) != NULL && strisnum(p)) {
		mdb.m_histlen = strtoi(p);
		if (mdb.m_histlen < 1)
			mdb.m_histlen = 1;
	}

	while (optind < argc) {
		while ((c = getopt(argc, argv,
		    "kmo:p:s:uwyAD:FI:L:MOP:R:SV:")) != (int)EOF) {
			switch (c) {
			case 'k':
				tgt_ctor = mdb_kvm_tgt_create;
				break;
			case 'm':
				mdb.m_tgtflags |= MDB_TGT_F_NOLOAD;
				mdb.m_tgtflags &= ~MDB_TGT_F_PRELOAD;
				break;
			case 'o':
				if (!mdb_set_options(optarg, TRUE))
					terminate(2);
				break;
			case 'p':
				tgt_ctor = mdb_proc_tgt_create;
				pidarg = optarg;
				break;
			case 's':
				if (!strisnum(optarg)) {
					warn("expected integer following -s\n");
					terminate(2);
				}
				mdb.m_symdist = (size_t)(uint_t)strtoi(optarg);
				break;
			case 'u':
				tgt_ctor = mdb_proc_tgt_create;
				break;
			case 'w':
				mdb.m_tgtflags |= MDB_TGT_F_RDWR;
				break;
			case 'y':
				mdb.m_flags |= MDB_FL_USECUP;
				break;
			case 'A':
				mdb.m_flags |= MDB_FL_NOMODS;
				break;
			case 'D':
				mdb_dmode(mdb_dstr2mode(optarg));
				break;
			case 'F':
				mdb.m_tgtflags |= MDB_TGT_F_FORCE;
				break;
			case 'I':
				(void) strncpy(mdb.m_ipathstr, optarg,
				    MAXPATHLEN);
				mdb.m_ipathstr[MAXPATHLEN - 1] = 0;
				break;
			case 'L':
				(void) strncpy(mdb.m_lpathstr, optarg,
				    MAXPATHLEN);
				mdb.m_lpathstr[MAXPATHLEN - 1] = 0;
				break;
			case 'M':
				mdb.m_tgtflags |= MDB_TGT_F_PRELOAD;
				mdb.m_tgtflags &= ~MDB_TGT_F_NOLOAD;
				break;
			case 'O':
				Oflag++;
				break;
			case 'P':
				if (!mdb_set_prompt(optarg))
					terminate(2);
				break;
			case 'R':
				(void) strncpy(mdb.m_root, optarg, MAXPATHLEN);
				mdb.m_root[MAXPATHLEN - 1] = '\0';
				Rflag++;
				break;
			case 'S':
				Sflag++;
				break;
			case 'V':
				Vflag = optarg;
				break;
			default:
				usage();
			}
		}

		if (optind < argc) {
			const char *arg = argv[optind++];

			if (arg[0] == '+' && strlen(arg) == 2) {
				if (arg[1] != 'o') {
					warn("illegal option -- %s\n", arg);
					terminate(2);
				}
				if (optind >= argc) {
					warn("option requires an argument -- "
					    "%s\n", arg);
					terminate(2);
				}
				if (!mdb_set_options(argv[optind++], FALSE))
					terminate(2);
			} else
				tgt_argv[tgt_argc++] = arg;
		}
	}

	if (mdb.m_debug & MDB_DBG_HELP)
		terminate(0); /* Quit here if we've printed out the tokens */

	/*
	 * If standard input appears to have tty attributes, attempt to
	 * initialize a terminal i/o backend on top of stdin and stdout.
	 */
	if (IOP_CTL(in_io, TCGETS, &tios) == 0) {
		in_io = mdb_termio_create(NULL, STDIN_FILENO, STDOUT_FILENO);
		mdb.m_term = in_io;
	}

	mdb.m_in = mdb_iob_create(in_io, MDB_IOB_RDONLY);
	if (mdb.m_term != NULL) {
		mdb_iob_setpager(mdb.m_out, mdb.m_term);
		if (mdb.m_flags & MDB_FL_PAGER)
			mdb_iob_setflags(mdb.m_out, MDB_IOB_PGENABLE);
		else
			mdb_iob_clrflags(mdb.m_out, MDB_IOB_PGENABLE);
	}

	mdb_pservice_init();
	mdb_lex_reset();

	if ((mdb.m_shell = getenv("SHELL")) == NULL)
		mdb.m_shell = "/bin/sh";

	if (tgt_ctor == mdb_kvm_tgt_create) {
		if (pidarg != NULL) {
			warn("-p and -k options are mutually exclusive\n");
			terminate(2);
		}
		/*
		 * Here we really mean to specify /dev/kmem (virtual addresses)
		 * but we use /dev/mem (physical addresses) instead: this is
		 * because on 2.6 and earlier, /dev/kmem is broken, and on 2.7
		 * and later, kvm_open() changes /dev/mem to /dev/kmem for us.
		 */
		if (tgt_argc == 0)
			tgt_argv[tgt_argc++] = "/dev/ksyms";
		if (tgt_argc == 1 && strisnum(tgt_argv[0]) == 0)
			tgt_argv[tgt_argc++] = "/dev/mem";
	}

	if (pidarg != NULL) {
		if (tgt_argc != 0) {
			warn("-p may not be used with other arguments\n");
			terminate(2);
		}
		if (proc_arg_psinfo(pidarg, PR_ARG_PIDS, NULL, &status) == -1) {
			warn("cannot attach to %s: %s\n",
			    pidarg, Pgrab_error(status));
			terminate(1);
		}
		if (strchr(pidarg, '/') != NULL)
			(void) mdb_iob_snprintf(object, MAXPATHLEN,
			    "%s/object/a.out", pidarg);
		else
			(void) mdb_iob_snprintf(object, MAXPATHLEN,
			    "/proc/%s/object/a.out", pidarg);
		tgt_argv[tgt_argc++] = object;
		tgt_argv[tgt_argc++] = pidarg;
	}

	if (tgt_argc > 0) {
		Elf32_Ehdr ehdr;
		mdb_io_t *io;

		/*
		 * If we just have an object file name, and that file doesn't
		 * exist, and it's a string of digits, infer it to be a
		 * sequence number referring to a pair of crash dump files.
		 */
		if (tgt_argc == 1 && access(tgt_argv[0], F_OK) == -1 &&
		    strisnum(tgt_argv[0])) {

			size_t len = strlen(tgt_argv[0]) + 8;
			const char *object = tgt_argv[0];

			tgt_argv[0] = mdb_alloc(len, UM_SLEEP);
			tgt_argv[1] = mdb_alloc(len, UM_SLEEP);

			(void) strcpy((char *)tgt_argv[0], "unix.");
			(void) strcat((char *)tgt_argv[0], object);
			(void) strcpy((char *)tgt_argv[1], "vmcore.");
			(void) strcat((char *)tgt_argv[1], object);

			tgt_argc = 2;
		}

		/*
		 * We need to open the object file in order to determine its
		 * ELF class and potentially re-exec ourself.
		 */
		if ((io = mdb_fdio_create_path(NULL, tgt_argv[0],
		    O_RDONLY, 0)) == NULL) {
			warn("failed to open %s", tgt_argv[0]);
			terminate(1);
		}

		/*
		 * If gelf_check fails completely, the file is not an ELF file:
		 * call gelf_check again with ET_EXEC to print an error message.
		 */
		if (mdb_gelf_check(io, &ehdr, ET_NONE) == -1) {
			(void) mdb_gelf_check(io, &ehdr, ET_EXEC);
			mdb_io_destroy(io);
			terminate(1);
		}

		mdb_io_destroy(io);

		/*
		 * The object file turned out to be a user core file (ET_CORE),
		 * and no other arguments were specified, swap 0 and 1.  The
		 * proc target will infer the executable for us.
		 */
		if (ehdr.e_type == ET_CORE) {
			tgt_argv[tgt_argc++] = tgt_argv[0];
			tgt_argv[0] = NULL;
			tgt_ctor = mdb_proc_tgt_create;
		}

		/*
		 * If tgt_argv[1] is filled in, open it up and determine if it
		 * is a vmcore file.  If it is, gelf_check will fail and we
		 * set tgt_ctor to 'kvm'; otherwise we use the default.
		 */
		if (tgt_argc > 1 && tgt_argv[0] != NULL && pidarg == NULL) {
			Elf32_Ehdr chdr;

			if (access(tgt_argv[1], F_OK) == -1) {
				warn("failed to access %s", tgt_argv[1]);
				terminate(1);
			}

			if ((io = mdb_fdio_create_path(NULL, tgt_argv[1],
			    O_RDONLY, 0)) == NULL) {
				warn("failed to open %s", tgt_argv[1]);
				terminate(1);
			}

			if (mdb_gelf_check(io, &chdr, ET_NONE) == -1)
				tgt_ctor = mdb_kvm_tgt_create;

			mdb_io_destroy(io);
		}

		/*
		 * At this point, we've read the ELF header for either an
		 * object file or core into ehdr.  If the class does not match
		 * ours, attempt to exec the mdb of the appropriate class.
		 */
#ifdef _LP64
		if (ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
#else
		if (ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
#endif
			if ((p = strrchr(execname, '/')) == NULL) {
				warn("cannot determine absolute pathname");
				terminate(1);
			}
#ifdef _LP64
#ifdef __sparc
			(void) strcpy(p, "/../sparcv7/");
#else
			(void) strcpy(p, "/../i86/");
#endif
#else
#ifdef __sparc
			(void) strcpy(p, "/../sparcv9/");
#else
			(void) strcpy(p, "/../ia64/");
#endif
#endif
			(void) strcat(p, mdb.m_pname);

			if (mdb.m_term != NULL)
				(void) IOP_CTL(in_io, TCSETS, &tios);

			(void) execv(execname, argv);

			/*
			 * If execv fails, suppress ENOEXEC.  Experience shows
			 * the most common reason is that the machine is booted
			 * under a 32-bit kernel, in which case it is clearer
			 * to only print the message below.
			 */
			if (errno != ENOEXEC)
				warn("failed to exec %s", execname);
#ifdef _LP64
			warn("64-bit %s cannot debug 32-bit program %s\n",
			    mdb.m_pname, tgt_argv[0] ?
			    tgt_argv[0] : tgt_argv[1]);
#else
			warn("32-bit %s cannot debug 64-bit program %s\n",
			    mdb.m_pname, tgt_argv[0] ?
			    tgt_argv[0] : tgt_argv[1]);
#endif
			terminate(1);
		}
	}

	/*
	 * Path evaluation part 1: Create the initial module path to allow
	 * the target constructor to load a support module.
	 */
	mdb_set_ipath(mdb.m_ipathstr);
	mdb_set_lpath(mdb.m_lpathstr);

	tgt = mdb_tgt_create(tgt_ctor, mdb.m_tgtflags, tgt_argc, tgt_argv);

	if (Vflag != NULL && mdb_dis_select(Vflag) == -1)
		warn("invalid disassembler mode -- %s\n", Vflag);

	mdb_tgt_activate(tgt);
	if (mdb.m_target == NULL)
		terminate(1); /* Exit if we failed to construct a target */

	if (Rflag && mdb.m_term != NULL)
		warn("Using proto area %s\n", mdb.m_root);

	/*
	 * If the target was successfully constructed and -O was specified,
	 * we now attempt to enter piggy-mode for debugging jurassic problems.
	 */
	if (Oflag) {
		pcinfo_t pci;

		(void) strcpy(pci.pc_clname, "RT");

		if (priocntl(P_LWPID, P_MYID, PC_GETCID, (caddr_t)&pci) != -1) {
			pcparms_t pcp;
			rtparms_t *rtp = (rtparms_t *)pcp.pc_clparms;

			rtp->rt_pri = 35;
			rtp->rt_tqsecs = 0;
			rtp->rt_tqnsecs = RT_TQDEF;

			pcp.pc_cid = pci.pc_cid;

			if (priocntl(P_LWPID, P_MYID, PC_SETPARMS,
			    (caddr_t)&pcp) == -1) {
				warn("failed to set RT parameters");
				Oflag = 0;
			}
		} else {
			warn("failed to get RT class id");
			Oflag = 0;
		}

		if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
			warn("failed to lock address space");
			Oflag = 0;
		}

		if (Oflag)
			mdb_printf("%s: oink, oink!\n", mdb.m_pname);
	}

	/*
	 * Path evaluation part 2: Re-evaluate the path now that the target
	 * is ready (and thus we have access to the real platform string).
	 * Do this before reading ~/.mdbrc to allow path modifications prior
	 * to performing module auto-loading.
	 */
	mdb_set_ipath(mdb.m_ipathstr);
	mdb_set_lpath(mdb.m_lpathstr);

	if (!Sflag && (p = getenv("HOME")) != NULL) {
		char rcpath[MAXPATHLEN];
		mdb_io_t *rc_io;
		int fd;

		(void) mdb_iob_snprintf(rcpath, MAXPATHLEN, "%s/.mdbrc", p);
		fd = open64(rcpath, O_RDONLY);

		if (fd >= 0 && (rc_io = mdb_fdio_create_named(fd, rcpath))) {
			mdb_iob_t *iob = mdb_iob_create(rc_io, MDB_IOB_RDONLY);
			mdb_iob_t *old = mdb.m_in;

			mdb.m_in = iob;
			(void) mdb_run();
			mdb.m_in = old;
		}
	}

	if (!(mdb.m_flags & MDB_FL_NOMODS))
		mdb_module_load_all();

	(void) mdb_signal_sethandler(SIGINT, int_handler, NULL);
	while ((status = mdb_run()) == MDB_ERR_ABORT)
		continue;

	terminate(status == MDB_ERR_QUIT ? 0 : 1);
	/*NOTREACHED*/
	return (0);
}
