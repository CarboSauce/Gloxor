#include "pmm.hpp"
#include "arch/paging.hpp"
#include "arch/segments.hpp"
#include "system/logging.hpp"
#include <bit>
#include <glox/assert.hpp>
#include <glox/bitfields.hpp>
#include <glox/util.hpp>
#include <gloxor/modules.hpp>

using namespace glox;

template <typename T>
struct node
{
	T data;
	node<T>* prev;
	node<T>* next;
	struct iterator
	{
		node<T>* it;
		auto operator++() { return it = it->next; }
		auto operator--() { return it = it->prev; }
		friend auto operator<=>(iterator, iterator) = default;
		auto& operator*() const { return *it; }
		auto& operator->() const { return it; }
	};
	iterator begin()
	{
		return {this};
	}

	iterator end()
	{
		return {nullptr};
	}

	friend auto operator<=>(const node<T>& l, const node<T>& r)
	{
		return l.data <=> r.data;
	}
	void remove()
	{
		// this is nullptr safe
		if (prev != nullptr)
			prev->next = this->next;
		if (next != nullptr)
			next->prev = this->prev;
	}
	void insert(node<T>& left, node<T>& right)
	{
		left.next = this;
		this->next = &right;
		this->prev = &left;
		right.prev = this;
	}
};

template <typename T>
struct list
{
	node<T>* front;
	node<T>* back;

	struct iterator
	{
		node<T>* it;
		auto operator++() { return it = it->next; }
		auto operator--() { return it = it->prev; }
		friend auto operator<=>(iterator, iterator) = default;
		auto& operator*() const { return *it; }
		auto& operator->() const { return it; }
	};
	iterator begin()
	{
		return {front};
	}
	iterator end()
	{
		return {nullptr};
	}

	void insert(node<T>& p)
	{
		gloxDebugLogln("back <= &p ", back, ' ', &p);
		if (*back <= p)
		{
			back->next = &p;
			p.prev = back;
			back = &p;
			return;
		}
		else if (*front >= p)
		{
			p.next = front;
			p.prev = nullptr;
			front->prev = &p;
			front = &p;
			return;
		}
		for (auto it = front->next; it != back; it = it->next)
		{
			gloxDebugLogln("&it <= &p ", &it, ' ', &p);
			if (*it >= p)
			{
				p.next = it;
				p.prev = it->prev;
				it->prev->next = &p;
				it->prev = &p;
				return;
			}
		}
	}
};

struct pmmChunk
{
	size_t size;
	friend auto operator<=>(const pmmChunk& l, const pmmChunk& r)
	{
		return &l <=> &r;
	};
};
using pmmHeader = node<pmmChunk>;
using pmmList = list<pmmChunk>;

static list<pmmChunk> pmmCtx;

inline void* allocFromChunk(list<pmmChunk>& chunk)
{
	auto& i = *chunk.front;
	i.data.size -= glox::pmmChunkSize;
	if (i.data.size == 0)
	{
		i = *i.next;
		if (i.next != nullptr)
			i.next->prev = nullptr;
		return &i;
	}
	return reinterpret_cast<void*>(reinterpret_cast<uintptr>(&i) + i.data.size);
}

inline void* allocFromChunk(list<pmmChunk>& chunk, sizeT pageCount)
{
	const auto allocSize = glox::pmmChunkSize * pageCount;
	for (auto& it : chunk)
	{
		if (it.data.size < allocSize)
			continue;
		it.data.size -= allocSize;
		if (it.data.size == 0)
		{
			if (it.next == nullptr)
			{
				if (it.prev == nullptr)
					return &it;
				it.prev->next = nullptr;
			}
			else if (it.prev == nullptr)
			{
				it.next->prev = nullptr;
			}
			else
			{
				it.next->prev = it.prev;
				it.prev->next = it.next;
			}
			return &it;
		}
		return reinterpret_cast<void*>(reinterpret_cast<uintptr>(&it) + it.data.size);
	}

	return nullptr;
}

inline void appendChunk(pmmHeader*& back, void* base, sizeT length)
{
	auto* chunk = (pmmHeader*)base;
	if ((uintptr)back + back->data.size == (uintptr)base)
	{
		back->data.size += length;
	}
	else
	{
		back->next = chunk;
		chunk->prev = back;
		back = chunk;
	}
}

// assumes base is in the node list range
inline void insertChunk(pmmHeader*& from, void* base, sizeT length)
{
	auto* chunk = (pmmHeader*)base;
	auto* it = from;
	for (; it < chunk; it = it->next)
	{
		if ((uintptr)it + it->data.size == (uintptr)base)
		{
			it->data.size += length;
			return;
		}
	}
	gloxAssert(it->next != nullptr);
	chunk->insert(*it,*it->next);
}

namespace glox
{

	// This should point to context initialized by early protocol inits
	// glox::pmmHeader* pmmCtx;
	void pmmAddChunk(void* base, size_t length)
	{
		gloxAssert(length % glox::pmmChunkSize == 0, "Pmm chunk length must be multiple of pmmChunkSize");
		auto* chunk = (pmmHeader*)base;
		auto& pmmStart = pmmCtx.front;
		auto& pmmEnd = pmmCtx.back;
		chunk->next = nullptr;
		chunk->data.size = length;

		if (pmmStart == nullptr)
		{
			pmmStart = chunk;
			pmmStart->prev = nullptr;
			pmmEnd = chunk;
		}
		else if (pmmEnd < chunk)
		{
			appendChunk(pmmEnd,base,length);
		}	
		else insertChunk(pmmStart,base,length);
	}

	void* pmmAlloc()
	{
		auto mem = allocFromChunk(pmmCtx);
		gloxDebugLogln("pmmAlloc(): allocated address: ", mem);
		return mem;
	}

	void* pmmAlloc(sizeT pageCount)
	{
		auto mem = allocFromChunk(pmmCtx, pageCount);
		gloxDebugLogln("pmmAlloc(", pageCount, "): allocated address: ", mem);
		return mem;
	}

	void* pmmAllocZ()
	{
		auto addr = pmmAlloc();
		memset(addr, 0, glox::pmmChunkSize);
		return addr;
	}
	void* pmmAllocZ(sizeT pageCount)
	{
		auto addr = pmmAlloc(pageCount);
		memset(addr, 0, pageCount * glox::pmmChunkSize);
		return addr;
	}

	void pmmFree(void* ptr)
	{
		glox::pmmAddChunk(ptr, glox::pmmChunkSize);
	}
	void pmmFree(void* ptr, sizeT pageCount)
	{
		glox::pmmAddChunk(ptr, glox::pmmChunkSize * pageCount);
	}
} // namespace glox

// static void test()
// {
// 	gloxDebugLog("pmm test\nBefore:\n");
// 	for (const auto& it : pmmCtx)
// 	{
// 		gloxDebugLogln(&it, '-', (char*)&it + it.data.size);
// 	}

// 	auto ptr = pmmAlloc(4);
// 	pmmFree(ptr, 4);
// 	ptr = pmmAlloc();
// 	pmmFree(ptr);
// 	gloxDebugLog("\nAfter:\n");
// 	for (const auto& it : pmmCtx)
// 	{
// 		gloxDebugLogln(&it, '-', (char*)&it + it.data.size);
// 	}
// }

// registerTest(test);