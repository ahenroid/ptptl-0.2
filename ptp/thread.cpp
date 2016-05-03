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

#ifndef WIN32
#include <signal.h>
#include <unistd.h>
#endif
#include <assert.h>
#include <ptp/thread.h>
#include <ptp/debug.h>

/**
 * PTP::Thread::Thread: Class constructor.
 */
PTP::Thread::Thread()
{
}

#ifdef PTPTL_DLL

/*
 * PTP::Thread::Thread: Copy constructor.
 * @thread: Source Thread.
 */
PTP::Thread::Thread(const Thread& thread)
{
	assert(0);
}

/*
 * PTP::Thread::operator=: Copy constructor.
 * @thread: Source Thread.
 */
PTP::Thread&
PTP::Thread::operator=(const Thread& thread)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/**
 * PTP::Thread::Start: Execute the @start function in a new thread.
 * @start: Function to execute.
 * @arg: Argument to @start.
 * Returns: 0 on success or -1 on failure.
 * Notes: The thread terminates upon return from @start.
 * Example:
 *   void *PrintThread(void *arg)
 *   {
 *       printf("%s\n", (const char*) arg);
 *       return NULL;
 *   }
 *   ...
 *   PTP::Thread thread;
 *   thread.$Start(PrintThread, "Running...");
 *   thread.Wait();
 */
int
PTP::Thread::Start(StartFunc start, void *arg)
{
#ifdef WIN32
	DWORD id = 0;
	m_thread = CreateThread(0,
				0,
				(LPTHREAD_START_ROUTINE) start,
				arg,
				0,
				&id);
	return ((m_thread != NULL) ? 0:-1);
#else
	int status = pthread_create(&m_thread, 0, start, arg);
	return ((status == 0) ? 0:-1);
#endif
}

/**
 * PTP::Thread::Wait: Wait for the thread to terminate.
 * Returns: Thread exit status on success or -1 on failure.
 * Example:
 *   void *RunThread(void *arg) {...}
 *   ...
 *   PTP::Thread thread;
 *   thread.Start(RunThread, NULL);
 *   thread.$Wait();
 */
int
PTP::Thread::Wait()
{
#ifdef WIN32
	DWORD status = 0;
	if (WaitForSingleObject(m_thread, INFINITE) != WAIT_OBJECT_0
	    || GetExitCodeThread(m_thread, &status) == 0)
		status = (DWORD) -1;
	CloseHandle(m_thread);
	m_thread = INVALID_HANDLE_VALUE;
#else
	void *status = NULL;
	if (pthread_join(m_thread, &status) || status == PTHREAD_CANCELED)
		status = (void*) -1;
#endif
	return (int) status;
}

/**
 * PTP::Thread::Kill: Terminate thread and wait for completion.
 * Returns: 0 on success or -1 on failure.
 * Example:
 *   void *RunThread(void *arg) {...}
 *   ...
 *   PTP::Thread thread;
 *   thread.Start(RunThread, NULL);
 *   thread.$Kill();
 */
int
PTP::Thread::Kill()
{
#ifdef WIN32
	BOOL status = TerminateThread(m_thread, 0);
	CloseHandle(m_thread);
	m_thread = INVALID_HANDLE_VALUE;
	return (status ? 0:-1);
#else
	void (*handler)(int) = signal(SIGTERM, Terminate);
	int status = pthread_kill(m_thread, SIGTERM);
	if (!status)
		Wait();
	signal(SIGTERM, handler);
	return ((status == 0) ? 0:-1);
#endif
}

/**
 * PTP::Thread::Exit: Terminate the current thread.
 * Type: static
 * @status: Exit status.
 * Example:
 *   $PTP::Thread::Exit(1);
 */
void
PTP::Thread::Exit(int status)
{
#ifdef WIN32
	ExitThread((DWORD) status);
#else
	pthread_exit((void*) status);
#endif
}

/**
 * PTP::Thread::Sleep: Sleep for the specified amount of time.
 * Type: static
 * @sec: Time (in seconds) to sleep.
 * Example:
 *   $PTP::Thread::Sleep(30);
 */
void
PTP::Thread::Sleep(int sec)
{
#ifdef WIN32
	::Sleep(sec * 100);
#else
	sleep(sec);
#endif
}

/*
 * PTP::Thread::Terminate: Abnormally terminate the current thread.
 * Type: static
 * @sig: Signal number.
 */
void
PTP::Thread::Terminate(int sig)
{
	PTP::Thread::Exit(-1);
}
