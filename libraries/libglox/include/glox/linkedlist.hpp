#pragma once
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
struct intrusive_list_node
{
	T* next = nullptr, *prev = nullptr;
};

template<typename T>
class intrusive_list
{
	T* first_node, *last_node;
	public:
	class iterator
	{
		T* it;
		public:
		auto operator++() { return it = it->list_node.next; }
		auto operator--() { return it = it->list_node.prev; }
		friend auto operator<=>(iterator, iterator) = default;
		auto& operator*() const { return *it; }
		auto& operator->() const { return it; }
	};
	iterator begin() { return {first_node}; }
	iterator end() { return {nullptr}; }
	T& back() { return *last_node; }
	T& front() { return *first_node; }
	const iterator begin() const { return {first_node}; }
	const iterator end() const { return {nullptr}; }
	const T& back() const { return {last_node}; }
	const T& front() const { return *first_node; }

	void push_back(T* node)
	{
		last_node->list_node.next = node;
		node->list_node.prev = last_node;
		last_node = node;
	};
	void push_front(T* node)
	{
		first_node->list_node.prev = node;
		node->list_node.next = first_node;
		first_node = node;
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

} // namespace glox
