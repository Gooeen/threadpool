#ifndef __ASSET_UTILITY_THREAD_POOL_HPP__
#define __ASSET_UTILITY_THREAD_POOL_HPP__

#include <cstddef> // std::size_t
#include <thread> // std::thread
#include <condition_variable> // std::condition_variable
#include <mutex> // std::mutex std::unique_lock
#include <atomic> // std::atomic
#include <future> // std::future, std::packaged_task
#include <functional> // std::function
#include <vector> // std::vector
#include <queue> // std::queue
#include <exception> // std::exception

namespace asset
{
	// Thread pool
	class threadpool;
}

// Thread pool
class asset::threadpool
{
private:

	// A function object without parameters and Return value.
	using task_type = std::function<void(void)>;

	// An unsigned integral type.
	using size_type = std::size_t;

public:

	// Constructor.
	// Parameter: size Size of threads.
	explicit threadpool(size_type size);

	// Destructor.
	~threadpool(void);

	// Delete copyable.
	threadpool(const threadpool &) = delete;

	// Delete copyable.
	void operator=(const threadpool &) = delete;

	// Stop committing.
	// After stopping committing, a exception will
	// be throw while enqueuing a task.
	void stop_committing(void);

	// Restart committing.
	void restart_committing(void);

	// Clear all waiting tasks.
	void clear_waiting_tasks(void);

	// Push a task into a queue.
	// Parameter: func A task to be executed.
	// Parameter: args Parameters of func.
	// Return value: A future.
	template <typename Func, typename ...Args>
	auto enqueue(Func &&func, Args && ...args)->std::future<decltype(func(args...))>;

	// Return the number of threads in the thread pool.
	// Return value: The number of threads in the thread pool.
	size_type size_of_threads(void) const noexcept;

	// Return the number of tasks in the thread pool.
	// Return value: The number of tasks in the thread pool.
	size_type size_of_tasks(void) noexcept;

private:

	// Get a task which to be executed.
	// The function is waiting while no task.
	// Return value: A task.
	task_type get_one_task(void);

	// Schedule a task.
	void schedule(void);

private:

	// Threads.
	std::vector<std::thread> m_threads;

	// Tasks queue.
	std::queue<task_type> m_tasks;

	// Mutex, to lock tasks queue.
	std::mutex m_mutex;

	// Condition_variable, run if tasks are not empty.
	std::condition_variable m_cv;

	// True indicates to ban committing, false otherwise.
	std::atomic<bool> m_stop_committing;

	// True indicates that the thread pool will be killed
	// after finishing all tasks, false otherwise.
	std::atomic<bool> m_close;
};

// Constructor.
// Parameter: size Size of threads.
inline asset::threadpool::threadpool(size_type size)
	: m_stop_committing(false)
	, m_close(false)
{
	// reserve space
	m_threads.reserve(size);

	// create threads
	for (size_type i = 0; i < size; ++i)
	{
		m_threads.emplace_back(&threadpool::schedule, this);
	}
}

// Destructor.
// The thread pool destroyed after all tasks finishing.
inline asset::threadpool::~threadpool(void)
{
	// stop all scheduling
	m_close = true;

	// notify all threads
	m_cv.notify_all();

	// waiting for the end of all tasks
	for (auto &thread : m_threads)
	{
		thread.join();
	}
}

// Stop committing.
inline void asset::threadpool::stop_committing(void)
{
	m_stop_committing = true;
}

inline void asset::threadpool::restart_committing(void)
{
	m_stop_committing = false;
}

// Clear all rest tasks.
inline void asset::threadpool::clear_waiting_tasks(void)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	while (!m_tasks.empty())
	{
		m_tasks.pop();
	}
}

// Return the number of threads in the thread pool.
// Return value: The number of threads in the thread pool.
inline asset::threadpool::size_type asset::threadpool::size_of_threads(void) const noexcept
{
	return m_threads.size();
}

// Return the number of tasks in the thread pool.
// Return value: The number of tasks in the thread pool.
inline asset::threadpool::size_type asset::threadpool::size_of_tasks(void) noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_tasks.size();
}

// Push a task into a queue.
// Parameter: func A task to be executed.
// Parameter: args Parameters of func.
// Return value: A future.
template<typename Func, typename ...Args>
inline auto asset::threadpool::enqueue(Func && func, Args && ...args) -> std::future<decltype(func(args...))>
{
	// if banning committing, throw a exception
	if (m_stop_committing)
	{
		throw std::runtime_error("The thread pool has closed commit.");
	}

	// the returned type of callback function
	using result_type = decltype(func(args...));

	// pack task
	auto task = std::make_shared<std::packaged_task<result_type(void)>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

	// push a task into a queue
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_tasks.emplace([task](void)
		{
			(*task)();
		});
	}

	// notify all threads
	m_cv.notify_all();

	// return a future of task
	return task->get_future();
}

// Get a task which to be executed.
// The function is waiting while no task.
// Return value: A task.
inline asset::threadpool::task_type asset::threadpool::get_one_task(void)
{
	// lock
	std::unique_lock<std::mutex> lock(m_mutex);

	// wait until tasks are not empty
	while (m_tasks.empty())
	{
		// if the thread pool needs to be closed and
		// there are no tasks, throw a exception
		if (m_close)
		{
			throw std::exception();
		}

		m_cv.wait(lock);
	}

	// get a task
	auto task = std::move(m_tasks.front());
	m_tasks.pop();

	return task;
}

// Schedule a task.
inline void asset::threadpool::schedule(void)
{
	// loop
	for (;;)
	{
		// a task
		task_type task;

		// wait until get a task
		// during waiting a task(tasks queue is empty),
		// if the thread pool needs to be closed, catch the
		// exception and exit function
		try
		{
			task = get_one_task();
		}
		catch (...)
		{
			return;
		}

		// execute the task
		task();
	}
}

#endif // !__ASSET_UTILITY_THREAD_POOL_HPP__
