/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * File name: praudit.h
 * praudit.c defines, globals
 */

#ifndef	_PRAUDIT_H
#define	_PRAUDIT_H

#pragma ident	"@(#)praudit.h	1.22	99/12/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/* DEFINES */

/*
 * output value types
 */
#define	PRA_INT32 0
#define	PRA_UINT32 1
#define	PRA_INT64 2
#define	PRA_UINT64 3
#define	PRA_SHORT 4
#define	PRA_USHORT 5
#define	PRA_CHAR 6
#define	PRA_UCHAR 7
#define	PRA_STRING 8
#define	PRA_HEX32 9
#define	PRA_HEX64 10
#define	PRA_SHEX 11
#define	PRA_OCT 12
#define	PRA_BYTE 13
#define	PRA_OUTREC 14
#define	PRA_LOCT 15

/*
 * output mode - set with values of command line flags
 */
#define	DEFAULTM 0
#define	RAWM  1
#define	SHORTM  2

/*
 * source of audit file names
 */
#define	PIPEMODE 0
#define	FILEMODE 1

/*
 * max. number of audit file names entered on command line
 */
#define	MAXFILENAMES 100

/*
 * max. size of file name
 */
#define	MAXFILELEN MAXPATHLEN+MAXNAMLEN+1

/*
 * GLOBALS
 */

/*
 * indicates source of audit file names
 */
static int	file_mode;

/*
 * initial value for output mode
 */
static int	format = DEFAULTM;

/*
 * output value type
 */
static int	uvaltype;

/*
 * one line output or formatted output
 */
static int	ONELINE = 0;

/*
 * field seperator for one line per record output
 */
#define	SEP_SIZE 4
static char	SEPARATOR[SEP_SIZE] = ",";

/*
 * token id for new praudit record format
 */
static signed char	tokenid = -1;

/*
 * do we cache the entire event file?
 */
static int	CACHE = 0;

/*
 * used to store value to be output
 */
static union u_tag {
	int32_t		int32_val;
	uint32_t	uint32_val;
	int64_t		int64_val;
	uint64_t	uint64_val;
	short		short_val;
	u_short		ushort_val;
	char		char_val;
	char		uchar_val;
	char		*string_val;
} uval;

/*
 * pointer to audit file
 */

static adr_t *audit_adr;

/*
 * ---------------------------------------------------------------
 *	Table of token names and praudit routines corresponding to
 *	each token type
 * ---------------------------------------------------------------
 */

static int	pa_header32_token();
static int	pa_header64_token();
static int	pa_header32_token_ex();
static int	pa_header64_token_ex();
static int	pa_exit_token();
static int	pa_file32_token();
static int	pa_file64_token();
static int	pa_trailer_token();
static int	pa_arbitrary_data_token();
static int	pa_SystemV_IPC_token();
static int	pa_path_token();
static int	pa_subject32_token();
static int	pa_subject64_token();
static int	pa_subject32_token_ex();
static int	pa_subject64_token_ex();
static int	pa_server_token();
static int	pa_process32_token();
static int	pa_process64_token();
static int	pa_process32_token_ex();
static int	pa_process64_token_ex();
static int	pa_return32_token();
static int	pa_return64_token();
static int	pa_text_token();
static int	pa_opaque_token();
static int	pa_ip_addr();
static int	pa_ip_addr_ex();
static int	pa_ip();
static int	pa_iport();
static int	pa_attribute_token();
static int	pa_attribute32_token();
static int	pa_attribute64_token();
static int	pa_acl_token();
static int	pa_SystemV_IPC_perm_token();
static int	pa_group_token();
static int	pa_newgroup_token();
static int	pa_arg32_token();
static int	pa_arg64_token();
static int	pa_socket_token();
static int	pa_socket_ex_token();
static int	pa_sequence_token();
static int	pa_exec_token();
static int	pa_cmd_token();

static struct tokentable {
	int	tokenid;		/* token id */
	char	*tokentype;	/* token type, in ASCII */
	int	(*func)();		/* pointer to function call */
};

/*
 * Note to C2/audit developers:
 *	If you change the tokentab, you will also have to update the
 *	structures.po file, in order to keep the internationalization
 *	tables up to date.  The name of each audit token type will
 *	be translated by the gettext() call in pa_gettokenstring().
 */

