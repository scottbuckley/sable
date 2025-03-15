#pragma once
#include "common.hh"
#include "kcobuchi.cc"
#include "kucb_intersection.cc"
#include <string>
#include <spot/tl/relabel.hh>
#include "words.cc"

bdd get_dead_letters_bdd(twa_graph_ptr g) {
  auto asc = make_accepting_state_cache(g);
  bdd dead_letters = bddtrue;
  for (unsigned i=0; i<g->num_states(); ++i) {
    for (auto & e : g->out(i)) {
      if (g->is_univ_dest(e.dst)) {
        for (auto dst : g->univ_dests(e.dst)) {
          if (is_accepting_state(dst, asc) && is_sink(g, dst)) {
            dead_letters = bdd_and(dead_letters, e.cond);
          }
        }
      } else if (is_accepting_state(e.dst, asc) && is_sink(g, e.dst)) {
        dead_letters = bdd_and(dead_letters, e.cond);
      }
    }
  }
  return dead_letters;
}

boost::dynamic_bitset<> get_dead_letter_map(twa_graph_ptr g, const ap_info & apinfo) {
  bdd dead_letters = get_dead_letters_bdd(g);
  auto bitmap = boost::dynamic_bitset<>();
  bitmap.resize(apinfo.letter_count, true);
  for (unsigned i=0; i<apinfo.letter_count; ++i) {
    if (bdd_implies(apinfo.bdd_alphabet[i], dead_letters)) {
      bitmap.reset(i);
    }
  }
  return bitmap;
}


void rename_aps_in_formula(formula & ltl, vector<string> & input_aps) {
  auto relabel_map = new std::map<formula, formula>();
  ltl = spot::relabel(ltl, Abc, relabel_map);
  IF_PAGE_BASIC {
    stringstream ss;
    for (auto & [new_ap, old_ap] : *relabel_map) {
      ss << new_ap << " = " << old_ap << endl;
    }
    cout << ss.str();
    page_code("LTL formula renaming map:", ss.str());
  }
  // make new input_aps list
  auto new_input_aps = vector<string>();
  for (auto & [new_ap, old_ap] : *relabel_map) {
    string old_ap_string = force_string(old_ap);
    if (std::any_of(input_aps.begin(), input_aps.end(), [old_ap_string](string s){return s==old_ap_string;})) {
      new_input_aps.push_back(force_string(new_ap));
    }
  }
  input_aps.swap(new_input_aps);
}



class TWAEdgeIterator {
private:
  std::vector<std::vector<tuple<unsigned, bdd>>> edges;
public:
  TWAEdgeIterator(twa_graph_ptr g) {
    edges.resize(g->num_states());
    for (unsigned s=0; s<g->num_states(); ++s) {
      auto & edges_s = edges[s];
      for (const auto & e : g->out(s)) {
        if (!g->is_univ_dest(e.dst)) {
          edges_s.push_back(make_tuple(e.dst, e.cond));
        } else {
          for (const auto & dst : g->univ_dests(e.dst)) {
            edges_s.push_back(make_tuple(dst, e.cond));
          }
        }
      }
    }
  }

  std::vector<tuple<unsigned, bdd>> & out(unsigned s) {
    return edges[s];
  }
};

TWAEdgeIterator * get_edge_iterator(twa_graph_ptr g) {
  TWAEdgeIterator * existing = g->get_named_prop<TWAEdgeIterator>("edge_iterator");
  if (existing != nullptr) return existing;
  auto edge_iter = new TWAEdgeIterator(g);
  g->set_named_prop<TWAEdgeIterator>("edge_iterator", edge_iter);
  return edge_iter;
}

/*

which benchmarks will i look at next?

ltl2dba
ltl2dpa

amba_case_study.tlsf


idea: hard-coded examples? add them to the initial hypothesis


*/

