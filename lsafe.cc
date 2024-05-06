#include "common.hh"
#include "kcobuchi.cc"
#include <spot/twa/formula2bdd.hh>
#include "btree.cc"
#include "membership.cc"
#include "safety_games.cc"
#include <spot/twaalgos/complement.hh>
#include <spot/twaalgos/complete.hh>
#include "time.cc"
#include "extras.cc"

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

// bool empty_is_member(twa_graph_ptr g) {

//   return is_accepting_state(g, g->get_init_state_number());
// }

typedef std::vector<string> qtset;

twa_graph_ptr make_hypothesis_machine(qtset q, qtset t, Ltable tbl, bdd_dict_ptr dict) {
  twa_graph_ptr out = make_twa_graph(dict);
  unsigned init_state = out->new_state();
  return out;
}

// since the bdd ids assigned to variables aren't going to be consecutive,
// we want to give them new indices so we can more mathematically iterate
// through them (and maybe do some bitwise trickery with them).
typedef std::vector<tuple<formula, unsigned>> better_var_map;

typedef std::shared_ptr<better_var_map> better_var_map_ptr;
inline better_var_map_ptr empty_better_var_map(){
  return std::make_shared<better_var_map>();
}

better_var_map_ptr get_better_var_map(bdd_dict_ptr dict) {
  better_var_map_ptr bvm = empty_better_var_map();
  bvm->reserve(dict->var_map.size());
  for (auto [f, i] : dict->var_map)
    bvm->push_back({f, i});
  return bvm;
}

bdd make_varset_bdd (better_var_map_ptr bvm) {
  bdd out = bddtrue;
  for (auto & [f, v] : *bvm)
    out &= bdd_ithvar(v);
  return out;
}

bdd make_bdd_from_bits(unsigned bits, better_var_map_ptr bvm) {
  bdd out = bddtrue;
  for (auto it = bvm->begin(); it != bvm->end(); ++it) {
    const auto [f, ind] = *it;
    if (bits & 1)
      out &= bdd_ithvar(ind);
    else
      out &= bdd_nithvar(ind);
    bits >>= 1;
  }
  return out;
}

// build an alphabet from a bdd varmap.
// consider every combination of APs, ordered such that their presence
// gives the bit truth of their bvm index (not their bdd var number)
// in the vector's index.
alphabet_vec alphabet_from_bvm(better_var_map_ptr bvm) {
  alphabet_vec alphabet;
  const unsigned num_letters = (1 << bvm->size());
  for (unsigned i=0; i<num_letters; i++)
    alphabet.push_back(make_bdd_from_bits(i, bvm));
  return alphabet;
}

// void make_closed(qtset & Q, qtset & T, Ltable & tbl, alphabet_vec & alphabet, twa_graph_ptr kucb) {

// }

typedef finite_word_ptr prefix;
typedef finite_word_ptr suffix;

const finite_word_ptr empty_word() {
  return make_finite_word();
}

finite_word_ptr copy_word(finite_word_ptr word) {
  finite_word_ptr out = make_finite_word();
  auto end = word->end();
  for (auto it = word->begin(); it!=end; ++it) {
    out->push_back(*it);
  }
  return out;
}

