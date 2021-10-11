/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_RSACrypt.java	1.4 99/07/16 SMI"
 *
 */

package com.sun.ami.crypto;

import java.util.*;
import java.rmi.*;
import java.security.*;
import java.security.spec.*;
import java.net.UnknownHostException;
import java.net.InetAddress;

import com.sun.ami.AMI_Exception;
import com.sun.ami.keygen.AMI_RSAPublicKey;
import com.sun.ami.keygen.AMI_RSAPrivateKey;
import com.sun.ami.keygen.AMI_PrivateKey;
import com.sun.ami.keygen.AMI_VirtualHost;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.amiserv.AMI_KeyServ;
import com.sun.ami.amiserv.AMI_KeyServClient;
import com.sun.ami.amiserv.AMI_CryptoInfo;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.common.*;
import com.sun.ami.crypto.*;

/**
 * This class is the internal RSA class that implements the
 * the wrapping and unwrapping algorithms. 
 * If the private key is not provided during the decryption phase,
 * this implementation sends the encrypted data to the AMI_Server
 * for decryption. This assumes that a previous login has been done
 * with the AMI_Server to register the private keys there.
 */

public class AMI_RSACrypt extends AMI_AsymmetricCipher {
    

    public AMI_RSACrypt() throws UnknownHostException, AMI_Exception {

        Properties props  = System.getProperties();

        AMI_C_Utils utils = new AMI_C_Utils();

	_userId = utils.ami_get_user_id();

	if (AMI_VirtualHost.getHostIP() == null)
	    _hostIP = InetAddress.getLocalHost().getHostAddress();
	else 
	    _hostIP = AMI_VirtualHost.getHostIP();
    }

    /**
     * Get the output encrypted or decrypted data len
     *
     */
    public int getDataLen() {
	return _dataLen;
    }
    
    /**
     * Get the Length of the key
     *
     * If key is null (possible in case of private key being used from the 
     * server), then the max key size is returned.
     * 
     */
    public int getKeyLen() {
        if (_key != null)
	   return _key.length;
	else
	  return 4096;  // Max key length.
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
	    throw new InvalidKeyException("Invalid key");
	
        if ((key instanceof AMI_RSAPublicKey) ||
	    (key instanceof AMI_RSAPrivateKey))
	    _key = key.getEncoded();
	else if (key instanceof AMI_PrivateKey) 
	  _keyAlias = ((AMI_PrivateKey)key).getAlias();
	else
	   throw new InvalidKeyException("Invalid key Format");
	
	if (!(key instanceof AMI_PrivateKey)  && (_key == null)) {
	    throw new InvalidKeyException
		("Invalid key data");
	}

	_keyAlg = key.getAlgorithm();
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
	    } else
	        actual = cipher;

	    cipherBlock(actual, cipherLen, plain);
	    plainOffset = 0;
    }


    private void cipherBlock(byte[] input, int inputLen, byte[] output) {

         AMI_Crypto crypto = null;
	 int length = 0;
	 byte[] tmpArray;

	 try {
	        crypto = new AMI_Crypto();
	 } catch (AMI_Exception e) {
		// e.printStackTrace();
	        return;
	 }

	 if (_key != null)
	    length = _key.length;

	 if (_decrypting) {  // Decrypting
	   try {

 	       if (_key != null) {
		    AMI_Debug.debugln(3, "AMI_RSACrypt:: trying to " +
			"decrypt data with key = " + _key);

		    crypto.ami_rsa_unwrap(input, inputLen, _keyAlg,
			_key, length, _wrapAlg);

		    tmpArray = crypto.getOutputData();

		    // Store the data len, to be retrieved later.
		    _dataLen = crypto.getOutputLen();
	        } else {
		   // Get instance of client, based on protocol
		   AMI_Debug.debugln(3,
			"AMI_RSACrypt:: invoking server decrypt");

		   AMI_KeyServ client = null;

		   try {
			client = AMI_KeyServClient.getInstance(
	  	        AMI_KeyMgntClient.getProperty(
			    AMI_Constants.PROTOCOL_PROPERTY));
		   } catch (Exception e) {
		      // e.printStackTrace();
		      return;
		   }

		   AMI_CryptoInfo info = new AMI_CryptoInfo(null,
			_hostIP, _userId, null,
			input, _wrapAlg, _keyAlias);

		   try {
		      tmpArray = client.unwrapData(info);
		      // Store the data len, to be retrieved later.
		      _dataLen =  tmpArray.length;
			// The return array is the correct length
		   } catch (Exception e) {
		      // e.printStackTrace();
		      return;
		   }
	        }
	   } catch (AMI_CryptoException e) {
	        // e.printStackTrace();
	        return;
	   } catch (Exception e) {
	        // e.printStackTrace();
		return;
	   }	     
	 } else {             // Encrypting
	   try {
   	        crypto.ami_rsa_wrap(input, inputLen, _keyAlg,
		    _key, length, _wrapAlg);
	        tmpArray = crypto.getOutputData();
	        // Store the data len, to be retrieved later.
	        _dataLen =  crypto.getOutputLen();
	   } catch (AMI_CryptoException e) {
	        // e.printStackTrace();
		return;
	   }	
	 }
	 
	 for (int ii = 0; ii < tmpArray.length; ii++)
	    output[ii] = tmpArray[ii];       
    }


    /*
     * the wrapping key information 
     */
    protected byte[] _key = null;
    protected String _keyAlg = null;
 
    /*
     * Wrapping Algorithm
     */
    protected String _wrapAlg = "RSA";   // Hard Coded here

    /*
     * Are we encrypting or decrypting?
     */
    protected boolean _decrypting = false;

    private int _dataLen = 0;  // Stores the length of encrypted
	// or decrypted o/p data

    private String  _keyAlias = null;
    private long    _userId;
    private String  _hostIP = null;

}
