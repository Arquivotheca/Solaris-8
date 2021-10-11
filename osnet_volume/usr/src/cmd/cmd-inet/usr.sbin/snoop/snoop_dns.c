/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 */

#ident	"@(#)snoop_dns.c	1.3	99/12/13 SMI"	/* SunOS	*/

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/tiuser.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include "snoop.h"

/*
 * public functions
 */
void interpret_dns(int flags, int proto, char *data, int len);

/*
 * private functions
 */
static char *dns_opcode_string(uint opcode);
static char *dns_rcode_string(uint rcode);
static char *dns_type_string(uint type, int detail);
static char *dns_class_string(uint cls, int detail);

static char *indent_line(char *line);
static int skip_question(char *header, char *data, char *dend);
static int print_question(char *line, char *header, char *data, char *dend,
	int detail);
static int print_answer(char *line, char *header, char *data, char *dend,
	int detail);
static char *binary_string(char data);
static void print_ip(char *line, char *data);
static char *get_char_string(char *data, char *charbuf);
static int print_char_string(char *line, char *data);
static char *get_domain_name(char *header, char *data, char *namebuf,
	char *namend);
static int print_domain_name(char *line, char *header, char *name,
	char *namend);

void
interpret_dns(int flags, int proto, char *data, int len)
{
	typedef HEADER dns_header;
	dns_header *header;
	char *line;
	u_short data_len;
	u_short id;
	u_short qdcount, ancount, nscount, arcount;
	u_short count;
	char *questions;
	char *answers;
	char *nservers;
	char *additions;
	u_short type, cls;
	char *name;
	char *result;
	char *data_end;

	if (proto == IPPROTO_TCP) {
		/* not supported now */
		return;
	}

	/* We need at least the header in order to parse a packet. */
	if (sizeof (dns_header) > len) {
		return;
	}
	data_end = data + len;
	header = (dns_header *)data;
	id = ntohs(header->id);
	qdcount = ntohs(header->qdcount);
	ancount = ntohs(header->ancount);
	nscount = ntohs(header->nscount);
	arcount = ntohs(header->arcount);

	if (flags & F_SUM) {
		line = get_sum_line();
		sprintf(line, "DNS %s ", header->qr ? "R" : "C");
		line += strlen(line);

		if (header->qr) {
			/* answer */
			if (header->rcode == 0) {
				/* reply is OK */
				questions = data + sizeof (dns_header);
				while (qdcount--) {
					if (questions >= data_end) {
						return;
					}
					questions += skip_question(
						(char *)header,
						questions,
						data_end);
				}
				/* the answers are following the questions */
				answers = questions;
				if (ancount > 0)
					(void) print_answer(line,
						(char *)header,
						answers, data_end, FALSE);
			} else {
				sprintf(line, " Error: %d(%s)", header->rcode,
					dns_rcode_string(header->rcode));
			}
		} else {
			/* question */
			questions = data + sizeof (dns_header);
			if (questions >= data_end) {
				return;
			}
			(void) print_question(line, (char *)header,
			    questions, data_end, FALSE);
		}
	}
	if (flags & F_DTAIL) {
		show_header("DNS:  ", "DNS Header", sizeof (dns_header));
		show_space();
		if (header->qr) {
			/* answer */
			(void) sprintf(get_line(0, 0), "Response ID = %d", id);
			(void) sprintf(get_line(0, 0), "%s%s%s",
				header->aa ? "AA (Authoritative Answer) " : "",
				header->tc ? "TC (TrunCation) " : "",
				header->ra ? "RA (Recursion Available) ": "");
			(void) sprintf(get_line(0, 0), "Response Code: %d (%s)",
				header->rcode,
				dns_rcode_string(header->rcode));
			line = get_line(0, 0);
			sprintf(line, "Reply to %d question(s)", qdcount);
			questions = data + sizeof (dns_header);
			count = 0;
			while (qdcount--) {
				if (questions >= data_end) {
					return;
				}
				count++;
				questions += print_question(get_line(0, 0),
					(char *)header, questions, data_end,
					TRUE);
				show_space();
			}
			line = get_line(0, 0);
			sprintf(line, "%d answer(s)", ancount);
			answers = questions;
			count = 0;
			while (ancount--) {
				if (answers >= data_end) {
					return;
				}
				count++;
				answers += print_answer(get_line(0, 0),
						(char *)header, answers,
						data_end, TRUE);
				show_space();
			}
			line = get_line(0, 0);
			sprintf(line, "%d name server resource(s)", nscount);
			nservers = answers;
			count = 0;
			while (nscount--) {
				if (nservers >= data_end) {
					return;
				}
				count++;
				line = get_line(0, 0);
				nservers += print_answer(line, (char *)header,
							nservers, data_end,
							TRUE);
				show_space();
			}
			line = get_line(0, 0);
			sprintf(line, "%d additional record(s)", arcount);
			additions = nservers;
			count = 0;
			while (arcount-- && additions < data_end) {
				count++;
				line = get_line(0, 0);
				additions += print_answer(line, (char *)header,
							additions, data_end,
							TRUE);
				show_space();
			}
		} else {
			/* question */
			(void) sprintf(get_line(0, 0), "Query ID = %d",
				header->id);
			(void) sprintf(get_line(0, 0), "Opcode: %s",
					dns_opcode_string(header->opcode));
			(void) sprintf(get_line(0, 0), "%s%s",
				header->tc ? "TC (TrunCation) " : "",
				header->rd ? "RD (Recursion Desired) " : "");
			line = get_line(0, 0);
			sprintf(line, "%d question(s)", qdcount);
			questions = data + sizeof (dns_header);
			count = 0;
			while (qdcount-- && questions < data_end) {
				count++;
				questions += print_question(get_line(0, 0),
					(char *)header, questions, data_end,
					TRUE);
				show_space();
			}
		}
	}

}


