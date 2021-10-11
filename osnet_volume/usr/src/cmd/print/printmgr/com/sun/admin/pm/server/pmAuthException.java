/*
 * ident	"@(#)pmAuthException.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmAuthException class
 * User is not authorized to perform an operation.
 */

package com.sun.admin.pm.server;

public class pmAuthException extends pmException
{
    public pmAuthException()
    {
	super();
    }

    public pmAuthException(String s)
    {
	super(s);
    }
}
