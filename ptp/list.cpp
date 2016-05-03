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

#include <assert.h>
#include <ptp/list.h>
#include <ptp/debug.h>

/**
 * PTP::List::Entry::Entry: Class constructor.
 */
PTP::List::Entry::Entry()
{
	m_next = this;
	m_prev = this;
}

#ifdef PTPTL_DLL

/*
 * PTP::List::Entry::Entry: Copy constructor.
 * @entry: Source Entry.
 */
PTP::List::Entry::Entry(const Entry& entry)
{
	assert(0);
}

/*
 * PTP::List::Entry::operator=: Copy constructor.
 * @entry: Source Entry.
 */
PTP::List::Entry&
PTP::List::Entry::operator=(const Entry& entry)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/**
 * PTP::List::Entry::GetNext
 * Returns: Next list element.
 */
PTP::List::Entry *
PTP::List::Entry::GetNext()
{
	return m_next;
}

/**
 * PTP::List::Entry::GetPrev
 * Returns: Previous list element.
 */
PTP::List::Entry *
PTP::List::Entry::GetPrev()
{
	return m_prev;
}

/**
 * PTP::List::List: Class constructor.
 */
PTP::List::List()
	:m_locking(1)
{
}

/**
 * PTP::List::List: Class constructor.
 * @locking: 1 to use locking (default) or 0 for no locking.
 */
PTP::List::List(int locking)
	:m_locking(locking)
{
}

#ifdef PTPTL_DLL

/*
 * PTP::List::List: Copy constructor.
 * @list: Source List.
 */
PTP::List::List(const List& list)
{
	assert(0);
}

/*
 * PTP::List::operator=: Copy constructor.
 * @list: Source List.
 */
PTP::List&
PTP::List::operator=(const List& list)
{
	assert(0);
	return *this;
}

#endif // PTPTL_DLL

/**
 * PTP::List::~List: Class destructor.
 */
PTP::List::~List()
{
}

/**
 * PTP::List::Lock: Acquire the list lock.
 */
void
PTP::List::Lock()
{
	if (m_locking)
		m_mutex.Lock();
}

/**
 * PTP::List::Unlock: Release the list lock.
 */
void
PTP::List::Unlock()
{
	if (m_locking)
		m_mutex.Unlock();
}

/**
 * PTP::List::IsValid
 * @entry: List entry.
 * Returns: 1 if @entry is a valid list element or else 0.
 */
int
PTP::List::IsValid(Entry *entry) const
{
	return (entry && entry != &m_head);
}

/**
 * PTP::List::GetHead
 * Returns: Head of list or NULL if empty.
 */
PTP::List::Entry *
PTP::List::GetHead()
{
	return IsValid(m_head.m_next) ? m_head.m_next:0;
}

/**
 * PTP::List::GetTail
 * Returns: Tail of list or NULL if empty.
 */
PTP::List::Entry *
PTP::List::GetTail()
{
	return IsValid(m_head.m_prev) ? m_head.m_prev:0;
}

/**
 * PTP::List::Insert: Insert entry at the head of the list.
 * @entry: New list entry.
 * @lock: 1 to acquire lock (default) or 0 for no locking.
 * Example:
 *   class A:public PTP::List::Entry {...};
 *   A x;
 *   PTP::List list;
 *   list.$Insert(&x);
 */
void
PTP::List::Insert(Entry *entry, int lock)
{
        if (lock)
                Lock();
	Entry *next = m_head.m_next;
        entry->m_next = next;
        entry->m_prev = &m_head;
        next->m_prev = entry;
        m_head.m_next = entry;
        if (lock)
                Unlock();
}

/**
 * PTP::List::Append: Append entry to the tail of the list.
 * @entry: New list entry.
 * @lock: 1 to acquire lock (default) or 0 for no locking.
 * Example:
 *   class A:public PTP::List::Entry {...};
 *   A x;
 *   PTP::List list;
 *   list.$Append(&x);
 */
void
PTP::List::Append(Entry *entry, int lock)
{
        if (lock)
                Lock();
        Entry *prev = m_head.m_prev;
        entry->m_next = &m_head;
        entry->m_prev = prev;
        prev->m_next = entry;
        m_head.m_prev = entry;
        if (lock)
                Unlock();
}

/**
 * PTP::List::Remove: Remove entry from the list.
 * @entry: Existing list entry.
 * @lock: 1 to acquire lock (default) or 0 for no locking.
 * Example:
 *   class A:public PTP::List::Entry {...};
 *   A x;
 *   PTP::List list;
 *   list.Insert(&x);
 *   ...
 *   list.$Remove(&x);
 */
void
PTP::List::Remove(Entry *entry, int lock)
{
        if (lock)
                Lock();
        Entry *next = entry->m_next;
        Entry *prev = entry->m_prev;
        next->m_prev = prev;
        prev->m_next = next;
        entry->m_next = entry;
        entry->m_prev = entry;
        if (lock)
                Unlock();
}
