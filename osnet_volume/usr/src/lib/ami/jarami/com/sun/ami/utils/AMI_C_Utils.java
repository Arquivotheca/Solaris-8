/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_C_Utils.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.utils;

import java.security.*;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Common;

/**
 * This class provides standard utilities in Java, which are
 * implemented in C, due to lack of availability of the functionality
 * in Java.
 */

public class AMI_C_Utils extends AMI_Common {

    public AMI_C_Utils() {
	// do nothing
    }

    /*
     * Retrieve the user id of the user currently logged in 
     * from the UNIX system.
     * @return user id
     * @throws AMI_Exception
     */
     public static native long ami_get_user_id() throws  AMI_Exception;	   

    /*
     * Retrieve the Keystore password for the user or the host, without
     * displaying it on the command line.
     * @param progname The Name of the class invoking this utility
     * @param loginType Password is being retreived ofr user or host.
     *        This determines the prompt line.
     * @return password
     * @throws AMI_Exception
     */
    public static native String ami_get_password(String progname,
	int loginType) throws  AMI_Exception;	   
}

