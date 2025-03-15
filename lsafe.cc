#include "common.hh"
#include "kcobuchi.cc"
#include <spot/twa/formula2bdd.hh>
#include "btree.cc"
#include "walkers.cc"
#include "safety_games.cc"
#include <spot/twaalgos/complement.hh>
#include <spot/twaalgos/complete.hh>
#include "time.cc"
#include "extras.cc"
// #include "select_substrategy.cc"
#include "substrategy_simulation.cc"

#define MAX_K 10

#define longest_counterexample 1000
// #include <spot/twa/bdddict.hh>

using std::cout;

/*
  Ideas:
  - Do we really need to consider all 2^ap transitions? There is probably some analysis we can do
    on the original graph that determines a minimal set of "letters" that actually are used to distinguish
    the semantics of the machine.
  - rows represented as a binary tree could certainly work, especially given all rows are the same length.
*/

bool is_reasonable_ucb(twa_graph_ptr kucb) {
  //FIXME: should we check stuff here?
  return true;
}

typedef std::vector<bool> Lrow;
typedef std::vector<Lrow> Ltable;
typedef std::vector<string> qtset;

twa_graph_ptr make_hypothesis_machine(qtset q, qtset t, Ltable tbl, bdd_dict_ptr dict) {
  twa_graph_ptr out = make_twa_graph(dict);
  unsigned init_state = out->new_state();
  return out;
}

string index_word_to_string(bdd_dict_ptr dict, index_word_ptr fword, const ap_info & apinfo) {
    stringstream out;
    bool first = true;
    out << "[";
    for (auto letter : *fword) {
        if (first) first = false;
        else out << ", ";
        out << bdd_to_formula(apinfo.bdd_alphabet[letter], dict);
    }
    out << "]";
    return out.str();
}

string dump_lsafe_state(bdd_dict_ptr dict, vector<index_word_ptr> P, vector<index_word_ptr> S, btree * tbl, const ap_info & apinfo) {
  stringstream out;
  tbl->print(out);
  out << endl << "P:" << endl;
  for (unsigned i=0; i<P.size(); ++i) {
    out << " (" << i << ") " << index_word_to_string(dict, P[i], apinfo) << endl;
  }
  out << "S:" << endl;
  for (auto s : S) {
    out << " - " << index_word_to_string(dict, s, apinfo) << endl;
  }
  return out.str();
}

bool word_not_empty(twa_word_ptr word) {
  return (word != nullptr);
}

// this uses a known infinite counterexample to check against two machines, to find the shortest prefix
// that is a counterexample. Actually this is silly, we aren't using the knowledge that this is known to
// be a counterexample, so we already know it will succeed on the "machine". so this function is silly lol
finite_word_ptr find_finite_counterexample(twa_graph_ptr machine, twa_graph_ptr kucb, twa_word_ptr counterexample) {
  Walker<bdd, ucb_walk_position_set, finite_word_ptr> * walk_machine = new UCB_BDD_Walker(machine);
  Walker<bdd, ucb_walk_position_set, finite_word_ptr> * walk_kucb = new UCB_BDD_Walker(kucb);
  finite_word_ptr out = make_finite_word();
  for (bdd letter : counterexample->prefix) {
    out->push_back(letter);
    if (walk_machine->step_and_check_failed(letter) != walk_kucb->step_and_check_failed(letter))
      return out;
  }
  for (unsigned i=0; i<longest_counterexample; i++) {
    for (bdd letter : counterexample->cycle) {
      out->push_back(letter);
      if (walk_machine->step_and_check_failed(letter) != walk_kucb->step_and_check_failed(letter))
        return out;
    }
  }
  throw std::runtime_error("we didnt' find a counterexample after " + to_string(longest_counterexample) + " steps. something wrong here?");
}

bool fully_specify_word(twa_word_ptr word, const ap_info & apinfo) {
  bool change_made = false;
  for (bdd & letter : word->prefix) {
    bdd newletter = complete_letter(letter, apinfo);
    if (newletter != letter) change_made = true;
    letter = newletter;
  }
  for (bdd & letter : word->cycle) {
    bdd newletter = complete_letter(letter, apinfo);
    if (newletter != letter) change_made = true;
    letter = newletter;
  }
  return change_made;
}

bool fully_specify_finite_word(finite_word_ptr word, const ap_info & apinfo) {
  bool change_made = false;
  for (bdd & letter : *word) {
    bdd newletter = complete_letter(letter, apinfo);
    if (newletter != letter) change_made = true;
    letter = newletter;
  }

  return change_made;
}

