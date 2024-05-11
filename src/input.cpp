#include "../include/algebra.hpp"
#include "../include/groebner.hpp"
#include "../include/randomize.hpp"
#include "../include/input.hpp"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <istream>
#include <string>
#include <set>
#include <thread>
#include <unordered_map>
#include <utility>

template<class R>
Input::InputHandler<R>::InputHandler(std::istream &in, std::ostream &out, std::ostream &err, Config config) :
    in_(in), out_(out), err_(err), config_(config) {
    if (config.batch_size < 0) throw std::invalid_argument("Invalid number of ides");

    node_stores_.resize(config_.batch_size);
    hypotheses_.resize(config_.batch_size);

    out_buffers_.resize(config_.batch_size);
    err_buffers_.resize(config_.batch_size);
}

void clean(std::string &input) {
    // Remove spaces
    input.erase(std::remove(input.begin(), input.end(), ' '), input.end());

    // Remove asterisks (multiplication can be recognized from juxtaposition)
    input.erase(std::remove(input.begin(), input.end(), '*'), input.end());

    // To lower
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] >= 'A' && input[i] <= 'Z') input[i] += 'a' - 'A';
    }
}

template<class R>
R parse_coeff(const std::string &input) {
    return std::stoi(input);
}

template<>
mpq_class parse_coeff(const std::string &input) {
    if (!input.empty() && input[0] == '+') {
        return mpq_class(input.substr(1));
    }
    return mpq_class(input);
}

// Input must be cleaned to work
template<class R>
const algebra::Polynode<R>* Input::InputHandler<R>::parse_polynode(int id, const std::string &input) {
    // The input will be of the form c1 prod1 + c2 prod + ... + ck prodk
    // where each prodi is a product of stuff that a node, or parentheses around a polynode
    // and the ci are coefficients
    size_t len = input.size();
    const algebra::Polynode<R>* ans = node_stores_[id].zero_p();

    size_t last = 0, next = 0;
    int nested = 0;
    do {
        // Seek next until to capture a summand
        if (input[next] == '(') nested++;
        else if (input[next] == ')') throw std::invalid_argument("Failed to parse expression '" + input + "'. Unexpected ')'");
        next++;
        for (; next < len && !((input[next] == '+' || input[next] == '-') && nested == 0); next++) {
            if (input[next] == '(') nested++;
            else if (input[next] == ')') nested--;
        }

        // [last, next) is a summand

        // We always interpret the beginning as the coefficient
        R coeff = 1;

        size_t cur = last;
        for (; cur < next && input[cur] != 'x' && input[cur] != 'f' && input[cur] != '('; cur++);

        std::string coeff_str = input.substr(last, cur - last);
        if (coeff_str == "" || coeff_str == "+") coeff = 1;
        else if (coeff_str == "-") coeff = -1;
        else {
            try {
                coeff = parse_coeff<R>(coeff_str);
            } catch (const std::exception &e) {
                throw std::invalid_argument("Failed to parse expression '" + input + 
                        "'. Unable to parse '" + coeff_str + "', treated as coefficient.");
            }
        }

        std::unordered_map<algebra::NodeHash, int> mono_factors;

        const algebra::Polynode<R>* term = node_stores_[id].one_p();

        // Next, tokenize the factors
        while (cur < next) {
            last = cur;
            // Variable
            if (input[last] == 'x') {
                cur++;
                for (; cur < next && input[cur] != 'x' && input[cur] != 'f'; cur++);
                
                //std::cerr << " Variable: " << last << " " << cur << " " << input.substr(last, cur - last) << std::endl;

                std::string var_str = input.substr(last + 1, cur - last - 1);
                int var = 0;
                try {
                    var = std::stoi(var_str);
                } catch (const std::exception &e) {
                    throw std::invalid_argument("Failed to parse expression '" + input + 
                            "'. Unable to parse '" + input.substr(last, cur - last) + "', treated as variable.");
                }
                mono_factors[node_stores_[id].node(var)->hash]++;
            // f(polynode), so we must recurse
            } else if (input[last] == 'f') {
                cur++;
                if (input[cur++] != '(') {
                    throw std::invalid_argument("Failed to parse expression '" + input + "'. f should be followed by '('.");
                }

                // Seek until degree of nesting is 0
                int inner_nested = 1;
                for (; cur < next && inner_nested > 0; cur++) {
                    if (input[cur] == '(') inner_nested++;
                    else if (input[cur] == ')') inner_nested--;
                }

                //std::cerr << " Function: " << last << " " << cur << " " << input.substr(last, cur - last) << std::endl;
                
                std::string sub_polynode = input.substr(last + 2, cur - last - 3);
                mono_factors[node_stores_[id].node(
                                parse_polynode(id, sub_polynode)->hash
                             )->hash]++;
            // (polynode), again we must recurse
            } else if (input[last] == '(') {
                cur++;

                int inner_nested = 1;
                for (; cur < next && inner_nested > 0; cur++) {
                    if (input[cur] == '(') inner_nested++;
                    else if (input[cur] == ')') inner_nested--;
                }

                std::string sub_polynode = input.substr(last + 1, cur - last - 2);
                term = *term * *parse_polynode(id, sub_polynode);
            } else {
                throw std::invalid_argument("Failed to parse expression '" + input + 
                        "'. Invalid factor starting with '" + input[last] + "'.");
            }
        }
        term = *term * *node_stores_[id].polynode({{ node_stores_[id].mononode(mono_factors)->hash, coeff }});
        ans = *ans + *term;

        last = next;
    } while (next < len);

    return ans;
}

