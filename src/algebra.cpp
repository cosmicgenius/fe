#include "../include/algebra.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <numeric>
#include <map>

#include <gmpxx.h>


/*
 * Util
 */

template<class T>
T pow(T n, int e) {
    T res = 1;
    while (e) {
        if (e & 1) res *= n;
        n *= n;
        e >>= 1;
    }
    return res;
}

// See https://stackoverflow.com/a/12996028
uint64_t fast_hash(uint64_t conj, uint64_t n) {
    uint64_t x = n ^ conj;
    x = (x ^ (x >> 30)) * (0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * (0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x ^ conj;
}

/*
 * Node Stats
 */
algebra::NodeStats::NodeStats(const int weight, const int nested_weight, const int depth, const int length_approx) 
    : weight(weight), nested_weight(nested_weight), depth(depth), length_approx(length_approx) {}

algebra::NodeStats& algebra::NodeStats::add_node(const NodeStats& rhs, int exp) {
    weight += rhs.weight * exp;
    nested_weight += rhs.nested_weight * exp;
    depth = std::max(depth, rhs.depth);
    length_approx += rhs.length_approx * exp;

    return *this;
}
algebra::NodeStats& algebra::NodeStats::add_mononode(const NodeStats& rhs, bool is_one) {
    weight += rhs.weight;
    nested_weight += rhs.nested_weight;
    depth = std::max(depth, rhs.depth);
    length_approx += rhs.length_approx + !is_one + 1; // for the operator

    return *this;
}

std::ostream& algebra::operator<<(std::ostream& os, const algebra::NodeStats& s) {
    return os << "w=" << s.weight << ",nw=" << s.nested_weight << ",d=" << s.depth << ",la=" << s.length_approx;
}

inline algebra::NodeStats from_polynode_stats(const algebra::NodeStats& stats) {
    return algebra::NodeStats(
        stats.weight * stats.weight, 
        stats.weight, 
        stats.depth + 1, 
        stats.length_approx + 3 // for 'f(' and ')'
    );
}

/*
 * Node Base
 */

template<class Hash>
algebra::NodeBase<Hash>::NodeBase(const Hash hash, const NodeStats node_stats) 
    : hash(hash), stats(node_stats) {}

template<class Hash>
bool algebra::NodeBase<Hash>::operator==(const NodeBase<Hash>& rhs) const
    { return hash == rhs.hash; }

template<class Hash>
bool algebra::NodeBase<Hash>::operator!=(const NodeBase<Hash>& rhs) const
    { return hash != rhs.hash; }

/*
 * NodeStore
 */

// All of this probably uses 2x as many 
// unordered_map queries as necessary, but i think 
// that's ok (probably not too bad)
//
// TODO: Am i using rvalue references right?

template<class R>
algebra::NodeStore<R>::NodeStore(const size_t seed) : conj_((seed ^ 0xab50cbf18725d1d1) * 0x80be920c700dedc1) {}

template<class R>
size_t algebra::NodeStore<R>::hash(const size_t n) const {
    return fast_hash(conj_, n);
}

template<class R>
const algebra::Node<R>* algebra::NodeStore<R>::get_node(const NodeHash hash) const {
    auto it = nodes_.find(hash);
    if (it == nodes_.end()) return nullptr;
    return &it->second;
}

template<class R>
const algebra::Mononode<R>* algebra::NodeStore<R>::get_mononode(const MononodeHash hash) const {
    auto it = mononodes_.find(hash);
    if (it == mononodes_.end()) return nullptr;
    return &it->second;
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::get_polynode(const PolynodeHash hash) const {
    auto it = polynodes_.find(hash);
    if (it == polynodes_.end()) return nullptr;
    return &it->second;
}

template<class R>
const algebra::Node<R>* algebra::NodeStore<R>::node(const PolynodeHash pol) {
    Node<R> node(pol, *this);
    return insert_node(std::move(node));
}

template<class R>
const algebra::Node<R>* algebra::NodeStore<R>::node(const Idx var) {
    Node<R> node(var, *this);
    return insert_node(std::move(node));
}

template<class R>
const algebra::Mononode<R>* algebra::NodeStore<R>::mononode(const std::unordered_map<NodeHash, int>& factors) {
    Mononode<R> mononode(factors, *this);
    return insert_mononode(std::move(mononode));
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::polynode(const std::vector<std::pair<MononodeHash, R>>& summands) {
    Polynode<R> polynode(summands, *this);

    return insert_polynode(std::move(polynode));
}

template<class R>
const algebra::Mononode<R>* algebra::NodeStore<R>::one_m() {
    return mononode({});
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::zero_p() {
    return polynode({});
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::one_p() {
    return polynode({{one_m()->hash, 1}});
}

template<class R>
const algebra::Node<R>* algebra::NodeStore<R>::insert_node(Node<R>&& node) {
    NodeHash hash = node.hash;

    // Does not already exist
    if (nodes_.find(hash) == nodes_.end()) {
        nodes_.insert({ hash, std::move(node) });
    }
    return &nodes_.at(hash);
}

template<class R>
const algebra::Mononode<R>* algebra::NodeStore<R>::insert_mononode(Mononode<R>&& mononode) {
    MononodeHash hash = mononode.hash;

    // Does not already exist
    if (mononodes_.find(hash) == mononodes_.end()) 
        mononodes_.insert({ hash, std::move(mononode) });
    return &mononodes_.at(hash);
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::insert_polynode(Polynode<R>&& polynode) {
    PolynodeHash hash = polynode.hash;

    // Does not already exist
    if (polynodes_.find(hash) == polynodes_.end()) 
        polynodes_.insert({ hash, std::move(polynode) });
    return &polynodes_.at(hash);
}

template<class R>
size_t algebra::NodeStore<R>::get_node_store_size() const 
    { return nodes_.size(); }

template<class R>
size_t algebra::NodeStore<R>::get_mononode_store_size() const 
    { return mononodes_.size(); }

template<class R>
size_t algebra::NodeStore<R>::get_polynode_store_size() const 
    { return polynodes_.size(); }

template<class R>
void algebra::NodeStore<R>::dump() const {
    std::vector<NodeHash> node_keys;
    node_keys.reserve(nodes_.size());

    for (const std::pair<const NodeHash, Node<R>>& node : nodes_) node_keys.push_back(node.first);
    std::sort(node_keys.begin(), node_keys.end(), 
            [this] (const NodeHash lhs, const NodeHash rhs) { return node_cmp(lhs, rhs) < 0;});

    std::cout << "Nodes:\n";
    for (const NodeHash nh : node_keys) {
        const Node<R>& node = nodes_.at(nh);
        std::cout << node.to_string() << " " << nh << " " << node.stats << "\n";
    }
    std::cout << std::flush;

    std::vector<MononodeHash> mononode_keys;
    mononode_keys.reserve(mononodes_.size());

    for (const std::pair<const MononodeHash, Mononode<R>>& mononode : mononodes_) mononode_keys.push_back(mononode.first);
    std::sort(mononode_keys.begin(), mononode_keys.end(),
            [this] (const MononodeHash lhs, const MononodeHash rhs) { return mononode_cmp(lhs, rhs) < 0;});

    std::cout << "Mononodes:\n";
    for (const MononodeHash mh : mononode_keys) {
        const Mononode<R>& mononode = mononodes_.at(mh);
        std::cout << mononode.to_string() << " " << mh << " " << mononode.stats << " " << mononode.get_degree() << "\n";
    }
    std::cout << std::flush;

    std::cout << "Polynodes:\n";
    for (const std::pair<const PolynodeHash, Polynode<R>>& polynode : polynodes_) {
        std::cout << polynode.second.to_string() << " " << polynode.first << " " << polynode.second.stats << "\n";
    }
    std::cout << std::flush;
}

// Puts f(polynodes) first (smaller) to allow for elimination in mononode order
// Inside f(polynodes), it is weight order
// Inside variables, it is lex
template<class R>
int algebra::NodeStore<R>::node_cmp(const NodeHash lhs, const NodeHash rhs) const {
    const algebra::Node<R>* const lhs_ptr = get_node(lhs), *rhs_ptr = get_node(rhs);

    if (lhs_ptr->type_ != rhs_ptr->type_) return lhs_ptr->type_ == NodeType::POL ? -1 : 1;

    if (lhs_ptr->type_ == NodeType::POL) {
        if (lhs_ptr->stats.weight != rhs_ptr->stats.weight) return rhs_ptr->stats.weight - lhs_ptr->stats.weight;
        
        // They are both f(polynode), with the same weight, arbitrarily tiebreak
        return (lhs == rhs) ? 0 : (lhs < rhs ? -1 : 1);
    } 
    // They are both variables
    return lhs_ptr->var_ - rhs_ptr->var_;
}

/*
 * Elimination order on the nodes that are f(polynode) then variables
 * grevlex inside
 *
 * Reversed in order to put the leading monomial in summands.front()
 */ 

template<class R>
int algebra::NodeStore<R>::mononode_cmp(const MononodeHash lhs, const MononodeHash rhs) const {
    const Mononode<R>* const lhs_ptr = get_mononode(lhs), *rhs_ptr = get_mononode(rhs);
    //std::cout << "Comparing: " << lhs << " " << lhs_ptr->to_string() << " " << rhs << " " << rhs_ptr->to_string() << "\n";
    if (lhs_ptr->pol_degree_ != rhs_ptr->pol_degree_) 
        return -(lhs_ptr->pol_degree_ - rhs_ptr->pol_degree_); // Reverse

    bool on_vars = false;
    auto lhs_it = lhs_ptr->begin(), rhs_it = rhs_ptr->begin(),
         lhs_end = lhs_ptr->end(), rhs_end = rhs_ptr->end();

    for (; lhs_it != lhs_end && rhs_it != rhs_end; lhs_it++, rhs_it++) {
        // The first time we run out of polynodes, check for var degree
        //
        // Note that if nothing has returned before this point, then lhs and rhs must have
        // the same f(polynode) parts, so we can simply check for only one of them
        if (!on_vars && get_node(lhs_it->first)->type_ == NodeType::VAR) {
            if (lhs_ptr->var_degree_ != rhs_ptr->var_degree_) 
                return -(lhs_ptr->var_degree_ - rhs_ptr->var_degree_); // Reversed
            on_vars = true;
        }

        if (lhs_it->first != rhs_it->first) 
            return -node_cmp(lhs_it->first, rhs_it->first); // Reversed
        if (lhs_it->second != rhs_it->second) 
            return lhs_it->second < rhs_it->second ? -1 : 1; // Two reverses cancel out
    }

    // We might never hit the on_vars if statement, so we need to check here 
    return -(lhs_ptr->var_degree_ - rhs_ptr->var_degree_); // Reverse
}


/*
 * Node
 */

template<class R>
algebra::Node<R>::Node(const PolynodeHash pol, NodeStore<R> &node_store) :
    NodeBase(node_store.hash(pol), from_polynode_stats(node_store.get_polynode(pol)->stats)),
    type_(NodeType::POL), pol_(pol), var_(0), node_store_(node_store) {}

template<class R>
algebra::Node<R>::Node(const Idx var, NodeStore<R> &node_store) : 
    NodeBase(node_store.hash(var), NodeStats(2, 0, 0, 2)), 
    type_(NodeType::VAR), pol_(0), var_(var), node_store_(node_store) {}

template<class R>
std::string algebra::Node<R>::to_string() const {
    switch (type_) {
        case algebra::NodeType::POL: return std::string("f(") + 
            node_store_.get_polynode(pol_)->to_string() + std::string(")");
        case algebra::NodeType::VAR: return std::string("x") + std::to_string(var_);
    }
    return "";
}

template<class R>
algebra::NodeType algebra::Node<R>::get_type() const { return type_; }

template<class R>
algebra::PolynodeHash algebra::Node<R>::get_polynode_hash() const { return pol_; }

template<class R>
algebra::Idx algebra::Node<R>::get_var() const { return var_; }

/*
 * Mononode
 */
template<class R>
std::map<algebra::NodeHash, int, std::function<bool(const algebra::NodeHash, const algebra::NodeHash)>> 
    algebra::Mononode<R>::clean_factors(const std::unordered_map<NodeHash, int> &factors, NodeStore<R> &node_store) {

    std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>> res(
        [&node_store] (const NodeHash lhs, const NodeHash rhs)
            { return node_store.node_cmp(lhs, rhs) < 0; }
        );
    for (const std::pair<const NodeHash, int>& cur : factors) {
        if(cur.second > 0) res[cur.first] = cur.second;
    }

    return res;
}

template<class R>
algebra::Mononode<R>::Mononode(
        const std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>>&& factors, 
        NodeStore<R> &node_store) : 
    NodeBase(
         std::accumulate(factors.begin(), factors.end(), MononodeHash(1),
            [](MononodeHash hash, const std::pair<NodeHash, int>& cur) { 
                return hash + cur.first * NodeHash(cur.second);
            }), 
         factors.size() == 0 ? NodeStats(0, 0, 0, 1) :
         std::accumulate(factors.begin(), factors.end(), NodeStats(), 
            [&node_store](NodeStats &stats, const std::pair<NodeHash, int>& cur) { 
                return stats.add_node(node_store.get_node(cur.first)->stats, cur.second);
            })
        ), 
    factors_(std::move(factors)),
    var_degree_(
         std::accumulate(factors.begin(), factors.end(), 0,
            [&node_store = node_store](int deg, const std::pair<const NodeHash, int>& cur) { 
                return deg + (node_store.get_node(cur.first)->get_type() == NodeType::VAR ? cur.second : 0);
            })
        ),
    pol_degree_(
         std::accumulate(factors.begin(), factors.end(), 0,
            [&node_store = node_store](int deg, const std::pair<const NodeHash, int>& cur) { 
                return deg + (node_store.get_node(cur.first)->get_type() == NodeType::POL ? cur.second : 0);
            })
        ),
    node_store_(node_store) {}

template<class R>
algebra::Mononode<R>::Mononode(const std::unordered_map<NodeHash, int>& factors, 
        NodeStore<R> &node_store) : 
    NodeBase(
         std::accumulate(factors.begin(), factors.end(), MononodeHash(1),
            [](MononodeHash hash, const std::pair<NodeHash, int>& cur) { 
                    return hash + cur.first * NodeHash(cur.second);
                }), 
         factors.size() == 0 ? NodeStats(0, 0, 0, 1) :
         std::accumulate(factors.begin(), factors.end(), NodeStats(), 
            [&node_store](NodeStats &stats, const std::pair<NodeHash, int>& cur) { 
                return stats.add_node(node_store.get_node(cur.first)->stats, cur.second);
            })
        ), 
    factors_(std::move(clean_factors(factors, node_store))),
    var_degree_(
         std::accumulate(factors.begin(), factors.end(), 0,
            [&node_store = node_store](int deg, const std::pair<NodeHash, int>& cur) { 
                return deg + (node_store.get_node(cur.first)->get_type() == NodeType::VAR ? cur.second : 0);
            })
        ),
    pol_degree_(
         std::accumulate(factors.begin(), factors.end(), 0,
            [&node_store = node_store](int deg, const std::pair<NodeHash, int>& cur) { 
                return deg + (node_store.get_node(cur.first)->get_type() == NodeType::POL ? cur.second : 0);
            })
        ),
    node_store_(node_store) {}

template<class R>
std::string algebra::Mononode<R>::to_string() const {
    if (factors_.empty()) return "";

    // Joins strings by a space
    std::string res = node_store_.get_node(begin()->first)->to_string();
    for (auto it = begin(); it != end(); it++) {
        for (int rep = (it == begin()); rep < it->second; rep++) { 
            res += " ";
            res += node_store_.get_node(it->first)->to_string();
        }
    }
    return res;
}

template<class R>
const algebra::Mononode<R>* algebra::Mononode<R>::operator*(const Mononode<R>& rhs) const {
    // Very inexpensive to compute the hash first,
    // test if it is a repeat, and if not, compute the whole thing
    MononodeHash product_hash = hash * rhs.hash;
    const Mononode<R>* cached = node_store_.get_mononode(product_hash);

    if (cached != nullptr) return cached;

    std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>>
        combined_factors(factors_.begin(), factors_.end(), 
        [&node_store = node_store_] (const NodeHash lhs, const NodeHash rhs) 
            { return node_store.node_cmp(lhs, rhs) < 0; }
        );
    for (const std::pair<const NodeHash, int> &rhs_entry : rhs.factors_) {
        combined_factors[rhs_entry.first] += rhs_entry.second;
    }

    return node_store_.insert_mononode(Mononode<R>(std::move(combined_factors), node_store_));
}

template<class R>
const algebra::Mononode<R>* algebra::Mononode<R>::lcm(const algebra::Mononode<R>& rhs) const {
    std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>>
        lcm([&node_store = node_store_] (const NodeHash lhs, const NodeHash rhs) 
            { return node_store.node_cmp(lhs, rhs) < 0; }
        );
    
    for (const std::pair<const NodeHash, int> &lhs_entry : factors_) {
        const NodeHash p = lhs_entry.first;
        const int lhs_exp = lhs_entry.second;
        if (rhs.factors_.find(p) == rhs.factors_.end()) {
            lcm[p] = lhs_exp;
        } else {
            lcm[p] = std::max(rhs.factors_.at(p), lhs_exp);
        }
    }

    for (const std::pair<const NodeHash, int> &rhs_entry : rhs.factors_) {
        const NodeHash p = rhs_entry.first;
        const int rhs_exp = rhs_entry.second;
        if (factors_.find(p) == factors_.end()) {
            lcm[p] = rhs_exp;
        } 
        // Overlaps are already handled by the above
    }

    return node_store_.insert_mononode(Mononode<R>(std::move(lcm), node_store_));
}

template<class R>
std::pair<const algebra::Mononode<R>*, const algebra::Mononode<R>*> 
algebra::Mononode<R>::symmetric_q(const Mononode<R>& rhs) const {
    std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>>
        q_lhs([&node_store = node_store_] (const NodeHash lhs, const NodeHash rhs) 
            { return node_store.node_cmp(lhs, rhs) < 0; }
        ), 
        q_rhs([&node_store = node_store_] (const NodeHash lhs, const NodeHash rhs) 
            { return node_store.node_cmp(lhs, rhs) < 0; }
        );
    
    for (const std::pair<const NodeHash, int> &lhs_entry : factors_) {
        const NodeHash p = lhs_entry.first;
        const int lhs_exp = lhs_entry.second;
        if (rhs.factors_.find(p) == rhs.factors_.end()) {
            q_rhs[p] = lhs_exp;
        } else {
            const int rhs_exp = rhs.factors_.at(p);

            if (rhs_exp < lhs_exp) {
                q_rhs[p] = lhs_exp - rhs_exp;
            } else if (rhs_exp > lhs_exp) {
                q_lhs[p] = rhs_exp - lhs_exp;
            }
        }
    }

    for (const std::pair<const NodeHash, int> &rhs_entry : rhs.factors_) {
        const NodeHash p = rhs_entry.first;
        const int rhs_exp = rhs_entry.second;
        if (factors_.find(p) == factors_.end()) {
            q_lhs[p] = rhs_exp;
        } 
        // Overlaps are already handled by the above
    }

    return {node_store_.insert_mononode(Mononode<R>(std::move(q_lhs), node_store_)),
            node_store_.insert_mononode(Mononode<R>(std::move(q_rhs), node_store_))};
}

template<class R>
bool algebra::Mononode<R>::divisible(const Mononode<R>& rhs) const {
    for (const std::pair<const NodeHash, int> &rhs_entry : rhs.factors_) {
        const NodeHash p = rhs_entry.first;
        const int rhs_exp = rhs_entry.second;
        auto it = factors_.find(p);
        if (it == factors_.end()) {
            return false;
        } else if (it->second < rhs_exp) {
            return false;
        }
    }
    return true;
}

template<class R>
std::map<algebra::NodeHash, int, std::function<bool(const algebra::NodeHash, const algebra::NodeHash)>>::const_iterator
    algebra::Mononode<R>::begin() const { return factors_.begin(); }

template<class R>
std::map<algebra::NodeHash, int, std::function<bool(const algebra::NodeHash, const algebra::NodeHash)>>::const_iterator
    algebra::Mononode<R>::end() const { return factors_.end(); }

template<class R>
int algebra::Mononode<R>::get_degree() const { return var_degree_ + pol_degree_; }

/*
 * Polynode
 */

template<class R>
algebra::PolynodeHash to_polynode_hash(const R &r) {
    return algebra::PolynodeHash(r);
}

template<>
algebra::PolynodeHash to_polynode_hash(const mpq_class &r) {
    // Quick and dirty rational hash
    double d = r.get_d();
    uint64_t* ptr = (uint64_t*) &d;
    return fast_hash(0x93c467e37db0c7a4, *ptr);
}

template<class R>
std::string algebra::R_to_string(const R &r) {
    return std::to_string(r);
}

template<>
std::string algebra::R_to_string(const mpq_class &r) {
    return r.get_str();
}

template<class R>
std::vector<std::pair<algebra::MononodeHash, R>> algebra::Polynode<R>::clean_summands(
        const std::vector<std::pair<MononodeHash, R>> &summands, NodeStore<R> &node_store) {

    std::vector<std::pair<algebra::MononodeHash, R>> res;
    res.reserve(summands.size());

    for (const std::pair<MononodeHash, R>& summand : summands) {
        if (summand.second != 0) res.emplace_back(std::move(summand));
    }

    std::sort(res.begin(), res.end(), 
            [&node_store](const std::pair<MononodeHash, R>& lhs, const std::pair<MononodeHash, R>& rhs) {
                return node_store.mononode_cmp(lhs.first, rhs.first) < 0;
            }
        );
    return res;
}

template<class R>
algebra::Polynode<R>::Polynode(const std::vector<std::pair<MononodeHash, R>>&& summands, 
        NodeStore<R> &node_store) : 
    NodeBase(std::accumulate(summands.begin(), summands.end(), PolynodeHash(0), 
                [&node_store] (const PolynodeHash hash, const std::pair<MononodeHash, R>& cur) { 
                    // Must combine with a commutative operation in order to create same hash as unsorted
                    return hash ^ node_store.hash(PolynodeHash(cur.first) + to_polynode_hash(cur.second));
                }), 
             std::accumulate(summands.begin(), summands.end(), NodeStats(), 
                [&node_store] (NodeStats &stats, const std::pair<MononodeHash, R>& cur) { 
                    return stats.add_mononode(node_store.get_mononode(cur.first)->stats, abs(cur.second) == 1);
                })
            ),
    summands_(std::move(summands)), 
    node_store_(node_store) {}

template<class R>
algebra::Polynode<R>::Polynode(const std::vector<std::pair<MononodeHash, R>>& summands, 
        NodeStore<R> &node_store) : 
    NodeBase(std::accumulate(summands.begin(), summands.end(), PolynodeHash(0), 
                [&node_store] (const PolynodeHash hash, const std::pair<MononodeHash, R>& cur) { 
                    // Must combine with a commutative operation in order to create same hash as unsorted
                    return hash ^ node_store.hash(PolynodeHash(cur.first) + to_polynode_hash(cur.second));
                }), 
             std::accumulate(summands.begin(), summands.end(), NodeStats(), 
                [&node_store] (NodeStats &stats, const std::pair<MononodeHash, R>& cur) { 
                    return stats.add_mononode(node_store.get_mononode(cur.first)->stats, abs(cur.second) == 1);
                })
            ),
    summands_(std::move(clean_summands(summands, node_store))), 
    node_store_(node_store) {}

template<class R>
std::string algebra::Polynode<R>::to_string() const {
    if (summands_.empty()) return "0";

    // Joins strings by +
    // Not pretty 
    R coeff = summands_.begin()->second;
    R a_coeff = abs(coeff);
    const Mononode<R>* mono = node_store_.get_mononode(summands_.begin()->first);

    std::string res = 
        (*mono == *node_store_.one_m()) ?
            ((coeff > 0 ? "" : "-") + R_to_string(a_coeff))
        :
            ((coeff > 0 ? "" : "-") + (a_coeff == 1 ? "" : R_to_string(a_coeff) + " ") + mono->to_string());

    for (auto it = ++summands_.begin(); it != summands_.end(); it++) {
        coeff = it->second;
        a_coeff = abs(coeff);

        mono = node_store_.get_mononode(it->first);
        
        if (*mono == *node_store_.one_m()) {
            res += ((coeff > 0 ? " + " : " - ") + R_to_string(a_coeff));
        } else {
            res += (coeff > 0 ? " + " : " - ")
                + (a_coeff == 1 ? "" : R_to_string(a_coeff) + " ")
                + mono->to_string(); 
        }
    }
    return res;
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator-() const {
    const Polynode<R>* cached = node_store_.get_polynode(-hash);

    if (cached != nullptr) return cached;

    std::vector<std::pair<MononodeHash, R>> neg_summands(summands_);
    for (std::pair<MononodeHash, R> &neg_entry : neg_summands) {
        neg_entry.second *= -1;
    }

    return node_store_.insert_polynode(Polynode<R>(std::move(neg_summands), node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator+(const Polynode<R>& rhs) const {
    std::vector<std::pair<MononodeHash, R>> combined_summands;
    combined_summands.reserve(summands_.size() + rhs.summands_.size());

    // Merge, assuming both are sorted
    for (auto itl = begin(), itr = rhs.begin(); itl != end() || itr != rhs.end();) {
        if (itl != end() && (itr == rhs.end() 
                    || node_store_.mononode_cmp(itl->first, itr->first) < 0)) {
            // Append *itl
            if (combined_summands.empty() || combined_summands.back().first != itl->first) {
                combined_summands.push_back(*itl);
            } else {
                combined_summands.back().second += itl->second;

                // Destroy any 0's that are created
                if (combined_summands.back().second == 0) combined_summands.pop_back();
            }
            itl++;
        } else {
            // Append *itr
            if (combined_summands.empty() || combined_summands.back().first != itr->first) {
                combined_summands.push_back(*itr);
            } else {
                combined_summands.back().second += itr->second;
                if (combined_summands.back().second == 0) combined_summands.pop_back();
            }
            itr++;
        }
    }
                
    return node_store_.insert_polynode(Polynode<R>(std::move(combined_summands), node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator-(const Polynode<R>& rhs) const {
    return *this + *(-rhs);
}

// Expensive function, but this won't be called too often
template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator*(const Polynode<R>& rhs) const {
    // The idea is that once we generate the cartesian product, we must
    //  1) combine like monomials
    //  2) sort monomials
    // We can do both of these with a heap! (here implemented with std::map)
    std::map<MononodeHash, R, std::function<bool(const MononodeHash, const MononodeHash)>> 
        combined_summands([&node_store = node_store_] (const MononodeHash lhs, const MononodeHash rhs) {
                    return node_store.mononode_cmp(lhs, rhs) < 0;
                });

    for (const std::pair<MononodeHash, R> &lhs_entry : summands_) {
        for (const std::pair<MononodeHash, R> &rhs_entry : rhs.summands_) {
            const Mononode<R>* prod = *node_store_.get_mononode(lhs_entry.first) 
                * *node_store_.get_mononode(rhs_entry.first);
            combined_summands[prod->hash] += lhs_entry.second * rhs_entry.second;
        }
    }

    std::vector<std::pair<MononodeHash, R>> combined_summands_vec;
    combined_summands_vec.reserve(combined_summands.size());

    for (auto it = combined_summands.begin(); it != combined_summands.end(); it++) {
        if (it->second != 0) combined_summands_vec.emplace_back(std::move(*it));
    }

    return node_store_.insert_polynode(
            Polynode<R>(std::move(combined_summands_vec), node_store_));
}

// By the definition of mononomial order, we do not have to reorder
template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::scale(const Mononode<R>& m, const R c) const {
    std::vector<std::pair<MononodeHash, R>> new_summands;
    new_summands.reserve(summands_.size());

    for (const std::pair<MononodeHash, R> &entry : summands_) {
        new_summands.emplace_back((*node_store_.get_mononode(entry.first) * m)->hash, entry.second * c);
    }

    return node_store_.insert_polynode(
            Polynode<R>(std::move(new_summands), node_store_));
}

template<class R>
const algebra::Mononode<R>* algebra::Polynode<R>::leading_m() const {
    return node_store_.get_mononode(summands_.front().first);
}

template<class R>
const R algebra::Polynode<R>::leading_c() const {
    return summands_.front().second;
}

template<class R>
typename std::vector<std::pair<algebra::MononodeHash, R>>::const_iterator algebra::Polynode<R>::begin() 
    const { return summands_.begin(); }

template<class R>
typename std::vector<std::pair<algebra::MononodeHash, R>>::const_iterator algebra::Polynode<R>::end() 
    const { return summands_.end(); }

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::sub(const Idx var, const Polynode<R>& val) const {
    // It is likely not a repeat, so we do not compute the hash first
    const Polynode<R>* sum = node_store_.zero_p();
    for (const std::pair<MononodeHash, R> &entry : summands_) {
        const Polynode<R>* term = node_store_.one_p();
        std::unordered_map<NodeHash, int> non_sub_factors{};

        // Pretty costly, but we'll just multiply everything together for now
        for (const std::pair<const NodeHash, int> &factor : *node_store_.get_mononode(entry.first)) { 
            const Node<R>* nptr = node_store_.get_node(factor.first);
            switch (nptr->type_) {
                // If it is the correct variable, substitute
                case NodeType::VAR: {
                    if (nptr->var_ == var) {
                        for (int rep = 0; rep < factor.second; rep++) {
                            term = *term * val; // Slow, but it's probably ok because exponents are small
                        }
                    } else {
                        non_sub_factors[factor.first] += factor.second;
                    }
                    break;
                }
                // If it is f(polynode), then we need to recursively substitute inside the entire polynode
                case NodeType::POL: {
                    non_sub_factors[node_store_.node
                            (node_store_.get_polynode(
                                node_store_.get_node(factor.first)->pol_
                            )->sub(var, val)->hash
                        )->hash] += factor.second;
                    break;
                }
            }
        }
        term = *term * 
                *node_store_.polynode(
                    {{node_store_.mononode(std::move(non_sub_factors))->hash, entry.second}}
                );

        sum = *sum + *term;
    }
    return sum;
}

// We do not assume that the order remains after subbing for zeros
// TODO: can we get rid of this?
template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::subs_zero(const std::unordered_set<Idx>& vars) const {
    std::map<MononodeHash, R, std::function<bool(const MononodeHash, const MononodeHash)>> 
        new_summands([&node_store = node_store_] (const MononodeHash lhs, const MononodeHash rhs) {
                    return node_store.mononode_cmp(lhs, rhs) < 0;
                });

    for (const std::pair<MononodeHash, R> &entry : summands_) {
        // Is one of the summands zero? If yes, we can just early break
        bool is_zero = false;
        std::unordered_map<NodeHash, int> factors;
        factors.reserve(node_store_.get_mononode(entry.first)->factors_.size());

        for (const std::pair<const NodeHash, int> &factor : *node_store_.get_mononode(entry.first)) { 
            const Node<R>* nptr = node_store_.get_node(factor.first);
            switch (nptr->type_) {
                case NodeType::VAR: {
                    if (vars.find(nptr->var_) != vars.end()) {
                        is_zero = true;
                    } else {
                        factors[factor.first] += factor.second;
                    }
                    break;
                }
                // If it is f(polynode), then we need to recursively substitute inside the entire polynode
                case NodeType::POL: {
                    factors[node_store_.node
                            (node_store_.get_polynode(
                                node_store_.get_node(factor.first)->pol_
                            )->subs_zero(vars)->hash
                        )->hash] += factor.second;
                    break;
                }
            }
            if (is_zero) break;
        }
        if (is_zero) continue;

        new_summands[node_store_.mononode(std::move(factors))->hash] += entry.second;
    }
    std::vector<std::pair<MononodeHash, R>> new_summands_vec;
    new_summands_vec.reserve(new_summands.size());

    for (auto it = new_summands.begin(); it != new_summands.end(); it++) {
        if (it->second != 0) new_summands_vec.emplace_back(std::move(*it));
    }

    return node_store_.insert_polynode(Polynode<R>(std::move(new_summands_vec), node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::subs_var(const std::unordered_map<Idx, Idx>& replace) const {
    std::map<MononodeHash, R, std::function<bool(const MononodeHash, const MononodeHash)>> 
        new_summands([&node_store = node_store_] (const MononodeHash lhs, const MononodeHash rhs) {
                    return node_store.mononode_cmp(lhs, rhs) < 0;
                });

    for (const std::pair<MononodeHash, R> &entry : summands_) {
        std::unordered_map<NodeHash, int> factors;
        factors.reserve(node_store_.get_mononode(entry.first)->factors_.size());

        for (const std::pair<const NodeHash, int> &factor : *node_store_.get_mononode(entry.first)) { 
            const Node<R>* nptr = node_store_.get_node(factor.first);
            switch (nptr->type_) {
                case NodeType::VAR: {
                    auto it = replace.find(nptr->var_);
                    if (it != replace.end()) {
                        factors[node_store_.node(it->second)->hash] += factor.second;
                    } else {
                        factors[factor.first] += factor.second;
                    }
                    break;
                }
                // If it is f(polynode), then we need to recursively substitute inside the entire polynode
                case NodeType::POL: {
                    factors[node_store_.node
                            (node_store_.get_polynode(
                                node_store_.get_node(factor.first)->pol_
                            )->subs_var(replace)->hash
                        )->hash] += factor.second;
                    break;
                }

            }
        }
        new_summands[node_store_.mononode(std::move(factors))->hash] += entry.second;
    }
    std::vector<std::pair<MononodeHash, R>> new_summands_vec;
    new_summands_vec.reserve(new_summands.size());

    for (auto it = new_summands.begin(); it != new_summands.end(); it++) {
        if (it->second != 0) new_summands_vec.emplace_back(std::move(*it));
    }

    return node_store_.insert_polynode(Polynode<R>(std::move(new_summands_vec), node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::apply_func(const Polynode<R>& rhs) const {
    return node_store_.polynode({
            {node_store_.mononode({
                    {node_store_.node((*this + rhs)->hash)->hash, 1}
                })->hash, 1},
            {node_store_.mononode({ 
                    {node_store_.node(rhs.hash)->hash, 1}
                })->hash, -1}
        });
}

template class algebra::NodeBase<size_t>;

//template class algebra::NodeStore<int>;
//template class algebra::Node<int>;
//template class algebra::Mononode<int>;
//template class algebra::Polynode<int>;

template class algebra::NodeStore<mpq_class>;
template class algebra::Node<mpq_class>;
template class algebra::Mononode<mpq_class>;
template class algebra::Polynode<mpq_class>;
