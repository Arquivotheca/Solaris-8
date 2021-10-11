/*
 * Copyright (c) 1992-1994, by Sun Microsystems, Inc
 */

#pragma	ident	"@(#)mkboot.c	1.5	94/09/28 SMI"

#include <sys/exechdr.h>
#include <sys/elf.h>
#include <sys/fcntl.h>
#include <stdio.h>

#define	BBSIZE	(15 * 512)

Elf32_Ehdr elfhdr;
Elf32_Phdr phdr, dphdr;
char buf[4096];

int
main(int argc, char **argv)
{
	struct exec exec;
	int ifd, ofd;
	int count;
	int text_written = 0;
	int data_written = 0;

	if (argc < 3) {
		printf("usage: %s elf_file a.outfile\n", argv[0]);
		exit(1);
	}
	if ((ifd = open(argv[1], O_RDONLY)) ==  -1) {
		perror("open input");
		exit(1);
	}
	if (read(ifd, &elfhdr, sizeof (elfhdr)) < sizeof (elfhdr)) {
		perror("read elfhdr");
		exit(1);
	}
	if ((ofd = open(argv[2], O_RDWR | O_TRUNC | O_CREAT, 0777)) ==  -1) {
		perror("open aout");
		exit(1);
	}

	if (elfhdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    elfhdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    elfhdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    elfhdr.e_ident[EI_MAG3] != ELFMAG3) {
		perror("elfmagic");
		exit(1);
	}
	if (lseek(ifd, elfhdr.e_phoff, 0) == -1) {
		perror("lseek");
		exit(1);
	}
	if (read(ifd, &phdr, sizeof (phdr)) < sizeof (phdr)) {
		perror("read phdr");
		exit(1);
	}
	if (read(ifd, &dphdr, sizeof (dphdr)) < sizeof (dphdr)) {
		perror("read dphdr");
		exit(1);
	}

	/*
	 * Create an a.out header that will fool the PROM
	 */
	exec.a_toolversion = 1;
#ifdef sparc
	exec.a_machtype = M_SPARC;
#else
#error	"unknown machine type!"
#endif
	exec.a_magic = OMAGIC;
	exec.a_text = dphdr.p_vaddr - phdr.p_vaddr;
	exec.a_data = dphdr.p_filesz;
	exec.a_bss = dphdr.p_memsz - dphdr.p_filesz;
	exec.a_entry = elfhdr.e_entry;

#define	text	exec.a_text
#define	data	exec.a_data
#define	bss	exec.a_bss

	if (write(ofd, &exec, sizeof (exec)) != sizeof (exec)) {
		perror("write exec");
		exit(1);
	}

	/*
	 * Text section
	 */
	printf("%d", text);
	if (lseek(ifd, phdr.p_offset, 0) == -1) {
		perror("lseek text");
		exit(1);
	}
	while (text_written < text &&
	    (count = read(ifd, buf, sizeof (buf))) > 0) {
		if (count > exec.a_text - text_written)
			count = (exec.a_text - text_written);
		if (write(ofd, buf, count) < count) {
			perror("write text file");
			exit(1);
		}
		text_written += count;
	}
	/* Include a.out header in the accounting */
	text_written += sizeof (exec);

	/*
	 * Data section
	 */
	if (lseek(ifd, dphdr.p_offset, 0) == -1) {
		perror("lseek data");
		exit(1);
	}
	printf(" + %d (%d/0x%x)", data, text+data, text+data);
	while (data_written < data &&
	    (count = read(ifd, buf, sizeof (buf))) > 0) {
		if (count > data - data_written)
			count = (data - data_written);
		if (write(ofd, buf, count) < count) {
			perror("write data file");
			exit(1);
		}
		data_written += count;
	}
	printf(" + %d (%d/0x%x)\n", bss, text+data+bss, text+data+bss);
	(void) close(ofd);
	(void) close(ifd);

	if (text_written + data_written > BBSIZE) {
		printf("*** WARNING! ***\nDo not install this bootblock!\n");
		printf("bootblock image is %d bytes too big.\n",
		    (text_written + data_written) - BBSIZE);
		exit(1);
	}

	printf("bootblock %d%% full - %d bytes to spare.\n",
	    (text_written + data_written) * 100 / BBSIZE,
	    BBSIZE - (text_written + data_written));
	exit(0);
}
