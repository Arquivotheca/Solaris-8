/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_MD5.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.digest;

import java.lang.*;
import java.security.*;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.*;

/**
 * The MD2 class is used to compute an MD2 message digest over a given
 * buffer of bytes. It is an implementation of the RSA Data Security Inc
 * MD2 algorithim as described in internet RFC 1321.
 *
 * @author Sangeeta Varma
 * @version 1.0
 */

public final class AMI_MD5 extends AMI_MessageDigestSpi {

    private static final String MD5_NAME = "MD5";
    private static final int MD5_LENGTH = 16;

    /**
     * Standard constructor, creates a new MD2 instance, allocates its
     * buffers from the heap.
     */
    public AMI_MD5() {
	DIGEST_NAME = new String(MD5_NAME);
	DIGEST_LENGTH = MD5_LENGTH;
	init();
    }

    /**
     * Method to digest the data
     */
    protected void ami_native_digest(AMI_Digest digest,
	byte[] buffer, int length) throws AMI_DigestException {
		digest.ami_md5_digest(_buffer, length);
    }

    /*
     * Clones this object.
     */
    public Object clone() {
          return (new AMI_MD5());
    }
}


