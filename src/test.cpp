#include "../include/algebra.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <iomanip>

void test_algebra() {
    clock_t tStart = clock();

    algebra::NodeStore<int> ns;
    const algebra::Node<int>* x = ns.new_node(1);
    const algebra::Node<int>* y = ns.new_node(2);
    const algebra::Mononode<int>* xx = ns.new_mononode(std::vector<algebra::NodeHash>{x->hash()});
    const algebra::Mononode<int>* yy = ns.new_mononode(std::vector<algebra::NodeHash>{y->hash()});
    const algebra::Polynode<int>* xy = ns.new_polynode(std::unordered_map<algebra::MononodeHash, int>
            {{xx->hash(), 1}, {yy->hash(), 1}});

    std::cout << x->to_string() << std::endl;
    std::cout << y->to_string() << std::endl;
    std::cout << xx->to_string() << std::endl;
    std::cout << yy->to_string() << std::endl;
    std::cout << xy->to_string() << std::endl;

    std::cout << "algebra: " << std::fixed << std::setprecision(3)
              << (double)(clock() - tStart) / CLOCKS_PER_SEC << "s"
              << std::endl;
}

int main() {
    test_algebra();
}

