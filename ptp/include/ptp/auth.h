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

#ifndef __PTP_AUTH_H__
#define __PTP_AUTH_H__

#include <ptp/ptp.h>
#include <ptp/list.h>
#include <ptp/id.h>
#include <ptp/store.h>

/**
 * PTP::Authenticator: Authentication support.
 * Synopsis: #include <ptp/auth.h>
 */
class EXPORT PTP::Authenticator
{
public:
	enum
	{
		/**
		 * PTP::Authenticator::CHALLENGE_SIZE
		 *
		 * Challenge data size for &Challenge and &Respond.
		 */
		CHALLENGE_SIZE = PTP::Identity::CIPHERTEXT_SIZE,
		
		/**
		 * PTP::Authenticator::RESPONSE_SIZE
		 *
		 * Response data size for &Respond and &Verify.
		 */
		RESPONSE_SIZE = PTP_DIGEST_SIZE
	};

	Authenticator(PTP::Store *store);
	virtual ~Authenticator();

	virtual int Challenge(const PTP::Identity *id,
			      unsigned expire,
			      void *context,
			      BYTE *chal);
	virtual int Respond(const BYTE *chal, BYTE *resp) const;
	virtual void *Verify(const BYTE *resp);

	static unsigned long GetTime();

protected:
	/*
	 * PTP::Authenticator::Pending: Pending response
	 */
	struct Pending:public PTP::List::Entry
	{
		BYTE m_resp[RESPONSE_SIZE];
		unsigned long m_expire;
		void *m_context;
	};

	Authenticator(const Authenticator& auth);
	Authenticator& operator=(const Authenticator& auth);

	PTP::Store *m_store;
	PTP::Identity *m_local;
	PTP::List m_pending;
};

#endif // __PTP_AUTH_H__
