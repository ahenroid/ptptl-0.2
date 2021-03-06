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

#ifndef __PTP_THREAD_H__
#define __PTP_THREAD_H__

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <ptp/ptp.h>

/**
 * PTP::Thread: Multi-thread support.
 * Synopsis: #include <ptp/thread.h>
 */
class EXPORT PTP::Thread
{
public:
	typedef void *(*StartFunc)(void *arg);

	Thread();

	int Start(StartFunc start, void *arg);
	int Wait();
	int Kill();

	static void Exit(int status = 0);
	static void Sleep(int sec);

protected:
	Thread(const Thread& thread);
	Thread& operator=(const Thread& thread);

	static void Terminate(int sig);
	
#ifdef WIN32
	HANDLE m_thread;
#else
	pthread_t m_thread;
#endif
};

#endif // __PTP_THREAD_H__
