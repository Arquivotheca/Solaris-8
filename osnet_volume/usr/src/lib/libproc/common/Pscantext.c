/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pscantext.c	1.3	99/05/04 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "Pcontrol.h"
#include "Putil.h"

#define	BLKSIZE	(8 * 1024)

#if defined(__i386) || defined(__ia64)
#define	LCALL	0x9a
#define	PCADJ	7
static syscall_t old_lcall = { 0x9a, 0, 0, 0, 0, 0x7, 0 };
static syscall_t new_lcall = { 0x9a, 0, 0, 0, 0, 0x27, 0 };
#elif sparc
#define	PCADJ	0
#endif

/*
 * Look for a SYSCALL instruction in the process's address space.
 */
int
Pscantext(struct ps_prochandle *P)
{
	char mapfile[100];
	int mapfd;
#if defined(__i386) || defined(__ia64)
	unsigned char *p;	/* pointer into buf */
	long osysaddr = 0;	/* address of old SYSCALL instruction */
#elif sparc
	uint32_t *p;		/* pointer into buf */
#endif
	off_t offset;		/* offset in text section */
	off_t endoff;		/* ending offset in text section */
	uintptr_t sysaddr;	/* address of SYSCALL instruction */
	int nbytes;		/* number of bytes in buffer */
	int n2bytes;		/* number of bytes in second buffer */
	int nmappings;		/* current number of mappings */
	syscall_t instr;	/* instruction from process */
	prmap_t *pdp;		/* pointer to map descriptor */
	prmap_t *prbuf;		/* buffer for map descriptors */
	unsigned nmap;		/* number of map descriptors */
	uint32_t buf[2 * BLKSIZE / sizeof (uint32_t)];	/* text buffer */
#if sparc
	syscall_t sysinstr;	/* syscall instruction we are looking for */

	if (P->status.pr_dmodel == PR_MODEL_LP64)
		sysinstr = (syscall_t)SYSCALL64;
	else
		sysinstr = (syscall_t)SYSCALL32;
#endif

	/* try the most recently-seen syscall address */
	if ((sysaddr = P->sysaddr) != 0) {
#if defined(__i386) || defined(__ia64)
		if (Pread(P, instr, sizeof (instr), sysaddr)
		    != sizeof (instr))
			sysaddr = 0;
		else if (memcmp(instr, old_lcall, sizeof (old_lcall)) == 0) {
			osysaddr = sysaddr;
			sysaddr = 0;
		} else if (memcmp(instr, new_lcall, sizeof (new_lcall)) != 0) {
			sysaddr = 0;
		}
#else
		if (Pread(P, &instr, sizeof (instr), sysaddr)
		    != sizeof (instr) || instr != sysinstr)
			sysaddr = 0;
#endif
	}

	/* try the current PC minus sizeof (syscall) */
	if (sysaddr == 0 && (sysaddr = P->REG[R_PC]-PCADJ) != 0) {
#if defined(__i386) || defined(__ia64)
		if (Pread(P, instr, sizeof (instr), sysaddr)
		    != sizeof (instr))
			sysaddr = 0;
		else if (memcmp(instr, old_lcall, sizeof (old_lcall)) == 0) {
			osysaddr = sysaddr;
			sysaddr = 0;
		} else if (memcmp(instr, new_lcall, sizeof (new_lcall)) != 0) {
			sysaddr = 0;
		}
#else
		if (Pread(P, &instr, sizeof (instr), sysaddr)
		    != sizeof (instr) || instr != sysinstr)
			sysaddr = 0;
#endif
	}

	if (sysaddr) {		/* we have the address of a SYSCALL */
		P->sysaddr = sysaddr;
		return (0);
	}

	P->sysaddr = 0;	/* assume failure */

	/* open the /proc/<pid>/map file */
	(void) sprintf(mapfile, "/proc/%d/map", (int)P->pid);
	if ((mapfd = open(mapfile, O_RDONLY)) < 0) {
		dprintf("failed to open %s: %s\n", mapfile, strerror(errno));
		return (-1);
	}

	/* allocate a plausible initial buffer size */
	nmap = 50;

	/* read all the map structures, allocating more space as needed */
	for (;;) {
		prbuf = malloc(nmap * sizeof (prmap_t));
		if (prbuf == NULL) {
			dprintf("Pscantext: failed to allocate buffer\n");
			(void) close(mapfd);
			return (-1);
		}
		nmappings = pread(mapfd, prbuf, nmap * sizeof (prmap_t), 0L);
		if (nmappings < 0) {
			dprintf("Pscantext: failed to read map file: %s\n",
			    strerror(errno));
			free(prbuf);
			(void) close(mapfd);
			return (-1);
		}
		nmappings /= sizeof (prmap_t);
		if (nmappings < nmap)	/* we read them all */
			break;
		/* allocate a bigger buffer */
		free(prbuf);
		nmap *= 2;
	}
	(void) close(mapfd);

	/* null out the entry following the last valid entry */
	(void) memset((char *)&prbuf[nmappings], 0, sizeof (prmap_t));

	/* scan the mappings looking for an executable mappings */
	for (pdp = &prbuf[0]; sysaddr == 0 && pdp->pr_size; pdp++) {

		offset = (off_t)pdp->pr_vaddr;	/* beginning of text */
		endoff = offset + pdp->pr_size;

		/* avoid non-EXEC mappings; avoid the stack and heap */
		if ((pdp->pr_mflags&MA_EXEC) == 0 ||
		    (endoff > P->status.pr_stkbase &&
		    offset < P->status.pr_stkbase + P->status.pr_stksize) ||
		    (endoff > P->status.pr_brkbase &&
		    offset < P->status.pr_brkbase + P->status.pr_brksize))
			continue;

		(void) lseek(P->asfd, (off_t)offset, 0);

		if ((nbytes = read(P->asfd, buf, 2*BLKSIZE)) <= 0)
			continue;

		if (nbytes < BLKSIZE)
			n2bytes = 0;
		else {
			n2bytes = nbytes - BLKSIZE;
			nbytes  = BLKSIZE;
		}
#if defined(__i386) || defined(__ia64)
		p = (unsigned char *)buf;
#elif sparc
		p = (uint32_t *)buf;
#endif
		/* search text for a SYSCALL instruction */
		while (sysaddr == 0 && offset < endoff) {
			if (nbytes <= 0) {	/* shift buffers */
				if ((nbytes = n2bytes) <= 0)
					break;
				(void) memcpy(buf,
					&buf[BLKSIZE / sizeof (buf[0])],
					nbytes);
#if defined(__i386) || defined(__ia64)
				p = (unsigned char *)buf;
#elif sparc
				p = (uint32_t *)buf;
#endif
				n2bytes = 0;
				if (nbytes == BLKSIZE &&
				    offset + BLKSIZE < endoff)
					n2bytes = read(P->asfd,
						&buf[BLKSIZE / sizeof (buf[0])],
						BLKSIZE);
			}
#if defined(__i386) || defined(__ia64)
			if (*p == LCALL) {
				if (memcmp(p, old_lcall, sizeof (old_lcall))
				    == 0)
					osysaddr = offset;
				if (memcmp(p, new_lcall, sizeof (new_lcall))
				    == 0)
					sysaddr = offset;
			}
			p++;
			offset++;
			nbytes--;
#elif sparc
			if (*p++ == sysinstr)
				sysaddr = offset;
			offset += sizeof (instr_t);
			nbytes -= sizeof (instr_t);
#endif
		}
	}

	free(prbuf);
#if defined(__i386) || defined(__ia64)
	/* if we failed to find a new syscall, use an old one, if any */
	if (sysaddr == 0)
		sysaddr = osysaddr;
#endif
	P->sysaddr = sysaddr;
	return (sysaddr? 0 : -1);
}
