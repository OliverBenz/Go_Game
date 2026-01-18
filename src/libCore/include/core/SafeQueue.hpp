#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace go {

//! Thread safe queue with a blocking Pop function.
template <class Entry>
class SafeQueue {
public:
	SafeQueue();

	//! Push element onto the queue.
	void Push(const Entry& value);

	//! Thread blocks here until there is an element to receive.
	//! \note Throws an exception when the queue is empty and blocking for threads is disabled.
	Entry Pop();

	//! Returns true if the queue is empty; false otherwise.
	bool Empty() const;

	//! Stop blocking the threads trying to pop an element from the queue.
	void Release();

protected:
	std::deque<Entry> m_queue;           //!< Stores the entries.
	std::mutex m_mutex;                  //!< Manage access to the queue.
	std::condition_variable m_condition; //!< Notify that element can be popped.
	std::atomic<bool> m_blockThreads;    //!< Should the Pop function block the threads or not.
};


template <class Entry>
SafeQueue<Entry>::SafeQueue() : m_blockThreads(true){};

template <class Entry>
void SafeQueue<Entry>::Push(const Entry& value) {
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue.push_back(value);
	}
	m_condition.notify_one();
}

template <class Entry>
Entry SafeQueue<Entry>::Pop() {
	Entry element;
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_condition.wait(lock, [this] { return !(m_queue.empty() && m_blockThreads); });

		if (m_queue.empty()) {
			// TODO: Throw Less generic exception
			throw std::exception();
		}
		element = m_queue.front();
		m_queue.pop_front();
	}

	return element;
}

template <class Entry>
bool SafeQueue<Entry>::Empty() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_queue.empty();
}

template <class Entry>
void SafeQueue<Entry>::Release() {
	m_blockThreads.store(false);
	m_condition.notify_all();
}

} // namespace go
