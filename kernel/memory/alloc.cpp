#include "alloc.hpp"
#include "glox/assert.hpp"
#include "gloxor/mutex.hpp"
#include "llallocator.hpp"
#include "memory/pmm.hpp"
#include "system/danger.hpp"
#include "system/logging.hpp"
#include <glox/array.hpp>
#include <glox/math.hpp>
#include <glox/mutex.hpp>
#include <glox/util.hpp>
#include <gloxor/test.hpp>

/*
	Allocator idea is pretty simple, its similar in idea to slab
	Its segregated fit with freelist of free slots in a bucket
	Metadata is stored in a unrolled linked list that fits the page,
	so you can construct it in the buffer provided by pageAlloc()
	Alloc calculates bucket index from the given size, and pops address
	from bucket with free slot, if there isnt a free list, new one is allocated
	Dealloc is pricy as it requires traversal of all entries of the given address
	TODO:
	-	Traversing unrolled list of buckets in case of allocation might be slow
		if lot of buckets are full, reconsider adding full list to keep track
		of allocated buckets, so we dont lose the information of them being allocated
	-	Freeing is costly, perhaps one could have FIFO/HashTable to speedup freeing
	-	Massive fragmentation might occur if lot of chunks are partially filled
*/
void* operator new(size_t size)
{
	return glox::memalloc(size);
}

void* operator new[](size_t size)
{
	return glox::memalloc(size);
}

void operator delete(void* p, size_t size)
{
	glox::memdealloc(p, size);
}

void operator delete[](void* p, size_t size)
{
	glox::memdealloc(p, size);
}

