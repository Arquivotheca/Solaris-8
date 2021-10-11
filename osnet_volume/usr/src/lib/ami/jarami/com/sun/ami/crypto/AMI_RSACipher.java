/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_RSACipher.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.crypto;

import java.security.*;
import java.security.spec.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import javax.crypto.BadPaddingException;
import java.net.UnknownHostException;

import com.sun.ami.AMI_Exception;
import com.sun.ami.keygen.AMI_RSAPublicKey;
import com.sun.ami.keygen.AMI_RSAPrivateKey;
import com.sun.ami.keygen.AMI_PrivateKey;

/**
 * This class provides an RSA Cipher, which is used to wrap
 * and unwrap symmetric keys.
 * This implementation of the RSA Cipher should be used, if 
 * the user intends to be able to perform this operation using
 * the AMI Server.
 */

public class AMI_RSACipher extends CipherSpi {

    /**
     * Creates an instance of RSA cipher 
     */
    public AMI_RSACipher() throws Exception {
          setRawAlg();
    }

    /**
     * Sets the raw algorithm to RSA. This method is called by the constructor,
     * and can be overwritten by any subclass.
     */
    protected void setRawAlg() throws UnknownHostException, AMI_Exception {
	_rawAlg = new AMI_RSACrypt();
    }

    /**
     * Sets the mode of this cipher.
     *
     * @param mode the cipher mode
     *
     * @exception NoSuchAlgorithmException No modes allowed for RSA.
     */
    public void engineSetMode(String mode) throws NoSuchAlgorithmException {
	    throw new NoSuchAlgorithmException(
		"No Cipher Modes Allowed for RSA");
    }

    /**
     * Sets the padding mechanism of this cipher.
     *
     * @param padding the padding mechanism
     *
     * @exception NoSuchPaddingException No padding allowed for RSA.
     */
    public void engineSetPadding(String paddingScheme) 
	throws NoSuchPaddingException {

	    if (paddingScheme == null)
		throw new NoSuchPaddingException("null padding");

	    if (!(paddingScheme.equalsIgnoreCase("NoPadding"))) {
		throw new NoSuchPaddingException("Padding: " +
						 paddingScheme + 
						 " not allowed for RSA");
	    }
    }

    /**
     * Returns the block size (in bytes). 
     *
     * @return the block size (in bytes). 0 for RSA.
     */
    protected int engineGetBlockSize() {
	return 0;
    }

    /**
     * Returns the length in bytes that an output buffer would need to be in
     * order to hold the result of the <code>doFinal</code> operation,
     * given the input length <code>inputLen</code> (in bytes).
     *
     * For RSA, the output length  will always be the length of the wrapping
     * key.
     *
     * @param inputLen the input length (in bytes)
     *
     * @return the required output buffer size (in bytes), which will be the
     * sixe of the wrapping key.
     */
    protected int engineGetOutputSize(int inputLen) {	  

	return _rawAlg.getKeyLen();
    }
    
    /**
     * Returns the initialization vector (IV), null in this case. 
     *
     * @return the initialization vector , NULL for RSA.
     */
    protected byte[] engineGetIV() {	
	return null;  // No IV for RSA
    }

    protected AlgorithmParameters engineGetParameters() {
	return null;  // No params for RSA
    }

	
    /**
     * Initializes this cipher with a key and a source of randomness.
     * 
     * <p>The cipher is initialized for encryption or decryption, depending on
     * the value of <code>opmode</code>.
     *
     * <p>If this cipher requires an initialization vector (IV), it will get
     * it from <code>random</code>.
     * This behaviour should only be used in encryption mode, however.
     * When initializing a cipher that requires an IV for decryption, the IV
     * (same IV that was used for encryption) must be provided explicitly as a
     * parameter, in order to get the correct result.
     *
     * <p>This method also cleans existing buffer and other related state
     * information.
     *
     * @param opmode the operation mode of this cipher (this is either
     * <code>ENCRYPT_MODE</code> or <code>DECRYPT_MODE</code>)
     * @param key the secret key
     * @param random the source of randomness
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this cipher
     */
    protected void engineInit(int opmode, Key key, SecureRandom random)
	throws InvalidKeyException {

	    if (opmode == Cipher.DECRYPT_MODE) {
		_decrypting = true;
	    } else {
		_decrypting = false;
	    }

	    if ((_decrypting) && (key == null))
	        return;
	    else if ((!_decrypting) && (key == null)) 
		throw new InvalidKeyException("No key given");

	    if ((_decrypting) && !(key instanceof AMI_PrivateKey) &&
		!(key instanceof AMI_RSAPublicKey) &&
		!(key instanceof AMI_RSAPrivateKey))
	        throw new InvalidKeyException("Key has to be a " +
		    "RSAPublicKey or RSAPrivateKey as only RSA can be " +
		    "used for wrap/unwrap ");

	    if ((!_decrypting) && !(key instanceof AMI_RSAPublicKey) &&
		!(key instanceof AMI_RSAPrivateKey))
	      	throw new InvalidKeyException("Key has to be a " +
		    "RSAPublicKey or RSAPrivateKey as only RSA " +
		    "can be used for wrap/unwrap ");

	    _rawAlg.init(key);
	    return;

    }

