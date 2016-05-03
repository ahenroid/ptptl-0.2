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

#include <string.h>
#include <assert.h>
#include <ptp/rand.h>
#include <ptp/auth.h>
#include <ptp/debug.h>

/**
 * PTP::Authenticator::Authenticator: Class constructor.
 * @store: Non-volatile certificate store.
 */
PTP::Authenticator::Authenticator(PTP::Store *store)
	:m_store(store)
{
	m_local = m_store->Find(NULL, 1, NULL, NULL);
}

#ifdef PTPTL_DLL

/*
 * PTP::Authenticator::Authenticator: Copy constructor.
 * @auth: Source Authenticator.
 */
PTP::Authenticator::Authenticator(const Authenticator& auth)
{
	assert(0);
}

/*
 * PTP::Authenticator::operator=: Copy constructor.
 * @auth: Source Authenticator.
 */
PTP::Authenticator&
PTP::Authenticator::operator=(const Authenticator& auth)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/**
 * PTP::Authenticator::~Authenticator: Class destructor.
 */
PTP::Authenticator::~Authenticator()
{
	// destroy pending responses
	m_pending.Lock();
	Pending *p;
	PTP_LIST_FOREACH(Pending, p, &m_pending)
	{
		m_pending.Remove(p, 0);
		delete p;
	}
	m_pending.Unlock();
}

/**
 * PTP::Authenticator::Challenge: Generate a challenge.
 * @id: Challenge subject.
 * @expire: Time (in seconds) until challenge expires.
 * @context: Context data to be returned from &Verify.
 * @chal: [$OUT] Challenge data (%CHALLENGE_SIZE bytes) or NULL.
 * Returns: %CHALLENGE_SIZE on success or -1 on error.
 * Notes: 
 *   Use a non-NULL value for @context so that a valid response can
 *   be differentiated from an invalid response upon return from
 *   &Verify.
 * Example:
 *   PTP::Authenticator *auth = ...;
 *   PTP::Identity *id = ...;
 *   BYTE chal[PTP::Authenticator::CHALLENGE_SIZE];
 *   auth->Challenge(id, 60, (void*) 1, chal);
 *   // send chal[]
 */
int
PTP::Authenticator::Challenge(const Identity *id,
			      unsigned expire,
			      void *context,
			      BYTE *chal)
{
	if (!id)
		return -1;
	if (chal)
	{
		// create a random nonce
		BYTE nonce[PTP::Identity::PLAINTEXT_SIZE];
		PTP::Random::Fill(nonce, sizeof(nonce));
		BYTE resp[RESPONSE_SIZE];

		// calculate nonce digest (expected response)
		EVP_MD_CTX digestCtx;
		EVP_DigestInit(&digestCtx, PTP_DIGEST);
		EVP_DigestUpdate(&digestCtx, nonce, sizeof(nonce));
		EVP_DigestFinal(&digestCtx, resp, 0);

		// encrypt nonce (challenge)
		if (id->Encrypt(nonce, sizeof(nonce), chal) != CHALLENGE_SIZE)
			return -1;

		// save expected response
		Pending *p = new Pending;
		memcpy(p->m_resp, resp, sizeof(p->m_resp));
		p->m_expire = GetTime() + expire;
		p->m_context = context;
		m_pending.Insert(p);
	}
	return CHALLENGE_SIZE;
}

/**
 * PTP::Authenticator::Respond: Generate a response to a challenge.
 * @chal: Challenge data (%CHALLENGE_SIZE bytes).
 * @resp: [$OUT] Response data (%RESPONSE_SIZE bytes) or NULL.
 * Returns: %RESPONSE_SIZE on success or -1 on error.
 * Example:
 *   PTP::Authenticator *auth = ...;
 *   BYTE chal[PTP::Authenticator::CHALLENGE_SIZE];
 *   // receive chal[]
 *   BYTE resp[PTP::Authenticator::RESPONSE_SIZE];
 *   auth->Respond(chal, resp);
 *   // send resp[]
 */
int
PTP::Authenticator::Respond(const BYTE *chal, BYTE *resp) const
{
	if (resp)
	{
		if (!chal || !m_local)
			return -1;

		// decrypt nonce
		BYTE nonce[PTP::Identity::PLAINTEXT_SIZE];
		if (m_local->Decrypt(chal, nonce) != sizeof(nonce))
			return -1;

		// calculate nonce digest (response)
		EVP_MD_CTX digestCtx;
		EVP_DigestInit(&digestCtx, PTP_DIGEST);
		EVP_DigestUpdate(&digestCtx, nonce, sizeof(nonce));
		EVP_DigestFinal(&digestCtx, resp, 0);
	}
	return RESPONSE_SIZE;
}

/**
 * PTP::Authenticator::Verify: Verify a response to a challenge.
 * @resp: Response data (%RESPONSE_SIZE bytes).
 * Returns: Context data from &Challenge or NULL for an invalid response.
 * Example:
 *   PTP::Authenticator *auth = ...;
 *   PTP::Identity *id = ...;
 *   BYTE chal[PTP::Authenticator::CHALLENGE_SIZE];
 *   auth->Challenge(id, 60, (void*) 1, chal);
 *   // send chal[]
 *   BYTE resp[PTP::Authenticator::RESPONSE_SIZE];
 *   // receive resp[]
 *   int ok = ((int) auth->Verify(resp) == 1);
 */
void *
PTP::Authenticator::Verify(const BYTE *resp)
{
	if (!resp)
		return NULL;

	unsigned long now = GetTime();
	void *context = NULL;

	// find matching response
	m_pending.Lock();
	Pending *p;
	PTP_LIST_FOREACH(Pending, p, &m_pending)
	{
		if (p->m_expire <= now)
		{
			// remove expired response
			m_pending.Remove(p, 0);
			delete p;
		}
		else if (memcmp(p->m_resp,
				resp,
				sizeof(p->m_resp)) == 0)
		{
			// matching response
			m_pending.Remove(p, 0);
			context = p->m_context;
			delete p;
			break;
		}
	}
	m_pending.Unlock();

	return context;
}

/**
 * PTP::Authenticator::GetTime
 * Type: static
 * Returns: Current time (in seconds).
 * Notes: The base time (whether the Epoch or simply boot time) is arbitrary.
 */
unsigned long
PTP::Authenticator::GetTime()
{
#ifdef WIN32
	return (GetTickCount() / 1000);
#else
	return time(NULL);
#endif
}
