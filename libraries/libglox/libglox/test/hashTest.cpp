#include "glox/algo.hpp"
#include <iostream>

int main()
{   
    std::cout << glox::fnv_hash("Test 123") << '\n';
    std::cout << glox::djb2_hash("Test 123") << '\n';
}