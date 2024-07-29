#pragma once

#include "common.hh"
#include "kcobuchi.cc"
#include "words.cc"
#include "accepting_state_cache.cc"
#include "time.cc"
#include "kcobuchi.cc"

#include <boost/dynamic_bitset.hpp>

using namespace spot;
using namespace std;

#define print_path false

// General class for walkers
template <typename Letter, typename WalkPosition, typename Word>
class Walker {
public:
  virtual Walker<Letter, WalkPosition, Word> * step(const Letter & letter) = 0;
  virtual Walker<Letter, WalkPosition, Word> * step(const Letter & letter, Stopwatch * timer) = 0;

  virtual Walker<Letter, WalkPosition, Word> * walk(Word word) = 0;
  virtual Walker<Letter, WalkPosition, Word> * walk(Word word, Stopwatch * timer) = 0;

  virtual bool failed() = 0;
  virtual void reset() = 0;

  virtual WalkPosition get_position() = 0;
  virtual void set_position(WalkPosition new_position) = 0;

  bool step_and_check_failed(const Letter & letter) {
    step(letter);
    return failed();
  }
};



typedef boost::dynamic_bitset<> stateset;
typedef std::shared_ptr<stateset> stateset_ptr;

// template <>
// struct hash<stateset_ptr>{
//   std::size_t operator()( stateset_ptr const& val ) const {
//     return 100;
//   }
// };

typedef unsigned stateset_index;
typedef unsigned letter_index;
/* This assumes that all accepting states in the input graph are sink states. */
/* It also treats the graph as universal - as soon as an accepting state is touched, all bets are off. */
class Indexed_Universal_Walker : public Walker<letter_index, stateset_ptr, index_word_ptr> {
private:
  unsigned num_states;
  unsigned num_letters;
  stateset_ptr current_stateset;
  accepting_state_cache asc;

  // these are used to cache the statesets stepped to on individual letters
  // from individual states.

  boost::unordered_map<stateset_ptr, stateset_index> ss_to_ssi;
  std::vector<stateset_ptr> ssi_to_ss;

  // graph_table[state][letter]
  stateset_index **graph_table;

  inline stateset_ptr make_statesett() {
    return make_shared<stateset>(num_states);
  }

  unsigned get_or_make_stateset_index(stateset_ptr ss) {
    auto find = ss_to_ssi.find(ss);
    if (find == ss_to_ssi.end()) {
      unsigned new_index = ssi_to_ss.size();
      ssi_to_ss.push_back(ss);
      ss_to_ssi.insert({ss, new_index});
      return new_index;
    } else {
      auto [ss, i] = *find;
      return i;
    }
  }

  // we make a "failure" stateset (which is at no states) first, so
  // we know it has index 0, which will help later.
  void make_failure_stateset() {
    if (ssi_to_ss.size() != 0) throw std::logic_error("This shouldn't happen.");
    auto ss = make_shared<stateset>(num_states);
    ssi_to_ss.push_back(ss);
    ss_to_ssi.insert({ss, 0});
  }

  void make_initial_stateset(unsigned starting_state) {
    if (ssi_to_ss.size() != 1) throw std::logic_error("This shouldn't happen.");
    auto ss = make_shared<stateset>(num_states);
    ss->set(starting_state);
    ssi_to_ss.push_back(ss);
    ss_to_ssi.insert({ss, 1});
  }

  void build_state_letter_stateset_cache(twa_graph_ptr g, const ap_info & apinfo) {
    // build a big cache of all the state sets stepped to from each state and for each letter
    for (unsigned s=0; s<g->num_states(); ++s) {
      // each state
      auto this_row = graph_table[s];
      for (unsigned l=0; l<num_letters; l++) {
        // each letter
        auto letter_bdd = apinfo.bdd_alphabet[l];
        auto sset = make_shared<stateset>(num_states);
        bool will_fail = false;
        for (const auto & e : g->out(s)) {
          // each edge
          if (letter_satisfies_cond(letter_bdd, e.cond)) {
            if (g->is_univ_dest(e.dst)) {
              for (unsigned dst : g->univ_dests(e.dst)) {
                if (is_accepting_state(dst, asc) && is_sink(g, dst))
                  will_fail = true;
                sset->set(dst);
              }
            } else {
              if (is_accepting_state(e.dst, asc) && is_sink(g, e.dst))
                will_fail = true;
              sset->set(e.dst);
            }
          }
        }
        if (will_fail) {
          // the new set contains sink states, so it completely fails.
          this_row[l] = 0;
        } else {
          this_row[l] = get_or_make_stateset_index(sset);
        }
      }
    }
  }

public:
  Indexed_Universal_Walker(twa_graph_ptr g, const ap_info & apinfo) {
    num_states = g->num_states();
    num_letters = apinfo.letter_count;
    asc = make_accepting_state_cache(g);
    // accepts_empty_word = is_accepting_state(g, g->get_init_state_number(), asc);

    // maps between sets of states and their index in the cache.
    ss_to_ssi = boost::unordered_map<stateset_ptr, stateset_index>();
    ssi_to_ss = std::vector<stateset_ptr>();

    // building this whole table at once, to give it a better chance of being contiguous in memory
    graph_table = new stateset_index * [num_states];
    for (int s = 0; s < num_states; ++s) {
        graph_table[s] = new stateset_index[num_letters];
    }

    make_failure_stateset();
    make_initial_stateset(g->get_init_state_number());
    build_state_letter_stateset_cache(g, apinfo);

    reset();
  }

