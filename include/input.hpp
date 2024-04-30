#include "algebra.hpp"

#include <gmpxx.h>

namespace Input {
struct Arg {
    bool groebner = true;
    bool pretty = true;
    bool randomize = false;
};

enum CMD_TYPE {
    hyp, // Add a hypothesis
    sub, // Substitute into a hypothesis
    app, // Apply f to both sides of a hypothesis after adding some polynode
    end,
};

template<class R>
class InputHandler {
private:
    std::istream &in_;
    std::ostream &out_;
    std::ostream &err_;
    const Arg opt_;

    algebra::NodeStore<R> node_store_;
    std::vector<const algebra::Polynode<R>*> hypotheses_;

    const algebra::Polynode<R>* parse_polynode(const std::string &input);

    // Return true if end
    bool eval(std::string &cmd, std::string &rest);

    // Return true if end
    bool handle_line(const std::string &input, int& line);
public:
    InputHandler(std::istream &in, std::ostream &out, std::ostream &err, Arg opt = Arg());
    void handle_input();
};
};
