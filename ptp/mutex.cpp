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

#define _GNU_SOURCE 1
#include <assert.h>
#include <ptp/mutex.h>
#include <ptp/debug.h>

/**
 * PTP::Mutex::Mutex: Class constructor.
 */
PTP::Mutex::Mutex()
{
#ifdef WIN32
	m_mutex = CreateMutex(0, FALSE, 0);
#else
	static pthread_mutex_t init
		= PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	memcpy(&m_mutex, &init, sizeof(m_mutex));
#endif
}

#ifdef PTPTL_DLL

/*
 * PTP::Mutex::Mutex: Copy constructor.
 * @mutex: Source Mutex.
 */
PTP::Mutex::Mutex(const Mutex& mutex)
{
	assert(0);
}

/*
 * PTP::Mutex::operator=: Copy constructor.
 * @mutex: Source Mutex.
 */
PTP::Mutex&
PTP::Mutex::operator=(const Mutex& mutex)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/**
 * PTP::Mutex::~Mutex: Class destructor.
 */
PTP::Mutex::~Mutex()
{
#ifdef WIN32
	CloseHandle(m_mutex);
#else
	pthread_mutex_destroy(&m_mutex);
#endif
}
	
/**
 * PTP::Mutex::Lock: Acquire mutex (blocking).
 * Notes: Once a mutex is acquired by a thread, it can be acquired again
 *        by the same thread without blocking (recursively).  &Unlock must
 *        be called an equal number of times as &Lock or &TryLock.
 */
void
PTP::Mutex::Lock()
{
#ifdef WIN32
	WaitForSingleObject(m_mutex, INFINITE);
#else
	pthread_mutex_lock(&m_mutex);
#endif
}

/**
 * PTP::Mutex::TryLock: Attempt to acquire mutex (non-blocking).
 * Returns: 0 on success or -1 if acquisition failed (lock is currently held).
 * Notes: Once a mutex is acquired by a thread, it can be acquired again
 *        by the same thread without blocking (recursively).  &Unlock must
 *        be called an equal number of times as &Lock or &TryLock.
 * Example:
 *   PTP::Mutex mutex;
 *   ...
 *   int acquired = (mutex.TryLock() == 0);
 *   printf("Mutex: %s\n", acquired ? "Acquired":"Not acquired");
 *   if (acquired)
 *       mutex.Unlock();
 */
int
PTP::Mutex::TryLock()
{
#ifdef WIN32
	DWORD status = WaitForSingleObject(m_mutex, 0);
	return ((status == WAIT_OBJECT_0) ? 0:-1);
#else
	int status = pthread_mutex_trylock(&m_mutex);
	return ((status == 0) ? 0:-1);
#endif
}

/**
 * PTP::Mutex::Unlock: Release mutex.
 * Notes: Once a mutex is acquired by a thread, it can be acquired again
 *        by the same thread without blocking (recursively).  &Unlock must
 *        be called an equal number of times as &Lock or &TryLock.
 */
void
PTP::Mutex::Unlock()
{
#ifdef WIN32
	ReleaseMutex(m_mutex);
#else
	pthread_mutex_unlock(&m_mutex);
#endif
}