// Return true if end
template<class R>
bool Input::InputHandler<R>::eval(int id, std::string &cmd, std::string &rest) {
    CMD_TYPE cmd_type;
    if (cmd == "hyp") cmd_type = CMD_TYPE::hyp;
    else if (cmd == "h") cmd_type = CMD_TYPE::hyp;
    else if (cmd == "sub") cmd_type = CMD_TYPE::sub;
    else if (cmd == "s") cmd_type = CMD_TYPE::sub;
    else if (cmd == "app") cmd_type = CMD_TYPE::app;
    else if (cmd == "a") cmd_type = CMD_TYPE::app;
    else if (cmd == "end") cmd_type = CMD_TYPE::end;
    else if (cmd == "e") cmd_type = CMD_TYPE::end;
    else throw std::invalid_argument("Invalid command " + cmd);

    switch (cmd_type) {
        case CMD_TYPE::hyp: {
            clean(rest);
            std::vector<const algebra::Polynode<R>*> parts;
            size_t last = 0, next;
            do {
                next = rest.find_first_of('=', last);
                parts.push_back(parse_polynode(id, rest.substr(last, next)));

                last = next + 1;
            } while (next != std::string::npos);

            if (parts.size() == 1) {
                hypotheses_[id].push_back(parts[0]);
            } else {
                for (size_t i = 1; i < parts.size(); i++) {
                    hypotheses_[id].push_back(*parts[i] - *parts[0]);
                }
            }

            break;
        }
        case CMD_TYPE::sub: {
            size_t last = 0;
            if (rest.at(last) != 'h') throw std::invalid_argument("Invalid sub command '" + rest + "'");
            size_t split = rest.find_first_of(' ');

            int hypo = std::stoi(rest.substr(last + 1, split));
            const algebra::Polynode<R>* primal = hypotheses_[id].at(hypo - 1);

            last = split + 1;
            if (rest.at(last) != 'x') throw std::invalid_argument("Invalid sub command '" + rest + "'");
            split = rest.find_first_of(' ', last);

            int var = std::stoi(rest.substr(last + 1, split));

            last = split + 1;

            std::string polynode_str = rest.substr(last);
            clean(polynode_str);

            hypotheses_[id].push_back(primal->sub(
                        var, *parse_polynode(id, polynode_str)
                    ));
            break;
        }
        case CMD_TYPE::app: {
            size_t last = 0;
            if (rest.at(last) != 'h') throw std::invalid_argument("Invalid app command '" + rest + "'");
            size_t split = rest.find_first_of(' ');

            int hypo = std::stoi(rest.substr(last + 1, split));
            const algebra::Polynode<R>* primal = hypotheses_[id].at(hypo - 1);

            last = split + 1;

            std::string polynode_str = rest.substr(last);
            clean(polynode_str);

            hypotheses_[id].push_back(primal->apply_func(
                        *parse_polynode(id, polynode_str)
                    ));
            break;
        }
        case CMD_TYPE::end: return true;
    }

    return false;
}

// Return true if end
template<class R>
bool Input::InputHandler<R>::handle_line(int id, const std::string &input, int& line) {
    size_t split = input.find(" ");

    std::string cmd = input.substr(0, split);
    std::string rest = input.substr(split + 1);

    line++;
    try {
        return eval(id, cmd, rest);
    } catch (const std::exception &e) {
        line--;
        err_ << " Error: " << e.what() << std::endl;
    } 
    return false;
}

