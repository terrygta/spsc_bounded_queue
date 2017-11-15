#ifndef SPSCBOUNDEDQUEUE_H
#define SPSCBOUNDEDQUEUE_H

#include <atomic>
#include <vector>

//============================================================================
// SPSCBoundedQueue
//============================================================================
/** @brief A Single Producer Single Consumer bounded queue
*
* This is one of the simplest lockfree data structures that exist.
* It is basically a circular buffer that supports only a single
* producer and a single consumer. If you have more then this will
* exhibit undefined behaviour.
*
* It is quite a fast queue as it only uses acquire/release loads
* and stores to achieve the synchronization.
*
* This is better than a lockfree container, it is a waitfree container. So
* calls to Enqueue() Dequeue() are guaranteed to be bounded in a finite
* period of time for all threads.
*/
template <typename DataT>
class SPSCBoundedQueue
{
public:
	//---------------------------------------------------------------------
	/** @brief Create a bounded queue that stores upt max_size items
	*/
	SPSCBoundedQueue(size_t max_size)
		: write_index(0)
		, read_index(0)

		// Note: Buffer size is max + 1 as when it is empty the cur_read_index/cur_write_index are the same
		// but when full cur_write_index=cur_read_index-1 so we need one more element to fix the max size in
		, buffer(max_size + 1)
	{
	}

	//---------------------------------------------------------------------
	/** @brief Add data to the queue.
	*
	* @return Returns false if the queue is currently full and
	* we were unable to add the data to the queue.
	*/
	bool Enqueue(const DataT& value_in)
	{
		// Only written from this enqueue thread so relaxed is fine
		size_t cur_write_index = write_index.load(std::memory_order_relaxed);
		size_t next = (cur_write_index + 1) % buffer.size();

		// Check to see if the ring-buffer is full
		if (next == read_index.load(std::memory_order_acquire))
			return false;

		// Write the data to the buffer
		buffer[cur_write_index] = value_in;

		// Update the write index
		write_index.store(next, std::memory_order_release);
		return true;
	}

	//---------------------------------------------------------------------
	/** @brief Removes data from the queue.
	*
	* @return Returns false if the queue is currently empty and
	* we were unable to remove the data from the queue.
	*/
	bool Dequeue(DataT& value_out)
	{
		size_t cur_write_index = write_index.load(std::memory_order_acquire);

		// only written from dequeue thread
		size_t cur_read_index = read_index.load(std::memory_order_relaxed);

		// Check to see if the buffer is empty
		if (cur_write_index == cur_read_index)
			return false;

		// Read the value from the buffer
		value_out = buffer[cur_read_index];

		size_t next = (cur_read_index + 1) % buffer.size();
		read_index.store(next, std::memory_order_release);
		return true;
	}

	//---------------------------------------------------------------------
	/** @brief Clears the buffer to empty it.
	*
	* WARNING NOT THREAD SAFE.
	*/
	void Clear()
	{
		write_index.store(0, std::memory_order_relaxed);
		read_index.store(0, std::memory_order_relaxed);
	}

	//---------------------------------------------------------------------
	/** @brief Returns true if the buffer is empty
	*
	* WARNING NOT THREAD SAFE. Dont use this while other threads are
	* Enqueue()ing Dequeue()ing data from the buffer.
	*
	* It is safe to call this from the same thread as the Dequeue()
	* calls. But not from any other threads. So it is *partially* thread
	* safe.
	*/
	bool Empty() const
	{
		return write_index.load(std::memory_order_acquire) == read_index.load(std::memory_order_relaxed);
	}

	//---------------------------------------------------------------------
	/** @brief Returns true if the buffer is full
	*
	* WARNING NOT THREAD SAFE. Dont use this while other threads are
	* Enqueue()ing Dequeue()ing data from the buffer.
	*
	* It is safe to call this from the same thread as the Enqueue()
	* calls. But not from any other threads. So it is *partially* thread
	* safe.
	*/
	bool Full() const
	{
		// Only written from this enqueue thread so relaxed is fine
		size_t cur_write_index = write_index.load(std::memory_order_relaxed);
		size_t next = (cur_write_index + 1) % buffer.size();

		// Check to see if the ring-buffer is full
		if (next == read_index.load(std::memory_order_acquire))
			return true;
		else
			return false;
	}

	//---------------------------------------------------------------------
	/** @brief Returns approximate number of elements in buffer
	*
	* WARNING NOT THREAD SAFE. Don't use this for logic, the size is
	* not guaranteed to be correct if either end is using the queue.
	*/
	size_t Size() const
	{
		size_t cur_write_index = write_index.load(std::memory_order_relaxed);
		size_t cur_read_index = read_index.load(std::memory_order_relaxed);

		if (cur_write_index > cur_read_index)
			return cur_write_index - cur_read_index;
		else if (cur_read_index > cur_write_index)
			return (buffer.size() - cur_read_index) + cur_write_index;

		return 0;
	}

	//---------------------------------------------------------------------
	/** @brief returns the max size of the queue
	*/
	size_t GetMaxSize() const
	{
		return buffer.size() - 1;
	}

	//---------------------------------------------------------------------

protected:

	static const int PADDING_SIZE = 64 - sizeof(size_t);
	std::atomic<size_t> write_index;
	char padding[PADDING_SIZE]; // force read_index and write_index to different cache lines
	std::atomic<size_t> read_index;
	std::vector<DataT> buffer;

private:
	SPSCBoundedQueue(const SPSCBoundedQueue& rhs) = delete;
	SPSCBoundedQueue& operator=(const SPSCBoundedQueue& rhs) = delete;
};

#endif