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
    const algebra::Polynode<int>* px = ns.new_polynode(std::unordered_map<algebra::MononodeHash, int>
            {{xx->hash(), 1}});
    const algebra::Polynode<int>* py = ns.new_polynode(std::unordered_map<algebra::MononodeHash, int>
            {{yy->hash(), 1}});

    const algebra::Node<int>* fx = ns.new_node(px->hash());
    const algebra::Node<int>* fy = ns.new_node(py->hash());
    const algebra::Node<int>* fxy = ns.new_node(xy->hash());

    std::cout << "x: " << x->hash() << std::endl;
    std::cout << "y: " << y->hash() << std::endl;
    std::cout << "XX: " << xx->hash() << std::endl;
    std::cout << "yy: " << yy->hash() << std::endl;
    std::cout << "xy: " << xy->hash() << std::endl;
    std::cout << "px: " << px->hash() << std::endl;
    std::cout << "py: " << py->hash() << std::endl;

    std::cout << xy->to_string() << std::endl;
    std::cout << fx->to_string() << std::endl;
    std::cout << fy->to_string() << std::endl;
    std::cout << fxy->to_string() << std::endl;

    std::cout << "algebra: " << std::fixed << std::setprecision(3)
              << (double)(clock() - tStart) / CLOCKS_PER_SEC << "s"
              << std::endl;
}

int main() {
    test_algebra();
}

