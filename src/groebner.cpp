#include "../include/groebner.hpp"

#include <iostream>
#include <vector>
#include <queue>

std::chrono::time_point<std::chrono::system_clock> now() {
    return std::chrono::system_clock::now();
}

template<class R>
groebner::Reducer<R>::Reducer(std::vector<groebner::Poly<R>*> polys, 
        algebra::NodeStore<R> &node_store) : polys_(polys), node_store_(node_store) {}

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

    // If the leading monomial of p is divisible by the leading monomial of *it, then subtract
    if (p->leading_m()->divisible(*(*it)->leading_m())) {
        std::pair<groebner::Mono<R>*, groebner::Mono<R>*> sym_q = p->leading_m()->symmetric_q(*(*it)->leading_m());
        p = *p + *(*it)->scale(*sym_q.second, -p->leading_c() / (*it)->leading_c());

        return true;
    }
    return false;
}

template<class R>
groebner::Poly<R>* groebner::Reducer<R>::lead_reduce(Poly<R>* p, const PolyIter<R> &b_start, const PolyIter<R> &b_end) {
    bool reduced = false;
    //int rep = 0;
    while (!reduced && !(stop_ && now() > stop_time_)) {
        reduced = true;

        for (PolyIter<R> it = b_start; it != b_end; it++) {
            if (try_lead_reduce(p, it, node_store_)) {
                reduced = false;
            }
        }
        //rep++;
    }
    //std::cout << "Done in " << rep << " repetitions" << std::endl;
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

        // If some monomial of p is divisible by the leading monomial of *it, then subtract
        if (m->divisible(*(*it)->leading_m())) {
            std::pair<groebner::Mono<R>*, groebner::Mono<R>*> sym_q = m->symmetric_q(*(*it)->leading_m());
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
    while (!reduced && !(stop_ && now() > stop_time_)) {
        reduced = true;

        for (PolyIter<R> it = b_start; it != b_end; it++) {
            if (try_reduce(p, it, node_store_)) {
                reduced = false;
            }
        }

        for (PolyIter<R> it = b_start2; it != b_end2; it++) {
            if (try_reduce(p, it, node_store_)) {
                reduced = false;
            }
        }
    }
    return p;
}

// Basic implementation of Buchberger's algorithm
// See https://www.andrew.cmu.edu/course/15-355/lectures/lecture11.pdf
template<class R>
bool groebner::Reducer<R>::calculate_gbasis() {
    if (polys_.empty()) return true;
    int len = polys_.size();

    // LCMs of leading mononodes
    // lm_lcms[i][j] = lcm(gen[i]->leading_m(), gen[j]->leading_m()) for i < j
    std::vector<std::vector<algebra::MononodeHash>> lm_lcms; 
    std::vector<std::vector<bool>> S_computed; 

    // pq stores all pairs that are still being considered, 
    // We wish to pick the S polynomial with the smallest lcm of leading monomials
    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>,
        std::function<bool(const std::pair<int, int>&, const std::pair<int, int>&)>>
    pq([this, &lm_lcms] 
        (const std::pair<int, int>& lhs, const std::pair<int, int>& rhs) {
            return node_store_.mononode_cmp(
                    lm_lcms[lhs.first][lhs.second],
                    lm_lcms[rhs.first][rhs.second]
                ) < 0;
        }); 

    S_computed.resize(len);
    lm_lcms.resize(len);

    for (int i = 1; i < len; i++) {
        lm_lcms[i].reserve(i);
        S_computed[i].resize(i);
        for (int j = 0; j < i; j++) {
            lm_lcms[i].push_back(polys_[i]->leading_m()->lcm(*polys_[j]->leading_m())->hash);
            pq.push({i, j});
        }
    }

    while (!pq.empty() && !(stop_ && now() > stop_time_)) {
        std::pair<int, int> ij = pq.top();
        pq.pop();
        
        int i = ij.first, j = ij.second;
        len = polys_.size();

        //std::cout << "Testing " << polys_[i]->to_string() << " and " << polys_[j]->to_string() << std::endl;
        //std::cout << "Testing " << i << " and " << j << ". Max length: " << len << std::endl;

        // Buchberger's first criterion
        // Efficient for linears
        Mono<R>* lcm = polys_[i]->leading_m()->lcm(*polys_[j]->leading_m());
        if (*(*polys_[i]->leading_m() * *polys_[j]->leading_m()) == *lcm) {
            //std::cout << "Skipping, fails Buchberger's first criterion" << std::endl;
            continue;
        }

        // Buchberger's second criterion
        bool skip = false;
        for (int k = 0; k < len; k++) {
            if (k == i || k == j) continue;
            if (S_computed[std::max(i, k)][std::min(i, k)] && 
                S_computed[std::max(j, k)][std::min(j, k)] && 
                lcm->divisible(*polys_[k]->leading_m())) {

                skip = true;
                break;
            }
        }
        if (skip) {
            //std::cout << "Skipping, fails Buchberger's second criterion" << std::endl;
            continue;
        }

        Poly<R>* S = S_poly(polys_[i], polys_[j]);
        Poly<R>* S_red = lead_reduce(S, polys_.begin(), polys_.end());

        if (*S_red != *node_store_.zero_p()) {
            //std::cout << "Added " << polys_.size() << ": len=" << (S_red->end() - S_red->begin()) << std::endl;

            S_computed.push_back({});
            S_computed.back().resize(len);
            lm_lcms.push_back({});

            const algebra::Mononode<R> *S_lm = S_red->leading_m();
            for (int k = 0; k < len; k++) {
                lm_lcms[len].push_back(polys_[k]->leading_m()->lcm(*S_lm)->hash);
                pq.push({len, k});
            }
            polys_.push_back(S_red);
        }

        S_computed[i][j] = true;
    }
    return !(stop_ && now() > stop_time_);
}

template<class R>
bool groebner::Reducer<R>::calculate_reduced_gbasis(int max_duration_ms) {
    stop_ = max_duration_ms > 0;
    stop_time_ = now() + std::chrono::milliseconds{max_duration_ms};

    bool finished = calculate_gbasis();
    size_t len = polys_.size();

    //std::cout << "Made basis of size " << len << std::endl;

    // Make the basis minimal
    std::vector<bool> divisible(len, false);
    for (size_t i = 0; i < len; i++) {
        for (size_t j = 0; j < i; j++) {
            // Try to divide polys_[i].lm() and polys_[j].lm() into each other

            // If one is divisible by the other, mark that one for deletion
            // If the leading terms are equal, we shouldn't remove both. Prioritize the first to be deleted
            if (polys_[i]->leading_m()->divisible(*polys_[j]->leading_m())) {
                divisible[i] = true;
            } else if (polys_[j]->leading_m()->divisible(*polys_[i]->leading_m())) {
                divisible[j] = true;
            }
        }
    }
    std::vector<Poly<R>*> min_basis;
    for (size_t i = 0; i < len; i++) {
        if (!divisible[i]) {
            min_basis.push_back(polys_[i]->scale(*node_store_.one_m(), 1 / polys_[i]->leading_c()));
        }
    }
    len = min_basis.size();
    //std::cout << "Minimized basis to size " << len << std::endl;

    // Turn the minimal basis into a reduced basis
    // https://pi.math.cornell.edu/~dmehrle/notes/old/alggeo/15BuchbergersAlgorithm.pdf
    //
    // Put the result in polys

    polys_.clear();
    polys_.reserve(len);
    //std::vector<Poly<R>*> red_basis;
    //red_basis.reserve(len);

    typename std::vector<Poly<R>*>::iterator after = min_basis.begin();
    for (size_t i = 0; i < len; i++) {
        after++;

        polys_.push_back(reduce(min_basis[i], after, min_basis.end(), polys_.begin(), polys_.end()));
    }

    return finished;
}

template<class R>
std::vector<groebner::Poly<R>*> groebner::Reducer<R>::get_polys() const {
    return polys_;
}

template class groebner::Reducer<mpq_class>;
