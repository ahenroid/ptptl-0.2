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

#ifndef __PTP_LIST_H__
#define __PTP_LIST_H__

#include <ptp/ptp.h>
#include <ptp/mutex.h>

/**
 * PTP::List: Doubly-linked list with lock.
 * Synopsis: #include <ptp/list.h>
 */
class EXPORT PTP::List
{
public:
	/**
	 * PTP::List::Entry: Basic list entry.
	 */
	class EXPORT Entry
	{
	public:
		Entry();
		
		Entry *GetNext();
		Entry *GetPrev();

	private:
		friend class PTP::List;

		Entry(const Entry& entry);
		Entry& operator=(const Entry& entry);

		Entry *m_next;
		Entry *m_prev;
	};

	List();
	List(int locking);
	~List();

	void Insert(Entry *entry, int lock = 1);
	void Append(Entry *entry, int lock = 1);
	void Remove(Entry *entry, int lock = 1);

	void Lock();
	void Unlock();

	int IsValid(Entry *entry) const;
	Entry *GetHead();
	Entry *GetTail();

protected:
	List(const List& list);
	List& operator=(const List& list);

	int m_locking;
	PTP::Mutex m_mutex;
	Entry m_head;
};

/**
 * PTP_LIST_FOREACH: Iterate through each element of a list.
 * @type: Iterator type.
 * @iterator: Iterator variable.
 * @list: List.
 * Notes: The list should be locked in multi-threaded environments.
 *        Also, note that C/C++ $break and $continue statements
 *        work correctly from within the block.
 * Example:
 *   class A:public PTP::List::Entry {...};
 *   PTP::List list;
 *   ...
 *   A *x;
 *   list.Lock();
 *   $PTP_LIST_FOREACH(A, x, &list)
 *   {
 *       ...
 *   }
 *   list.Unlock();
 */
#define PTP_LIST_FOREACH(type, iterator, list) \
	PTP::List::Entry *__tmp_##iterator; \
	for (__tmp_##iterator = (list)->GetHead(), iterator = 0; \
	     (list)->IsValid(__tmp_##iterator) \
	     && (iterator = (type*) __tmp_##iterator) \
	     && (__tmp_##iterator = __tmp_##iterator->GetNext()); \
	     iterator = 0)

#endif // __PTP_LIST_H__
