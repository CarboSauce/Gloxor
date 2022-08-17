#pragma once
#include <iterator>
#include "glox/alloc.hpp"
#include "glox/metaprog.hpp"
#include "glox/assert.hpp"
#include "glox/util.hpp"

namespace glox
{
// TODO: Factory based constructors
// More member funcs
template<typename T, typename Allocator = glox::default_allocator>
class vector : private Allocator
{
	T* start = nullptr;
	size_t cap=0,siz=0;
	public:
	using allocator = Allocator;
	vector() = default;
	vector(size_t reserve)
	{
		start = (T*)this->allocate(sizeof(T)*reserve);
		if (!start)
			return;
		cap = reserve;
		siz = 0;
	}
	vector(const T& val, size_t size)
	{
		start = (T*)this->allocate(sizeof(T)*size);
		if (!start)
			return;
		cap = size;
		siz = size;
		for (size_t i = 0; i < size; ++i)
		{
			::new (start+i) T(val);
		}
	}
	vector(const vector& other) : Allocator(static_cast<Allocator&>(other))
	{
		start = (T*)this->allocate(sizeof(T)*other.cap);
		if (!start)
			return;
		cap = other.cap;
		siz = other.siz;
		for (size_t i = 0; i < siz; ++i)
		{
			::new (start+i) T(other.start[i]);
		}
	}
	vector& operator=(const vector& other)
	{
		start = (T*)this->reallocate(start, cap, sizeof(T)*other.cap);
		if (!start)
		{
			siz = cap = 0;
			return *this;
		}
		cap = other.cap;
		siz = other.siz;
		for (size_t i = 0; i < siz; ++i)
		{
			::new (start+i) T(other.start[i]);
		}
	}
	vector(vector&& other) : Allocator(RVALUE(static_cast<Allocator&>(other)))
	{
		start = other.start; other.start = nullptr;
		cap   = other.cap  ; other.cap   = 0;
		siz   = other.siz  ; other.siz   = 0;
	}
	vector& operator=(vector& other)
	{
		using glox::swap;
		swap(start,other.start);
		swap(static_cast<Allocator&>(*this),static_cast<Allocator&>(other));
		cap = other.cap; other.cap = 0;
		siz = other.siz; other.siz = 0;
	}
	auto begin() { return start; }
	auto end() { return start+siz;}
	auto& back() { return start[siz-1];}
	const auto& back() const { return start[siz-1];}
	auto& front() { return start[0];}
	const auto& front() const { return start[0];}
	const auto begin() const { return start; }
	const auto end() const {return start+siz;}
	auto size() const { return siz; }
	auto capacity() const { return cap; }

	template<typename... Args>
	bool emplace_back(Args&&... args)
	{
		if (!ensure_can_fit(siz+1)) return false;
		new (start + siz++) T{FORWARD(args)...};
		return true;
	}
	void pop_back()
	{
		start[--siz].~T();
	}
	const T& operator[](size_t i) const 
	{
		gloxAssert(i < size);
		return *(start+i);
	}
	T& operator[](size_t i)
	{
		return const_cast<T&>(static_cast<const vector>(*this)[i]); 
	}
	private:
	/*
	 * @brief Expands vector to fit new_size index 
	 * @param new_size new allocation size
	 */
	bool ensure_can_fit(size_t new_size)
	{
		if (new_size > cap)
		{
			auto old_siz = cap;
			new_size = cap < 4 ?4:cap + cap/2; // siz = siz * 1.5;
			auto tmp_ptr = (T*)this->reallocate(start,old_siz*sizeof(T),new_size*sizeof(T));
			if (!tmp_ptr) return false;
			start = tmp_ptr;
			cap = new_size;
		}
		return true;
	}
};
}
