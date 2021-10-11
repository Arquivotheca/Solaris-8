/*
 * ident	"@(#)pmCmdFailedException.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmCmdFailedException class
 * A command unexpectedly failed.
 */

package com.sun.admin.pm.server;

public class pmCmdFailedException extends pmException
{
    public pmCmdFailedException()
    {
	super();
    }

    public pmCmdFailedException(String s)
    {
	super(s);
    }
}
