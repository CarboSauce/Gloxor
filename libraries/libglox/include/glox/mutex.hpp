#pragma once
namespace glox
{
template <typename T>
class lock_guard
{
	T& mutex;

 public:
	lock_guard(T& mutex) : mutex(mutex)
	{
		mutex.lock();
	}
	~lock_guard()
	{
		mutex.unlock();
	}
	lock_guard(const lock_guard&) = delete;
	lock_guard& operator=(const lock_guard&) = delete;
};
template <typename T>
class scoped_lock
{
	T mutex;

 public:
	scoped_lock()
	{
		mutex.lock();
	}
	~scoped_lock()
	{
		mutex.unlock();
	}
	scoped_lock(const scoped_lock&) = delete;
	scoped_lock& operator=(const scoped_lock&) = delete;
};
} // namespace glox