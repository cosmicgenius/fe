#include "../include/input.hpp"

#include <iostream>
#include <string>

typedef mpq_class R;

Input::Arg parse_arg(int argc, char** argv) {
    Input::Arg arg;
    
    for (int i = 0; i < argc; i++) {
        std::string flag = std::string(argv[i]);
        if (flag == "-no-groebner" || flag == "-ng") {
            arg.groebner = false;
        } else if (flag == "-no-pretty" || flag == "-np") {
            arg.pretty = false;
        } else if (flag == "-randomize" || flag == "-rand" || flag == "-r") {
            arg.randomize = true;
        }
    }

    return arg;
}

int main(int argc, char** argv) {
    Input::Arg arg = parse_arg(argc, argv);

    Input::InputHandler<R> handler(std::cin, std::cout, std::cerr, arg);
    handler.handle_input();
}