// this assumes that the given counterexample is in fact a counterexample, so it will
// eventually fail when executing on the machine "g"
finite_word_ptr find_shortest_failing_prefix(twa_graph_ptr g, twa_word_ptr counterexample) {
  shared_ptr<UCB_BDD_Walker> walker = make_shared<UCB_BDD_Walker>(g);
  finite_word_ptr out = make_finite_word();
  for (bdd & letter : counterexample->prefix) {
    out->push_back(letter);
    if (walker->step_and_check_failed(letter)) return out;
  }
  for (unsigned i=0; i<longest_counterexample; i++) {
    for (bdd & letter : counterexample->cycle) {
      out->push_back(letter);
      if (walker->step_and_check_failed(letter)) return out;
    }
  }
  throw std::runtime_error("we didnt' find a counterexample after " + to_string(longest_counterexample) + " steps. something wrong here?");
}

// this does not totally assume that the given counterexample is a real counterexample,
// but will throw an error if it is not.
finite_word_ptr find_shortest_moore_counterexample(const twa_graph_ptr & g, const twa_graph_ptr & kucb, twa_word_ptr counterexample) {
  shared_ptr<UCB_BDD_Walker> walker_g = make_shared<UCB_BDD_Walker>(g);
  shared_ptr<UCB_BDD_Walker> walker_kucb = make_shared<UCB_BDD_Walker>(kucb);
  finite_word_ptr out = make_finite_word();
  for (bdd & letter : counterexample->prefix) {
    out->push_back(letter);
    const bool failed_g    = walker_g->step_and_check_failed(letter);
    const bool failed_kucb = walker_kucb->step_and_check_failed(letter);
    // this counterexample should be something that FAILS the hypothesis, but is still safe in the kucb.
    // if this fails the kucb, throw an error.
    // cout << "g walk state: " << endl;
    // walker_g->debug_dump();
    // cout << "kucb walk state: " << endl;
    // walker_kucb->debug_dump();

    // if (!failed_g && !failed_kucb) cout << "step taken, both are fine" << endl;
    // if (!failed_g && failed_kucb) cout << "step taken, g is still ok but kucb failed" << endl;
    // if (failed_g && !failed_kucb) cout << "step taken, g failed but kucb is still ok" << endl;
    // if (failed_g && failed_kucb) cout << "step taken, both failed at the same time (this is weird right?)" << endl;
    if (failed_kucb) throw std::runtime_error("this shouldn't happen");
    if (failed_g) return out;
  }
  for (unsigned i=0; i<longest_counterexample; i++) {
    for (bdd & letter : counterexample->cycle) {
      out->push_back(letter);
      const bool failed_g    = walker_g->step_and_check_failed(letter);
      const bool failed_kucb = walker_kucb->step_and_check_failed(letter);
      // this counterexample should be something that FAILS the hypothesis, but is still safe in the kucb.
      // if this fails the kucb, throw an error.
      // cout << "g walk state: " << endl;
      // walker_g->debug_dump();
      // cout << "kucb walk state: " << endl;
      // walker_kucb->debug_dump();
      // if (!failed_g && !failed_kucb) cout << "step taken, both are fine" << endl;
      // if (!failed_g && failed_kucb) cout << "step taken, g is still ok but kucb failed" << endl;
      // if (failed_g && !failed_kucb) cout << "step taken, g failed but kucb is still ok" << endl;
      // if (failed_g && failed_kucb) cout << "step taken, both failed at the same time (this is weird right?)" << endl;
      if (failed_kucb) throw std::runtime_error("this shouldn't happen");
      if (failed_g) return out;
    }
  }
  throw std::runtime_error("we didnt' find a counterexample after " + to_string(longest_counterexample) + " steps. something wrong here?");
}

twa_graph_ptr get_buchi(formula ltl) {
  translator trans;
  trans.set_type(postprocessor::Buchi);
  trans.set_level(postprocessor::High);
  trans.set_pref(postprocessor::SBAcc + postprocessor::Complete);
  twa_graph_ptr aut = trans.run(ltl);
  return aut;
}


#define CONFIG_WALKER_TYPE_BDDS 0
#define CONFIG_WALKER_TYPE_INDEXES 1


#define CONFIG_WALKER_TYPE CONFIG_WALKER_TYPE_INDEXES


#if CONFIG_WALKER_TYPE == CONFIG_WALKER_TYPE_BDDS
  #define WALKER_POSITION_TYPE ucb_walk_position_set
  #define WALKER_TAKES_BDD 1
#elif CONFIG_WALKER_TYPE == CONFIG_WALKER_TYPE_INDEXES
  #define WALKER_POSITION_TYPE stateset_ptr
  #define WALKER_TAKES_BDD 0
#endif


