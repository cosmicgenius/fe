#include "../include/input.hpp"

#include <iostream>
#include <string>

typedef mpq_class R;

const std::string truthies[] = { "true", "1", "yes" };

bool truthy(std::string s) {
    // To lowercase
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] += 'a' - 'A';
    }

    for (const std::string &t : truthies) {
        if (s == t) return true;
    }
    return false;
}

Input::Config parse_arg(int argc, char** argv) {
    Input::Config config;
    
    // Support only --key=value arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = std::string(argv[i]);

        if (arg.substr(0, 2) != "--") {
            throw std::invalid_argument("Invalid argument: " + arg);
        } 
        int split = arg.find_first_of('=');
        std::string key = arg.substr(2, split - 2);
        std::string val = arg.substr(split + 1);
        
        if (key == "groebner") {
            config.groebner = truthy(val);
        } else if (key == "pretty") {
            config.pretty = truthy(val);
        } else if (key == "randomize" || key == "rand") {
            config.randomize = truthy(val);
        } else if (key == "simplify" || key == "simp") {
            config.simplify = std::stoi(val);
        } else if (key == "simplify_timeout" || key == "simp_timeout") {
            config.simplify_timeout = std::stoi(val);
        } else if (key == "batch_size") {
            config.batch_size = std::stoi(val);
        } else if (key == "threads") {
            config.threads = std::stoi(val);
        } else {
            std::cerr << "Warninig: Unknown argument '" << key << "'" << std::endl;
        }
    }

    return config;
}

int main(int argc, char** argv) {
    Input::Config config = parse_arg(argc, argv);

    Input::InputHandler<R> handler(std::cin, std::cout, std::cerr, config);
    handler.handle_input();
}