static char *dns_opcode_string(uint opcode)
{
	static char buffer[64];
	switch (opcode) {
		case 0: return "Query";
		case 1: return "Inverse Query";
		case 2: return "Status";
		default:
			sprintf(buffer, "Unknown (%u)", opcode);
			return (buffer);
	}
}

static char *dns_rcode_string(uint rcode)
{
	static char buffer[64];
	switch (rcode) {
		case 0: return "OK";
		case 1: return "Format Error";
		case 2: return "Server Fail";
		case 3: return "Name Error";
		case 4: return "Unimplemented";
		case 5: return "Refused";
		default:
			sprintf(buffer, "Unknown (%u)", rcode);
			return (buffer);
	}
}

static char *dns_type_string(uint type, int detail)
{
	static char buffer[64];
	switch (type) {
		case 1:	return (detail ? "Address" : "Addr");
		case 2: return (detail ? "Authoritative Name Server" : "NS");
		case 5: return (detail ? "Canonical Name" : "CNAME");
		case 6: return (detail ? "Start Of a zone Authority" : "SOA");
		case 7: return (detail ? "Mailbox domain name" : "MB");
		case 8: return (detail ? "Mailbox Group member" : "MG");
		case 9: return (detail ? "Mail Rename domain name" : "MR");
		case 10: return "NULL";
		case 11: return (detail ? "Well Known Service" : "WKS");
		case 12: return (detail ? "Domain Name Pointer" : "PTR");
		case 13: return (detail ? "Host Information": "HINFO");
		case 14: return (detail ? "Mailbox or maillist Info" : "MINFO");
		case 15: return (detail ? "Mail Exchange" : "MX");
		case 16: return (detail ? "Text strings" : "TXT");
		case 252: return (detail ? "Transfer of entire zone" : "AXFR");
		case 253: return (detail ? "Mailbox related records" : "MAILB");
		case 254: return (detail ? "Mail agent RRs" : "MAILA");
		case 255: return (detail ? "All records" : "*");
		default:
			sprintf(buffer, "Unknown (%u)", type);
			return (buffer);
	}
}

static char *dns_class_string(uint cls, int detail)
{
	static char buffer[64];
	switch (cls) {
		case 1: return (detail ? "Internet" : "Internet");
		case 2: return (detail ? "CSNET" : "CS");
		case 3: return (detail ? "CHAOS" : "CH");
		case 4: return (detail ? "Hesiod" : "HS");
		case 255: return (detail ? "* (Any class)" : "*");
		default:
			sprintf(buffer, "Unknown (%u)", cls);
			return (buffer);
	}
}

static char *indent_line(char *line)
{
	sprintf(line, "    ");
	line += strlen(line);
	return (line);
}	

