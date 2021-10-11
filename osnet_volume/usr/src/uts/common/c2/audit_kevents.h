/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*
 * File history:
 * @(#)audit_kernel.h 2.15 92/02/29 SMI; SunOS CMW
 * @(#)audit_event.h 4.2.1.2 91/05/08 SMI; BSM Module
 *
 * Audit event numbers.
 *
 * 0			Reserved as an invalid event number.
 * 1		- 2047	Reserved for the SunOS kernel.
 * 2048		- 32767 Reserved for the SunOS TCB.
 * 32768	- 65535	Available for other super-user applications.
 */

#ifndef _BSM_AUDIT_KEVENTS_H
#define	_BSM_AUDIT_KEVENTS_H

#pragma ident	"@(#)audit_kevents.h	1.24	99/10/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	AUE_NULL		0	/* =no indir system call */
#define	AUE_EXIT		1	/* =pc exit(2) */
#define	AUE_FORK		2	/* =pc fork(2) */
#define	AUE_OPEN		3	/* =fa open(2): place holder */
#define	AUE_CREAT		4	/* =fc create(2) */
#define	AUE_LINK		5	/* =fc link(2) */
#define	AUE_UNLINK		6	/* =fd unlink(2) */
#define	AUE_EXEC		7	/* =pc exec(2) */
#define	AUE_CHDIR		8	/* =pc chdir(2) */
#define	AUE_MKNOD		9	/* =fc mknod(2) */
#define	AUE_CHMOD		10	/* =fm chmod(2) */
#define	AUE_CHOWN		11	/* =fm chown(2) */
#define	AUE_UMOUNT		12	/* =ad umount(2): old version */
#define	AUE_JUNK		13	/* non existant event */
#define	AUE_ACCESS		14	/* =fa access(2) */
#define	AUE_KILL		15	/* =pc kill(2) */
#define	AUE_STAT		16	/* =fa stat(2) */
#define	AUE_LSTAT		17	/* =fa lstat(2) */
#define	AUE_ACCT		18	/* =ad acct(2) */
#define	AUE_MCTL		19	/* =fm mctl(2) */
#define	AUE_REBOOT		20	/* =ad reboot(2) */
#define	AUE_SYMLINK		21	/* =fc symlink(2) */
#define	AUE_READLINK		22	/* =fr readlink(2) */
#define	AUE_EXECVE		23	/* =pc execve(2) */
#define	AUE_CHROOT		24	/* =pc chroot(2) */
#define	AUE_VFORK		25	/* =pc vfork(2) */
#define	AUE_SETGROUPS		26	/* =pc setgroups(2) */
#define	AUE_SETPGRP		27	/* =pc setpgrp(2) */
#define	AUE_SWAPON		28	/* =ad swapon(2) */
#define	AUE_SETHOSTNAME		29	/* =ad sethostname(2) */
#define	AUE_FCNTL		30	/* =fm fcntl(2) */
#define	AUE_SETPRIORITY		31	/* =pc setpriority(2) */
#define	AUE_CONNECT		32	/* =nt connect(2) */
#define	AUE_ACCEPT		33	/* =nt accept(2) */
#define	AUE_BIND		34	/* =nt bind(2) */
#define	AUE_SETSOCKOPT		35	/* =nt setsockopt(2) */
#define	AUE_VTRACE		36	/* =pc vtrace(2) */
#define	AUE_SETTIMEOFDAY	37	/* =ad settimeofday(2) */
#define	AUE_FCHOWN		38	/* =fm fchown(2) */
#define	AUE_FCHMOD		39	/* =fm fchmod(2) */
#define	AUE_SETREUID		40	/* =pc setreuid(2) */
#define	AUE_SETREGID		41	/* =pc setregid(2) */
#define	AUE_RENAME		42	/* =fc,fd rename(2) */
#define	AUE_TRUNCATE		43	/* =fd truncate(2) */
#define	AUE_FTRUNCATE		44	/* =fd ftruncate(2) */
#define	AUE_FLOCK		45	/* =fm flock(2) */
#define	AUE_SHUTDOWN		46	/* =nt shutdown(2) */
#define	AUE_MKDIR		47	/* =fc mkdir(2) */
#define	AUE_RMDIR		48	/* =fd rmdir(2) */
#define	AUE_UTIMES		49	/* =fm utimes(2) */
#define	AUE_ADJTIME		50	/* =ad adjtime(2) */
#define	AUE_SETRLIMIT		51	/* =ad setrlimit(2) */
#define	AUE_KILLPG		52	/* =pc killpg(2) */
#define	AUE_NFS_SVC		53	/* =ad nfs_svc(2) */
#define	AUE_STATFS		54	/* =fa statfs(2) */
#define	AUE_FSTATFS		55	/* =fa fstatfs(2) */
#define	AUE_UNMOUNT		56	/* =ad unmount(2) */
#define	AUE_ASYNC_DAEMON	57	/* =ad async_daemon(2) */
#define	AUE_NFS_GETFH		58	/* =ad nfs_getfh(2) */
#define	AUE_SETDOMAINNAME	59	/* =ad setdomainname(2) */
#define	AUE_QUOTACTL		60	/* =ad quotactl(2) */
#define	AUE_EXPORTFS		61	/* =ad exportfs(2) */
#define	AUE_MOUNT		62	/* =ad mount(2) */
#define	AUE_SEMSYS		63	/* =fa semsys(2): place holder */
#define	AUE_MSGSYS		64	/* =fa msgsys(2): place holder */
#define	AUE_SHMSYS		65	/* =fa shmsys(2): place holder */
#define	AUE_BSMSYS		66	/* =fa bsmsys(2): place holder */
#define	AUE_RFSSYS		67	/* =fa rfssys(2): place holder */
#define	AUE_FCHDIR		68	/* =pc fchdir(2) */
#define	AUE_FCHROOT		69	/* =pc fchroot(2) */
#define	AUE_VPIXSYS		70	/* =fa vpixsys(2): obsolete */
#define	AUE_PATHCONF		71	/* =fa pathconf(2) */
#define	AUE_OPEN_R		72	/* =fr open(2): read */
#define	AUE_OPEN_RC		73	/* =fc,fr open(2): read,creat */
#define	AUE_OPEN_RT		74	/* =fd,fr open(2): read,trunc */
#define	AUE_OPEN_RTC		75	/* =fc,fd,fr open(2): rd,cr,tr */
#define	AUE_OPEN_W		76	/* =fw open(2): write */
#define	AUE_OPEN_WC		77	/* =fc,fw open(2): write,creat */
#define	AUE_OPEN_WT		78	/* =fd,fw open(2): write,trunc */
#define	AUE_OPEN_WTC		79	/* =fc,fd,fw open(2): wr,cr,tr */
#define	AUE_OPEN_RW		80	/* =fr,fw open(2): read,write */
#define	AUE_OPEN_RWC		81	/* =fc,fw,fr open(2): rd,wr,cr */
#define	AUE_OPEN_RWT		82	/* =fd,fr,fw open(2): rd,wr,tr */
#define	AUE_OPEN_RWTC		83	/* =fc,fd,fw,fr open(2): rd,wr,cr,tr */
#define	AUE_MSGCTL		84	/* =ip msgctl(2): illegal command */
#define	AUE_MSGCTL_RMID		85	/* =ip msgctl(2): IPC_RMID command */
#define	AUE_MSGCTL_SET		86	/* =ip msgctl(2): IPC_SET command */
#define	AUE_MSGCTL_STAT		87	/* =ip msgctl(2): IPC_STAT command */
#define	AUE_MSGGET		88	/* =ip msgget(2) */
#define	AUE_MSGRCV		89	/* =ip msgrcv(2) */
#define	AUE_MSGSND		90	/* =ip msgsnd(2) */
#define	AUE_SHMCTL		91	/* =ip shmctl(2): Illegal command */
#define	AUE_SHMCTL_RMID		92	/* =ip shmctl(2): IPC_RMID command */
#define	AUE_SHMCTL_SET		93	/* =ip shmctl(2): IPC_SET command */
#define	AUE_SHMCTL_STAT		94	/* =ip shmctl(2): IPC_STAT command */
#define	AUE_SHMGET		95	/* =ip shmget(2) */
#define	AUE_SHMAT 		96	/* =ip shmat(2) */
#define	AUE_SHMDT 		97	/* =ip shmdt(2) */
#define	AUE_SEMCTL		98	/* =ip semctl(2): illegal command */
#define	AUE_SEMCTL_RMID		99	/* =ip semctl(2): IPC_RMID command */
#define	AUE_SEMCTL_SET		100	/* =ip semctl(2): IPC_SET command */
#define	AUE_SEMCTL_STAT		101	/* =ip semctl(2): IPC_STAT command */
#define	AUE_SEMCTL_GETNCNT	102	/* =ip semctl(2): GETNCNT command */
#define	AUE_SEMCTL_GETPID	103	/* =ip semctl(2): GETPID command */
#define	AUE_SEMCTL_GETVAL	104	/* =ip semctl(2): GETVAL command */
#define	AUE_SEMCTL_GETALL	105	/* =ip semctl(2): GETALL command */
#define	AUE_SEMCTL_GETZCNT	106	/* =ip semctl(2): GETZCNT command */
#define	AUE_SEMCTL_SETVAL	107	/* =ip semctl(2): SETVAL command */
#define	AUE_SEMCTL_SETALL	108	/* =ip semctl(2): SETALL command */
#define	AUE_SEMGET		109	/* =ip semget(2) */
#define	AUE_SEMOP		110	/* =ip semop(2) */
#define	AUE_CORE		111	/* =fc process dumped core */
#define	AUE_CLOSE		112	/* =cl close(2) */
#define	AUE_SYSTEMBOOT		113	/* =na system booted */
#define	AUE_ASYNC_DAEMON_EXIT	114	/* =ad async_daemon(2) exited */
#define	AUE_NFSSVC_EXIT		115	/* =ad nfssvc(2) exited */
#define	AUE_SETCMWPLABEL	116	/* =sl setcmwplabel(2) */
#define	AUE_SETCLEARANCE	117	/* =sl setclearance(2) */
#define	AUE_FGETCMWLABEL	118	/* =fa fgetcmwlabel(2) */
#define	AUE_FSETCMWLABEL	119	/* =sl fsetcmwlabel(2) */
#define	AUE_GETCMWFSRANGE	120	/* =fa getcmwfsrange(2) */
#define	AUE_GETCMWLABEL		121	/* =fa getcmwlabel(2) */
#define	AUE_GETFILEPRIV		122	/* =fa getfilepriv(2) */
#define	AUE_LGETCMWLABEL	123	/* =fa lgetcmwlabel(2) */
#define	AUE_MKFSO		124	/* =fc mkfso(2) */
#define	AUE_SETCMWLABEL		125	/* =sl setcmwlabel(2) */
#define	AUE_SETFILEPRIV		126	/* =sl setfilepriv(2) */
#define	AUE_SETPROCPRIV		127	/* =sl setprocpriv(2) */
#define	AUE_WRITEL		128	/* =no writel(2) */
#define	AUE_WRITEVL		129	/* =no writevl(2) */
#define	AUE_GETAUID		130	/* =ad getauid(2) */
#define	AUE_SETAUID		131	/* =ad setauid(2) */
#define	AUE_GETAUDIT		132	/* =ad getaudit(2) */
#define	AUE_SETAUDIT		133	/* =ad setaudit(2) */
#define	AUE_GETUSERAUDIT	134	/* =ad getuseraudit(2) */
#define	AUE_SETUSERAUDIT	135	/* =ad setuseraudit(2) */
#define	AUE_AUDITSVC		136	/* =ad auditsvc(2) */
#define	AUE_AUDITUSER		137	/* =ad audituser(2) */
#define	AUE_AUDITON		138	/* =ad auditon(2) */
#define	AUE_AUDITON_GTERMID	139	/* =ad auditctl(2): GETTERMID */
#define	AUE_AUDITON_STERMID	140	/* =ad auditctl(2): SETTERMID */
#define	AUE_AUDITON_GPOLICY	141	/* =ad auditctl(2): GETPOLICY */
#define	AUE_AUDITON_SPOLICY	142	/* =ad auditctl(2): SETPOLICY */
#define	AUE_AUDITON_GESTATE	143	/* =ad auditctl(2): GETESTATE */
#define	AUE_AUDITON_SESTATE	144	/* =ad auditctl(2): SETESTATE */
#define	AUE_AUDITON_GQCTRL	145	/* =ad auditctl(2): GETQCTRL */
#define	AUE_AUDITON_SQCTRL	146	/* =ad auditctl(2): SETQCTRL */
#define	AUE_GETKERNSTATE	147	/* =ad getkernstate(2) */
#define	AUE_SETKERNSTATE	148	/* =ad setkernstate(2) */
#define	AUE_GETPORTAUDIT	149	/* =ad getportaudit(2) */
#define	AUE_AUDITSTAT		150	/* =ad auditstat(2) */
#define	AUE_REVOKE		151	/* =fm revoke(2) */
#define	AUE_MAC			152	/* =ma MAC use */
#define	AUE_ENTERPROM		153	/* =na enter prom */
#define	AUE_EXITPROM		154	/* =na exit prom */
#define	AUE_IFLOAT		155	/* =il inode IL float */
#define	AUE_PFLOAT		156	/* =il process IL float */
#define	AUE_UPRIV		157	/* =pr privilege use */
#define	AUE_IOCTL		158	/* =io ioctl(2) */
#define	AUE_FIND_RH		159	/* =na ipintr: pkt from unknown host */
#define	AUE_BADSATTR		160	/* =na ipintr: unknown security attr */
#define	AUE_TN_GEN		161	/* =na ipintr: out-of-sync generat */
#define	AUE_TFRWRD		162	/* =na ipintr: bad forward route */
#define	AUE_TN_BYPASS		163	/* =na ipintr: bypassed security */
#define	AUE_TN_ISPRIV		164	/* =na ipintr: insufficient privilege */
#define	AUE_TN_CKRT		165	/* =na ipintr: route security reject */
#define	AUE_TN_CKIPOUT		166	/* =na ipintr: ip outpt securty rjct */
#define	AUE_KTNETD		167	/* =nt tnetd turned off */
#define	AUE_STNETD		168	/* =nt tnetd started */
#define	AUE_HLTSR		169	/* =nt session record halted */
#define	AUE_STRTSR		170	/* =nt session record started */
#define	AUE_FREESR		171	/* =nt session record freed */
#define	AUE_TN_ACCRED		172	/* =nt import accred failed */
#define	AUE_ONESIDE		173	/* =nt one-sided session record */
#define	AUE_MSGGETL		174	/* =ip msggetl(2) */
#define	AUE_MSGRCVL		175	/* =ip msgrcvl(2) */
#define	AUE_MSGSNDL		176	/* =ip msgsndl(2) */
#define	AUE_SEMGETL		177	/* =ip semgetl(2) */
#define	AUE_SHMGETL		178	/* =ip shmgetl(2) */
#define	AUE_GETMLDADORN		179	/* =fa getmldadorn(2) */
#define	AUE_GETSLDNAME		180	/* =fa getsldname(2) */
#define	AUE_MLDLSTAT		181	/* =fa mldlstat(2) */
#define	AUE_MLDSTAT		182	/* =fa mldstat(2) */
#define	AUE_SOCKET		183	/* =no socket(2) */
#define	AUE_SENDTO		184	/* =nt sendto(2) */
#define	AUE_PIPE		185	/* =no pipe(2) */
#define	AUE_SOCKETPAIR		186	/* =no socketpair(2) */
#define	AUE_SEND		187	/* =no send(2) */
#define	AUE_SENDMSG		188	/* =nt sendmsg(2) */
#define	AUE_RECV		189	/* =no recv(2) */
#define	AUE_RECVMSG		190	/* =nt recvmsg(2) */
#define	AUE_RECVFROM		191	/* =nt recvfrom(2) */
#define	AUE_READ		192	/* =no read(2) */
#define	AUE_GETDENTS		193	/* =no getdents(2) */
#define	AUE_LSEEK		194	/* =no lseek(2) */
#define	AUE_WRITE		195	/* =no write(2) */
#define	AUE_WRITEV		196	/* =no writev(2) */
#define	AUE_NFS			197	/* =no NFS server */
#define	AUE_READV		198	/* =no readv(2) */
#define	AUE_OSTAT		199	/* =fa old stat(2) */
#define	AUE_SETUID		200	/* =pc old setuid(2) */
#define	AUE_STIME		201	/* =ad old stime(2) */
#define	AUE_UTIME		202	/* =fm old utime(2) */
#define	AUE_NICE		203	/* =pc old nice(2) */
#define	AUE_OSETPGRP		204	/* =pc old setpgrp(2) */
#define	AUE_SETGID		205	/* =pc old setgid(2) */
#define	AUE_READL		206	/* =no readl(2) */
#define	AUE_READVL		207	/* =no readvl(2) */
#define	AUE_FSTAT		208	/* =no fstat(2) */
#define	AUE_DUP2		209	/* =no dup2(2) u-o-p */
#define	AUE_MMAP		210	/* =fr,fw mmap(2) u-o-p */
#define	AUE_AUDIT		211	/* =no audit(2) u-o-p */
#define	AUE_PRIOCNTLSYS		212	/* =pc priocntlsys */
#define	AUE_MUNMAP		213	/* =cl munmap(2) u-o-p */
#define	AUE_SETEGID		214	/* =pc setegid(2) */
#define	AUE_SETEUID		215	/* =pc seteuid(2) */
#define	AUE_PUTMSG		216	/* =nt */
#define	AUE_GETMSG		217	/* =nt */
#define	AUE_PUTPMSG		218	/* =nt */
#define	AUE_GETPMSG		219	/* =nt */
#define	AUE_AUDITSYS		220	/* =fa place holder */
#define	AUE_AUDITON_GETKMASK	221	/* =ad */
#define	AUE_AUDITON_SETKMASK	222	/* =ad */
#define	AUE_AUDITON_GETCWD	223	/* =ad */
#define	AUE_AUDITON_GETCAR	224	/* =ad */
#define	AUE_AUDITON_GETSTAT	225	/* =ad */
#define	AUE_AUDITON_SETSTAT	226	/* =ad */
#define	AUE_AUDITON_SETUMASK	227	/* =ad */
#define	AUE_AUDITON_SETSMASK	228	/* =ad */
#define	AUE_AUDITON_GETCOND	229	/* =ad */
#define	AUE_AUDITON_SETCOND	230	/* =ad */
#define	AUE_AUDITON_GETCLASS	231	/* =ad */
#define	AUE_AUDITON_SETCLASS	232	/* =ad */
#define	AUE_FUSERS		233	/* =fa */
#define	AUE_STATVFS		234	/* =fa */
#define	AUE_XSTAT		235	/* =fa */
#define	AUE_LXSTAT		236	/* =fa */
#define	AUE_LCHOWN		237	/* =fm */
#define	AUE_MEMCNTL		238	/* =pr */
#define	AUE_SYSINFO		239	/* =ad */
#define	AUE_XMKNOD		240	/* =fc */
#define	AUE_FORK1		241	/* =pc */
#define	AUE_MODCTL		242	/* =ad */
#define	AUE_MODLOAD		243	/* =ad */
#define	AUE_MODUNLOAD		244	/* =ad */
#define	AUE_MODCONFIG		245	/* =ad */
#define	AUE_MODADDMAJ		246	/* =ad */
#define	AUE_SOCKACCEPT		247	/* =nt */
#define	AUE_SOCKCONNECT		248	/* =nt */
#define	AUE_SOCKSEND		249	/* =nt */
#define	AUE_SOCKRECEIVE		250	/* =nt */
#define	AUE_ACLSET		251	/* =fa */
#define	AUE_FACLSET		252	/* =fa */
#define	AUE_DOORFS		253	/* =ip */
#define	AUE_DOORFS_DOOR_CALL	254	/* =ip */
#define	AUE_DOORFS_DOOR_RETURN	255	/* =ip */
#define	AUE_DOORFS_DOOR_CREATE	256	/* =ip */
#define	AUE_DOORFS_DOOR_REVOKE	257	/* =ip */
#define	AUE_DOORFS_DOOR_INFO	258	/* =ip */
#define	AUE_DOORFS_DOOR_CRED	259	/* =ip */
#define	AUE_DOORFS_DOOR_BIND	260	/* =ip */
#define	AUE_DOORFS_DOOR_UNBIND	261	/* =ip */
#define	AUE_P_ONLINE		262	/* =ad */
#define	AUE_PROCESSOR_BIND	263	/* =ad */
#define	AUE_INST_SYNC		264	/* =ad */
#define	AUE_SOCKCONFIG		265	/* =nt */
#define	AUE_SETAUDIT_ADDR	266	/* =ad setaudit_addr(2) */
#define	AUE_GETAUDIT_ADDR	267	/* =ad getaudit_addr(2) */

/*
 * Maximum number of kernel events in the event to class table
 * leave a couple extra ones just incase somebody wants to load a new
 * driver with build in auditing
 */

#define	MAX_KEVENTS		512

#ifdef __cplusplus
}
#endif

#endif /* _BSM_AUDIT_KEVENTS_H */