static struct tokentable tokentab[] = {
		{ AUT_ARG32, "argument", pa_arg32_token		},
		{ AUT_ARG64, "argument", pa_arg64_token		},
		{ AUT_ACL, "acl", pa_acl_token			},
		{ AUT_ATTR, "attribute", pa_attribute_token	},
		{ AUT_ATTR32, "attribute", pa_attribute32_token	},
		{ AUT_ATTR64, "attribute", pa_attribute64_token	},
		{ AUT_CMD, "cmd", pa_cmd_token			},
		{ AUT_DATA, "arbitrary", pa_arbitrary_data_token},
		{ AUT_EXEC_ARGS, "exec_args", pa_exec_token 	},
		{ AUT_EXEC_ENV, "exec_env", pa_exec_token	},
		{ AUT_EXIT, "exit", pa_exit_token		},
		{ AUT_GROUPS, "group", pa_group_token		},
		{ AUT_HEADER32, "header", pa_header32_token	},
		{ AUT_HEADER64, "header", pa_header64_token	},
		{ AUT_HEADER32_EX, "header", pa_header32_token_ex	},
		{ AUT_HEADER64_EX, "header", pa_header64_token_ex	},
		{ AUT_IN_ADDR, "ip address", pa_ip_addr		},
		{ AUT_IN_ADDR_EX, "ip address", pa_ip_addr_ex	},
		{ AUT_IP, "ip", pa_ip				},
		{ AUT_IPC, "IPC", pa_SystemV_IPC_token		},
		{ AUT_IPC_PERM, "IPC_perm", pa_SystemV_IPC_perm_token },
		{ AUT_IPORT, "ip port", pa_iport		},
		{ AUT_NEWGROUPS, "group", pa_newgroup_token	},
		{ AUT_OPAQUE, "opaque", pa_opaque_token		},
		{ AUT_OTHER_FILE32, "file", pa_file32_token	},
		{ AUT_OTHER_FILE64, "file", pa_file64_token	},
		{ AUT_PATH, "path", pa_path_token		},
		{ AUT_PROCESS32, "process", pa_process32_token	},
		{ AUT_PROCESS64, "process", pa_process64_token	},
		{ AUT_PROCESS32_EX, "process", pa_process32_token_ex	},
		{ AUT_PROCESS64_EX, "process", pa_process64_token_ex	},
		{ AUT_RETURN32, "return", pa_return32_token	},
		{ AUT_RETURN64, "return", pa_return64_token	},
		{ AUT_SEQ, "sequence", pa_sequence_token	},
		{ AUT_SERVER32, "server", pa_server_token	},
		{ AUT_SERVER64, "server", pa_server_token	},
		{ AUT_SOCKET, "socket", pa_socket_token		},
		{ AUT_SOCKET_EX, "socket", pa_socket_ex_token	},
		{ AUT_SUBJECT32, "subject", pa_subject32_token	},
		{ AUT_SUBJECT64, "subject", pa_subject64_token	},
		{ AUT_SUBJECT32_EX, "subject", pa_subject32_token_ex	},
		{ AUT_SUBJECT64_EX, "subject", pa_subject64_token_ex	},
		{ AUT_TEXT, "text", pa_text_token		},
		{ AUT_TRAILER, "trailer", pa_trailer_token	},
	 };


static int	numtokenentries = sizeof (tokentab)
			/ sizeof (struct tokentable);

/*
 * ------------------------------------------------------
 * field widths for arbitrary data token type
 * ------------------------------------------------------
 */
static struct fw {
	char	basic_unit;
	struct {
		char	print_base;
		int	field_width;
	} pwidth[5];
} fwidth[] = {
	/* character data type, 8 bits */
		AUR_CHAR, { AUP_BINARY, 12 },
				{ AUP_OCTAL, 6 },
				{ AUP_DECIMAL, 6 },
				{ AUP_HEX, 6 },
				{ AUP_STRING, 4 },
		AUR_BYTE, { AUP_BINARY, 12 },
				{ AUP_OCTAL, 6 },
				{ AUP_DECIMAL, 6 },
				{ AUP_HEX, 6 },
				{ AUP_STRING, 4 },
		AUR_SHORT, { AUP_BINARY, 20 },
				{ AUP_OCTAL, 10 },
				{ AUP_DECIMAL, 10 },
				{ AUP_HEX, 8 },
				{ AUP_STRING, 6 },
		AUR_INT32, { AUP_BINARY, 36 },
				{ AUP_OCTAL, 18 },
				{ AUP_DECIMAL, 18 },
				{ AUP_HEX, 12 },
				{ AUP_STRING, 10 },
		AUR_INT64, { AUP_BINARY, 68 },
				{ AUP_OCTAL, 34 },
				{ AUP_DECIMAL, 34 },
				{ AUP_HEX, 20 },
				{ AUP_STRING, 20 } };


static int	numwidthentries = sizeof (fwidth)
			/ sizeof (struct fw);

#ifdef __cplusplus
}
#endif

#endif	/* _PRAUDIT_H */
