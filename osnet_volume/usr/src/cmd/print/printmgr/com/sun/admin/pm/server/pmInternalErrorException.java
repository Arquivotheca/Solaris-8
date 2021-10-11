/*
 * ident	"@(#)pmInternalErrorException.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmInternalErrorException class
 * An internal error has occuured. This is often associated
 * with the server not getting enough information to perform
 * an action.
 */

package com.sun.admin.pm.server;

public class pmInternalErrorException extends pmException
{
    public pmInternalErrorException()
    {
	super();
    }

    public pmInternalErrorException(String s)
    {
	super(s);
    }
}
