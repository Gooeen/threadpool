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
#include <list> // std::list

namespace gutility
{
	// Thread pool.
	class threadpool;

	// Divide a sequence into multiple segments.
	// Parameter: first Forward iterators to the initial position
	//            in a sequence of the elements.
	// Parameter: last Forward iterators to the final position
	//            in a sequence of the elements. The element pointed
	//            by first, but not the element pointed by last.
	//            This must be reachable from first.
	// Parameter: n Number of segments.
	// Return value: Multiple segments include iterators to the
	//               initial and final positions.
	template <typename ForwardIterator>
	std::vector<std::pair<ForwardIterator, ForwardIterator>> divide(ForwardIterator first, ForwardIterator last, std::size_t n);

	// Divide a sequence into multiple segments.
	// Parameter: first Forward iterators to the initial position
	//            in a sequence of at least n elements.
	// Parameter: size Numbers of elements.
	// Parameter: n Number of segments.
	// Return value: Multiple segments include iterators to the
	//               initial and final positions.
	template <typename ForwardIterator>
	std::vector<std::pair<ForwardIterator, ForwardIterator>> divide_n(ForwardIterator first, std::size_t size, std::size_t n);
}

// Divide a sequence into multiple segments.
// Parameter: first Forward iterators to the initial position
//            in a sequence of the elements.
// Parameter: last Forward iterators to the final position
//            in a sequence of the elements. The element pointed
//            by first, but not the element pointed by last.
//            This must be reachable from first.
// Parameter: n Number of segments.
// Return value: Multiple segments include iterators to the
//               initial and final positions.
template<typename ForwardIterator>
std::vector<std::pair<ForwardIterator, ForwardIterator>> gutility::divide(ForwardIterator first, ForwardIterator last, std::size_t n)
{
	return divide_n(first, std::distance(first, last), n);
}

// Divide a sequence into multiple segments.
// Parameter: first Forward iterators to the initial position
//            in a sequence of at least n elements.
// Parameter: size Numbers of elements.
// Parameter: n Number of segments.
// Return value: Multiple segments include iterators to the
//               initial and final positions.
template<typename ForwardIterator>
std::vector<std::pair<ForwardIterator, ForwardIterator>> gutility::divide_n(ForwardIterator first, std::size_t size, std::size_t n)
{
	auto average = size / n;
	auto remainder = size % n;

	std::vector<std::pair<ForwardIterator, ForwardIterator>> pointers;
	pointers.reserve(n);
	
	for (std::size_t i = 0; i < remainder; ++i)
	{
		ForwardIterator last = first;
		std::advance(last, average + 1);

		pointers.emplace_back(first, last);
		first = last;
	}

	if (average == 0)
	{
		return pointers;
	}

	for (std::size_t i = remainder; i < n; ++i)
	{
		ForwardIterator last = first;
		std::advance(last, average);

		pointers.emplace_back(first, last);
		first = last;
	}

	return pointers;
}

// Thread pool.
class gutility::threadpool
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
	size_type count_of_threads(void) const noexcept;

	// Return the number of tasks in the thread pool.
	// Return value: The number of tasks in the thread pool.
	size_type count_of_tasks(void) noexcept;

	// Divide a sequence into multiple average segments and enqueue them.
	// Return after all tasks finishing.
	// Parameter: first Forward iterators to the initial position
	//            in a sequence of the elements.
	// Parameter: last Forward iterators to the final position
	//            in a sequence of the elements. The element pointed
	//            by first, but not the element pointed by last.
	//            This must be reachable from first.
	// Parameter: func The task function that accepts two iterators to the
	//            initial and final positions as arguments.
	//            The function shall not modify any of its arguments.
	//            This can either be a function pointer or a function object.
	template <typename InputIterator, typename TaskFunc>
	void parallel(InputIterator first, InputIterator last, TaskFunc &&func);

	// Divide a sequence into multiple segments and enqueue them.
	// Return after all tasks finishing.
	// Parameter: first Forward iterators to the initial position
	//            in a sequence of the elements.
	// Parameter: last Forward iterators to the final position
	//            in a sequence of the elements. The element pointed
	//            by first, but not the element pointed by last.
	//            This must be reachable from first.
	// Parameter: func The task function that accepts two iterators to the
	//            initial and final positions as arguments.
	//            The function shall not modify any of its arguments.
	//            This can either be a function pointer or a function object.
	// Parameter: func The task function that accepts two iterators and an
	//            unsigned integral. Two iterators are to the initial and
	//            final positions and an unsigned integral indicates the
	//            number of segments. It returns a container of a std::pair
	//            of two iterators to the initial and final positions. The
	//            container must have a begin() function, an end() function.
	//            The function shall not modify any of its arguments.
	//            This can either be a function pointer or a function object.
	template <typename InputIterator, typename TaskFunc, typename SplitFunc>
	void parallel(InputIterator first, InputIterator last, TaskFunc &&func, SplitFunc &&split);

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
inline gutility::threadpool::threadpool(size_type size)
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
inline gutility::threadpool::~threadpool(void)
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
inline void gutility::threadpool::stop_committing(void)
{
	m_stop_committing = true;
}

