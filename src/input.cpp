#include "../include/algebra.hpp"
#include "../include/groebner.hpp"
#include "../include/input.hpp"

#include <algorithm>
#include <iostream>
#include <istream>
#include <string>
#include <utility>

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
const algebra::Polynode<R>* InputHandler<R>::parse_polynode(const std::string &input) {
    //std::cerr << input << std::endl;
    size_t len = input.size();
    std::unordered_map<algebra::MononodeHash, R> summands;

    size_t last = 0, next = 0;
    int nested = 0;
    do {
        next++;
        for (; next < len && !((input[next] == '+' || input[next] == '-') && nested == 0); next++) {
            if (input[next] == '(') nested++;
            else if (input[next] == ')') nested--;
        }

        //std::cerr << " Summand: " << next << " " << last << " " << input.substr(last, next - last) << std::endl;

        // [last, next) is a summand

        // First thing is the coefficient
        R coeff = 1;

        size_t cur = last;
        for (; cur < next && input[cur] != 'x' && input[cur] != 'f'; cur++);

        std::string coeff_str = input.substr(last, cur - last);
        if (coeff_str == "" || coeff_str == "+") coeff = 1;
        else if (coeff_str == "-") coeff = -1;
        else coeff = parse_coeff<R>(coeff_str);

        std::unordered_map<algebra::NodeHash, int> factors;

        // Next, tokenize the factors
        while (cur < next) {
            last = cur;
            // Variable
            if (input[last] == 'x') {
                cur++;
                for (; cur < next && input[cur] != 'x' && input[cur] != 'f'; cur++);
                
                //std::cerr << " Variable: " << last << " " << cur << " " << input.substr(last, cur - last) << std::endl;

                std::string var_str = input.substr(last + 1, cur - last);
                factors[this->node_store_.node(std::stoi(var_str))->hash]++;
            // f(polynode), so we must recurse
            } else if (input[last] == 'f') {
                cur++;
                if (input[cur++] != '(') {
                    throw std::invalid_argument("Failed to parse expression '" + input + "'");
                }

                int inner_nested = 1;
                for (; cur < next && inner_nested > 0; cur++) {
                    if (input[cur] == '(') inner_nested++;
                    else if (input[cur] == ')') inner_nested--;
                }

                //std::cerr << " Function: " << last << " " << cur << " " << input.substr(last, cur - last) << std::endl;
                
                std::string sub_polynode = input.substr(last + 2, cur - last - 3);
                factors[this->node_store_.node(
                        this->parse_polynode(sub_polynode)->hash
                    )->hash]++;
            } else {
                throw std::invalid_argument("Failed to parse expression '" + input + "'");
            }
        }
        summands[this->node_store_.mononode(factors)->hash] += coeff;
        last = next;
    } while (next < len);

    std::vector<std::pair<algebra::MononodeHash, R>> summands_vec;
    summands_vec.reserve(summands.size());
    // Remove zeros
    for (auto it = summands.begin(); it != summands.end(); it++) {
        if (it->second != 0) summands_vec.emplace_back(std::move(*it));
    }

    return this->node_store_.polynode(summands_vec);
}

// Return true if end
template<class R>
bool InputHandler<R>::eval(std::string &cmd, std::string &rest) {
    CMD_TYPE cmd_type;
    if (cmd == "hyp") cmd_type = CMD_TYPE::hyp;
    else if (cmd == "sub") cmd_type = CMD_TYPE::sub;
    else if (cmd == "app") cmd_type = CMD_TYPE::app;
    else if (cmd == "end") cmd_type = CMD_TYPE::end;
    else throw std::invalid_argument("Invalid command " + cmd);

    switch (cmd_type) {
        case CMD_TYPE::hyp: {
            clean(rest);
            size_t split = rest.find_first_of('=');

            if (split == std::string::npos) {
                // No equal is implicitly rest = 0
                this->hypotheses_.push_back(this->parse_polynode(rest));
            } else {
                const algebra::Polynode<R> *lhs = this->parse_polynode(rest.substr(0, split)),
                                           *rhs = this->parse_polynode(rest.substr(split + 1));
                this->hypotheses_.push_back(*lhs - *rhs);
            }

            break;
        }
        case CMD_TYPE::sub: {
            size_t last = 0;
            if (rest.at(last) != 'h') throw std::invalid_argument("Invalid sub command '" + rest + "'");
            size_t split = rest.find_first_of(' ');

            int hypo = std::stoi(rest.substr(last + 1, split));
            const algebra::Polynode<R>* primal = this->hypotheses_.at(hypo - 1);

            last = split + 1;
            if (rest.at(last) != 'x') throw std::invalid_argument("Invalid sub command '" + rest + "'");
            split = rest.find_first_of(' ', last);

            int var = std::stoi(rest.substr(last + 1, split));

            last = split + 1;

            std::string polynode_str = rest.substr(last);
            clean(polynode_str);

            this->hypotheses_.push_back(primal->sub(
                        var, *this->parse_polynode(polynode_str)
                    ));
            break;
        }
        case CMD_TYPE::app: {
            size_t last = 0;
            if (rest.at(last) != 'h') throw std::invalid_argument("Invalid app command '" + rest + "'");
            size_t split = rest.find_first_of(' ');

            int hypo = std::stoi(rest.substr(last + 1, split));
            const algebra::Polynode<R>* primal = this->hypotheses_.at(hypo - 1);

            last = split + 1;

            std::string polynode_str = rest.substr(last);
            clean(polynode_str);

            this->hypotheses_.push_back(primal->apply_func(
                        *this->parse_polynode(polynode_str)
                    ));
            break;
        }
        case CMD_TYPE::end: return true;
    }

    return false;
}

// Return true if end
template<class R>
bool InputHandler<R>::handle_line(const std::string &input, int& line) {
    size_t split = input.find(" ");

    std::string cmd = input.substr(0, split);
    std::string rest = input.substr(split + 1);

    line++;
    try {
        return eval(cmd, rest);
    } catch (const std::exception &e) {
        line--;
        this->err_ << " Error: " << e.what() << std::endl;
    } 
    return false;
}

template<class R>
InputHandler<R>::InputHandler(std::istream &in, std::ostream &out, std::ostream &err) :
    in_(in), out_(out), err_(err) {
    this->node_store_ = algebra::NodeStore<R>();
}

template<class R>
void InputHandler<R>::handle_input() {
    std::string input;
    int idx = 1;
    do {
        this->out_ << "h" << idx << ": " << std::flush;
        std::getline(this->in_, input);
    } while (!handle_line(input, idx));

    this->out_ << "Hypotheses:" << std::endl;
    idx = 1;
    for (const algebra::Polynode<R>* const h : this->hypotheses_) {
        this->out_ << "h" << idx++ << ": " << h->to_string() << std::endl;
    }

    groebner::Reducer<R> reducer(this->node_store_);
    std::vector<const algebra::Polynode<R>*> gbasis = reducer.reduced_basis(this->hypotheses_);

    this->out_ << "Reduced Groebner basis:" << std::endl;
    idx = 1;
    for (const algebra::Polynode<R>* const h : gbasis) {
        this->out_ << "b" << idx++ << ": " << h->to_string() << std::endl;
    }
}

//template class InputHandler<int>;
template class InputHandler<mpq_class>;
