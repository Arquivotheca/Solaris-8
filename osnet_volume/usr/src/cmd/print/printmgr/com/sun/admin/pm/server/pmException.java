/*
 * ident	"@(#)pmException.java	1.1	99/03/16 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmException class
 * General print manager exceptions 
 */

package com.sun.admin.pm.server;

public class pmException extends Exception
{
    public pmException()
    {
	super();
    }

    public pmException(String s)
    {
	super(s);
    }
}
