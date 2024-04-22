#include "../include/algebra.hpp"

#include <cstdint>
#include <cstdio>
#include <numeric>

#include <iostream>

/*
 * Hashed class
 */

template<class Hash>
algebra::NodeBase<Hash>::NodeBase(const Hash hash, const int weight) 
    : hash(hash), weight(weight) {}

template<class Hash>
bool algebra::NodeBase<Hash>::operator==(const NodeBase<Hash>& rhs) const
    { return this->hash == rhs.hash; }

template<class Hash>
bool algebra::NodeBase<Hash>::operator!=(const NodeBase<Hash>& rhs) const
    { return this->hash != rhs.hash; }

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

// See https://stackoverflow.com/a/12996028
template<class R>
size_t algebra::NodeStore<R>::hash(const size_t n) const {
    uint64_t x = uint64_t(n) ^ this->conj_;
    x = (x ^ (x >> 30)) * (0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * (0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return size_t(x ^ this->conj_);
}

template<class R>
const algebra::Node<R>* algebra::NodeStore<R>::get_node(const NodeHash hash) const {
    auto it = this->nodes_.find(hash);
    if (it == this->nodes_.end()) return nullptr;
    return &it->second;
}

template<class R>
const algebra::Mononode<R>* algebra::NodeStore<R>::get_mononode(const MononodeHash hash) const {
    auto it = this->mononodes_.find(hash);
    if (it == this->mononodes_.end()) return nullptr;
    return &it->second;
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::get_polynode(const PolynodeHash hash) const {
    auto it = this->polynodes_.find(hash);
    if (it == this->polynodes_.end()) return nullptr;
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
const algebra::Mononode<R>* algebra::NodeStore<R>::mononode(const std::vector<NodeHash>&& factors) {
    Mononode<R> mononode(std::move(factors), *this);
    return insert_mononode(std::move(mononode));
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::polynode(const std::unordered_map<MononodeHash, R>&& summands) {
    Polynode<R> polynode(std::move(summands), *this);
    return insert_polynode(std::move(polynode));
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::zero() {
    return polynode({});
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::one() {
    return polynode({{mononode({})->hash, 1}});
}

template<class R>
const algebra::Node<R>* algebra::NodeStore<R>::insert_node(Node<R>&& node) {
    NodeHash hash = node.hash;

    // Does not already exist
    if (this->nodes_.find(hash) == this->nodes_.end()) {
        this->nodes_.insert({ hash, std::move(node) });
    }
    return &this->nodes_.at(hash);
}

template<class R>
const algebra::Mononode<R>* algebra::NodeStore<R>::insert_mononode(Mononode<R>&& mononode) {
    MononodeHash hash = mononode.hash;

    // Does not already exist
    if (this->mononodes_.find(hash) == this->mononodes_.end()) 
        this->mononodes_.insert({ hash, std::move(mononode) });
    return &this->mononodes_.at(hash);
}

template<class R>
const algebra::Polynode<R>* algebra::NodeStore<R>::insert_polynode(Polynode<R>&& polynode) {
    PolynodeHash hash = polynode.hash;

    // Does not already exist
    if (this->polynodes_.find(hash) == this->polynodes_.end()) 
        this->polynodes_.insert({ hash, std::move(polynode) });
    return &this->polynodes_.at(hash);
}

template<class R>
size_t algebra::NodeStore<R>::get_node_store_size() const 
    { return this->nodes_.size(); }

template<class R>
size_t algebra::NodeStore<R>::get_mononode_store_size() const 
    { return this->mononodes_.size(); }

template<class R>
size_t algebra::NodeStore<R>::get_polynode_store_size() const 
    { return this->polynodes_.size(); }

/*
 * Node
 */

inline int sq(const int n) {
    return n * n;
}

template<class R>
algebra::Node<R>::Node(const PolynodeHash pol, NodeStore<R> &node_store) :
    NodeBase(node_store.hash(pol), sq(node_store.get_polynode(pol)->weight)),
    type_(NodeType::POL), pol_(pol), var_(0), node_store_(node_store) {}

template<class R>
algebra::Node<R>::Node(const Idx var, NodeStore<R> &node_store) : 
    NodeBase(node_store.hash(var), 2), 
    type_(NodeType::VAR), pol_(0), var_(var), node_store_(node_store) {}

template<class R>
std::string algebra::Node<R>::to_string() const {
    switch (this->type_) {
        case algebra::NodeType::POL: return std::string("f(") + 
            this->node_store_.get_polynode(this->pol_)->to_string() + std::string(")");
        case algebra::NodeType::VAR: return std::string("x") + std::to_string(this->var_);
    }
    return "";
}

/*
 * Mononode
 */

template<class R>
algebra::Mononode<R>::Mononode(const std::vector<NodeHash>&& factors, 
        NodeStore<R> &node_store) : 
    NodeBase(std::accumulate(factors.begin(), factors.end(), 1, std::multiplies<NodeHash>()), 
             std::accumulate(factors.begin(), factors.end(), 0, 
                 [&node_store](int weight, NodeHash cur) { return weight + node_store.get_node(cur)->weight; } 
            )), 
    factors_(std::move(factors)), node_store_(node_store) {}

template<class R>
std::string algebra::Mononode<R>::to_string() const {
    if (this->factors_.empty()) return "";

    // Joins strings by a space
    std::string res = this->node_store_.get_node(this->factors_[0])->to_string();
    for (auto it = this->factors_.begin() + 1; it != this->factors_.end(); it++) {
        res += " ";
        res += this->node_store_.get_node(*it)->to_string();
    }
    return res;
}

template<class R>
const algebra::Mononode<R>* algebra::Mononode<R>::operator*(const Mononode<R>& rhs) const {
    MononodeHash product_hash = this->hash * rhs.hash;
    const Mononode<R>* cached = this->node_store_.get_mononode(product_hash);

    if (cached != nullptr) return cached;

    std::vector<NodeHash> combined_factors(this->factors_.begin(), this->factors_.end());
    combined_factors.insert(combined_factors.end(), rhs.factors_.begin(), rhs.factors_.end());

    return this->node_store_.insert_mononode(Mononode<R>(std::move(combined_factors), this->node_store_));
}

/*
 * Polynode
 */

template<class R>
algebra::Polynode<R>::Polynode(const std::unordered_map<MononodeHash, R>&& summands, 
        NodeStore<R> &node_store) : 
    NodeBase(std::accumulate(summands.begin(), summands.end(), 0, 
                [] (const PolynodeHash hash, const std::pair<MononodeHash, R>& cur) { 
                    return hash + PolynodeHash(cur.first) * PolynodeHash(cur.second);
                }), 
             std::accumulate(summands.begin(), summands.end(), 0, 
                [&node_store] (const PolynodeHash weight, const std::pair<MononodeHash, R>& cur) { 
                    return weight + node_store.get_mononode(cur.first)->weight;
                })
            ),
    summands_(std::move(summands)), node_store_(node_store) {}

template<class R>
std::string algebra::Polynode<R>::to_string() const {
    if (this->summands_.empty()) return "0";

    // Joins strings by +
    // Not pretty 
    R coeff = this->summands_.begin()->second;
    std::string res = (coeff == 1 ? "" : (coeff == -1 ? "-" : std::to_string(coeff) + " ")) +
        this->node_store_.get_mononode(this->summands_.begin()->first)->to_string();


    for (auto it = ++this->summands_.begin(); it != this->summands_.end(); it++) {
        R coeff = it->second;
        res += (coeff > 0 
                ? (" + " + (coeff == 1 ? "" : std::to_string(coeff) + " "))
                : (" - " + (coeff == -1 ? "" : std::to_string(-coeff) + " ")))
            + this->node_store_.get_mononode(it->first)->to_string();
    }
    return res;
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator-() const {
    const Polynode<R>* cached = this->node_store_.get_polynode(-this->hash);

    if (cached != nullptr) return cached;

    std::unordered_map<MononodeHash, R> neg_summands(this->summands_);
    for (const std::pair<MononodeHash, R> neg_entry : neg_summands) {
        neg_summands[neg_entry.first] = -neg_entry.second;
    }

    return this->node_store_.insert_polynode(Polynode<R>(std::move(neg_summands), this->node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator+(const Polynode<R>& rhs) const {
    // Very inexpensive to compute the hash first,
    // test if it is a repeat, and if not, compute the whole thing
    PolynodeHash sum_hash = this->hash + rhs.hash;
    const Polynode<R>* cached = this->node_store_.get_polynode(sum_hash);

    if (cached != nullptr) return cached;

    std::unordered_map<MononodeHash, R> combined_summands(this->summands_);
    for (const std::pair<MononodeHash, R> rhs_entry : rhs.summands_) {
        combined_summands[rhs_entry.first] += rhs_entry.second;
    }

    for (auto it = combined_summands.begin(); it != combined_summands.end();) {
        if (it->second == 0) it = combined_summands.erase(it);
        else it++;
    }
                
    return this->node_store_.insert_polynode(Polynode<R>(std::move(combined_summands), this->node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator-(const Polynode<R>& rhs) const {
    PolynodeHash sum_hash = this->hash - rhs.hash;
    const Polynode<R>* cached = this->node_store_.get_polynode(sum_hash);

    if (cached != nullptr) return cached;

    std::unordered_map<MononodeHash, R> combined_summands(this->summands_);
    for (const std::pair<MononodeHash, R> rhs_entry : rhs.summands_) {
        combined_summands[rhs_entry.first] -= rhs_entry.second;
    }

    for (auto it = combined_summands.begin(); it != combined_summands.end();) {
        if (it->second == 0) it = combined_summands.erase(it);
        else it++;
    }
                
    return this->node_store_.insert_polynode(Polynode<R>(std::move(combined_summands), this->node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator*(const Polynode<R>& rhs) const {
    PolynodeHash product_hash = this->hash * rhs.hash;
    const Polynode<R>* cached = this->node_store_.get_polynode(product_hash);

    if (cached != nullptr) return cached;

    std::unordered_map<MononodeHash, R> combined_summands;
    for (const std::pair<MononodeHash, R> lhs_entry : this->summands_) {
        for (const std::pair<MononodeHash, R> rhs_entry : rhs.summands_) {
            const Mononode<R>* prod = *this->node_store_.get_mononode(lhs_entry.first) * *this->node_store_.get_mononode(rhs_entry.first);
            combined_summands[prod->hash] += lhs_entry.second * rhs_entry.second;
        }
    }

    for (auto it = combined_summands.begin(); it != combined_summands.end();) {
        if (it->second == 0) it = combined_summands.erase(it);
        else it++;
    }

    return this->node_store_.insert_polynode(
            Polynode<R>(std::move(combined_summands), this->node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::sub(const Idx var, const Polynode<R>& val) const {
    // It is likely not a repeat, so we do not compute the hash first
    const Polynode<R>* sum = this->node_store_.zero();
    for (const std::pair<MononodeHash, R> entry : this->summands_) {
        const Polynode<R>* term = this->node_store_.one();
        std::vector<NodeHash> non_sub_factors{};

        // Pretty costly, but we'll just multiply everything together for now
        for (const NodeHash factor : this->node_store_.get_mononode(entry.first)->factors_) { 
            switch (this->node_store_.get_node(factor)->type_) {
                // If it is the correct variable, substitute
                case NodeType::VAR: {
                    if (this->node_store_.get_node(factor)->var_ == var) {
                        term = *term * val;
                    } else {
                        non_sub_factors.push_back(factor);
                    }
                    break;
                }
                // If it is f(polynode), then we need to recursively substitute inside the entire polynode
                case NodeType::POL: {
                    non_sub_factors.push_back(this->node_store_.node
                            (this->node_store_.get_polynode(
                                this->node_store_.get_node(factor)->pol_
                            )->sub(var, val)->hash
                        )->hash);
                    break;
                }
            }
        }
        term = *term * 
                *this->node_store_.polynode(
                    {{this->node_store_.mononode(std::move(non_sub_factors))->hash, entry.second}}
                );

        sum = *sum + *term;
    }
    return sum;
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::apply_func(const Polynode<R>& rhs) const {
    return this->node_store_.polynode({
            {this->node_store_.mononode({ 
                    this->node_store_.node((*this + rhs)->hash)->hash 
                })->hash, 1},
            {this->node_store_.mononode({ 
                    this->node_store_.node(rhs.hash)->hash 
                })->hash, -1}
        });
}

template class algebra::NodeBase<size_t>;

template class algebra::NodeStore<int>;
template class algebra::Node<int>;
template class algebra::Mononode<int>;
template class algebra::Polynode<int>;
