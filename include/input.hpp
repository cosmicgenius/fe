#include "algebra.hpp"

enum CMD_TYPE {
    hyp, // Add a hypothesis
    sub, // Substitute into a hypothesis
    app, // Apply f to both sides of a hypothesis after adding some polynode
    end,
};

class InputHandler {
private:
    std::istream &in_;
    std::ostream &out_;
    std::ostream &err_;

    algebra::NodeStore<int> node_store_;
    std::vector<algebra::PolynodeHash> hypotheses_;

    const algebra::Polynode<int>* parse_polynode(const std::string &input);

    // Return true if end
    bool eval(std::string &cmd, std::string &rest);

    // Return true if end
    bool handle_line(const std::string &input, int& line);
public:
    InputHandler(std::istream &in, std::ostream &out, std::ostream &err);
    void handle_input();
};