string dump_lsafe_state(bdd_dict_ptr dict, std::vector<prefix> P, std::vector<suffix> S, btree * tbl) {
  stringstream out;
  tbl->print(out);
  out << endl << "P:" << endl;
  for (unsigned i=0; i<P.size(); ++i) {
    out << " (" << i << ") " << prefix_to_string(dict, P[i]) << endl;
  }
  out << "S:" << endl;
  for (prefix s : S) {
    out << " - " << prefix_to_string(dict, s) << endl;
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
  UCBWalker walk_machine = UCBWalker(machine);
  UCBWalker walk_kucb = UCBWalker(kucb);
  finite_word_ptr out = make_finite_word();
  for (bdd letter : counterexample->prefix) {
    out->push_back(letter);
    if (walk_machine.step_and_check_failed(letter) != walk_kucb.step_and_check_failed(letter))
      return out;
  }
  for (unsigned i=0; i<longest_counterexample; i++) {
    for (bdd letter : counterexample->cycle) {
      out->push_back(letter);
      if (walk_machine.step_and_check_failed(letter) != walk_kucb.step_and_check_failed(letter))
        return out;
    }
  }
  throw std::runtime_error("we didnt' find a counterexample after " + to_string(longest_counterexample) + " steps. something wrong here?");
}

inline bdd complete_letter(bdd letter, bdd varset) {
  return bdd_satoneset(letter, varset, bddtrue);
}

bool fully_specify_word(twa_word_ptr word, bdd & varset) {
  bool change_made = false;
  for (bdd & letter : word->prefix) {
    bdd newletter = complete_letter(letter, varset);
    if (newletter != letter) change_made = true;
    letter = newletter;
  }
  for (bdd & letter : word->cycle) {
    bdd newletter = complete_letter(letter, varset);
    if (newletter != letter) change_made = true;
    letter = newletter;
  }
  return change_made;
}

// this assumes that the given counterexample is in fact a counterexample, so it will
// eventually fail when executing on the machine "g"
finite_word_ptr find_shortest_failing_prefix(twa_graph_ptr g, twa_word_ptr counterexample) {
  UCBWalker walker = UCBWalker(g);
  finite_word_ptr out = make_finite_word();
  for (bdd & letter : counterexample->prefix) {
    out->push_back(letter);
    if (walker.step_and_check_failed(letter)) return out;
  }
  for (unsigned i=0; i<longest_counterexample; i++) {
    for (bdd & letter : counterexample->cycle) {
      out->push_back(letter);
      if (walker.step_and_check_failed(letter)) return out;
    }
  }
  throw std::runtime_error("we didnt' find a counterexample after " + to_string(longest_counterexample) + " steps. something wrong here?");
}

twa_graph_ptr get_buchi(formula ltl) {
  translator trans;
  trans.set_type(postprocessor::Buchi);
  trans.set_level(postprocessor::Low); // stopping optimisation for now
  trans.set_pref(postprocessor::SBAcc + postprocessor::Complete);
  twa_graph_ptr aut = trans.run(ltl);
  return aut;
}

ap_map make_ap_map(std::vector<string> input_aps, spot::bdd_dict_ptr dict) {
  auto x = dict->var_map;
  unsigned max_bdd_index = 0;
  
  for (auto [a, b] : x) if (b > max_bdd_index) max_bdd_index = b;

  ap_map apmap = ap_map(max_bdd_index+1, false);

  for (auto [formula, varnum] : x) {
    // std::cout << formula << ", " << varnum << endl;
    string bdd_var_string = force_string(formula);
    if (std::any_of(input_aps.begin(), input_aps.end(), [bdd_var_string](string s){return s==bdd_var_string;})) {
      apmap[varnum] = true;
    }
  }
  // print_vector(apmap);
  return apmap;
}













struct PrefixTree {
  bool already_failed = false;

  PrefixTree * parent = nullptr;

  vector<PrefixTree*> chren;

  walk_position ucb_walk_position;

  int index_in_P;
  int index_in_alphabet;

  btree * bt_position;

  unsigned suffixes_processed_up_to = 0;

  PrefixTree(int index_in_alphabet, const unsigned letter_count, PrefixTree * parent, int index_in_P=-1) : index_in_alphabet{index_in_alphabet}, parent{parent}, index_in_P{index_in_P} {
    chren = vector<PrefixTree *>(letter_count, nullptr);
  }

  prefix get_prefix(const alphabet_vec & alphabet) {
    if (index_in_alphabet == -1)
      return make_finite_word();
    auto prev = parent->get_prefix(alphabet);
    prev->push_back(alphabet[index_in_alphabet]);
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


string dump_lsafe_state2(bdd_dict_ptr dict, alphabet_vec & alphabet, PrefixTree * ptree, vector<PrefixTree*> P, std::vector<suffix> S, btree * tbl) {
  stringstream out;

  unsigned w = 3; // padding width for numeric labels

  out << "Table:" << endl;
  tbl->print_table(out, w);

  out << endl << "Prefixes:" << endl;
  for (unsigned i=0; i<P.size(); ++i) {
    out << pad_number(i, w) << ": " << prefix_to_string(dict, P[i]->get_prefix(alphabet)) << endl;
  }

  out << endl << "Suffixes:" << endl;
  for (unsigned i=0; i<S.size(); ++i) {
    out << pad_number(i, w) << ": " << prefix_to_string(dict, S[i]) << endl;
  }

  // out << "P size: " << P_size << endl << endl;

  // raw btree
  // tbl->print(out);

  // out << "PTree:" << endl;
  // ptree->print(out);
  // out << endl;

  return out.str();
}

void LSafe2(formula ltl, std::vector<string> input_aps, StopwatchSet & timers) {

  // set up lots of timers
  auto global_closing_timer    = timers.make_timer("Table closing");
  auto k_expansion_timer       = timers.make_timer("K Expansion");
  auto global_inclusion_timer  = timers.make_timer("Inclusion Checks");
  auto global_solving_timer    = timers.make_timer("Safety Game Solving");
  auto global_kucb_compl_timer = timers.make_timer("KUCB Complementing");
  auto init_timer              = timers.make_timer("Initialisation");
  auto global_walk_timer       = timers.make_timer("Walking");
  global_walk_timer->set_subsumption_parent(global_closing_timer);

  // Set up lots of required bits and pieces (and time it)
  init_timer->start();
    formula ltl_neg = formula::Not(ltl);
    twa_graph_ptr buchi_neg = get_buchi(ltl_neg);
    twa_graph_ptr cobuchi = dualize_to_univ_coBuchi(buchi_neg);
    ap_map apmap = make_ap_map(input_aps, cobuchi->get_dict());
    bdd_dict_ptr dict = cobuchi->get_dict();
    better_var_map_ptr bvm = get_better_var_map(dict);
    bdd varset = make_varset_bdd(bvm);
    alphabet_vec alphabet = alphabet_from_bvm(bvm);
    unsigned const letter_count = alphabet.size();
    const unsigned h_fin_set = 0;

    auto illegal_letters = get_dead_letter_map(cobuchi, alphabet);
  init_timer->stop();


  #if GEN_PAGE_BASIC
    page_text(to_string(ltl), "LTL");
    page_text(to_string(input_aps), "Input APs");

    #if GEN_PAGE_DETAILS
      const unsigned illegal_letter_count = count_false(illegal_letters);
      if (illegal_letter_count > 0) {
        stringstream ss;
        ss << illegal_letter_count << " out of the " << letter_count << " letters (" << ((illegal_letter_count * 100.0) / letter_count) << "%) are illegal." << endl;
        page_text(ss.str(), "Illegal letters");
      }
    #endif

    page_text(to_string(ltl_neg), "&not;LTL");
    page_graph(buchi_neg, "NonDet Buchi of negated LTL");
    page_graph(cobuchi, "Universal CoBuchi of LTL");
    // page_text(to_string(bdd_to_formula(illegal_letters, dict)), "Illegal letters");
  #endif

  auto LSafeForK = [&](unsigned k) {
    cout << "Iterating with K = " << k << endl;

    // set up some local timers (which will mostly report back to the timers defined above)
    auto local_timers     = StopwatchSet();
    auto inclusion_timerA = local_timers.make_timer("Inclusion Check Instance (Moore intersects kUCB complement)", global_inclusion_timer);
    auto inclusion_timerB = local_timers.make_timer("Inclusion Check Instance (Sanity check)", global_inclusion_timer);
    auto inclusion_timerC = local_timers.make_timer("Inclusion Check Instance (Moore intersects kUCB)", global_inclusion_timer);
    auto solving_timer    = local_timers.make_timer("Safety Game Solving Instance", global_solving_timer);
    auto kucb_compl_timer = local_timers.make_timer("KUCB Complementing Instance", global_kucb_compl_timer);
    auto walk_timer       = local_timers.make_timer("Walking Instance", global_walk_timer);
    auto closing_timer    = local_timers.make_timer("Table Closing Instance", global_closing_timer);
    auto lsafe_k_total    = local_timers.make_timer("Total for K", nullptr, TIMER_GRAPH_TOTAL);
    walk_timer->set_subsumption_parent(closing_timer);
    // auto walk_timer = new Stopwatch("meow"); // so it's not used


    // auto test_timer = local_timers.make_timer("Test Timer");
    

    lsafe_k_total->start();
    auto _ = RunOnReturn([&](){
      lsafe_k_total->stop();
      local_timers.report("  > ");
      #if GEN_K_TIMERS
        local_timers.draw_page_donut("Timers for K = " + to_string(k));
      #endif
    });

    // expand the UCB into a kUCB
    k_expansion_timer->start();
      twa_graph_ptr kucb = kcobuchi_expand(cobuchi, k);
    k_expansion_timer->stop();
    cout << "  (kcobuchi state count: " << kucb->num_states() << ")" << endl;

    // set up the walker
    auto walk = UCBWalker(kucb);
    const bool init_state_accepted = !walk.failed();
    
    #if GEN_PAGE_DETAILS
      if (k == 0) page_heading("Starting with K = " + to_string(k));
      else    page_heading("Incrementing K to " + to_string(k));
      page_graph(kucb, "Expanded K-CoBuchi for K="+to_string(k));
    #endif

    // complement the kucb (for later inclusion checks)
    kucb_compl_timer->start();
      twa_graph_ptr kucb_complement = spot::complement(kucb);
    kucb_compl_timer->stop();

    #if GEN_PAGE_DETAILS
      page_graph(kucb_complement, "kUCB complement");
      page_text_bool(init_state_accepted, "The empty word is accepted.", "The empty word is not accepted.");
    #endif

    /* right now I am storing the prefixes in 3 different forms:
      (A) a prefixtree that keeps track of all prefixes, not just those found in P, as
        well as caches of UCB walk states etc.
      (B) a regular vector of pointers into A (these are the state-unique prefixes)
      (C) a btree that stores an index into B.
    */
    
    PrefixTree * ptree = new PrefixTree(-1, letter_count, nullptr, 0); // tree representation of prefixes
    ptree->ucb_walk_position = walk.get_position(); // the initial walk position
    ptree->already_failed = !init_state_accepted;

    btree * bt = new btree();
    ptree->bt_position = bt->force_bool(init_state_accepted);
    ptree->bt_position->set_rowid(0);

    vector<PrefixTree*> P = {ptree}; // set of unique prefixes
    vector<suffix> S = {empty_word()}; // set of separating suffixes


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

    auto close_table = [&](){
      // set up the new graph we are building.
      twa_graph_ptr H = make_twa_graph(dict);
      H->set_acceptance(acc_cond::acc_code::cobuchi());
      H->prop_state_acc(true);

      for (int pi=0; pi<P.size(); ++pi) {
        if (pi > 1000) throw std::runtime_error("This hypothesis is getting out of control!");
        auto node = P[pi];

        // make a new state in the hypothesis for this row.
        auto new_state_num = H->new_state();
        // just a sanity check
        if (new_state_num != pi) throw std::logic_error("the assumption that these states will have ids that align with my code here was obviously wrong");

        if (node->already_failed) {
          // if this node is already failing, then it is the sink state. Give
          // it a universal accepting edge to itself.
          H->new_acc_edge(pi, pi, bddtrue, true);
        } else for (unsigned i=0; i<letter_count; ++i) {
          // do nothing if it already exists. right now we assume that all existing
          // nodes have all current suffixes processed.
          auto child = node->chren[i];
          if (child == nullptr) {
            // this child hasn't yet been considered. create it and
            // process it. we store the UCB walk position for this node,
            // check if it has already failed, and figure out which unique row in P
            // it is associated with (or if it's a new one).

            node->chren[i] = new PrefixTree(i, letter_count, node);
            child = node->chren[i];
            walk_timer->start();
              walk.set_position(node->ucb_walk_position);
              auto child_walk_position = walk.step(alphabet[i])->get_position();
              child->ucb_walk_position = child_walk_position;
              child->already_failed = walk.failed();
            walk_timer->stop();

            if (child->already_failed) {
              // without considering any suffixes, this child fails, so we already
              // know that it will point to the sink node, which may or may not already exist.
              btree * empty_bt = bt->force_false();
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
              btree * btree_node = bt;
              for (unsigned si=0; si<S.size(); ++si) {
                walk_timer->start();
                  // reset walk to the new prefix (including the letter we are adding)
                  if (i!=0) walk.set_position(child_walk_position);
                  // traverse the btree to store the suffix bits for this prefix
                  const bool accepted = !(walk.walk(S[si])->failed());
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
            btree * btree_node = child->bt_position;
            for (unsigned si=child->suffixes_processed_up_to; si<S.size(); ++si) {
              walk_timer->start();
                // push the walker back to the start of this child, before any suffixes are considered.
                walk.set_position(child->ucb_walk_position);
                const bool accepted = !(walk.walk(S[si])->failed());
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
          H->new_edge(pi, node->chren[i]->index_in_P, alphabet[i]);
        }
      }
      cout << "finished table closing." << endl;
      return H;
    };


    auto extend_table_with_suffix = [&](suffix suffix){
      /* with a new suffix, there are a number of things we need to do:
          - add it to S (obviously)
          - go through the unique prefixes in P, and process this suffix. This will
          always "create" a new row - these rows were unique already, before the new suffix.
          - the other stuff will be taken over by close_table.
      */
      S.push_back(suffix);
      for (int pi=0; pi<P.size(); ++pi) {
        auto node = P[pi];
        node->suffixes_processed_up_to++;
        if (!node->already_failed) {
          walk.set_position(node->ucb_walk_position);
          walk.walk(suffix);
          auto accepted = !walk.failed();
          node->bt_position = node->bt_position->force_bool(accepted);
          node->bt_position->set_rowid(pi);
        }
      }
    };


    const unsigned max_iterations = 200;
    for (unsigned i=1; i<=max_iterations; i++) {
      if (i==max_iterations) {
        #if GEN_PAGE_DETAILS
          page_text("stopping at " + to_string(max_iterations) + " iterations");
        #endif
        cout << "stopping at " << max_iterations << " iterations" << endl;
      }

      #if GEN_PAGE_DETAILS
        page_heading("LSafe iteration #" + to_string(i));
        page_text("Closing table ...");
      #endif

      cout << "  LSafe Iteration " << i << endl;
      // close the table
      
      closing_timer->start();
        twa_graph_ptr H = close_table();
      closing_timer->stop()->report("  > ")->reset();
      #if GEN_PAGE_DETAILS
        page_code("Hypothesis table:", dump_lsafe_state2(dict, alphabet, ptree, P, S, bt));
        page_text(to_string(H->num_states()), "Number of states");
      #endif
      cout << "  hypothesis number of states: " << H->num_states() << endl;
      // page_graph(H, "Hypothesis graph (unmerged)");

      // we have closed the table. now we start to solve it
      solving_timer->start();
        auto [realizable, machine] = solve_safety(H, apmap, false);
      solving_timer->stop()->report();

      if (realizable) {
        highlight_strategy_vec(H, 5, 4);
        #if GEN_PAGE_DETAILS
          page_text("Hypothesis is realisable.");
          page_graph(H, "Strategy");
          page_graph(machine, "Mealy machine " + to_string(machine->num_states()));
        #endif
        inclusion_timerA->start();
          twa_word_ptr counterexample = machine->intersecting_word(kucb_complement);
        inclusion_timerA->stop();
        if (word_not_empty(counterexample)) {
          #if GEN_PAGE_DETAILS
            string orig_counterexample_string = force_string(*counterexample);
            if (fully_specify_word(counterexample, varset))
              page_text(orig_counterexample_string, "Original (symbolic) counterexample");
            page_text(force_string(*counterexample), "Counterexample");
          #else
            fully_specify_word(counterexample, varset);
          #endif

          finite_word_ptr finite_counterexample = find_shortest_failing_prefix(kucb, counterexample);

          #if GEN_PAGE_DETAILS
            page_text(prefix_to_string(dict, finite_counterexample), "Shortest finite counterexample");
          #endif

          // ok im trying just adding the whole thing as a suffix, except the first letter.
          // page_text("Right now we are adding the entire counterexample, minus its first letter, as a suffix.");
          #if GEN_PAGE_DETAILS
            page_text("Adding all suffixes of the counterexample to S.");
          #endif
          while (finite_counterexample->size()>1) {
            finite_counterexample->pop_front();
            // make a copy
            extend_table_with_suffix(copy_word(finite_counterexample));
          }
          #if GEN_PAGE_DETAILS
            page_code("Table after accounting for counterexample:", dump_lsafe_state2(dict, alphabet, ptree, P, S, bt));
          #endif
        } else {
          #if GEN_PAGE_DETAILS
          page_text("No counterexample found. The mealy machine above satisfies the kUCB.");
          page_text("Also checking against the UCB");
          #endif
          inclusion_timerB->start();
            twa_word_ptr new_counterexample = machine->intersecting_word(buchi_neg);
          inclusion_timerB->stop();
          if (word_not_empty(new_counterexample)) {
            #if GEN_PAGE_DETAILS
            page_text("OH NO PROBLEMS");
            #endif
          } else {
            #if GEN_PAGE_DETAILS
            page_text("Also passed the Buchi test.");
            #endif
            cout << "sanity check passed" << endl;
          }
          return make_tuple(true, machine);
        }
      } else {
        #if GEN_PAGE_DETAILS
          page_text("Hypothesis is NOT realisable.");
          highlight_strategy_vec(H, 5, 4);
          page_graph(H, "Strategy");
          page_graph(machine, "Moore machine");
        #endif
        // we just got back a moore machine. find an intersecting word with the kucb to see if it
        // gives us a positive counterexample.
        inclusion_timerC->start();
          twa_word_ptr counterexample = machine->intersecting_word(kucb);
        inclusion_timerC->stop()->report();
        if (word_not_empty(counterexample)) {
          fully_specify_word(counterexample, varset);
          finite_word_ptr finite_counterexample = find_shortest_failing_prefix(H, counterexample);

          #if GEN_PAGE_DETAILS
            page_text(force_string(*counterexample), "Counterexample");
            page_text(prefix_to_string(dict, finite_counterexample), "Shortest finite counterexample");
            page_text("Adding all suffixes of the counterexample to S.");
          #endif

          while (finite_counterexample->size()>1) {
            finite_counterexample->pop_front();
            // make a copy
            extend_table_with_suffix(copy_word(finite_counterexample));
          }
          #if GEN_PAGE_DETAILS
            page_code("Table after accounting for counterexample:", dump_lsafe_state2(dict, alphabet, ptree, P, S, bt));
          #endif
        } else {
          #if GEN_PAGE_DETAILS
            page_text("No counterexample found. For this K, the formula is not realisable.");
          #endif
          // now we check in general, to see if it's a total counterexample or if we need to
          // increment K.
          return make_tuple(false, machine);
        }
      }
    }
    throw std::logic_error("We exceeded the maximum number of transitions hard-coded here :(");
    twa_graph_ptr xx = nullptr;
    return make_tuple(false, xx);
  };

  // Perform LSafe for each K, until we find what we need
  for (unsigned k=0; k<=MAX_K; ++k) {
    auto [realisable, machine] = LSafeForK(k);
    if (realisable) {
      #if GEN_PAGE_DETAILS
        page_text("We have a solution for K = " + to_string(k) + ".");
      #endif
      cout << "we have a solution" << endl;
      return;
    } else {
      twa_word_ptr counterexample = machine->intersecting_word(buchi_neg);
      if (word_not_empty(counterexample)) {
        #if GEN_PAGE_DETAILS
          page_text("We have a proof of unrealisability for K = " + to_string(k));
          page_text("However this is not a proof for the general formula. We need to increment K.");
          page_text(force_string(*counterexample), "Counterexample");
        #endif
        continue;
      } else {
        #if GEN_PAGE_DETAILS
          page_text("We have a proof of unrealisability for K = " + to_string(k));
          page_text("This is also a proof for the general formula. We have proven the specification to be unrealisable.");
        #endif
        return;
      }
    }
  }
}