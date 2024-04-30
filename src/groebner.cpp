#include "../include/groebner.hpp"

#include <iostream>
#include <vector>

template<class R>
groebner::Reducer<R>::Reducer(algebra::NodeStore<R> &node_store) : node_store_(node_store) {}

template<class R>
groebner::Poly<R>* groebner::Reducer<R>::S_poly(Poly<R>* p1, Poly<R>* p2) {
    std::pair<Mono<R>*, Mono<R>*> sym_q = p1->leading_m()->symmetric_q(*p2->leading_m());
    
    auto S=*p1->scale(*sym_q.first , 1 / p1->leading_c()) 
         - *p2->scale(*sym_q.second, 1 / p2->leading_c());

    return S;
}

// Returns true if p was lead reduced
template<class R>
bool try_lead_reduce(groebner::Poly<R>* &p, const groebner::PolyIter<R> &it, algebra::NodeStore<R> &node_store) {
    if (*p == *node_store.zero_p()) {
        return false;
    }

    std::pair<groebner::Mono<R>*, groebner::Mono<R>*> sym_q = p->leading_m()->symmetric_q(*(*it)->leading_m());

    // If the leading monomial of p is divisible by the leading monomial of *it, then subtract
    if (*sym_q.first == *node_store.one_m()) {
        p = *p + *(*it)->scale(*sym_q.second, -p->leading_c() / (*it)->leading_c());
        return true;
    }
    return false;
}

template<class R>
groebner::Poly<R>* groebner::Reducer<R>::lead_reduce(Poly<R>* p, const PolyIter<R> &b_start, const PolyIter<R> &b_end) {
    bool reduced = false;
    while (!reduced) {
        reduced = true;

        for (PolyIter<R> it = b_start; it != b_end; it++) {
            if (try_lead_reduce(p, it, this->node_store_)) {
                reduced = false;
            }
        }
    }
    return p;
}

// Returns true if p was reduced
template<class R>
bool try_reduce(groebner::Poly<R>* &p, const groebner::PolyIter<R> &it, algebra::NodeStore<R> &node_store) {
    if (*p == *node_store.zero_p()) {
        return false;
    }

    // TODO:
    // Probably ends up being Schlemiel the painter 
    // (if nothing up to term n can be reduced the first itoration, it doesn't change the next)
    // but that's ok for now
    for (const std::pair<algebra::MononodeHash, R> &term : *p) {
        groebner::Mono<R>* m = node_store.get_mononode(term.first);
        std::pair<groebner::Mono<R>*, groebner::Mono<R>*> sym_q = m->symmetric_q(*(*it)->leading_m());

        // If some monomial of p is divisible by the leading monomial of *it, then subtract
        if (*sym_q.first == *node_store.one_m()) {
            p = *p + *(*it)->scale(*sym_q.second, -term.second / (*it)->leading_c());
            return true;
        }
    }
    return false;
}

template<class R>
groebner::Poly<R>* groebner::Reducer<R>::reduce(Poly<R>* p, 
        const PolyIter<R> &b_start, const PolyIter<R> &b_end, 
        const PolyIter<R> &b_start2, const PolyIter<R> &b_end2) {
    bool reduced = false;
    while (!reduced) {
        reduced = true;

        for (PolyIter<R> it = b_start; it != b_end; it++) {
            if (try_reduce(p, it, this->node_store_)) {
                reduced = false;
            }
        }

        for (PolyIter<R> it = b_start2; it != b_end2; it++) {
            if (try_reduce(p, it, this->node_store_)) {
                reduced = false;
            }
        }
    }
    return p;
}

// Basic implementation of Buchberger's algorithm
template<class R>
std::vector<groebner::Poly<R>*> groebner::Reducer<R>::basis(std::vector<Poly<R>*> gen) {
    for (size_t i = 0; i < gen.size(); i++) {
        for (size_t j = 0; j < i; j++) {
            //std::cout << "Testing " << gen[i]->to_string() << " and " << gen[j]->to_string() << std::endl;
            // See https://www.andrew.cmu.edu/course/15-355/lectures/lecture11.pdf
            // Buchberger's first criterion
            //
            // Efficient for linears
            std::pair<Mono<R>*, Mono<R>*> sym_q = gen[i]->leading_m()->symmetric_q(*gen[j]->leading_m());
            if (*sym_q.first == *gen[j]->leading_m() && *sym_q.second == *gen[i]->leading_m()) {
                continue;
            }

            Poly<R>* S = this->S_poly(gen[i], gen[j]);
            Poly<R>* S_red = this->lead_reduce(S, gen.begin(), gen.end());

            if (*S_red != *this->node_store_.zero_p()) {
                gen.push_back(S_red);
            }
        }
    }
    return gen;
}

template<class R>
std::vector<groebner::Poly<R>*> groebner::Reducer<R>::reduced_basis(std::vector<Poly<R>*> gen) {
    std::vector<Poly<R>*> basis = this->basis(gen);
    size_t len = basis.size();

    //std::cout << "Made basis of size " << len << std::endl;

    // Make the basis minimal
    std::vector<bool> divisible(len, false);
    for (size_t i = 0; i < len; i++) {
        for (size_t j = 0; j < i; j++) {
            // Try to divide basis[i].lm() and basis[j].lm() into each other with symmetric_q
            std::pair<Mono<R>*, Mono<R>*> sym_q = basis[i]->leading_m()->symmetric_q(*basis[j]->leading_m());
            // If one is divisible by the other, mark that one for deletion
            // If the leading terms are equal, we shouldn't remove both. Prioritize the first to be deleted
            if (*sym_q.first == *this->node_store_.one_m()) {
                divisible[i] = true;
            } else if (*sym_q.second == *this->node_store_.one_m()) {
                divisible[j] = true;
            }
        }
    }
    std::vector<Poly<R>*> min_basis;
    for (size_t i = 0; i < len; i++) {
        if (!divisible[i]) {
            min_basis.push_back(basis[i]->scale(*this->node_store_.one_m(), 1 / basis[i]->leading_c()));
        }
    }
    len = min_basis.size();
    //std::cout << "Minimized basis to size " << len << std::endl;

    // Turn the minimal basis into a reduced basis
    // https://pi.math.cornell.edu/~dmehrle/notes/old/alggeo/15BuchbergersAlgorithm.pdf

    std::vector<Poly<R>*> red_basis;
    red_basis.reserve(len);

    typename std::vector<Poly<R>*>::iterator after = min_basis.begin();
    for (size_t i = 0; i < len; i++) {
        after++;

        red_basis.push_back(reduce(min_basis[i], after, min_basis.end(), red_basis.begin(), red_basis.end()));
    }

    return red_basis;
}

template class groebner::Reducer<mpq_class>;