// here we are testing out our own moore/ucb intersection
twa_word_ptr test_moore_kucb_intersection(twa_graph_ptr moore_machine, twa_graph_ptr ucb, unsigned k, const twa_graph_ptr & kucb, bdd_dict_ptr dict) {
  // moore_kucb_intersection
  // cout << "\n\n\n\n\n";
  
  // cout << "performing moore kucb intersection (ours) ..." << endl;
  // bool we_found_counter = moore_kucb_intersection(moore_machine, ucb, k);
  // cout << "done." << endl;

  #if !CONFIG_REDUNDANT_CHECK_MOORE_INTERSECTION_AGAINST_SPOT
    if (we_found_counter == false) {
      return nullptr;
    }
  #endif

  // cout << "performing moore kucb intersection (spot) ..." << endl;
  // auto counter = moore_machine->intersecting_word(kucb);
  // cout << "done. " << "counterexample found: " << ((counter == nullptr) ? "no" : "yes") << endl;
  
  // cout << x << endl;

  auto our_counter = moore_kucb_intersection(moore_machine, ucb, k);
  // auto our_counter = moore_kucb_intersection(moore_machine, kucb, 0);
  
  // if (counter == nullptr) {
  //   cout << "NO counterexample found by spot" << endl;
  // } else {
  //   cout << "counterexample found by spot" << endl;
  //   if (our_counter != nullptr) {
  //     cout << "we can compare counterexamples" << endl;
  //     cout << endl << "spot's:" << endl;
  //     cout << *counter << endl;
  //     cout << endl << "our's:" << endl;
  //     cout << *our_counter << endl;
  //     cout << endl << endl;
  //   }
  // }
  

  return our_counter;

  // if ((counter == nullptr && we_found_counter) || (counter != nullptr && !we_found_counter)) {
  // if ((counter != nullptr && !we_found_counter)) { // this only complains if spot found a counterexample but we didn't
  //   cout << "we got a different result to spot" << endl;

  //   if (counter != nullptr)
  //     cout << "spot's counterexample: " << force_string(*counter) << endl;

  //   page_finish();
  //   exit(1);
  // }
  // cout << print_prefix(cout, moore_machine->get_dict(), inter) << endl;

  // if (counter != nullptr) {
  //   cout << "well we recognised the existence of a counterexample, although we currently have no way to actually extract it." << endl;
  // }

  // cout << "\n\n\n\n\n";
  // exit(0);
  // return counter;
}

// assumes no universal edges
bool is_valid_mealy_machine(twa_graph_ptr g, const ap_info & apinfo) {
  const unsigned num_states = g->num_states();
  for (unsigned s=0; s<num_states; ++s) {
    bdd input_covered = bddfalse;
    for (const auto & e : g->out(s)) {
      bdd cond_input = bdd_existcomp(e.cond, apinfo.bdd_input_aps);
      input_covered |= cond_input;
    }
    if (input_covered != bddtrue) {
      return false;
    }
  }
  return true;
}

bool is_valid_moore_machine(twa_graph_ptr g, const ap_info & apinfo) {
  const unsigned num_states = g->num_states();
  for (unsigned s=0; s<num_states; ++s) {
    bdd common_input = bddtrue;
    for (const auto & e : g->out(s)) {
      bdd cond_input = bdd_existcomp(e.cond, apinfo.bdd_input_aps);
      common_input &= cond_input;
    }
    if (common_input == bddfalse) {
      // there is no common input
      cout << "there's no common input on state " << s << endl;
      return false;
    }
    bdd output_covered = bddfalse;
    for (const auto & e : g->out(s)) {
      bdd cond_output = bdd_exist(e.cond, apinfo.bdd_input_aps);
      output_covered |= cond_output;
    }
    if (output_covered != bddtrue) {
      cout << "output isn't covered" << endl;
      return false;
    }
  }
  return true;
}

int get_bdd_var_index(bdd_dict_ptr dict, string label) {
  for (const auto & [f, i] : dict->var_map)
    if (force_string(f) == label) return i;
  throw std::runtime_error("the label '" + label + "' does not exist in the give dictionary");
}