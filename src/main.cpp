#include "../include/input.hpp"

#include <iostream>
#include <string>

int main() {
    InputHandler ih(std::cin, std::cout, std::cerr);
    ih.handle_input();
}

