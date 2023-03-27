#include "glox/tuple.hpp"
#include <iostream>
#include <type_traits>
int main()
{
	auto tpl = glox::tuple{"Hello", "World", 5};
	glox::apply([](const auto&... args)
					{
				std::cout << "Tuple: ";
				((std::cout <<  args << '\n'), ...); },
					tpl);
	auto [a, b, c] = tpl;
	std::cout << "Unpacking: " << b << '\n';
	glox::for_each(tpl,
						[i = 0](auto&& val) mutable
						{ std::cout << "Type " << i++ << ": Value = " << val << '\n'; });
	static_assert(std::is_trivial_v<glox::tuple<int, int, int>> == true);
}
