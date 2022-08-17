#pragma once
#include "glox/iterator.hpp"
#include <glox/assert.hpp>
namespace glox
{
// keep for now, remove and replace legacy code once intrusive_list is tested
template <typename T>
struct node : public T
{
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
		return static_cast<T>(l) <=> static_cast<T>(r);
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
template<typename T>
struct list_node
{
	T* next = nullptr, *prev = nullptr;
	friend bool operator==(const list_node&,const list_node&) = default;
};

template<typename T>
class intrusive_list
{
	T* first_node = nullptr, *last_node = nullptr;
	public:
	class iterator
	{
		T* it;
		public:
		using iterator_category= std::bidirectional_iterator_tag;
		iterator() = default;
		iterator(T* p) : it(p) {}
		iterator(const iterator&) = default;
		iterator(iterator&&) = default;
		iterator& operator=(const iterator&) = default;
		iterator& operator=(iterator&&) = default;
		auto operator++() { return it = it->list_node.next; }
		auto operator--() { return it = it->list_node.prev; }
		auto operator++(int) 
		{ 
			auto tmp = it;
			it = it->list_node.next;
			return tmp;
		}
		auto operator--(int) 
		{ 
			auto tmp = it;
			it = it->list_node.prev; 
			return tmp;
		}
		friend bool operator==(iterator l, iterator r) = default;
		auto& operator*() const { return *it; }
		auto& operator->() const { return it; }
	};
	intrusive_list() = default;
	//intrusive_list(T* pb, T* pe) : first_node(pb),last_node(pe){}
	intrusive_list(const intrusive_list&) = delete;
	intrusive_list& operator=(const intrusive_list&) = delete;
	intrusive_list(intrusive_list&& other)
	{
		first_node = other.first_node; other.first_node = nullptr;
		last_node  = other.last_node ; other.last_node  = nullptr;
	}
	intrusive_list& operator=(intrusive_list&& other)
	{
		gloxAssert(first_node && last_node);
		first_node = other.first_node; other.first_node = nullptr;
		last_node  = other.last_node ; other.last_node  = nullptr;
		return *this;
	}

	iterator begin() { return {first_node}; }
	// non conformant cuz end()-- is ub
	iterator end() { return {nullptr}; }
	T& back() { return *last_node; }
	T& front() { return *first_node; }
	const iterator begin() const { return {first_node}; }
	const iterator end() const { return {last_node}; }
	const T& back() const { return *last_node; }
	const T& front() const { return *first_node; }
	bool is_empty() const { return first_node == nullptr;}

	// todo: replace null checks with calls to insert that would check null
	void push_back(T* node)
	{
		if (last_node == nullptr)
		{
			node->list_node.next = nullptr;
			node->list_node.prev = nullptr;
			first_node = node;
			last_node = node;
		}
		else
		{
			last_node->list_node.next = node;
			node->list_node.prev = last_node;
			last_node = node;	
		}
	};
	void push_front(T* node)
	{
		if (first_node == nullptr)
		{
			node->list_node.next = nullptr;
			node->list_node.prev = nullptr;
			first_node = node;
			last_node = node;
		}
		else
		{
			first_node->list_node.prev = node;
			node->list_node.next = first_node;
			first_node = node;
		}
	}
	iterator insert(iterator iter, T* node)
	{
		node->list_node.prev = iter->list_node.prev;
		node->list_node.next = iter;
		iter->list_node.prev = node;
		iter->list_node.prev->list_node.next = node;
		return node;
	}
	iterator erase(iterator iter)
	{
		iter->list_node.next->list_node.prev = iter->list_node.prev;
		iter->list_node.prev->list_node.next = iter->list_node.next;
		return iter->list_node.next;
	}
	template<typename Cb>
	void clear(Cb fn)
	{
		for(;last_node;last_node=last_node->list_node.prev)
		{
			fn(last_node);
		} 
		first_node = last_node;
	}
};

template<typename T>
struct fwd_list_node
{
	T* next;
};

template<typename T>
class intrusive_fwd_list
{
	T *first_node=nullptr;
	public:
	struct iterator
	{
		T* it;
		public:
		using iterator_category= std::forward_iterator_tag;
		iterator() = default;
		iterator(T* p) : it(p) {}
		iterator(const iterator&) = default;
		iterator(iterator&&) = default;
		iterator& operator=(const iterator&) = default;
		iterator& operator=(iterator&&) = default;
		auto operator++() { return it = it->fwd_list_node.next; }
		auto operator++(int) 
		{ 
			auto tmp = it;
			it = it->list_node.next;
			return tmp;
		}
		friend bool operator==(iterator l, iterator r) = default;
		auto& operator*() const { return *it; }
		auto& operator->() const { return it; }
	};
	intrusive_fwd_list() = default;
	intrusive_fwd_list(T* pb) : first_node(pb){}
	intrusive_fwd_list(const intrusive_fwd_list&) = delete;
	intrusive_fwd_list& operator=(const intrusive_fwd_list&) = delete;
	intrusive_fwd_list(intrusive_fwd_list&& other)
	{
		first_node = other.first_node; other.first_node = nullptr;
	}
	intrusive_fwd_list& operator=(intrusive_fwd_list&& other)
	{
		gloxAssert(first_node);
		first_node = other.first_node; other.first_node = nullptr;
		return *this;
	}
	iterator begin() { return {first_node}; }
	iterator end() { return {nullptr}; }
	T& front() { return *first_node; }
	const iterator begin() const { return {first_node}; }
	const iterator end() const { return {nullptr}; }
	const T& front() const { return *first_node; }
	bool is_empty() const { return first_node == nullptr;}
	void push_front(T* p)
	{
		p->fwd_list_node.next = first_node;
		first_node = p;
	}
	void pop_front()
	{
		if (first_node) first_node = first_node->fwd_list_node.next;
	}
	void pop_front_unsafe()
	{
		first_node = first_node->fwd_list_node.next;
	}
	void erase_after(iterator it)
	{
		auto&& oneafter = it->fwd_list_node.next;
		it->fwd_list_node.next = oneafter->fwd_list_node.next; 
	}
	void insert_after(iterator it, T* val)
	{
		auto&& tmp = it->fwd_list_node.next;
		val->fwd_list_node.next = tmp->fwd_list_node.next;
		it->fwd_list_node.next = val;
	}
	template<typename Cb>
	void clear(Cb fn)
	{
		while(first_node)
		{
			auto tmp = first_node;
			first_node = first_node->fwd_list_node.next;
			fn(tmp);
		}
	}
};
// legacy
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
} // namespace glox