template<class R>
void Input::InputHandler<R>::take_input(int id) {
    std::string input;
    int idx = 1;
    do {
        // Needs to print immediately, so no buffers
        if (config_.pretty) out_ << "h" << idx << ": " << std::flush;
        std::getline(in_, input);
    } while (!handle_line(id, input, idx));
}

template<class R>
void Input::InputHandler<R>::echo_hypotheses(int id) {
    if (config_.pretty) out_buffers_[id] << "Hypotheses:" << std::endl;
    int idx = 1;
    for (const algebra::Polynode<R>* const h : hypotheses_[id]) {
        if(config_.pretty) out_buffers_[id] << "h" << idx++ << ": ";
        out_buffers_[id] << h->to_string() << std::endl;
    }
}

template<class R>
void Input::InputHandler<R>::echo_rand_hypotheses(int id) {
    randomize::Randomizer<R> randomizer(node_stores_[id]);

    if (config_.pretty) out_buffers_[id] << "Randomized hypotheses:" << std::endl;
    int idx = 1;
    for (const algebra::Polynode<R>* const h : hypotheses_[id]) {
        if(config_.pretty) out_buffers_[id] << "h" << idx++ << ": ";
        out_buffers_[id] << randomizer.to_random_string(*h, true) << std::endl;
    }
}

// Quick and dirty function to get the variables in a polynode
// TODO: make this better
template<class R>
std::set<algebra::Idx> get_vars(const algebra::Polynode<R>& p, algebra::NodeStore<R> &node_store) {
    std::set<algebra::Idx> vars;

    for (const auto& term : p) {
        for (const auto &node : *node_store.get_mononode(term.first)) {
            const algebra::Node<R>* nptr = node_store.get_node(node.first);

            switch (nptr->get_type()) {
                case algebra::NodeType::VAR: {
                    vars.insert(nptr->get_var());
                    break;
                }
                case algebra::NodeType::POL: {
                    std::set<algebra::Idx> v = get_vars(*node_store.get_polynode(nptr->get_polynode_hash()), node_store);
                    vars.insert(v.begin(), v.end());
                }
            }
        }
    }
    return vars;
}

template<class R>
void Input::InputHandler<R>::clean_hypotheses(int id) {
    // Remove duplicates
    std::set<const algebra::Polynode<R>*> reduced_hypotheses(hypotheses_[id].begin(), hypotheses_[id].end());
    // Delete zeros
    auto it = reduced_hypotheses.find(node_stores_[id].zero_p());
    if (it != reduced_hypotheses.end()) reduced_hypotheses.erase(it);
    hypotheses_[id] = std::vector<const algebra::Polynode<R>*>(reduced_hypotheses.begin(), reduced_hypotheses.end());
}

template<class R>
void Input::InputHandler<R>::prepare_hypotheses(int id) {
    clean_hypotheses(id);

    // Substitute zeros
    if (config_.simplify >= 1) {
        std::vector<const algebra::Polynode<R>*> sub_zero;
        for (const algebra::Polynode<R>* h : hypotheses_[id]) {
            // Again slow, but that's ok (probably). TODO
            std::set<algebra::Idx> vars = get_vars(*h, node_stores_[id]);

            for (int mask = 1; mask < (1 << vars.size()); mask++) {
                int mask_copy = mask;
                std::unordered_set<algebra::Idx> zero_out;
                zero_out.reserve(vars.size());
                for (const algebra::Idx v : vars) {
                    if (mask_copy & 1) {
                        zero_out.insert(v);
                    } 
                    mask_copy >>= 1;
                }   
                sub_zero.push_back(h->subs_zero(zero_out));
            }
        }
        
        hypotheses_[id].insert(hypotheses_[id].end(), sub_zero.begin(), sub_zero.end());
        clean_hypotheses(id);
    }
    // Permute the variables (and "push" them down to the n smallest indexes)
    if (config_.simplify >= 2) {
        std::vector<const algebra::Polynode<R>*> permuted;
        for (const algebra::Polynode<R>* h : hypotheses_[id]) {
            // Again slow, but that's ok (probably). TODO
            std::set<algebra::Idx> vars_set = get_vars(*h, node_stores_[id]);
            std::vector<algebra::Idx> vars_idx(vars_set.begin(), vars_set.end());
            std::vector<algebra::Idx> new_idx(vars_set.size()); 
            std::iota(new_idx.begin(), new_idx.end(), 1);

            do {
                std::unordered_map<algebra::Idx, algebra::Idx> replace;
                replace.reserve(vars_idx.size());
                
                for (size_t i = 0; i < vars_idx.size(); i++) {
                    replace[vars_idx[i]] = new_idx[i];
                }

                permuted.push_back(h->subs_var(replace));
            } while (std::next_permutation(new_idx.begin(), new_idx.end()));
        }
        
        hypotheses_[id].insert(hypotheses_[id].end(), permuted.begin(), permuted.end());
        clean_hypotheses(id);
    }

    if (config_.pretty) out_buffers_[id] << "Substituted to obtain the following hypotheses (simplification level = " << 
        config_.simplify << "):" << std::endl;
    else out_buffers_[id] << std::endl;
    int idx = 1;
    for (const algebra::Polynode<R>* const h : hypotheses_[id]) {
        if(config_.pretty) out_buffers_[id] << "s" << idx++ << ": ";
        out_buffers_[id] << h->to_string() << std::endl;
    }
}

