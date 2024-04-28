#include "../include/input.hpp"

#include <iostream>
#include <string>

typedef mpq_class R;

struct Arg {
    bool groebner = true;
    bool pretty = true;
};

Arg parse_arg(int argc, char** argv) {
    Arg res;
    
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "-no-groebner" || std::string(argv[i]) == "-ng"){
            res.groebner = false;
        } else if (std::string(argv[i]) == "-no-pretty" || std::string(argv[i]) == "-np"){
            res.pretty = false;
        }
    }

    return res;
}

int main(int argc, char** argv) {
    Arg arg = parse_arg(argc, argv);

    InputHandler<R> ih(std::cin, std::cout, std::cerr);
    ih.handle_input(arg.groebner, arg.pretty);
}

