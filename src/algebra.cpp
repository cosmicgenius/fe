#include "../include/algebra.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <numeric>
#include <map>


/*
 * Util
 */

inline int sq(const int n) {
    return n * n;
}

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

template<class R>
void algebra::NodeStore<R>::dump() const {
    std::vector<NodeHash> node_keys;
    node_keys.reserve(this->nodes_.size());

    for (const std::pair<const NodeHash, Node<R>>& node : this->nodes_) node_keys.push_back(node.first);
    std::sort(node_keys.begin(), node_keys.end(), 
            [this] (const NodeHash lhs, const NodeHash rhs) { return this->node_cmp(lhs, rhs) < 0;});

    std::cout << "Nodes:\n";
    for (const NodeHash nh : node_keys) {
        const Node<R>& node = this->nodes_.at(nh);
        std::cout << node.to_string() << " " << nh << " " << node.weight << "\n";
    }
    std::cout << std::flush;

    std::vector<MononodeHash> mononode_keys;
    mononode_keys.reserve(this->mononodes_.size());

    for (const std::pair<const MononodeHash, Mononode<R>>& mononode : this->mononodes_) mononode_keys.push_back(mononode.first);
    std::sort(mononode_keys.begin(), mononode_keys.end(),
            [this] (const MononodeHash lhs, const MononodeHash rhs) { return this->mononode_cmp(lhs, rhs) < 0;});

    std::cout << "Mononodes:\n";
    for (const MononodeHash mh : mononode_keys) {
        const Mononode<R>& mononode = this->mononodes_.at(mh);
        std::cout << mononode.to_string() << " " << mh << " " << mononode.weight << " " << mononode.degree << "\n";
    }
    std::cout << std::flush;

    std::cout << "Polynodes:\n";
    for (const std::pair<const PolynodeHash, Polynode<R>>& polynode : this->polynodes_) {
        std::cout << polynode.second.to_string() << " " << polynode.first << " " << polynode.second.weight << "\n";
    }
    std::cout << std::flush;
}

/*
 * Node
 */

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

template<class R>
int algebra::NodeStore<R>::node_cmp(const NodeHash lhs, const NodeHash rhs) const {
    const algebra::Node<R>* const lhs_ptr = this->get_node(lhs), *rhs_ptr = this->get_node(rhs);

    if (lhs_ptr->weight != rhs_ptr->weight) return lhs_ptr->weight - rhs_ptr->weight;
    // If they have the same weight and one is a variable, the other is too
    if (lhs_ptr->type_ == algebra::NodeType::VAR) return lhs_ptr->var_ - rhs_ptr->var_;

    // They are both f(polynode), arbitrarily tiebreak
    return (lhs == rhs) ? 0 : (lhs < rhs ? -1 : 1);
}

/*
 * grevlex for now, TODO: change to some sort of elimination order
 *
 * Reversed in order to put the leading monomial in summands.front()
 */ 

