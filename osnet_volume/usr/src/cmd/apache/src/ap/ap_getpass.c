/* ====================================================================
 * Copyright (c) 1995-1999 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */
/*
 * ap_getpass.c: abstraction to provide for obtaining a password from the
 * command line in whatever way the OS supports.  In the best case, it's a
 * wrapper for the system library's getpass() routine; otherwise, we
 * use one we define ourselves.
 */

#include "ap_config.h"
#include <sys/types.h>
#include <errno.h>
#include "ap.h"

#ifdef WIN32
#include <conio.h>
#endif

#ifndef CHARSET_EBCDIC
#define LF 10
#define CR 13
#else /* CHARSET_EBCDIC */
#define LF '\n'
#define CR '\r'
#endif /* CHARSET_EBCDIC */

#define MAX_STRING_LEN 256

#define ERR_OVERFLOW 5

#ifdef MPE
/*
 * MPE lacks getpass() and a way to suppress stdin echo.  So for now, just
 * issue the prompt and read the results with echo.  (Ugh).
 */

static char *getpass(const char *prompt)
{
    static char password[MAX_STRING_LEN];

    fputs(prompt, stderr);
    gets((char *) &password);

    if (strlen((char *) &password) > (MAX_STRING_LEN - 1)) {
	password[MAX_STRING_LEN - 1] = '\0';
    }

    return (char *) &password;
}

#endif

#ifdef WIN32
/*
 * Windows lacks getpass().  So we'll re-implement it here.
 */

static char *getpass(const char *prompt)
{
    static char password[MAX_STRING_LEN];
    int n = 0;

    fputs(prompt, stderr);
    
    while ((password[n] = _getch()) != '\r') {
        if (password[n] >= ' ' && password[n] <= '~') {
            n++;
            printf("*");
        }
	else {
            printf("\n");
            fputs(prompt, stderr);
            n = 0;
        }
    }
 
    password[n] = '\0';
    printf("\n");

    if (n > (MAX_STRING_LEN - 1)) {
        password[MAX_STRING_LEN - 1] = '\0';
    }

    return (char *) &password;
}
#endif

/*
 * Use the OS getpass() routine (or our own) to obtain a password from
 * the input stream.
 *
 * Exit values:
 *  0: Success
 *  5: Partial success; entered text truncated to the size of the
 *     destination buffer
 *
 * Restrictions: Truncation also occurs according to the host system's
 * getpass() semantics, or at position 255 if our own version is used,
 * but the caller is *not* made aware of it.
 */

API_EXPORT(int) ap_getpass(const char *prompt, char *pwbuf, size_t bufsiz)
{
    char *pw_got;
    int result = 0;

    pw_got = getpass(prompt);
    if (strlen(pw_got) > (bufsiz - 1)) {
	result = ERR_OVERFLOW;
    }
    ap_cpystrn(pwbuf, pw_got, bufsiz);
    return result;
}
