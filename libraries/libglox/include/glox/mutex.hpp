#pragma once
namespace glox
{
template <typename T>
class lockGuard
{
	T& mutex;

 public:
	lockGuard(T& mutex) : mutex(mutex)
	{
		mutex.lock();
	}
	~lockGuard()
	{
		mutex.unlock();
	}
	lockGuard(const lockGuard&) = delete;
	lockGuard& operator=(const lockGuard&) = delete;
};
template <typename T>
class scopedLock
{
	T mutex;

 public:
	scopedLock()
	{
		mutex.lock();
	}
	~scopedLock()
	{
		mutex.unlock();
	}
	scopedLock(const scopedLock&) = delete;
	scopedLock& operator=(const scopedLock&) = delete;
};
} // namespace glox