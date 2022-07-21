#pragma once
namespace glox
{
	template<typename T>
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
		lockGuard<T>(const lockGuard<T>&) = delete;
		lockGuard<T>& operator=(const lockGuard<T>&) = delete;
	};
	template<typename T>
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
		scopedLock<T>(const scopedLock<T>&) = delete;
		scopedLock<T>& operator=(const scopedLock<T>&) = delete;
	};
}