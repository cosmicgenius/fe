// algebra.hpp
#ifndef ALGEBRA_HPP_
#define ALGEBRA_HPP_

#include <cstddef>
#include <set>
#include <unordered_map>
#include <vector>
#include <string>

namespace algebra {
    typedef int Idx;
    typedef size_t NodeHash;
    typedef size_t MononodeHash;
    typedef size_t PolynodeHash;
    
    template<class H>
    class HashedClass {
    protected:
        H hash_;

    public:
        bool operator==(const HashedClass<H>& rhs) const;
        bool operator!=(const HashedClass<H>& rhs) const;

        H hash() const;
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
    public:
        const Node<R>* get_node(const NodeHash hash) const;
        const Mononode<R>* get_mononode(const MononodeHash hash) const;
        const Polynode<R>* get_polynode(const PolynodeHash hash) const;

        const Node<R>* node(const PolynodeHash pol);
        const Node<R>* node(const Idx var);

        const Mononode<R>* mononode(const std::vector<NodeHash>&& factors);

        const Polynode<R>* polynode(const std::unordered_map<MononodeHash, R>&& summands);

        const Polynode<R>* zero();
        const Polynode<R>* one();

        const Node<R>* insert_node(Node<R>&& node);
        const Mononode<R>* insert_mononode(Mononode<R>&& mononode);
        const Polynode<R>* insert_polynode(Polynode<R>&& polynode);

        size_t get_node_store_size() const;
        size_t get_mononode_store_size() const;
        size_t get_polynode_store_size() const;
    };

    // Nodes are either:
    //  i)  f(P) where P is a polynode
    //  ii) xi, a variable
    enum NodeType {
        POL,
        VAR,
    };
    
    template<class R>
    class Node : public HashedClass<NodeHash> {
    private:
        const NodeType type_;

        const PolynodeHash pol_;
        const Idx var_;

        void calculate_hash();
        NodeStore<R> &node_store_;

    public:
        Node(const PolynodeHash pol, NodeStore<R> &node_store);
        Node(const Idx var, NodeStore<R> &node_store);

        Node(const Node& other) = delete;
        Node(Node&& other) = default;

        std::string to_string() const;

        friend class Polynode<R>;
    };

    // A monomial of nodes
    // (formally, of lesser order)
    //
    // Hash should not depend on the order of the elements in factors_
    template <class R>
    class Mononode : public HashedClass<MononodeHash> {
    private:
        const std::vector<NodeHash> factors_;

        void calculate_hash();
        NodeStore<R> &node_store_;

    public:
        Mononode(const std::vector<NodeHash>&& factors, NodeStore<R> &node_store);

        Mononode(const Mononode& other) = delete;
        Mononode(Mononode&& other) = default;

        std::string to_string() const;
                       
        const Mononode<R>* operator*(const Mononode<R>& rhs) const;

        friend class Polynode<R>;
    };

    template<class R>
    class Polynode : public HashedClass<PolynodeHash> {
    private:
        const std::unordered_map<MononodeHash, R> summands_;

        void calculate_hash();
        NodeStore<R> &node_store_;
    public:
        Polynode(const std::unordered_map<MononodeHash, R>&& summands, NodeStore<R> &node_store);

        Polynode(const Polynode& other) = delete;
        Polynode(Polynode&& other) = default;

        std::string to_string() const;

        const Polynode<R>* operator-() const;

        const Polynode<R>* operator+(const Polynode<R>& rhs) const;
        const Polynode<R>* operator-(const Polynode<R>& rhs) const;
        const Polynode<R>* operator*(const Polynode<R>& rhs) const;

        // Substitute a variable by a polynode
        const Polynode<R>* sub(const Idx var, const Polynode<R>& val) const;

        // Given the equation this = 0, 
        // apply f to both sides of the equation this + rhs = rhs
        const Polynode<R>* apply_func(const Polynode<R>& rhs) const;
    };
}; 

#endif 
