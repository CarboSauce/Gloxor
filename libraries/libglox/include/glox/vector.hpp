#pragma once
#include <iterator>
#include "glox/alloc.hpp"
#include "glox/metaprog.hpp"
#include "glox/assert.hpp"
namespace glox
{
// TODO: Factory based constructors
// More member funcs
template<typename T, typename Allocator = glox::default_allocator>
class vector : public Allocator
{
	T* start = nullptr;
	size_t cap=-1,siz=0;
	public:
	using allocator = Allocator;
	vector() = default;
	vector(size_t reserve)
	{
		start = (T*)this->allocate(sizeof(T)*reserve);
		if (!start)
			return;
		cap = -1;
		siz = reserve;
	}
	vector(const T& val, size_t size)
	{
		start = (T*)this->allocate(sizeof(T)*size);
		if (!start)
			return;
		cap = size-1;
		siz = size;
		for (size_t i = 0; i < size; ++i)
		{
			::new (start+i) T(val);
		}
	}
	auto begin() { return start; }
	auto end() { return start+siz;}
	auto& back() { return start[cap];}
	const auto& back() const { return start[cap];}
	auto& front() const { return start[0];}
	const auto& front() const { return start[0];}
	const auto begin() const { return start; }
	const auto end() const {return start+siz;}
	auto size() const { return siz; }
	auto capacity() const { return cap+1; }

	template<typename... Args>
	bool emplace_back(Args&&... args)
	{
		if (!ensure_can_fit(cap+1)) return false;
		cap+=1;
		new (start+cap) T{FORWARD(args)...};
	}
	void pop_back()
	{
		start[cap].~T();
		cap-=1;
	}
	const T& operator[](size_t i) const 
	{
		gloxAssert(i <= cap);
		return *(start+i);
	}
	T& operator[](size_t i)
	{
		return const_cast<T&>(static_cast<const vector>(*this)[i]); 
	}
	private:
	/*
	 * @brief Expands vector to fit new_size bytes
	 * @param new_size new allocation size
	 */
	bool ensure_can_fit(size_t new_size)
	{
		if (new_size > siz)
		{
			auto old_siz = siz;
			new_size = siz + siz/2; // siz = siz * 1.5;
			auto tmp_ptr = (T*)this->reallocate(start,old_siz,new_size);
			if (!tmp_ptr) return false;
			start = tmp_ptr;
			siz = new_size;
		}
	}
};
}