static int skip_question(char *header, char *data, char *dend)
{
	char *data_bak = data;
	u_short dummy_short;
	char dummy_buffer[1024];

	data = get_domain_name(header, data, dummy_buffer,
		dummy_buffer + sizeof (dummy_buffer));
	GETSHORT(dummy_short, data);
	GETSHORT(dummy_short, data);
	return (data - data_bak);
}

static int print_question(char *line, char *header, char *data, char *dend,
	int detail)
{
	char *data_bak = data;
	u_short type;
	u_short cls;

	if (detail) {
		line = indent_line(line);
		sprintf(line, "Domain Name: ");
		line += strlen(line);
	}
	data += print_domain_name(line, header, data, dend);
	line += strlen(line);

	GETSHORT(type, data);
	GETSHORT(cls, data);

	if (detail) {
		line = indent_line(get_line(0, 0));
		sprintf(line, "Class: %d (%s)", cls,
			dns_class_string(cls, detail));
		line = indent_line(get_line(0, 0));
		sprintf(line, "Type:  %d (%s)", type,
			dns_type_string(type, detail));
	} else {
		sprintf(line, " %s %s \?",
			dns_class_string(cls, detail),
			dns_type_string(type, detail));
		line += strlen(line);
	}
	return (data - data_bak);
}

static int print_answer(char *line, char *header, char *data, char *dend,
	int detail)
{
	char *data_bak = data;
	char *data_next;
	u_short type;
	u_short cls;
	u_long ttl;
	u_short rdlen;
	u_long serial, refresh, retry, expire, minimum;
	u_char protocol;
	int linepos;
	u_short preference;

	if (detail) {
		line = indent_line(line);
		sprintf(line, "Domain Name: ");
		line += strlen(line);
	}
	data += print_domain_name(line, header, data, dend);
	line += strlen(line);

	GETSHORT(type, data);
	GETSHORT(cls, data);

	if (detail) {
		line = indent_line(get_line(0, 0));
		sprintf(line, "Class: %d (%s)", cls,
			dns_class_string(cls, detail));
		line = indent_line(get_line(0, 0));
		sprintf(line, "Type:  %d (%s)", type,
			dns_type_string(type, detail));
	} else {
		sprintf(line, " %s %s ",
			dns_class_string(cls, detail),
			dns_type_string(type, detail));
		line += strlen(line);
	}

	GETLONG(ttl, data);
	if (detail) {
		line = indent_line(get_line(0, 0));
		sprintf(line, "TTL (Time To Live): %lu", ttl);
	}

	GETSHORT(rdlen, data);
	if (detail) {
		/* start another line to print */
		line = indent_line(get_line(0, 0));
		sprintf(line, "%s: ", dns_type_string(type, detail));
		line += strlen(line);
	}

	switch (type) {
		case T_A:
			print_ip(line, data);
			break;
		case T_HINFO:
			sprintf(line, "CPU: ");
			line += strlen(line);
			data_next = data + print_char_string(line, data);
			line += strlen(line);
			sprintf(line, "OS: ");
			line += strlen(line);
			(void) print_char_string(line, data_next);
			line += strlen(line);
			break;
		case T_NS:
		case T_CNAME:
		case T_MB:
		case T_MG:
		case T_MR:
		case T_PTR:
			print_domain_name(line, header, data, dend);
			break;
		case T_MX:
			data_next = data;
			GETSHORT(preference, data_next);
			if (detail) {
				print_domain_name(line, header, data_next,
					dend);
				line = indent_line(get_line(0, 0));
				sprintf(line, "Preference: %u", preference);
			} else {
				print_domain_name(line, header, data_next,
					dend);
			}
			break;
		case T_SOA:
			if (!detail)
				break;
			line = indent_line(get_line(0, 0));
			sprintf(line, "MNAME (Server name): ");
			line += strlen(line);
			data_next = data +
				print_domain_name(line, header, data, dend);
			line = indent_line(get_line(0, 0));
			sprintf(line, "RNAME (Resposible mailbox): ");
			line += strlen(line);
			data_next = data_next +
				print_domain_name(line, header, data_next,
					dend);
			line = indent_line(get_line(0, 0));
			GETLONG(serial, data_next);
			GETLONG(refresh, data_next);
			GETLONG(retry, data_next);
			GETLONG(expire, data_next);
			GETLONG(minimum, data_next);
			sprintf(line, "Serial: %lu", serial);
			line = indent_line(get_line(0, 0));
			sprintf(line,
			"Refresh: %lu  Retry: %lu  Expire: %lu Minimum: %lu",
				refresh, retry, expire, minimum);
			break;
		case T_WKS:
			print_ip(line, data);
			if (!detail)
				break;
			line = indent_line(get_line(0, 0));
			data_next = data + 4;
			protocol = *data_next++;
			sprintf(line, "Protocol: %u ", protocol);
			line += strlen(line);
			switch (protocol) {
				case IPPROTO_UDP:
					sprintf(line, "(UDP)");
					break;
				case IPPROTO_TCP:
					sprintf(line, "(TCP)");
					break;
			}
			line = indent_line(get_line(0, 0));
			sprintf(line, "Service bitmap:");
			line = indent_line(get_line(0, 0));
			sprintf(line, "0       8       16      24");
			linepos = 4;
			while (data_next < data + rdlen) {
				if (linepos == 4) {
					line = indent_line(get_line(0, 0));
					linepos = 0;
				}
				sprintf(line, "%s", binary_string(*data_next));
				line += strlen(line);
				linepos++;
				data_next++;
			}
			break;
		case T_MINFO:
			if (!detail)
				break;
			line = indent_line(get_line(0, 0));
			sprintf(line, "RMAILBX (Resposible mailbox): ");
			line += strlen(line);
			data_bak = data +
				print_domain_name(line, header, data, dend);
			line = indent_line(get_line(0, 0));
			sprintf(line,
				"EMAILBX (mailbox to receive err message): ");
			line += strlen(line);
			data_bak = data_bak +
				print_domain_name(line, header, data_bak, dend);
			break;
	}
	data += rdlen;
	return (data - data_bak);
}

