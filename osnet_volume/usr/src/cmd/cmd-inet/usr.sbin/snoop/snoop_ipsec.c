/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 */

#ident	"@(#)snoop_ipsec.c	1.1	98/12/16 SMI"	/* SunOS	*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <inet/ipsecesp.h>
#include <inet/ipsecah.h>
#include "snoop.h"

extern char *dlc_header;

int
interpret_esp(int flags, uint8_t *hdr, int iplen, int fraglen)
{
	esph_t *esph = (esph_t *)hdr;
	esph_t *aligned_esph;
	esph_t storage;	/* In case hdr isn't aligned. */
	char *line;

	if (fraglen < sizeof (esph_t))
		return;	/* incomplete header */

	if (!IS_P2ALIGNED(hdr, 4)) {
		aligned_esph = &storage;
		bcopy(hdr, aligned_esph, sizeof (esph_t));
	} else {
		aligned_esph = esph;
	}

	if (flags & F_SUM) {
		line = (char *)get_sum_line();
		/*
		 * sprintf() is safe because line guarantees us 80 columns,
		 * and SPI and replay certainly won't exceed that.
		 */
		(void) sprintf(line, "ESP SPI=0x%x Replay=%u",
		    ntohl(aligned_esph->esph_spi),
		    ntohl(aligned_esph->esph_replay));
		line += strlen(line);
	}

	if (flags & F_DTAIL) {
		show_header("ESP:  ", "Encapsulating Security Payload",
		    sizeof (esph_t));
		show_space();
		/*
		 * sprintf() is safe because get_line guarantees us 80 columns,
		 * and SPI and replay certainly won't exceed that.
		 */
		(void) sprintf(get_line((char *)&esph->esph_spi - dlc_header,
		    4), "SPI = 0x%x", ntohl(aligned_esph->esph_spi));
		(void) sprintf(get_line((char *)&esph->esph_replay -
		    dlc_header, 4), "Replay = %u",
		    ntohl(aligned_esph->esph_replay));
		(void) sprintf(get_line((char *)(esph + 1) - dlc_header,
		    4), "   ....ENCRYPTED DATA....");
	}

	return (sizeof (esph_t));
}

int
interpret_ah(int flags, uint8_t *hdr, int iplen, int fraglen)
{
	ah_t *ah = (ah_t *)hdr;
	ah_t *aligned_ah;
	ah_t storage;	/* In case hdr isn't aligned. */
	char *line, *buff;
	uint_t ahlen, auth_data_len;
	uint8_t *auth_data, *data;

	if (fraglen < sizeof (ah_t))
		return;		/* incomplete header */

	if (!IS_P2ALIGNED(hdr, 4)) {
		aligned_ah = (ah_t *)&storage;
		bcopy(hdr, storage, sizeof (ah_t));
	} else {
		aligned_ah = ah;
	}

	/*
	 * "+ 8" is for the "constant" part that's not included in the AH
	 * length.
	 *
	 * The AH RFC specifies the length field in "length in 4-byte units,
	 * not counting the first 8 bytes".  So if an AH is 24 bytes long,
	 * the length field will contain "4".  (4 * 4 + 8 == 24).
	 */
	ahlen = (aligned_ah->ah_length << 2) + 8;
	fraglen -= ahlen;
	if (fraglen < 0)
		return;		/* incomplete header */

	auth_data_len = ahlen - sizeof (ah_t);
	auth_data = (uint8_t *)(ah + 1);
	data = auth_data + auth_data_len;

	if (flags & F_SUM) {
		line = (char *)get_sum_line();
		(void) sprintf(line, "AH SPI=0x%x Replay=%u",
		    ntohl(aligned_ah->ah_spi), ntohl(aligned_ah->ah_replay));
		line += strlen(line);
	}

	if (flags & F_DTAIL) {
		show_header("AH:  ", "Authentication Header", ahlen);
		show_space();
		(void) sprintf(get_line((char *)&ah->ah_nexthdr - dlc_header,
		    1), "Next header = %d (%s)", aligned_ah->ah_nexthdr,
		    getproto(aligned_ah->ah_nexthdr));
		(void) sprintf(get_line((char *)&ah->ah_length - dlc_header, 1),
		    "AH length = %d (%d bytes)", aligned_ah->ah_length, ahlen);
		(void) sprintf(get_line((char *)&ah->ah_reserved - dlc_header,
		    2), "<Reserved field = 0x%x>",
		    ntohs(aligned_ah->ah_reserved));
		(void) sprintf(get_line((char *)&ah->ah_spi - dlc_header, 4),
		    "SPI = 0x%x", ntohl(aligned_ah->ah_spi));
		(void) sprintf(get_line((char *)&ah->ah_replay - dlc_header, 4),
		    "Replay = %u", ntohl(aligned_ah->ah_replay));

		/* * 2 for two hex digits per auth_data byte. */
		buff = malloc(auth_data_len * 2);
		if (buff != NULL) {
			int i;

			for (i = 0; i < auth_data_len; i++)
				sprintf(buff + i * 2, "%02x", auth_data[i]);
		}

		(void) sprintf(get_line((char *)auth_data - dlc_header,
		    auth_data_len), "ICV = %s",
		    (buff == NULL) ? "<out of memory>" : buff);

		/* malloc(3c) says I can call free even if buff == NULL */
		free(buff);

		show_space();
	}

	if (fraglen > 0)
		switch (aligned_ah->ah_nexthdr) {
			case IPPROTO_ENCAP:
				interpret_ip(flags, data, iplen - ahlen);
				break;
			case IPPROTO_ICMP:
				interpret_icmp(flags, data, iplen - ahlen,
				    fraglen);
				break;
			case IPPROTO_TCP:
				interpret_tcp(flags, data, iplen - ahlen,
				    fraglen);
				break;

			case IPPROTO_ESP:
				interpret_esp(flags, data, iplen - ahlen,
				    fraglen);
				break;
			case IPPROTO_AH:
				interpret_ah(flags, data, iplen - ahlen,
				    fraglen);
				break;

			case IPPROTO_UDP:
				interpret_udp(flags, data, iplen - ahlen,
				    fraglen);
				break;
			/* default case is to not print anything else */
		}

	return (ahlen);
}
