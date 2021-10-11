/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RC2Cipher.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.crypto;

import java.security.*;
import java.security.spec.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import javax.crypto.BadPaddingException;
import com.sun.crypto.provider.Padding;

/**
 * This class implements the RC2 algorithm  with the 
 *  <code>CBC</code> mode.
 * 
 * @author Sangeeta Varma
 * @version 
 * @see AMI_RC2Crypt
 */

public class AMI_RC2Cipher extends CipherSpi {

    public static final int BLOCK_SIZE = 8;
    public static final int EXTRA_UPDATE_BYTES = 24;

    /*
     * the cipher mode (Can only be CBC as of now , as SKI supports only CBC)
     */
    private int cipherMode;

    /*
     * are we encrypting or decrypting?
     */
    private boolean decrypting = false;

    /*
     * Block Mode constants
     */
    private static final int CBC_MODE = 1;
 
    /*
     * The (raw) algorithm. This is the implementation of the raw RC2 
     * algorithm.
     */
    protected AMI_SymmetricCipher rawAlg;

    /**
     * Creates an instance of RC2 cipher with default CBC mode and
     * PKCS5Padding.
     */
    public AMI_RC2Cipher() {
	    setRawAlg();


	    // set default mode and padding (for future use maybe ?)
	    cipherMode = CBC_MODE;
    }

    /**
     * Creates an instance of RC2 cipher with the requested mode.
     *
     * @param mode the cipher mode
     * @param paddingScheme the padding mechanism
     *
     * @exception NoSuchAlgorithmException if the required cipher mode is
     * unavailable
     * @exception NoSuchPaddingException if the required padding mechanism
     * is unavailable
     */
    public AMI_RC2Cipher(String mode, String paddingScheme)
	throws NoSuchAlgorithmException, NoSuchPaddingException {
	    setRawAlg();

	    // set mode and padding (for future use maybe ?)

            engineSetMode(mode);
	    engineSetPadding(paddingScheme);
    }

    /**
     * Sets the raw algorithm to RC2. This method is called by the constructor,
     * and can be overwritten by any subclass.
     */
    protected void setRawAlg() {
	rawAlg = new AMI_RC2Crypt();
    }

    /**
     * Sets the mode of this cipher.
     *
     * @param mode the cipher mode
     *
     * @exception NoSuchAlgorithmException if the requested cipher mode does
     * not exist
     */
    public void engineSetMode(String mode) throws NoSuchAlgorithmException {
	
	if (mode == null) 
	    throw new NoSuchAlgorithmException("null mode");

	String modeUpperCase = mode.toUpperCase();

	if (modeUpperCase.equals("CBC")) {
	    cipherMode = CBC_MODE;
	}

	else {
	    throw new NoSuchAlgorithmException("Cipher Mode: " + mode
					       + " not supported");
	}
    }

    /**
     * Sets the padding mechanism of this cipher.
     *
     * @param padding the padding mechanism
     *
     * @exception NoSuchPaddingException if the requested padding mechanism
     * does not exist
     */
    public void engineSetPadding(String paddingScheme) 
	throws NoSuchPaddingException {
	/*
	 * if (paddingScheme == null)
	 *	throw new NoSuchPaddingException("null padding");
	 *
	 *    if (paddingScheme.equalsIgnoreCase("PKCS5Padding")) {
	 *	return;
	 *    } else if (paddingScheme.equalsIgnoreCase("NoPadding")) {
	 *	padding = null;
	 *	// xxx we could also shrink the buffer here, but is it worth
	 *	// it??
	 *    } else {
	 *
	 */
		throw new NoSuchPaddingException("Paddding: " +
		    paddingScheme + " not implemented");
	// }
    }

    /**
     * Returns the block size (in bytes).
     *
     * @return the block size (in bytes), or 0 if the underlying algorithm is
     * not a block cipher
     */
    protected int engineGetBlockSize() {
	return BLOCK_SIZE;
    }

