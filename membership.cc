#pragma once

#include "common.hh"
#include "kcobuchi.cc"
#include "words.cc"

using namespace spot;
using namespace std;

#define print_path false

inline bool letter_satisfies_cond(const bdd & letter, const bdd & cond) {
    return (bdd_apply(letter, cond, bddop_imp) == bddtrue);
}

typedef std::unordered_set<unsigned> walk_position;

class UCBWalker {
private:
    twa_graph_ptr g;
    walk_position cur_states;
    walk_position temp_cur_states;
    bool has_failed = false;
    std::vector<bool> accepting_state_cache;

    bool check_accepting_state_nocache(const unsigned state) const {
        for (auto& t: g->out(state)) {
            for (auto v: t.acc.sets())
                return true;
            break;
        }
        return false;
    }

    void init_accepting_state_cache() {
        accepting_state_cache.resize(g->num_states(), false);
        for (unsigned i=0; i<g->num_states(); ++i) {
            accepting_state_cache[i] = check_accepting_state_nocache(i);
        }
    }

    const inline bool is_accepting_state(const unsigned state) {
        return accepting_state_cache[state];
    }

    // return true (and abort) if you hit an accepting state
    bool try_step(const bdd & letter) {
        temp_cur_states.clear();
        for (unsigned state : cur_states) {
            for (auto edge : g->out(state)) {
                if (letter_satisfies_cond(letter, edge.cond)) {
                    if (!g->is_univ_dest(edge.dst)) {
                        if (is_accepting_state(edge.dst)) return true;
                        temp_cur_states.insert(edge.dst);
                    } else {
                        for (auto d : g->univ_dests(edge.dst)) {
                            if (is_accepting_state(d)) return true;
                            temp_cur_states.insert(d);
                        }
                    }
                }
            }
        }
        // swap the new state over to the active collection.
        temp_cur_states.swap(cur_states);
        return false;
    }

    bool try_walk(finite_word_ptr word) {
        for (bdd letter : *word)
            if (try_step(letter))
                return true;
        return false;
    }
public:
    UCBWalker(twa_graph_ptr g) {
        this->g = g;
        const unsigned init_state = g->get_init_state_number();
        cur_states.insert(init_state);
        init_accepting_state_cache();
        // if the initial state is accepting, we have already failed.
        has_failed = is_accepting_state(init_state);
    }

    bool failed() {
        return has_failed;
    }

    bool step(const bdd & letter) {
        if (has_failed) throw std::logic_error("you can't continue stepping after you have hit an accepting state.");
        return (has_failed = try_step(letter));
    }

    bool walk(finite_word_ptr word) {
        if (has_failed) throw std::logic_error("you can't continue stepping after you have hit an accepting state.");
        if (try_walk(word)) {
            has_failed = true;
            return true;
        }
        return false;
    }

    void reset() {
        has_failed = is_accepting_state(g->get_init_state_number());
        cur_states.clear();
    }

    // store the current configuration to be later restored.
    walk_position get_position() {
        if (has_failed) {
            throw std::logic_error("you can't save a position after hitting an accepting state.");
        } else {
            return cur_states;
        }
    }

    void set_position(walk_position new_position) {
        // if (cur_states_stored.empty()) throw std::logic_error("you can't load a position before saving one.");
        cur_states = new_position;
        has_failed = false;
    }
};

// given a universal cobuchi graph, execute the given word's prefix, and
// return whether it ended up on an accepting state.
// if single_failure is true, it will fail as soon as it hits an accepting state.
// bool fword_hits_accepting(twa_graph_ptr g, finite_word_ptr fword) {
//     std::vector<unsigned> path_to_accepting_state;

//     function<bool(unsigned, finite_word::iterator, finite_word::iterator)> run_from
//     = [&](unsigned state, finite_word::iterator wordbegin, finite_word::iterator wordend) {

//         // if we hit an accepting state, return true
//         if (is_accepting_state(g, state)) {
//             // cout << "!! hit an accepting state !! " << state << endl;
//             if (print_path) path_to_accepting_state.push_back(state);
//             return true;
//         }
//         // we reached the end
//         if (wordbegin == wordend) {
//             // cout << "!! got to the end of the word!" << endl;
//             return false;
//         }

//         bdd letter = *wordbegin;
//         ++wordbegin; // now start looking at the rest of the word.

//         for (auto e : g->out(state)) {
//             if (letter_satisfies_cond(letter, e.cond)) {
//                 if (!g->is_univ_dest(e.dst)) {
//                     if (run_from(e.dst, wordbegin, wordend)) {
//                         if (print_path) path_to_accepting_state.push_back(state);
//                         return true;
//                     }
//                 } else {
//                     for (auto d : g->univ_dests(e.dst))
//                         if (run_from(d, wordbegin, wordend)) {
//                             if (print_path) path_to_accepting_state.push_back(state);
//                             return true;
//                         }
//                 }
//             }
//         }
//         return false;
//     };

//     if (run_from(g->get_init_state_number(), fword->begin(), fword->end())) {
//         if (print_path) {
//             cout << "path to accepting state: ";
//             for (auto it = path_to_accepting_state.rbegin(); it != path_to_accepting_state.rend(); ++it) { 
//                 cout << *it << " "; 
//             } 
//             cout << endl;
//         }
//         return true;
//     }
//     return false;
// }

// check a simple word
bool member(finite_word_ptr fword, twa_graph_ptr g) {
    UCBWalker * walk = new UCBWalker(g);
    for (auto l : *fword) {
        if (walk->step(l)) return false;
    }
    return true;
}

bool member(finite_word_ptr prefix, bdd middle, finite_word_ptr suffix, twa_graph_ptr g) {
    UCBWalker * walk = new UCBWalker(g);
    for (auto l : *prefix)
        if (walk->step(l)) return false;
    if (walk->step(middle)) return false;
    for (auto l : *suffix)
        if (walk->step(l)) return false;
    return true;
}

UCBWalker * make_walker(twa_graph_ptr g) {
    return new UCBWalker(g);
}

// bool member(finite_word_ptr fwordprefix, finite_word_ptr fwordsuffix, twa_graph_ptr g) {
//     return !fword_hits_accepting(g, fwordprefix, fwordsuffix);
// }