template<class R>
int algebra::NodeStore<R>::mononode_cmp(const MononodeHash lhs, const MononodeHash rhs) const {
    const Mononode<R>* const lhs_ptr = this->get_mononode(lhs), *rhs_ptr = this->get_mononode(rhs);
    if (lhs_ptr->degree != rhs_ptr->degree) return -(lhs_ptr->degree - rhs_ptr->degree); // Reverse
    if (lhs_ptr->degree == 0) return 0; // There is only one such mononode, the empty product

    for (auto lhs_factor = lhs_ptr->factors_.begin(), rhs_factor = rhs_ptr->factors_.begin();
            lhs_factor != lhs_ptr->factors_.end() && rhs_factor != rhs_ptr->factors_.end();
            lhs_factor++, rhs_factor++) {
        // Reversed
        if (lhs_factor->first != rhs_factor->first) return -this->node_cmp(lhs_factor->first, rhs_factor->first); 
        // Two reverses cancel out
        if (lhs_factor->second != rhs_factor->second) return lhs_factor->second < rhs_factor->second ? -1 : 1;
    }

    return 0;
}


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
         std::accumulate(factors.begin(), factors.end(), 0, 
            [&node_store](int weight, const std::pair<NodeHash, int>& cur) { 
                return weight + node_store.get_node(cur.first)->weight * cur.second; 
            })
        ), 
    factors_(std::move(factors)),
    degree(
         std::accumulate(factors.begin(), factors.end(), 0,
            [](int deg, const std::pair<NodeHash, int>& cur) { 
                    return deg + cur.second;
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
         std::accumulate(factors.begin(), factors.end(), 0, 
            [&node_store](int weight, const std::pair<NodeHash, int>& cur) { 
                return weight + node_store.get_node(cur.first)->weight * cur.second; 
            })
        ), 
    factors_(std::move(clean_factors(factors, node_store))),
    degree(
         std::accumulate(factors.begin(), factors.end(), 0,
            [](int deg, const std::pair<NodeHash, int>& cur) { 
                    return deg + cur.second;
            })
        ),
    node_store_(node_store) {}

template<class R>
std::string algebra::Mononode<R>::to_string() const {
    if (this->factors_.empty()) return "";

    // Joins strings by a space
    std::string res = this->node_store_.get_node(this->factors_.begin()->first)->to_string();
    for (auto it = this->factors_.begin(); it != this->factors_.end(); it++) {
        for (int rep = (it == this->factors_.begin()); rep < it->second; rep++) { 
            res += " ";
            res += this->node_store_.get_node(it->first)->to_string();
        }
    }
    return res;
}

template<class R>
const algebra::Mononode<R>* algebra::Mononode<R>::operator*(const Mononode<R>& rhs) const {
    // Very inexpensive to compute the hash first,
    // test if it is a repeat, and if not, compute the whole thing
    MononodeHash product_hash = this->hash * rhs.hash;
    const Mononode<R>* cached = this->node_store_.get_mononode(product_hash);

    if (cached != nullptr) return cached;

    std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>>
        combined_factors(this->factors_.begin(), this->factors_.end(), 
        [&node_store = this->node_store_] (const NodeHash lhs, const NodeHash rhs) 
            { return node_store.node_cmp(lhs, rhs) < 0; }
        );
    for (const std::pair<const NodeHash, int> &rhs_entry : rhs.factors_) {
        combined_factors[rhs_entry.first] += rhs_entry.second;
    }

    return this->node_store_.insert_mononode(Mononode<R>(std::move(combined_factors), this->node_store_));
}


/*
 * Polynode
 */

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
                    return hash ^ node_store.hash(PolynodeHash(cur.first) + PolynodeHash(cur.second));
                }), 
             std::accumulate(summands.begin(), summands.end(), 0, 
                [&node_store] (const PolynodeHash weight, const std::pair<MononodeHash, R>& cur) { 
                    return weight + node_store.get_mononode(cur.first)->weight;
                })
            ),
    summands_(std::move(summands)), 
    node_store_(node_store) {}