struct freelist
{
	freelist* next;
	struct iterator
	{
		freelist* it;
		auto operator++() { return it = it->next; }
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
};
struct chunkPtr
{
	sizeT bytesUsed;
	freelist* list;
	constexpr bool isFull() const
	{
		return bytesUsed == glox::pmmChunkSize;
	}
};
struct metadata
{
	static constexpr sizeT chunkCount = (glox::pmmChunkSize - (sizeof(metadata*) + sizeof(sizeT))) / sizeof(chunkPtr);
	using chunkHeadersT = glox::array<chunkPtr, chunkCount>;
	metadata* next;
	sizeT usedChunks;
	chunkHeadersT chunkHeaders;
};

static_assert(sizeof(metadata) == glox::pmmChunkSize);
struct metadataCtx
{
	sizeT leftover; // how many bytes left
	metadata* list;
};

constexpr glox::array<int, 8> tinyBuckets{16, 32, 64, 128, 256, 512, 1024, 2048};
struct heapCtx
{
	static constexpr sizeT bucketCount = tinyBuckets.size();
	sizeT totalPages;
	//	freelist* bigAllocList;
	glox::array<metadataCtx, bucketCount> buckets;
};

static heapCtx globalHeap;
// alligns number upwards to next power of 2
// if one uses it for bucket index calculation, make sure to offset it
inline auto alignUpPow2(sizeT x)
{
	return x == 1 ? 1 : sizeof(sizeT) * 8 - glox::clz(x - 1);
}

inline sizeT size2bucket(sizeT size)
{
	gloxAssert(size != 0);
	if (size <= tinyBuckets[0])
		return 0;
	return alignUpPow2(size) - 4;
}

void* bigAlloc(sizeT size)
{
	return glox::pmmAllocator::alloc(size);
}

inline bool initChunk(chunkPtr& list, sizeT bucketSize)
{
	auto freshAddr = (freelist*)glox::pageAlloc();
	if (freshAddr == nullptr)
		return false;
	list.list = freshAddr;
	auto offset = bucketSize;
	for (sizeT i = 0; i < glox::pmmChunkSize; i += offset)
	{
		*freshAddr = {(freelist*)((uintptr)freshAddr + offset)};
		freshAddr = freshAddr->next;
	}
	freshAddr->next = nullptr;
	list.bytesUsed = 0;
	return true;
}

inline glox::pair<void*, bool> allocFromChunk(metadata::chunkHeadersT& chunks, sizeT bucketSize)
{
	for (auto&& it : chunks)
	{
		// completely borked, quickly fix
		if (it.list == nullptr)
		{
			if (!initChunk(it, bucketSize))
				return {nullptr, true};
		}
		// we need tagged pointers to mark if page is full or not
		// fallthrough from previous branch
		if (!it.isFull())
		{
			auto tmp = it.list;
			it.list = it.list->next;
			it.bytesUsed += bucketSize;
			return {tmp, true};
		}
	}
	return {nullptr, false};
}

inline bool freeChunk(chunkPtr& it, void* ptr, sizeT size)
{
	auto newentry = (freelist*)ptr;
	it.bytesUsed -= size;
	if (it.bytesUsed == 0)
	{
		glox::pageDealloc((void*)ALIGN((uintptr)it.list, glox::pmmChunkSize));
		return true;
	}
	newentry->next = it.list;
	it.list = newentry;
	return true;
}

inline bool freeMem(void* ptr, sizeT size)
{
	if (ptr == nullptr or size == 0)
	{
		return false;
	}
	if (size > tinyBuckets[tinyBuckets.size() - 1])
	{
		return glox::pmmAllocator::dealloc(ptr, size), true;
	}
	auto tbindex = size2bucket(size);
	auto realsize = tinyBuckets[tbindex];
	auto* curlist = globalHeap.buckets[tbindex].list;
	auto allignedptr = ALIGN((uintptr)ptr, glox::pmmChunkSize);
	for (auto iter = curlist; iter; iter = iter->next)
	{
		for (auto& it : iter->chunkHeaders)
		{
			if (allignedptr == ALIGN((uintptr)it.list, glox::pmmChunkSize))
				return freeChunk(it, ptr, realsize);
		}
	}
	return false;
}

inline void* allocMem(sizeT size)
{
	if (size == 0)
		return nullptr;
	if (size > tinyBuckets[tinyBuckets.size() - 1])
	{
		return bigAlloc(size);
	}
	auto index = size2bucket(size);
	auto bucketSize = tinyBuckets[index];
	auto& curList = globalHeap.buckets[index].list;
	// gloxDebugLog("Memalloc(", bucketSize, ")\n");
	auto* iter = curList;
	// TODO: Rewrite into inf loop, as its probably clearer
	if (iter == nullptr)
	{
		iter = (metadata*)glox::pageAllocZ();
		if (iter == nullptr)
			return nullptr;
		curList = iter;
	}
	auto iterNext = iter;
	do
	{
		iter = iterNext;
		if (auto val = allocFromChunk(iter->chunkHeaders, bucketSize); val.second)
		{
			return val.first;
		}
		iterNext = iter->next;
	} while (iterNext);
	iterNext = (metadata*)glox::pageAllocZ();
	if (iterNext == nullptr)
		return nullptr;
	iter->next = iterNext;
	return allocFromChunk(iter->chunkHeaders, bucketSize).first;
}

namespace glox
{

void* memalloc(sizeT size)
{
	glox::scopedLock<irqLock> _;
	return allocMem(size);
}

void memdealloc(void* ptr, sizeT size)
{
	glox::scopedLock<irqLock> _;
	if (!freeMem(ptr, size))
	{
		// TODO: Add debuging capabilities
		gloxDebugLogln("Failed to free addr: ", ptr, " of size: ", size);
	}
}

} // namespace glox
#ifdef TEST
[[maybe_unused]] static bool test()
{
	struct list
	{
		int x;
		list* next;
	};
	list *head = new list{.x = 21, .next = nullptr}, *iter = head;
	for (int i = 1; i < 20; ++i)
	{
		iter->next = new list{.x = i, .next = nullptr};
		iter = iter->next;
	}
	gloxPrint("List:\n");
	for (auto it = head; it; it = it->next)
	{
		gloxPrint(it->x, ' ');
	}
	gloxPrint("\nEndlist\n");
	for (auto it = head; it;)
	{
		auto tmp = it;
		it = it->next;
		glox::dealloc(tmp, 1);
	}
	return true;
}
registerTest("Allocator test",test);
#endif
