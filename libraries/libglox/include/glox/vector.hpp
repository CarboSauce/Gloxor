#pragma once
#include "alloc.hpp"
#include "assert.hpp"
#include "detail/moveutils.hpp"
#include "iterator.hpp"
#include "metaprog.hpp"
#include "result.hpp"

namespace glox
{
// TODO: Factory based constructors
// More member funcs
template <typename T, typename Allocator = glox::default_allocator>
class vector
{
	T* start = nullptr;
	size_t cap = 0, siz = 0;
	[[no_unique_address]] Allocator alloc;

 public:
	using allocator = Allocator;
	vector() = default;
	vector(size_t reserve)
	{
		start = (T*)alloc.allocate(sizeof(T) * reserve);
		if (!start)
			cap = 0;
		else
			cap = reserve;
		siz = 0;
	}
	vector(const T& val, size_t size)
	{
		start = (T*)alloc.allocate(sizeof(T) * size);
		if (!start)
			return;
		cap = size;
		siz = size;
		for (size_t i = 0; i < size; ++i)
		{
			::new (start + i) T(val);
		}
	}
	vector(const vector& other) : alloc(other.alloc)
	{
		start = (T*)alloc.allocate(sizeof(T) * other.cap);
		if (!start)
			return;
		cap = other.cap;
		siz = other.siz;
		for (size_t i = 0; i < siz; ++i)
		{
			::new (start + i) T(other.start[i]);
		}
	}
	vector& operator=(const vector& other)
	{
		start = realloc_buffer(cap, other.cap);
		if (!start)
		{
			siz = cap = 0;
			return *this;
		}
		cap = other.cap;
		siz = other.siz;
		for (size_t i = 0; i < siz; ++i)
		{
			::new (start + i) T(other.start[i]);
		}
	}
	vector(vector&& other) : alloc(RVALUE(other.alloc))
	{
		start = other.start;
		other.start = nullptr;
		cap = other.cap;
		other.cap = 0;
		siz = other.siz;
		other.siz = 0;
	}
	vector& operator=(vector&& other)
	{
		using glox::swap;
		swap(start, other.start);
		swap(static_cast<Allocator&>(*this), static_cast<Allocator&>(other));
		cap = other.cap;
		other.cap = 0;
		siz = other.siz;
		other.siz = 0;
	}
	~vector()
	{
		for (size_t i = 0; i != siz; ++i)
		{
			start[i].~T();
		}
		alloc.deallocate(start, cap * sizeof(T));
	}
	static glox::result<glox::vector<T>, option_t> with_capacity(size_t cap)
	{
		glox::vector<T> tmp(cap);
		if (tmp.is_null())
			return option_t::none;
		else
			return tmp;
	}
	auto begin() { return start; }
	auto end() { return start + siz; }
	auto& back() { return start[siz - 1]; }
	const auto& back() const { return start[siz - 1]; }
	auto& front() { return start[0]; }
	const auto& front() const { return start[0]; }
	auto begin() const { return start; }
	auto end() const { return start + siz; }
	auto size() const { return siz; }
	auto capacity() const { return cap; }
	auto empty() const { return siz == 0; }
	auto is_null() const { return start == nullptr; }

	template <typename... Args>
	bool emplace_back(Args&&... args)
	{
		if (!ensure_can_fit(siz + 1))
			return false;
		new (start + siz++) T{FORWARD(args)...};
		return true;
	}
	bool reserve(size_t new_cap)
	{
		if (cap < new_cap)
			return realloc_buffer(cap, new_cap);
		return true;
	}
	void pop_back()
	{
		gloxAssert(siz > 0);
		start[--siz].~T();
	}
	const T& operator[](size_t i) const
	{
		gloxAssert(i < size);
		return *(start + i);
	}
	T& operator[](size_t i)
	{
		return const_cast<T&>(static_cast<const vector>(*this)[i]);
	}

 private:
	auto* realloc_buffer(size_t old, size_t news)
	{
		if constexpr (std::is_trivially_copyable<T>::value)
		{
			return (T*)alloc.reallocate(start, sizeof(T) * old, sizeof(T) * news);
		}
		else
		{
			T* newb = (T*)alloc.allocate(sizeof(T) * news);
			if (!newb)
			{
				alloc.deallocate(start, old * sizeof(T));
				return newb;
			}
			for (size_t i = 0; i < old; ++i)
			{
				::new (newb + i) T(RVALUE(start[i]));
			}
			alloc.deallocate(start, old * sizeof(T));
			return newb;
		}
	}
	/*
	 * @brief Expands vector to fit new_size index
	 * @param new_size new allocation size
	 */
	bool ensure_can_fit(size_t new_size)
	{
		if (new_size > cap)
		{
			new_size = cap < 4 ? 4 : cap + cap / 2; // siz = siz * 1.5;
			auto tmp_ptr = realloc_buffer(cap * sizeof(T), new_size * sizeof(T));
			// auto tmp_ptr = (T*)this->reallocate(start,old_siz*sizeof(T),new_size*sizeof(T));
			if (!tmp_ptr)
				return false;
			start = tmp_ptr;
			cap = new_size;
		}
		return true;
	}
};
} // namespace glox
