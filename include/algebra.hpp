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

        const Node<R>* new_node(const PolynodeHash pol);
        const Node<R>* new_node(const Idx var);

        const Mononode<R>* new_mononode(const std::vector<NodeHash>&& factors);

        const Polynode<R>* new_polynode(const std::unordered_map<MononodeHash, R>&& summands);

        const Node<R>* insert_node(Node<R>&& node);
        const Mononode<R>* insert_mononode(Mononode<R>&& mononode);
        const Polynode<R>* insert_polynode(Polynode<R>&& polynode);
    };

    // Nodes are either:
    //  i)  f(P) where P is a polynode
    //  ii) xi, a variable
    enum NodeType {
        POL,
        VAR,
    };
    
    template<class R>
    class Node {
    private:
        const NodeType type_;

        const PolynodeHash pol_;
        const Idx var_;

        NodeHash hash_;
        void calculate_hash();

        NodeStore<R> &node_store_;

    public:
        Node(const PolynodeHash pol, NodeStore<R> &node_store);
        Node(const Idx var, NodeStore<R> &node_store);

        Node(const Node& other) = delete;
        Node(Node&& other) = default;

        bool operator==(const Node<R>& rhs) const;

        NodeHash hash() const;
        std::string to_string() const;
    };

    // A monomial of nodes
    // (formally, of lesser order)
    template <class R>
    class Mononode {
    private:
        const std::vector<NodeHash> factors_;

        MononodeHash hash_;
        void calculate_hash();

        NodeStore<R> &node_store_;

    public:
        Mononode(const std::vector<NodeHash>&& factors, NodeStore<R> &node_store);

        Mononode(const Mononode& other) = delete;
        Mononode(Mononode&& other) = default;

        MononodeHash hash() const; // should not depend on the order of the elements in factors_
        std::string to_string() const;
                       
        const Mononode<R>* operator*(const Mononode<R>& rhs) const;
    };

    template<class R>
    class Polynode {
    private:
        const std::unordered_map<MononodeHash, R> summands_;

        PolynodeHash hash_;
        void calculate_hash();

        NodeStore<R> &node_store_;
    public:
        Polynode(const std::unordered_map<MononodeHash, R>&& summands, NodeStore<R> &node_store);

        Polynode(const Polynode& other) = delete;
        Polynode(Polynode&& other) = default;

        PolynodeHash hash() const;
        std::string to_string() const;

        const Polynode<R>* operator+(const Polynode<R>& rhs) const;
        const Polynode<R>* operator*(const Polynode<R>& rhs) const;
    };
}; 

#endif 
