#pragma once
namespace glox
{
	
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