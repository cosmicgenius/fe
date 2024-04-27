#include "../include/input.hpp"

#include <iostream>
#include <string>

typedef mpq_class R;

int main() {
    InputHandler<R> ih(std::cin, std::cout, std::cerr);
    ih.handle_input();
}