    /**
     * Initializes this cipher with a key, a set of
     * algorithm parameters, and a source of randomness.
     *
     * <p>The cipher is initialized for encryption or decryption, depending on
     * the value of <code>opmode</code>.
     *
     * <p>If this cipher (including its underlying feedback or padding scheme)
     * requires any random bytes, it will get them from <code>random</code>.
     *
     * @param opmode the operation mode of this cipher (this is either
     * <code>ENCRYPT_MODE</code> or <code>DECRYPT_MODE</code>)
     * @param key the encryption key
     * @param params the algorithm parameters
     * @param random the source of randomness
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this cipher
     * @exception InvalidAlgorithmParameterException if the given algorithm
     * parameters are inappropriate for this cipher
     */
    protected void engineInit(int opmode, Key key,
    AlgorithmParameterSpec params,
    SecureRandom random)
    throws InvalidKeyException, InvalidAlgorithmParameterException {
	    
	    if (opmode == Cipher.DECRYPT_MODE)
		_decrypting = true;
	    else
		_decrypting = false;	   

	    if ((_decrypting) && (key == null))
	        return;
	    else if ((!_decrypting) && (key == null)) 
		throw new InvalidKeyException("No key given");

	    if ((_decrypting) && !(key instanceof AMI_PrivateKey) &&
		!(key instanceof AMI_RSAPublicKey) &&
		!(key instanceof AMI_RSAPrivateKey))
	        throw new InvalidKeyException("Key has to be a " +
		    "RSAPublicKey or RSAPrivateKey as only RSA can " +
		    "be used for wrap/unwrap ");

	    if ((!_decrypting) && !(key instanceof AMI_RSAPublicKey) &&
		!(key instanceof AMI_RSAPrivateKey))
	      	throw new InvalidKeyException("Key has to be a " +
		    "RSAPublicKey or RSAPrivateKey as only RSA can " +
		    "be used for wrap/unwrap ");

	    if (params != null) {
		 throw new InvalidAlgorithmParameterException
			("No  parameters expected");
	    } else {				    	
		 _rawAlg.init(key);
	    }
    }

    protected void engineInit(int opmode, Key key,
                              AlgorithmParameters params,
                              SecureRandom random)
        throws InvalidKeyException, InvalidAlgorithmParameterException {
            if (opmode == Cipher.DECRYPT_MODE)
                _decrypting = true;
            else
                _decrypting = false;

            if ((_decrypting) && (key == null))
                return;
            else if ((!_decrypting) && (key == null))
                throw new InvalidKeyException("No key given");

            if (!(key instanceof AMI_RSAPublicKey) &&
		!(key instanceof AMI_RSAPrivateKey))
                throw new InvalidKeyException("Key has to be a " +
		    "RSAPublicKey or RSAPrivateKey as only RSA can " +
		    "be used for wrap/unwrap ");

            if (params != null) {
                 throw new InvalidAlgorithmParameterException
                        ("No  parameters expected");
            } else {
                 _rawAlg.init(key);
            }
    }

    /**
     * Continues a multiple-part encryption or decryption operation
     * (depending on how this cipher was initialized), processing another data
     * part.
     *
     * THIS IS NOT ALLOWED FOR RSA Cipher, as RSA wrap/unwrap is a one step
     * process. The doFinal() methods only should be used.
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     *
     * @return the new buffer with the result
     *
     * @exception IllegalStateException  NOT ALLOWED TO DO UPDATE
     */
    protected byte[] engineUpdate(byte[] input, int inputOffset,
				  int inputLen) {
        throw new IllegalStateException("RSA cipher operations " +
	    "can only be single step. Use doFinal() methods only ");
    }

