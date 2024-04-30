#include "../include/randomize.hpp"

#include <algorithm>
#include <random>

template<class R>
std::string randomize::to_random_string(const algebra::Node<R> &n, algebra::NodeStore<R> &node_store) {
    switch (n.get_type()) {
        case algebra::NodeType::POL: return std::string("f(") + 
            to_random_string(*node_store.get_polynode(n.get_polynode_hash()), node_store) + std::string(")");
        case algebra::NodeType::VAR: return std::string("x") + std::to_string(n.get_var());
    }
    return "";
}

template<class R>
std::string randomize::to_random_string(const algebra::Mononode<R> &m, algebra::NodeStore<R> &node_store) {
    if (m.get_degree() == 0) return "";

    std::vector<algebra::NodeHash> factors;
    factors.reserve(m.get_degree());

    for (const std::pair<algebra::MononodeHash, int> &f : m) {
        factors.insert(factors.end(), f.second, f.first);
    }

    std::random_device rd;
    std::default_random_engine generator(rd());

    std::shuffle(factors.begin(), factors.end(), generator);

    // Joins strings by a space
    std::string res = to_random_string(*node_store.get_node(factors.front()), node_store);
    for (auto it = ++factors.begin(); it != factors.end(); it++) {
        res += " ";
        res += to_random_string(*node_store.get_node(it), node_store);
    }
    return res;
}

template<class R>
std::string randomize::to_random_string(const algebra::Polynode<R> &p, algebra::NodeStore<R> &node_store) {
    std::vector<std::pair<algebra::MononodeHash, R>> summands = p.get_summands();
    if (summands.empty()) return "0";

    std::random_device rd;
    std::default_random_engine generator(rd());

    std::shuffle(summands.begin(), summands.end(), generator);

    // Joins strings by +
    // Not pretty 
    R coeff = summands.begin()->second;
    R a_coeff = abs(coeff);
    const algebra::Mononode<R>* mono = node_store.get_mononode(summands.begin()->first);

    std::string res = 
        (*mono == *node_store.one_m()) ?
            ((coeff > 0 ? "" : "-") + algebra::R_to_string(a_coeff))
        :
            ((coeff > 0 ? "" : "-") + (a_coeff == 1 ? "" : algebra::R_to_string(a_coeff) + " ")
             + to_random_string(*mono, node_store));

    for (auto it = ++summands.begin(); it != summands.end(); it++) {
        coeff = it->second;
        a_coeff = abs(coeff);

        mono = node_store.get_mononode(it->first);
        
        if (*mono == *node_store.one_m()) {
            res += ((coeff > 0 ? " + " : " - ") + algebra::R_to_string(a_coeff));
        } else {
            res += (coeff > 0 ? " + " : " - ")
                + (a_coeff == 1 ? "" : algebra::R_to_string(a_coeff) + " ")
                + to_random_string(*mono, node_store); 
        }
    }
    return res;
}
