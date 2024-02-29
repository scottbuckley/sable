#include "common.hh"
#include "kcobuchi.cc"

#include <spot/twaalgos/word.hh>

using namespace spot;
using namespace std;

bool letter_satisfies_cond(bdd letter, bdd cond) {
    return (bdd_apply(letter, cond, bddop_imp) == bddtrue);
}

// given a universal cobuchi graph, execute the given word's prefix, and
// return whether it ended up on an accepting state.
// if single_failure is true, it will fail as soon as it hits an accepting state.
bool prefix_hits_accepting(twa_graph_ptr g, twa_word_ptr w, bool print_path=false) {
    std::list<bdd> prefix = w->prefix;
    unsigned length = prefix.size();

    std::vector<unsigned> path_to_accepting_state;

    function<bool(unsigned, twa_word::seq_t::iterator, twa_word::seq_t::iterator)> run_from
    = [&](unsigned state, twa_word::seq_t::iterator wordbegin, twa_word::seq_t::iterator wordend) {

        // cout << "considering a " << *wordbegin << " at state " << state << endl;

        // if we hit an accepting state, return true
        if (is_accepting_state(g, state)) {
            // cout << "!! hit an accepting state !! " << state << endl;
            if (print_path) path_to_accepting_state.push_back(state);
            return true;
        }
        // we reached the end
        if (wordbegin == wordend) {
            // cout << "!! got to the end of the word!" << endl;
            return false;
        }

        bdd letter = *wordbegin;
        ++wordbegin; // now start looking at the rest of the word.

        for (auto e : g->out(state)) {
            if (letter_satisfies_cond(letter, e.cond)) {
                if (!g->is_univ_dest(e.dst)) {
                    if (run_from(e.dst, wordbegin, wordend)) {
                        if (print_path) path_to_accepting_state.push_back(state);
                        return true;
                    }
                } else {
                    for (auto d : g->univ_dests(e.dst))
                        if (run_from(d, wordbegin, wordend)) {
                            if (print_path) path_to_accepting_state.push_back(state);
                            return true;
                        }
                }
            }
        }
        return false;
    };

    auto b = prefix.begin();

    if (run_from(g->get_init_state_number(), prefix.begin(), prefix.end())) {
        if (print_path) {
            cout << "path to accepting state: ";
            for (auto it = path_to_accepting_state.rbegin(); it != path_to_accepting_state.rend(); ++it) { 
                cout << *it << " "; 
            } 
            cout << endl;
        }
        return true;
    }
    return false;
}