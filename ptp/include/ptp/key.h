/*
 * Copyright (c) 2001 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * 3. Neither the name of the Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PTP_KEY_H__
#define __PTP_KEY_H__

#include <openssl/sha.h>
#include <ptp/ptp.h>
#include <ptp/list.h>

/**
 * PTP::Key: Symmetric encryption support.
 * Synopsis: #include <ptp/key.h>
 * Notes: The symmetric cipher and key sizes used depend on the
 *        value of $PTP_SESSION_CIPHER and $PTP_SESSION_KEY_SIZE
 *        in ``ptp.h''.
 */
class EXPORT PTP::Key:public PTP::List::Entry
{
public:
	enum
	{
		/**
		 * PTP::Key::KEY_SIZE: Key data size.
		 */
		KEY_SIZE = PTP_SESSION_KEY_SIZE,

		IV_SIZE = PTP_SESSION_IV_SIZE,

		READ_SIZE_DEFAULT = 1024,
	};

	/**
	 * PTP::Key::Read: Read function.
	 * @data: [$OUT] Data buffer.
	 * @size: Maximum read size.
	 * @context: Function context data.
	 * Returns: Read size or -1 on error.
	 */
	typedef int (*Read)(BYTE *data, int size, void *context);

	/**
	 * PTP::Key::Write: Write function.
	 * @data: Data buffer.
	 * @size: Data size.
	 * @context: Function context data.
	 * Returns: Write size or -1 on error.
	 */
	typedef int (*Write)(const BYTE *data, int size, void *context);

	Key();
	Key(const BYTE *key);
	Key(const char *passwd, const BYTE *salt, int saltsize);
	~Key();

	int Encrypt(const BYTE *src,
		    int size,
		    BYTE *dst,
		    int iv = 1,
		    int digest = 1) const;
	int Decrypt(const BYTE *src,
		    int size,
		    BYTE *dst,
		    int iv = 1,
		    int digest = 1) const;

	int Encrypt(Read read,
		    Write write,
		    void *context,
		    int iv = 1,
		    int digest = 1,
		    int readsize = READ_SIZE_DEFAULT) const;
	int Decrypt(Read read,
		    Write write,
		    void *context,
		    int iv = 1,
		    int digest = 1,
		    int readsize = READ_SIZE_DEFAULT) const;
	static int Transfer(Read read,
			    Write write,
			    void *context,
			    int readsize = READ_SIZE_DEFAULT);

	int Export(BYTE *data) const;

protected:
	friend class PTP::Store;

	enum
	{
		DIGEST_PADDED_SIZE = PTP_DIGEST_SIZE * 2
	};

	Key(const Key& key);
	Key &operator=(const Key& key);

	static int ReadAll(Read read, BYTE *buffer, int size, void *context);
	static int WriteAll(Write write,
			    const BYTE *buffer,
			    int size,
			    void *context,
			    int *total);

	BYTE m_key[KEY_SIZE];
};

#endif // __PTP_KEY_H__