template<class R>
void Input::InputHandler<R>::calc_groebner(int id) {
    if (config_.pretty) out_buffers_[id] << "Calculating Groebner basis ..." << std::endl;
    groebner::Reducer<R> reducer(hypotheses_[id], node_stores_[id]);
    bool finished = reducer.calculate_reduced_gbasis(config_.simplify_timeout);
    std::vector<const algebra::Polynode<R>*> gbasis = reducer.get_polys();
    std::sort(gbasis.begin(), gbasis.end(), [](const algebra::Polynode<R>* a, const algebra::Polynode<R>* b) {
                return a->stats.weight < b->stats.weight;
            });

    if (config_.pretty) {
        if (finished) {
            out_buffers_[id] << "Finished." << std::endl;
            out_buffers_[id] << "Reduced Groebner basis:" << std::endl;
        } else {
            out_buffers_[id] << "Terminated after " << config_.simplify_timeout << "ms." << std::endl;
            out_buffers_[id] << "Reduced partial Groebner basis:" << std::endl;
        }
    } 
    else out_buffers_[id] << std::endl;
    int idx = 1;
    for (const algebra::Polynode<R>* const h : gbasis) {
        if(config_.pretty) out_buffers_[id] << "b" << idx++ << " ";
        out_buffers_[id] << "[" << h->stats << "]";
        if (config_.pretty) out_buffers_[id] << ":";
        out_buffers_[id] << " ";
        out_buffers_[id] << h->to_string() << std::endl;
    }
}

template<class R>
void Input::InputHandler<R>::flush_buffers() {
    for (std::stringstream &ss : out_buffers_) {
        out_ << ss.str();

        ss.clear();
        ss.str(std::string());
    }

    for (std::stringstream &ss : err_buffers_) {
        err_ << ss.str();

        ss.clear();
        ss.str(std::string());
    }
}

template<class R>
void Input::InputHandler<R>::handle_input() {
    // Not concurrent because they come in order
    // Also this is relatively fast, so it is ok
    for (int id = 0; id < config_.batch_size; id++) {
        take_input(id);
    }

    // See above
    for (int id = 0; id < config_.batch_size; id++) {
        echo_hypotheses(id);
    }
    flush_buffers();

    if (config_.randomize) {
        for (int id = 0; id < config_.batch_size; id++) {
            echo_rand_hypotheses(id);
        }
        flush_buffers();
    }

    if (config_.groebner) {
        if (config_.threads > 1) {
            std::vector<std::thread> threads;
            std::atomic_int id{0};

            auto do_work = [this] (int id) -> bool {
                if (id >= config_.batch_size) return false;
                
                prepare_hypotheses(id);
                calc_groebner(id);

                return true;
            };

            for (int t_id = 0; t_id < config_.threads; t_id++) {
                threads.push_back(std::thread([do_work, &id] () { 
                    while (do_work(id++));
                }));
            }
            for (std::thread &t : threads) t.join();
            std::cout << "Done." << std::endl;
        } else {
            for (int id = 0; id < config_.batch_size; id++) {
                prepare_hypotheses(id);
                calc_groebner(id);
            }
        }
    }
    flush_buffers();
}

//template class Input::InputHandler<int>;
template class Input::InputHandler<mpq_class>;