    /**
     * Continues a multiple-part encryption or decryption operation
     * (depending on how this cipher was initialized), processing another data
     * part.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, are processed, and the
     * result is stored in the <code>output</code> buffer, starting at
     * <code>outputOffset</code>.
     *
     * THIS IS NOT ALLOWED FOR RSA Cipher, as RSA wrap/unwrap is a one step
     * process. The doFinal() methods only should be used.
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     * @param output the buffer for the result
     * @param outputOffset the offset in <code>output</code> where the result
     * is stored
     *
     * @return the number of bytes stored in <code>output</code>
     *
     * @exception IllegalStateException  NOT ALLOWED TO DO UPDATE
     */
    protected int engineUpdate(byte[] input, int inputOffset, int inputLen,
			       byte[] output, int outputOffset)
	throws IllegalStateException {
        throw new IllegalStateException("RSA cipher operations " +
	    "can only be single step. Use doFinal() methods only ");
    }

    /**
     * Encrypts or decrypts data in a single-part operation,
     * The data is encrypted or decrypted, depending on how this cipher was
     * initialized.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, and any input bytes that
     * may have been buffered during a previous <code>update</code> operation,
     * are processed, with padding (if requested) being applied.
     * The result is stored in a new buffer.
     *
     * <p>The cipher is reset to its initial state (uninitialized) after this
     * call.
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     *
     * @return the new buffer with the result
     *
     * @exception IllegalBlockSizeException if this cipher is a block cipher,
     * no padding has been requested (only in encryption mode), and the total
     * input length of the data processed by this cipher is not a multiple of
     * block size
     * @exception BadPaddingException if this cipher is in decryption mode,
     * and (un)padding has been requested, but the decrypted data is not
     * bounded by the appropriate padding bytes
     */
    protected byte[] engineDoFinal(byte[] input, int inputOffset, int inputLen)
	throws IllegalBlockSizeException, BadPaddingException {
	    byte[] output = null;
	    byte[] out = null;
	    try {
		output = new byte[engineGetOutputSize(inputLen)];
		int len = engineDoFinal(input, inputOffset, inputLen,
					output, 0);
		if (len < output.length) {
		    out = new byte[len];
		    if (len != 0)
			System.arraycopy(output, 0, out, 0, len);
		} else {
		    out = output;
		}
	    } catch (ShortBufferException e) {
		// never thrown
	    }
	    return out;
    }

    /**
     * Encrypts or decrypts data in a single-part operation
     * The data is encrypted or decrypted, depending on how this cipher was
     * initialized.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, and any input bytes that
     * may have been buffered during a previous <code>update</code> operation,
     * are processed, with padding (if requested) being applied.
     * The result is stored in the <code>output</code> buffer, starting at
     * <code>outputOffset</code>.
     *
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     * @param output the buffer for the result
     * @param outputOffset the offset in <code>output</code> where the result
     * is stored
     *
     * @return the number of bytes stored in <code>output</code>
     *
     * @exception IllegalBlockSizeException Not thrown
     * @exception ShortBufferException if the given output buffer is too small
     * to hold the result
     * @exception BadPaddingException  Not thrown
     */
    protected int engineDoFinal(byte[] input, int inputOffset, int inputLen,
				byte[] output, int outputOffset)
	throws IllegalBlockSizeException, ShortBufferException, 
	       BadPaddingException {

	    if (_decrypting) {
		_rawAlg.decrypt(input, inputOffset, inputLen,
		    output, outputOffset);
	    } else {
		_rawAlg.encrypt(input, inputOffset, inputLen,
		    output, outputOffset);
	    }

	    // return the number of bytes in the output buffer,
	    // which actually has data
	    return _rawAlg.getDataLen();
    }

    /*
     * are we encrypting or decrypting?
     */
    private boolean _decrypting = false;

    /*
     * The (raw) algorithm. This is the implementation of the raw RSA 
     * algorithm.
     */
    protected AMI_AsymmetricCipher _rawAlg;

}
