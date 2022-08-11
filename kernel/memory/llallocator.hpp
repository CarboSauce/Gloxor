#pragma once
#include "glox/linkedlist.hpp"
#include "gloxor/types.hpp"
/*
	Both of those functions actually return nullptr on OOM situations
	Single chunk allocator is seperate for purpouses of optimization.
	I very much doubt compiler can eliminate loop when page count is 1.
	Loop doesnt need to be there because we can guarantee that if
	node is present in the list, it has atleast 1 free page
*/
template <typename T>
inline void* alloc_from_chunk(glox::list<T>& chunk, size_t chunkSize)
{
	auto& i = *chunk.front;
	i.size -= chunkSize;
	if (i.size == 0)
	{
		i = *i.next;
		if (i.next != nullptr)
			i.next->prev = nullptr;
		return &i;
	}
	return reinterpret_cast<void*>(reinterpret_cast<uintptr>(&i) + i.size);
}

template <typename T>
inline void* alloc_from_chunk(glox::list<T>& chunk, size_t chunkSize, sizeT pageCount)
{
	const auto allocSize = chunkSize * pageCount;
	for (auto& it : chunk)
	{
		if (it.size < allocSize)
			continue;
		it.size -= allocSize;
		if (it.size == 0)
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
		return reinterpret_cast<void*>(reinterpret_cast<uintptr>(&it) + it.size);
	}

	return nullptr;
}

// this functions seems like it could be eliminated and added to insert chunk
template <typename T>
inline void append_chunk(T*& back, T* chunk, sizeT length)
{
	if ((uintptr)back + back->size == (uintptr)chunk)
	{
		back->size += length;
	}
	else
	{
		back->next = chunk;
		chunk->prev = back;
		back = chunk;
	}
}

template <typename T>
inline void prepend_chunk(T*& front, T* chunk, sizeT length)
{
	if ((uintptr)chunk + length == (uintptr)front)
	{
		chunk->size = front->size + length;
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
template <typename T>
inline void insert_chunk(T*& from, T* chunk, sizeT length)
{
	auto* it = from;
	if (it > chunk)
	{
		if ((uintptr)chunk + length == (uintptr)it)
		{
			chunk->size += it->size;
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
		if ((uintptr)it + it->size == (uintptr)chunk)
		{
			it->size += length;
			auto nextChunk = it->next;
			if ((uintptr)it + it->size == (uintptr)nextChunk)
			{
				it->size += nextChunk->size;
				auto farAhead = nextChunk->next;
				farAhead->prev = it;
				it->next = farAhead;
			}
			return;
		}
	}
	if ((uintptr)chunk + length == (uintptr)it)
	{
		chunk->size += it->size;
		it->next->prev = chunk;
		chunk->next = it->next;
		chunk->prev = it->prev;
		it->prev->next = chunk;
		return;
	}
	chunk->insert(it->prev, it);
}
