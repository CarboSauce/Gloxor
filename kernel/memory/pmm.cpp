#include "pmm.hpp"
#include "arch/addrspace.hpp"
#include "system/logging.hpp"
#include <glox/assert.hpp>
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
		if (prev != nullptr)
			prev->next = this->next;
		if (next != nullptr)
			next->prev = this->prev;
	}
	void insert(node<T>* left, node<T>* right)
	{
		// gloxAssert(left != nullptr && right != nullptr);
		//  temporary fix
		if (left != nullptr)
			left->next = this;
		if (right != nullptr)
			right->prev = this;
		this->next = right;
		this->prev = left;
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

	/*
		this function isnt actually used anywhere it handles too many edge cases
		which in certain conditions can be guaranteed to never occur
		maybe compiler is good enough to ellide them, should be investigated
	*/
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

/*
	@TODO: size could perhaps be replaced pointer to end of range
*/
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
static u64 memorySize;
/*
	Both of those functions actually return nullptr on OOM situations
	Single chunk allocator is seperate for purpouses of optimization.
	I very much doubt compiler can eliminate loop when page count is 1.
	Loop doesnt need to be there because we can guarantee that if
	node is present in the list, it has atleast 1 free page
*/
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
			if (&it == chunk.front)
				chunk.front = it.next;
			if (&it == chunk.back)
				chunk.back = it.prev;
			return &it;
		}
		return reinterpret_cast<void*>(reinterpret_cast<uintptr>(&it) + it.data.size);
	}

	return nullptr;
}

// this functions seems like it could be eliminated and added to insert chunk
inline void appendChunk(pmmHeader*& back, pmmHeader* chunk, sizeT length)
{
	if ((uintptr)back + back->data.size == (uintptr)chunk)
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

inline void prependChunk(pmmHeader*& front, pmmHeader* chunk, sizeT length)
{
	if ((uintptr)chunk + length == (uintptr)front)
	{
		chunk->data.size = front->data.size + length;
		chunk->next = front->next;
		front = chunk;
	}
	else
	{
		chunk->next = front;
		chunk->prev = nullptr;
		front = chunk;
	}
}

// assumes chunk is in the node list range
inline void insertChunk(pmmHeader*& from, pmmHeader* chunk, sizeT length)
{
	auto* it = from;
	if (it > chunk)
	{
		if ((uintptr)chunk + length == (uintptr)it)
		{
			chunk->data.size += it->data.size;
			it->next->prev = chunk;
			chunk->next = it->next;
			from = chunk;
			return;
		}
		it->prev = chunk;
		chunk->next = it;
		chunk->prev = nullptr;
		return;
	}
	for (; it < chunk; it = it->next)
	{
		if ((uintptr)it + it->data.size == (uintptr)chunk)
		{
			it->data.size += length;
			auto nextChunk = it->next;
			if ((uintptr)it + it->data.size == (uintptr)nextChunk)
			{
				it->data.size += nextChunk->data.size;
				auto farAhead = nextChunk->next;
				farAhead->prev = it;
				it->next = farAhead;
			}
			return;
		}
	}
	if ((uintptr)chunk + length == (uintptr)it)
	{
		chunk->data.size += it->data.size;
		it->next->prev = chunk;
		chunk->next = it->next;
		chunk->prev = it->prev;
		it->prev->next = chunk;
		return;
	}
	chunk->insert(it->prev, it);
}

namespace glox
{

void pmmAddChunk(void* base, size_t length)
{
	gloxAssert(length % glox::pmmChunkSize == 0, "Pmm chunk length must be multiple of pmmChunkSize");
	const auto realBase = arch::toVirt((glox::vaddrT)base);
	auto* chunk = reinterpret_cast<pmmHeader*>(realBase);
	auto& pmmStart = pmmCtx.front;
	auto& pmmEnd = pmmCtx.back;
	chunk->next = nullptr;
	chunk->data.size = length;
	memorySize += length;
	if (pmmStart == nullptr)
	{
		pmmStart = chunk;
		pmmStart->prev = nullptr;
		pmmEnd = chunk;
	}
	else if (pmmEnd < chunk)
		appendChunk(pmmEnd, chunk, length);
	else if (pmmStart > chunk)
		prependChunk(pmmStart, chunk, length);
	else
		insertChunk(pmmStart, chunk, length);
}

void* pmmAlloc(sizeT pageCount)
{
	auto mem = allocFromChunk(pmmCtx, pageCount);
	//gloxDebugLogln("pmmAlloc(", pageCount, "): allocated address: ", mem);
	return mem;
}
void* pmmAllocZ(sizeT pageCount)
{
	auto addr = pmmAlloc(pageCount);
	memset(addr, 0, pageCount * glox::pmmChunkSize);
	return addr;
}

// void pmmFree(void* ptr)
// {
// 	glox::pmmAddChunk(ptr, glox::pmmChunkSize);
// }
void pmmFree(void* ptr, sizeT pageCount)
{
	if (ptr == nullptr)
		return;
	glox::pmmAddChunk(ptr, glox::pmmChunkSize * pageCount);
}
} // namespace glox

static void test()
{
	gloxDebugLog("Memory size: ", memorySize, " pmm test\nBefore:\n");
	for (const auto& it : pmmCtx)
	{
		gloxDebugLogln(&it, '-', (char*)&it + it.data.size);
	}

	constexpr auto size = 200;
	static glox::array<void*, size> ptrs;
	for (int i = 0; i < size; ++i)
		ptrs[i] = pmmAlloc();
	gloxDebugLog("\nAfter allocations:\n");
	for (const auto& it : pmmCtx)
	{
		gloxDebugLogln(&it, '-', (char*)&it + it.data.size);
	}
	for (int i = 0; i < size; i++)
		pmmFree(ptrs[i]);

	gloxDebugLog("\nAfter freeing:\n");
	for (const auto& it : pmmCtx)
	{
		gloxDebugLogln(&it, '-', (char*)&it + it.data.size);
	}
}

registerTest(test);
