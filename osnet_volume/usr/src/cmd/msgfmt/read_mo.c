#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
main(argc, argv)
        int argc;
        char *argv[];
{
	struct struct_mo_info {
		int             message_mid;
		int             message_count;
		int             string_count_msgid;
		int             string_count_msg;
		int             message_struct_size;
	}              *mo_info;
	struct message_struct {
		int             less;
		int             more;
		int             msgid_offset;
		int             msg_offset;
	}              *message_list;
	char		file[512];
	char           *msg_ids;
	char           *msgs;
	struct stat     sb;
	caddr_t         mmap();
	void		doprint();
	void           *addr;
	int             fd = -1;
	int             i;
	char		lflg = 0;

        argc--, argv++;
        while (argc > 0 && argv[0][0] == '-') {
                register char *cp = &(*argv++)[1];
 
                argc--;
                if (*cp == 0) {
                        continue;
                }
                do switch (*cp++) {
 
                case 'l':
                        lflg = 1;
                        continue;
 
                } while (*cp);
        }

	if (argc != 1) {
		perror("usage: read_mo filename\n");
		exit(2);
	}

	(void)strcpy(file,argv[0]);
	if ((fd = open(file, O_RDONLY)) != -1 &&
			fstat(fd, &sb) != -1) {
		addr = (void *) mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	} else {
		perror("Can't open file ");
		exit(2);
	}
	mo_info = (struct struct_mo_info *) addr;
	message_list = (struct message_struct *) & mo_info[1];
	msg_ids = (char *) &message_list[mo_info->message_count];
	msgs = (char *) msg_ids + mo_info->string_count_msgid;

	if (lflg) {
		(void)printf("message mid = %d\n",mo_info->message_mid);
	}
	(void)printf("domain \"%s\"\n",file);
	for (i =  mo_info->message_count-1;i>=0; --i) {
		if (lflg) {
			(void)printf("\n");
			(void)printf("Message %d \n", i);
			(void)printf("%d\n", (message_list[i].less));
			(void)printf("%d\n", (message_list[i].more));
		}
		(void)doprint(1, (msg_ids + message_list[i].msgid_offset));
		(void)doprint(0, (msgs + message_list[i].msg_offset));
	}
}

void
doprint(type, s)
int type; /* 1 for message id, 0 for message */
char *s;
{
	char *ch;

	if (type) {
		printf("msgid \"");
	} else {
		printf("msgstr \"");
	}
	ch = s;

	while (*ch) {
		switch(*ch) {
			case '\n':
				printf("\\n");
				ch++;
				break;
			case '\t':
				printf("\\t");
				ch++;
				break;
			case '\v':
				printf("\\v");
				ch++;
				break;
			case '\b':
				printf("\\b");
				ch++;
				break;
			case '\r':
				printf("\\r");
				ch++;
				break;
			case '\f':
				printf("\\f");
				ch++;
				break;
			case '\\':
				printf("\\\\");
				ch++;
				break;
			case '\"':
				printf("\\\"");
				ch++;
				break;
		default :
			printf("%c",*ch);
			ch++;
		}
	}
	printf("\"\n");
}