    /**
     * Returns the length in bytes that an output buffer would need to be in
     * order to hold the result of the next <code>update</code> or
     * <code>doFinal</code> operation, given the input length
     * <code>inputLen</code> (in bytes).
     *
     * <p>This call takes into account any unprocessed (buffered) data from a
     * previous <code>update</code> call, and padding.
     *
     * <p>The actual output length of the next <code>update</code> or
     * <code>doFinal</code> call may be smaller than the length returned by
     * this method.
     * The requirement for the underlying BSafe library is 24 extra bytes.
     *
     * @param inputLen the input length (in bytes)
     *
     * @return the required output buffer size (in bytes)
     */
    protected int engineGetOutputSize(int inputLen) {
	 return (inputLen + EXTRA_UPDATE_BYTES);	    
    }
    
    /**
     * Returns the initialization vector (IV) in a new buffer.
     *
     * <p>This is useful in the case where a random IV has been created
     * (see <a href = "#init">init</a>),
     * or in the context of password-based encryption or
     * decryption, where the IV is derived from a user-provided passphrase. 
     *
     * @return the initialization vector in a new buffer, or null if the
     * underlying algorithm does not use an IV, or if the IV has not yet
     * been set.
     */
    protected byte[] engineGetIV() {	
	    return (rawAlg.getIV());
    }

    /*
     * Returns the parameters used with this cipher. 
     * The returned parameters may be the same that were used to
     * initialize this cipher, or
     * may contain a combination of default and random parameter
     * values used by the
     * underlying cipher implementation if this cipher requires
     * algorithm parameters but
     * was not initialized with any.
     * Returns:the parameters used with this cipher
     */
    protected AlgorithmParameters engineGetParameters() {	

        AlgorithmParameters algorithmParameters = null;
        int keySize = 0;
	byte[] iv = new byte[BLOCK_SIZE];

        if (engineGetIV() == null) {
           SecureRandom random = new SecureRandom();
           random.nextBytes(iv);
        }
        if (getEffectiveKeySize() <= 0)
           keySize = 40; // intialize the RC2 effective key
			// size to 40, if not available
        RC2ParameterSpec rc2ParameterSpec = new RC2ParameterSpec(keySize, iv);
        try
        {
            algorithmParameters = AlgorithmParameters.getInstance(
		"RC2", "SunAMI");
        }
        catch (NoSuchAlgorithmException nsae)
        {
            throw new RuntimeException(nsae.toString());
        }
        catch (NoSuchProviderException nspe)
        {
            throw new RuntimeException(nspe.toString());
        }
        try
        {
            algorithmParameters.init(rc2ParameterSpec);
            return algorithmParameters;
        }
        catch (InvalidParameterSpecException ipse)
        {
            throw new RuntimeException(ipse.toString());
        }
    }

    /**
     */
    private int getEffectiveKeySize() {
	return (rawAlg.getEffectiveKeySize());
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
            RC2ParameterSpec rc2Spec = null;
	    if (opmode == Cipher.DECRYPT_MODE) {
		decrypting = true;
                throw new InvalidKeyException(
		    "Cannot decrypt without Parameters");
            }
	    else
		decrypting = false;
	    
	    if (key == null) {
		throw new InvalidKeyException("No key given");
	    }
	    try {
                AlgorithmParameters algParam = engineGetParameters();
                engineInit(opmode, key, algParam, random);
	    } catch (Exception e) {
		      throw new InvalidKeyException(e.toString());
	    }

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
	AlgorithmParameterSpec params, SecureRandom random)
	throws InvalidKeyException, InvalidAlgorithmParameterException {
	    
	    if (opmode == Cipher.DECRYPT_MODE)
		decrypting = true;
	    else
		decrypting = false;
	    
	    if (key == null) {
		throw new InvalidKeyException("No key given");
	    }
	    byte[] iv = new byte[BLOCK_SIZE];

	    if (params != null) {
		if (params instanceof RC2ParameterSpec) {
		  if (((RC2ParameterSpec) params).getIV() == null) {
		       random.nextBytes(iv);
		       RC2ParameterSpec newParams = new RC2ParameterSpec(
			((RC2ParameterSpec)params).getEffectiveKeyBits(), iv);
	 
		       rawAlg.init(key, newParams);
		  }
		  else
		       rawAlg.init(key, params);

		} else {
		    throw new InvalidAlgorithmParameterException
			("Wrong Paramter type :: RC2ParameterSpec expected");
		}
	    } else 
	        throw new InvalidAlgorithmParameterException(
			"Parameter RC2ParameterSpec expected");
    }	    

