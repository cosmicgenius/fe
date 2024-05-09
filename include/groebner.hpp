#include "algebra.hpp"

#include <gmpxx.h>
#include <chrono>
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
    bool stop_;
    std::chrono::system_clock::time_point stop_time_;

    std::vector<Poly<R>*> polys_;

    algebra::NodeStore<R> &node_store_;

    Poly<R>* S_poly(Poly<R>* p1, Poly<R>* p2);
    
    // Lead reduce p wrt the basis [*b_start, *b_end)
    Poly<R>* lead_reduce(Poly<R>* p, const PolyIter<R> &b_start, const PolyIter<R> &b_end);

    // (All term) Reduce p wrt the basis [*b_start, *b_end) cup [*b_start2, *b_end2)
    Poly<R>* reduce(Poly<R>* p, const PolyIter<R> &b_start, const PolyIter<R> &b_end, 
                                const PolyIter<R> &b_start2, const PolyIter<R> &b_end2);

    bool calculate_gbasis();
public:
    Reducer(std::vector<Poly<R>*> polys, algebra::NodeStore<R> &node_store);

    // Calculate a reduced Groebner basis, and override the current polynomials
    //
    // Takes a max duration in milliseconds that determines how long the main loop
    // will run (-1 for no limit). The postprocessing should be fast
    //
    // Returns whether the main loop finished 
    //
    // NOT thread-safe
    bool calculate_reduced_gbasis(int max_duration_ms = -1);
    
    std::vector<Poly<R>*> get_polys() const;
};
};
