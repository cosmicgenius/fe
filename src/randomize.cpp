#include "../include/randomize.hpp"

#include <cmath>
#include <gmpxx.h>
#include <algorithm>

template<class R>
randomize::Randomizer<R>::Randomizer(algebra::NodeStore<R> &node_store) : node_store_(node_store) {
    std::random_device rd;
    gen_ = std::mt19937(rd());
    dist01_ = std::uniform_real_distribution<double>(0.0, 1.0);
}

template<class R>
randomize::Randomizer<R>::Randomizer(algebra::NodeStore<R> &node_store, int seed) : node_store_(node_store) {
    gen_ = std::mt19937(seed);
    dist01_ = std::uniform_real_distribution<double>(0.0, 1.0);
}

template<class R>
std::string randomize::Randomizer<R>::to_random_string(const algebra::Node<R> &n) {
    switch (n.get_type()) {
        case algebra::NodeType::POL: return std::string("f(") + 
            to_random_string(*node_store_.get_polynode(n.get_polynode_hash())) + std::string(")");
        case algebra::NodeType::VAR: return std::string("x") + std::to_string(n.get_var());
    }
    return "";
}

template<class R>
std::string randomize::Randomizer<R>::to_random_string(const algebra::Mononode<R> &m) {
    if (m.get_degree() == 0) return "";

    std::vector<algebra::NodeHash> factors;
    factors.reserve(m.get_degree());

    for (const std::pair<const algebra::NodeHash, int> &f : m) {
        factors.insert(factors.end(), f.second, f.first);
    }

    std::random_device rd;
    std::default_random_engine generator(rd());

    std::shuffle(factors.begin(), factors.end(), generator);

    // Joins strings by a space
    std::string res = to_random_string(*node_store_.get_node(factors.front()));
    for (auto it = ++factors.begin(); it != factors.end(); it++) {
        res += " ";
        res += to_random_string(*node_store_.get_node(*it));
    }
    return res;
}

template<class R>
R randomize::Randomizer<R>::add_noise(const R& x) {
    double d = dist01_(gen_) * double(x);
    return R(d);
}

constexpr double DROPOUT = 0.75;
constexpr double VARIATION = 0.5;
constexpr double SWITCH = 0.3;

template<>
mpq_class randomize::Randomizer<mpq_class>::add_noise(const mpq_class& x) {
    double q = (1.0 + VARIATION * dist01_(gen_)) * x.get_d();

    double dropout = DROPOUT;
    if (x.get_den().get_d() > 1.0) dropout /= std::log2(x.get_den().get_d() + 1.0);

    int best_num = 0, best_den = 0;
    double best_dist = 2.0;
    for (int den = 1;; den++) {
        int num = std::round(den * q);
        double dist = std::abs(double(num) / den - q);
        if (dist < best_dist) {
            best_dist = dist;
            best_num = num;
            best_den = den;
        }

        if (dist01_(gen_) < dropout) break;
    }

    return mpq_class(best_num * (dist01_(gen_) < SWITCH ? -1 : 1), best_den);
}

template<class R>
std::string randomize::Randomizer<R>::to_random_string(const algebra::Polynode<R> &p, bool noisy) {
    if (p.begin() == p.end()) return "0";

    std::vector<std::pair<algebra::MononodeHash, R>> summands(p.begin(), p.end());

    if (noisy) {
        for (auto it = summands.begin(); it != summands.end();) {
            it->second = add_noise(it->second);
            if (it->second == 0) it = summands.erase(it);
            else it++;
        }
    }

    std::shuffle(summands.begin(), summands.end(), gen_);

    // Joins strings by +
    // Not pretty 
    R coeff = summands.begin()->second;
    R a_coeff = abs(coeff);
    const algebra::Mononode<R>* mono = node_store_.get_mononode(summands.begin()->first);

    std::string res = 
        (*mono == *node_store_.one_m()) ?
            ((coeff > 0 ? "" : "-") + algebra::R_to_string(a_coeff))
        :
            ((coeff > 0 ? "" : "-") + (a_coeff == 1 ? "" : algebra::R_to_string(a_coeff) + " ")
             + to_random_string(*mono));

    for (auto it = ++summands.begin(); it != summands.end(); it++) {
        coeff = it->second;
        a_coeff = abs(coeff);

        mono = node_store_.get_mononode(it->first);
        
        if (*mono == *node_store_.one_m()) {
            res += ((coeff > 0 ? " + " : " - ") + algebra::R_to_string(a_coeff));
        } else {
            res += (coeff > 0 ? " + " : " - ")
                + (a_coeff == 1 ? "" : algebra::R_to_string(a_coeff) + " ")
                + to_random_string(*mono); 
        }
    }
    return res;
}

//template class randomize::Randomizer<int>;
template class randomize::Randomizer<mpq_class>;

