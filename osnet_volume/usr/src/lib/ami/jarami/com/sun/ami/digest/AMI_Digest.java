/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Digest.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.digest;

import java.lang.System;
import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Common;

/**
 * This class is the base AMI digest class, which provides native wrapper
 * methods for all AMI-native digest methods. It currently implements
 * MD2, MD5 and SHA1 digest.
 */

public class AMI_Digest extends AMI_Common {

	protected byte[] _digest;

	public AMI_Digest() throws AMI_Exception {
		super();
	}

	// Native method for MD2 digest
	public native void ami_md2_digest(byte[] toBeDigested,
	    int tobeDigestedLen) throws AMI_DigestException;

	// Native method for MD5 digest
	public native void ami_md5_digest(byte[] toBeDigested,
	    int tobeDigestedLen) throws AMI_DigestException;

	// Native method for SHA1 digest
	public native void ami_sha1_digest(byte[] toBeDigested,
	    int tobeDigestedLen) throws AMI_DigestException;

	// Return the generated digest
	public byte[] getDigest() {
		return (_digest);
	}
}

