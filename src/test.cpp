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
    const algebra::Node<int>* x = ns.node(1);
    const algebra::Node<int>* y = ns.node(2);
    const algebra::Node<int>* a = ns.node(3);
    const algebra::Node<int>* b = ns.node(4);

    const algebra::Polynode<int>* px = ns.polynode({{ns.mononode({x->hash()})->hash(), 1}});
    const algebra::Polynode<int>* py = ns.polynode({{ns.mononode({y->hash()})->hash(), 1}});

    const algebra::Polynode<int>* x_plus_y = ns.polynode({{ns.mononode({x->hash()})->hash(), 1},
            {ns.mononode({y->hash()})->hash(), 1}});

    assert(*px + *py == x_plus_y);

    const algebra::Polynode<int>* n_x_plus_y = ns.polynode({{ns.mononode({x->hash()})->hash(), -1},
            {ns.mononode({y->hash()})->hash(), -1}});

    const algebra::Polynode<int>* z = *n_x_plus_y + *x_plus_y;
    const algebra::Polynode<int>* zero = ns.polynode({});

    assert(*z == *zero);
    assert(z->to_string() == "0");

    const algebra::Polynode<int>* fzero = ns.polynode({{
            ns.mononode({ns.node(zero->hash())->hash()})->hash(), 1
        }});

    assert(!(*fzero == *zero));
    assert(fzero->to_string() == "f(0)");

    const algebra::Polynode<int>* a_plus_b = ns.polynode({{ns.mononode({a->hash()})->hash(), 1},
            {ns.mononode({b->hash()})->hash(), 1}});

    const algebra::Polynode<int>* foil = ns.polynode({
            {ns.mononode({a->hash(), x->hash()})->hash(), 1}, 
            {ns.mononode({a->hash(), y->hash()})->hash(), 1}, 
            {ns.mononode({b->hash(), x->hash()})->hash(), 1}, 
            {ns.mononode({b->hash(), y->hash()})->hash(), 1},
        });

    assert(*x_plus_y * *a_plus_b == foil);

    std::cout << "algebra: " << std::fixed << std::setprecision(3)
              << (double)(clock() - tStart) / CLOCKS_PER_SEC << "s"
              << std::endl;
}

int main() {
    test_algebra();
}

