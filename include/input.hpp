#include "algebra.hpp"

#include <gmpxx.h>
#include <sstream>

namespace Input {
struct Config {
    bool groebner = true; // Calculate Groebner basis?
    bool pretty = true; // Pretty print?
    bool randomize = false; // Randomize values and orders?
    int simplify = 0; // Degree of simplification:
                      // 0 = no attempt
                      // 1 = reorder variables
                      // 2 = 1 and plug in 0s
    int simplify_timeout = 60000; // Number of milliseconds to spend simplifying
    int batch_size = 1; // Number of batches
    int threads = 1; // Number of threads for simplification
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

    std::vector<std::stringstream> out_buffers_;
    std::vector<std::stringstream> err_buffers_;
    const Config config_;

    // Unordered maps are not thread-safe, so we cannot store nodes in a thread-safe way
    // Thus, each id has its own node store
    std::vector<algebra::NodeStore<R>> node_stores_;

    // Each id has a list of hypotheses
    std::vector<std::vector<const algebra::Polynode<R>*>> hypotheses_;

    const algebra::Polynode<R>* parse_polynode(int id, const std::string &input);

    // Return true if end
    bool eval(int id, std::string &cmd, std::string &rest);

    // Return true if end
    bool handle_line(int id, const std::string &input, int& line);

    void flush_buffers();

    // All of the following functions are NOT thread-safe
    // However, it is okay to run them concurrently IF they are 
    // run on different ids
    void take_input(int id); // Take another input (in the same batch)
    void echo_hypotheses(int id);
    void echo_rand_hypotheses(int id); 
    void clean_hypotheses(int id); // Gets rid of duplicate or 0 hypotheses
    void prepare_hypotheses(int id); // Prerares hypotheses for simplification
    void calc_groebner(int id);

public:
    InputHandler(std::istream &in, std::ostream &out, std::ostream &err, Config config = Config());

    // NOT thread-safe
    // Handles input config.batch_size times, then stops
    // Will hang if there are fewer than config.batch_size ends in the input
    void handle_input();
};
};