static char *binary_string(char data)
{
	static char bstring[8 + 1];
	char *ptr;
	int i;
	ptr = bstring;
	for (i = 0; i < 8; i++) {
		*ptr++ = (data & 0x80) ? '1' : '0';
		data = data << 1;
	}
	*ptr = (char)0;
	return (bstring);
}

static void print_ip(char *line, char *data)
{
#define	UC(b)	(((int)b)&0xff)
	sprintf(line, "%d.%d.%d.%d",
		UC(data[0]), UC(data[1]), UC(data[2]), UC(data[3]));
}

static char *get_char_string(char *data, char *charbuf)
{
	u_char len;
	char *name = charbuf;
	len = *data;
	data++;
	while (len > 0) {
		*name = *data;
		name++;
		data++;
		len--;
	}
	*name = (char)0;
	return (data);
}

static int print_char_string(char *line, char *data)
{
	char charbuf[1024];
	char *data_bak = data;
	data = get_char_string(data, charbuf);
	sprintf(line, "%s", charbuf);
	return (data - data_bak);
}

/*
 * header: the entire message header, this is where we start to
 *	   count the offset of the compression scheme
 * data:   the start of the domain name
 * namebuf: user supplied buffer
 * return: the next byte after what we have parsed
 */

static char *get_domain_name(char *header, char *data, char *namebuf,
		char *namend)
{
	u_char len;
	u_short offset;
	char *name = namebuf;
	char *new_data;

	while (name < (namend - 1)) {
		len = *data;
		data++;
		if (len == 0)
			break;
		/*
		 * test if we are using the compression scheme
		 */
		if ((len & 0xc0) == 0xc0) {
			/*
			 * compression: backup one byte then read in
			 * a short int, which is the offset (from the
			 * first byte of the header) to where the
			 * next label is.
			 */
			data--;
			GETSHORT(offset, data);
			new_data = header + (offset & 0x3fff);
			get_domain_name(header, new_data, name, namend);
			return (data);
		} else {
			while (len > 0 && name < (namend - 2)) {
				*name = *data;
				name++;
				data++;
				len--;
			}
			*name = '.';
			name++;
		}
	}
	*name = (char)0;
	/*
	 * Give up on packet if buffer overflow is occurring.
	 * XXX currently the dns interpreter may still read beyond
	 * the end of the packet, due to erratic checking of packet
	 * buffer end.
	 */
	if (name >= namend)
		pr_err("DNS domain buffer overflow");
	return (data);
}

static int print_domain_name(char *line, char *header, char *data, char *dend)
{
	int total_len = 0;
	char name[1024];
	char *new_data;

	new_data = get_domain_name(header, data, name, name + sizeof (name));

	sprintf(line, "%s", name);
	return (new_data - data);

}
