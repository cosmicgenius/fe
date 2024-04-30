// randomize.hpp
#ifndef RANDOMIZE_HPP_
#define RANDOMIZE_HPP_

#include "algebra.hpp"

namespace randomize {

template<class R>
std::string to_random_string(const algebra::Node<R>& n, algebra::NodeStore<R> &node_store);

template<class R>
std::string to_random_string(const algebra::Mononode<R>& m, algebra::NodeStore<R> &node_store);

template<class R>
std::string to_random_string(const algebra::Polynode<R>& p, algebra::NodeStore<R> &node_store);
};

#endif
