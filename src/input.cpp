#include "../include/algebra.hpp"
#include "../include/groebner.hpp"
#include "../include/randomize.hpp"
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
const algebra::Polynode<R>* Input::InputHandler<R>::parse_polynode(const std::string &input) {
    // The input will be of the form c1 prod1 + c2 prod + ... + ck prodk
    // where each prodi is a product of stuff that a node, or parentheses around a polynode
    // and the ci are coefficients
    size_t len = input.size();
    const algebra::Polynode<R>* ans = this->node_store_.zero_p();

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

        const algebra::Polynode<R>* term = this->node_store_.one_p();

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
                mono_factors[this->node_store_.node(var)->hash]++;
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
                mono_factors[this->node_store_.node(
                                this->parse_polynode(sub_polynode)->hash
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
                term = *term * *this->parse_polynode(sub_polynode);
            } else {
                throw std::invalid_argument("Failed to parse expression '" + input + 
                        "'. Invalid factor starting with '" + input[last] + "'.");
            }
        }
        term = *term * *this->node_store_.polynode({{ this->node_store_.mononode(mono_factors)->hash, coeff }});
        ans = *ans + *term;

        last = next;
    } while (next < len);

    return ans;
}

// Return true if end
template<class R>
bool Input::InputHandler<R>::eval(std::string &cmd, std::string &rest) {
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
                parts.push_back(this->parse_polynode(rest.substr(last, next)));

                last = next + 1;
            } while (next != std::string::npos);

            if (parts.size() == 1) {
                this->hypotheses_.push_back(parts[0]);
            } else {
                for (size_t i = 1; i < parts.size(); i++) {
                    this->hypotheses_.push_back(*parts[i] - *parts[0]);
                }
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
bool Input::InputHandler<R>::handle_line(const std::string &input, int& line) {
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
Input::InputHandler<R>::InputHandler(std::istream &in, std::ostream &out, std::ostream &err, Arg opt) :
    in_(in), out_(out), err_(err), opt_(opt) {
    this->node_store_ = algebra::NodeStore<R>();
}

template<class R>
void Input::InputHandler<R>::handle_input() {
    std::string input;
    int idx = 1;
    do {
        if (opt_.pretty) this->out_ << "h" << idx << ": " << std::flush;
        std::getline(this->in_, input);
    } while (!handle_line(input, idx));

    randomize::Randomizer<R> randomizer(this->node_store_);

    if (opt_.pretty) this->out_ << "Hypotheses:" << std::endl;
    idx = 1;
    for (const algebra::Polynode<R>* const h : this->hypotheses_) {
        if(opt_.pretty) this->out_ << "h" << idx++ << ": ";
        this->out_ << (opt_.randomize ? randomizer.to_random_string(*h, true) : h->to_string()) << std::endl;
    }

    if (!opt_.groebner) return;
    
    // Remove duplicates
    std::set<const algebra::Polynode<R>*> reduced_hypotheses(this->hypotheses_.begin(), this->hypotheses_.end());
    // Delete zeros
    auto it = reduced_hypotheses.find(this->node_store_.zero_p());
    if (it != reduced_hypotheses.end()) reduced_hypotheses.erase(it);
    this->hypotheses_ = std::vector<const algebra::Polynode<R>*>(reduced_hypotheses.begin(), reduced_hypotheses.end());

    groebner::Reducer<R> reducer(this->node_store_);
    std::vector<const algebra::Polynode<R>*> gbasis = reducer.reduced_basis(this->hypotheses_);

    if (opt_.pretty) this->out_ << "Reduced Groebner basis:" << std::endl;
    else this->out_ << std::endl;
    idx = 1;
    for (const algebra::Polynode<R>* const h : gbasis) {
        if(opt_.pretty) this->out_ << "b" << idx++ << ": ";
        this->out_ << h->to_string() << std::endl;
    }
}

//template class Input::InputHandler<int>;
template class Input::InputHandler<mpq_class>;
