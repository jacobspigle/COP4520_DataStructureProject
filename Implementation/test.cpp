#include <iostream>
#include "concurrent.hpp"

int main(void)
{
    auto tree = new ConcurrentTree<uint32_t>();
    tree->InsertOrUpdate(5, 5);
}