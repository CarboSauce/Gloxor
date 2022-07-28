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
};
struct chunkPtr
{
	sizeT bytesUsed;
	freelist* list;
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
	sizeT pageCount;
	metadata* list;
};

constexpr glox::array<int, 6> tinyBuckets{16, 32, 64, 128, 256, 512};
struct heapCtx
{
	static constexpr sizeT bucketCount = tinyBuckets.size();
	sizeT totalPages;
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
	if (size > tinyBuckets[tinyBuckets.size() - 1])
		return tinyBuckets.size();
	if (size <= 8)
	{
		return 0;
	}
	return alignUpPow2(size) - 3;
}

namespace glox
{

}
