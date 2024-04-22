#include "../include/algebra.hpp"
#include "../include/input.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <string>

void test_algebra() {
    clock_t tStart = clock();

    algebra::NodeStore<int> ns;
    const algebra::Node<int>* x = ns.node(1);
    const algebra::Node<int>* y = ns.node(2);
    const algebra::Node<int>* a = ns.node(3);
    const algebra::Node<int>* b = ns.node(4);

    const algebra::Polynode<int>* px = ns.polynode({{ns.mononode({x->hash})->hash, 1}});
    const algebra::Polynode<int>* py = ns.polynode({{ns.mononode({y->hash})->hash, 1}});

    const algebra::Polynode<int>* x_plus_y = ns.polynode({{ns.mononode({x->hash})->hash, 1},
            {ns.mononode({y->hash})->hash, 1}});

    assert(*px + *py == x_plus_y);

    const algebra::Polynode<int>* n_x_plus_y = -(*x_plus_y);

    const algebra::Polynode<int>* z = *n_x_plus_y + *x_plus_y;

    assert(*z == *ns.zero());
    assert(*x_plus_y - *px == py);
    assert(z->to_string() == "0");

    const algebra::Polynode<int>* fzero = ns.polynode({{
            ns.mononode({ns.node(ns.zero()->hash)->hash})->hash, 1
        }});

    assert(!(*fzero == *ns.zero()));
    assert(fzero->to_string() == "f(0)");

    const algebra::Polynode<int>* a_plus_b = ns.polynode({{ns.mononode({a->hash})->hash, 1},
            {ns.mononode({b->hash})->hash, 1}});

    const algebra::Polynode<int>* foil = ns.polynode({
            {ns.mononode({a->hash, x->hash})->hash, 1}, 
            {ns.mononode({a->hash, y->hash})->hash, 1}, 
            {ns.mononode({b->hash, x->hash})->hash, 1}, 
            {ns.mononode({b->hash, y->hash})->hash, 1},
        });

    assert(*x_plus_y * *a_plus_b == foil);

    int N = 28; // Needs to be small to avoid overflow

    std::unordered_map<algebra::MononodeHash, int> binom_summands;

    int coeff = 1;
    for (int i = 0; i <= N; i++) {
        std::vector<algebra::NodeHash> term;
        term.reserve(N);

        for (int j = 0; j < N; j++) {
            if (j >= i) term.push_back(x->hash);
            else term.push_back(y->hash);
        }

        binom_summands[ns.mononode(std::move(term))->hash] = coeff;

        if (i != N) {
            coeff *= N - i;
            coeff /= i + 1;
        }
    }

    const algebra::Polynode<int>* xy_prod = ns.one();
    for (int i = 0; i < N; i++) {
        xy_prod = *xy_prod * *x_plus_y;
    }
    const algebra::Polynode<int>* binom = ns.polynode(std::move(binom_summands));

    assert(binom == xy_prod);
    assert(*py->sub(2, *px) == *px);
    
    const algebra::Polynode<int>* fx = ns.polynode({{ns.mononode({ns.node(px->hash)->hash})->hash, 1}});
    const algebra::Polynode<int>* ffoil = ns.polynode({{ns.mononode({ns.node(foil->hash)->hash})->hash, 1}});

    assert(*fx->sub(1, *foil) == *ffoil);

    const algebra::Polynode<int>* x_pow = ns.polynode({{
            ns.mononode(std::vector<algebra::NodeHash>(N, x->hash))->hash,
        (1 << N)}});

    assert(*xy_prod->sub(2, *px) == *x_pow);

    const algebra::Polynode<int>* fx_minus_fy = ns.polynode({
            {ns.mononode({ns.node(px->hash)->hash})->hash, 1},
            {ns.mononode({ns.node(py->hash)->hash})->hash, -1}
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
        "end");

    std::stringstream out, err;

    InputHandler ih(in, out, err);
    ih.handle_input();

    std::vector<std::string> outputs;
    std::string output;
    while (std::getline(out, output)) outputs.push_back(output);

    std::vector<std::string> errors;
    std::string error;
    while (std::getline(err, error)) errors.push_back(error);

    std::set<int> zeros{3, 7};
    assert(outputs.size() == 8);
    for (size_t i = 1; i < outputs.size(); i++) {
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

