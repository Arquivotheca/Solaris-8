/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RC4Crypt.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.crypto;

import java.security.*;
import java.security.spec.*;

import com.sun.ami.AMI_Exception;
import com.sun.ami.crypto.*;

/**
 * This class is the internal RC4 class that does encryption and 
 * decryption on an array. 
 * <P>The encryption and decryption is defined by the RC4 algorithm.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 * @see     AMI_RC4Cipher
 * @see     AMI_SymmetricCipher
 */

public class AMI_RC4Crypt extends AMI_SymmetricCipher {

    /**
     * Gets this cipher's block size.
     *
     * @return this cipher's block size
     */
    public int getBlockSize() {
	return 0;
    }
    
    /**
     * Get the output encrypted or decrypted data len
     *
     */
    public int getDataLen() {
	return _dataLen;
    }
    
    /**
     * Gets the IV 
     *
     * @return null ( as RC4 does not have IV )
     */
    public byte[] getIV() {
	return null;
    }
    
    /**
     * Gets the effective key size 
     *
     * @return null ( as RC4 does not have this )
     */
    public int getEffectiveKeySize() {
	return 0;
    }

    /**
     * Set the encryption/decryption stage
     *
     * @param stage : UPDATE or FINAL
     */
    public void setStage(int stage) {
	_stage = stage;
    }
    /**
     * Initializes this cipher with the specified key.
     *
     * @param key the key
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * this cipher
     */
    public void init(Key key) throws InvalidKeyException {
	if (key == null)
	    throw new InvalidKeyException("Null key");
	
	byte[] rawKey = key.getEncoded();
	if (!(key.getFormat().equals("RAW")) || 
	    (rawKey == null)) {
	    throw new InvalidKeyException
		("key not raw bytes or not expected number of bytes");
	}
	_key = rawKey;

	 try {
	        _crypto = new AMI_Crypto();
		_crypto.setAMIHandleValid(false);
        } catch (Exception e) {
               throw new InvalidKeyException("Unable to Initialize");
        }
    }
    
    /**
     * Initializes this cipher with the specified key.
     *
     * @param key the key
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * this cipher
     */
    public void init(Key key, AlgorithmParameterSpec params)
	throws InvalidKeyException {
	    this.init(key);
    }
    
    /**
     * Performs encryption operation.
     * 
     * <p>The input plain text <code>plain</code>, starting at
     * <code>plainOffset</code> and ending at
     * <code>(plainOffset + len - 1)</code>, is encrypted.
     * The result is stored in <code>cipher</code>, starting at
     * <code>cipherOffset</code>.
     *
     * <p>The subclass that implements Cipher should ensure that
     * <code>init</code> has been called before this method is called.
     *
     * @param plain the buffer with the input data to be encrypted
     * @param plainOffset the offset in <code>plain</code>
     * @param plainLen the length of the input data
     * @param cipher the buffer for the result
     * @param cipherOffset the offset in <code>cipher</code>
     *
     */
    public void encrypt(byte[] plain, int plainOffset, int plainLen,
			byte[] cipher, int cipherOffset)
    {
	    byte[] actual = null;
	    _decrypting = false;

	    if ((plainOffset != 0) && (plain != null))
	    {
		actual = new byte[plainLen];
		for (int ii = 0; ii < plainLen; ii++)
		   actual[ii] = plain[plainOffset + ii];
	    }
	    else
	        actual = plain;

	    cipherBlock(actual, plainLen, cipher);
	    cipherOffset = 0;
    }

    /**
     * Performs decryption operation.
     * 
     * <p>The input cipher text <code>cipher</code>, starting at
     * <code>cipherOffset</code> and ending at
     * <code>(cipherOffset + len - 1)</code>, is decrypted.
     * The result is stored in <code>plain</code>, starting at
     * <code>plainOffset</code>.
     *
     * <p>The subclass that implements Cipher should ensure that
     * <code>init</code> has been called before this method is called.
     *
     * @param cipher the buffer with the input data to be decrypted
     * @param cipherOffset the offset in <code>cipherOffset</code>
     * @param cipherLen the length of the input data
     * @param plain the buffer for the result
     * @param plainOffset the offset in <code>plain</code>
     *
     */
    public void decrypt(byte[] cipher, int cipherOffset, int cipherLen, 
			byte[] plain, int plainOffset) 
    {
	    byte[] actual = null;
	    _decrypting = true;

	    if ((cipherOffset != 0) && (cipher != null))
	    {
	        actual = new byte[cipherLen];
	        for (int ii = 0; ii < cipherLen; ii++)
			actual[ii] = cipher[cipherOffset + ii];
	    }
	    else
	        actual = cipher;

	    cipherBlock(actual, cipherLen, plain);
	    plainOffset = 0;
    }


    private void cipherBlock(byte[] input, int inputLen, byte[] output)
    {
	 if (_decrypting) {  // Decrypting
	   try {
		if (!_crypto.getAMIHandleValid()) {
			_crypto.ami_init();
			_crypto.setAMIHandleValid(true);
		}
		_crypto.ami_rc4_decrypt(input, inputLen, _stage,
		    _key, _key.length);
	   } catch (AMI_CryptoException e) {
		e.printStackTrace();
	   }	     
	 } else  {             // Encrypting
	   try {
		if (!_crypto.getAMIHandleValid()) {
			_crypto.ami_init();
			_crypto.setAMIHandleValid(true);
		}
		_crypto.ami_rc4_encrypt(input, inputLen, _stage,
		    _key, _key.length);
	   } catch (AMI_CryptoException e) {
		e.printStackTrace();
	   }	
	 }
	 // Return the encrypted/decrypted data
	 byte[]tmp = _crypto.getOutputData();
	 
	 for (int ii = 0; ii < tmp.length; ii++)
	    output[ii] = tmp[ii];       

	 // Store the data len, to be retrieved later.
	 _dataLen =  _crypto.getOutputLen();

	 if (_stage == FINAL) {
		if (_crypto.getAMIHandleValid()) {
		    _crypto.ami_end();
		    _crypto.setAMIHandleValid(false);
		}
	}
    }


    /*
     * the encryption key array 
     */
    protected byte[] _key = null;
 
    /*
     * Are we encrypting or decrypting?
     */
    protected boolean _decrypting = false;

    private int _stage = UPDATE;

    private int _dataLen = 0;  // Stores the length of
	// encrypted or decrypted o/p data

    private AMI_Crypto _crypto = null;

}
