// algebra.hpp
#ifndef ALGEBRA_HPP_
#define ALGEBRA_HPP_

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace algebra {
    typedef int Idx;
    typedef size_t NodeHash;
    typedef size_t MononodeHash;
    typedef size_t PolynodeHash;
    
    template<class Hash>
    class NodeBase {
    public:
        const Hash hash;
        const int weight;

        NodeBase(const Hash hash, const int weight);

        bool operator==(const NodeBase<Hash>& rhs) const;
        bool operator!=(const NodeBase<Hash>& rhs) const;
    };

    // R should be an integral domain
    
    template<class R>
    class Node;

    template<class R>
    class Mononode;

    template<class R>
    class Polynode;

    template<class R>
    class NodeStore {
    private:
        std::unordered_map<NodeHash, Node<R>> nodes_;
        std::unordered_map<MononodeHash, Mononode<R>> mononodes_;
        std::unordered_map<PolynodeHash, Polynode<R>> polynodes_;
    
        size_t conj_;

        void dump() const;
    public:
        NodeStore(const size_t seed = 0);

        size_t hash(const size_t n) const;

        const Node<R>* get_node(const NodeHash hash) const;
        const Mononode<R>* get_mononode(const MononodeHash hash) const;
        const Polynode<R>* get_polynode(const PolynodeHash hash) const;

        const Node<R>* node(const PolynodeHash pol);
        const Node<R>* node(const Idx var);

        const Mononode<R>* mononode(const std::unordered_map<NodeHash, int>& factors);

        const Polynode<R>* polynode(const std::vector<std::pair<MononodeHash, R>>& summands);

        const Mononode<R>* one_m();

        const Polynode<R>* zero_p();
        const Polynode<R>* one_p();

        const Node<R>* insert_node(Node<R>&& node);
        const Mononode<R>* insert_mononode(Mononode<R>&& mononode);
        const Polynode<R>* insert_polynode(Polynode<R>&& polynode);

        int node_cmp(const NodeHash lhs, const NodeHash rhs) const;
        int mononode_cmp(const MononodeHash lhs, const MononodeHash rhs) const;

        size_t get_node_store_size() const;
        size_t get_mononode_store_size() const;
        size_t get_polynode_store_size() const;
    };

    // Nodes are either:
    //  i)  f(P), where P is a polynode
    //  ii)   xi, a variable
    enum NodeType {
        POL,
        VAR,
    };
    
    template<class R>
    class Node : public NodeBase<NodeHash> {
    private:
        const NodeType type_;

        const PolynodeHash pol_;
        const Idx var_;

        NodeStore<R> &node_store_;

    public:
        Node(const PolynodeHash pol, NodeStore<R> &node_store);
        Node(const Idx var, NodeStore<R> &node_store);

        Node(const Node& other) = delete;
        Node(Node&& other) = default;

        std::string to_string() const;

        friend class Polynode<R>;
        friend class NodeStore<R>;
    };

    // A monomial of nodes
    // (formally, of lesser order)
    template <class R>
    class Mononode : public NodeBase<MononodeHash> {
    private:
        const std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>> factors_;
        const int degree;

        NodeStore<R> &node_store_;

        static std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>> clean_factors(
                const std::unordered_map<NodeHash, int> &factors, NodeStore<R> &node_store);

        // Private constructor with move assumes correct sorting in map
        Mononode(const std::map<NodeHash, int, std::function<bool(const NodeHash, const NodeHash)>>&& factors, 
                NodeStore<R> &node_store);

    public:
        Mononode(const std::unordered_map<NodeHash, int>& factors, NodeStore<R> &node_store);

        Mononode(const Mononode& other) = delete;
        Mononode(Mononode&& other) = default;

        std::string to_string() const;
                       
        const Mononode<R>* operator*(const Mononode<R>& rhs) const;

        // Return {lcm(lhs, rhs) / lhs, lcm(lhs, rhs) / rhs}, the "symmetric quotient"
        std::pair<const Mononode<R>*, const Mononode<R>*> symmetric_q(const Mononode<R>& rhs) const;

        friend class Polynode<R>;
        friend class NodeStore<R>;
    };

    template<class R>
    class Polynode : public NodeBase<PolynodeHash> {
    private:
        const std::vector<std::pair<MononodeHash, R>> summands_;

        NodeStore<R> &node_store_;

        static std::vector<std::pair<MononodeHash, R>> clean_summands(
                const std::vector<std::pair<MononodeHash, R>> &summands, NodeStore<R> &node_store);

        // Private constructor with move assumes already sorted "keys"
        Polynode(const std::vector<std::pair<MononodeHash, R>>&& summands, NodeStore<R> &node_store);
    public:
        Polynode(const std::vector<std::pair<MononodeHash, R>>& summands, NodeStore<R> &node_store);

        Polynode(const Polynode& other) = delete;
        Polynode(Polynode&& other) = default;

        std::string to_string() const;

        const Polynode<R>* operator-() const;

        const Polynode<R>* operator+(const Polynode<R>& rhs) const;
        const Polynode<R>* operator-(const Polynode<R>& rhs) const;
        const Polynode<R>* operator*(const Polynode<R>& rhs) const;

        // Scale by c * m
        const Polynode<R>* scale(const Mononode<R>& m, const R c) const;

        // Leading monomial
        const Mononode<R>* leading_m() const;

        // Leading coefficient
        const R leading_c() const;

        // All summands
        // Apparently minimal Gröbner basis => reduced Gröbner basis requires reduction on all summands
        //
        // TODO: This is very not optimal
        const std::vector<std::pair<MononodeHash, R>>& summands() const;

        // Substitute a variable by a polynode
        const Polynode<R>* sub(const Idx var, const Polynode<R>& val) const;

        // Given the equation this = 0, 
        // apply f to both sides of the equation this + rhs = rhs
        const Polynode<R>* apply_func(const Polynode<R>& rhs) const;
    };
}; 

#endif 
