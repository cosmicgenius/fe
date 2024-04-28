#include "algebra.hpp"

#include <gmpxx.h>
#include <iterator>

namespace groebner {
template<class R> 
using Mono = const algebra::Mononode<R>;

template<class R> 
using Poly = const algebra::Polynode<R>;

template<class R>
using PolyIter = typename std::vector<Poly<R>*>::const_iterator;

template<class R>
class Reducer {

private:
    algebra::NodeStore<R> &node_store_;

    Poly<R>* S_poly(Poly<R>* p1, Poly<R>* p2);
    
    // Lead reduce p wrt the basis [*b_start, *b_end)
    Poly<R>* lead_reduce(Poly<R>* p, const PolyIter<R> &b_start, const PolyIter<R> &b_end);

    // (All term) Reduce p wrt the basis [*b_start, *b_end) cup [*b_start2, *b_end2)
    Poly<R>* reduce(Poly<R>* p, const PolyIter<R> &b_start, const PolyIter<R> &b_end, 
                                const PolyIter<R> &b_start2, const PolyIter<R> &b_end2);
public:
    Reducer(algebra::NodeStore<R> &node_store);

    std::vector<Poly<R>*> basis(std::vector<Poly<R>*> gen);
    std::vector<Poly<R>*> reduced_basis(std::vector<Poly<R>*> gen);
};
};
