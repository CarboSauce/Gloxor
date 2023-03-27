#pragma once
#include "glox/iterator.hpp"
#include <glox/assert.hpp>
#include <cstddef>
#include <sys/types.h>
#include "glox/macros.hpp"
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
/*
 * Dear mortals reading it, this is offsetof equivalent 
 * that works with C++ member pointers
 * Based on proposal: 
 * https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0908r0.html
 * please make it live :(
 */
template<typename Base, typename Member>
GLOX_ALWAYS_INLINE 
inline std::ptrdiff_t offset_of(const Member Base::* ptr)
{
	alignas(Base) char ahack[sizeof(Base)]{};
	const Base* base = reinterpret_cast<const Base*>(ahack);
	return reinterpret_cast<std::ptrdiff_t>(&(base->*ptr)) 
		- reinterpret_cast<std::ptrdiff_t>(base);
}

/*
 * @brief List node type, if used for intrusive_list
 * 		  define it as 'list_node'
 */
struct list_node
{
	list_node* next = this, *prev = this;
	friend bool operator==(const list_node&,const list_node&) = default;
	void remove()
	{
		list_node* const n = next;
		list_node* const p = prev;
		p->next = n;
		n->prev = p;
	}
	void insert(list_node* new_current)
	{
		new_current->next = this;
		new_current->prev = this->prev;
		this->prev->next = new_current;
		this->prev = new_current;
	}
};
template<typename T,list_node T::*NodePtr = &T::list_node>
class intrusive_list
{
	static_assert(std::is_same_v<glox::list_node,decltype(T::list_node)>);
	// .prev is last element, .next is first
	list_node sentinel;
	size_t list_size;
	using node_t = list_node;
	//constexpr static auto node_offset = offsetof(T,.*NodePtr);
	template<bool IsConst>
	class iter_base
	{
		using iter = std::conditional_t<IsConst,const list_node*,list_node*>;
		iter it;
		public:
		iter_base(iter p) : it(p){}
		friend intrusive_list;
		using pointer = std::conditional_t<IsConst,const T*,T*>;
		using reference = std::conditional_t<IsConst,const T&,T&>;
		using iterator_category= std::bidirectional_iterator_tag;

		iter_base() = default;
		iter_base(pointer p) : it(&(p->*NodePtr)) {}
		iter_base(const iter_base&) = default;
		iter_base(iter_base&&) = default;
		iter_base& operator=(const iter_base&) = default;
		iter_base& operator=(iter_base&&) = default;
		operator pointer() const 
		{ return intrusive_list::ptr_from_node(it);}
		operator iter_base<true>() const { return iter_base<true>(it);}

		iter_base next() const { return it->next;}
		iter_base prev() const { return it->prev;}

		iter_base operator++() { return it = it->next; }
		iter_base operator++(int) 
		{
			auto tmp = it;
			it = it->next;
			return tmp;
		}
		iter_base operator--() { return it = it->prev; }
		iter_base operator--(int) 
		{ 
			auto tmp = it;
			it = it->prev; 
			return tmp;
		}
		friend bool operator==(iter_base l, iter_base r) = default;
		reference operator*() const { return *operator->(); }
		pointer operator->() const { return static_cast<T*>(*this);}
	};
	public:
	//struct iterator : public iter_base<false> {using iter_base<false>::iter_base;};
	//struct const_iterator : public iter_base<true>{using iter_base<true>::iter_base;};
	using iterator = iter_base<false>;
	using const_iterator = iter_base<true>;
	friend iterator;

	intrusive_list() : sentinel{},list_size(0){}
	intrusive_list(const intrusive_list&) = delete;
	intrusive_list& operator=(const intrusive_list&) = delete;
	intrusive_list(intrusive_list&& other)
	{
		sentinel = other.sentinel;
		list_size = other.list_size;
		other.sentinel = {&sentinel,&sentinel};
		other.list_size = 0;
	}
	intrusive_list& operator=(intrusive_list&& other)
	{
		gloxAssert(other.header.size);
		sentinel = other.sentinel;
		list_size = other.list_size;
		other.sentinel = {&sentinel,&sentinel};
		other.list_size = 0;
		return *this;
	}

	auto size() const { return list_size; }
	iterator begin() { return sentinel.next; }
	// non conformant cuz end()-- is ub
	iterator end() { return {(node_t*)&sentinel}; }
	T& back() { return *ptr_from_node(sentinel.prev);}
	T& front() { return *ptr_from_node(sentinel.next);}
	const_iterator begin() const { return {sentinel.next}; }
	const_iterator end() const { return {&sentinel}; }
	const T& back() const { return *ptr_from_node(sentinel.prev);}
	const T& front() const { return *ptr_from_node(sentinel.next);}
	bool is_empty() const { return list_size == 0; } 

	// todo: replace null checks with calls to insert that would check null
	void push_back(T* node)
	{
		impl_insert(&sentinel,&(node->*NodePtr));
	};
	void push_front(T* node)
	{
		impl_insert(sentinel.next,&(node->*NodePtr));
	}
	iterator insert(iterator iter, T* node)
	{
		impl_insert(iter.it,&(node->*NodePtr));
		return node;
	}
	iterator erase(iterator iter)
	{
		iter.it->remove();
		return iter;
	}
	void clear()
	{
		for(auto tmp = sentinel.prev;tmp!=&sentinel;)
		{
			auto saved = tmp->prev;
			tmp->remove();
			tmp = saved;
		} 
		sentinel = {&sentinel,&sentinel};
		list_size = 0;
	}
	template<typename Cb>
	void clear(Cb fn)
	{
		for(auto tmp = sentinel.prev;tmp!=&sentinel;tmp=tmp->prev)
		{
			auto saved = tmp->prev;
			fn(ptr_from_node(tmp->remove()));
			tmp = saved;
		} 
		sentinel = {&sentinel,&sentinel};
		list_size = 0;
	}
	private:
	GLOX_ALWAYS_INLINE
	static const T* ptr_from_node(const list_node *const a)
	{
		return reinterpret_cast<const T*>(
				reinterpret_cast<const char*>(a) - offset_of(NodePtr));
	}
	GLOX_ALWAYS_INLINE
	static T* ptr_from_node(list_node *const a)
	{
		return const_cast<T*>(
				intrusive_list::ptr_from_node(static_cast<const list_node*>(a)));
	}
	void impl_insert(list_node* new_next, list_node* new_current)
	{
		new_next->insert(new_current);
		list_size += 1;
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
};
} // namespace glox
