/*
 *
 * Copyright 07/06/99 Sun Microsystems, Inc. 
 * All Rights Reserved
 *
 *
 * Comments:   
 *
 */

#pragma ident   "@(#)sec.h 1.2     99/07/06 SMI"

#ifndef _SEC_H_
#define _SEC_H_

#include <sys/types.h>
#include "md5.h"

void hmac_md5(unsigned char *text, int text_len, unsigned char *key,
			  int key_len, caddr_t digest);

char *hexa_print(char *aString, int aLen);
char *hexa2str(char *anHexaStr, int *aResLen);

#endif /* _SEC_H_ */
