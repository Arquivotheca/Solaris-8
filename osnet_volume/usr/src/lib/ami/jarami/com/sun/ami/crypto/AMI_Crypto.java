/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Crypto.java	1.2 99/07/16 SMI"
 *
 */

package com.sun.ami.crypto;

import java.security.*;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Common;

/**
 * This class is the base AMI Cryptoclass, which provides native
 * wrapper methods for all AMI-native crypto algorithms.
 * <P>Currently it provides wrapper functions for : <UL>
 *  <LI>        RC2/RC4/DES/3DES encryption and decryption
 *  <LI>        RSA Wrapping and unwrapping
 */

public class AMI_Crypto extends AMI_Common {
  
      	public AMI_Crypto()  throws  AMI_Exception   {
		super();
      	}

	/*
	 * Native method for initialisation of ami handle.
	 * This is needed as encrypt/decrypt is a multi-part
	 * operation and state information needs to be carried
	 * forward.
	 *
	 * @exception AMI_CryptoException Throws this exception when 
	 * it encounters
	 * any error during encryption.
	 */
      	public native void ami_init() throws AMI_CryptoException;

	/*
	 * Native method for closing of ami handle.
	 * @exception AMI_CryptoException Throws this exception when 
	 * it encounters
	 * any error during encryption.
	 */
	public native void ami_end();

	/*
	 * Native method for encryption using rc2
	 */
	public native void ami_rc2_encrypt(byte[] input, int inputLen,
	    int flag, byte[] key, int keyLen, int effectiveKeySize,
	    byte[] iv) throws AMI_CryptoException;

	/*
	 * Native method for decryption using rc2
	 */
	public native void ami_rc2_decrypt(byte[] input, int inputLen,
	    int flag, byte[] key, int keyLen, int effectiveKeySize,
	    byte[] iv) throws  AMI_CryptoException;

	/*
	 * Native method for encryption using des/3des
	 */
	public native void ami_des3des_encrypt(String algname, byte[] input,
	    int inputLen, int flag, byte[] key,
	    byte[] iv) throws AMI_CryptoException;

	/*
	 * Native method for decryption using des/3des
	 */
	public native void ami_des3des_decrypt(String algname, byte[] input,
	    int inputLen, int flag, byte[] key, byte[] iv) 
	    throws  AMI_CryptoException;

	/**
	 * Native method for encryption using rc4
	 */
	public native void ami_rc4_encrypt(byte[] input, int inputLen,
	    int flag, byte[] key, int keyLen) throws  AMI_CryptoException;

	/**
	 * Native method for decryption using rc4
	 */
	public native void ami_rc4_decrypt(byte[] input, int inputLen,
	    int flag, byte[] key, int keyLen) throws  AMI_CryptoException;

	/**
	 * Native method for wrapping using RSA
	 */
	public native void ami_rsa_wrap(byte[] key, int keyLen,
	    String wrappingKeyAlg, byte[] wrappingKey, int wrappingKeyLen,
	    String wrappingAlg) throws  AMI_CryptoException;


	/**
	 * Native method for unwrapping using RSA
	 */
	public native void ami_rsa_unwrap(byte[] wrappedKey,
	    int wrappedKeyLen,
	    String unwrappingKeyAlg, byte[] unwrappingKey,
	    int unwrappingKeyLen,
	    String unwrappingAlg) throws  AMI_CryptoException;


	/**
	 * Native method for encrypting using RSA
	 */
	public native void ami_rsa_encrypt(byte[] key, int keyLen,
	    byte[] data, int dataLen) throws  AMI_CryptoException;


	/**
	 * Native method for decrypting using RSA
	 */
	public native void ami_rsa_decrypt(byte[] key, int keyLen,
	    byte[] encrData, int encrDataLen)
	    throws  AMI_CryptoException;

	/**
	 * Returns the output data after the crypto operation has been
	 * performed.
	 * @return byte[] output  data
	 */
	public byte[] getOutputData() {
		return _outputData;
	}

	/**
	 * Returns the lentgth of the output data after the crypto operation 
	 * has been performed.
	 * @return int output  data len
	 */
	public int getOutputLen() {
		return _outputDataLen;
	}

	public boolean getAMIHandleValid() {
		return amiHandleValid;
	} 

	public void setAMIHandleValid(boolean amiHandleValid) {
		this.amiHandleValid = amiHandleValid;
	}

	/*
	 *  Parameters set from C native methods
	 */
	public byte[] _outputData;
 	public int _outputDataLen;
      
	private byte[] _amiHandle;
	protected boolean amiHandleValid;
}
  