inline void gutility::threadpool::restart_committing(void)
{
	m_stop_committing = false;
}

// Clear all rest tasks.
inline void gutility::threadpool::clear_waiting_tasks(void)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	while (!m_tasks.empty())
	{
		m_tasks.pop();
	}
}

// Return the number of threads in the thread pool.
// Return value: The number of threads in the thread pool.
inline gutility::threadpool::size_type gutility::threadpool::count_of_threads(void) const noexcept
{
	return m_threads.size();
}

// Return the number of tasks in the thread pool.
// Return value: The number of tasks in the thread pool.
inline gutility::threadpool::size_type gutility::threadpool::count_of_tasks(void) noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_tasks.size();
}

// Push a task into a queue.
// Parameter: func A task to be executed.
// Parameter: args Parameters of func.
// Return value: A future.
template<typename Func, typename ...Args>
inline auto gutility::threadpool::enqueue(Func && func, Args && ...args) -> std::future<decltype(func(args...))>
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

// Divide a sequence into multiple average segments and enqueue them.
// Return after all tasks finishing.
// Parameter: first Forward iterators to the initial position
//            in a sequence of the elements.
// Parameter: last Forward iterators to the final position
//            in a sequence of the elements. The element pointed
//            by first, but not the element pointed by last.
//            This must be reachable from first.
// Parameter: func The task function that accepts two iterators to the
//            initial and final positions as arguments.
//            The function shall not modify any of its arguments.
//            This can either be a function pointer or a function object.
template<typename InputIterator, typename TaskFunc>
inline void gutility::threadpool::parallel(InputIterator first, InputIterator last, TaskFunc &&func)
{
	this->parallel(first, last, std::forward<TaskFunc>(func), divide<InputIterator>);
}

// Divide a sequence into multiple segments and enqueue them.
// Return after all tasks finishing.
// Parameter: first Forward iterators to the initial position
//            in a sequence of the elements.
// Parameter: last Forward iterators to the final position
//            in a sequence of the elements. The element pointed
//            by first, but not the element pointed by last.
//            This must be reachable from first.
// Parameter: func The task function that accepts two iterators to the
//            initial and final positions as arguments.
//            The function shall not modify any of its arguments.
//            This can either be a function pointer or a function object.
// Parameter: func The task function that accepts two iterators and an
//            unsigned integral. Two iterators are to the initial and
//            final positions and an unsigned integral indicates the
//            number of segments. It returns a container of a std::pair
//            of two iterators to the initial and final positions. The
//            container must have a begin() function, an end() function.
//            The function shall not modify any of its arguments.
//            This can either be a function pointer or a function object.
template<typename InputIterator, typename TaskFunc, typename SplitFunc>
inline void gutility::threadpool::parallel(InputIterator first, InputIterator last, TaskFunc &&func, SplitFunc &&split)
{
	auto splitfunction = split(first, last, this->count_of_threads());

	std::list<std::future<void>> results;
	for (const auto &group : splitfunction)
	{
		results.push_back(this->enqueue(func, group.first, group.second));
	}

	for (auto &result : results)
	{
		result.get();
	}
}

// Get a task which to be executed.
// The function is waiting while no task.
// Return value: A task.
inline gutility::threadpool::task_type gutility::threadpool::get_one_task(void)
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
inline void gutility::threadpool::schedule(void)
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
