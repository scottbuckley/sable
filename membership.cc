/*
    Note: this is build for universal cobuchi automaton, and further assumes that
    all accepting states are sink states. As soon as any accepting state is hit, we
    consider the word to be accepting (and therefore not a member).
*/


#pragma once

#include "common.hh"
#include "kcobuchi.cc"
#include "words.cc"
#include "accepting_state_cache.cc"

using namespace spot;
using namespace std;

#define print_path false

// NOTE: we have that the letter "a & !b" satisfies the condition "a", but
// we DO NOT HAVE that the letter "a" (which is not really a letter)
// satisfies the condition "a & !b".
inline bool letter_satisfies_cond(const bdd & letter, const bdd & cond) {
    // cout << "letter: " << letter << ", cond: " << cond << " = ";
    const bool result = bdd_apply(letter, cond, bddop_imp) == bddtrue;
    // cout << result << endl;
    return (result);
}

//TODO: change this to a bool vector of the number of states in the graph. that would be faster.
typedef std::unordered_set<unsigned> walk_position;


class UCBWalker {
private:
    twa_graph_ptr g;
    AcceptingStateCache accepting_state_cache;
    walk_position cur_states;
    walk_position temp_cur_states;
    bool has_failed = false;

    unsigned bad_membership_count = 0;
    unsigned membership_letter_count = 0;

    // if we hit an accepting state, there's no point keeping track of anything else.
    // so we just set the cur_states list to be only the failing state.
    void mark_failed_at_state(unsigned state) {
        cur_states.clear();
        cur_states.insert(state);
        has_failed = true;
    }

    // return true (and abort) if you hit an accepting state
    bool try_step(const bdd & letter) {
        ++membership_letter_count;
        temp_cur_states.clear();
        for (unsigned state : cur_states) {
            for (auto edge : g->out(state)) {
                if (letter_satisfies_cond(letter, edge.cond)) {
                    if (!g->is_univ_dest(edge.dst)) {
                        if (accepting_state_cache.accepting(edge.dst)) {
                            mark_failed_at_state(edge.dst);
                            return true;
                        }
                        temp_cur_states.insert(edge.dst);
                    } else {
                        for (auto d : g->univ_dests(edge.dst)) {
                            if (accepting_state_cache.accepting(d)) {
                                mark_failed_at_state(d);
                                return true;
                            }
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

    bool check_if_failed() {
        for (unsigned s : cur_states) {
            if (accepting_state_cache.accepting(s)) {
                has_failed = true;
                return true;
            }
        }
        has_failed = false;
        return false;
    }
public:
    void reset() {
        // initialise the set of current states to just the initial state.
        cur_states.clear();
        cur_states.insert(g->get_init_state_number());
        
        // if the initial state is accepting, we have already failed.
        check_if_failed();
    }

    UCBWalker(twa_graph_ptr g) : g{g}, accepting_state_cache(g) {
        reset();
    }

    bool failed() {
        ++bad_membership_count;
        return has_failed;
    }

    void step(const bdd & letter) {
        // if (has_failed) throw std::logic_error("you can't continue stepping after you have hit an accepting state.");
        if (has_failed) return;
        has_failed = try_step(letter);
    }

    bool step_and_check_failed(const bdd & letter) {
        step(letter);
        return failed();
    }

    // take a step, but talk loudly about it while you do it
    bool step_verbose(const bdd & letter) {
        if (has_failed) {
            cout << "wanted to take a step, but we have already failed";
            return true;
        }
        cout << "about to take step with letter " << letter << endl;
        cout << "current states: "; print_vector(cur_states);
        
        return (has_failed = try_step(letter));
    }

    bool walk(finite_word_ptr word) {
        // if (has_failed) throw std::logic_error("you can't continue stepping after you have hit an accepting state.");
        if (has_failed) return true;
        if (try_walk(word)) {
            has_failed = true;
            return true;
        }
        return false;
    }

    // store the current configuration to be later restored.
    walk_position get_position() {
        return cur_states;
    }

    void set_position(walk_position new_position) {
        cur_states = new_position;
        check_if_failed();
    }

    unsigned get_bad_membership_count() {
        return bad_membership_count;
    }

    unsigned get_membership_letter_count() {
        return membership_letter_count;
    }
};

// check a simple word
bool member(finite_word_ptr fword, twa_graph_ptr g) {
    UCBWalker * walk = new UCBWalker(g);
    for (auto l : *fword) {
        if (walk->step_and_check_failed(l)) return false;
    }
    return true;
}

bool member(finite_word_ptr prefix, bdd middle, finite_word_ptr suffix, twa_graph_ptr g) {
    UCBWalker * walk = new UCBWalker(g);
    for (auto l : *prefix)
        if (walk->step_and_check_failed(l)) return false;
    if (walk->step_and_check_failed(middle)) return false;
    for (auto l : *suffix)
        if (walk->step_and_check_failed(l)) return false;
    return true;
}

UCBWalker * make_walker(twa_graph_ptr g) {
    return new UCBWalker(g);
}

// bool member(finite_word_ptr fwordprefix, finite_word_ptr fwordsuffix, twa_graph_ptr g) {
//     return !fword_hits_accepting(g, fwordprefix, fwordsuffix);
// }