typedef stateset_ptr walk_position;
struct PrefixTree {
  bool already_failed = false;

  // note this can't be a shared_ptr, as this would cause cyclic ownership
  // and the shared_ptr garbage collector would never delete anything.
  PrefixTree * parent = nullptr;

  vector<shared_ptr<PrefixTree>> chren;

  WALKER_POSITION_TYPE ucb_walk_position;

  int index_in_P;
  int index_in_alphabet;

  shared_ptr<btree> bt_position;

  unsigned suffixes_processed_up_to = 0;

  PrefixTree(int index_in_alphabet, const unsigned letter_count, shared_ptr<PrefixTree> parent, int index_in_P=-1)
  : index_in_alphabet{index_in_alphabet}
  , parent{parent.get()}
  , index_in_P{index_in_P} {
    chren = vector<shared_ptr<PrefixTree>>(letter_count, nullptr); //leak
  }

  // ~PrefixTree() {
  //   cout << endl << endl << "   prefix destruction    " << endl << endl;
  // }

  finite_word_ptr get_bdd_prefix(const ap_info & apinfo) {
    if (index_in_alphabet == -1)
      return make_finite_word();
    finite_word_ptr prev = parent->get_bdd_prefix(apinfo);
    prev->push_back(apinfo.bdd_alphabet[index_in_alphabet]);
    return prev;
  }

  index_word_ptr get_prefix() {
    if (index_in_alphabet == -1)
      return empty_word();
    auto prev = parent->get_prefix();
    prev->push_back(index_in_alphabet);
    return prev;
  }

  void print(stringstream & out, string indent = "") {
    // cout << "Ptree" << endl;
    out << indent << (already_failed ? "F" : "A") << " @ " << index_in_P << endl;
    for (unsigned i=0; i<chren.size(); ++i) {
      auto child = chren[i];
      if (child == nullptr) {
        // cout << "child does not exist" << endl;
      } else {
        child->print(out, indent + "  ");
      }
    }

  }
};

finite_word_ptr index_word_to_bdd_word(index_word_ptr word, const ap_info & apinfo) {
  auto out = make_finite_word();
  for (auto it=word->begin(); it!=word->end(); ++it)
    out->push_back(apinfo.bdd_alphabet[*it]);
  return out;
}

string dump_lsafe_state2(bdd_dict_ptr dict, const ap_info & apinfo, const shared_ptr<PrefixTree> & ptree, const vector<shared_ptr<PrefixTree>> & P, const vector<index_word_ptr> & S, const shared_ptr<btree> & tbl) {
  stringstream out;

  unsigned w = 3; // padding width for numeric labels

  out << "Table:" << endl;
  tbl->print_table(out, w);

  out << endl << "Prefixes:" << endl;
  for (unsigned i=0; i<P.size(); ++i) {
    out << pad_number(i, w) << ": " << prefix_to_string(dict, P[i]->get_bdd_prefix(apinfo)) << endl;
  }

  out << endl << "Suffixes:" << endl;
  for (unsigned i=0; i<S.size(); ++i) {
    out << pad_number(i, w) << ": " << prefix_to_string(dict, index_word_to_bdd_word(S[i], apinfo)) << endl;
  }

  // out << "P size: " << P_size << endl << endl;

  // raw btree
  // tbl->print(out);

  // out << "PTree:" << endl;
  // ptree->print(out);
  // out << endl;

  return out.str();
}