template<class R>
algebra::Polynode<R>::Polynode(const std::vector<std::pair<MononodeHash, R>>& summands, 
        NodeStore<R> &node_store) : 
    NodeBase(std::accumulate(summands.begin(), summands.end(), PolynodeHash(0), 
                [&node_store] (const PolynodeHash hash, const std::pair<MononodeHash, R>& cur) { 
                    return hash ^ node_store.hash(PolynodeHash(cur.first) + PolynodeHash(cur.second));
                }), 
             std::accumulate(summands.begin(), summands.end(), 0, 
                [&node_store] (const PolynodeHash weight, const std::pair<MononodeHash, R>& cur) { 
                    return weight + node_store.get_mononode(cur.first)->weight;
                })
            ),
    summands_(std::move(clean_summands(summands, node_store))), 
    node_store_(node_store) {}

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

    std::vector<std::pair<MononodeHash, R>> neg_summands(this->summands_);
    for (std::pair<MononodeHash, R> &neg_entry : neg_summands) {
        neg_entry.second *= -1;
    }

    return this->node_store_.insert_polynode(Polynode<R>(std::move(neg_summands), this->node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::operator+(const Polynode<R>& rhs) const {
    std::vector<std::pair<MononodeHash, R>> combined_summands;
    combined_summands.reserve(this->summands_.size() + rhs.summands_.size());

    // Merge, assuming both are sorted
    for (auto itl = this->summands_.begin(), itr = rhs.summands_.begin();
            itl != this->summands_.end() || itr != rhs.summands_.end();) {
        if (itl != this->summands_.end() && (itr == rhs.summands_.end() 
                    || this->node_store_.mononode_cmp(itl->first, itr->first) < 0)) {
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
                
    return this->node_store_.insert_polynode(Polynode<R>(std::move(combined_summands), this->node_store_));
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
        combined_summands([&node_store = this->node_store_] (const MononodeHash lhs, const MononodeHash rhs) {
                    return node_store.mononode_cmp(lhs, rhs) < 0;
                });

    for (const std::pair<MononodeHash, R> &lhs_entry : this->summands_) {
        for (const std::pair<MononodeHash, R> &rhs_entry : rhs.summands_) {
            const Mononode<R>* prod = *this->node_store_.get_mononode(lhs_entry.first) 
                * *this->node_store_.get_mononode(rhs_entry.first);
            combined_summands[prod->hash] += lhs_entry.second * rhs_entry.second;
        }
    }

    std::vector<std::pair<MononodeHash, R>> combined_summands_vec;
    combined_summands_vec.reserve(combined_summands.size());

    for (auto it = combined_summands.begin(); it != combined_summands.end(); it++) {
        if (it->second != 0) combined_summands_vec.emplace_back(std::move(*it));
    }

    return this->node_store_.insert_polynode(
            Polynode<R>(std::move(combined_summands_vec), this->node_store_));
}

template<class R>
const algebra::Polynode<R>* algebra::Polynode<R>::sub(const Idx var, const Polynode<R>& val) const {
    // It is likely not a repeat, so we do not compute the hash first
    const Polynode<R>* sum = this->node_store_.zero();
    for (const std::pair<MononodeHash, R> &entry : this->summands_) {
        const Polynode<R>* term = this->node_store_.one();
        std::unordered_map<NodeHash, int> non_sub_factors{};

        // Pretty costly, but we'll just multiply everything together for now
        for (const std::pair<const NodeHash, int> &factor : this->node_store_.get_mononode(entry.first)->factors_) { 
            switch (this->node_store_.get_node(factor.first)->type_) {
                // If it is the correct variable, substitute
                case NodeType::VAR: {
                    if (this->node_store_.get_node(factor.first)->var_ == var) {
                        for (int rep = 0; rep < factor.second; rep++) {
                            term = *term * val;
                        }
                    } else {
                        non_sub_factors[factor.first] += factor.second;
                    }
                    break;
                }
                // If it is f(polynode), then we need to recursively substitute inside the entire polynode
                case NodeType::POL: {
                    non_sub_factors[this->node_store_.node
                            (this->node_store_.get_polynode(
                                this->node_store_.get_node(factor.first)->pol_
                            )->sub(var, val)->hash
                        )->hash] += factor.second;
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
                    {this->node_store_.node((*this + rhs)->hash)->hash, 1}
                })->hash, 1},
            {this->node_store_.mononode({ 
                    {this->node_store_.node(rhs.hash)->hash, 1}
                })->hash, -1}
        });
}

template class algebra::NodeBase<size_t>;

template class algebra::NodeStore<int>;
template class algebra::Node<int>;
template class algebra::Mononode<int>;
template class algebra::Polynode<int>;