    protected void engineInit(int opmode, Key key,
			      AlgorithmParameters params,
			      SecureRandom random)
	throws InvalidKeyException, InvalidAlgorithmParameterException {
	    
	    RC2ParameterSpec rc2Spec = null;

	    if (opmode == Cipher.DECRYPT_MODE)
		decrypting = true;
	    else
		decrypting = false;
	    
	    if (key == null) {
		throw new InvalidKeyException("No key given");
	    }

	    if (params != null) {
		try {
		        rc2Spec = 
		        (RC2ParameterSpec)params.getParameterSpec(
			Class.forName("javax.crypto.spec.RC2ParameterSpec"));
		} catch (Exception e) {
		      throw new InvalidAlgorithmParameterException(
			e.toString());
		}
		engineInit(opmode, key, rc2Spec, random);
	    } else
	        throw new InvalidAlgorithmParameterException(
		    "Null AlgorithmParameter passed !");
    }

    /**
     * Continues a multiple-part encryption or decryption operation
     * (depending on how this cipher was initialized), processing another data
     * part.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, are processed, and the
     * result is stored in a new buffer.
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     *
     * @return the new buffer with the result
     *
     * @exception IllegalStateException if this cipher is in a wrong state
     * (e.g., has not been initialized)
     */
    protected byte[] engineUpdate(byte[] input, int inputOffset,
				  int inputLen) {
	    byte[] output = null;
	    byte[] out = null;
	    try {
		output = new byte[engineGetOutputSize(inputLen)];
		int len = engineUpdate(input, inputOffset, inputLen, output,
				       0);
		if (len < output.length) {
		    out = new byte[len];
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
     * Continues a multiple-part encryption or decryption operation
     * (depending on how this cipher was initialized), processing another data
     * part.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, are processed, and the
     * result is stored in the <code>output</code> buffer, starting at
     * <code>outputOffset</code>.
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
     * @exception ShortBufferException if the given output buffer is too small
     * to hold the result
     */
    protected int engineUpdate(byte[] input, int inputOffset, int inputLen,
			       byte[] output, int outputOffset)
	throws ShortBufferException {


          	rawAlg.setStage(AMI_RC2Crypt.UPDATE); // set stage to update

		try {
		    if (decrypting)
			rawAlg.decrypt(input, 0, inputLen, output,
			    outputOffset);
		    else
			rawAlg.encrypt(input, 0, inputLen, output,
			    outputOffset);
		} catch (IllegalBlockSizeException e) {
		    // never thrown
		}
		
	    return rawAlg.getDataLen();
    }

    /**
     * Encrypts or decrypts data in a single-part operation,
     * or finishes a multiple-part operation.
     * The data is encrypted or decrypted, depending on how this cipher was
     * initialized.
     *
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
     * Encrypts or decrypts data in a single-part operation,
     * or finishes a multiple-part operation.
     * The data is encrypted or decrypted, depending on how this cipher was
     * initialized.
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
     * @exception IllegalBlockSizeException if this cipher is a block cipher,
     * no padding has been requested (only in encryption mode), and the total
     * input length of the data processed by this cipher is not a multiple of
     * block size
     * @exception ShortBufferException if the given output buffer is too small
     * to hold the result
     * @exception BadPaddingException if this cipher is in decryption mode,
     * and (un)padding has been requested, but the decrypted data is not
     * bounded by the appropriate padding bytes
     */
    protected int engineDoFinal(byte[] input, int inputOffset, int inputLen,
				byte[] output, int outputOffset)
	throws IllegalBlockSizeException, ShortBufferException, 
	       BadPaddingException {

                rawAlg.setStage(AMI_RC2Crypt.FINAL); // set stage to Final

		try {
		    if (decrypting)
			rawAlg.decrypt(input, 0, inputLen, output,
			    outputOffset);
		    else
			rawAlg.encrypt(input, 0, inputLen, output,
			    outputOffset);
		} catch (IllegalBlockSizeException e) {
		    // never thrown
		}
		
	    return rawAlg.getDataLen();
    }
}