void LSafe(formula ltl, std::vector<string> input_aps, StopwatchSet & timers) {

  // set up lots of timers
  auto global_closing_timer    = timers.make_timer("Table closing");
  auto k_expansion_timer       = timers.make_timer("K Expansion");
  auto global_inclusion_timer  = timers.make_timer("Inclusion Checks");
  auto global_solving_timer    = timers.make_timer("Safety Game Solving  (incl minimisation)");
  auto global_det_count_timer  = timers.make_timer("Determinisation State Count");
  // auto global_kucb_compl_timer = timers.make_timer("KUCB Complementing");
  auto init_timer              = timers.make_timer("Initialisation");
  auto global_walk_timer       = timers.make_timer("Walking");
  auto global_walk_building_timer       = timers.make_timer("Building Walkers");
  global_walk_timer->set_subsumption_parent(global_closing_timer);

  auto test_timer = Stopwatch("testing");

  // Set up lots of required bits and pieces (and time it)
  init_timer->start();
    formula ltl_neg = formula::Not(ltl);
    twa_graph_ptr buchi_neg = get_buchi(ltl_neg);
    twa_graph_ptr cobuchi = dualize_to_univ_coBuchi(buchi_neg);
    bdd_dict_ptr dict = cobuchi->get_dict();
    const ap_info apinfo = ap_info(dict, input_aps);
    const unsigned h_fin_set = 0;

    auto illegal_letters = get_dead_letter_map(cobuchi, apinfo);
  init_timer->stop();

  IF_PAGE_BASIC {
    page_text(to_string(ltl), "LTL");
    page_text(to_string(input_aps), "Input APs");

    IF_PAGE_DETAILS {
      const unsigned illegal_letter_count = illegal_letters.count();
      if (illegal_letter_count > 0) {
        stringstream ss;
        ss << illegal_letter_count << " out of the " << apinfo.letter_count << " letters (" << ((illegal_letter_count * 100.0) / apinfo.letter_count) << "%) are illegal." << endl;
        page_text(ss.str(), "Illegal letters");
      }
    }
    IF_PAGE_DETAILS {
      page_text(to_string(ltl_neg), "&not;LTL");
      page_graph(buchi_neg, "NonDet Buchi of negated LTL");
    }
    page_graph(cobuchi, "Universal CoBuchi of LTL");
    // page_text(to_string(bdd_to_formula(illegal_letters, dict)), "Illegal letters");
  }

  auto LSafeForK = [&](unsigned k) {
    cout << "Iterating with K = " << k << endl;
    PAUSE;

    // set up some local timers (which will mostly report back to the timers defined above)
    auto local_timers       = StopwatchSet();
    auto inclusion_timerA   = local_timers.make_timer("Inclusion Check Instance (Moore intersects kUCB complement)", global_inclusion_timer);
    auto inclusion_timerB   = local_timers.make_timer("Inclusion Check Instance (Sanity check)", global_inclusion_timer);
    auto inclusion_timerC   = local_timers.make_timer("Inclusion Check Instance (Moore intersects kUCB)", global_inclusion_timer);
    auto det_count_timer    = local_timers.make_timer("Determinisation State Count", global_det_count_timer);
    auto solving_timer      = local_timers.make_timer("Safety Game Solving Instance", global_solving_timer);
    // auto kucb_compl_timer = local_timers.make_timer("KUCB Complementing Instance", global_kucb_compl_timer);
    auto walk_timer         = local_timers.make_timer("Walking Instance", global_walk_timer);
    auto build_walker_timer = local_timers.make_timer("Building Walker", global_walk_building_timer);
    auto closing_timer      = local_timers.make_timer("Table Closing Instance", global_closing_timer);
    auto lsafe_k_total      = local_timers.make_timer("Total for K", nullptr, TIMER_GRAPH_TOTAL);
    walk_timer->set_subsumption_parent(closing_timer);
    // auto walk_timer = new Stopwatch("meow"); // so it's not used

    // auto test_timer = local_timers.make_timer("Test Timer");
    
    lsafe_k_total->start();
    auto _ = RunOnReturn([&](){
      lsafe_k_total->stop();
      local_timers.report("  > ");
      IF_K_TIMERS local_timers.draw_page_donut("Timers for K = " + to_string(k));
    });

    // find state count for determinisation of UCB with bound K
    unsigned determinisation_count = 0;
    if (opt.count_det) {
      det_count_timer->start();
        cout << "counting determinisation states ..." << endl;
        determinisation_count = count_determinisation_states_with_cache(cobuchi, k, apinfo, opt.tlsf_path);
      det_count_timer->stop();
      cout << "  determinisation state count: " << determinisation_count << endl;
    }

    // expand the UCB into a kUCB
    k_expansion_timer->start();
      twa_graph_ptr kucb = kcobuchi_expand(cobuchi, k);
    k_expansion_timer->stop();
    cout << "  (kcobuchi state count: " << kucb->num_states() << ")" << endl;

    // set up the walker
    #if CONFIG_WALKER_TYPE == CONFIG_WALKER_TYPE_BDDS
      Walker<bdd, ucb_walk_position_set, finite_word_ptr> * walk = new UCB_BDD_Walker(kucb);
      build_walker_timer->flag_ignored();
    #elif CONFIG_WALKER_TYPE == CONFIG_WALKER_TYPE_INDEXES
      build_walker_timer->start();
        shared_ptr<Walker<unsigned, walk_position, index_word_ptr>> walk = make_shared<Indexed_Universal_Walker>(kucb, apinfo);
      build_walker_timer->stop();
    #endif
    const bool init_state_accepted = !walk->failed();

    
    
    IF_PAGE_DETAILS {
      if (k == 0) page_heading("Starting with K = " + to_string(k));
      else    page_heading("Incrementing K to " + to_string(k));
      page_text(to_string(determinisation_count), "Determinisation state count");
      page_graph(kucb, "Expanded K-CoBuchi for K="+to_string(k));
    }

    // complement the kucb (for later inclusion checks)
    // kucb_compl_timer->start();
      twa_graph_ptr kucb_complement = spot::complement(kucb);
    // kucb_compl_timer->stop();

    IF_PAGE_DETAILS {
      page_graph(kucb_complement, "kUCB complement");
      page_text_bool(init_state_accepted, "The empty word is accepted.", "The empty word is not accepted.");
    }

    /* right now I am storing the prefixes in 3 different forms:
      (A) a prefixtree that keeps track of all prefixes, not just those found in P, as
        well as caches of UCB walk states etc.
      (B) a regular vector of pointers into A (these are the state-unique prefixes)
      (C) a btree that stores an index into B.
    */
    
    test_timer.start();
    shared_ptr<PrefixTree> ptree = make_shared<PrefixTree>(-1, apinfo.letter_count, nullptr, 0); // tree representation of prefixes
    ptree->ucb_walk_position = walk->get_position(); // the initial walk position
    ptree->already_failed = !init_state_accepted;
    test_timer.stop();

    // i think this is now taken care of by shared_ptr mechanisms.
    // auto x = RunOnReturn([&ptree](){
      // since ptrees contain references to eachother, I don't think
      // the shared_ptr system will know to delete them when `ptree` goes
      // out of scope.
      // ptree->~PrefixTree();
    // });

    shared_ptr<btree> bt = make_shared<btree>();
    ptree->bt_position = bt->force_bool(init_state_accepted);
    // cout << ptree->bt_position << "  " << (ptree->bt_position == nullptr) << endl;
    ptree->bt_position->set_rowid(0);

    vector<shared_ptr<PrefixTree>> P = {ptree}; // set of unique prefixes
    vector<index_word_ptr> S = {empty_word()}; // set of separating suffixes


    // typedef vector<unsigned> idx_word;

    // // unsigned sink_row_id = -1;
    // auto get_or_create_sink_row = [&]() {
    //   btree * bt_sink = bt->force_false();
    //   if (bt_sink->has_rowid()) return bt_sink->get_rowid();
      


    //   // return bt->force_false();
    //   // if (sink_row_id != -1) return sink_row_id;
    //   // record this row as existing near the top of the btree (just after one false)
    //   // btree * bt_empty = bt->force_false();
    //   // if (bt_empty->has_rowid()) {
    //   //   sink_row_id = bt_empty->get_rowid();
    //   // }
    //   // cur_node->set_rowid(sink_row_id);
    //   // return sink_row_id;

    //   // cout << "creating sink row" << endl;
    //   // // the sink row doesn't already exist, so we are now creating it, at the end of P.
    //   // sink_row_id = P.size();
    //   // P.push_back(P[node_idx]);

    //   // // give this node in the graph its own self-edge
    //   // H->new_acc_edge(sink_row_id, sink_row_id, bddtrue, true);

      
    // };

    auto close_table = [&](bdd & most_recent_counterexample_first_letter){
      // set up the new graph we are building.
      twa_graph_ptr H = make_twa_graph(dict);
      H->set_acceptance(acc_cond::acc_code::cobuchi());
      H->prop_state_acc(true);

      for (int pi=0; pi<P.size(); ++pi) {
        auto node = P[pi];

        // make a new state in the hypothesis for this row.
        auto new_state_num = H->new_state();
        // just a sanity check
        if (new_state_num != pi) throw std::logic_error("the assumption that these states will have ids that align with my code here was obviously wrong");

        if (node->already_failed) {
          // if this node is already failing, then it is the sink state. Give
          // it a universal accepting edge to itself.
          H->new_acc_edge(pi, pi, bddtrue, true);
        } else {
        
          auto iterate_over_letter = [&](unsigned i){
            // do nothing if it already exists. right now we assume that all existing
            // nodes have all current suffixes processed.
            auto child = node->chren[i];
            if (child == nullptr) {
              // this child hasn't yet been considered. create it and
              // process it. we store the UCB walk position for this node,
              // check if it has already failed, and figure out which unique row in P
              // it is associated with (or if it's a new one).
              // if (debug_here) cout << "debug A" << endl;
              node->chren[i] = make_shared<PrefixTree>(i, apinfo.letter_count, node);
              // if (debug_here) cout << "debug B" << endl;
              child = node->chren[i];
              walk_timer->start();
                walk->set_position(node->ucb_walk_position);
                #if WALKER_TAKES_BDD
                  const bdd & step_param = (*alphabet)[i];
                #else
                  const unsigned & step_param = i;
                #endif
                auto child_walk_position = walk->step(step_param)->get_position();
                child->ucb_walk_position = child_walk_position;
                child->already_failed = walk->failed();
              walk_timer->stop();

              if (child->already_failed) {
                // without considering any suffixes, this child fails, so we already
                // know that it will point to the sink node, which may or may not already exist.
                shared_ptr<btree> empty_bt = bt->force_false();
                child->bt_position = empty_bt;
                child->suffixes_processed_up_to = S.size();
                if (empty_bt->has_rowid()) {
                  // we already have an empty row. point to it.
                  child->index_in_P = empty_bt->get_rowid();
                } else {
                  // this is now the new empty row. add it to P, and the btree.
                  unsigned new_index = P.size();
                  empty_bt->set_rowid(new_index);
                  child->index_in_P = new_index;
                  P.push_back(child);
                }
              } else {
                // walk through all current suffixes to figure out what the equivalent
                // row in P is (if there is one).
                shared_ptr<btree> btree_node = bt;
                for (unsigned si=0; si<S.size(); ++si) {
                  walk_timer->start();
                    // reset walk to the new prefix (including the letter we are adding)
                    if (i!=0) walk->set_position(child_walk_position);
                    // traverse the btree to store the suffix bits for this prefix
                    #if WALKER_TAKES_BDD
                      const auto & walk_param = index_word_to_bdd_word(S[si], alphabet);
                    #else
                      const index_word_ptr & walk_param = S[si];
                    #endif
                    const bool accepted = !(walk->walk(walk_param)->failed());
                  walk_timer->stop();
                  btree_node = btree_node->force_bool(accepted);
                }
                child->suffixes_processed_up_to = S.size();
                child->bt_position = btree_node;
                // check if there's an existing row in P for these suffix bits.
                if (btree_node->has_rowid()) {
                  child->index_in_P = btree_node->get_rowid();
                } else {
                  // we have discovered a unique new row. Add it to P etc.
                  // note to self: we already know this is not a sink node.
                  int new_index = P.size();
                  btree_node->set_rowid(new_index);
                  child->index_in_P = new_index;
                  P.push_back(child);
                }
              }
            } else if (!child->already_failed && child->suffixes_processed_up_to < S.size()) {
              // this node already exists, but there must be new suffixes since the
              // last time it was processed, so we need to deal with new suffixes,
              // although only if it hasn't already failed.
              shared_ptr<btree> btree_node = child->bt_position;
              for (unsigned si=child->suffixes_processed_up_to; si<S.size(); ++si) {
                walk_timer->start();
                  // push the walker back to the start of this child, before any suffixes are considered.
                  walk->set_position(child->ucb_walk_position);
                  #if WALKER_TAKES_BDD
                    const auto & walk_param = index_word_to_bdd_word(S[si], alphabet);
                  #else
                    const index_word_ptr & walk_param = S[si];
                  #endif
                  const bool accepted = !(walk->walk(walk_param)->failed());
                walk_timer->stop();
                btree_node = btree_node->force_bool(accepted);
              }
              child->suffixes_processed_up_to = S.size();
              child->bt_position = btree_node;
              // check if there's an existing row in P for these suffix bits.
              if (btree_node->has_rowid()) {
                child->index_in_P = btree_node->get_rowid();
              } else {
                // we have discovered a unique new row. Add it to P etc.
                // note to self: at this point we already know it's not a sink node.
                int new_index = P.size();
                btree_node->set_rowid(new_index);
                child->index_in_P = new_index;
                P.push_back(child);
              }
            }
            H->new_edge(pi, node->chren[i]->index_in_P, apinfo.bdd_alphabet[i]);
          };
          if (most_recent_counterexample_first_letter != bdd_false()) {
            unsigned first_check_letter_idx = make_bits_from_bdd(most_recent_counterexample_first_letter, apinfo);
            iterate_over_letter(first_check_letter_idx);
          }
          for (unsigned i=0; i<apinfo.letter_count; ++i) {
            iterate_over_letter(i);
          }
        }
      }
      // cout << "finished table closing." << endl;
      return H;
    };


    auto extend_table_with_suffix = [&](index_word_ptr suffix){
      /* with a new suffix, there are a number of things we need to do:
          - add it to S (obviously)
          - go through the unique prefixes in P, and process this suffix. This will
          always "create" a new row - these rows were unique already, before the new suffix.
          - the other stuff will be taken over by close_table.
      */
      S.push_back(suffix);

      #if WALKER_TAKES_BDD
        const finite_word_ptr & walk_suffix_param = index_word_to_bdd_word(suffix, alphabet);
      #else
        const index_word_ptr & walk_suffix_param = suffix;
      #endif

      for (int pi=0; pi<P.size(); ++pi) {
        auto node = P[pi];
        node->suffixes_processed_up_to++;
        if (!node->already_failed) {
          walk->set_position(node->ucb_walk_position);
          walk->walk(walk_suffix_param);
          auto accepted = !walk->failed();
          node->bt_position = node->bt_position->force_bool(accepted);
          node->bt_position->set_rowid(pi);
        }
      }
    };


    bdd most_recent_counterexample_first_letter = bdd_false();
    const unsigned max_iterations = 500;
    for (unsigned i=1; i<=max_iterations; i++) {
      if (i==max_iterations) {
        IF_PAGE_DETAILS {
          page_text("stopping at " + to_string(max_iterations) + " iterations");
        }
        cout << "stopping at " << max_iterations << " iterations" << endl;
      }

      IF_PAGE_DETAILS {
        page_heading("LSafe iteration #" + to_string(i));
        page_text("Closing table ...");
      }

      cout << "  LSafe Iteration " << i << endl;
      // close the table
      
      closing_timer->start();
        twa_graph_ptr H = close_table(most_recent_counterexample_first_letter);
      closing_timer->stop();
      IF_PAGE_DETAILS {
        page_code("Hypothesis table:", dump_lsafe_state2(dict, apinfo, ptree, P, S, bt));
        page_text(to_string(H->num_states()), "Number of states");
      }
      cout << "    hypothesis has " << H->num_states() << " states" << endl;
      // page_graph(H, "Hypothesis graph (unmerged)");

      // we have closed the table. now we start to solve it
      solving_timer->start();
        auto [realizable, machine] = solve_safety(H, apinfo, false);
      solving_timer->stop();

      if (realizable) {
        highlight_strategy_vec(H, 5, 4);
        IF_PAGE_DETAILS {
          page_text("Hypothesis is realisable.");
          page_graph(H, "Strategy");
          // debug
          if (i == 3 && k == 2) {
            // debug_find_smaller_substrategy(H, apinfo);
          }
          page_graph(machine, "Mealy machine (" + to_string(machine->num_states()) + " states)");
        }
        inclusion_timerA->start();
          twa_word_ptr counterexample = machine->intersecting_word(kucb_complement);
        inclusion_timerA->stop();
        if (word_not_empty(counterexample)) {
          IF_PAGE_DETAILS {
            string orig_counterexample_string = force_string(*counterexample);
            if (fully_specify_word(counterexample, apinfo))
              page_text(orig_counterexample_string, "Original (symbolic) counterexample");
            page_text(force_string(*counterexample), "Counterexample");
          } else {
            fully_specify_word(counterexample, apinfo);
          }

          finite_word_ptr finite_counterexample = find_shortest_failing_prefix(kucb, counterexample);

          IF_PAGE_DETAILS {
            page_text(prefix_to_string(dict, finite_counterexample), "Shortest finite counterexample");
          }

          // ok im trying just adding the whole thing as a suffix, except the first letter.
          // page_text("Right now we are adding the entire counterexample, minus its first letter, as a suffix.");
          IF_PAGE_DETAILS {
            page_text("Adding all suffixes of the counterexample to S.");
          }
          most_recent_counterexample_first_letter = finite_counterexample->front();
          while (finite_counterexample->size()>1) {
            finite_counterexample->pop_front();
            // make a copy
            extend_table_with_suffix(bdd_word_to_index_word(finite_counterexample, apinfo));
          }
          IF_PAGE_DETAILS {
            page_code("Table after accounting for counterexample:", dump_lsafe_state2(dict, apinfo, ptree, P, S, bt));
          }
        } else {
          IF_PAGE_DETAILS {
            page_text("No counterexample found. The mealy machine above satisfies the kUCB.");
            page_text("Also checking against the UCB");
          }
          inclusion_timerB->start();
            // this is doing something wrong i think.
            // we have our machine. we are in the REALIZABLE section of this algorithm.
            // so it's a mealy machine. we want to see if this is included in ....
            twa_word_ptr new_counterexample = machine->intersecting_word(buchi_neg);
          inclusion_timerB->stop();
          if (word_not_empty(new_counterexample)) {
            IF_PAGE_DETAILS {
              page_text("This machine apparently doesn't satisfy the UCB.", "OH NO!");
              page_code("Counterexample", force_string(*new_counterexample));
              exit(1);
            }
          } else {
            IF_PAGE_DETAILS {
              page_text("Also passed the Buchi test.");
            }
            IF_PAGE_TABLE {
              //table format:
              // name, k_solved, det_count, final_count, times
              page_table_cell(to_string(k));
              page_table_cell(to_string(i));
              if (opt.count_det)
                page_table_cell(to_string(determinisation_count));
              page_table_cell(to_string(machine->num_states()));
              // page_table_cell(timers.getTimer("LSafe Total")->smart_duration_string());
            }
            cout << "sanity check passed" << endl;
            // debug_check_hardcoded_solution(H, apmap);
            // here is where we have a final (mealy) solution.
            // i will try to re-select a smaller mealy machine from the most-permissive strategy here.
            // debug_find_smaller_substrategy(H, apinfo);
            // find_smaller_simulation(H, apinfo);
          }
          return make_tuple(true, machine);
        }
      } else {
        IF_PAGE_DETAILS {
          page_text("Hypothesis is NOT realisable.");
          highlight_strategy_vec(H, 5, 4);
          page_graph(H, "Strategy");
          page_graph(machine, "Moore machine");
          // H->merge_edges();
          // page_graph(H, "Strategy (simplified)");
        }
        // we just got back a moore machine. find an intersecting word with the kucb to see if it
        // gives us a positive counterexample.
        inclusion_timerC->start();
          // twa_word_ptr counterexample = machine->intersecting_word(kucb);
          twa_word_ptr counterexample = test_moore_kucb_intersection(machine, cobuchi, k, kucb, dict);  //machine->intersecting_word(kucb);
        inclusion_timerC->stop();
        if (counterexample != nullptr) {
          IF_PAGE_DETAILS {
            string orig_counterexample_string = force_string(*counterexample);
            if (fully_specify_word(counterexample, apinfo))
              page_text(orig_counterexample_string, "Original (symbolic) counterexample");
            page_text(force_string(*counterexample), "Counterexample");
          } else {
            fully_specify_word(counterexample, apinfo);
          }

          // different kind of counterexample: it's something that fails in the 
          // hypothesis but succeeds in the kUCB.
          // twa_graph_ptr H_copy = make_shared<twa_graph>(*H);
          // twa_graph_ptr moore_copy = make_shared<twa_graph>(*machine);
          // H_copy->merge_edges();
          // moore_copy->merge_edges();
          // page_graph(H_copy, "merged hypothesis");
          // page_graph(moore_copy, "merged moore");

          finite_word_ptr finite_counterexample = find_shortest_moore_counterexample(H, kucb, counterexample);
          // finite_word_ptr finite_counterexample = find_shortest_failing_prefix(H, counterexample);
          // auto & finite_counterexample = counterexample;

          IF_PAGE_DETAILS {
            page_text(prefix_to_string(dict, finite_counterexample), "Shortest finite counterexample");
            page_text("Adding all suffixes of the counterexample to S.");
          }

          // throw std::runtime_error("moore counterexample");

          while (finite_counterexample->size()>1) {
            finite_counterexample->pop_front();
            // make a copy
            extend_table_with_suffix(bdd_word_to_index_word(finite_counterexample, apinfo));
          }
          IF_PAGE_DETAILS {
            page_code("Table after accounting for counterexample:", dump_lsafe_state2(dict, apinfo, ptree, P, S, bt));
          }
        } else {
          IF_PAGE_DETAILS {
            page_text("No counterexample found. For this K, the formula is not realisable.");
          }
          // now we check in general, to see if it's a total counterexample or if we need to
          // increment K.
          return make_tuple(false, machine);
        }
      }
    }
    throw std::logic_error("We exceeded the maximum number of iterations hard-coded here :(");
    twa_graph_ptr xx = nullptr;
    return make_tuple(false, xx);
  };

  // Perform LSafe for each K, until we find what we need
  for (unsigned k=0; k<=MAX_K; ++k) {
    auto [realisable, machine] = LSafeForK(k);
    if (realisable) {
      cout << "we have a solution" << endl;
      test_timer.report(" ---- ");
      IF_PAGE_BASIC {
        page_text("We have a solution for K = " + to_string(k) + ".");
        page_graph(machine, "Solution mealy machine");
      }
      return;
    } else {
      twa_word_ptr counterexample = machine->intersecting_word(buchi_neg);
      if (word_not_empty(counterexample)) {
        IF_PAGE_DETAILS {
          page_text("We have a proof of unrealisability for K = " + to_string(k));
          page_text("However this is not a proof for the general formula. We need to increment K.");
          page_text(force_string(*counterexample), "Counterexample");
        }
        continue;
      } else {
        IF_PAGE_DETAILS {
          page_text("We have a proof of unrealisability for K = " + to_string(k));
          page_text("This is also a proof for the general formula. We have proven the specification to be unrealisable.");
        }
        return;
      }
    }
  }
}