  // release memory
  ~Indexed_Universal_Walker() {
    for (int i = 0; i < num_states; ++i)
        delete[] graph_table[i];
    delete[] graph_table;
  }

  virtual Indexed_Universal_Walker * step(const letter_index & letter) override {
    if (current_stateset == nullptr) return this;

    //TODO: perhaps we cache per stateset and letter here too? would want this to
    // be a sparse map of (ssi, letter) -> next_ssi. Try adding this later and see
    // how it goes I guess. This would essentially cache what it means to make a step
    // from a particular walk configuration with a particular letter. Optimisations
    // in the code elsewhere makes sure we don't make walk queries from the same *prefix*
    // with the same letter, so this optimisation here would only be helpful when there are
    // different prefixes that result in the same set of states. How often would this
    // happen, and how much would it cost to do the checks involved with such a caching?
    // Right now, i'm thinking it's not likely to be very helpful.

    auto next_ss = make_shared<stateset>(num_states);
    // fast iteration through true indices in the bitset
    for (size_t s=current_stateset->find_first(); s!=stateset::npos; s=current_stateset->find_next(s)) {
      // for each state that we are currently at
      auto next_ssi = graph_table[s][letter];
      if (next_ssi == 0) {
        // we fail, taking this step from this state. so everything fails.
        current_stateset = 0;
        return this;
      }
      // bitwise OR
      next_ss->operator|=(*(ssi_to_ss[next_ssi]));
    }
    current_stateset = next_ss;
    return this;
  }

  virtual Indexed_Universal_Walker * step(const letter_index & letter, Stopwatch * timer) override {
    if (current_stateset == nullptr) return this;
    timer->start();
      step(letter);
    timer->stop();
    return this;
  }

  virtual Indexed_Universal_Walker * walk(index_word_ptr word) override {
    for (const auto & l : *word)
      step(l);
    return this;
  }

  virtual Indexed_Universal_Walker * walk(index_word_ptr word, Stopwatch * timer) override {
    if (current_stateset == nullptr) return this;
    timer->start();
    for (const auto & l : *word)
      step(l);
    timer->stop();
    return this;
  }

  virtual bool failed() override {
    return current_stateset == nullptr;
  }

  virtual void reset() override {
    current_stateset = ssi_to_ss[1];
  }

  virtual stateset_ptr get_position() override {
    return current_stateset;
  }

  virtual void set_position(stateset_ptr new_position) override {
    current_stateset = new_position;
  }

};












/*
  Note: this is build for universal cobuchi automaton, and further assumes that
  all accepting states are sink states. As soon as any accepting state is hit, we
  consider the word to be accepting (and therefore not a member).
*/
//TODO: change this to a bool vector of the number of states in the graph. that would be faster.
typedef std::unordered_set<unsigned> ucb_walk_position_set;
class UCB_BDD_Walker : public Walker<bdd, ucb_walk_position_set, finite_word_ptr> {
private:
  twa_graph_ptr g;
  AcceptingStateCache accepting_state_cache;
  ucb_walk_position_set cur_states;
  ucb_walk_position_set temp_cur_states;
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
  virtual void reset() override {
    // initialise the set of current states to just the initial state.
    cur_states.clear();
    cur_states.insert(g->get_init_state_number());
    
    // if the initial state is accepting, we have already failed.
    check_if_failed();
  }

  UCB_BDD_Walker(twa_graph_ptr g) : g{g}, accepting_state_cache(g) {
    bad_membership_count = 0;
    membership_letter_count = 0;
    reset();
  }

  virtual bool failed() override {
    ++bad_membership_count;
    return has_failed;
  }

  virtual UCB_BDD_Walker * step(const bdd & letter) override {
    if (has_failed) return this;
    has_failed = try_step(letter);
    return this;
  }

  virtual UCB_BDD_Walker * step(const bdd & letter, Stopwatch * timer) override {
    if (has_failed) return this;
    timer->start();
      has_failed = try_step(letter);
    timer->stop();
    return this;
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

  bool step_and_check_failed(const bdd & letter) {
    step(letter);
    return failed();
  }

  virtual UCB_BDD_Walker * walk(finite_word_ptr word) override {
    if (!has_failed && try_walk(word))
      has_failed = true;
    return this;
  }

  virtual UCB_BDD_Walker * walk(finite_word_ptr word, Stopwatch * timer) override {
    timer->start();
    if (!has_failed && try_walk(word))
      has_failed = true;
    timer->stop();
    return this;
  }

  // store the current configuration to be later restored.
  virtual ucb_walk_position_set get_position() override {
    //FIXME: figure out if this copy here is needed.
    return ucb_walk_position_set(cur_states);
    // return cur_states;
  }

  virtual void set_position(ucb_walk_position_set new_position) override {
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






































// // check a simple word
// bool member(finite_word_ptr fword, twa_graph_ptr g) {
//   UCB_BDD_Walker * walk = new UCB_BDD_Walker(g);
//   for (auto l : *fword) {
//     if (walk->step_and_check_failed(l)) return false;
//   }
//   return true;
// }

// bool member(finite_word_ptr prefix, bdd middle, finite_word_ptr suffix, twa_graph_ptr g) {
//   UCB_BDD_Walker * walk = new UCB_BDD_Walker(g);
//   for (auto l : *prefix)
//     if (walk->step_and_check_failed(l)) return false;
//   if (walk->step_and_check_failed(middle)) return false;
//   for (auto l : *suffix)
//     if (walk->step_and_check_failed(l)) return false;
//   return true;
// }