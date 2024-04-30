#include "../include/algebra.hpp"
#include "../include/input.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <string>

#include <gmpxx.h>

typedef mpq_class R;

void test_algebra() {
    clock_t tStart = clock();

    algebra::NodeStore<R> ns;
    const algebra::Node<R>* x = ns.node(1);
    const algebra::Node<R>* y = ns.node(2);
    const algebra::Node<R>* a = ns.node(3);
    const algebra::Node<R>* b = ns.node(4);

    const algebra::Polynode<R>* px = ns.polynode({{ns.mononode({{x->hash, 1}})->hash, 1}});
    const algebra::Polynode<R>* py = ns.polynode({{ns.mononode({{y->hash, 1}})->hash, 1}});

    const algebra::Polynode<R>* x_plus_y = ns.polynode({
            {ns.mononode({{x->hash, 1}})->hash, 1},
            {ns.mononode({{y->hash, 1}})->hash, 1}
        });

    assert(*px + *py == x_plus_y);

    const algebra::Polynode<R>* n_x_plus_y = -(*x_plus_y);

    const algebra::Polynode<R>* z = *n_x_plus_y + *x_plus_y;

    assert(*z == *ns.zero_p());
    assert(*x_plus_y - *px == py);
    assert(z->to_string() == "0");

    const algebra::Polynode<R>* fzero = ns.polynode({{
            ns.mononode({{ns.node(ns.zero_p()->hash)->hash, 1}})->hash, 1
        }});

    assert(!(*fzero == *ns.zero_p()));
    assert(fzero->to_string() == "f(0)");

    const algebra::Polynode<R>* a_plus_b = ns.polynode({
            {ns.mononode({{a->hash, 1}})->hash, 1},
            {ns.mononode({{b->hash, 1}})->hash, 1}
        });

    const algebra::Polynode<R>* foil = ns.polynode({
            {ns.mononode({{a->hash, 1}, {x->hash, 1}})->hash, 1}, 
            {ns.mononode({{a->hash, 1}, {y->hash, 1}})->hash, 1}, 
            {ns.mononode({{b->hash, 1}, {x->hash, 1}})->hash, 1}, 
            {ns.mononode({{b->hash, 1}, {y->hash, 1}})->hash, 1},
        });

    assert(*x_plus_y * *a_plus_b == foil);

    int N = 10; // Needs to be small to avoid overflow

    std::vector<std::pair<algebra::MononodeHash, R>> binom_summands;
    binom_summands.reserve(N + 1);

    R coeff = 1;
    for (int i = 0; i <= N; i++) {
        binom_summands.emplace_back(ns.mononode({{x->hash, N - i}, {y->hash, i}})->hash, coeff);

        coeff *= N - i;
        coeff /= i + 1;
    }

    const algebra::Polynode<R>* xy_prod = ns.one_p();
    for (int i = 0; i < N; i++) {
        xy_prod = *xy_prod * *x_plus_y;
    }
    const algebra::Polynode<R>* binom = ns.polynode(binom_summands);

    assert(*binom == *xy_prod);
    assert(*py->sub(2, *px) == *px);
    
    const algebra::Polynode<R>* fx = ns.polynode({{ns.mononode({{ns.node(px->hash)->hash, 1}})->hash, 1}});
    const algebra::Polynode<R>* ffoil = ns.polynode({{ns.mononode({{ns.node(foil->hash)->hash, 1}})->hash, 1}});

    assert(*fx->sub(1, *foil) == *ffoil);

    const algebra::Polynode<R>* x_pow = ns.polynode({{
            ns.mononode({{x->hash, N}})->hash,
        (1 << N)}});

    assert(*xy_prod->sub(2, *px) == *x_pow);

    const algebra::Polynode<R>* fx_minus_fy = ns.polynode({
            {ns.mononode({{ns.node(px->hash)->hash, 1}})->hash, 1},
            {ns.mononode({{ns.node(py->hash)->hash, 1}})->hash, -1}
        });

    assert(*fx_minus_fy == *(*px - *py)->apply_func(*py));

    std::cout << "algebra: " << std::fixed << std::setprecision(3)
              << (double)(clock() - tStart) / CLOCKS_PER_SEC << "s"
              << std::endl;
}

void test_input() {
    clock_t tStart = clock();

    std::istringstream in(
        "hyp x1 + f(x2 * x1 + x3) - f(2 x1) * f(x3) * x2\n"
        "sub h1 x2 0\n"
        "sub h2 x1 -f(1 * x3)\n"
        "hyp x1 - x2\n"
        "sub h4 x2 x3 + x4\n"
        "app h5 x3 + x4\n"
        "sub h6 x3 x1 - x4\n"
        "hyp f(f(x1 + x2)) - f(2 x1) - 2 f(x2)\n"
        "sub h8 x1 0\n"
        "sub h8 x2 0\n"
        "sub h9 x2 0\n"
        "end");

    std::stringstream out, err;

    InputHandler<R> ih(in, out, err);
    ih.handle_input(true, true);

    std::vector<std::string> outputs;
    std::string output;
    while (std::getline(out, output)) outputs.push_back(output);

    std::vector<std::string> errors;
    std::string error;
    while (std::getline(err, error)) errors.push_back(error);

    std::set<int> zeros{3, 7};
    assert(outputs.size() == 1 + 11 + 1 + 9);
    for (size_t i = 1; i <= 11; i++) {
        const std::string &s = outputs[i];

        std::string pref = "h";
        pref += std::to_string(i);
        
        size_t split = s.find_first_of(":");
        assert(s.substr(0, split) == pref);
        if (zeros.find(i) != zeros.end()) assert(s.substr(split + 2) == "0");
    }

    assert(errors.size() == 0);

    std::cout << "input: " << std::fixed << std::setprecision(3)
              << (double)(clock() - tStart) / CLOCKS_PER_SEC << "s"
              << std::endl;
}

int main() {
    test_algebra();
    test_input();
}

