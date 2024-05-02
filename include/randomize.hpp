// randomize.hpp
#ifndef RANDOMIZE_HPP_
#define RANDOMIZE_HPP_

#include "algebra.hpp"
#include<random>

namespace randomize {

template<class R>
class Randomizer {
private:
    algebra::NodeStore<R> &node_store_;

    std::mt19937 gen_;
    // Uniformly random on [0, 1)
    std::uniform_real_distribution<double> dist01_;

    // Adds noise to some x
    R add_noise(const R &x);

public:
    Randomizer(algebra::NodeStore<R> &node_store);
    Randomizer(algebra::NodeStore<R> &node_store, int seed);
    std::string to_random_string(const algebra::Node<R>& n);
    std::string to_random_string(const algebra::Mononode<R>& m);
    std::string to_random_string(const algebra::Polynode<R>& p, bool noisy = false);

};
};

#endif
