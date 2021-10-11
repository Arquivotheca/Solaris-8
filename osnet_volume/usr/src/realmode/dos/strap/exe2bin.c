#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <io.h>

typedef unsigned short word;

void usage(char *progname);

struct exehdr {
        char            sig[2]; /* EXE file signature 4D+5A(h) */
        word            excess; /* Image size mod 512 (bytes in last* page) */
        word            pages;  /* # 512-byte pages in image */
        word            relo_ct;/* Count of relocation table entries */
        word            hdr_size;       /* Size of header, in paragraphs */
        word            min_mem;/* Min required memory */
        word            max_mem;/* Max required memory */
        word            ss;     /* Stack seg offset in load module */
        word            sp;     /* Initial value of SP */
        word            cksum;  /* File checksum */
        word            ip;     /* Initial value of IP */
        word            cs;     /* CS offset in load module */
        word            relo_start;     /* Offset of first relo item */
        word            ovl_num;/* Overlay number */
};

unsigned char buf[8192];

main(int argc, char **argv)
{
        char in_file[129];      /* Input file name */
        char out_file[129];     /* Output file name */
        int infd, outfd;        /* input/output file handles */

        unsigned long code_start; /* Offset of program image in EXE file */
        unsigned long code_size;  /* Size of program image, in bytes */
        struct exehdr hdr;        /* the header itself */

        unsigned size;          /* size of buf to read and write */
        int strip = 0;          /* strip code before entry point? */
	int doonce = 1;
	unsigned int *ip;
        char *p;
        char *progname;

        p = strrchr(strlwr(*argv), '\\');
        if (p) 
                progname = p+1;
        else
                progname = *argv;
        
        argc--; argv++;
        
        if (argc < 1) {
                usage(progname);        
                exit(1);
        }
        
        /* process switches */
        while (*argv && **argv == '-') {
                if (strcmp(strlwr(*argv), "-strip_to_entry") == 0)
                        strip = 1;
                else {
                        fprintf(stderr, "unknown option %s\n", *argv);
                        usage(progname);        
                        exit(1);
                }
                argc--; argv++;
        }

        strcpy(in_file, strlwr(*argv));
        argc--; argv++;
        if (!strchr(in_file, '.'))
                strcat(in_file, ".exe");
        if (argc)
                strcpy(out_file, strlwr(*argv));
        else
                strcpy(out_file, in_file);
        argc--; argv++;
        p = strchr(out_file, '.');
        if (p != NULL && strcmp(p, ".exe") == 0)
                strcpy(p, ".com");

        infd = open(in_file, O_RDONLY|O_BINARY);
        outfd = open(out_file, O_RDWR|O_CREAT|O_TRUNC|O_BINARY,
                        S_IREAD|S_IWRITE);
        if (read(infd, &hdr, sizeof(hdr)) <= 0) {
                fprintf(stderr, "Can't read .EXE hdr\n");
                exit(1);
        }

        /* Check .EXE and die or warn about anomalies */
        if (strncmp(hdr.sig, "MZ", 2)) {
                fprintf(stderr, "No .EXE signature\n;");
                exit(1);
        }
        if (hdr.relo_ct)
                fprintf(stderr, "Warning: .EXE has relocation items\n");
        if (hdr.ss || hdr.sp)
                fprintf(stderr, "Warning: .EXE has SS:SP defined\n");
        if (hdr.ip != 0) {
                if (strip)
                        fprintf(stderr, "Warning: entry point not 0, strip will lose data\n");
                else
                        fprintf(stderr, "Warning: entry point not 0\n");
        }

        /* hdr_size is in paragraphs */
        code_start = ((unsigned long) hdr.hdr_size) << 4;

        /* 
         * hdr.pages is exact file size if hdr.excess is 0, else the 
         * last page is only partly filled (so don't add the whole page)
         */
        if (hdr.excess == 0)
                code_size = (unsigned long) hdr.pages * 512 - code_start;
        else
                code_size = (unsigned long)(hdr.pages - 1) * 512 +
                        hdr.excess - code_start;

        if (strip) {
                /* Copy from entry point to output */
                lseek(infd, code_start+hdr.ip, 0);
                code_size -= hdr.ip;
        } else {
                /* Copy from code_start to output */
                lseek(infd, code_start, 0);
        }

        while (code_size) {
                size = min(sizeof(buf), code_size); 
                if (read(infd, buf, size) <= 0) {
                        fprintf(stderr, "Read error on input\n");
                        exit(1);
                }
		if (doonce) {
			doonce = 0;
			if (buf[3] == 'M' && buf[4] == 'D' && buf[5] == 'B' &&
			    buf[6] == 'X') {
				fprintf(stderr, "Installing size 0x%x\n", code_size);
				ip = (unsigned int *)&buf[9];
				*ip = code_size;
			}
		}
                if (write(outfd, buf, size) <= 0) {
                        fprintf(stderr, "Write error on output\n");
                        exit(1);
                }
                code_size -= size;
        }

        close(infd);
        close(outfd);
}

void usage(char *progname)
{
        fprintf(stderr, "usage: %s [ -strip_to_entry ] <exefn> [ outfn ]\n", progname);
        fprintf(stderr, " exefn is .EXE file to get load image from\n");
        fprintf(stderr, " outfn is file to write to (defaults to exefn with .BIN extension)\n");
        fprintf(stderr, " -strip_to_entry causes code before .EXE entry point to be ignored\n");
}
