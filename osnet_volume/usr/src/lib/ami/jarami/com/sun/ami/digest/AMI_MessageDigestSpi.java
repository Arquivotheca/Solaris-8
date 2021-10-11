/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_MessageDigestSpi.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.digest;


import java.lang.*;
import java.security.*;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.*;

/**
 * The AMI_MessageDigestSpi provides a framework for building
 * AMI digest classes such as MD2, MD5 and SHA1.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 **/

public abstract class AMI_MessageDigestSpi extends MessageDigestSpi {

	/* contains data to be digested */
	protected byte[] _buffer;

	/* length of data to be digested */
	protected int _bufLen;

	/* contains the length of the digest */
	protected int DIGEST_LENGTH;

	/* digest name */
	protected String DIGEST_NAME;

	/**
         * Initialize the AMI_MessageDiget state information and reset the
	 * bit count to 0. Given this implementation you are constrained
	 * to counting 2^64 bits.
	 */
	public void init() {
		_buffer = null;
	}

	protected void engineReset() {
		init();
	}

	/**
	 * Return the digest length in bytes
	 */
	protected int engineGetDigestLength() {
		return (DIGEST_LENGTH);
	}

	/**
	 * Update adds the passed byte to the digested data.
	 */
	protected synchronized void engineUpdate(byte b) {
		byte[] tmp = null;
		int last_indx = 0;

		if (_buffer != null) {
			tmp = new byte[_buffer.length + 1];
			System.arraycopy(_buffer, 0, tmp, 0, _buffer.length);
			_buffer = tmp;
			last_indx = _buffer.length - 1;
		} else
		     _buffer = new byte[1];

		// Append the new data
		_buffer[last_indx] = b;
	}

	/**
	 * Update adds the selected part of an array of bytes to the digest.
	 * This version is more efficient than the byte-at-a-time version;
	 * it avoids data copies and reduces per-byte call overhead.
	 */
	protected synchronized void engineUpdate(byte input[],
	    int offset, int len) {
		byte[] tmp = null;
		int last_indx = 0;
		int original_length = 0;

		if (_buffer != null) {
			original_length = _buffer.length;
			tmp = new byte[len + original_length];
			System.arraycopy(_buffer, 0, tmp, 0, original_length);
			_buffer = tmp;
		} else 
			_buffer = new byte[len];
	
		// Append the new data
		System.arraycopy(input, offset, _buffer, original_length, len);
	}

	protected abstract void ami_native_digest(AMI_Digest digest,
	    byte[] buffer, int length) throws AMI_DigestException;

	/**
	 * Method to digest the data
	 */
	protected byte[] engineDigest()  {

		try {
			AMI_Debug.debugln(2,
			    "AMI_MessageDigestSpi:: Attempting " +
			    DIGEST_NAME + " digest");   
		} catch (Exception e) {
			throw new RuntimeException(e.toString());
		}

		AMI_Digest digest;

		if (_buffer == null || _buffer.length == 0)
			_bufLen = 0;
		else
			_bufLen = _buffer.length;
		
		try {		        
			digest = new AMI_Digest();
			ami_native_digest(digest, _buffer, _bufLen);
		} catch (Exception se) {
			try {
				AMI_Debug.debugln(1,
				    "AMI_MessageDigestSpi::Unable to perform " +
				    DIGEST_NAME + " digest: " + se.toString());
			} catch (Exception e) {
				throw new RuntimeException(
				    se.getLocalizedMessage() + e.toString());
			}
			throw new RuntimeException(se.getLocalizedMessage());
		}

		try {
			AMI_Debug.debugln(1, "AMI_MessageDigestSpi::" +
			    DIGEST_NAME + "Digest Completed");
			Object[] messageArguments = {
			    new String(DIGEST_NAME)
			    };
			AMI_Log.writeLog(1,
			    "AMI_Digest.digest", messageArguments);
		} catch (Exception e) {
			throw new RuntimeException(e.toString());
		}

		byte[] digestBits = digest.getDigest();
		byte[] result = new byte[DIGEST_LENGTH];
		System.arraycopy(digestBits, 0, result, 0, DIGEST_LENGTH);
		init();
		return result;
	}

	/**
	 * Digest the data and return the results in the buffer
	 */
	protected int engineDigest(byte[] buf, int offset, int len)
	    throws DigestException {
		if (len < DIGEST_LENGTH)
			throw new DigestException(
			    "partial digests not returned");
		if (buf.length - offset < DIGEST_LENGTH)
			throw new DigestException("insufficient space in " +
			    "the output buffer to store the digest");

		byte[] digestedData = engineDigest();
		System.arraycopy(digestedData, 0, buf, offset, DIGEST_LENGTH);
		return (DIGEST_LENGTH);
	}

	/*
	 * Clones this object.
	 */
	public Object clone() {
		  return null;
	}
}
