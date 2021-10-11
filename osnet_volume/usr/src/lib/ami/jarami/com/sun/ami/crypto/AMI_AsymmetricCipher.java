/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_AsymmetricCipher.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.crypto;

import java.security.*;
import java.security.spec.*;
import javax.crypto.*;
import javax.crypto.spec.*;

public abstract class AMI_AsymmetricCipher { 

    /**
     * Gets the length of the final output data
     *
     * Had to add this as this was computed by the JCE funcs, whereas
     * in AMI , this is returned from the SKI functions.
     *
     * @return length of the encrypted/decrypted data in the output buffer
     */
    public abstract int getDataLen();
    

    /**
     * Gets the length of the Key (used for wrapping/ unwrapping)
     *
     *
     * @return length of the key in bytes
     */
    public abstract int getKeyLen();
    

    /**
     * Sets the cipher key.
     *
     * @param key The cipher key which is an object of type Key.
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this cipher
     */
    public abstract void init(Key key) throws InvalidKeyException;

    /**
     * Sets the cipher key and parameters.
     *
     * @param key The cipher key which is an object of type Key.
     * @param params An algorithmParameter spec object, containing the paramters
     * (if any) for this key.
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this cipher
     * @exception InvalidAlgorithmParameterException if the given algorithm
     * parameters are inappropriate for this cipher
     */
    public abstract void init(Key key, AlgorithmParameterSpec params)
        throws InvalidKeyException, InvalidAlgorithmParameterException;


    /**
     * Performs encryption operation.
     *
     * <p>The input <code>plain</code>, starting at <code>plainOffset</code>
     * and ending at <code>(plainOffset+plainLen-1)</code>, is encrypted.
     * The result is stored in <code>cipher</code>, starting at
     * <code>cipherOffset</code>.
     *
     * <p>The subclass that implements Cipher should ensure that
     * <code>init</code> has been called before this method is called.
     *
     * @param plain the input buffer with the data to be encrypted
     * @param plainOffset the offset in <code>plain</code>
     * @param plainLen the length of the input data
     * @param cipher the buffer for the encryption result
     * @param cipherOffset the offset in <code>cipher</code>
     *
     * @exception IllegalBlockSizeException if this cipher is a block cipher,
     * no padding has been requested, and the total input length is not a
     * multiple of this cipher's block size
     */
    public abstract void encrypt(byte[] plain, int plainOffset, int plainLen,
                                 byte[] cipher, int cipherOffset)
        throws IllegalBlockSizeException;

    /**
    * Performs decryption operation.
    *
    * @param cipher the input buffer with the data to be decrypted
    * @param cipherOffset the offset in < code > cipher < /code >
    * @param cipherLen the length of the input data
    * @param plain the buffer for the decryption result
    * @param plainOffset the offset in < code > plain < /code >
    *
    * @exception IllegalBlockSizeException if this cipher is a block cipher,
    * and the total input length is not a multiple of this cipher's block size
    */
    public abstract void decrypt(byte[] cipher, int cipherOffset,
                                 int cipherLen,
                                 byte[] plain, int plainOffset)
        throws IllegalBlockSizeException;


}
