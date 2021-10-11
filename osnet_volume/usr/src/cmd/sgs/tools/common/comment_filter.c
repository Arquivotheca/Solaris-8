#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

/*
 * Simple program which will read in files given on the command line
 * and strip all comments out of them.  It will then dump all of the output
 * to stdout.
 *
 */

void
comment_filter(char * str, int size)
{
	int i;
	int inside_comment = 0;
	int transition = 1;
	char prev_char, cur_char;
	if (size == 0)
		return;

	for (i = 0; i < size; i++) {
		cur_char = str[i];

		if (transition) {
			prev_char = cur_char;
			transition = 0;
			continue;
		}

		if (!inside_comment) {
			if ((prev_char == '/') && (cur_char == '*')) {
				inside_comment = 1;
				transition++;
			} else {
				putchar(prev_char);
			}
		} else {
			if ((prev_char == '*') && (cur_char == '/')) {
				inside_comment = 0;
				transition++;
			}
		}
		prev_char = cur_char;
	}
}

/*
 * comment # 2
 */

main(int argc, char ** argv) /* comment #4 */
{
	int i;

	for (i = 1; i < argc; i++) {
		int		fd;
		struct stat	buf;
		caddr_t		file_buf;

		if ((fd = open(argv[i], O_RDONLY)) == -1) {
			perror("open");
			continue;
		}
		if (fstat(fd, &buf) == -1) {
			perror("fstat");
			close(fd);
			continue;
		}
		if ((file_buf = mmap(0, buf.st_size, PROT_READ,
		    MAP_SHARED, fd, 0)) == MAP_FAILED) {
			perror("mmap");
			close(fd);
			continue;
		}
		close(fd);
		comment_filter(file_buf, buf.st_size);
		munmap(file_buf, buf.st_size);
	}
}
