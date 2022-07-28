#include "alloc.hpp"
#include "llallocator.hpp"
#include "memory/pmm.hpp"
#include <glox/array.hpp>
#include <glox/math.hpp>

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
	metadata* next;
	sizeT usedChunks;
	glox::array<chunkPtr, chunkCount> chunkHeaders;
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
	freelist* bigAllocList;
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
	if (size <= 8)
		return 0;
	return alignUpPow2(size) - 3;
}

void* bigAlloc(sizeT size)
{
	return glox::pmmAllocator::alloc(size);
}

bool initChunk(chunkPtr& list, sizeT bucketSize)
{
	auto freshAddr = (freelist*)glox::pageAllocZ();
	if (freshAddr == nullptr) return false; 
	list.list = freshAddr;
	auto offset = bucketSize;
	for (sizeT i = 0; i < glox::pmmChunkSize; i += offset)
	{
		freshAddr->next = (freelist*)((uintptr)freshAddr+offset);
		freshAddr = freshAddr->next;
	}
	freshAddr->next = nullptr;
	list.bytesUsed = 0;
	return true;
}

namespace glox
{

void* memalloc(sizeT size)
{
	if (size == 0) return nullptr;
	if (size > tinyBuckets[tinyBuckets.size() - 1])
	{
		return bigAlloc(size);
	}
	auto index = size2bucket(size);
	auto bucketSize = tinyBuckets[index];
	auto& curList = globalHeap.buckets[index].list;
	if (curList == nullptr)
	{
		curList = (metadata*)pageAllocZ();
	}
	for (auto iter = curList; iter != nullptr; iter = iter->next)
	{
		for (auto&& it : curList->chunkHeaders)
		{
			// completely borked, quickly fix
			if (it.list == nullptr)
			{
				if (!initChunk(it, index)) return nullptr;
			}
			// we need tagged pointers to mark if page is full or not
			// fallthrough from previous branch
			if (!it.isFull())
			{
				auto tmp = it.list;
				it.list = it.list->next;
				it.bytesUsed += bucketSize;
				return tmp;
			}
		}
	}
	//TODO: Expand list when not empty
	return nullptr;
}

void memdealloc(void* ptr, sizeT size)
{

}


}
